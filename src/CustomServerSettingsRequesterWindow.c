#include <exec/exec.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "AmigaGPTConfig.h"
#include "CustomServerSettingsRequesterWindow.h"
#include "gui.h"
#include "openai.h"

/* (Debug logging removed) */
#define PSDPRINTF(...)                                                         \
    do {                                                                       \
    } while (0)

Object *customServerTemplateCycle;
Object *customServerHostString;
Object *customServerPortString;
Object *customServerUsesSSLCycle;
Object *customServerAuthorizationTypeCycle;
Object *customServerApiKeyString;
Object *customServerChatModelString;
Object *customServerModelList;
Object *customServerFetchModelsButton;
Object *customServerApiEndpointCycle;
Object *customServerApiEndpointUrlString;
Object *customServerCustomHeadersString;
Object *customServerFullUrlPreviewString;
Object *customServerStreamingCycle;
Object *customServerSettingsRequesterWindowObject;
Object *customServerProfileList;
Object *customServerProfileNameString;
Object *customServerSaveProfileButton;
Object *customServerDeleteProfileButton;

/* Root group for layout changes */
static Object *customServerSettingsRootGroup = NULL;
/* Frame group containing the Streaming option (chat-only) */
static Object *customServerStreamingGroup = NULL;

/* API endpoint options (chat vs image) */
static STRPTR chatApiEndpointOptions[6] = {NULL};
static STRPTR imageApiEndpointOptions[3] = {NULL};

static struct json_object *customServerModelsJson = NULL;
static struct json_object *profilesJson = NULL;

static BOOL profileSettingsDirty = FALSE;
static LONG lastSelectedProfile = -1;
static BOOL loadingProfile = FALSE;
static BOOL settingsIsImageMode = FALSE;
static BOOL suppressProfileSelected = FALSE;

/* Template definitions */
enum {
    TEMPLATE_NONE = 0,
    TEMPLATE_GOOGLE_GEMINI,
    TEMPLATE_LM_STUDIO,
    TEMPLATE_ANTHROPIC_CLAUDE,
    TEMPLATE_XAI_GROK
};

/* Forward declarations */
static void populateModelList(void);
static void populateProfileList(LONG forceActive);
static void loadProfilesFromConfig(void);
static void saveProfilesToConfig(void);
static BOOL applyProfileFromFormToStorage(LONG profileListIndex,
                                          BOOL setActiveAfterSave,
                                          LONG forceActiveForList);
SAVEDS void refreshUrlPreview(void);

/**
 * Load profiles JSON from config
 */
static void loadProfilesFromConfig(void) {
    if (profilesJson != NULL) {
        json_object_put(profilesJson);
        profilesJson = NULL;
    }

    STRPTR profilesStr = settingsIsImageMode
                             ? configGetCustomImageServerProfiles()
                             : configGetCustomServerProfiles();
    if (profilesStr != NULL && strlen(profilesStr) > 0) {
        profilesJson = json_tokener_parse(profilesStr);
        if (profilesJson == NULL ||
            !json_object_is_type(profilesJson, json_type_array)) {
            if (profilesJson != NULL) {
                json_object_put(profilesJson);
            }
            profilesJson = json_object_new_array();
        }
    } else {
        profilesJson = json_object_new_array();
    }
}

/**
 * Save profiles JSON to config
 */
static void saveProfilesToConfig(void) {
    if (profilesJson != NULL) {
        CONST_STRPTR jsonStr = json_object_to_json_string(profilesJson);
        if (settingsIsImageMode) {
            configSetCustomImageServerProfiles(jsonStr);
        } else {
            configSetCustomServerProfiles(jsonStr);
        }
    }
}

/* Number of built-in providers shown in the profile list.
 * Chat mode: OpenAI, Gemini, Grok, Anthropic (4)
 * Image mode: OpenAI, Gemini, Grok (3) */
static LONG getBuiltinProviderCount(void) {
    return settingsIsImageMode ? 3 : 4;
}

/**
 * Check if a list index corresponds to a built-in provider
 * Index 0 = New Profile, 1-4 = built-in providers, 5+ = custom profiles
 */
static BOOL isBuiltinProviderIndex(LONG index) {
    return (index >= 1 && index <= getBuiltinProviderCount());
}

/**
 * Get the Provider enum value from a list index
 * Returns -1 if not a built-in provider
 */
static LONG getProviderFromIndex(LONG index) {
    if (index < 1 || index > getBuiltinProviderCount())
        return -1;
    return index - 1; /* PROVIDER_OPENAI=0, PROVIDER_GEMINI=1, etc. */
}

/**
 * Enable or disable server/connection setting gadgets (host, port, SSL,
 * auth, endpoint, template, fetch models). API key and chat model stay
 * under caller control. Used to lock these when a built-in provider is
 * selected.
 */
static void setServerSettingsGadgetsEnabled(BOOL enabled) {
    if (customServerTemplateCycle != NULL)
        set(customServerTemplateCycle, MUIA_Disabled, enabled ? FALSE : TRUE);
    if (customServerHostString != NULL)
        set(customServerHostString, MUIA_Disabled, enabled ? FALSE : TRUE);
    if (customServerPortString != NULL)
        set(customServerPortString, MUIA_Disabled, enabled ? FALSE : TRUE);
    if (customServerUsesSSLCycle != NULL)
        set(customServerUsesSSLCycle, MUIA_Disabled, enabled ? FALSE : TRUE);
    if (customServerAuthorizationTypeCycle != NULL)
        set(customServerAuthorizationTypeCycle, MUIA_Disabled,
            enabled ? FALSE : TRUE);
    if (customServerApiEndpointCycle != NULL)
        set(customServerApiEndpointCycle, MUIA_Disabled,
            enabled ? FALSE : TRUE);
    if (customServerApiEndpointUrlString != NULL)
        set(customServerApiEndpointUrlString, MUIA_Disabled,
            enabled ? FALSE : TRUE);
    if (customServerCustomHeadersString != NULL)
        set(customServerCustomHeadersString, MUIA_Disabled,
            enabled ? FALSE : TRUE);
    /* Fetch Models is left enabled for built-in providers (e.g. OpenAI) */
}

/**
 * Load built-in provider settings into UI
 */
static void loadBuiltinProviderIntoUI(Provider provider) {
    PSDPRINTF("loadBuiltinProviderIntoUI enter provider=%ld imageMode=%ld",
              (long)provider, (long)settingsIsImageMode);
    struct ProviderConfig *config = getProviderConfig(provider);
    if (config == NULL)
        return;

    set(customServerHostString, MUIA_String_Contents,
        config->host ? config->host : "");
    set(customServerPortString, MUIA_String_Integer, (LONG)config->port);
    set(customServerUsesSSLCycle, MUIA_Cycle_Active, config->useSSL ? 1 : 0);
    set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
        (LONG)config->authorizationType);
    if (!settingsIsImageMode) {
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            (LONG)config->apiEndpoint);
    } else if (customServerApiEndpointCycle != NULL) {
        /* Image endpoint defaults:
         * - Gemini: native generateContent
         * - Others: images/generations
         * If Gemini is currently the selected image provider, load the saved
         * endpoint; otherwise default to native for the Gemini profile. */
        LONG active = 0;
        if (provider == PROVIDER_GEMINI) {
            if (configGetImageProvider() == PROVIDER_GEMINI) {
                active = (configGetImageApiEndpoint() ==
                          (ULONG)API_ENDPOINT_GEMINI_GENERATE_CONTENT)
                             ? 1
                             : 0;
            } else {
                active = 1;
            }
        } else {
            active = 0;
        }
        set(customServerApiEndpointCycle, MUIA_Cycle_Active, active);
    }
    set(customServerApiEndpointUrlString, MUIA_String_Contents,
        config->apiEndpointUrl ? config->apiEndpointUrl : "v1");
    set(customServerCustomHeadersString, MUIA_String_Contents,
        config->customHeaders ? config->customHeaders : "");
    if (!settingsIsImageMode && customServerStreamingCycle != NULL) {
        set(customServerStreamingCycle, MUIA_Cycle_Active,
            configGetChatStreamingEnabledForProvider(provider) ? 1 : 0);
    }

    /* Load API key for this provider */
    STRPTR apiKey = configGetApiKeyForProvider(provider);
    set(customServerApiKeyString, MUIA_String_Contents, apiKey ? apiKey : "");

    /* Load the saved model */
    STRPTR modelName = NULL;
    CONST_STRPTR *modelList = NULL;
    if (settingsIsImageMode) {
        modelName = configGetImageModelName();
        switch (provider) {
        case PROVIDER_OPENAI:
            modelList = OPENAI_IMAGE_MODELS;
            break;
        case PROVIDER_GEMINI:
            modelList = GEMINI_IMAGE_MODELS;
            break;
        case PROVIDER_GROK:
            modelList = GROK_IMAGE_MODELS;
            break;
        default:
            modelList = OPENAI_IMAGE_MODELS;
            break;
        }
    } else {
        modelName = configGetChatModelNameForProvider(provider);
        switch (provider) {
        case PROVIDER_OPENAI:
            modelList = OPENAI_CHAT_MODELS;
            break;
        case PROVIDER_GEMINI:
            modelList = GEMINI_CHAT_MODELS;
            break;
        case PROVIDER_GROK:
            modelList = GROK_CHAT_MODELS;
            break;
        case PROVIDER_ANTHROPIC:
            modelList = ANTHROPIC_CHAT_MODELS;
            break;
        default:
            modelList = NULL;
            break;
        }
    }

    if (modelName != NULL && strlen(modelName) > 0) {
        set(customServerChatModelString, MUIA_String_Contents, modelName);
    } else if (modelList != NULL && modelList[0] != NULL &&
               strlen(modelList[0]) > 0) {
        set(customServerChatModelString, MUIA_String_Contents, modelList[0]);
    }
    PSDPRINTF("loadBuiltinProviderIntoUI exit");
}

