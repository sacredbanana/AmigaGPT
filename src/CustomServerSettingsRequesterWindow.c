#include <exec/exec.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/NFloattext_mcc.h>
#include <mui/Guigfx_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <stdlib.h>
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
Object *customServerNewProfileButton;
Object *customServerDuplicateProfileButton;
Object *customServerDeleteProfileButton;

/* Profiles list entry (so names can update live from JSON) */
typedef struct ProfileListEntry {
    BOOL isBuiltin;
    Provider provider;
    struct json_object *profile; /* only for custom profiles */
} ProfileListEntry;

/* Root group for layout changes */
static Object *customServerSettingsRootGroup = NULL;
/* Frame group containing the Streaming option (chat-only) */
static Object *customServerStreamingGroup = NULL;

/* API endpoint options (chat vs image) */
static STRPTR chatApiEndpointOptions[6] = {NULL};
static STRPTR imageApiEndpointOptions[3] = {NULL};

static struct json_object *customServerModelsJson = NULL;
static struct json_object *profilesJson = NULL;

/* In-window pending (not persisted until OK) */
static STRPTR pendingBuiltinApiKeys[PROVIDER_COUNT] = {0};
static STRPTR pendingBuiltinChatModels[PROVIDER_COUNT] = {0};
static BOOL pendingBuiltinStreaming[PROVIDER_COUNT] = {0};
static STRPTR pendingImageModelName = NULL; /* image model name is global */

static BOOL profileSettingsDirty = FALSE;
static LONG lastSelectedProfile = -1;
static BOOL loadingProfile = FALSE;
static BOOL settingsIsImageMode = FALSE;
static BOOL suppressProfileSelected = FALSE;

/* Forward declarations */
static void populateModelList(void);
static void populateProfileList(LONG forceActive);
static void loadProfilesFromConfig(void);
static void saveProfilesToConfig(void);
static BOOL applyProfileFromFormToStorage(LONG profileListIndex,
                                          BOOL setActiveAfterSave,
                                          LONG forceActiveForList);
SAVEDS void refreshUrlPreview(void);