/**
 * Populate the profile list from profilesJson
 * @param forceActive -1 to use default selection, or >=0 to select that
 * index
 */
static void populateProfileList(LONG forceActive) {
    PSDPRINTF(
        "populateProfileList begin (imageMode=%ld forceActive=%ld list=%p)",
        (long)settingsIsImageMode, (long)forceActive,
        (void *)customServerProfileList);
    if (customServerProfileList == NULL)
        return;

    DoMethod(customServerProfileList, MUIM_NList_Clear);

    /* Add "(New Profile)" entry first */
    DoMethod(customServerProfileList, MUIM_NList_InsertSingle,
             (ULONG)STRING_NEW_PROFILE, MUIV_NList_Insert_Bottom);

    /* Add built-in providers (non-deletable) */
    DoMethod(customServerProfileList, MUIM_NList_InsertSingle,
             (ULONG)STRING_PROVIDER_OPENAI, MUIV_NList_Insert_Bottom);
    DoMethod(customServerProfileList, MUIM_NList_InsertSingle,
             (ULONG)STRING_PROVIDER_GEMINI, MUIV_NList_Insert_Bottom);
    DoMethod(customServerProfileList, MUIM_NList_InsertSingle,
             (ULONG)STRING_PROVIDER_GROK, MUIV_NList_Insert_Bottom);
    if (!settingsIsImageMode) {
        DoMethod(customServerProfileList, MUIM_NList_InsertSingle,
                 (ULONG)STRING_PROVIDER_ANTHROPIC, MUIV_NList_Insert_Bottom);
    }

    /* Add custom profiles from JSON */
    if (profilesJson != NULL &&
        json_object_is_type(profilesJson, json_type_array)) {
        int len = json_object_array_length(profilesJson);
        for (int i = 0; i < len; i++) {
            struct json_object *profile =
                json_object_array_get_idx(profilesJson, i);
            if (profile != NULL) {
                struct json_object *nameObj =
                    json_object_object_get(profile, "name");
                if (nameObj != NULL) {
                    CONST_STRPTR name = json_object_get_string(nameObj);
                    if (name != NULL) {
                        DoMethod(customServerProfileList,
                                 MUIM_NList_InsertSingle, (ULONG)name,
                                 MUIV_NList_Insert_Bottom);
                    }
                }
            }
        }
    }

    /* Select based on current provider for this window mode, or use forced
     * index */
    if (forceActive >= 0) {
        set(customServerProfileList, MUIA_NList_Active, forceActive);
    } else {
        Provider currentProvider = settingsIsImageMode
                                       ? configGetImageProvider()
                                       : configGetChatProvider();
        if (settingsIsImageMode && currentProvider == PROVIDER_ANTHROPIC) {
            currentProvider = PROVIDER_OPENAI;
        }
        if (currentProvider < PROVIDER_CUSTOM &&
            currentProvider < (Provider)getBuiltinProviderCount()) {
            set(customServerProfileList, MUIA_NList_Active,
                currentProvider + 1);
        } else {
            STRPTR activeProfile = settingsIsImageMode
                                       ? configGetActiveImageProfileName()
                                       : configGetActiveProfileName();
            if (activeProfile != NULL && strlen(activeProfile) > 0) {
                if (customServerProfileNameString != NULL)
                    set(customServerProfileNameString, MUIA_String_Contents,
                        activeProfile);
                if (profilesJson != NULL) {
                    int len = json_object_array_length(profilesJson);
                    for (int i = 0; i < len; i++) {
                        struct json_object *profile =
                            json_object_array_get_idx(profilesJson, i);
                        struct json_object *nameObj =
                            json_object_object_get(profile, "name");
                        if (nameObj != NULL) {
                            CONST_STRPTR name = json_object_get_string(nameObj);
                            if (name != NULL &&
                                strcmp(name, activeProfile) == 0) {
                                set(customServerProfileList, MUIA_NList_Active,
                                    i + 1 + getBuiltinProviderCount());
                                break;
                            }
                        }
                    }
                }
            } else {
                set(customServerProfileList, MUIA_NList_Active, 0);
            }
        }
    }
    {
        LONG active = MUIV_NList_Active_Off;
        get(customServerProfileList, MUIA_NList_Active, &active);
        PSDPRINTF("populateProfileList end (active=%ld)", (long)active);
    }
}

/**
 * Load a profile's settings into the UI fields
 */
static void loadProfileIntoUI(struct json_object *profile) {
    struct json_object *obj;

    obj = json_object_object_get(profile, "host");
    if (obj != NULL)
        set(customServerHostString, MUIA_String_Contents,
            json_object_get_string(obj));

    obj = json_object_object_get(profile, "port");
    if (obj != NULL)
        set(customServerPortString, MUIA_String_Integer,
            json_object_get_int(obj));

    obj = json_object_object_get(profile, "useSSL");
    if (obj != NULL)
        set(customServerUsesSSLCycle, MUIA_Cycle_Active,
            json_object_get_boolean(obj) ? 1 : 0);

    obj = json_object_object_get(profile, "authorizationType");
    if (obj != NULL)
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            json_object_get_int(obj));

    obj = json_object_object_get(profile, "apiKey");
    if (obj != NULL)
        set(customServerApiKeyString, MUIA_String_Contents,
            json_object_get_string(obj));

    if (settingsIsImageMode) {
        obj = json_object_object_get(profile, "imageModel");
        if (obj == NULL)
            obj = json_object_object_get(profile, "chatModel");
    } else {
        obj = json_object_object_get(profile, "chatModel");
        if (obj == NULL)
            obj = json_object_object_get(profile, "imageModel");
    }
    if (obj != NULL) {
        set(customServerChatModelString, MUIA_String_Contents,
            json_object_get_string(obj));
    }

    obj = json_object_object_get(profile, "apiEndpoint");
    if (obj != NULL && customServerApiEndpointCycle != NULL) {
        if (settingsIsImageMode) {
            LONG ep = json_object_get_int(obj);
            LONG active =
                (ep == (LONG)API_ENDPOINT_GEMINI_GENERATE_CONTENT) ? 1 : 0;
            set(customServerApiEndpointCycle, MUIA_Cycle_Active, active);
        } else {
            set(customServerApiEndpointCycle, MUIA_Cycle_Active,
                json_object_get_int(obj));
        }
    }

    obj = json_object_object_get(profile, "apiEndpointUrl");
    if (obj != NULL)
        set(customServerApiEndpointUrlString, MUIA_String_Contents,
            json_object_get_string(obj));

    obj = json_object_object_get(profile, "customHeaders");
    if (obj != NULL)
        set(customServerCustomHeadersString, MUIA_String_Contents,
            json_object_get_string(obj));

    if (!settingsIsImageMode) {
        obj = json_object_object_get(profile, "streaming");
        if (customServerStreamingCycle != NULL) {
            if (obj != NULL) {
                set(customServerStreamingCycle, MUIA_Cycle_Active,
                    json_object_get_boolean(obj) ? 1 : 0);
            } else {
                set(customServerStreamingCycle, MUIA_Cycle_Active,
                    configGetCustomChatStreamEnabled() ? 1 : 0);
            }
        }
    }
}

/**
 * Create a profile JSON object from current UI values
 */
static struct json_object *createProfileFromUI(CONST_STRPTR name) {
    struct json_object *profile = json_object_new_object();

    json_object_object_add(profile, "name", json_object_new_string(name));

    STRPTR host;
    get(customServerHostString, MUIA_String_Contents, &host);
    json_object_object_add(profile, "host",
                           json_object_new_string(host ? host : ""));

    LONG port;
    get(customServerPortString, MUIA_String_Integer, &port);
    json_object_object_add(profile, "port", json_object_new_int(port));

    LONG useSSL;
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &useSSL);
    json_object_object_add(profile, "useSSL",
                           json_object_new_boolean(useSSL == 1));

    LONG authType;
    get(customServerAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
    json_object_object_add(profile, "authorizationType",
                           json_object_new_int(authType));

    STRPTR apiKey;
    get(customServerApiKeyString, MUIA_String_Contents, &apiKey);
    json_object_object_add(profile, "apiKey",
                           json_object_new_string(apiKey ? apiKey : ""));

    STRPTR modelStr;
    get(customServerChatModelString, MUIA_String_Contents, &modelStr);
    /* Store both keys so chat/image windows can share the same custom profile
     * list without losing values when saved from either window. */
    json_object_object_add(profile, "chatModel",
                           json_object_new_string(modelStr ? modelStr : ""));
    json_object_object_add(profile, "imageModel",
                           json_object_new_string(modelStr ? modelStr : ""));

    LONG apiEndpoint;
    get(customServerApiEndpointCycle, MUIA_Cycle_Active, &apiEndpoint);
    if (settingsIsImageMode) {
        apiEndpoint = (apiEndpoint == 1) ? API_ENDPOINT_GEMINI_GENERATE_CONTENT
                                         : API_ENDPOINT_IMAGES_GENERATIONS;
    }
    json_object_object_add(profile, "apiEndpoint",
                           json_object_new_int(apiEndpoint));

    STRPTR apiEndpointUrl;
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &apiEndpointUrl);
    json_object_object_add(
        profile, "apiEndpointUrl",
        json_object_new_string(apiEndpointUrl ? apiEndpointUrl : ""));

    STRPTR customHeaders;
    get(customServerCustomHeadersString, MUIA_String_Contents, &customHeaders);
    json_object_object_add(
        profile, "customHeaders",
        json_object_new_string(customHeaders ? customHeaders : ""));

    if (!settingsIsImageMode) {
        LONG streamingEnabled = 0;
        if (customServerStreamingCycle != NULL)
            get(customServerStreamingCycle, MUIA_Cycle_Active,
                &streamingEnabled);
        json_object_object_add(profile, "streaming",
                               json_object_new_boolean(streamingEnabled == 1));
    }

    return profile;
}

/**
 * Apply current form to storage for the given profile list index.
 * For built-in: write API key and chat model to config.
 * For New/custom: update profilesJson and save; optionally set active.
 * @param forceActiveForList -1 to use default selection in
 * populateProfileList, or >=0 to select that index after refresh.
 * @return FALSE only when New Profile and name is empty
 */
static BOOL applyProfileFromFormToStorage(LONG profileListIndex,
                                          BOOL setActiveAfterSave,
                                          LONG forceActiveForList) {
    STRPTR apiKey = NULL;
    STRPTR chatModel = NULL;
    LONG streamingEnabled = 0;
    get(customServerApiKeyString, MUIA_String_Contents, &apiKey);
    get(customServerChatModelString, MUIA_String_Contents, &chatModel);
    if (!settingsIsImageMode && customServerStreamingCycle != NULL)
        get(customServerStreamingCycle, MUIA_Cycle_Active, &streamingEnabled);

    if (isBuiltinProviderIndex(profileListIndex)) {
        Provider provider = (Provider)getProviderFromIndex(profileListIndex);
        if (settingsIsImageMode) {
            /* Image Provider Settings */
            configSetImageProvider(provider);
        } else {
            /* Chat Provider Settings */
            configSetChatProvider(provider);
        }
        switch (provider) {
        case PROVIDER_OPENAI:
            configSetOpenAiApiKey(apiKey);
            break;
        case PROVIDER_GEMINI:
            configSetGeminiApiKey(apiKey);
            break;
        case PROVIDER_GROK:
            configSetGrokApiKey(apiKey);
            break;
        case PROVIDER_ANTHROPIC:
            configSetAnthropicApiKey(apiKey);
            break;
        default:
            break;
        }
        if (settingsIsImageMode) {
            /* Save image endpoint selection */
            LONG imageEndpointActive = 0;
            if (customServerApiEndpointCycle != NULL)
                get(customServerApiEndpointCycle, MUIA_Cycle_Active,
                    &imageEndpointActive);
            ULONG imageEndpoint =
                (imageEndpointActive == 1)
                    ? (ULONG)API_ENDPOINT_GEMINI_GENERATE_CONTENT
                    : (ULONG)API_ENDPOINT_IMAGES_GENERATIONS;
            configSetImageApiEndpoint(imageEndpoint);

            /* Save image model string (stored globally for now). Validate
             * against the built-in provider model list. */
            CONST_STRPTR *allowed = NULL;
            switch (provider) {
            case PROVIDER_OPENAI:
                allowed = OPENAI_IMAGE_MODELS;
                break;
            case PROVIDER_GEMINI:
                allowed = GEMINI_IMAGE_MODELS;
                break;
            case PROVIDER_GROK:
                allowed = GROK_IMAGE_MODELS;
                break;
            default:
                allowed = OPENAI_IMAGE_MODELS;
                break;
            }

            CONST_STRPTR modelName =
                (chatModel != NULL && strlen(chatModel) > 0) ? chatModel : NULL;
            BOOL ok = FALSE;
            if (modelName != NULL && allowed != NULL) {
                for (UBYTE i = 0; allowed[i] != NULL; i++) {
                    if (strcmp(modelName, allowed[i]) == 0) {
                        ok = TRUE;
                        break;
                    }
                }
            }
            if (!ok && allowed != NULL && allowed[0] != NULL) {
                modelName = allowed[0];
            }
            if (modelName != NULL) {
                configSetImageModelName(modelName);
            }
        } else {
            configSetChatModelName(chatModel);
            configSetChatStreamingEnabledForProvider(provider,
                                                     streamingEnabled == 1);
        }
        return TRUE;
    }

    STRPTR profileName = NULL;
    get(customServerProfileNameString, MUIA_String_Contents, &profileName);
    if (profileName == NULL || strlen(profileName) == 0) {
        if (profileListIndex == 0)
            return FALSE;
        /* custom: use existing name from list if form empty */
        if (profilesJson != NULL &&
            profileListIndex > getBuiltinProviderCount()) {
            int pi = profileListIndex - 1 - getBuiltinProviderCount();
            struct json_object *p = json_object_array_get_idx(profilesJson, pi);
            if (p != NULL) {
                struct json_object *no = json_object_object_get(p, "name");
                if (no != NULL)
                    profileName = (STRPTR)json_object_get_string(no);
            }
        }
        if (profileName == NULL || strlen(profileName) == 0)
            return FALSE;
    }

    struct json_object *newProfile = createProfileFromUI(profileName);

    if (profileListIndex == 0) {
        json_object_array_add(profilesJson, newProfile);
        saveProfilesToConfig();
        if (setActiveAfterSave)
            if (settingsIsImageMode)
                configSetActiveImageProfileName(profileName);
            else
                configSetActiveProfileName(profileName);
        populateProfileList(forceActiveForList);
        return TRUE;
    }

    /* custom profile: replace at index */
    int profileIndex = profileListIndex - 1 - getBuiltinProviderCount();
    if (profileIndex >= 0 && profilesJson != NULL) {
        json_object_array_put_idx(profilesJson, profileIndex, newProfile);
        saveProfilesToConfig();
        if (setActiveAfterSave)
            if (settingsIsImageMode)
                configSetActiveImageProfileName(profileName);
            else
                configSetActiveProfileName(profileName);
        populateProfileList(forceActiveForList);
    }
    return TRUE;
}

/* NList hooks for profiles */
HOOKPROTONHNO(ConstructProfileLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    ULONG len = strlen(entry) + 1;
    STRPTR copy = AllocVec(len, MEMF_ANY);
    if (copy)
        strcpy(copy, entry);
    return copy;
}
MakeHook(ConstructProfileLI_TextHook, ConstructProfileLI_TextFunc);

HOOKPROTONHNO(DestructProfileLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    if (ndm->entry)
        FreeVec(ndm->entry);
}
MakeHook(DestructProfileLI_TextHook, DestructProfileLI_TextFunc);

HOOKPROTONHNO(DisplayProfileLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayProfileLI_TextHook, DisplayProfileLI_TextFunc);