static void updateApiKeyEnabledFromAuthType(void) {
    if (customServerAuthorizationTypeCycle == NULL ||
        customServerApiKeyString == NULL)
        return;
    LONG authType = 0;
    get(customServerAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
    set(customServerApiKeyString, MUIA_Disabled,
        (authType == AUTHORIZATION_TYPE_NONE) ? TRUE : FALSE);
}

static BOOL testJsonIndicatesError(struct json_object *json) {
    if (json == NULL)
        return FALSE;

    /* Common OpenAI-compatible error shapes:
     * - {"error": {...}}
     * - {"type":"error", ...}
     */
    struct json_object *err = NULL;
    if (json_object_object_get_ex(json, "error", &err) && err != NULL) {
        return TRUE;
    }
    struct json_object *typeObj = NULL;
    if (json_object_object_get_ex(json, "type", &typeObj) && typeObj != NULL) {
        CONST_STRPTR type = json_object_get_string(typeObj);
        if (type != NULL && strcasecmp(type, "error") == 0)
            return TRUE;
    }
    return FALSE;
}

/* Test result popup windows (created lazily) */
static Object *customServerTestResultWindowObject = NULL;
static Object *customServerTestResultFloattextObject = NULL;
static STRPTR customServerTestResultTextBuffer = NULL;
static Object *customServerTestImageWindowObject = NULL;
static Object *customServerTestImageViewGroup = NULL;
static Object *customServerTestImageView = NULL;

static void showTestResultTextWithStatus(CONST_STRPTR title, BOOL success,
                                         CONST_STRPTR statusDetail,
                                         CONST_STRPTR text) {
    if (app == NULL)
        return;

    if (customServerTestResultWindowObject == NULL) {
        Object *closeButton = NULL;
        if ((customServerTestResultWindowObject = WindowObject,
             MUIA_Window_Title,
             title != NULL ? title : (CONST_STRPTR)STRING_TEST,
             MUIA_Window_CloseGadget, TRUE, MUIA_Window_SizeGadget, TRUE,
             MUIA_Window_DepthGadget, TRUE, MUIA_Window_DragBar, TRUE,
             MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
             MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered, WindowContents,
             VGroup, Child, NListviewObject, MUIA_NListview_Horiz_ScrollBar,
             MUIV_NListview_HSB_None, MUIA_NListview_Vert_ScrollBar,
             MUIV_NListview_VSB_Auto, MUIA_NListview_NList,
             customServerTestResultFloattextObject = NFloattextObject,
             MUIA_Font,
             configGetFixedWidthFonts() ? MUIV_NList_Font_Fixed
                                        : MUIV_NList_Font,
             MUIA_Frame, MUIV_Frame_Text, MUIA_ContextMenu, NULL,
             MUIA_NFloattext_Text, "", End, End, Child, HGroup, Child,
             closeButton = MUI_MakeObject(MUIO_Button, STRING_OK, TAG_DONE),
             End, End, End) == NULL) {
            return;
        }

        DoMethod(customServerTestResultWindowObject, MUIM_Notify,
                 MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Window_Open, FALSE);
        DoMethod(closeButton, MUIM_Notify, MUIA_Pressed, FALSE,
                 MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_Open, FALSE);

        DoMethod(app, OM_ADDMEMBER, customServerTestResultWindowObject);
    }

    set(customServerTestResultWindowObject, MUIA_Window_Title,
        title != NULL ? title : (CONST_STRPTR)STRING_TEST);

    /* Build wrapped display text (no horizontal scrolling). */
    if (customServerTestResultTextBuffer != NULL) {
        FreeVec(customServerTestResultTextBuffer);
        customServerTestResultTextBuffer = NULL;
    }
    if (statusDetail == NULL)
        statusDetail = "";
    CONST_STRPTR statusWord = success ? (CONST_STRPTR)STRING_TEST_PASS
                                      : (CONST_STRPTR)STRING_TEST_FAIL;
    CONST_STRPTR body = (text != NULL) ? text : (CONST_STRPTR) "";

    /* Strip CR characters so wrapping looks right on CRLF JSON. */
    ULONG bodyLen = strlen(body);
    STRPTR bodyNoCR = AllocVec(bodyLen + 1, MEMF_ANY | MEMF_CLEAR);
    if (bodyNoCR != NULL) {
        ULONG j = 0;
        for (ULONG i = 0; i < bodyLen; i++) {
            if (body[i] != '\r')
                bodyNoCR[j++] = body[i];
        }
        bodyNoCR[j] = '\0';
        body = bodyNoCR;
    }

    char statusLine[256];
    snprintf(statusLine, sizeof(statusLine), "%s%s%s\n\n", statusWord,
             (statusDetail[0] != '\0') ? ": " : "", statusDetail);

    ULONG totalLen = strlen(statusLine) + strlen(body) + 1;
    customServerTestResultTextBuffer =
        AllocVec(totalLen, MEMF_ANY | MEMF_CLEAR);
    if (customServerTestResultTextBuffer != NULL) {
        strcpy(customServerTestResultTextBuffer, statusLine);
        strcat(customServerTestResultTextBuffer, body);
        if (customServerTestResultFloattextObject != NULL) {
            set(customServerTestResultFloattextObject, MUIA_NFloattext_Text,
                customServerTestResultTextBuffer);
        }
    }
    if (bodyNoCR != NULL)
        FreeVec(bodyNoCR);

    set(customServerTestResultWindowObject, MUIA_Window_Open, TRUE);
}

static void showTestResultText(CONST_STRPTR title, CONST_STRPTR text) {
    /* Legacy helper: treat non-empty text as success, no detail line. */
    showTestResultTextWithStatus(title, (text != NULL && text[0] != '\0'), NULL,
                                 text);
}

static void showTestImage(CONST_STRPTR title, CONST_STRPTR filePath) {
    if (app == NULL || filePath == NULL || strlen(filePath) == 0)
        return;

    if (customServerTestImageWindowObject == NULL) {
        Object *closeButton = NULL;
        if ((customServerTestImageWindowObject = WindowObject,
             MUIA_Window_Title,
             title != NULL ? title : (CONST_STRPTR)STRING_IMAGE_TEST_TITLE,
             MUIA_Window_CloseGadget, TRUE, MUIA_Window_SizeGadget, TRUE,
             MUIA_Window_DepthGadget, TRUE, MUIA_Window_DragBar, TRUE,
             MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
             MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered, WindowContents,
             VGroup, Child, customServerTestImageViewGroup = VGroup, Child,
             customServerTestImageView = GuigfxObject, MUIA_Guigfx_FileName,
             filePath, MUIA_Guigfx_Quality, MUIV_Guigfx_Quality_Low,
             MUIA_Guigfx_ScaleMode, NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE,
             MUIA_Guigfx_Transparency, NITRF_MASK, End, End, Child, HGroup,
             Child,
             closeButton = MUI_MakeObject(MUIO_Button, STRING_OK, TAG_DONE),
             End, End, End) == NULL) {
            return;
        }

        DoMethod(customServerTestImageWindowObject, MUIM_Notify,
                 MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Window_Open, FALSE);
        DoMethod(closeButton, MUIM_Notify, MUIA_Pressed, FALSE,
                 MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_Open, FALSE);

        DoMethod(app, OM_ADDMEMBER, customServerTestImageWindowObject);
    }

    set(customServerTestImageWindowObject, MUIA_Window_Title,
        title != NULL ? title : (CONST_STRPTR)STRING_IMAGE_TEST_TITLE);

    /* Recreate image view so it reloads the file */
    if (customServerTestImageViewGroup != NULL &&
        customServerTestImageView != NULL) {
        DoMethod(customServerTestImageViewGroup, MUIM_Group_InitChange);
        DoMethod(customServerTestImageViewGroup, OM_REMMEMBER,
                 customServerTestImageView);
        MUI_DisposeObject(customServerTestImageView);
        customServerTestImageView = GuigfxObject, MUIA_Guigfx_FileName,
        filePath, MUIA_Guigfx_Quality, MUIV_Guigfx_Quality_Low,
        MUIA_Guigfx_ScaleMode, NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE,
        MUIA_Guigfx_Transparency, NITRF_MASK, End;
        DoMethod(customServerTestImageViewGroup, OM_ADDMEMBER,
                 customServerTestImageView);
        DoMethod(customServerTestImageViewGroup, MUIM_Group_MoveMember,
                 customServerTestImageView, 0);
        DoMethod(customServerTestImageViewGroup, MUIM_Group_ExitChange);
    }

    set(customServerTestImageWindowObject, MUIA_Window_Open, TRUE);
}

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

static STRPTR dupStrLocal(CONST_STRPTR s) {
    if (s == NULL)
        return NULL;
    ULONG len = strlen(s);
    STRPTR out = AllocVec(len + 1, MEMF_CLEAR);
    if (out != NULL)
        strncpy(out, s, len);
    return out;
}

static void freePendingBuiltins(void) {
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        if (pendingBuiltinApiKeys[i] != NULL) {
            FreeVec(pendingBuiltinApiKeys[i]);
            pendingBuiltinApiKeys[i] = NULL;
        }
        if (pendingBuiltinChatModels[i] != NULL) {
            FreeVec(pendingBuiltinChatModels[i]);
            pendingBuiltinChatModels[i] = NULL;
        }
        pendingBuiltinStreaming[i] = FALSE;
    }
    if (pendingImageModelName != NULL) {
        FreeVec(pendingImageModelName);
        pendingImageModelName = NULL;
    }
}

static void loadPendingBuiltinsFromConfig(void) {
    freePendingBuiltins();

    pendingBuiltinApiKeys[PROVIDER_OPENAI] =
        dupStrLocal(configGetOpenAiApiKey());
    pendingBuiltinApiKeys[PROVIDER_GEMINI] =
        dupStrLocal(configGetGeminiApiKey());
    pendingBuiltinApiKeys[PROVIDER_GROK] = dupStrLocal(configGetGrokApiKey());
    pendingBuiltinApiKeys[PROVIDER_ANTHROPIC] =
        dupStrLocal(configGetAnthropicApiKey());

    pendingBuiltinChatModels[PROVIDER_OPENAI] =
        dupStrLocal(configGetChatModelNameForProvider(PROVIDER_OPENAI));
    pendingBuiltinChatModels[PROVIDER_GEMINI] =
        dupStrLocal(configGetChatModelNameForProvider(PROVIDER_GEMINI));
    pendingBuiltinChatModels[PROVIDER_GROK] =
        dupStrLocal(configGetChatModelNameForProvider(PROVIDER_GROK));
    pendingBuiltinChatModels[PROVIDER_ANTHROPIC] =
        dupStrLocal(configGetChatModelNameForProvider(PROVIDER_ANTHROPIC));

    pendingBuiltinStreaming[PROVIDER_OPENAI] =
        configGetChatStreamingEnabledForProvider(PROVIDER_OPENAI);
    pendingBuiltinStreaming[PROVIDER_GEMINI] =
        configGetChatStreamingEnabledForProvider(PROVIDER_GEMINI);
    pendingBuiltinStreaming[PROVIDER_GROK] =
        configGetChatStreamingEnabledForProvider(PROVIDER_GROK);
    pendingBuiltinStreaming[PROVIDER_ANTHROPIC] =
        configGetChatStreamingEnabledForProvider(PROVIDER_ANTHROPIC);

    pendingImageModelName = dupStrLocal(configGetImageModelName());
}