/* Hook for profile selection */
HOOKPROTONHNONP(ProfileSelectedFunc, void) {
    PSDPRINTF("ProfileSelectedFunc enter (imageMode=%ld dirty=%ld last=%ld)",
              (long)settingsIsImageMode, (long)profileSettingsDirty,
              (long)lastSelectedProfile);
    if (suppressProfileSelected) {
        PSDPRINTF("ProfileSelectedFunc suppressed");
        return;
    }
    if (customServerProfileList == NULL ||
        customServerProfileNameString == NULL)
        return;
    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);
    PSDPRINTF("ProfileSelectedFunc active=%ld", (long)active);

    if (profileSettingsDirty && lastSelectedProfile >= 0 &&
        active != lastSelectedProfile && active != MUIV_NList_Active_Off) {
        LONG res = MUI_Request(app, customServerSettingsRequesterWindowObject,
#ifdef __MORPHOS__
                               NULL,
#else
                               MUIV_Requester_Image_Info,
#endif
                               "Save changes?", "*_Yes|_No|_Cancel",
                               STRING_SAVE_CHANGES_PROMPT, TAG_DONE);
        if (res == 3) {
            set(customServerProfileList, MUIA_NList_Active,
                lastSelectedProfile);
            return;
        }
        if (res == 1) {
            loadingProfile = TRUE;
            if (!applyProfileFromFormToStorage(lastSelectedProfile, FALSE,
                                               active)) {
                displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
                set(customServerProfileList, MUIA_NList_Active,
                    lastSelectedProfile);
                loadingProfile = FALSE;
                return;
            }
            loadingProfile = FALSE;
        }
        profileSettingsDirty = FALSE;
    }
    lastSelectedProfile = active;
    loadingProfile = TRUE;

    if (active == MUIV_NList_Active_Off || active == 0) {
        PSDPRINTF("ProfileSelectedFunc: new profile branch");
        /* "(New Profile)" selected or nothing - clear name */
        set(customServerProfileNameString, MUIA_String_Contents, "");
        if (customServerProfileNameString != NULL)
            set(customServerProfileNameString, MUIA_Disabled, FALSE);
        if (!settingsIsImageMode && customServerStreamingCycle != NULL)
            set(customServerStreamingCycle, MUIA_Cycle_Active,
                configGetCustomChatStreamEnabled() ? 1 : 0);
        /* Disable delete button for new profile */
        if (customServerDeleteProfileButton != NULL)
            set(customServerDeleteProfileButton, MUIA_Disabled, TRUE);
        /* Enable Save Profile and server settings for new custom profile */
        if (customServerSaveProfileButton != NULL)
            set(customServerSaveProfileButton, MUIA_Disabled, FALSE);
        setServerSettingsGadgetsEnabled(TRUE);
        PSDPRINTF("ProfileSelectedFunc: new profile refreshUrlPreview()");
        refreshUrlPreview();
        loadingProfile = FALSE;
        return;
    }

    /* Check if this is a built-in provider */
    if (isBuiltinProviderIndex(active)) {
        Provider provider = (Provider)getProviderFromIndex(active);
        PSDPRINTF("ProfileSelectedFunc: builtin provider=%ld", (long)provider);
        loadBuiltinProviderIntoUI(provider);

        /* Set the profile name to the provider name (read-only info) */
        set(customServerProfileNameString, MUIA_String_Contents,
            PROVIDER_NAMES[provider]);

        /* Disable delete, Save Profile and profile name for built-in;
         * only API key and chat model are editable */
        if (customServerDeleteProfileButton != NULL)
            set(customServerDeleteProfileButton, MUIA_Disabled, TRUE);
        if (customServerSaveProfileButton != NULL)
            set(customServerSaveProfileButton, MUIA_Disabled, TRUE);
        if (customServerProfileNameString != NULL)
            set(customServerProfileNameString, MUIA_Disabled, TRUE);
        setServerSettingsGadgetsEnabled(FALSE);

        /* Replace fetched models with prepopulated list for this provider
         */
        if (customServerModelsJson != NULL) {
            json_object_put(customServerModelsJson);
            customServerModelsJson = NULL;
        }
        {
            CONST_STRPTR *modelList = NULL;
            switch (provider) {
            case PROVIDER_OPENAI:
                modelList = settingsIsImageMode ? OPENAI_IMAGE_MODELS
                                                : OPENAI_CHAT_MODELS;
                break;
            case PROVIDER_GEMINI:
                modelList = settingsIsImageMode ? GEMINI_IMAGE_MODELS
                                                : GEMINI_CHAT_MODELS;
                break;
            case PROVIDER_GROK:
                modelList =
                    settingsIsImageMode ? GROK_IMAGE_MODELS : GROK_CHAT_MODELS;
                break;
            case PROVIDER_ANTHROPIC:
                modelList = settingsIsImageMode ? NULL : ANTHROPIC_CHAT_MODELS;
                break;
            default:
                break;
            }

            if (modelList != NULL) {
                customServerModelsJson = json_object_new_array();
                for (UBYTE i = 0; modelList[i] != NULL; i++)
                    json_object_array_add(customServerModelsJson,
                                          json_object_new_string(modelList[i]));
                PSDPRINTF("ProfileSelectedFunc: populateModelList()");
                populateModelList();
            } else if (customServerModelList != NULL) {
                DoMethod(customServerModelList, MUIM_NList_Clear);
            }
        }
        PSDPRINTF("ProfileSelectedFunc: builtin refreshUrlPreview()");
        refreshUrlPreview();
        loadingProfile = FALSE;
        return;
    }

    /* Enable delete, Save Profile, profile name and server settings for
     * custom profiles */
    if (customServerDeleteProfileButton != NULL)
        set(customServerDeleteProfileButton, MUIA_Disabled, FALSE);
    if (customServerSaveProfileButton != NULL)
        set(customServerSaveProfileButton, MUIA_Disabled, FALSE);
    if (customServerProfileNameString != NULL)
        set(customServerProfileNameString, MUIA_Disabled, FALSE);
    setServerSettingsGadgetsEnabled(TRUE);

    /* Load the selected custom profile
     * Custom profiles start after built-in providers:
     * Index 0 = New Profile, 1-4 = built-in, 5+ = custom */
    int profileIndex = active - 1 - getBuiltinProviderCount();
    PSDPRINTF("ProfileSelectedFunc: custom profileIndex=%ld",
              (long)profileIndex);
    if (profilesJson != NULL &&
        json_object_is_type(profilesJson, json_type_array) &&
        profileIndex >= 0) {
        struct json_object *profile =
            json_object_array_get_idx(profilesJson, profileIndex);
        if (profile != NULL) {
            struct json_object *nameObj =
                json_object_object_get(profile, "name");
            if (nameObj != NULL) {
                set(customServerProfileNameString, MUIA_String_Contents,
                    json_object_get_string(nameObj));
            }
            loadProfileIntoUI(profile);

            /* Clear fetched models list since server changed */
            if (customServerModelList != NULL) {
                DoMethod(customServerModelList, MUIM_NList_Clear);
            }
            if (customServerModelsJson != NULL) {
                json_object_put(customServerModelsJson);
                customServerModelsJson = NULL;
            }
        }
    }
    PSDPRINTF("ProfileSelectedFunc: custom refreshUrlPreview()");
    refreshUrlPreview();
    loadingProfile = FALSE;
}
MakeHook(ProfileSelectedHook, ProfileSelectedFunc);

/* Hook for Save Profile button */
HOOKPROTONHNONP(SaveProfileFunc, void) {
    if (customServerProfileNameString == NULL)
        return;

    STRPTR profileName;
    get(customServerProfileNameString, MUIA_String_Contents, &profileName);

    if (profileName == NULL || strlen(profileName) == 0) {
        displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
        return;
    }

    /* Disallow names that match built-in providers */
    for (int i = 0; i < getBuiltinProviderCount() && PROVIDER_NAMES[i] != NULL;
         i++) {
        if (strcasecmp(profileName, PROVIDER_NAMES[i]) == 0) {
            displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
            return;
        }
    }

    /* Check if profile with this name already exists */
    int existingIndex = -1;
    if (profilesJson != NULL) {
        int len = json_object_array_length(profilesJson);
        for (int i = 0; i < len; i++) {
            struct json_object *profile =
                json_object_array_get_idx(profilesJson, i);
            struct json_object *nameObj =
                json_object_object_get(profile, "name");
            if (nameObj != NULL) {
                CONST_STRPTR name = json_object_get_string(nameObj);
                if (name != NULL && strcmp(name, profileName) == 0) {
                    existingIndex = i;
                    break;
                }
            }
        }
    }

    /* Create new profile from UI */
    struct json_object *newProfile = createProfileFromUI(profileName);

    if (existingIndex >= 0) {
        /* Replace existing profile */
        json_object_array_put_idx(profilesJson, existingIndex, newProfile);
    } else {
        /* Add new profile */
        json_object_array_add(profilesJson, newProfile);
    }

    /* Save and refresh */
    saveProfilesToConfig();
    if (settingsIsImageMode)
        configSetActiveImageProfileName(profileName);
    else
        configSetActiveProfileName(profileName);
    loadingProfile = TRUE;
    populateProfileList(-1);
    loadingProfile = FALSE;
    profileSettingsDirty = FALSE;
}
MakeHook(SaveProfileHook, SaveProfileFunc);

/* Hook for Delete Profile button */
HOOKPROTONHNONP(DeleteProfileFunc, void) {
    if (customServerProfileList == NULL ||
        customServerProfileNameString == NULL)
        return;

    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);

    /* Can't delete "(New Profile)" entry or built-in providers */
    if (active <= 0 || isBuiltinProviderIndex(active)) {
        return;
    }

    /* Calculate actual profile index (skip New Profile and built-in
     * providers)
     */
    int profileIndex = active - 1 - getBuiltinProviderCount();
    if (profileIndex < 0) {
        return;
    }

    if (profilesJson != NULL &&
        json_object_is_type(profilesJson, json_type_array)) {
        /* Remove from array */
        json_object_array_del_idx(profilesJson, profileIndex, 1);

        /* Save and refresh */
        saveProfilesToConfig();
        if (settingsIsImageMode)
            configSetActiveImageProfileName("");
        else
            configSetActiveProfileName("");
        set(customServerProfileNameString, MUIA_String_Contents, "");
        populateProfileList(-1);
    }
}
MakeHook(DeleteProfileHook, DeleteProfileFunc);

/* NList hooks for models */
HOOKPROTONHNO(ConstructCustomModelLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    return entry;
}
MakeHook(ConstructCustomModelLI_TextHook, ConstructCustomModelLI_TextFunc);

HOOKPROTONHNO(DestructCustomModelLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    /* Don't free - strings owned by JSON object */
}
MakeHook(DestructCustomModelLI_TextHook, DestructCustomModelLI_TextFunc);

HOOKPROTONHNO(DisplayCustomModelLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayCustomModelLI_TextHook, DisplayCustomModelLI_TextFunc);

/* Hook for when a model is selected from the list */
HOOKPROTONHNONP(CustomModelSelectedFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(customServerModelList, MUIA_NList_Active, &active);

    if (active != MUIV_NList_Active_Off && customServerModelsJson != NULL &&
        json_object_is_type(customServerModelsJson, json_type_array)) {
        struct json_object *modelIdObj =
            json_object_array_get_idx(customServerModelsJson, active);
        if (modelIdObj != NULL) {
            CONST_STRPTR modelId = json_object_get_string(modelIdObj);
            if (modelId != NULL) {
                set(customServerChatModelString, MUIA_String_Contents, modelId);
            }
        }
    }
}
MakeHook(CustomModelSelectedHook, CustomModelSelectedFunc);

/* Hook for fetching models from the server */
HOOKPROTONHNONP(FetchCustomModelsFunc, void) {
    STRPTR host;
    LONG port;
    LONG usesSSL;
    LONG authorizationType;
    STRPTR apiKey;
    STRPTR endpointUrl;
    STRPTR customHeaders;

    get(customServerHostString, MUIA_String_Contents, &host);
    get(customServerPortString, MUIA_String_Integer, &port);
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    get(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
        &authorizationType);
    get(customServerApiKeyString, MUIA_String_Contents, &apiKey);
    get(customServerApiEndpointUrlString, MUIA_String_Contents, &endpointUrl);
    get(customServerCustomHeadersString, MUIA_String_Contents, &customHeaders);

    if (host == NULL || strlen(host) == 0) {
        displayError(STRING_ERROR_NO_HOST);
        return;
    }

    updateStatusBar(STRING_FETCHING_MODELS, yellowPen);
    set(customServerFetchModelsButton, MUIA_Disabled, TRUE);

    /* Free previous models JSON if any */
    if (customServerModelsJson != NULL) {
        json_object_put(customServerModelsJson);
        customServerModelsJson = NULL;
    }

    /* Fetch models from the custom provider */
    customServerModelsJson =
        getChatModels(host, port, usesSSL == 1, apiKey, configGetProxyEnabled(),
                      configGetProxyHost(), configGetProxyPort(),
                      configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
                      configGetProxyUsername(), configGetProxyPassword(),
                      endpointUrl, authorizationType, customHeaders);

    if (customServerModelsJson != NULL) {
        populateModelList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_FETCHING_MODELS);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(customServerFetchModelsButton, MUIA_Disabled, FALSE);
}
MakeHook(FetchCustomModelsHook, FetchCustomModelsFunc);

/* Populate the model list from JSON */
static void populateModelList(void) {
    DoMethod(customServerModelList, MUIM_NList_Clear);

    if (customServerModelsJson == NULL)
        return;

    /* getChatModels returns a simple array of model ID strings */
    if (!json_object_is_type(customServerModelsJson, json_type_array)) {
        return;
    }

    LONG modelCount = json_object_array_length(customServerModelsJson);
    STRPTR currentModel = NULL;
    get(customServerChatModelString, MUIA_String_Contents, &currentModel);
    LONG selectedIndex = MUIV_NList_Active_Off;

    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelIdObj =
            json_object_array_get_idx(customServerModelsJson, i);
        CONST_STRPTR modelId = json_object_get_string(modelIdObj);

        if (modelId != NULL) {
            DoMethod(customServerModelList, MUIM_NList_InsertSingle, modelId,
                     MUIV_NList_Insert_Bottom);

            /* Check if this is the currently selected model */
            if (currentModel != NULL && strcmp(modelId, currentModel) == 0) {
                selectedIndex = i;
            }
        }
    }

    /* Select the current model in the list if found */
    if (selectedIndex != MUIV_NList_Active_Off) {
        set(customServerModelList, MUIA_NList_Active, selectedIndex);
    }
}

HOOKPROTONHNONP(TemplateSelectedFunc, void) {
    if (settingsIsImageMode)
        return;
    LONG template;
    get(customServerTemplateCycle, MUIA_Cycle_Active, &template);

    switch (template) {
    case TEMPLATE_GOOGLE_GEMINI:
        set(customServerHostString, MUIA_String_Contents,
            "generativelanguage.googleapis.com");
        set(customServerPortString, MUIA_String_Integer, 443);
        set(customServerUsesSSLCycle, MUIA_Cycle_Active, 1); /* SSL enabled */
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            AUTHORIZATION_TYPE_NONE);
        set(customServerChatModelString, MUIA_String_Contents,
            "gemini-2.5-flash");
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            API_ENDPOINT_GEMINI_GENERATE_CONTENT);
        set(customServerApiEndpointUrlString, MUIA_String_Contents, "v1beta");
        break;
    case TEMPLATE_LM_STUDIO:
        set(customServerHostString, MUIA_String_Contents, "localhost");
        set(customServerPortString, MUIA_String_Integer, 1234);
        set(customServerUsesSSLCycle, MUIA_Cycle_Active, 0); /* No SSL */
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            AUTHORIZATION_TYPE_NONE);
        set(customServerChatModelString, MUIA_String_Contents, "");
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            API_ENDPOINT_RESPONSES);
        set(customServerApiEndpointUrlString, MUIA_String_Contents, "v1");
        break;
    case TEMPLATE_ANTHROPIC_CLAUDE:
        set(customServerHostString, MUIA_String_Contents, "api.anthropic.com");
        set(customServerPortString, MUIA_String_Integer, 443);
        set(customServerUsesSSLCycle, MUIA_Cycle_Active, 1); /* SSL enabled */
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            AUTHORIZATION_TYPE_X_API_KEY);
        set(customServerChatModelString, MUIA_String_Contents,
            "claude-opus-4-5-20251101");
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            API_ENDPOINT_MESSAGES);
        set(customServerApiEndpointUrlString, MUIA_String_Contents, "v1");
        set(customServerCustomHeadersString, MUIA_String_Contents,
            "anthropic-version: 2023-06-01");
        break;
    case TEMPLATE_XAI_GROK:
        set(customServerHostString, MUIA_String_Contents, "api.x.ai");
        set(customServerPortString, MUIA_String_Integer, 443);
        set(customServerUsesSSLCycle, MUIA_Cycle_Active, 1); /* SSL enabled */
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            AUTHORIZATION_TYPE_BEARER);
        set(customServerChatModelString, MUIA_String_Contents, "grok-4");
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            API_ENDPOINT_CHAT_COMPLETIONS);
        set(customServerApiEndpointUrlString, MUIA_String_Contents, "v1");
        set(customServerCustomHeadersString, MUIA_String_Contents, "");
        break;
    default:
        /* TEMPLATE_NONE - do nothing */
        break;
    }

    /* Reset template cycle back to "None" after applying */
    if (template != TEMPLATE_NONE) {
        set(customServerTemplateCycle, MUIA_Cycle_Active, TEMPLATE_NONE);

        /* Clear fetched models list since server changed */
        if (customServerModelList != NULL) {
            DoMethod(customServerModelList, MUIM_NList_Clear);
        }
        if (customServerModelsJson != NULL) {
            json_object_put(customServerModelsJson);
            customServerModelsJson = NULL;
        }
    }

    refreshUrlPreview();
}
MakeHook(TemplateSelectedHook, TemplateSelectedFunc);