/* Number of built-in providers shown in the profile list.
 * Chat mode: OpenAI, Gemini, Grok, Anthropic (4)
 * Image mode: OpenAI, Gemini, Grok (3) */
static LONG getBuiltinProviderCount(void) {
    return settingsIsImageMode ? 3 : 4;
}

/**
 * Check if a list index corresponds to a built-in provider
 * Index 0..builtinCount-1 = built-in providers, builtinCount+ = custom profiles
 */
static BOOL isBuiltinProviderIndex(LONG index) {
    return (index >= 0 && index < getBuiltinProviderCount());
}

/**
 * Get the Provider enum value from a list index
 * Returns -1 if not a built-in provider
 */
static LONG getProviderFromIndex(LONG index) {
    if (index < 0 || index >= getBuiltinProviderCount())
        return -1;
    return index; /* PROVIDER_OPENAI=0, PROVIDER_GEMINI=1, etc. */
}

/**
 * Enable or disable server/connection setting gadgets (host, port, SSL,
 * auth, endpoint, template, fetch models). API key and chat model stay
 * under caller control. Used to lock these when a built-in provider is
 * selected.
 */
static void setServerSettingsGadgetsEnabled(BOOL enabled) {
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
    updateApiKeyEnabledFromAuthType();
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
            pendingBuiltinStreaming[provider] ? 1 : 0);
    }

    /* Load API key for this provider */
    STRPTR apiKey = pendingBuiltinApiKeys[provider];
    set(customServerApiKeyString, MUIA_String_Contents, apiKey ? apiKey : "");

    /* Load the saved model */
    STRPTR modelName = NULL;
    CONST_STRPTR *modelList = NULL;
    if (settingsIsImageMode) {
        modelName = pendingImageModelName;
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
        modelName = pendingBuiltinChatModels[provider];
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

    /* Add built-in providers (non-deletable) */
    {
        ProfileListEntry e = {0};
        e.isBuiltin = TRUE;

        e.provider = PROVIDER_OPENAI;
        DoMethod(customServerProfileList, MUIM_NList_InsertSingle, (ULONG)&e,
                 MUIV_NList_Insert_Bottom);
        e.provider = PROVIDER_GEMINI;
        DoMethod(customServerProfileList, MUIM_NList_InsertSingle, (ULONG)&e,
                 MUIV_NList_Insert_Bottom);
        e.provider = PROVIDER_GROK;
        DoMethod(customServerProfileList, MUIM_NList_InsertSingle, (ULONG)&e,
                 MUIV_NList_Insert_Bottom);
    }
    if (!settingsIsImageMode) {
        ProfileListEntry e = {0};
        e.isBuiltin = TRUE;
        e.provider = PROVIDER_ANTHROPIC;
        DoMethod(customServerProfileList, MUIM_NList_InsertSingle, (ULONG)&e,
                 MUIV_NList_Insert_Bottom);
    }

    /* Add custom profiles from JSON */
    if (profilesJson != NULL &&
        json_object_is_type(profilesJson, json_type_array)) {
        int len = json_object_array_length(profilesJson);
        for (int i = 0; i < len; i++) {
            struct json_object *profile =
                json_object_array_get_idx(profilesJson, i);
            if (profile != NULL) {
                ProfileListEntry e = {0};
                e.isBuiltin = FALSE;
                e.profile = profile;
                DoMethod(customServerProfileList, MUIM_NList_InsertSingle,
                         (ULONG)&e, MUIV_NList_Insert_Bottom);
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
            set(customServerProfileList, MUIA_NList_Active, currentProvider);
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
                                    i + getBuiltinProviderCount());
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

    updateApiKeyEnabledFromAuthType();
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
 * NOTE: This does NOT persist anything to disk/config. It only updates the
 * in-window working copy (pending built-in values or profilesJson).
 */
static BOOL applyProfileFromFormToStorage(LONG profileListIndex,
                                          BOOL setActiveAfterSave,
                                          LONG forceActiveForList) {
    (void)setActiveAfterSave;

    if (profileListIndex == MUIV_NList_Active_Off) {
        return FALSE;
    }

    STRPTR apiKey = NULL;
    STRPTR modelStr = NULL;
    LONG streamingEnabled = 0;
    if (customServerApiKeyString != NULL)
        get(customServerApiKeyString, MUIA_String_Contents, &apiKey);
    if (customServerChatModelString != NULL)
        get(customServerChatModelString, MUIA_String_Contents, &modelStr);
    if (!settingsIsImageMode && customServerStreamingCycle != NULL)
        get(customServerStreamingCycle, MUIA_Cycle_Active, &streamingEnabled);

    if (isBuiltinProviderIndex(profileListIndex)) {
        Provider provider = (Provider)getProviderFromIndex(profileListIndex);

        if (pendingBuiltinApiKeys[provider] != NULL) {
            FreeVec(pendingBuiltinApiKeys[provider]);
            pendingBuiltinApiKeys[provider] = NULL;
        }
        pendingBuiltinApiKeys[provider] = dupStrLocal(apiKey);

        if (!settingsIsImageMode) {
            if (pendingBuiltinChatModels[provider] != NULL) {
                FreeVec(pendingBuiltinChatModels[provider]);
                pendingBuiltinChatModels[provider] = NULL;
            }
            pendingBuiltinChatModels[provider] = dupStrLocal(modelStr);
            pendingBuiltinStreaming[provider] = (streamingEnabled == 1);
        } else {
            if (pendingImageModelName != NULL) {
                FreeVec(pendingImageModelName);
                pendingImageModelName = NULL;
            }
            pendingImageModelName = dupStrLocal(modelStr);
        }

        if (forceActiveForList >= 0)
            populateProfileList(forceActiveForList);
        return TRUE;
    }

    /* Custom profile */
    LONG builtinCount = getBuiltinProviderCount();
    int profileIndex = (int)(profileListIndex - builtinCount);
    if (profilesJson == NULL ||
        !json_object_is_type(profilesJson, json_type_array) ||
        profileIndex < 0 ||
        profileIndex >= (int)json_object_array_length(profilesJson)) {
        return FALSE;
    }

    STRPTR profileName = NULL;
    get(customServerProfileNameString, MUIA_String_Contents, &profileName);
    if (profileName == NULL || strlen(profileName) == 0) {
        return FALSE;
    }

    /* Disallow names that match built-in providers */
    for (int i = 0; i < getBuiltinProviderCount() && PROVIDER_NAMES[i] != NULL;
         i++) {
        if (strcasecmp(profileName, PROVIDER_NAMES[i]) == 0) {
            displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
            return FALSE;
        }
    }

    /* Disallow duplicates among other custom profiles */
    int len = json_object_array_length(profilesJson);
    for (int i = 0; i < len; i++) {
        if (i == profileIndex)
            continue;
        struct json_object *p = json_object_array_get_idx(profilesJson, i);
        if (p == NULL)
            continue;
        struct json_object *no = json_object_object_get(p, "name");
        if (no == NULL)
            continue;
        CONST_STRPTR name = json_object_get_string(no);
        if (name != NULL && strcmp(name, profileName) == 0) {
            displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
            return FALSE;
        }
    }

    struct json_object *newProfile = createProfileFromUI(profileName);
    json_object_array_put_idx(profilesJson, profileIndex, newProfile);

    if (forceActiveForList >= 0)
        populateProfileList(forceActiveForList);
    return TRUE;
}

/* NList hooks for profiles */
HOOKPROTONHNO(ConstructProfileLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    ProfileListEntry *src = (ProfileListEntry *)ncm->entry;
    ProfileListEntry *copy =
        AllocVec(sizeof(ProfileListEntry), MEMF_ANY | MEMF_CLEAR);
    if (copy != NULL && src != NULL) {
        *copy = *src;
    }
    return copy;
}
MakeHook(ConstructProfileLI_TextHook, ConstructProfileLI_TextFunc);

HOOKPROTONHNO(DestructProfileLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    if (ndm->entry != NULL)
        FreeVec(ndm->entry);
}
MakeHook(DestructProfileLI_TextHook, DestructProfileLI_TextFunc);

HOOKPROTONHNO(DisplayProfileLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    ProfileListEntry *entry = (ProfileListEntry *)ndm->entry;
    if (entry == NULL) {
        ndm->strings[0] = (STRPTR) "";
        return;
    }
    if (entry->isBuiltin) {
        switch (entry->provider) {
        case PROVIDER_OPENAI:
            ndm->strings[0] = (STRPTR)STRING_PROVIDER_OPENAI;
            break;
        case PROVIDER_GEMINI:
            ndm->strings[0] = (STRPTR)STRING_PROVIDER_GEMINI;
            break;
        case PROVIDER_GROK:
            ndm->strings[0] = (STRPTR)STRING_PROVIDER_GROK;
            break;
        case PROVIDER_ANTHROPIC:
            ndm->strings[0] = (STRPTR)STRING_PROVIDER_ANTHROPIC;
            break;
        default:
            ndm->strings[0] = (STRPTR) "";
            break;
        }
        return;
    }
    if (entry->profile != NULL) {
        struct json_object *nameObj =
            json_object_object_get(entry->profile, "name");
        if (nameObj != NULL) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            ndm->strings[0] = (STRPTR)(name != NULL ? name : "");
            return;
        }
    }
    ndm->strings[0] = (STRPTR) "";
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

    /* Auto-apply edits to the previous selection into the in-window working
     * copy before switching. */
    if (profileSettingsDirty && lastSelectedProfile >= 0 &&
        active != lastSelectedProfile && active != MUIV_NList_Active_Off) {
        loadingProfile = TRUE;
        if (!applyProfileFromFormToStorage(lastSelectedProfile, FALSE, -1)) {
            displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
            set(customServerProfileList, MUIA_NList_Active,
                lastSelectedProfile);
            loadingProfile = FALSE;
            return;
        }
        loadingProfile = FALSE;
        profileSettingsDirty = FALSE;
    }

    lastSelectedProfile = active;
    loadingProfile = TRUE;

    if (active == MUIV_NList_Active_Off) {
        /* No selection */
        if (customServerDeleteProfileButton != NULL)
            set(customServerDeleteProfileButton, MUIA_Disabled, TRUE);
        if (customServerDuplicateProfileButton != NULL)
            set(customServerDuplicateProfileButton, MUIA_Disabled, TRUE);
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

        /* Built-ins cannot be deleted; profile name is read-only info. */
        if (customServerDeleteProfileButton != NULL)
            set(customServerDeleteProfileButton, MUIA_Disabled, TRUE);
        if (customServerDuplicateProfileButton != NULL)
            set(customServerDuplicateProfileButton, MUIA_Disabled, FALSE);
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

    /* Custom profile selected */
    if (customServerDeleteProfileButton != NULL)
        set(customServerDeleteProfileButton, MUIA_Disabled, FALSE);
    if (customServerDuplicateProfileButton != NULL)
        set(customServerDuplicateProfileButton, MUIA_Disabled, FALSE);
    if (customServerProfileNameString != NULL)
        set(customServerProfileNameString, MUIA_Disabled, FALSE);
    setServerSettingsGadgetsEnabled(TRUE);

    /* Load the selected custom profile
     * Custom profiles start after built-in providers */
    int profileIndex = (int)(active - getBuiltinProviderCount());
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

static BOOL isNameReservedForBuiltinProviders(CONST_STRPTR profileName) {
    if (profileName == NULL)
        return TRUE;
    for (int i = 0; i < getBuiltinProviderCount() && PROVIDER_NAMES[i] != NULL;
         i++) {
        if (strcasecmp(profileName, PROVIDER_NAMES[i]) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL isCustomProfileNameTaken(CONST_STRPTR profileName,
                                     LONG excludeIndex) {
    if (profilesJson == NULL ||
        !json_object_is_type(profilesJson, json_type_array) ||
        profileName == NULL)
        return FALSE;
    int len = json_object_array_length(profilesJson);
    for (int i = 0; i < len; i++) {
        if (excludeIndex >= 0 && i == (int)excludeIndex)
            continue;
        struct json_object *p = json_object_array_get_idx(profilesJson, i);
        if (p == NULL)
            continue;
        struct json_object *no = json_object_object_get(p, "name");
        if (no == NULL)
            continue;
        CONST_STRPTR name = json_object_get_string(no);
        if (name != NULL && strcmp(name, profileName) == 0)
            return TRUE;
    }
    return FALSE;
}

static STRPTR makeUniqueProfileName(CONST_STRPTR base, CONST_STRPTR suffix) {
    if (base == NULL)
        base = STRING_DEFAULT_NEW_PROFILE_NAME;
    if (suffix == NULL)
        suffix = "";

    /* base + suffix + optional " 2" ... */
    for (int n = 1; n < 1000; n++) {
        char candidate[128];
        if (n == 1) {
            snprintf(candidate, sizeof(candidate), "%s%s", base, suffix);
        } else {
            snprintf(candidate, sizeof(candidate), "%s%s %d", base, suffix, n);
        }
        if (isNameReservedForBuiltinProviders(candidate))
            continue;
        if (!isCustomProfileNameTaken(candidate, -1)) {
            return dupStrLocal(candidate);
        }
    }
    return dupStrLocal(STRING_DEFAULT_NEW_PROFILE_NAME);
}

/* + New */
HOOKPROTONHNONP(NewProfileFunc, void) {
    if (profilesJson == NULL ||
        !json_object_is_type(profilesJson, json_type_array)) {
        profilesJson = json_object_new_array();
    }

    STRPTR newName = makeUniqueProfileName(STRING_DEFAULT_NEW_PROFILE_NAME, "");
    struct json_object *profile = json_object_new_object();
    json_object_object_add(profile, "name", json_object_new_string(newName));

    /* Start blank (do not inherit last-used custom host). */
    if (settingsIsImageMode) {
        json_object_object_add(profile, "host", json_object_new_string(""));
        json_object_object_add(profile, "port", json_object_new_int(443));
        json_object_object_add(profile, "useSSL",
                               json_object_new_boolean(TRUE));
        json_object_object_add(
            profile, "authorizationType",
            json_object_new_int((LONG)AUTHORIZATION_TYPE_BEARER));
        json_object_object_add(profile, "apiKey", json_object_new_string(""));
        json_object_object_add(profile, "chatModel",
                               json_object_new_string(""));
        json_object_object_add(profile, "imageModel",
                               json_object_new_string(""));
        json_object_object_add(
            profile, "apiEndpoint",
            json_object_new_int((LONG)API_ENDPOINT_IMAGES_GENERATIONS));
        json_object_object_add(profile, "apiEndpointUrl",
                               json_object_new_string("v1"));
        json_object_object_add(profile, "customHeaders",
                               json_object_new_string(""));
    } else {
        json_object_object_add(profile, "host", json_object_new_string(""));
        json_object_object_add(profile, "port", json_object_new_int(443));
        json_object_object_add(profile, "useSSL",
                               json_object_new_boolean(TRUE));
        json_object_object_add(
            profile, "authorizationType",
            json_object_new_int((LONG)AUTHORIZATION_TYPE_BEARER));
        json_object_object_add(profile, "apiKey", json_object_new_string(""));
        json_object_object_add(profile, "chatModel",
                               json_object_new_string(""));
        json_object_object_add(profile, "imageModel",
                               json_object_new_string(""));
        json_object_object_add(
            profile, "apiEndpoint",
            json_object_new_int((LONG)API_ENDPOINT_CHAT_COMPLETIONS));
        json_object_object_add(profile, "apiEndpointUrl",
                               json_object_new_string("v1"));
        json_object_object_add(profile, "customHeaders",
                               json_object_new_string(""));
        json_object_object_add(profile, "streaming",
                               json_object_new_boolean(FALSE));
    }

    json_object_array_add(profilesJson, profile);
    FreeVec(newName);

    LONG builtinCount = getBuiltinProviderCount();
    LONG newListIndex =
        builtinCount + (LONG)json_object_array_length(profilesJson) - 1;
    loadingProfile = TRUE;
    populateProfileList(newListIndex);
    loadingProfile = FALSE;

    if (customServerProfileList != NULL)
        set(customServerProfileList, MUIA_NList_Active, newListIndex);
    if (customServerSettingsRequesterWindowObject != NULL &&
        customServerProfileNameString != NULL) {
        SetAttrs(customServerSettingsRequesterWindowObject,
                 MUIA_Window_ActiveObject, customServerProfileNameString,
                 TAG_DONE);
    }
    profileSettingsDirty = FALSE;
}
MakeHook(NewProfileHook, NewProfileFunc);

/* Duplicate */
HOOKPROTONHNONP(DuplicateProfileFunc, void) {
    if (customServerProfileList == NULL ||
        customServerProfileNameString == NULL)
        return;

    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off)
        return;

    STRPTR currentName = NULL;
    get(customServerProfileNameString, MUIA_String_Contents, &currentName);
    if (currentName == NULL || strlen(currentName) == 0) {
        /* Fall back to provider name if built-in */
        if (isBuiltinProviderIndex(active)) {
            Provider p = (Provider)getProviderFromIndex(active);
            currentName = (STRPTR)PROVIDER_NAMES[p];
        }
    }
    STRPTR newName =
        makeUniqueProfileName(currentName, STRING_PROFILE_COPY_SUFFIX);

    if (profilesJson == NULL ||
        !json_object_is_type(profilesJson, json_type_array)) {
        profilesJson = json_object_new_array();
    }
    struct json_object *dupProfile = createProfileFromUI(newName);
    json_object_array_add(profilesJson, dupProfile);

    LONG builtinCount = getBuiltinProviderCount();
    LONG newListIndex =
        builtinCount + (LONG)json_object_array_length(profilesJson) - 1;
    loadingProfile = TRUE;
    populateProfileList(newListIndex);
    loadingProfile = FALSE;

    if (customServerProfileList != NULL)
        set(customServerProfileList, MUIA_NList_Active, newListIndex);
    if (customServerSettingsRequesterWindowObject != NULL &&
        customServerProfileNameString != NULL) {
        SetAttrs(customServerSettingsRequesterWindowObject,
                 MUIA_Window_ActiveObject, customServerProfileNameString,
                 TAG_DONE);
    }

    if (newName)
        FreeVec(newName);
    profileSettingsDirty = FALSE;
}
MakeHook(DuplicateProfileHook, DuplicateProfileFunc);

/* Hook for Delete Profile button */
HOOKPROTONHNONP(DeleteProfileFunc, void) {
    if (customServerProfileList == NULL)
        return;

    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);

    /* Built-in providers are non-deletable */
    if (active == MUIV_NList_Active_Off || isBuiltinProviderIndex(active)) {
        return;
    }

    int profileIndex = (int)(active - getBuiltinProviderCount());
    if (profileIndex < 0) {
        return;
    }

    if (profilesJson != NULL &&
        json_object_is_type(profilesJson, json_type_array)) {
        /* Remove from array */
        json_object_array_del_idx(profilesJson, profileIndex, 1);

        /* Refresh list and select the nearest entry */
        LONG builtinCount = getBuiltinProviderCount();
        LONG remainingCustom = (LONG)json_object_array_length(profilesJson);
        LONG newActive = 0;
        if (remainingCustom > 0) {
            LONG desired = active;
            LONG maxIdx = builtinCount + remainingCustom - 1;
            if (desired > maxIdx)
                desired = maxIdx;
            if (desired < builtinCount)
                desired = builtinCount;
            newActive = desired;
        } else {
            newActive = 0; /* first built-in */
        }
        populateProfileList(newActive);
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

HOOKPROTONHNONP(AuthTypeChangedFunc, void) {
    updateApiKeyEnabledFromAuthType();
    if (!loadingProfile)
        profileSettingsDirty = TRUE;
    refreshUrlPreview();
}
MakeHook(AuthTypeChangedHook, AuthTypeChangedFunc);

HOOKPROTONHNONP(ProfileNameChangedFunc, void) {
    if (!loadingProfile)
        profileSettingsDirty = TRUE;

    /* Update the underlying profile name immediately so the list reflects it.
     */
    if (customServerProfileList != NULL && profilesJson != NULL &&
        json_object_is_type(profilesJson, json_type_array)) {
        LONG active = MUIV_NList_Active_Off;
        get(customServerProfileList, MUIA_NList_Active, &active);
        if (active != MUIV_NList_Active_Off &&
            !isBuiltinProviderIndex(active)) {
            int profileIndex = (int)(active - getBuiltinProviderCount());
            if (profileIndex >= 0 &&
                profileIndex < (int)json_object_array_length(profilesJson)) {
                struct json_object *profile =
                    json_object_array_get_idx(profilesJson, profileIndex);
                if (profile != NULL) {
                    STRPTR newName = NULL;
                    get(customServerProfileNameString, MUIA_String_Contents,
                        &newName);
                    json_object_object_add(
                        profile, "name",
                        json_object_new_string(newName != NULL ? newName : ""));
                    /* Redraw active row */
                    DoMethod(customServerProfileList, MUIM_NList_Redraw,
                             MUIV_NList_Redraw_Active);
                }
            }
        }
    }

    refreshUrlPreview();
}
MakeHook(ProfileNameChangedHook, ProfileNameChangedFunc);

HOOKPROTONHNONP(RefreshUrlPreviewOnlyFunc, void) { refreshUrlPreview(); }
MakeHook(RefreshUrlPreviewOnlyHook, RefreshUrlPreviewOnlyFunc);

HOOKPROTONHNONP(CustomServerSettingsRequesterOkButtonClickedFunc, void) {
    /* Get the currently selected profile/provider */
    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);

    /* Always apply current form into the in-window working copy before
     * committing. */
    if (profileSettingsDirty && active != MUIV_NList_Active_Off) {
        loadingProfile = TRUE;
        if (!applyProfileFromFormToStorage(active, FALSE, -1)) {
            displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
            loadingProfile = FALSE;
            return;
        }
        loadingProfile = FALSE;
        profileSettingsDirty = FALSE;
    }

    /* Persist custom profiles list to config now (OK is the commit point). */
    saveProfilesToConfig();

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

        /* Save the API key for this provider from pending state */
        CONST_STRPTR apiKeyToSave = pendingBuiltinApiKeys[provider];
        switch (provider) {
        case PROVIDER_OPENAI:
            configSetOpenAiApiKey(apiKeyToSave);
            break;
        case PROVIDER_GEMINI:
            configSetGeminiApiKey(apiKeyToSave);
            break;
        case PROVIDER_GROK:
            configSetGrokApiKey(apiKeyToSave);
            break;
        case PROVIDER_ANTHROPIC:
            configSetAnthropicApiKey(apiKeyToSave);
            break;
        default:
            break;
        }

        /* Save the model/streaming */
        if (settingsIsImageMode) {
            CONST_STRPTR imageModelName =
                pendingImageModelName != NULL
                    ? pendingImageModelName
                    : (CONST_STRPTR)customServerChatModel;
            if (imageModelName != NULL)
                configSetImageModelName(imageModelName);

            /* Persist image endpoint selection too (even if disabled). */
            LONG imageEndpointActive = 0;
            if (customServerApiEndpointCycle != NULL)
                get(customServerApiEndpointCycle, MUIA_Cycle_Active,
                    &imageEndpointActive);
            ULONG imageEndpoint =
                (imageEndpointActive == 1)
                    ? (ULONG)API_ENDPOINT_GEMINI_GENERATE_CONTENT
                    : (ULONG)API_ENDPOINT_IMAGES_GENERATIONS;
            configSetImageApiEndpoint(imageEndpoint);
        } else {
            CONST_STRPTR modelToSave = pendingBuiltinChatModels[provider];
            if (modelToSave != NULL && strlen(modelToSave) > 0) {
                configSetChatModelNameForProvider(provider, modelToSave);
                configSetChatModelName(modelToSave);
            }
            configSetChatStreamingEnabledForProvider(
                provider, pendingBuiltinStreaming[provider]);
        }

        set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
        return;
    }

    /* Handle custom provider (commit current selection + fields) */
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

    /* Set active profile name for custom selection */
    if (customServerProfileNameString != NULL) {
        STRPTR profileName = NULL;
        get(customServerProfileNameString, MUIA_String_Contents, &profileName);
        if (profileName != NULL && strlen(profileName) > 0) {
            if (settingsIsImageMode)
                configSetActiveImageProfileName(profileName);
            else
                configSetActiveProfileName(profileName);
        }
    }

    set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(CustomServerSettingsRequesterOkButtonClickedHook,
         CustomServerSettingsRequesterOkButtonClickedFunc);

HOOKPROTONHNONP(CustomServerSettingsRequesterTestButtonClickedFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(customServerProfileList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off) {
        showTestResultTextWithStatus(STRING_TEST_RESULT_TITLE, FALSE,
                                     STRING_TEST_DETAIL_NO_PROFILE_SELECTED,
                                     (CONST_STRPTR) "");
        return;
    }

    /* Use current UI field values */
    STRPTR host = NULL, portStr = NULL, apiKey = NULL, modelName = NULL;
    STRPTR apiEndpointUrl = NULL, customHeaders = NULL;
    LONG sslActive = 0, authTypeActive = 0, apiEndpointActive = 0;

    get(customServerHostString, MUIA_String_Contents, &host);
    get(customServerPortString, MUIA_String_Contents, &portStr);
    get(customServerApiKeyString, MUIA_String_Contents, &apiKey);
    get(customServerChatModelString, MUIA_String_Contents, &modelName);
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &apiEndpointUrl);
    get(customServerCustomHeadersString, MUIA_String_Contents, &customHeaders);

    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &sslActive);
    get(customServerAuthorizationTypeCycle, MUIA_Cycle_Active, &authTypeActive);
    if (customServerApiEndpointCycle != NULL)
        get(customServerApiEndpointCycle, MUIA_Cycle_Active,
            &apiEndpointActive);

    UWORD port = (UWORD)(portStr != NULL ? atoi(portStr) : 0);
    BOOL useSSL = (sslActive == 1) ? TRUE : FALSE;
    AuthorizationType authType = (AuthorizationType)authTypeActive;
    CONST_STRPTR apiKeyToUse = (authType == AUTHORIZATION_TYPE_NONE)
                                   ? (CONST_STRPTR) ""
                                   : (CONST_STRPTR)apiKey;

    /* Proxy settings */
    BOOL useProxy = configGetProxyEnabled();
    CONST_STRPTR proxyHost = configGetProxyHost();
    UWORD proxyPort = (UWORD)configGetProxyPort();
    BOOL proxyUsesSSL = configGetProxyUsesSSL();
    BOOL proxyRequiresAuth = configGetProxyRequiresAuth();
    CONST_STRPTR proxyUsername = configGetProxyUsername();
    CONST_STRPTR proxyPassword = configGetProxyPassword();

    if (!settingsIsImageMode) {
        APIEndpoint apiEndpoint = (APIEndpoint)apiEndpointActive;
        struct Conversation *conv = newConversation();
        if (conv != NULL) {
            addTextToConversation(
                conv,
                (UTF8 *)"Reply to this message letting me know the API "
                        "connection was successful",
                (STRPTR) "user");
        }
        struct json_object **responses = postChatMessageToOpenAI(
            conv, host, port, useSSL, (CONST_STRPTR)modelName, apiKeyToUse,
            FALSE, useProxy, proxyHost, proxyPort, proxyUsesSSL,
            proxyRequiresAuth, proxyUsername, proxyPassword, FALSE, apiEndpoint,
            (CONST_STRPTR)apiEndpointUrl, authType,
            (CONST_STRPTR)customHeaders);

        if (responses != NULL && responses[0] != NULL) {
            BOOL ok = !testJsonIndicatesError(responses[0]);
            updateStatusBar(ok ? STRING_READY : STRING_ERROR,
                            ok ? greenPen : redPen);
            CONST_STRPTR jsonStr = json_object_to_json_string_ext(
                responses[0], JSON_C_TO_STRING_PRETTY);
            showTestResultTextWithStatus(
                STRING_CHAT_TEST_RESULT_TITLE, ok,
                ok ? STRING_TEST_DETAIL_RECEIVED_JSON_RESPONSE
                   : STRING_TEST_DETAIL_ERROR_RESPONSE_RECEIVED,
                jsonStr);
        } else {
            updateStatusBar(STRING_ERROR, redPen);
            showTestResultTextWithStatus(
                STRING_CHAT_TEST_RESULT_TITLE, FALSE,
                STRING_TEST_DETAIL_NO_JSON_RESPONSE_RECEIVED,
                (CONST_STRPTR) "");
        }

        if (responses != NULL) {
            for (int i = 0; responses[i] != NULL; i++)
                json_object_put(responses[i]);
            FreeVec(responses);
        }
        if (conv != NULL)
            freeConversation(conv);
        return;
    }

    /* Image mode test */
    APIEndpoint imageEndpoint = (apiEndpointActive == 1)
                                    ? API_ENDPOINT_GEMINI_GENERATE_CONTENT
                                    : API_ENDPOINT_IMAGES_GENERATIONS;
    ImageFormat imageFormat = configGetImageFormat();

    struct json_object *resp = postImageCreationRequestToOpenAIWithServer(
        (CONST_STRPTR) "an Amiga boing ball", host, port, useSSL,
        (CONST_STRPTR)apiEndpointUrl, authType, (CONST_STRPTR)customHeaders,
        (CONST_STRPTR)modelName, IMAGE_SIZE_512x512, apiKeyToUse, useProxy,
        proxyHost, proxyPort, proxyUsesSSL, proxyRequiresAuth, proxyUsername,
        proxyPassword, imageFormat, PROVIDER_CUSTOM, imageEndpoint);

    if (resp == NULL) {
        updateStatusBar(STRING_ERROR, redPen);
        showTestResultTextWithStatus(
            STRING_IMAGE_TEST_RESULT_TITLE, FALSE,
            STRING_TEST_DETAIL_NO_JSON_RESPONSE_RECEIVED, (CONST_STRPTR) "");
        return;
    }

    CONST_STRPTR b64 = NULL;
    struct json_object *dataArr = json_object_object_get(resp, "data");
    if (dataArr != NULL && json_object_is_type(dataArr, json_type_array) &&
        json_object_array_length(dataArr) > 0) {
        struct json_object *first = json_object_array_get_idx(dataArr, 0);
        if (first != NULL) {
            struct json_object *b64Obj =
                json_object_object_get(first, "b64_json");
            if (b64Obj != NULL)
                b64 = json_object_get_string(b64Obj);
        }
    }

    if (b64 != NULL && strlen(b64) > 0) {
        LONG dataLen = 0;
        UBYTE *decoded = decodeBase64((UBYTE *)b64, &dataLen);
        if (decoded != NULL && dataLen > 0) {
            static ULONG testCounter = 0;
            UBYTE outPath[128];
            snprintf(outPath, sizeof(outPath),
                     "T:AmigaGPT_TestBoingBall_%lu.png", (ULONG)++testCounter);
            BPTR fh = Open(outPath, MODE_NEWFILE);
            if (fh != 0) {
                updateStatusBar(STRING_READY, greenPen);
                Write(fh, decoded, dataLen);
                Close(fh);
                showTestImage(STRING_IMAGE_TEST_RESULT_TITLE, outPath);
            } else {
                updateStatusBar(STRING_ERROR, redPen);
                CONST_STRPTR jsonStr = json_object_to_json_string_ext(
                    resp, JSON_C_TO_STRING_PLAIN);
                showTestResultTextWithStatus(
                    STRING_IMAGE_TEST_RESULT_TITLE, FALSE,
                    STRING_TEST_DETAIL_IMAGE_RECEIVED_COULD_NOT_SAVE_SHOWING_JSON,
                    jsonStr);
            }
        } else {
            updateStatusBar(STRING_ERROR, redPen);
            CONST_STRPTR jsonStr =
                json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
            showTestResultTextWithStatus(
                STRING_IMAGE_TEST_RESULT_TITLE, FALSE,
                STRING_TEST_DETAIL_IMAGE_DATA_DECODE_FAILED_SHOWING_JSON,
                jsonStr);
        }
        if (decoded != NULL)
            FreeVec(decoded);
    } else {
        updateStatusBar(STRING_ERROR, redPen);
        CONST_STRPTR jsonStr =
            json_object_to_json_string_ext(resp, JSON_C_TO_STRING_PLAIN);
        showTestResultTextWithStatus(
            STRING_IMAGE_TEST_RESULT_TITLE, FALSE,
            STRING_TEST_DETAIL_NO_IMAGE_DATA_RECEIVED_SHOWING_JSON, jsonStr);
    }

    json_object_put(resp);
}
MakeHook(CustomServerSettingsRequesterTestButtonClickedHook,
         CustomServerSettingsRequesterTestButtonClickedFunc);

/**
 * Create the provider settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createCustomServerSettingsRequesterWindow() {
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
        *customServerSettingsRequesterCancelButton,
        *customServerSettingsRequesterTestButton;
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
                        MUIA_NList_AutoVisible, TRUE,
                    End,
                    MUIA_CycleChain, TRUE,
                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                End,
                Child, HGroup,
                    Child, customServerNewProfileButton =
                        MUI_MakeObject(MUIO_Button, STRING_NEW),
                    Child, customServerDuplicateProfileButton =
                        MUI_MakeObject(MUIO_Button, STRING_DUPLICATE),
                    Child, customServerDeleteProfileButton =
                        MUI_MakeObject(MUIO_Button, STRING_DELETE_PROFILE),
                End,
            End,
            /* Right panel - Settings (two columns to reduce height) */
            Child, VGroup,
                Child, HGroup,
                    /* Left column */
                    Child, VGroup,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PROFILE_NAME,
                            Child, customServerProfileNameString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_MaxLen, 64,
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
                    Child, customServerSettingsRequesterTestButton = MUI_MakeObject(MUIO_Button, STRING_TEST,
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
    DoMethod(customServerSettingsRequesterTestButton, MUIM_Notify, MUIA_Pressed,
             FALSE, MUIV_Notify_Window, 2, MUIM_CallHook,
             &CustomServerSettingsRequesterTestButtonClickedHook);
    DoMethod(customServerSettingsRequesterCancelButton, MUIM_Notify,
             MUIA_Pressed, FALSE, MUIV_Notify_Window, 3, MUIM_Set,
             MUIA_Window_Open, FALSE);
    DoMethod(customServerSettingsRequesterWindowObject, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &ProfileSelectedHook);
    DoMethod(customServerSettingsRequesterWindowObject, MUIM_Notify, MUIA_Window_Open, TRUE, MUIV_Notify_Application, 2, MUIM_CallHook, &RefreshUrlPreviewOnlyHook);
    DoMethod(customServerHostString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerPortString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerUsesSSLCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerAuthorizationTypeCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &AuthTypeChangedHook);
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
             &ProfileNameChangedHook);
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
    DoMethod(customServerNewProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &NewProfileHook);
    DoMethod(customServerDuplicateProfileButton, MUIM_Notify, MUIA_Pressed,
             FALSE, MUIV_Notify_Application, 2, MUIM_CallHook,
             &DuplicateProfileHook);
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

    /* Reload in-window pending state from config each open. */
    loadPendingBuiltinsFromConfig();

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

    /* Update colored profile-management button labels */
    if (greenPen != 0 && redPen != 0 && bluePen != 0) {
        const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
        STRPTR buttonLabelText =
            AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY | MEMF_CLEAR);
        if (buttonLabelText != NULL) {
            if (customServerNewProfileButton != NULL) {
                snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                         "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW);
                set(customServerNewProfileButton, MUIA_Text_Contents,
                    buttonLabelText);
            }
            if (customServerDuplicateProfileButton != NULL) {
                snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                         "\33c\33P[%ld]%s\0", bluePen, STRING_DUPLICATE);
                set(customServerDuplicateProfileButton, MUIA_Text_Contents,
                    buttonLabelText);
            }
            if (customServerDeleteProfileButton != NULL) {
                snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                         "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_PROFILE);
                set(customServerDeleteProfileButton, MUIA_Text_Contents,
                    buttonLabelText);
            }
            FreeVec(buttonLabelText);
        }
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

    updateApiKeyEnabledFromAuthType();

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