/**
 * Update the URL preview string only. Does not set profileSettingsDirty.
 * Used when the window opens so opening alone does not mark settings dirty.
 */
SAVEDS void refreshUrlPreview(void) {
    PSDPRINTF("refreshUrlPreview enter (imageMode=%ld)",
              (long)settingsIsImageMode);
    /* Safety check: don't access gadgets if they're not ready */
    if (customServerHostString == NULL || customServerPortString == NULL ||
        customServerUsesSSLCycle == NULL ||
        customServerApiEndpointUrlString == NULL ||
        customServerFullUrlPreviewString == NULL)
        return;

    STRPTR customServerHost = NULL;
    get(customServerHostString, MUIA_String_Contents, &customServerHost);
    LONG port = 0;
    get(customServerPortString, MUIA_String_Integer, &port);
    LONG usesSSL = 0;
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    STRPTR customServerApiEndpointUrl = NULL;
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &customServerApiEndpointUrl);
    LONG apiEndpointActive = 0;
    if (customServerApiEndpointCycle != NULL) {
        get(customServerApiEndpointCycle, MUIA_Cycle_Active,
            &apiEndpointActive);
    }

    UBYTE fullUrlPreviewString[512];
    CONST_STRPTR endpointUrl = customServerApiEndpointUrl
                                   ? (CONST_STRPTR)customServerApiEndpointUrl
                                   : "";
    CONST_STRPTR host = customServerHost ? (CONST_STRPTR)customServerHost : "";
    CONST_STRPTR endpointPath = NULL;
    if (settingsIsImageMode) {
        endpointPath =
            (apiEndpointActive == 1)
                ? API_ENDPOINT_NAMES[API_ENDPOINT_GEMINI_GENERATE_CONTENT]
                : API_ENDPOINT_NAMES[API_ENDPOINT_IMAGES_GENERATIONS];
    } else {
        endpointPath = API_ENDPOINT_NAMES[apiEndpointActive];
    }
    snprintf(fullUrlPreviewString, sizeof(fullUrlPreviewString),
             "%s://%s:%ld%s%s/%s", usesSSL == 1 ? "https" : "http", host,
             (long)port,
             (endpointUrl != NULL && strlen(endpointUrl) > 0) ? "/" : "",
             endpointUrl, endpointPath);
    set(customServerFullUrlPreviewString, MUIA_Text_Contents,
        fullUrlPreviewString);
    PSDPRINTF("refreshUrlPreview exit: %s", fullUrlPreviewString);
}

HOOKPROTONHNONP(SettingsChangedFunc, void) {
    if (!loadingProfile)
        profileSettingsDirty = TRUE;
    refreshUrlPreview();
}
MakeHook(SettingsChangedHook, SettingsChangedFunc);

HOOKPROTONHNONP(RefreshUrlPreviewOnlyFunc, void) { refreshUrlPreview(); }
MakeHook(RefreshUrlPreviewOnlyHook, RefreshUrlPreviewOnlyFunc);

HOOKPROTONHNONP(CustomServerSettingsRequesterOkButtonClickedFunc, void) {
    /* Get the currently selected profile/provider */
    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);

    /* If there are unsaved changes, prompt before applying and closing */
    if (profileSettingsDirty && active != MUIV_NList_Active_Off) {
        LONG res = MUI_Request(app, customServerSettingsRequesterWindowObject,
#ifdef __MORPHOS__
                               NULL,
#else
                               MUIV_Requester_Image_Info,
#endif
                               "Save changes?", "*_Yes|_No|_Cancel",
                               STRING_SAVE_CHANGES_PROMPT, TAG_DONE);
        if (res == 3) {
            /* Cancel - stay in window */
            return;
        }
        if (res == 1) {
            /* Yes - save current form to storage */
            loadingProfile = TRUE;
            if (!applyProfileFromFormToStorage(active, FALSE, -1)) {
                displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
                loadingProfile = FALSE;
                return;
            }
            loadingProfile = FALSE;
        }
        if (res == 2) {
            /* No - close without saving */
            profileSettingsDirty = FALSE;
            set(customServerSettingsRequesterWindowObject, MUIA_Window_Open,
                FALSE);
            return;
        }
        /* Yes - clear dirty and fall through to apply and close */
        profileSettingsDirty = FALSE;
    }

    /* Get common values from UI */
    STRPTR customServerApiKey;
    get(customServerApiKeyString, MUIA_String_Contents, &customServerApiKey);

    STRPTR customServerChatModel;
    get(customServerChatModelString, MUIA_String_Contents,
        &customServerChatModel);
    LONG streamingEnabled = 0;
    if (!settingsIsImageMode && customServerStreamingCycle != NULL)
        get(customServerStreamingCycle, MUIA_Cycle_Active, &streamingEnabled);

    /* Handle built-in providers */
    if (isBuiltinProviderIndex(active)) {
        Provider provider = (Provider)getProviderFromIndex(active);

        if (settingsIsImageMode) {
            configSetImageProvider(provider);
        } else {
            configSetChatProvider(provider);
        }

        /* Save the API key for this provider */
        switch (provider) {
        case PROVIDER_OPENAI:
            configSetOpenAiApiKey(customServerApiKey);
            break;
        case PROVIDER_GEMINI:
            configSetGeminiApiKey(customServerApiKey);
            break;
        case PROVIDER_GROK:
            configSetGrokApiKey(customServerApiKey);
            break;
        case PROVIDER_ANTHROPIC:
            configSetAnthropicApiKey(customServerApiKey);
            break;
        default:
            break;
        }

        /* Save the model name */
        if (settingsIsImageMode) {
            configSetImageModelName(customServerChatModel);
        } else {
            configSetChatModelName(customServerChatModel);
            configSetChatStreamingEnabledForProvider(provider,
                                                     streamingEnabled == 1);
        }

        set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
        return;
    }

    /* Handle custom provider (original behavior) */
    if (settingsIsImageMode) {
        configSetImageProvider(PROVIDER_CUSTOM);
    } else {
        configSetChatProvider(PROVIDER_CUSTOM);
    }

    STRPTR customServerHost;
    get(customServerHostString, MUIA_String_Contents, &customServerHost);
    if (settingsIsImageMode)
        configSetCustomImageHost(customServerHost);
    else
        configSetCustomHost(customServerHost);

    LONG port;
    get(customServerPortString, MUIA_String_Integer, &port);
    if (port < 0 || port > 65535) {
        displayError(STRING_ERROR_INVALID_PORT);
        return;
    }
    if (settingsIsImageMode)
        configSetCustomImagePort(port);
    else
        configSetCustomPort(port);

    LONG usesSSL;
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    if (settingsIsImageMode)
        configSetCustomImageUseSSL(usesSSL == 1);
    else
        configSetCustomUseSSL(usesSSL == 1);

    LONG authorizationType;
    get(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
        &authorizationType);
    if (settingsIsImageMode)
        configSetCustomImageAuthorizationType(authorizationType);
    else
        configSetCustomAuthorizationType(authorizationType);

    if (settingsIsImageMode)
        configSetCustomImageApiKey(customServerApiKey);
    else
        configSetCustomApiKey(customServerApiKey);
    if (settingsIsImageMode) {
        configSetCustomImageModel(customServerChatModel);
        configSetImageModelName(customServerChatModel);
    } else {
        configSetCustomChatModel(customServerChatModel);
        configSetChatModelName(customServerChatModel);
        configSetCustomChatStreamEnabled(streamingEnabled == 1);
    }

    if (!settingsIsImageMode) {
        LONG apiEndpoint;
        get(customServerApiEndpointCycle, MUIA_Cycle_Active, &apiEndpoint);
        configSetCustomApiEndpoint(apiEndpoint);
    }

    STRPTR customServerApiEndpointUrl;
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &customServerApiEndpointUrl);
    if (settingsIsImageMode)
        configSetCustomImageApiEndpointUrl(customServerApiEndpointUrl);
    else
        configSetCustomApiEndpointUrl(customServerApiEndpointUrl);

    if (settingsIsImageMode) {
        LONG imageEndpointActive = 0;
        if (customServerApiEndpointCycle != NULL)
            get(customServerApiEndpointCycle, MUIA_Cycle_Active,
                &imageEndpointActive);
        ULONG imageEndpoint = (imageEndpointActive == 1)
                                  ? (ULONG)API_ENDPOINT_GEMINI_GENERATE_CONTENT
                                  : (ULONG)API_ENDPOINT_IMAGES_GENERATIONS;
        configSetImageApiEndpoint(imageEndpoint);
    }

    STRPTR customServerCustomHeaders;
    get(customServerCustomHeadersString, MUIA_String_Contents,
        &customServerCustomHeaders);
    if (settingsIsImageMode)
        configSetCustomImageHeaders(customServerCustomHeaders);
    else
        configSetCustomHeaders(customServerCustomHeaders);

    set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(CustomServerSettingsRequesterOkButtonClickedHook,
         CustomServerSettingsRequesterOkButtonClickedFunc);

/**
 * Create the provider settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createCustomServerSettingsRequesterWindow() {
    static STRPTR templateOptions[6] = {NULL};
    templateOptions[TEMPLATE_NONE] = STRING_MENU_TEMPLATE_NONE;
    templateOptions[TEMPLATE_GOOGLE_GEMINI] =
        STRING_MENU_TEMPLATE_GOOGLE_GEMINI;
    templateOptions[TEMPLATE_LM_STUDIO] = STRING_MENU_TEMPLATE_LM_STUDIO;
    templateOptions[TEMPLATE_ANTHROPIC_CLAUDE] =
        STRING_MENU_TEMPLATE_ANTHROPIC_CLAUDE;
    templateOptions[TEMPLATE_XAI_GROK] = STRING_MENU_TEMPLATE_XAI_GROK;

    static STRPTR sslOptions[3] = {NULL};
    sslOptions[0] = STRING_ENCRYPTION_NONE;
    sslOptions[1] = STRING_ENCRYPTION_SSL;

    chatApiEndpointOptions[API_ENDPOINT_RESPONSES] =
        (STRPTR)API_ENDPOINT_NAMES[API_ENDPOINT_RESPONSES];
    chatApiEndpointOptions[API_ENDPOINT_CHAT_COMPLETIONS] =
        (STRPTR)API_ENDPOINT_NAMES[API_ENDPOINT_CHAT_COMPLETIONS];
    chatApiEndpointOptions[API_ENDPOINT_MESSAGES] =
        (STRPTR)API_ENDPOINT_NAMES[API_ENDPOINT_MESSAGES];
    chatApiEndpointOptions[API_ENDPOINT_GEMINI_GENERATE_CONTENT] =
        (STRPTR)API_ENDPOINT_NAMES[API_ENDPOINT_GEMINI_GENERATE_CONTENT];

    /* Image endpoint options (index-based, not enum-based) */
    imageApiEndpointOptions[0] =
        (STRPTR)API_ENDPOINT_NAMES[API_ENDPOINT_IMAGES_GENERATIONS];
    imageApiEndpointOptions[1] =
        (STRPTR)API_ENDPOINT_NAMES[API_ENDPOINT_GEMINI_GENERATE_CONTENT];

    static STRPTR authorizationTypeOptions[4] = {NULL};
    authorizationTypeOptions[AUTHORIZATION_TYPE_NONE] =
        STRING_AUTHORIZATION_NONE;
    authorizationTypeOptions[AUTHORIZATION_TYPE_BEARER] =
        STRING_AUTHORIZATION_BEARER;
    authorizationTypeOptions[AUTHORIZATION_TYPE_X_API_KEY] =
        STRING_AUTHORIZATION_X_API_KEY;
    static STRPTR streamingOptions[3] = {NULL};
    streamingOptions[0] = STRING_OFF;
    streamingOptions[1] = STRING_ON;

    Object *customServerSettingsRequesterOkButton,
        *customServerSettingsRequesterCancelButton;
    if ((customServerSettingsRequesterWindowObject = WindowObject,
        MUIA_Window_Title, STRING_CHAT_PROVIDER_SETTINGS,
        MUIA_Window_Width, 500,
        MUIA_Window_Height, MUIV_Window_Height_Default,
        MUIA_Window_CloseGadget, FALSE,
        WindowContents, customServerSettingsRootGroup = HGroup,
            /* Left panel - Profiles */
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_PROFILES,
                MUIA_FixWidth, 150,
                Child, NListviewObject,
                    MUIA_NListview_NList, customServerProfileList = NListObject,
                        MUIA_NList_ConstructHook2, &ConstructProfileLI_TextHook,
                        MUIA_NList_DestructHook2, &DestructProfileLI_TextHook,
                        MUIA_NList_DisplayHook2, &DisplayProfileLI_TextHook,
                        MUIA_NList_Format, "",
                        MUIA_NList_Title, FALSE,
                        MUIA_NList_MinLineHeight, 16,
                    End,
                    MUIA_CycleChain, TRUE,
                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                End,
                Child, customServerProfileNameString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_MaxLen, 64,
                End,
                Child, customServerSaveProfileButton =
                    MUI_MakeObject(MUIO_Button, STRING_SAVE_PROFILE),
                Child, customServerDeleteProfileButton =
                    MUI_MakeObject(MUIO_Button, STRING_DELETE_PROFILE),
            End,
            /* Right panel - Settings (two columns to reduce height) */
            Child, VGroup,
                Child, HGroup,
                    /* Left column */
                    Child, VGroup,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_TEMPLATE,
                            Child, customServerTemplateCycle = CycleObject,
                                MUIA_CycleChain, TRUE,
                                MUIA_Cycle_Entries, templateOptions,
                                MUIA_Cycle_Active, TEMPLATE_NONE,
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PROXY_HOST,
                            Child, customServerHostString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, configGetCustomHost(),
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PROXY_PORT,
                            Child, customServerPortString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Accept, "0123456789",
                                MUIA_String_MaxLen, 5,
                                MUIA_String_Integer, (LONG)configGetCustomPort(),
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PROXY_ENCRYPTION,
                            Child, customServerUsesSSLCycle = CycleObject,
                                MUIA_CycleChain, TRUE,
                                MUIA_Cycle_Entries, sslOptions,
                                MUIA_Cycle_Active, configGetCustomUseSSL() ? 1 : 0,
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_AUTHORIZATION_TYPE,
                            Child, customServerAuthorizationTypeCycle = CycleObject,
                                MUIA_CycleChain, TRUE,
                                MUIA_Cycle_Entries, authorizationTypeOptions,
                                MUIA_Cycle_Active, configGetCustomAuthorizationType(),
                            End,
                        End,
                        Child, customServerStreamingGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_STREAMING,
                            Child, customServerStreamingCycle = CycleObject,
                                MUIA_CycleChain, TRUE,
                                MUIA_Cycle_Entries, streamingOptions,
                                MUIA_Cycle_Active,
                                    configGetCustomChatStreamEnabled() ? 1 : 0,
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_OPENAI_API_KEY,
                            Child, customServerApiKeyString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, configGetCustomApiKey(),
                                MUIA_String_MaxLen, 256,
                            End,
                        End,
                    End,
                    /* Right column */
                    Child, VGroup,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_OPENAI_CHAT_MODEL,
                            Child, customServerChatModelString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, configGetCustomChatModel() != NULL ? configGetCustomChatModel() : "gemini-2.0-flash",
                            End,
                            Child, HGroup,
                                Child, customServerFetchModelsButton = MUI_MakeObject(
                                    MUIO_Button, STRING_FETCH_MODELS, MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify, TAG_DONE),
                            End,
                            Child, NListviewObject,
                                MUIA_NListview_NList, customServerModelList = NListObject,
                                    MUIA_NList_ConstructHook2, &ConstructCustomModelLI_TextHook,
                                    MUIA_NList_DestructHook2, &DestructCustomModelLI_TextHook,
                                    MUIA_NList_DisplayHook2, &DisplayCustomModelLI_TextHook,
                                    MUIA_NList_Format, "",
                                    MUIA_NList_Title, FALSE,
                                    MUIA_NList_MinLineHeight, 16,
                                End,
                                MUIA_CycleChain, TRUE,
                                MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                                MUIA_FixHeight, 80,
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_OPENAI_API_ENDPOINT,
                            Child, customServerApiEndpointCycle = CycleObject,
                                MUIA_CycleChain, TRUE,
                                MUIA_Cycle_Entries, chatApiEndpointOptions,
                                MUIA_Cycle_Active, configGetCustomApiEndpoint(),
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_OPENAI_API_ENDPOINT_URL,
                            Child, customServerApiEndpointUrlString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, configGetCustomApiEndpointUrl() != NULL ? configGetCustomApiEndpointUrl() : "v1",
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_CUSTOM_HEADERS,
                            Child, customServerCustomHeadersString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, configGetCustomHeaders() != NULL ? configGetCustomHeaders() : "",
                                MUIA_String_MaxLen, 512,
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_OPENAI_FULL_URL_PREVIEW,
                            Child, customServerFullUrlPreviewString = TextObject,
                                MUIA_Frame, MUIV_Frame_String,
                            End,
                        End,
                    End,
                End,
                Child, HGroup,
                    Child, customServerSettingsRequesterOkButton = MUI_MakeObject(MUIO_Button, STRING_OK,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                    Child, customServerSettingsRequesterCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                End,
            End, /* End right panel VGroup */
        End, /* End outer HGroup */
    End) == NULL) {
        displayError(STRING_ERROR_CUSTOM_PROVIDER_SETTINGS);
        return RETURN_ERROR;
    }
    DoMethod(customServerSettingsRequesterOkButton, MUIM_Notify, MUIA_Pressed,
             FALSE, MUIV_Notify_Window, 2, MUIM_CallHook,
             &CustomServerSettingsRequesterOkButtonClickedHook);
    DoMethod(customServerSettingsRequesterCancelButton, MUIM_Notify,
             MUIA_Pressed, FALSE, MUIV_Notify_Window, 3, MUIM_Set,
             MUIA_Window_Open, FALSE);
    DoMethod(customServerSettingsRequesterWindowObject, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &ProfileSelectedHook);
    DoMethod(customServerSettingsRequesterWindowObject, MUIM_Notify, MUIA_Window_Open, TRUE, MUIV_Notify_Application, 2, MUIM_CallHook, &RefreshUrlPreviewOnlyHook);
    DoMethod(customServerTemplateCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &TemplateSelectedHook);
    DoMethod(customServerHostString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerPortString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerUsesSSLCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerAuthorizationTypeCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerStreamingCycle, MUIM_Notify, MUIA_Cycle_Active,
             MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook,
             &SettingsChangedHook);
    DoMethod(customServerApiKeyString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiEndpointCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiEndpointUrlString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerChatModelString, MUIM_Notify, MUIA_String_Contents,
             MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook,
             &SettingsChangedHook);
    DoMethod(customServerProfileNameString, MUIM_Notify, MUIA_String_Contents,
             MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook,
             &SettingsChangedHook);
    DoMethod(customServerFetchModelsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &FetchCustomModelsHook);
    DoMethod(customServerModelList, MUIM_Notify, MUIA_NList_Active,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &CustomModelSelectedHook);

    /* Initialize profile list BEFORE setting up notification hooks */
    loadProfilesFromConfig();
    populateProfileList(-1);

    /* Profile management hooks */
    DoMethod(customServerProfileList, MUIM_Notify, MUIA_NList_Active,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &ProfileSelectedHook);
    DoMethod(customServerSaveProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &SaveProfileHook);
    DoMethod(customServerDeleteProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &DeleteProfileHook);

    /* Prefill fields for the initially selected profile/provider by
     * re-applying the current Active index, which will trigger the
     * ProfileSelectedHook via the existing MUI notification. */
    {
        LONG active = MUIV_NList_Active_Off;
        get(customServerProfileList, MUIA_NList_Active, &active);
        if (active != MUIV_NList_Active_Off) {
            set(customServerProfileList, MUIA_NList_Active, active);
        }
    }

    return RETURN_OK;
}

static void applyProviderSettingsWindowMode(BOOL isImageMode) {
    settingsIsImageMode = isImageMode ? TRUE : FALSE;

    if (customServerSettingsRequesterWindowObject != NULL) {
        set(customServerSettingsRequesterWindowObject, MUIA_Window_Title,
            settingsIsImageMode ? STRING_IMAGE_PROVIDER_SETTINGS
                                : STRING_CHAT_PROVIDER_SETTINGS);
    }

    /* Chat-only UI: Streaming + selectable API endpoint */
    if (customServerSettingsRootGroup != NULL)
        DoMethod(customServerSettingsRootGroup, MUIM_Group_InitChange);
    if (customServerStreamingGroup != NULL) {
        set(customServerStreamingGroup, MUIA_ShowMe,
            settingsIsImageMode ? FALSE : TRUE);
    }
    if (customServerApiEndpointCycle != NULL) {
        if (settingsIsImageMode) {
            set(customServerApiEndpointCycle, MUIA_Cycle_Entries,
                imageApiEndpointOptions);
            set(customServerApiEndpointCycle, MUIA_Disabled, FALSE);
        } else {
            set(customServerApiEndpointCycle, MUIA_Cycle_Entries,
                chatApiEndpointOptions);
            set(customServerApiEndpointCycle, MUIA_Disabled, FALSE);
        }
    }
    if (customServerSettingsRootGroup != NULL)
        DoMethod(customServerSettingsRootGroup, MUIM_Group_ExitChange);

    if (customServerTemplateCycle != NULL) {
        set(customServerTemplateCycle, MUIA_Disabled,
            settingsIsImageMode ? TRUE : FALSE);
    }

    /* Seed fields for "(New Profile)" from mode-specific custom settings */
    if (settingsIsImageMode) {
        set(customServerHostString, MUIA_String_Contents,
            configGetCustomImageHost() ? configGetCustomImageHost() : "");
        set(customServerPortString, MUIA_String_Integer,
            (LONG)configGetCustomImagePort());
        set(customServerUsesSSLCycle, MUIA_Cycle_Active,
            configGetCustomImageUseSSL() ? 1 : 0);
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            (LONG)configGetCustomImageAuthorizationType());
        set(customServerApiKeyString, MUIA_String_Contents,
            configGetCustomImageApiKey() ? configGetCustomImageApiKey() : "");
        set(customServerChatModelString, MUIA_String_Contents,
            configGetCustomImageModel() ? configGetCustomImageModel() : "");
        set(customServerApiEndpointUrlString, MUIA_String_Contents,
            configGetCustomImageApiEndpointUrl()
                ? configGetCustomImageApiEndpointUrl()
                : "v1");
        set(customServerCustomHeadersString, MUIA_String_Contents,
            configGetCustomImageHeaders() ? configGetCustomImageHeaders() : "");
    } else {
        set(customServerHostString, MUIA_String_Contents,
            configGetCustomHost() ? configGetCustomHost() : "");
        set(customServerPortString, MUIA_String_Integer,
            (LONG)configGetCustomPort());
        set(customServerUsesSSLCycle, MUIA_Cycle_Active,
            configGetCustomUseSSL() ? 1 : 0);
        set(customServerAuthorizationTypeCycle, MUIA_Cycle_Active,
            (LONG)configGetCustomAuthorizationType());
        set(customServerApiKeyString, MUIA_String_Contents,
            configGetCustomApiKey() ? configGetCustomApiKey() : "");
        set(customServerChatModelString, MUIA_String_Contents,
            configGetCustomChatModel() ? configGetCustomChatModel() : "");
        set(customServerApiEndpointUrlString, MUIA_String_Contents,
            configGetCustomApiEndpointUrl() ? configGetCustomApiEndpointUrl()
                                            : "v1");
        set(customServerCustomHeadersString, MUIA_String_Contents,
            configGetCustomHeaders() ? configGetCustomHeaders() : "");
        if (customServerStreamingCycle != NULL)
            set(customServerStreamingCycle, MUIA_Cycle_Active,
                configGetCustomChatStreamEnabled() ? 1 : 0);
        if (customServerApiEndpointCycle != NULL)
            set(customServerApiEndpointCycle, MUIA_Cycle_Active,
                (LONG)configGetCustomApiEndpoint());
    }

    /* Rebuild profile list with mode-appropriate built-ins */
    loadingProfile = TRUE;
    suppressProfileSelected = TRUE;
    profileSettingsDirty = FALSE;
    lastSelectedProfile = -1;
    loadProfilesFromConfig();
    populateProfileList(-1);
    suppressProfileSelected = FALSE;
    loadingProfile = FALSE;
    /* Force selection hook to run once after list rebuild */
    if (customServerProfileList != NULL) {
        LONG active = MUIV_NList_Active_Off;
        get(customServerProfileList, MUIA_NList_Active, &active);
        set(customServerProfileList, MUIA_NList_Active, MUIV_NList_Active_Off);
        set(customServerProfileList, MUIA_NList_Active, active);
    }
}

void openChatProviderSettingsRequesterWindow(void) {
    applyProviderSettingsWindowMode(FALSE);
    set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, TRUE);
}

void openImageProviderSettingsRequesterWindow(void) {
    applyProviderSettingsWindowMode(TRUE);
    set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, TRUE);
}