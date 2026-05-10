#include "SpeechProviderSettingsRequesterWindow.h"

#ifdef DAEMON
Object *speechProviderSettingsRequesterWindowObject = NULL;

LONG createSpeechProviderSettingsRequesterWindow(void) { return RETURN_OK; }

void openSpeechProviderSettingsRequesterWindow(void) {}

#else

#include <dos/dos.h>
#include <exec/exec.h>
#include <json-c/json.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <utility/tagitem.h>
#include <mui/BetterString_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include <string.h>
#include "AmigaGPTConfig.h"
#include "MainWindow.h"
#include "gui.h"
#include "openai.h"
#include "speech.h"

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d)                                                    \
    (((ULONG)(a) << 24) | ((ULONG)(b) << 16) | ((ULONG)(c) << 8) | (ULONG)(d))
#endif

/* Some older MUI SDKs may not define this attribute.
 * If it's missing, treat it as TAG_IGNORE so the taglist still compiles. */
#ifndef MUIA_Window_AutoAdjust
#define MUIA_Window_AutoAdjust TAG_IGNORE
#endif

Object *speechProviderSettingsRequesterWindowObject = NULL;

static Object *speechProfileList = NULL;
static Object *speechProfileNameString = NULL;
static Object *speechSystemCycle = NULL;
static Object *speechConnectionGroup = NULL;
static Object *speechHostString = NULL;
static Object *speechPortString = NULL;
static Object *speechUsesSSLCycle = NULL;
static Object *speechAuthorizationTypeCycle = NULL;
static Object *speechEndpointUrlString = NULL;

/* OpenAI fields */
static Object *openAiApiKeyString = NULL;
static Object *openAiTtsCurrentModelText = NULL;
static Object *openAiTtsFetchModelsButton = NULL;
static Object *openAiTtsModelList = NULL;
static Object *openAiTtsVoiceCycle = NULL;
static Object *openAiVoiceInstructionsEditor = NULL;

/* ElevenLabs fields */
static Object *elevenLabsAPIKeyString = NULL;
static Object *elevenLabsModelList = NULL;
static Object *elevenLabsVoiceSearchString = NULL;
static Object *elevenLabsVoiceList = NULL;
static Object *elevenLabsSearchButton = NULL;
static Object *elevenLabsFetchModelsButton = NULL;
static Object *elevenLabsCurrentModelText = NULL;
static Object *elevenLabsCurrentVoiceText = NULL;

/* Workbench fields */
static Object *workbenchAccentString = NULL;
static Object *workbenchAccentBrowseButton = NULL;
static Object *workbenchWarningText = NULL;
static Object *workbenchRateString = NULL;
static Object *workbenchPitchString = NULL;
static Object *workbenchModeCycle = NULL;
static Object *workbenchSexCycle = NULL;

/* Flite fields */
static Object *fliteVoiceCycle = NULL;
static Object *fliteWarningText = NULL;

static Object *newProfileButton = NULL;
static Object *duplicateProfileButton = NULL;
static Object *deleteProfileButton = NULL;

static Object *rightScrollgroup = NULL;
static Object *rightVirtgroup = NULL;
static Object *customCommonGroup = NULL;
static Object *workbenchGroup = NULL;
static Object *fliteGroup = NULL;
static Object *openAiGroup = NULL;
static Object *elevenLabsGroup = NULL;
static Object *okButton = NULL;
static Object *cancelButton = NULL;
static Object *testButton = NULL;

static struct json_object *speechProfilesJson = NULL;
static struct json_object *builtinSpeechProfilesJson[8] = {NULL};
static struct json_object *elevenLabsModelsJson = NULL;
static struct json_object *elevenLabsVoicesJson = NULL;
static struct json_object *openAITTSModelsJson = NULL;
static BOOL suppressProfileSelected = FALSE;
static BOOL speechSettingsDirty = FALSE;
static LONG lastSelectedProfile = MUIV_NList_Active_Off;
static LONG pendingProfileListSelect = MUIV_NList_Active_Off;

static STRPTR speechSystemOptions[8] = {NULL};
static STRPTR speechSslOptions[3] = {NULL};
static STRPTR speechAuthorizationTypeOptions[6] = {NULL};

#ifdef __AMIGAOS3__
static STRPTR narratorModeOptions[3] = {NULL};
static STRPTR narratorSexOptions[3] = {NULL};
#endif

#define ELEVENLABS_HOST "api.elevenlabs.io"
#define ELEVENLABS_PORT 443

static BOOL speechSystemUsesNetwork(SpeechSystem sys) {
    return (sys == SPEECH_SYSTEM_OPENAI || sys == SPEECH_SYSTEM_ELEVENLABS);
}

static CONST_STRPTR defaultHostForSpeechSystem(SpeechSystem sys) {
    return (sys == SPEECH_SYSTEM_ELEVENLABS) ? ELEVENLABS_HOST
                                             : "api.openai.com";
}

static AuthorizationType defaultAuthForSpeechSystem(SpeechSystem sys) {
    return (sys == SPEECH_SYSTEM_ELEVENLABS) ? AUTHORIZATION_TYPE_XI_API_KEY
                                             : AUTHORIZATION_TYPE_BEARER;
}

static CONST_STRPTR headerNameForAuthorizationType(AuthorizationType authType) {
    switch (authType) {
    case AUTHORIZATION_TYPE_BEARER:
        return "Authorization";
    case AUTHORIZATION_TYPE_X_API_KEY:
        return "x-api-key";
    case AUTHORIZATION_TYPE_X_GOOGLE_API_KEY:
        return "x-goog-api-key";
    case AUTHORIZATION_TYPE_XI_API_KEY:
        return "xi-api-key";
    case AUTHORIZATION_TYPE_NONE:
    default:
        return "";
    }
}

static void loadProfilesFromConfig(void);
static void saveProfilesToConfig(void);
static void populateProfileList(void);
static void loadProfileIntoUI(LONG activeIndex);
static struct json_object *createDefaultProfile(CONST_STRPTR name);
static struct json_object *createBuiltinProfileFromConfig(SpeechSystem sys);
static struct json_object *createProfileFromUI(CONST_STRPTR name);
static STRPTR getOpenAiVoiceInstructionsText(BOOL *needsFree);
static struct json_object *getWorkingProfileForListIndex(LONG activeIndex);
static BOOL applyProfileFromUIToWorking(LONG activeIndex);
static void commitBuiltinProfileToConfig(struct json_object *profile);
static void updateProfileManagementButtonLabels(void);
static void reinitSpeechForActiveProfile(void);
static SpeechSystem getSpeechSystemFromCycle(void);
static void updateSpeechApiKeyStringEnabledFromAuth(SpeechSystem sys);
static void setFieldsEnabledForSystem(SpeechSystem sys, BOOL isCustomOrNew);
static void updateGroupVisibilityForSystem(SpeechSystem sys,
                                           BOOL isCustomOrNew);

static void beginRightPanelUpdate(void) {
    if (speechProviderSettingsRequesterWindowObject != NULL)
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Sleep,
            TRUE);
    if (rightVirtgroup != NULL)
        DoMethod(rightVirtgroup, MUIM_Group_InitChange);
}

static void endRightPanelUpdate(void) {
    if (rightVirtgroup != NULL)
        DoMethod(rightVirtgroup, MUIM_Group_ExitChange);
    if (speechProviderSettingsRequesterWindowObject != NULL)
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Sleep,
            FALSE);
}

static void updateProfileManagementButtonLabels(void) {
    if (greenPen == 0 || redPen == 0 || bluePen == 0)
        return;

    const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
    STRPTR buttonLabelText =
        AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY | MEMF_CLEAR);
    if (buttonLabelText == NULL)
        return;

    if (newProfileButton != NULL) {
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s",
                 greenPen, STRING_NEW);
        set(newProfileButton, MUIA_Text_Contents, buttonLabelText);
    }
    if (duplicateProfileButton != NULL) {
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s",
                 bluePen, STRING_DUPLICATE);
        set(duplicateProfileButton, MUIA_Text_Contents, buttonLabelText);
    }
    if (deleteProfileButton != NULL) {
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s",
                 redPen, STRING_DELETE_PROFILE);
        set(deleteProfileButton, MUIA_Text_Contents, buttonLabelText);
    }

    FreeVec(buttonLabelText);
}

static LONG getBuiltinSpeechProviderCount(void) {
    LONG count = 0;
#ifdef __AMIGAOS3__
    /* Workbench v34/v37 + OpenAI + ElevenLabs */
    count = 4;
#else
#ifdef __AMIGAOS4__
    /* Flite + OpenAI + ElevenLabs */
    count = 3;
#else
    /* MorphOS: OpenAI + ElevenLabs */
    count = 2;
#endif
#endif
    return count;
}

static BOOL isBuiltinListIndex(LONG idx) {
    return (idx >= 0 && idx < getBuiltinSpeechProviderCount());
}

static SpeechSystem builtinSystemForListIndex(LONG idx) {
#ifdef __AMIGAOS3__
    /* 0=v34, 1=v37, 2=OpenAI, 3=ElevenLabs */
    if (idx == 0)
        return SPEECH_SYSTEM_34;
    if (idx == 1)
        return SPEECH_SYSTEM_37;
    if (idx == 2)
        return SPEECH_SYSTEM_OPENAI;
    return SPEECH_SYSTEM_ELEVENLABS;
#else
#ifdef __AMIGAOS4__
    /* 0=Flite, 1=OpenAI, 2=ElevenLabs */
    if (idx == 0)
        return SPEECH_SYSTEM_FLITE;
    if (idx == 1)
        return SPEECH_SYSTEM_OPENAI;
    return SPEECH_SYSTEM_ELEVENLABS;
#else
    /* 0=OpenAI, 1=ElevenLabs */
    if (idx == 0)
        return SPEECH_SYSTEM_OPENAI;
    return SPEECH_SYSTEM_ELEVENLABS;
#endif
#endif
}

static CONST_STRPTR builtinNameForSystem(SpeechSystem sys) {
    /* Stored in config as a plain string; use SPEECH_SYSTEM_NAMES entries. */
    if (sys >= 0 && SPEECH_SYSTEM_NAMES[sys] != NULL)
        return SPEECH_SYSTEM_NAMES[sys];
    return SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI];
}

static BOOL profileNameIsBuiltin(CONST_STRPTR name) {
    if (name == NULL)
        return FALSE;
    for (LONG bi = 0; bi < getBuiltinSpeechProviderCount(); bi++) {
        CONST_STRPTR bn = builtinNameForSystem(builtinSystemForListIndex(bi));
        if (bn != NULL && strcmp(name, bn) == 0)
            return TRUE;
    }
    return FALSE;
}

static int profileArrayIndexForVisibleCustomIndex(int visibleIndex) {
    if (visibleIndex < 0 || speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array))
        return -1;
    int seen = 0;
    int len = json_object_array_length(speechProfilesJson);
    for (int i = 0; i < len; i++) {
        struct json_object *p = json_object_array_get_idx(speechProfilesJson, i);
        struct json_object *nameObj =
            p ? json_object_object_get(p, "name") : NULL;
        CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
        if (profileNameIsBuiltin(name))
            continue;
        if (seen == visibleIndex)
            return i;
        seen++;
    }
    return -1;
}

static LONG visibleCustomCount(void) {
    LONG count = 0;
    if (speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array))
        return 0;
    int len = json_object_array_length(speechProfilesJson);
    for (int i = 0; i < len; i++) {
        struct json_object *p = json_object_array_get_idx(speechProfilesJson, i);
        struct json_object *nameObj =
            p ? json_object_object_get(p, "name") : NULL;
        CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
        if (!profileNameIsBuiltin(name))
            count++;
    }
    return count;
}

static STRPTR dupStrLocal(CONST_STRPTR s) {
    if (s == NULL)
        return NULL;
    ULONG len = strlen(s);
    STRPTR copy = AllocVec(len + 1, MEMF_ANY | MEMF_CLEAR);
    if (copy != NULL)
        strcpy(copy, s);
    return copy;
}

static BOOL fileExists(CONST_STRPTR path) {
    if (path == NULL || strlen(path) == 0)
        return FALSE;
    BPTR f = Open(path, MODE_OLDFILE);
    if (f != 0) {
        Close(f);
        return TRUE;
    }
    return FALSE;
}

static void updateRequirementWarningForSystem(SpeechSystem sys,
                                              Object *warningTextObject) {
    if (warningTextObject == NULL)
        return;

    static UBYTE warningBuf[512];
    warningBuf[0] = '\0';

#ifdef __AMIGAOS3__
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        struct Library *TranslatorBase = OpenLibrary("translator.library", 43);
        if (TranslatorBase != NULL) {
            CloseLibrary(TranslatorBase);
            TranslatorBase = NULL;
        } else {
            UBYTE line[256];
            snprintf(line, sizeof(line), STRING_SPEECH_WARNING_MISSING_FORMAT,
                     "translator.library v43");
            strncat(warningBuf, line,
                    sizeof(warningBuf) - strlen(warningBuf) - 1);
        }

        CONST_STRPTR narratorPath =
            (sys == SPEECH_SYSTEM_34)
                ? "AMIGAGPT:devs/speech/34/narrator.device"
                : "AMIGAGPT:devs/speech/37/narrator.device";
        if (!fileExists(narratorPath)) {
            if (strlen(warningBuf) > 0)
                strncat(warningBuf, "\n",
                        sizeof(warningBuf) - strlen(warningBuf) - 1);
            UBYTE line[256];
            snprintf(line, sizeof(line), STRING_SPEECH_WARNING_MISSING_FORMAT,
                     narratorPath);
            strncat(warningBuf, line,
                    sizeof(warningBuf) - strlen(warningBuf) - 1);
        }
    }
#endif

#ifdef __AMIGAOS4__
    if (sys == SPEECH_SYSTEM_FLITE) {
        /* Minimal device check */
        struct MsgPort *port =
            (struct MsgPort *)AllocSysObject(ASOT_PORT, NULL);
        struct IOStdReq *req = NULL;
        if (port != NULL) {
            req = (struct IOStdReq *)AllocSysObjectTags(
                ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct IOStdReq),
                ASOIOR_ReplyPort, port, TAG_END);
        }
        if (req == NULL) {
            UBYTE line[256];
            snprintf(line, sizeof(line), STRING_SPEECH_WARNING_MISSING_FORMAT,
                     "flite.device");
            strncat(warningBuf, line,
                    sizeof(warningBuf) - strlen(warningBuf) - 1);
        } else {
            if (OpenDevice("flite.device", 0, (struct IORequest *)req, 0) !=
                0) {
                UBYTE line[256];
                snprintf(line, sizeof(line),
                         STRING_SPEECH_WARNING_MISSING_FORMAT, "flite.device");
                strncat(warningBuf, line,
                        sizeof(warningBuf) - strlen(warningBuf) - 1);
            } else {
                CloseDevice((struct IORequest *)req);
            }
        }
        if (req != NULL)
            FreeSysObject(ASOT_IOREQUEST, req);
        if (port != NULL)
            FreeSysObject(ASOT_PORT, port);
    }
#endif

    set(warningTextObject, MUIA_Text_Contents, warningBuf);
}

static struct json_object *
getElevenLabsModels(CONST_STRPTR host, UWORD port, BOOL useSSL,
                    CONST_STRPTR endpointUrl, AuthorizationType authType,
                    CONST_STRPTR apiKey) {
    UBYTE endpoint[512];
    if (host == NULL || strlen(host) == 0)
        host = ELEVENLABS_HOST;
    if (port == 0)
        port = ELEVENLABS_PORT;
    snprintf(endpoint, sizeof(endpoint), "/%s/models",
             (endpointUrl != NULL && strlen(endpointUrl) > 0) ? endpointUrl
                                                              : "v1");
    return makeHttpsGetRequest(
        host, port, useSSL, endpoint, apiKey,
        headerNameForAuthorizationType(authType),
        authType == AUTHORIZATION_TYPE_BEARER, configGetProxyEnabled(),
        configGetProxyHost(), configGetProxyPort(), configGetProxyUsesSSL(),
        configGetProxyRequiresAuth(), configGetProxyUsername(),
        configGetProxyPassword());
}

static void addDefaultElevenLabsModel(struct json_object *models,
                                      CONST_STRPTR modelId,
                                      CONST_STRPTR name) {
    struct json_object *model = json_object_new_object();
    json_object_object_add(model, "model_id",
                           json_object_new_string(modelId ? modelId : ""));
    json_object_object_add(model, "name",
                           json_object_new_string(name ? name : ""));
    json_object_array_add(models, model);
}

static void ensureDefaultElevenLabsModelsJson(void) {
    if (elevenLabsModelsJson != NULL)
        return;

    elevenLabsModelsJson = json_object_new_object();
    struct json_object *models = json_object_new_array();
    addDefaultElevenLabsModel(models, ELEVENLABS_MODEL_V3_ID,
                              ELEVENLABS_MODEL_V3_NAME);
    addDefaultElevenLabsModel(models, ELEVENLABS_MODEL_MULTILINGUAL_V2_ID,
                              ELEVENLABS_MODEL_MULTILINGUAL_V2_NAME);
    addDefaultElevenLabsModel(models, ELEVENLABS_MODEL_FLASH_V2_5_ID,
                              ELEVENLABS_MODEL_FLASH_V2_5_NAME);
    addDefaultElevenLabsModel(models, ELEVENLABS_MODEL_TURBO_V2_5_ID,
                              ELEVENLABS_MODEL_TURBO_V2_5_NAME);
    json_object_object_add(elevenLabsModelsJson, "models", models);
}

static void setElevenLabsModelListActiveById(CONST_STRPTR modelId) {
    if (elevenLabsModelList == NULL || elevenLabsModelsJson == NULL ||
        modelId == NULL)
        return;

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(elevenLabsModelsJson, "models",
                                   &modelsArray)) {
        if (json_object_is_type(elevenLabsModelsJson, json_type_array))
            modelsArray = elevenLabsModelsJson;
        else
            return;
    }

    LONG modelCount = json_object_array_length(modelsArray);
    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *modelIdObj = NULL;
        if (modelObj != NULL &&
            json_object_object_get_ex(modelObj, "model_id", &modelIdObj)) {
            CONST_STRPTR mid = json_object_get_string(modelIdObj);
            if (mid != NULL && strcmp(mid, modelId) == 0) {
                set(elevenLabsModelList, MUIA_NList_Active, i);
                return;
            }
        }
    }
}

static void addDefaultOpenAITTSModel(struct json_object *models,
                                     CONST_STRPTR modelId) {
    if (modelId == NULL)
        return;
    struct json_object *model = json_object_new_object();
    /* For OpenAI the id IS the human-friendly name. */
    json_object_object_add(model, "id", json_object_new_string(modelId));
    json_object_array_add(models, model);
}

static void ensureDefaultOpenAITTSModelsJson(void) {
    if (openAITTSModelsJson != NULL)
        return;

    openAITTSModelsJson = json_object_new_object();
    struct json_object *models = json_object_new_array();
    for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++)
        addDefaultOpenAITTSModel(models, OPENAI_TTS_MODEL_NAMES[i]);
    json_object_object_add(openAITTSModelsJson, "data", models);
}

static struct json_object *
getOpenAITTSModels(CONST_STRPTR host, UWORD port, BOOL useSSL,
                   CONST_STRPTR endpointUrl, AuthorizationType authType,
                   CONST_STRPTR apiKey) {
    /* getChatModels already targets /<endpointUrl>/models and handles auth /
     * proxy, so reuse it. The response is the raw OpenAI models payload. */
    return getChatModels((STRPTR)host, (ULONG)port, useSSL, apiKey,
                         configGetProxyEnabled(), configGetProxyHost(),
                         configGetProxyPort(), configGetProxyUsesSSL(),
                         configGetProxyRequiresAuth(), configGetProxyUsername(),
                         configGetProxyPassword(), endpointUrl, authType, NULL);
}

/* Build a "models cache" json object from an OpenAI /v1/models response,
 * keeping only entries whose id contains "tts" — the API doesn't expose a
 * modality flag, so this filter is the closest we can get. */
static struct json_object *
filterOpenAITTSModels(struct json_object *rawModels) {
    if (rawModels == NULL)
        return NULL;

    struct json_object *dataArray = NULL;
    if (!json_object_object_get_ex(rawModels, "data", &dataArray)) {
        if (json_object_is_type(rawModels, json_type_array))
            dataArray = rawModels;
        else
            return NULL;
    }
    if (!json_object_is_type(dataArray, json_type_array))
        return NULL;

    struct json_object *out = json_object_new_object();
    struct json_object *filtered = json_object_new_array();

    /* Always include the well-known TTS defaults so the user sees them even
     * if /v1/models on the configured host omits them. */
    for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++)
        addDefaultOpenAITTSModel(filtered, OPENAI_TTS_MODEL_NAMES[i]);

    LONG count = json_object_array_length(dataArray);
    for (LONG i = 0; i < count; i++) {
        struct json_object *entry = json_object_array_get_idx(dataArray, i);
        struct json_object *idObj = NULL;
        if (entry == NULL ||
            !json_object_object_get_ex(entry, "id", &idObj))
            continue;
        CONST_STRPTR id = json_object_get_string(idObj);
        if (id == NULL || strstr(id, "tts") == NULL)
            continue;

        /* Skip duplicates already added from the defaults. */
        BOOL alreadyPresent = FALSE;
        LONG existingCount = json_object_array_length(filtered);
        for (LONG j = 0; j < existingCount; j++) {
            struct json_object *existing =
                json_object_array_get_idx(filtered, j);
            struct json_object *existingId = NULL;
            if (existing == NULL ||
                !json_object_object_get_ex(existing, "id", &existingId))
                continue;
            CONST_STRPTR existingIdStr = json_object_get_string(existingId);
            if (existingIdStr != NULL && strcmp(existingIdStr, id) == 0) {
                alreadyPresent = TRUE;
                break;
            }
        }
        if (alreadyPresent)
            continue;

        addDefaultOpenAITTSModel(filtered, id);
    }
    json_object_object_add(out, "data", filtered);
    return out;
}

static void setOpenAITtsModelListActiveById(CONST_STRPTR modelId) {
    if (openAiTtsModelList == NULL || openAITTSModelsJson == NULL ||
        modelId == NULL)
        return;

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(openAITTSModelsJson, "data",
                                   &modelsArray)) {
        if (json_object_is_type(openAITTSModelsJson, json_type_array))
            modelsArray = openAITTSModelsJson;
        else
            return;
    }

    LONG modelCount = json_object_array_length(modelsArray);
    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *idObj = NULL;
        if (modelObj != NULL &&
            json_object_object_get_ex(modelObj, "id", &idObj)) {
            CONST_STRPTR mid = json_object_get_string(idObj);
            if (mid != NULL && strcmp(mid, modelId) == 0) {
                set(openAiTtsModelList, MUIA_NList_Active, i);
                return;
            }
        }
    }
}

static struct json_object *
searchElevenLabsVoices(CONST_STRPTR host, UWORD port, BOOL useSSL,
                       AuthorizationType authType, CONST_STRPTR apiKey,
                       CONST_STRPTR query) {
    UBYTE endpoint[512];
    if (host == NULL || strlen(host) == 0)
        host = ELEVENLABS_HOST;
    if (port == 0)
        port = ELEVENLABS_PORT;
    if (query != NULL && strlen(query) > 0) {
        snprintf(endpoint, sizeof(endpoint), "/v2/voices?search=%s", query);
    } else {
        snprintf(endpoint, sizeof(endpoint), "/v2/voices");
    }
    return makeHttpsGetRequest(
        host, port, useSSL, endpoint, apiKey,
        headerNameForAuthorizationType(authType),
        authType == AUTHORIZATION_TYPE_BEARER, configGetProxyEnabled(),
        configGetProxyHost(), configGetProxyPort(), configGetProxyUsesSSL(),
        configGetProxyRequiresAuth(), configGetProxyUsername(),
        configGetProxyPassword());
}

/* NList hooks for simple string lists (do not free entries) */
HOOKPROTONHNO(ConstructStringLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    return entry;
}
MakeHook(ConstructStringLI_TextHook, ConstructStringLI_TextFunc);

HOOKPROTONHNO(DestructStringLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    /* Don't free - strings owned elsewhere */
}
MakeHook(DestructStringLI_TextHook, DestructStringLI_TextFunc);

HOOKPROTONHNO(DisplayStringLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayStringLI_TextHook, DisplayStringLI_TextFunc);

static void populateOpenAITtsModelList(void) {
    if (openAiTtsModelList == NULL)
        return;
    DoMethod(openAiTtsModelList, MUIM_NList_Clear);
    ensureDefaultOpenAITTSModelsJson();
    if (openAITTSModelsJson == NULL)
        return;

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(openAITTSModelsJson, "data",
                                   &modelsArray)) {
        if (json_object_is_type(openAITTSModelsJson, json_type_array))
            modelsArray = openAITTSModelsJson;
        else
            return;
    }

    LONG modelCount = json_object_array_length(modelsArray);
    LONG selectedIndex = MUIV_NList_Active_Off;
    STRPTR currentModelId = configGetOpenAITTSModelId();

    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *idObj = NULL;
        if (modelObj == NULL)
            continue;
        if (!json_object_object_get_ex(modelObj, "id", &idObj))
            continue;
        CONST_STRPTR id = json_object_get_string(idObj);
        if (id == NULL)
            continue;
        DoMethod(openAiTtsModelList, MUIM_NList_InsertSingle, id,
                 MUIV_NList_Insert_Bottom);
        if (currentModelId != NULL && strcmp(currentModelId, id) == 0)
            selectedIndex = i;
    }

    if (selectedIndex != MUIV_NList_Active_Off)
        set(openAiTtsModelList, MUIA_NList_Active, selectedIndex);
}

static void populateElevenLabsModelList(void) {
    if (elevenLabsModelList == NULL)
        return;
    DoMethod(elevenLabsModelList, MUIM_NList_Clear);
    ensureDefaultElevenLabsModelsJson();
    if (elevenLabsModelsJson == NULL)
        return;

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(elevenLabsModelsJson, "models",
                                   &modelsArray)) {
        if (json_object_is_type(elevenLabsModelsJson, json_type_array)) {
            modelsArray = elevenLabsModelsJson;
        } else {
            return;
        }
    }

    LONG modelCount = json_object_array_length(modelsArray);
    LONG selectedIndex = MUIV_NList_Active_Off;
    STRPTR currentModelId = configGetElevenLabsModel();
    STRPTR currentModelName = NULL;
    if (elevenLabsCurrentModelText != NULL)
        get(elevenLabsCurrentModelText, MUIA_Text_Contents, &currentModelName);

    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *nameObj = NULL;
        struct json_object *modelIdObj = NULL;
        if (modelObj == NULL)
            continue;
        if (json_object_object_get_ex(modelObj, "name", &nameObj)) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            DoMethod(elevenLabsModelList, MUIM_NList_InsertSingle, name,
                     MUIV_NList_Insert_Bottom);
            if (currentModelId != NULL &&
                json_object_object_get_ex(modelObj, "model_id", &modelIdObj)) {
                CONST_STRPTR mid = json_object_get_string(modelIdObj);
                if (mid != NULL && strcmp(currentModelId, mid) == 0) {
                    selectedIndex = i;
                }
            }
            if (selectedIndex == MUIV_NList_Active_Off &&
                currentModelName != NULL &&
                strcmp(currentModelName, STRING_NONE_SELECTED) != 0 &&
                strcmp(currentModelName, name) == 0) {
                selectedIndex = i;
            }
        }
    }

    if (selectedIndex != MUIV_NList_Active_Off) {
        set(elevenLabsModelList, MUIA_NList_Active, selectedIndex);
    }
}

static void populateElevenLabsVoiceList(void) {
    if (elevenLabsVoiceList == NULL)
        return;
    DoMethod(elevenLabsVoiceList, MUIM_NList_Clear);
    if (elevenLabsVoicesJson == NULL)
        return;

    struct json_object *voicesArray = NULL;
    if (!json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                   &voicesArray)) {
        if (json_object_is_type(elevenLabsVoicesJson, json_type_array)) {
            voicesArray = elevenLabsVoicesJson;
        } else {
            return;
        }
    }

    LONG voiceCount = json_object_array_length(voicesArray);
    LONG selectedIndex = MUIV_NList_Active_Off;
    STRPTR currentVoiceID = configGetElevenLabsVoiceID();

    for (LONG i = 0; i < voiceCount; i++) {
        struct json_object *voiceObj =
            json_object_array_get_idx(voicesArray, i);
        struct json_object *nameObj = NULL;
        struct json_object *voiceIdObj = NULL;
        if (voiceObj == NULL)
            continue;
        if (json_object_object_get_ex(voiceObj, "name", &nameObj)) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            DoMethod(elevenLabsVoiceList, MUIM_NList_InsertSingle, name,
                     MUIV_NList_Insert_Bottom);
            if (currentVoiceID != NULL &&
                json_object_object_get_ex(voiceObj, "voice_id", &voiceIdObj)) {
                CONST_STRPTR vid = json_object_get_string(voiceIdObj);
                if (vid != NULL && strcmp(currentVoiceID, vid) == 0) {
                    selectedIndex = i;
                }
            }
        }
    }

    if (selectedIndex != MUIV_NList_Active_Off) {
        set(elevenLabsVoiceList, MUIA_NList_Active, selectedIndex);
    }
}

HOOKPROTONHNONP(OpenAITTSFetchModelsButtonClickedFunc, void) {
    STRPTR apiKey = NULL;
    if (openAiApiKeyString != NULL)
        get(openAiApiKeyString, MUIA_String_Contents, &apiKey);
    STRPTR host = NULL;
    LONG port = 443;
    LONG useSSL = 1;
    LONG authType = AUTHORIZATION_TYPE_BEARER;
    STRPTR endpointUrl = NULL;
    get(speechHostString, MUIA_String_Contents, &host);
    get(speechPortString, MUIA_String_Integer, &port);
    get(speechUsesSSLCycle, MUIA_Cycle_Active, &useSSL);
    get(speechAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
    get(speechEndpointUrlString, MUIA_String_Contents, &endpointUrl);

    if (((AuthorizationType)authType) != AUTHORIZATION_TYPE_NONE &&
        (apiKey == NULL || strlen(apiKey) == 0)) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }

    updateStatusBar(STRING_FETCHING_OPENAI_MODELS, yellowPen);
    set(openAiTtsFetchModelsButton, MUIA_Disabled, TRUE);

    struct json_object *raw = getOpenAITTSModels(
        host, (UWORD)port, useSSL == 1, endpointUrl,
        (AuthorizationType)authType, apiKey);
    struct json_object *filtered = filterOpenAITTSModels(raw);
    if (raw != NULL)
        json_object_put(raw);

    if (filtered != NULL) {
        if (openAITTSModelsJson != NULL)
            json_object_put(openAITTSModelsJson);
        openAITTSModelsJson = filtered;
        populateOpenAITtsModelList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_FETCHING_OPENAI_MODELS);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(openAiTtsFetchModelsButton, MUIA_Disabled, FALSE);
}
MakeHook(OpenAITTSFetchModelsButtonClickedHook,
         OpenAITTSFetchModelsButtonClickedFunc);

HOOKPROTONHNONP(ElevenLabsFetchModelsButtonClickedFunc, void) {
    STRPTR apiKey = NULL;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);
    STRPTR host = NULL;
    LONG port = ELEVENLABS_PORT;
    LONG useSSL = 1;
    LONG authType = AUTHORIZATION_TYPE_XI_API_KEY;
    STRPTR endpointUrl = NULL;
    get(speechHostString, MUIA_String_Contents, &host);
    get(speechPortString, MUIA_String_Integer, &port);
    get(speechUsesSSLCycle, MUIA_Cycle_Active, &useSSL);
    get(speechAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
    get(speechEndpointUrlString, MUIA_String_Contents, &endpointUrl);

    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }

    updateStatusBar(STRING_FETCHING_ELEVENLABS_MODELS, yellowPen);
    set(elevenLabsFetchModelsButton, MUIA_Disabled, TRUE);

    if (elevenLabsModelsJson != NULL) {
        json_object_put(elevenLabsModelsJson);
        elevenLabsModelsJson = NULL;
    }

    elevenLabsModelsJson =
        getElevenLabsModels(host, (UWORD)port, useSSL == 1, endpointUrl,
                            (AuthorizationType)authType, apiKey);
    if (elevenLabsModelsJson != NULL) {
        populateElevenLabsModelList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_FETCHING_ELEVENLABS_MODELS);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(elevenLabsFetchModelsButton, MUIA_Disabled, FALSE);
}
MakeHook(ElevenLabsFetchModelsButtonClickedHook,
         ElevenLabsFetchModelsButtonClickedFunc);

HOOKPROTONHNONP(ElevenLabsSearchVoicesButtonClickedFunc, void) {
    STRPTR apiKey = NULL;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);
    STRPTR host = NULL;
    LONG port = ELEVENLABS_PORT;
    LONG useSSL = 1;
    LONG authType = AUTHORIZATION_TYPE_XI_API_KEY;
    get(speechHostString, MUIA_String_Contents, &host);
    get(speechPortString, MUIA_String_Integer, &port);
    get(speechUsesSSLCycle, MUIA_Cycle_Active, &useSSL);
    get(speechAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }

    STRPTR query = NULL;
    get(elevenLabsVoiceSearchString, MUIA_String_Contents, &query);

    updateStatusBar(STRING_SEARCHING_ELEVENLABS_VOICES, yellowPen);
    set(elevenLabsSearchButton, MUIA_Disabled, TRUE);

    if (elevenLabsVoicesJson != NULL) {
        json_object_put(elevenLabsVoicesJson);
        elevenLabsVoicesJson = NULL;
    }

    elevenLabsVoicesJson = searchElevenLabsVoices(
        host, (UWORD)port, useSSL == 1, (AuthorizationType)authType, apiKey,
        query);
    if (elevenLabsVoicesJson != NULL) {
        populateElevenLabsVoiceList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_SEARCHING_ELEVENLABS_VOICES);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(elevenLabsSearchButton, MUIA_Disabled, FALSE);
}
MakeHook(ElevenLabsSearchVoicesButtonClickedHook,
         ElevenLabsSearchVoicesButtonClickedFunc);

static void updateElevenLabsSearchButtonLabel(void) {
    if (elevenLabsSearchButton == NULL || elevenLabsVoiceSearchString == NULL)
        return;
    STRPTR query = NULL;
    get(elevenLabsVoiceSearchString, MUIA_String_Contents, &query);
    set(elevenLabsSearchButton, MUIA_Text_Contents,
        (query != NULL && strlen(query) > 0) ? STRING_SEARCH
                                             : STRING_GET_ALL_VOICES);
}

HOOKPROTONHNONP(ElevenLabsVoiceSearchChangedFunc, void) {
    updateElevenLabsSearchButtonLabel();
}
MakeHook(ElevenLabsVoiceSearchChangedHook, ElevenLabsVoiceSearchChangedFunc);

static void loadProfilesFromConfig(void) {
    if (speechProfilesJson != NULL) {
        json_object_put(speechProfilesJson);
        speechProfilesJson = NULL;
    }
    STRPTR profilesStr = configGetSpeechProfiles();
    if (profilesStr != NULL && strlen(profilesStr) > 0) {
        speechProfilesJson = json_tokener_parse(profilesStr);
        if (speechProfilesJson == NULL ||
            !json_object_is_type(speechProfilesJson, json_type_array)) {
            if (speechProfilesJson != NULL)
                json_object_put(speechProfilesJson);
            speechProfilesJson = json_object_new_array();
        }
    } else {
        speechProfilesJson = json_object_new_array();
    }

    for (LONG i = 0; i < getBuiltinSpeechProviderCount(); i++) {
        if (builtinSpeechProfilesJson[i] != NULL) {
            json_object_put(builtinSpeechProfilesJson[i]);
            builtinSpeechProfilesJson[i] = NULL;
        }
        SpeechSystem sys = builtinSystemForListIndex(i);
        CONST_STRPTR builtinName = builtinNameForSystem(sys);
        struct json_object *storedProfile = NULL;
        int len = speechProfilesJson != NULL
                      ? json_object_array_length(speechProfilesJson)
                      : 0;
        for (int j = 0; j < len; j++) {
            struct json_object *p = json_object_array_get_idx(speechProfilesJson, j);
            struct json_object *nameObj =
                p ? json_object_object_get(p, "name") : NULL;
            CONST_STRPTR name =
                nameObj ? json_object_get_string(nameObj) : NULL;
            if (name != NULL && strcmp(name, builtinName) == 0) {
                storedProfile = p;
                break;
            }
        }
        if (storedProfile != NULL) {
            builtinSpeechProfilesJson[i] =
                json_tokener_parse(json_object_to_json_string(storedProfile));
        }
        if (builtinSpeechProfilesJson[i] == NULL) {
            builtinSpeechProfilesJson[i] = createBuiltinProfileFromConfig(sys);
        }
    }
}

static void saveProfilesToConfig(void) {
    if (speechProfilesJson == NULL)
        return;
    CONST_STRPTR jsonStr = json_object_to_json_string(speechProfilesJson);
    configSetSpeechProfiles(jsonStr);
}

static void populateSpeechSystemOptions(void) {
    int n = 0;
#ifdef __AMIGAOS3__
    speechSystemOptions[n++] = (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_34];
    speechSystemOptions[n++] = (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_37];
#endif
#ifdef __AMIGAOS4__
    speechSystemOptions[n++] = (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_FLITE];
#endif
    speechSystemOptions[n++] =
        (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI];
    speechSystemOptions[n++] =
        (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_ELEVENLABS];
    speechSystemOptions[n] = NULL;
}

static SpeechSystem getSpeechSystemFromCycle(void) {
    LONG sysIdx = 0;
    if (speechSystemCycle != NULL)
        get(speechSystemCycle, MUIA_Cycle_Active, &sysIdx);
    if (speechSystemOptions[sysIdx] != NULL) {
        for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
            if (strcmp(speechSystemOptions[sysIdx], SPEECH_SYSTEM_NAMES[s]) ==
                0) {
                return (SpeechSystem)s;
            }
        }
    }
    return SPEECH_SYSTEM_OPENAI;
}

HOOKPROTONHNONP(SpeechProviderSystemChangedFunc, void) {
    /* Changing the system toggles group visibility; keep window height stable.
     */
    LONG wasOpen = FALSE;
    LONG h = 0;
    if (speechProviderSettingsRequesterWindowObject != NULL) {
        get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            &wasOpen);
        if (wasOpen)
            get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height,
                &h);
    }

    SpeechSystem sys = getSpeechSystemFromCycle();
    updateGroupVisibilityForSystem(sys, TRUE);
    if (speechSystemUsesNetwork(sys)) {
        if (speechHostString != NULL)
            set(speechHostString, MUIA_String_Contents,
                defaultHostForSpeechSystem(sys));
        if (speechPortString != NULL)
            set(speechPortString, MUIA_String_Integer, 443);
        if (speechUsesSSLCycle != NULL)
            set(speechUsesSSLCycle, MUIA_Cycle_Active, 1);
        if (speechAuthorizationTypeCycle != NULL)
            set(speechAuthorizationTypeCycle, MUIA_Cycle_Active,
                (LONG)defaultAuthForSpeechSystem(sys));
        if (speechEndpointUrlString != NULL)
            set(speechEndpointUrlString, MUIA_String_Contents, "v1");
    }
    setFieldsEnabledForSystem(sys, TRUE);
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        updateRequirementWarningForSystem(sys, workbenchWarningText);
    } else if (sys == SPEECH_SYSTEM_FLITE) {
        updateRequirementWarningForSystem(sys, fliteWarningText);
    } else {
        updateRequirementWarningForSystem(sys, workbenchWarningText);
        updateRequirementWarningForSystem(sys, fliteWarningText);
    }

    if (speechProviderSettingsRequesterWindowObject != NULL && wasOpen &&
        h > 0) {
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height, h);
    }
}
MakeHook(SpeechProviderSystemChangedHook, SpeechProviderSystemChangedFunc);

HOOKPROTONHNONP(SpeechProviderSpeechAuthTypeChangedFunc, void) {
    updateSpeechApiKeyStringEnabledFromAuth(getSpeechSystemFromCycle());
}
MakeHook(SpeechProviderSpeechAuthTypeChangedHook,
         SpeechProviderSpeechAuthTypeChangedFunc);

static void populateProfileList(void) {
    if (speechProfileList == NULL)
        return;

    suppressProfileSelected = TRUE;
    DoMethod(speechProfileList, MUIM_NList_Clear);

    /* Built-ins */
#ifdef __AMIGAOS3__
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_34],
             MUIV_NList_Insert_Bottom);
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_37],
             MUIV_NList_Insert_Bottom);
#endif
#ifdef __AMIGAOS4__
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_FLITE],
             MUIV_NList_Insert_Bottom);
#endif
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI],
             MUIV_NList_Insert_Bottom);
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_ELEVENLABS],
             MUIV_NList_Insert_Bottom);

    /* Custom profiles */
    if (speechProfilesJson != NULL &&
        json_object_is_type(speechProfilesJson, json_type_array)) {
        int len = json_object_array_length(speechProfilesJson);
        for (int i = 0; i < len; i++) {
            struct json_object *p =
                json_object_array_get_idx(speechProfilesJson, i);
            if (p == NULL)
                continue;
            struct json_object *nameObj = json_object_object_get(p, "name");
            if (nameObj == NULL)
                continue;
            CONST_STRPTR name = json_object_get_string(nameObj);
            if (profileNameIsBuiltin(name))
                continue;
            if (name != NULL && strlen(name) > 0) {
                DoMethod(speechProfileList, MUIM_NList_InsertSingle,
                         (ULONG)name, MUIV_NList_Insert_Bottom);
            }
        }
    }

    /* Select active */
    LONG select = 0; /* default */
    STRPTR activeName = configGetActiveSpeechProfileName();
    if (activeName != NULL && strlen(activeName) > 0) {
        /* Built-ins */
        for (LONG bi = 0; bi < getBuiltinSpeechProviderCount(); bi++) {
            SpeechSystem sys = builtinSystemForListIndex(bi);
            CONST_STRPTR bn = builtinNameForSystem(sys);
            if (bn != NULL && strcmp(activeName, bn) == 0) {
                select = bi;
                break;
            }
        }
        /* Custom */
        if (speechProfilesJson != NULL) {
            int len = json_object_array_length(speechProfilesJson);
            LONG visible = 0;
            for (int i = 0; i < len; i++) {
                struct json_object *p =
                    json_object_array_get_idx(speechProfilesJson, i);
                struct json_object *nameObj =
                    p ? json_object_object_get(p, "name") : NULL;
                CONST_STRPTR name =
                    nameObj ? json_object_get_string(nameObj) : NULL;
                if (profileNameIsBuiltin(name))
                    continue;
                if (name != NULL && strcmp(activeName, name) == 0) {
                    select = getBuiltinSpeechProviderCount() + visible;
                    break;
                }
                visible++;
            }
        }
    }

    if (pendingProfileListSelect != MUIV_NList_Active_Off) {
        select = pendingProfileListSelect;
        pendingProfileListSelect = MUIV_NList_Active_Off;
    }

    set(speechProfileList, MUIA_NList_Active, select);
    suppressProfileSelected = FALSE;
    lastSelectedProfile = select;
    loadProfileIntoUI(select);
}

static void updateSpeechApiKeyStringEnabledFromAuth(SpeechSystem sys) {
    LONG authType = (LONG)AUTHORIZATION_TYPE_BEARER;
    if (speechAuthorizationTypeCycle != NULL)
        get(speechAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
    BOOL authNone =
        ((AuthorizationType)authType == AUTHORIZATION_TYPE_NONE);
    if (openAiApiKeyString != NULL)
        set(openAiApiKeyString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_OPENAI) || authNone);
    if (elevenLabsAPIKeyString != NULL)
        set(elevenLabsAPIKeyString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS) || authNone);
}

static void setFieldsEnabledForSystem(SpeechSystem sys, BOOL isCustomOrNew) {
    /* Enable/disable the “custom common” controls. Provider groups
     * are shown/hidden separately. */
    if (speechProfileNameString != NULL)
        set(speechProfileNameString, MUIA_Disabled, !isCustomOrNew);
    if (speechSystemCycle != NULL)
        set(speechSystemCycle, MUIA_Disabled, !isCustomOrNew);

    if (workbenchAccentString != NULL)
        set(workbenchAccentString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchAccentBrowseButton != NULL)
        set(workbenchAccentBrowseButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
#ifdef __AMIGAOS3__
    if (workbenchRateString != NULL)
        set(workbenchRateString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchPitchString != NULL)
        set(workbenchPitchString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchModeCycle != NULL)
        set(workbenchModeCycle, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchSexCycle != NULL)
        set(workbenchSexCycle, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
#endif

    if (fliteVoiceCycle != NULL)
        set(fliteVoiceCycle, MUIA_Disabled, !(sys == SPEECH_SYSTEM_FLITE));

    /* Built-in OpenAI / ElevenLabs profiles ship with locked connection
     * settings — users who want to point at a different host should clone
     * to a custom profile first. */
    BOOL connectionEditable = speechSystemUsesNetwork(sys) && isCustomOrNew;
    if (speechHostString != NULL)
        set(speechHostString, MUIA_Disabled, !connectionEditable);
    if (speechPortString != NULL)
        set(speechPortString, MUIA_Disabled, !connectionEditable);
    if (speechUsesSSLCycle != NULL)
        set(speechUsesSSLCycle, MUIA_Disabled, !connectionEditable);
    if (speechAuthorizationTypeCycle != NULL)
        set(speechAuthorizationTypeCycle, MUIA_Disabled, !connectionEditable);
    if (speechEndpointUrlString != NULL)
        set(speechEndpointUrlString, MUIA_Disabled, !connectionEditable);

    updateSpeechApiKeyStringEnabledFromAuth(sys);
    if (openAiTtsCurrentModelText != NULL)
        set(openAiTtsCurrentModelText, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiTtsFetchModelsButton != NULL)
        set(openAiTtsFetchModelsButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiTtsModelList != NULL)
        set(openAiTtsModelList, MUIA_Disabled, !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiTtsVoiceCycle != NULL)
        set(openAiTtsVoiceCycle, MUIA_Disabled, !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiVoiceInstructionsEditor != NULL)
        set(openAiVoiceInstructionsEditor, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_OPENAI));

    if (elevenLabsFetchModelsButton != NULL)
        set(elevenLabsFetchModelsButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsModelList != NULL)
        set(elevenLabsModelList, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsVoiceSearchString != NULL)
        set(elevenLabsVoiceSearchString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsSearchButton != NULL)
        set(elevenLabsSearchButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsVoiceList != NULL)
        set(elevenLabsVoiceList, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
}

static void updateGroupVisibilityForSystem(SpeechSystem sys,
                                           BOOL isCustomOrNew) {
    if (customCommonGroup != NULL)
        set(customCommonGroup, MUIA_ShowMe, isCustomOrNew);

    if (workbenchGroup != NULL)
        set(workbenchGroup, MUIA_ShowMe,
            (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (fliteGroup != NULL)
        set(fliteGroup, MUIA_ShowMe, (sys == SPEECH_SYSTEM_FLITE));
    if (speechConnectionGroup != NULL)
        set(speechConnectionGroup, MUIA_ShowMe, speechSystemUsesNetwork(sys));
    if (openAiGroup != NULL)
        set(openAiGroup, MUIA_ShowMe, (sys == SPEECH_SYSTEM_OPENAI));
    if (elevenLabsGroup != NULL)
        set(elevenLabsGroup, MUIA_ShowMe, (sys == SPEECH_SYSTEM_ELEVENLABS));
}

static void loadProfileIntoUI(LONG activeIndex) {
    if (speechProfileNameString == NULL)
        return;

    if (activeIndex < 0)
        return;

    beginRightPanelUpdate();

    BOOL isCustom = !isBuiltinListIndex(activeIndex);
    SpeechSystem sys = SPEECH_SYSTEM_OPENAI;
    struct json_object *customProfile = NULL;
    BOOL isCustomOrNew = isCustom;

    if (isBuiltinListIndex(activeIndex)) {
        sys = builtinSystemForListIndex(activeIndex);
        customProfile = builtinSpeechProfilesJson[activeIndex];
        set(speechProfileNameString, MUIA_String_Contents,
            builtinNameForSystem(sys));
    } else {
        /* Custom */
        int profileIndex = profileArrayIndexForVisibleCustomIndex(
            (int)(activeIndex - getBuiltinSpeechProviderCount()));
        struct json_object *p =
            (profileIndex >= 0 && speechProfilesJson != NULL)
                ? json_object_array_get_idx(speechProfilesJson, profileIndex)
                : NULL;
        customProfile = p;
        struct json_object *nameObj =
            p ? json_object_object_get(p, "name") : NULL;
        struct json_object *sysObj =
            p ? json_object_object_get(p, "speechSystem") : NULL;
        CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
        if (name != NULL)
            set(speechProfileNameString, MUIA_String_Contents, name);
        if (sysObj != NULL)
            sys = (SpeechSystem)json_object_get_int(sysObj);
    }

    /* System cycle selection (only meaningful for custom/new) */
    if (speechSystemCycle != NULL) {
        LONG active = 0;
        for (LONG i = 0; speechSystemOptions[i] != NULL; i++) {
            if (SPEECH_SYSTEM_NAMES[sys] != NULL &&
                strcmp(speechSystemOptions[i], SPEECH_SYSTEM_NAMES[sys]) == 0) {
                active = i;
                break;
            }
        }
        set(speechSystemCycle, MUIA_Cycle_Active, active);
    }

    updateGroupVisibilityForSystem(sys, isCustomOrNew);

    if (speechSystemUsesNetwork(sys)) {
        CONST_STRPTR host = defaultHostForSpeechSystem(sys);
        LONG port = 443;
        LONG useSSL = 1;
        LONG authType = (LONG)defaultAuthForSpeechSystem(sys);
        CONST_STRPTR endpointUrl = "v1";
        if (customProfile != NULL) {
            struct json_object *hostObj =
                json_object_object_get(customProfile, "host");
            struct json_object *portObj =
                json_object_object_get(customProfile, "port");
            struct json_object *sslObj =
                json_object_object_get(customProfile, "useSSL");
            struct json_object *authObj =
                json_object_object_get(customProfile, "authorizationType");
            struct json_object *endpointObj =
                json_object_object_get(customProfile, "apiEndpointUrl");
            if (hostObj != NULL)
                host = json_object_get_string(hostObj);
            if (portObj != NULL)
                port = json_object_get_int(portObj);
            if (sslObj != NULL)
                useSSL = json_object_get_boolean(sslObj) ? 1 : 0;
            if (authObj != NULL)
                authType = json_object_get_int(authObj);
            if (endpointObj != NULL)
                endpointUrl = json_object_get_string(endpointObj);
        }
        if (speechHostString != NULL)
            set(speechHostString, MUIA_String_Contents, host ? host : "");
        if (speechPortString != NULL)
            set(speechPortString, MUIA_String_Integer, port);
        if (speechUsesSSLCycle != NULL)
            set(speechUsesSSLCycle, MUIA_Cycle_Active, useSSL);
        if (speechAuthorizationTypeCycle != NULL)
            set(speechAuthorizationTypeCycle, MUIA_Cycle_Active, authType);
        if (speechEndpointUrlString != NULL)
            set(speechEndpointUrlString, MUIA_String_Contents,
                endpointUrl ? endpointUrl : "v1");
    }

    /* Accent */
    if (workbenchAccentString != NULL) {
        CONST_STRPTR accent = NULL;
        if (customProfile != NULL) {
            struct json_object *aObj =
                json_object_object_get(customProfile, "speechAccent");
            if (aObj != NULL)
                accent = json_object_get_string(aObj);
        }
        if (accent == NULL && isBuiltinListIndex(activeIndex)) {
            if (sys == SPEECH_SYSTEM_34)
                accent = configGetSpeechAccent34();
            else if (sys == SPEECH_SYSTEM_37)
                accent = configGetSpeechAccent37();
        }
        if (accent == NULL)
            accent = configGetSpeechAccent();
        set(workbenchAccentString, MUIA_String_Contents, accent ? accent : "");
    }

#ifdef __AMIGAOS3__
    /* narrator.device parameters (Workbench profiles) */
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        LONG rate = (sys == SPEECH_SYSTEM_34) ? (LONG)configGetNarratorRate34()
                                              : (LONG)configGetNarratorRate37();
        LONG pitch = (sys == SPEECH_SYSTEM_34)
                         ? (LONG)configGetNarratorPitch34()
                         : (LONG)configGetNarratorPitch37();
        LONG mode = (sys == SPEECH_SYSTEM_34) ? (LONG)configGetNarratorMode34()
                                              : (LONG)configGetNarratorMode37();
        LONG sex = (sys == SPEECH_SYSTEM_34) ? (LONG)configGetNarratorSex34()
                                             : (LONG)configGetNarratorSex37();

        if (customProfile != NULL) {
            struct json_object *o = NULL;
            o = json_object_object_get(customProfile, "narratorRate");
            if (o != NULL)
                rate = (LONG)json_object_get_int(o);
            o = json_object_object_get(customProfile, "narratorPitch");
            if (o != NULL)
                pitch = (LONG)json_object_get_int(o);
            o = json_object_object_get(customProfile, "narratorMode");
            if (o != NULL)
                mode = (LONG)json_object_get_int(o);
            o = json_object_object_get(customProfile, "narratorSex");
            if (o != NULL)
                sex = (LONG)json_object_get_int(o);
        }

        if (workbenchRateString != NULL)
            set(workbenchRateString, MUIA_String_Integer, rate);
        if (workbenchPitchString != NULL)
            set(workbenchPitchString, MUIA_String_Integer, pitch);
        if (workbenchModeCycle != NULL)
            set(workbenchModeCycle, MUIA_Cycle_Active, (mode != 0) ? 1 : 0);
        if (workbenchSexCycle != NULL)
            set(workbenchSexCycle, MUIA_Cycle_Active, (sex != 0) ? 1 : 0);
    }
#endif

    /* Flite voice */
    if (fliteVoiceCycle != NULL) {
        LONG fv = (LONG)configGetSpeechFliteVoice();
        if (customProfile != NULL) {
            struct json_object *fvObj =
                json_object_object_get(customProfile, "speechFliteVoice");
            if (fvObj != NULL)
                fv = (LONG)json_object_get_int(fvObj);
        }
        set(fliteVoiceCycle, MUIA_Cycle_Active, fv);
    }

    /* OpenAI fields */
    if (openAiApiKeyString != NULL) {
        STRPTR key = configGetOpenAiSpeechApiKey();
        if (customProfile != NULL) {
            struct json_object *kObj =
                json_object_object_get(customProfile, "openAiApiKey");
            if (kObj != NULL)
                key = (STRPTR)json_object_get_string(kObj);
        }
        set(openAiApiKeyString, MUIA_String_Contents, key ? key : "");
    }
    if (openAiTtsModelList != NULL) {
        STRPTR mid = configGetOpenAITTSModelId();
        if (customProfile != NULL) {
            struct json_object *midObj =
                json_object_object_get(customProfile, "openAITTSModelId");
            if (midObj != NULL) {
                CONST_STRPTR storedMid = json_object_get_string(midObj);
                if (storedMid != NULL && strlen(storedMid) > 0)
                    mid = (STRPTR)storedMid;
            } else {
                /* Legacy enum-based profile JSON */
                struct json_object *mObj =
                    json_object_object_get(customProfile, "openAITTSModel");
                if (mObj != NULL) {
                    OpenAITTSModel legacy =
                        (OpenAITTSModel)json_object_get_int(mObj);
                    CONST_STRPTR fallback =
                        OPENAI_TTS_MODEL_NAMES[legacy]
                            ? OPENAI_TTS_MODEL_NAMES[legacy]
                            : OPENAI_TTS_MODEL_NAMES
                                  [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS];
                    mid = (STRPTR)fallback;
                }
            }
        }
        if (openAiTtsCurrentModelText != NULL)
            set(openAiTtsCurrentModelText, MUIA_Text_Contents,
                (mid != NULL && strlen(mid) > 0) ? mid
                                                 : (STRPTR)STRING_NONE_SELECTED);
        ensureDefaultOpenAITTSModelsJson();
        populateOpenAITtsModelList();
        setOpenAITtsModelListActiveById(mid);
    }
    if (openAiTtsVoiceCycle != NULL) {
        LONG v = (LONG)configGetOpenAITTSVoice();
        if (customProfile != NULL) {
            struct json_object *vObj =
                json_object_object_get(customProfile, "openAITTSVoice");
            if (vObj != NULL)
                v = (LONG)json_object_get_int(vObj);
        }
        set(openAiTtsVoiceCycle, MUIA_Cycle_Active, v);
    }
    if (openAiVoiceInstructionsEditor != NULL) {
        STRPTR instr = configGetOpenAIVoiceInstructions();
        if (customProfile != NULL) {
            struct json_object *iObj = json_object_object_get(
                customProfile, "openAIVoiceInstructions");
            if (iObj != NULL)
                instr = (STRPTR)json_object_get_string(iObj);
        }
        set(openAiVoiceInstructionsEditor, MUIA_TextEditor_Contents,
            instr ? instr : "");
    }

    /* ElevenLabs fields */
    if (elevenLabsAPIKeyString != NULL) {
        STRPTR k = configGetElevenLabsAPIKey();
        if (customProfile != NULL) {
            struct json_object *ko =
                json_object_object_get(customProfile, "elevenLabsAPIKey");
            if (ko != NULL)
                k = (STRPTR)json_object_get_string(ko);
        }
        set(elevenLabsAPIKeyString, MUIA_String_Contents, k ? k : "");
    }
    if (elevenLabsCurrentModelText != NULL) {
        STRPTR mid = configGetElevenLabsModel();
        STRPTR mn = configGetElevenLabsModelName();
        if (customProfile != NULL) {
            struct json_object *midObj =
                json_object_object_get(customProfile, "elevenLabsModel");
            struct json_object *mno =
                json_object_object_get(customProfile, "elevenLabsModelName");
            if (midObj != NULL) {
                CONST_STRPTR storedMid = json_object_get_string(midObj);
                if (storedMid != NULL && strlen(storedMid) > 0)
                    mid = (STRPTR)storedMid;
            }
            if (mno != NULL) {
                CONST_STRPTR storedName = json_object_get_string(mno);
                if (storedName != NULL && strlen(storedName) > 0)
                    mn = (STRPTR)storedName;
            }
        }
        set(elevenLabsCurrentModelText, MUIA_Text_Contents,
            mn != NULL ? mn : (STRPTR)STRING_NONE_SELECTED);
        ensureDefaultElevenLabsModelsJson();
        populateElevenLabsModelList();
        setElevenLabsModelListActiveById(mid);
    }
    if (elevenLabsCurrentVoiceText != NULL) {
        STRPTR vn = configGetElevenLabsVoiceName();
        if (customProfile != NULL) {
            struct json_object *vno =
                json_object_object_get(customProfile, "elevenLabsVoiceName");
            if (vno != NULL)
                vn = (STRPTR)json_object_get_string(vno);
        }
        set(elevenLabsCurrentVoiceText, MUIA_Text_Contents,
            vn != NULL ? vn : (STRPTR)STRING_NONE_SELECTED);
    }

    setFieldsEnabledForSystem(sys, isCustomOrNew);
    /* Update per-provider dependency warnings (shown only in relevant groups)
     */
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        updateRequirementWarningForSystem(sys, workbenchWarningText);
    } else if (sys == SPEECH_SYSTEM_FLITE) {
        updateRequirementWarningForSystem(sys, fliteWarningText);
    } else {
        /* Clear both */
        updateRequirementWarningForSystem(sys, workbenchWarningText);
        updateRequirementWarningForSystem(sys, fliteWarningText);
    }

    /* Built-ins can’t be deleted. */
    if (deleteProfileButton != NULL)
        set(deleteProfileButton, MUIA_Disabled,
            (activeIndex < getBuiltinSpeechProviderCount()));
    if (duplicateProfileButton != NULL)
        set(duplicateProfileButton, MUIA_Disabled,
            (activeIndex == MUIV_NList_Active_Off));
    if (newProfileButton != NULL)
        set(newProfileButton, MUIA_Disabled, FALSE);
    updateElevenLabsSearchButtonLabel();

    endRightPanelUpdate();
}

HOOKPROTONHNONP(ProfileSelectedFunc, void) {
    if (suppressProfileSelected)
        return;
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);

    if (lastSelectedProfile != MUIV_NList_Active_Off &&
        active != lastSelectedProfile) {
        if (!applyProfileFromUIToWorking(lastSelectedProfile)) {
            suppressProfileSelected = TRUE;
            set(speechProfileList, MUIA_NList_Active, lastSelectedProfile);
            suppressProfileSelected = FALSE;
            return;
        }
    }

    /* Profile selection toggles group visibility; keep window height stable. */
    LONG wasOpen = FALSE;
    LONG h = 0;
    if (speechProviderSettingsRequesterWindowObject != NULL) {
        get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            &wasOpen);
        if (wasOpen)
            get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height,
                &h);
    }

    loadProfileIntoUI(active);

    if (speechProviderSettingsRequesterWindowObject != NULL && wasOpen &&
        h > 0) {
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height, h);
    }

    lastSelectedProfile = active;
    speechSettingsDirty = FALSE;
}
MakeHook(SpeechProviderProfileSelectedHook, ProfileSelectedFunc);

static struct json_object *createDefaultProfile(CONST_STRPTR name) {
    struct json_object *p = json_object_new_object();
    json_object_object_add(p, "name", json_object_new_string(name ? name : ""));
    json_object_object_add(p, "speechSystem",
                           json_object_new_int((int)SPEECH_SYSTEM_OPENAI));
    json_object_object_add(p, "speechAccent", json_object_new_string(""));
    json_object_object_add(
        p, "speechFliteVoice",
        json_object_new_int((int)configGetSpeechFliteVoice()));
    json_object_object_add(p, "host", json_object_new_string("api.openai.com"));
    json_object_object_add(p, "port", json_object_new_int(443));
    json_object_object_add(p, "useSSL", json_object_new_boolean(TRUE));
    json_object_object_add(
        p, "authorizationType",
        json_object_new_int((int)AUTHORIZATION_TYPE_BEARER));
    json_object_object_add(p, "apiEndpointUrl", json_object_new_string("v1"));
    json_object_object_add(p, "openAiApiKey", json_object_new_string(""));
    {
        STRPTR mid = configGetOpenAITTSModelId();
        json_object_object_add(
            p, "openAITTSModelId",
            json_object_new_string(mid ? mid : OPENAI_TTS_MODEL_NAMES
                                              [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS]));
    }
    json_object_object_add(
        p, "openAITTSVoice", json_object_new_int((int)configGetOpenAITTSVoice()));
    json_object_object_add(p, "openAIVoiceInstructions",
                           json_object_new_string(""));
    json_object_object_add(p, "elevenLabsAPIKey", json_object_new_string(""));
    json_object_object_add(p, "elevenLabsModel",
                           json_object_new_string(ELEVENLABS_MODEL_FLASH_V2_5_ID));
    json_object_object_add(
        p, "elevenLabsModelName",
        json_object_new_string(ELEVENLABS_MODEL_FLASH_V2_5_NAME));
    return p;
}

static struct json_object *createBuiltinProfileFromConfig(SpeechSystem sys) {
    struct json_object *p = json_object_new_object();
    json_object_object_add(p, "name",
                           json_object_new_string(builtinNameForSystem(sys)));
    json_object_object_add(p, "speechSystem", json_object_new_int((int)sys));

    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        CONST_STRPTR accent = (sys == SPEECH_SYSTEM_34)
                                  ? configGetSpeechAccent34()
                                  : configGetSpeechAccent37();
        json_object_object_add(p, "speechAccent",
                               json_object_new_string(accent ? accent : ""));
#ifdef __AMIGAOS3__
        json_object_object_add(
            p, "narratorRate",
            json_object_new_int((int)((sys == SPEECH_SYSTEM_34)
                                          ? configGetNarratorRate34()
                                          : configGetNarratorRate37())));
        json_object_object_add(
            p, "narratorPitch",
            json_object_new_int((int)((sys == SPEECH_SYSTEM_34)
                                          ? configGetNarratorPitch34()
                                          : configGetNarratorPitch37())));
        json_object_object_add(
            p, "narratorMode",
            json_object_new_int((int)((sys == SPEECH_SYSTEM_34)
                                          ? configGetNarratorMode34()
                                          : configGetNarratorMode37())));
        json_object_object_add(
            p, "narratorSex",
            json_object_new_int((int)((sys == SPEECH_SYSTEM_34)
                                          ? configGetNarratorSex34()
                                          : configGetNarratorSex37())));
#endif
    }

    if (sys == SPEECH_SYSTEM_FLITE) {
        json_object_object_add(
            p, "speechFliteVoice",
            json_object_new_int((int)configGetSpeechFliteVoice()));
    }

    if (sys == SPEECH_SYSTEM_OPENAI) {
        json_object_object_add(p, "host", json_object_new_string("api.openai.com"));
        json_object_object_add(p, "port", json_object_new_int(443));
        json_object_object_add(p, "useSSL", json_object_new_boolean(TRUE));
        json_object_object_add(
            p, "authorizationType",
            json_object_new_int((int)AUTHORIZATION_TYPE_BEARER));
        json_object_object_add(p, "apiEndpointUrl",
                               json_object_new_string("v1"));
        json_object_object_add(
            p, "openAiApiKey",
            json_object_new_string(configGetOpenAiSpeechApiKey()
                                       ? configGetOpenAiSpeechApiKey()
                                       : ""));
        {
            STRPTR mid = configGetOpenAITTSModelId();
            json_object_object_add(
                p, "openAITTSModelId",
                json_object_new_string(mid ? mid : OPENAI_TTS_MODEL_NAMES
                                                  [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS]));
        }
        json_object_object_add(
            p, "openAITTSVoice",
            json_object_new_int((int)configGetOpenAITTSVoice()));
        json_object_object_add(
            p, "openAIVoiceInstructions",
            json_object_new_string(configGetOpenAIVoiceInstructions()
                                       ? configGetOpenAIVoiceInstructions()
                                       : ""));
    }

    if (sys == SPEECH_SYSTEM_ELEVENLABS) {
        json_object_object_add(p, "host", json_object_new_string(ELEVENLABS_HOST));
        json_object_object_add(p, "port", json_object_new_int(ELEVENLABS_PORT));
        json_object_object_add(p, "useSSL", json_object_new_boolean(TRUE));
        json_object_object_add(
            p, "authorizationType",
            json_object_new_int((int)AUTHORIZATION_TYPE_XI_API_KEY));
        json_object_object_add(p, "apiEndpointUrl",
                               json_object_new_string("v1"));
        json_object_object_add(
            p, "elevenLabsAPIKey",
            json_object_new_string(configGetElevenLabsAPIKey()
                                       ? configGetElevenLabsAPIKey()
                                       : ""));
        json_object_object_add(
            p, "elevenLabsModel",
            json_object_new_string(configGetElevenLabsModel()
                                       ? configGetElevenLabsModel()
                                       : ""));
        json_object_object_add(
            p, "elevenLabsModelName",
            json_object_new_string(configGetElevenLabsModelName()
                                       ? configGetElevenLabsModelName()
                                       : ""));
        json_object_object_add(
            p, "elevenLabsVoiceID",
            json_object_new_string(configGetElevenLabsVoiceID()
                                       ? configGetElevenLabsVoiceID()
                                       : ""));
        json_object_object_add(
            p, "elevenLabsVoiceName",
            json_object_new_string(configGetElevenLabsVoiceName()
                                       ? configGetElevenLabsVoiceName()
                                       : ""));
    }

    return p;
}

static struct json_object *getWorkingProfileForListIndex(LONG activeIndex) {
    if (isBuiltinListIndex(activeIndex))
        return builtinSpeechProfilesJson[activeIndex];

    int profileIndex = profileArrayIndexForVisibleCustomIndex(
        (int)(activeIndex - getBuiltinSpeechProviderCount()));
    if (profileIndex < 0 || speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array))
        return NULL;
    return json_object_array_get_idx(speechProfilesJson, profileIndex);
}

static struct json_object *createProfileFromUI(CONST_STRPTR name) {
    struct json_object *p = json_object_new_object();
    json_object_object_add(p, "name", json_object_new_string(name ? name : ""));

    /* System */
    LONG sysIdx = 0;
    get(speechSystemCycle, MUIA_Cycle_Active, &sysIdx);
    SpeechSystem sys = SPEECH_SYSTEM_OPENAI;
    if (speechSystemOptions[sysIdx] != NULL) {
        for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
            if (strcmp(speechSystemOptions[sysIdx], SPEECH_SYSTEM_NAMES[s]) ==
                0) {
                sys = (SpeechSystem)s;
                break;
            }
        }
    }
    json_object_object_add(p, "speechSystem", json_object_new_int((int)sys));

    /* Accent */
    STRPTR accent = NULL;
    if (workbenchAccentString != NULL)
        get(workbenchAccentString, MUIA_String_Contents, &accent);
    json_object_object_add(p, "speechAccent",
                           json_object_new_string(accent ? accent : ""));

#ifdef __AMIGAOS3__
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        LONG rate = 0, pitch = 0, mode = 0, sex = 0;
        if (workbenchRateString != NULL)
            get(workbenchRateString, MUIA_String_Integer, &rate);
        if (workbenchPitchString != NULL)
            get(workbenchPitchString, MUIA_String_Integer, &pitch);
        if (workbenchModeCycle != NULL)
            get(workbenchModeCycle, MUIA_Cycle_Active, &mode);
        if (workbenchSexCycle != NULL)
            get(workbenchSexCycle, MUIA_Cycle_Active, &sex);

        json_object_object_add(p, "narratorRate",
                               json_object_new_int((int)rate));
        json_object_object_add(p, "narratorPitch",
                               json_object_new_int((int)pitch));
        json_object_object_add(p, "narratorMode",
                               json_object_new_int((int)((mode != 0) ? 1 : 0)));
        json_object_object_add(p, "narratorSex",
                               json_object_new_int((int)((sex != 0) ? 1 : 0)));
    }
#endif

    /* Flite voice */
    LONG fv = 0;
    if (fliteVoiceCycle != NULL)
        get(fliteVoiceCycle, MUIA_Cycle_Active, &fv);
    json_object_object_add(p, "speechFliteVoice", json_object_new_int((int)fv));

    if (speechSystemUsesNetwork(sys)) {
        STRPTR host = NULL;
        LONG port = 443;
        LONG useSSL = 1;
        LONG authType = defaultAuthForSpeechSystem(sys);
        STRPTR endpointUrl = NULL;
        if (speechHostString != NULL)
            get(speechHostString, MUIA_String_Contents, &host);
        if (speechPortString != NULL)
            get(speechPortString, MUIA_String_Integer, &port);
        if (speechUsesSSLCycle != NULL)
            get(speechUsesSSLCycle, MUIA_Cycle_Active, &useSSL);
        if (speechAuthorizationTypeCycle != NULL)
            get(speechAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
        if (speechEndpointUrlString != NULL)
            get(speechEndpointUrlString, MUIA_String_Contents, &endpointUrl);
        json_object_object_add(
            p, "host",
            json_object_new_string((host != NULL && strlen(host) > 0)
                                       ? host
                                       : defaultHostForSpeechSystem(sys)));
        json_object_object_add(p, "port", json_object_new_int((int)port));
        json_object_object_add(p, "useSSL",
                               json_object_new_boolean(useSSL == 1));
        json_object_object_add(p, "authorizationType",
                               json_object_new_int((int)authType));
        json_object_object_add(
            p, "apiEndpointUrl",
            json_object_new_string((endpointUrl != NULL &&
                                    strlen(endpointUrl) > 0)
                                       ? endpointUrl
                                       : "v1"));
    }

    /* OpenAI */
    STRPTR apiKey = NULL;
    if (openAiApiKeyString != NULL)
        get(openAiApiKeyString, MUIA_String_Contents, &apiKey);
    json_object_object_add(p, "openAiApiKey",
                           json_object_new_string(apiKey ? apiKey : ""));

    LONG v = 0;
    if (openAiTtsVoiceCycle != NULL)
        get(openAiTtsVoiceCycle, MUIA_Cycle_Active, &v);
    json_object_object_add(p, "openAITTSVoice", json_object_new_int((int)v));

    /* OpenAI TTS model: take whatever id is currently selected in the list,
     * or fall back to the saved config value. */
    {
        CONST_STRPTR selectedId = NULL;
        LONG modelActive = MUIV_NList_Active_Off;
        if (openAiTtsModelList != NULL)
            get(openAiTtsModelList, MUIA_NList_Active, &modelActive);
        if (modelActive != MUIV_NList_Active_Off) {
            STRPTR selected = NULL;
            DoMethod(openAiTtsModelList, MUIM_NList_GetEntry, modelActive,
                     &selected);
            if (selected != NULL && strlen(selected) > 0)
                selectedId = (CONST_STRPTR)selected;
        }
        if (selectedId == NULL || strlen(selectedId) == 0) {
            STRPTR fallback = configGetOpenAITTSModelId();
            selectedId = (fallback != NULL && strlen(fallback) > 0)
                             ? (CONST_STRPTR)fallback
                             : OPENAI_TTS_MODEL_NAMES
                                   [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS];
        }
        json_object_object_add(p, "openAITTSModelId",
                               json_object_new_string(selectedId));
    }

    BOOL freeInstr = FALSE;
    STRPTR instr = getOpenAiVoiceInstructionsText(&freeInstr);
    json_object_object_add(p, "openAIVoiceInstructions",
                           json_object_new_string(instr ? instr : ""));
    if (freeInstr)
        FreeVec(instr);

    /* ElevenLabs */
    STRPTR eApiKey = NULL;
    if (elevenLabsAPIKeyString != NULL)
        get(elevenLabsAPIKeyString, MUIA_String_Contents, &eApiKey);
    json_object_object_add(p, "elevenLabsAPIKey",
                           json_object_new_string(eApiKey ? eApiKey : ""));

    LONG modelActive = MUIV_NList_Active_Off;
    if (elevenLabsModelList != NULL)
        get(elevenLabsModelList, MUIA_NList_Active, &modelActive);
    if (modelActive != MUIV_NList_Active_Off && elevenLabsModelsJson != NULL) {
        STRPTR selectedModelName = NULL;
        DoMethod(elevenLabsModelList, MUIM_NList_GetEntry, modelActive,
                 &selectedModelName);
        if (selectedModelName != NULL) {
            json_object_object_add(p, "elevenLabsModelName",
                                   json_object_new_string(selectedModelName));

            struct json_object *modelsArray = NULL;
            if (json_object_object_get_ex(elevenLabsModelsJson, "models",
                                          &modelsArray) ||
                json_object_is_type(elevenLabsModelsJson, json_type_array)) {
                if (modelsArray == NULL)
                    modelsArray = elevenLabsModelsJson;
                LONG modelCount = json_object_array_length(modelsArray);
                for (LONG i = 0; i < modelCount; i++) {
                    struct json_object *modelObj =
                        json_object_array_get_idx(modelsArray, i);
                    struct json_object *nameObj = NULL;
                    if (modelObj == NULL)
                        continue;
                    if (json_object_object_get_ex(modelObj, "name", &nameObj)) {
                        CONST_STRPTR n = json_object_get_string(nameObj);
                        if (n != NULL && strcmp(n, selectedModelName) == 0) {
                            struct json_object *modelIdObj = NULL;
                            if (json_object_object_get_ex(modelObj, "model_id",
                                                          &modelIdObj)) {
                                CONST_STRPTR modelId =
                                    json_object_get_string(modelIdObj);
                                if (modelId != NULL) {
                                    json_object_object_add(
                                        p, "elevenLabsModel",
                                        json_object_new_string(modelId));
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    } else {
        struct json_object *oldProfile =
            getWorkingProfileForListIndex(lastSelectedProfile);
        struct json_object *oldModel = oldProfile
                                           ? json_object_object_get(
                                                 oldProfile, "elevenLabsModel")
                                           : NULL;
        struct json_object *oldModelName =
            oldProfile ? json_object_object_get(oldProfile,
                                                "elevenLabsModelName")
                       : NULL;
        if (oldModel != NULL) {
            json_object_object_add(
                p, "elevenLabsModel",
                json_object_new_string(json_object_get_string(oldModel)));
        }
        if (oldModelName != NULL) {
            json_object_object_add(
                p, "elevenLabsModelName",
                json_object_new_string(json_object_get_string(oldModelName)));
        }
    }

    LONG voiceActive = MUIV_NList_Active_Off;
    if (elevenLabsVoiceList != NULL)
        get(elevenLabsVoiceList, MUIA_NList_Active, &voiceActive);
    if (voiceActive != MUIV_NList_Active_Off && elevenLabsVoicesJson != NULL) {
        STRPTR selectedVoiceName = NULL;
        DoMethod(elevenLabsVoiceList, MUIM_NList_GetEntry, voiceActive,
                 &selectedVoiceName);
        if (selectedVoiceName != NULL) {
            json_object_object_add(p, "elevenLabsVoiceName",
                                   json_object_new_string(selectedVoiceName));

            struct json_object *voicesArray = NULL;
            if (json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                          &voicesArray) ||
                json_object_is_type(elevenLabsVoicesJson, json_type_array)) {
                if (voicesArray == NULL)
                    voicesArray = elevenLabsVoicesJson;
                LONG voiceCount = json_object_array_length(voicesArray);
                for (LONG i = 0; i < voiceCount; i++) {
                    struct json_object *voiceObj =
                        json_object_array_get_idx(voicesArray, i);
                    struct json_object *nameObj = NULL;
                    if (voiceObj == NULL)
                        continue;
                    if (json_object_object_get_ex(voiceObj, "name", &nameObj)) {
                        CONST_STRPTR n = json_object_get_string(nameObj);
                        if (n != NULL && strcmp(n, selectedVoiceName) == 0) {
                            struct json_object *voiceIdObj = NULL;
                            if (json_object_object_get_ex(voiceObj, "voice_id",
                                                          &voiceIdObj)) {
                                CONST_STRPTR voiceId =
                                    json_object_get_string(voiceIdObj);
                                if (voiceId != NULL) {
                                    json_object_object_add(
                                        p, "elevenLabsVoiceID",
                                        json_object_new_string(voiceId));
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    } else {
        struct json_object *oldProfile =
            getWorkingProfileForListIndex(lastSelectedProfile);
        struct json_object *oldVoice = oldProfile
                                           ? json_object_object_get(
                                                 oldProfile, "elevenLabsVoiceID")
                                           : NULL;
        struct json_object *oldVoiceName =
            oldProfile ? json_object_object_get(oldProfile,
                                                "elevenLabsVoiceName")
                       : NULL;
        if (oldVoice != NULL) {
            json_object_object_add(
                p, "elevenLabsVoiceID",
                json_object_new_string(json_object_get_string(oldVoice)));
        }
        if (oldVoiceName != NULL) {
            json_object_object_add(
                p, "elevenLabsVoiceName",
                json_object_new_string(json_object_get_string(oldVoiceName)));
        }
    }

    return p;
}

static CONST_STRPTR jsonStringOrEmpty(struct json_object *profile,
                                      CONST_STRPTR key) {
    struct json_object *obj =
        profile ? json_object_object_get(profile, key) : NULL;
    CONST_STRPTR value = obj ? json_object_get_string(obj) : NULL;
    return value ? value : "";
}

static LONG jsonIntOrDefault(struct json_object *profile, CONST_STRPTR key,
                             LONG defaultValue) {
    struct json_object *obj =
        profile ? json_object_object_get(profile, key) : NULL;
    return obj ? (LONG)json_object_get_int(obj) : defaultValue;
}

static void commitBuiltinProfileToConfig(struct json_object *profile) {
    if (profile == NULL)
        return;

    struct json_object *sysObj = json_object_object_get(profile, "speechSystem");
    if (sysObj == NULL)
        return;
    SpeechSystem sys = (SpeechSystem)json_object_get_int(sysObj);

    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        CONST_STRPTR accent = jsonStringOrEmpty(profile, "speechAccent");
        if (sys == SPEECH_SYSTEM_34) {
            configSetSpeechAccent34(accent);
#ifdef __AMIGAOS3__
            configSetNarratorRate34(
                (ULONG)jsonIntOrDefault(profile, "narratorRate",
                                        (LONG)configGetNarratorRate34()));
            configSetNarratorPitch34(
                (ULONG)jsonIntOrDefault(profile, "narratorPitch",
                                        (LONG)configGetNarratorPitch34()));
            configSetNarratorMode34(
                (ULONG)jsonIntOrDefault(profile, "narratorMode",
                                        (LONG)configGetNarratorMode34()));
            configSetNarratorSex34(
                (ULONG)jsonIntOrDefault(profile, "narratorSex",
                                        (LONG)configGetNarratorSex34()));
#endif
        } else {
            configSetSpeechAccent37(accent);
#ifdef __AMIGAOS3__
            configSetNarratorRate37(
                (ULONG)jsonIntOrDefault(profile, "narratorRate",
                                        (LONG)configGetNarratorRate37()));
            configSetNarratorPitch37(
                (ULONG)jsonIntOrDefault(profile, "narratorPitch",
                                        (LONG)configGetNarratorPitch37()));
            configSetNarratorMode37(
                (ULONG)jsonIntOrDefault(profile, "narratorMode",
                                        (LONG)configGetNarratorMode37()));
            configSetNarratorSex37(
                (ULONG)jsonIntOrDefault(profile, "narratorSex",
                                        (LONG)configGetNarratorSex37()));
#endif
        }
    } else if (sys == SPEECH_SYSTEM_FLITE) {
        configSetSpeechFliteVoice((SpeechFliteVoice)jsonIntOrDefault(
            profile, "speechFliteVoice", (LONG)configGetSpeechFliteVoice()));
    } else if (sys == SPEECH_SYSTEM_OPENAI) {
        CONST_STRPTR key = jsonStringOrEmpty(profile, "openAiApiKey");
        configSetOpenAiSpeechApiKey(key);
        if (key != NULL && strlen(key) > 0) {
            STRPTR existing = configGetOpenAiApiKey();
            if (existing == NULL || strlen(existing) == 0)
                configSetOpenAiApiKey(key);
            existing = configGetOpenAiImageApiKey();
            if (existing == NULL || strlen(existing) == 0)
                configSetOpenAiImageApiKey(key);
        }
        {
            struct json_object *midObj =
                json_object_object_get(profile, "openAITTSModelId");
            CONST_STRPTR midStr =
                midObj ? json_object_get_string(midObj) : NULL;
            if (midStr == NULL || strlen(midStr) == 0) {
                /* Legacy enum-based profile JSON */
                struct json_object *m =
                    json_object_object_get(profile, "openAITTSModel");
                if (m != NULL) {
                    OpenAITTSModel legacy =
                        (OpenAITTSModel)json_object_get_int(m);
                    midStr = OPENAI_TTS_MODEL_NAMES[legacy]
                                 ? OPENAI_TTS_MODEL_NAMES[legacy]
                                 : OPENAI_TTS_MODEL_NAMES
                                       [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS];
                }
            }
            if (midStr != NULL && strlen(midStr) > 0)
                configSetOpenAITTSModelId(midStr);
        }
        configSetOpenAITTSVoice((OpenAITTSVoice)jsonIntOrDefault(
            profile, "openAITTSVoice", (LONG)configGetOpenAITTSVoice()));
        configSetOpenAIVoiceInstructions(
            jsonStringOrEmpty(profile, "openAIVoiceInstructions"));
    } else if (sys == SPEECH_SYSTEM_ELEVENLABS) {
        configSetElevenLabsAPIKey(jsonStringOrEmpty(profile, "elevenLabsAPIKey"));
        configSetElevenLabsModel(jsonStringOrEmpty(profile, "elevenLabsModel"));
        configSetElevenLabsModelName(
            jsonStringOrEmpty(profile, "elevenLabsModelName"));
        configSetElevenLabsVoiceID(
            jsonStringOrEmpty(profile, "elevenLabsVoiceID"));
        configSetElevenLabsVoiceName(
            jsonStringOrEmpty(profile, "elevenLabsVoiceName"));
    }

    if (sys == SPEECH_SYSTEM_34) {
        STRPTR a = configGetSpeechAccent34();
        if (a != NULL)
            configSetSpeechAccent(a);
    } else if (sys == SPEECH_SYSTEM_37) {
        STRPTR a = configGetSpeechAccent37();
        if (a != NULL)
            configSetSpeechAccent(a);
    }
}

static void reinitSpeechForActiveProfile(void) {
    struct SpeechRequestSettings s;
    configGetSpeechRequestSettings(&s);
    closeSpeech();
    if (initSpeech(s.speechSystem) == RETURN_ERROR) {
        switch (s.speechSystem) {
        case SPEECH_SYSTEM_34:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_34);
            break;
        case SPEECH_SYSTEM_37:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_37);
            break;
        case SPEECH_SYSTEM_FLITE:
            displayError(STRING_ERROR_SPEECH_INIT_FLITE);
            break;
        case SPEECH_SYSTEM_OPENAI:
            displayError(STRING_ERROR_SPEECH_INIT_OPENAI);
            break;
        default:
            displayError(STRING_ERROR_SPEECH_UNKNOWN_SYSTEM);
            break;
        }
    }
    configFreeSpeechRequestSettings(&s);
}

static BOOL isNameReservedForBuiltin(CONST_STRPTR name) {
    if (name == NULL || strlen(name) == 0)
        return TRUE;
    for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
        if (strcasecmp(name, SPEECH_SYSTEM_NAMES[s]) == 0)
            return TRUE;
    }
    return FALSE;
}

static int findCustomProfileIndexByName(CONST_STRPTR name) {
    if (name == NULL || speechProfilesJson == NULL)
        return -1;
    if (!json_object_is_type(speechProfilesJson, json_type_array))
        return -1;
    int len = json_object_array_length(speechProfilesJson);
    for (int i = 0; i < len; i++) {
        struct json_object *p =
            json_object_array_get_idx(speechProfilesJson, i);
        if (p == NULL)
            continue;
        struct json_object *nameObj = json_object_object_get(p, "name");
        CONST_STRPTR n = nameObj ? json_object_get_string(nameObj) : NULL;
        if (n != NULL && strcmp(n, name) == 0)
            return i;
    }
    return -1;
}

static BOOL isCustomProfileNameTaken(CONST_STRPTR name) {
    return findCustomProfileIndexByName(name) >= 0;
}

static STRPTR makeUniqueProfileName(CONST_STRPTR base, CONST_STRPTR suffix) {
    if (base == NULL || strlen(base) == 0)
        base = STRING_DEFAULT_NEW_PROFILE_NAME;
    if (suffix == NULL)
        suffix = "";

    for (int n = 1; n < 1000; n++) {
        char candidate[128];
        if (n == 1)
            snprintf(candidate, sizeof(candidate), "%s%s", base, suffix);
        else
            snprintf(candidate, sizeof(candidate), "%s%s %d", base, suffix, n);

        if (isNameReservedForBuiltin(candidate))
            continue;
        if (!isCustomProfileNameTaken(candidate))
            return dupStrLocal(candidate);
    }

    return dupStrLocal(STRING_DEFAULT_NEW_PROFILE_NAME);
}

static BOOL applyProfileFromUIToWorking(LONG activeIndex) {
    if (activeIndex == MUIV_NList_Active_Off)
        return TRUE;

    if (speechSystemUsesNetwork(getSpeechSystemFromCycle())) {
        LONG port = 0;
        if (speechPortString != NULL)
            get(speechPortString, MUIA_String_Integer, &port);
        if (port < 0 || port > 65535) {
            displayError(STRING_ERROR_INVALID_PORT);
            return FALSE;
        }
    }

    if (isBuiltinListIndex(activeIndex)) {
        SpeechSystem sys = builtinSystemForListIndex(activeIndex);
        struct json_object *newProfile =
            createProfileFromUI(builtinNameForSystem(sys));
        json_object_object_del(newProfile, "speechSystem");
        json_object_object_add(newProfile, "speechSystem",
                               json_object_new_int((int)sys));
        if (builtinSpeechProfilesJson[activeIndex] != NULL)
            json_object_put(builtinSpeechProfilesJson[activeIndex]);
        builtinSpeechProfilesJson[activeIndex] = newProfile;
        return TRUE;
    }

    int profileIndex = profileArrayIndexForVisibleCustomIndex(
        (int)(activeIndex - getBuiltinSpeechProviderCount()));
    if (profileIndex < 0 || speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array))
        return TRUE;

    STRPTR profileName = NULL;
    get(speechProfileNameString, MUIA_String_Contents, &profileName);
    if (profileName == NULL || strlen(profileName) == 0) {
        displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
        return FALSE;
    }
    if (isNameReservedForBuiltin(profileName)) {
        displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
        return FALSE;
    }
    {
        int existingIdx = findCustomProfileIndexByName(profileName);
        if (existingIdx >= 0 && existingIdx != profileIndex) {
            displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
            return FALSE;
        }
    }

    struct json_object *newProfile = createProfileFromUI(profileName);
    json_object_array_put_idx(speechProfilesJson, profileIndex, newProfile);
    return TRUE;
}

static void upsertBuiltinProfilesIntoSavedProfiles(void) {
    if (speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array))
        speechProfilesJson = json_object_new_array();

    for (LONG i = 0; i < getBuiltinSpeechProviderCount(); i++) {
        struct json_object *profile = builtinSpeechProfilesJson[i];
        struct json_object *nameObj =
            profile ? json_object_object_get(profile, "name") : NULL;
        CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
        if (profile == NULL || name == NULL)
            continue;

        int existing = findCustomProfileIndexByName(name);
        struct json_object *clone =
            json_tokener_parse(json_object_to_json_string(profile));
        if (clone == NULL)
            continue;
        if (existing >= 0) {
            json_object_array_put_idx(speechProfilesJson, existing, clone);
        } else {
            json_object_array_add(speechProfilesJson, clone);
        }
    }
}

HOOKPROTONHNONP(SpeechProviderOkFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(speechProfileList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off)
        return;

    if (!applyProfileFromUIToWorking(active))
        return;

    for (LONG i = 0; i < getBuiltinSpeechProviderCount(); i++)
        commitBuiltinProfileToConfig(builtinSpeechProfilesJson[i]);

    upsertBuiltinProfilesIntoSavedProfiles();
    saveProfilesToConfig();

    if (isBuiltinListIndex(active)) {
        SpeechSystem sys = builtinSystemForListIndex(active);
        configSetActiveSpeechProfileName(builtinNameForSystem(sys));
        configSetSpeechSystem(sys);
    } else {
        struct json_object *profile = getWorkingProfileForListIndex(active);
        struct json_object *nameObj =
            profile ? json_object_object_get(profile, "name") : NULL;
        struct json_object *sysObj =
            profile ? json_object_object_get(profile, "speechSystem") : NULL;
        CONST_STRPTR profileName =
            nameObj ? json_object_get_string(nameObj) : NULL;
        configSetActiveSpeechProfileName(profileName ? profileName : "");
        configSetSpeechSystem(sysObj ? (SpeechSystem)json_object_get_int(sysObj)
                                     : getSpeechSystemFromCycle());
    }

    reinitSpeechForActiveProfile();
    speechSettingsDirty = FALSE;
    set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(SpeechProviderOkHook, SpeechProviderOkFunc);

HOOKPROTONHNONP(SpeechProviderCancelFunc, void) {
    speechSettingsDirty = FALSE;
    set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(SpeechProviderCancelHook, SpeechProviderCancelFunc);

/* Read the OpenAI voice-instructions editor as plain text. MUIA_TextEditor_Contents
 * returns the editor's internal markup format (or NULL) and is unsafe for
 * round-tripping via JSON, so we use MUIM_TextEditor_ExportText on Amiga and
 * fall back to MUIA_String_Contents on AROS, matching the pattern used for
 * the other TextEditor objects in the project.
 *
 * On non-AROS the returned buffer is freshly allocated and must be released
 * with FreeVec(); *needsFree is set to TRUE in that case. */
static STRPTR getOpenAiVoiceInstructionsText(BOOL *needsFree) {
    if (needsFree != NULL)
        *needsFree = FALSE;
    if (openAiVoiceInstructionsEditor == NULL)
        return NULL;
    STRPTR text = NULL;
    if (isAROS) {
        get(openAiVoiceInstructionsEditor, MUIA_String_Contents, &text);
    } else {
        text = (STRPTR)DoMethod(openAiVoiceInstructionsEditor,
                                MUIM_TextEditor_ExportText);
        if (needsFree != NULL)
            *needsFree = (text != NULL);
    }
    return text;
}

/* Look up the ElevenLabs model_id/voice_id matching the currently selected
 * row in the UI lists. Returns a pointer owned by elevenLabsModelsJson /
 * elevenLabsVoicesJson, or NULL if no row is selected or no match is found.
 */
static CONST_STRPTR currentElevenLabsModelIdFromUI(void) {
    if (elevenLabsModelList == NULL || elevenLabsModelsJson == NULL)
        return NULL;
    LONG active = MUIV_NList_Active_Off;
    get(elevenLabsModelList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off)
        return NULL;
    STRPTR selectedName = NULL;
    DoMethod(elevenLabsModelList, MUIM_NList_GetEntry, active, &selectedName);
    if (selectedName == NULL)
        return NULL;
    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(elevenLabsModelsJson, "models",
                                   &modelsArray)) {
        if (json_object_is_type(elevenLabsModelsJson, json_type_array))
            modelsArray = elevenLabsModelsJson;
    }
    if (modelsArray == NULL)
        return NULL;
    LONG count = json_object_array_length(modelsArray);
    for (LONG i = 0; i < count; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *nameObj = NULL;
        if (modelObj == NULL)
            continue;
        if (!json_object_object_get_ex(modelObj, "name", &nameObj))
            continue;
        CONST_STRPTR n = json_object_get_string(nameObj);
        if (n == NULL || strcmp(n, selectedName) != 0)
            continue;
        struct json_object *idObj = NULL;
        if (json_object_object_get_ex(modelObj, "model_id", &idObj))
            return json_object_get_string(idObj);
        return NULL;
    }
    return NULL;
}

static CONST_STRPTR currentElevenLabsVoiceIdFromUI(void) {
    if (elevenLabsVoiceList == NULL || elevenLabsVoicesJson == NULL)
        return NULL;
    LONG active = MUIV_NList_Active_Off;
    get(elevenLabsVoiceList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off)
        return NULL;
    STRPTR selectedName = NULL;
    DoMethod(elevenLabsVoiceList, MUIM_NList_GetEntry, active, &selectedName);
    if (selectedName == NULL)
        return NULL;
    struct json_object *voicesArray = NULL;
    if (!json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                   &voicesArray)) {
        if (json_object_is_type(elevenLabsVoicesJson, json_type_array))
            voicesArray = elevenLabsVoicesJson;
    }
    if (voicesArray == NULL)
        return NULL;
    LONG count = json_object_array_length(voicesArray);
    for (LONG i = 0; i < count; i++) {
        struct json_object *voiceObj =
            json_object_array_get_idx(voicesArray, i);
        struct json_object *nameObj = NULL;
        if (voiceObj == NULL)
            continue;
        if (!json_object_object_get_ex(voiceObj, "name", &nameObj))
            continue;
        CONST_STRPTR n = json_object_get_string(nameObj);
        if (n == NULL || strcmp(n, selectedName) != 0)
            continue;
        struct json_object *idObj = NULL;
        if (json_object_object_get_ex(voiceObj, "voice_id", &idObj))
            return json_object_get_string(idObj);
        return NULL;
    }
    return NULL;
}

HOOKPROTONHNONP(SpeechProviderTestFunc, void) {
    /* Build a temporary settings struct from the current UI state. */
    struct SpeechRequestSettings s;
    memset(&s, 0, sizeof(s));

    s.speechSystem = getSpeechSystemFromCycle();

    /* Accent (Workbench) */
    if (workbenchAccentString != NULL) {
        STRPTR a = NULL;
        get(workbenchAccentString, MUIA_String_Contents, &a);
        s.accentPath = a;
    }

#ifdef __AMIGAOS3__
    if (workbenchRateString != NULL) {
        LONG v = 0;
        get(workbenchRateString, MUIA_String_Integer, &v);
        s.narratorRate = (UWORD)v;
    }
    if (workbenchPitchString != NULL) {
        LONG v = 0;
        get(workbenchPitchString, MUIA_String_Integer, &v);
        s.narratorPitch = (UWORD)v;
    }
    if (workbenchModeCycle != NULL) {
        LONG v = 0;
        get(workbenchModeCycle, MUIA_Cycle_Active, &v);
        s.narratorMode = (UWORD)((v != 0) ? 1 : 0);
    }
    if (workbenchSexCycle != NULL) {
        LONG v = 0;
        get(workbenchSexCycle, MUIA_Cycle_Active, &v);
        s.narratorSex = (UWORD)((v != 0) ? 1 : 0);
    }
#endif

    /* Flite */
    if (fliteVoiceCycle != NULL) {
        LONG fv = 0;
        get(fliteVoiceCycle, MUIA_Cycle_Active, &fv);
        s.fliteVoice = (SpeechFliteVoice)fv;
    }

    if (speechSystemUsesNetwork(s.speechSystem)) {
        STRPTR host = NULL;
        LONG port = 443;
        LONG useSSL = 1;
        LONG authType = defaultAuthForSpeechSystem(s.speechSystem);
        STRPTR endpointUrl = NULL;
        get(speechHostString, MUIA_String_Contents, &host);
        get(speechPortString, MUIA_String_Integer, &port);
        get(speechUsesSSLCycle, MUIA_Cycle_Active, &useSSL);
        get(speechAuthorizationTypeCycle, MUIA_Cycle_Active, &authType);
        get(speechEndpointUrlString, MUIA_String_Contents, &endpointUrl);
        s.host = host;
        s.port = (UWORD)port;
        s.useSSL = (useSSL == 1);
        s.authorizationType = (AuthorizationType)authType;
        s.apiEndpointUrl = endpointUrl;
    }

    /* OpenAI */
    if (openAiApiKeyString != NULL) {
        STRPTR k = NULL;
        get(openAiApiKeyString, MUIA_String_Contents, &k);
        s.openAiApiKey = k;
    }
    /* OpenAI TTS model id: prefer the row currently selected in the list,
     * fall back to the saved config so the test always has *some* model. */
    {
        STRPTR uiModelId = NULL;
        LONG modelActive = MUIV_NList_Active_Off;
        if (openAiTtsModelList != NULL)
            get(openAiTtsModelList, MUIA_NList_Active, &modelActive);
        if (modelActive != MUIV_NList_Active_Off) {
            DoMethod(openAiTtsModelList, MUIM_NList_GetEntry, modelActive,
                     &uiModelId);
        }
        s.openAiTtsModelId = (uiModelId != NULL && strlen(uiModelId) > 0)
                                 ? uiModelId
                                 : configGetOpenAITTSModelId();
    }
    if (openAiTtsVoiceCycle != NULL) {
        LONG v = 0;
        get(openAiTtsVoiceCycle, MUIA_Cycle_Active, &v);
        s.openAiTtsVoice = (OpenAITTSVoice)v;
    }
    BOOL freeVoiceInstructions = FALSE;
    s.openAiVoiceInstructions =
        getOpenAiVoiceInstructionsText(&freeVoiceInstructions);

    /* ElevenLabs */
    if (elevenLabsAPIKeyString != NULL) {
        STRPTR k = NULL;
        get(elevenLabsAPIKeyString, MUIA_String_Contents, &k);
        s.elevenLabsApiKey = k;
    }

    /* Prefer the model/voice currently selected in the UI lists; fall back to
     * the saved config when nothing is selected (these are just pointers, not
     * ownership — they remain valid for the duration of this call). */
    CONST_STRPTR uiModelId = currentElevenLabsModelIdFromUI();
    CONST_STRPTR uiVoiceId = currentElevenLabsVoiceIdFromUI();
    s.elevenLabsModel =
        uiModelId ? (STRPTR)uiModelId : configGetElevenLabsModel();
    s.elevenLabsVoiceID =
        uiVoiceId ? (STRPTR)uiVoiceId : configGetElevenLabsVoiceID();

    /* Test phrase */
    STRPTR test =
        (STRPTR) "Aurora borealis, at this time of year, at this time of day, "
                 "in "
                 "this part of the country, localized entirely within your "
                 "kitchen? - Yes.\n"
                 "May I see it? No.";

    AudioFormat fmt = AUDIO_FORMAT_PCM;
    speakTextWithSettings(test, NULL, &fmt, &s);

    if (freeVoiceInstructions && s.openAiVoiceInstructions != NULL)
        FreeVec(s.openAiVoiceInstructions);
}
MakeHook(SpeechProviderTestHook, SpeechProviderTestFunc);

HOOKPROTONHNONP(NewProfileFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(speechProfileList, MUIA_NList_Active, &active);
    if (!applyProfileFromUIToWorking(active))
        return;

    if (speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array)) {
        speechProfilesJson = json_object_new_array();
    }

    STRPTR newName = makeUniqueProfileName(STRING_DEFAULT_NEW_PROFILE_NAME, "");
    struct json_object *profile = createDefaultProfile(newName);
    json_object_array_add(speechProfilesJson, profile);

    LONG newListIndex = getBuiltinSpeechProviderCount() + visibleCustomCount() - 1;
    pendingProfileListSelect = newListIndex;
    populateProfileList();
    set(speechProfileList, MUIA_NList_Active, newListIndex);
    loadProfileIntoUI(newListIndex);

    if (speechProviderSettingsRequesterWindowObject != NULL &&
        speechProfileNameString != NULL) {
        SetAttrs(speechProviderSettingsRequesterWindowObject,
                 MUIA_Window_ActiveObject, speechProfileNameString, TAG_DONE);
    }

    if (newName != NULL)
        FreeVec(newName);
    speechSettingsDirty = FALSE;
}
MakeHook(SpeechProviderNewProfileHook, NewProfileFunc);

HOOKPROTONHNONP(DuplicateProfileFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(speechProfileList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off)
        return;
    if (!applyProfileFromUIToWorking(active))
        return;

    STRPTR currentName = NULL;
    get(speechProfileNameString, MUIA_String_Contents, &currentName);
    if ((currentName == NULL || strlen(currentName) == 0) &&
        isBuiltinListIndex(active)) {
        currentName = (STRPTR)builtinNameForSystem(
            builtinSystemForListIndex(active));
    }

    STRPTR newName =
        makeUniqueProfileName(currentName, STRING_PROFILE_COPY_SUFFIX);

    if (speechProfilesJson == NULL ||
        !json_object_is_type(speechProfilesJson, json_type_array)) {
        speechProfilesJson = json_object_new_array();
    }

    struct json_object *profile = createProfileFromUI(newName);
    json_object_array_add(speechProfilesJson, profile);

    LONG newListIndex = getBuiltinSpeechProviderCount() + visibleCustomCount() - 1;
    pendingProfileListSelect = newListIndex;
    populateProfileList();
    set(speechProfileList, MUIA_NList_Active, newListIndex);
    loadProfileIntoUI(newListIndex);

    if (speechProviderSettingsRequesterWindowObject != NULL &&
        speechProfileNameString != NULL) {
        SetAttrs(speechProviderSettingsRequesterWindowObject,
                 MUIA_Window_ActiveObject, speechProfileNameString, TAG_DONE);
    }

    if (newName != NULL)
        FreeVec(newName);
    speechSettingsDirty = FALSE;
}
MakeHook(SpeechProviderDuplicateProfileHook, DuplicateProfileFunc);

HOOKPROTONHNONP(DeleteProfileFunc, void) {
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);
    if (active < getBuiltinSpeechProviderCount())
        return;
    if (!applyProfileFromUIToWorking(active))
        return;
    int profileIndex = profileArrayIndexForVisibleCustomIndex(
        (int)(active - getBuiltinSpeechProviderCount()));
    if (profileIndex < 0 || speechProfilesJson == NULL)
        return;

    json_object_array_del_idx(speechProfilesJson, profileIndex, 1);
    pendingProfileListSelect =
        (active > 0) ? active - 1 : MUIV_NList_Active_Off;
    populateProfileList();
}
MakeHook(SpeechProviderDeleteProfileHook, DeleteProfileFunc);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
HOOKPROTONHNONP(SelectAccentFunc, void) {
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);

    struct FileRequester *fileRequester;
    if ((fileRequester = (struct FileRequester *)MUI_AllocAslRequestTags(
             ASL_FileRequest, ASLFR_Window, mainWindow, ASLFR_PopToFront, TRUE,
             ASLFR_Activate, TRUE, ASLFR_DrawersOnly, FALSE,
             ASLFR_InitialDrawer, "LOCALE:accents", ASLFR_DoPatterns, TRUE,
             ASLFR_InitialPattern, "#?.accent", TAG_DONE)) != NULL) {
        if (MUI_AslRequestTags(fileRequester, TAG_DONE)) {
            set(workbenchAccentString, MUIA_String_Contents,
                fileRequester->fr_File);
        }
        FreeAslRequest(fileRequester);
    }
}
MakeHook(SpeechProviderSelectAccentHook, SelectAccentFunc);
#endif

LONG createSpeechProviderSettingsRequesterWindow(void) {
    if (speechProviderSettingsRequesterWindowObject != NULL)
        return RETURN_OK;

    populateSpeechSystemOptions();
#ifdef __AMIGAOS3__
    narratorModeOptions[0] = STRING_NARRATOR_MODE_NATURAL;
    narratorModeOptions[1] = STRING_NARRATOR_MODE_ROBOTIC;
    narratorModeOptions[2] = NULL;
    narratorSexOptions[0] = STRING_NARRATOR_SEX_MALE;
    narratorSexOptions[1] = STRING_NARRATOR_SEX_FEMALE;
    narratorSexOptions[2] = NULL;
#endif
    speechSslOptions[0] = STRING_ENCRYPTION_NONE;
    speechSslOptions[1] = STRING_ENCRYPTION_SSL;
    speechSslOptions[2] = NULL;
    speechAuthorizationTypeOptions[AUTHORIZATION_TYPE_NONE] =
        STRING_AUTHORIZATION_NONE;
    speechAuthorizationTypeOptions[AUTHORIZATION_TYPE_BEARER] =
        STRING_AUTHORIZATION_BEARER;
    speechAuthorizationTypeOptions[AUTHORIZATION_TYPE_X_API_KEY] = "x-api-key";
    speechAuthorizationTypeOptions[AUTHORIZATION_TYPE_X_GOOGLE_API_KEY] =
        "x-goog-api-key";
    speechAuthorizationTypeOptions[AUTHORIZATION_TYPE_XI_API_KEY] =
        "xi-api-key";
    loadProfilesFromConfig();

    // clang-format off
    speechProviderSettingsRequesterWindowObject = WindowObject,
        MUIA_Window_Title, STRING_SPEECH_PROVIDER_SETTINGS,
        MUIA_Window_CloseGadget, TRUE,
        /* Keep window size stable when switching profiles (ShowMe toggles). */
        MUIA_Window_AutoAdjust, FALSE,
        MUIA_Window_SizeGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
        MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
        MUIA_Window_Width, MUIV_Window_Width_Screen(80),
        MUIA_Window_Height, MUIV_Window_Height_Screen(80),
        WindowContents, HGroup,
            /* Left panel - Profiles */
            Child, VGroup,
                MUIA_VertWeight, 100,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_PROFILES,
                MUIA_FixWidth, 170,
                Child, NListviewObject,
                    MUIA_VertWeight, 100,
                    MUIA_NListview_NList, speechProfileList = NListObject,
                        MUIA_NList_Format, "",
                        MUIA_NList_Title, FALSE,
                        MUIA_NList_MinLineHeight, 16,
                    End,
                    MUIA_CycleChain, TRUE,
                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                End,
                Child, HGroup,
                    Child, newProfileButton =
                        MUI_MakeObject(MUIO_Button, STRING_NEW, TAG_DONE),
                    Child, duplicateProfileButton =
                        MUI_MakeObject(MUIO_Button, STRING_DUPLICATE, TAG_DONE),
                    Child, deleteProfileButton =
                        MUI_MakeObject(MUIO_Button, STRING_DELETE_PROFILE, TAG_DONE),
                End,
            End,
            /* Right panel - Scrollable settings + OK/Cancel */
            Child, VGroup,
                MUIA_VertWeight, 100,
                Child, rightScrollgroup = ScrollgroupObject,
                    MUIA_VertWeight, 100,
                    MUIA_Scrollgroup_FreeHoriz, TRUE,
                    MUIA_Scrollgroup_Contents, rightVirtgroup = VirtgroupObject,
                        Child, customCommonGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PROFILE_NAME,
                            Child, ColGroup(2),
                                Child, Label(STRING_PROFILE_NAME),
                                Child, speechProfileNameString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_MaxLen, 64,
                                End,
                                Child, Label(STRING_MENU_SPEECH_SYSTEM),
                                Child, speechSystemCycle = CycleObject,
                                    MUIA_Cycle_Entries, speechSystemOptions,
                                    MUIA_CycleChain, TRUE,
                                End,
                            End,
                        End,

                        Child, speechConnectionGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_CONNECTION_SETTINGS,
                            Child, ColGroup(2),
                                Child, Label(STRING_PROXY_HOST),
                                Child, speechHostString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_MaxLen, 255,
                                End,
                                Child, Label(STRING_PROXY_PORT),
                                Child, speechPortString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_Accept, "0123456789",
                                    MUIA_String_MaxLen, 5,
                                    MUIA_String_Integer, (LONG)443,
                                End,
                                Child, Label(STRING_PROXY_ENCRYPTION),
                                Child, speechUsesSSLCycle = CycleObject,
                                    MUIA_Cycle_Entries, speechSslOptions,
                                    MUIA_Cycle_Active, 1,
                                    MUIA_CycleChain, TRUE,
                                End,
                                Child, Label(STRING_AUTHORIZATION_TYPE),
                                Child, speechAuthorizationTypeCycle = CycleObject,
                                    MUIA_Cycle_Entries, speechAuthorizationTypeOptions,
                                    MUIA_Cycle_Active, (LONG)AUTHORIZATION_TYPE_BEARER,
                                    MUIA_CycleChain, TRUE,
                                End,
                                Child, Label(STRING_MENU_OPENAI_API_ENDPOINT_URL),
                                Child, speechEndpointUrlString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_MaxLen, 255,
                                    MUIA_String_Contents, "v1",
                                End,
                            End,
                        End,

                        Child, workbenchGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_WORKBENCH_NARRATOR_DEVICE,
                            Child, ColGroup(2),
                                Child, Label(STRING_MENU_SPEECH_ACCENT),
                                Child, HGroup,
                                    Child, workbenchAccentString = StringObject,
                                        MUIA_Frame, MUIV_Frame_String,
                                        MUIA_CycleChain, TRUE,
                                    End,
                                    Child, workbenchAccentBrowseButton =
                                        MUI_MakeObject(MUIO_Button, "...", TAG_DONE),
                                End,
#ifdef __AMIGAOS3__
                                Child, Label(STRING_NARRATOR_RATE_WPM),
                                Child, workbenchRateString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_Accept, "0123456789",
                                    MUIA_String_MaxLen, 5,
                                    MUIA_String_Integer, (LONG)150,
                                End,
                                Child, Label(STRING_NARRATOR_PITCH_HZ),
                                Child, workbenchPitchString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_Accept, "0123456789",
                                    MUIA_String_MaxLen, 5,
                                    MUIA_String_Integer, (LONG)110,
                                End,
                                Child, Label(STRING_NARRATOR_MODE),
                                Child, workbenchModeCycle = CycleObject,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_Cycle_Entries, narratorModeOptions,
                                    MUIA_Cycle_Active, 0,
                                End,
                                Child, Label(STRING_NARRATOR_SEX),
                                Child, workbenchSexCycle = CycleObject,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_Cycle_Entries, narratorSexOptions,
                                    MUIA_Cycle_Active, 0,
                                End,
#endif
                                Child, Label(""),
                                Child, workbenchWarningText = TextObject,
                                    MUIA_Text_Contents, "",
                                End,
                            End,
                            Child, VSpace(0),
                        End,

                        Child, fliteGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_FLITE_VOICE,
                            Child, ColGroup(2),
                                Child, Label(STRING_MENU_FLITE_VOICE),
                                Child, fliteVoiceCycle = CycleObject,
                                    MUIA_Cycle_Entries, (ULONG)SPEECH_FLITE_VOICE_NAMES,
                                    MUIA_CycleChain, TRUE,
                                End,
                            End,
                            Child, fliteWarningText = TextObject,
                                MUIA_Text_Contents, "",
                            End,
                        End,

                        Child, openAiGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PROVIDER_OPENAI,
                            Child, VGroup,
                                MUIA_Frame, MUIV_Frame_Group,
                                MUIA_FrameTitle, STRING_MENU_OPENAI_API_KEY,
                                Child, openAiApiKeyString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_MaxLen, 256,
                                End,
                            End,
                            Child, VGroup,
                                MUIA_Frame, MUIV_Frame_Group,
                                MUIA_FrameTitle, STRING_MENU_SPEECH_OPENAI_MODEL,
                                Child, HGroup,
                                    Child, Label(STRING_CURRENT),
                                    Child, openAiTtsCurrentModelText = TextObject,
                                        MUIA_Text_Contents, STRING_NONE_SELECTED,
                                    End,
                                End,
                                Child, HGroup,
                                    Child, openAiTtsFetchModelsButton =
                                        MUI_MakeObject(MUIO_Button, STRING_FETCH_MODELS, TAG_DONE),
                                End,
                                Child, NListviewObject,
                                    MUIA_NListview_NList, openAiTtsModelList = NListObject,
                                        MUIA_NList_ConstructHook2, &ConstructStringLI_TextHook,
                                        MUIA_NList_DestructHook2, &DestructStringLI_TextHook,
                                        MUIA_NList_DisplayHook2, &DisplayStringLI_TextHook,
                                        MUIA_NList_Format, "",
                                        MUIA_NList_Title, FALSE,
                                        MUIA_NList_MinLineHeight, 16,
                                    End,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                                    MUIA_FixHeight, 80,
                                End,
                            End,
                            Child, ColGroup(2),
                                Child, Label(STRING_MENU_OPENAI_VOICE),
                                Child, openAiTtsVoiceCycle = CycleObject,
                                    MUIA_Cycle_Entries, (ULONG)OPENAI_TTS_VOICE_NAMES,
                                    MUIA_CycleChain, TRUE,
                                End,
                            End,
                            Child, VGroup,
                                MUIA_Frame, MUIV_Frame_Group,
                                MUIA_FrameTitle, STRING_MENU_OPENAI_VOICE_INSTRUCTIONS,
                                Child, openAiVoiceInstructionsEditor = TextEditorObject,
                                    MUIA_TextEditor_FixedFont, TRUE,
                                    MUIA_CycleChain, TRUE,
                                End,
                            End,
                        End,

                        Child, elevenLabsGroup = VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_MENU_ELEVENLABS_SETTINGS,
                            Child, VGroup,
                                MUIA_Frame, MUIV_Frame_Group,
                                MUIA_FrameTitle, STRING_MENU_OPENAI_API_KEY,
                                Child, elevenLabsAPIKeyString = StringObject,
                                    MUIA_Frame, MUIV_Frame_String,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_String_MaxLen, 256,
                                End,
                            End,
                            Child, VGroup,
                                MUIA_Frame, MUIV_Frame_Group,
                                MUIA_FrameTitle, STRING_MENU_ELEVENLABS_MODEL,
                                Child, HGroup,
                                    Child, Label(STRING_CURRENT),
                                    Child, elevenLabsCurrentModelText = TextObject,
                                        MUIA_Text_Contents, STRING_NONE_SELECTED,
                                    End,
                                End,
                                Child, HGroup,
                                    Child, elevenLabsFetchModelsButton =
                                        MUI_MakeObject(MUIO_Button, STRING_FETCH_ELEVENLABS_MODELS, TAG_DONE),
                                End,
                                Child, NListviewObject,
                                    MUIA_NListview_NList, elevenLabsModelList = NListObject,
                                        MUIA_NList_ConstructHook2, &ConstructStringLI_TextHook,
                                        MUIA_NList_DestructHook2, &DestructStringLI_TextHook,
                                        MUIA_NList_DisplayHook2, &DisplayStringLI_TextHook,
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
                                MUIA_FrameTitle, STRING_MENU_ELEVENLABS_VOICE,
                                Child, HGroup,
                                    Child, Label(STRING_CURRENT),
                                    Child, elevenLabsCurrentVoiceText = TextObject,
                                        MUIA_Text_Contents, STRING_NONE_SELECTED,
                                    End,
                                End,
                                Child, HGroup,
                                    Child, elevenLabsVoiceSearchString = StringObject,
                                        MUIA_Frame, MUIV_Frame_String,
                                        MUIA_CycleChain, TRUE,
                                    End,
                                    Child, elevenLabsSearchButton =
                                        MUI_MakeObject(MUIO_Button, STRING_GET_ALL_VOICES, TAG_DONE),
                                End,
                                Child, NListviewObject,
                                    MUIA_NListview_NList, elevenLabsVoiceList = NListObject,
                                        MUIA_NList_ConstructHook2, &ConstructStringLI_TextHook,
                                        MUIA_NList_DestructHook2, &DestructStringLI_TextHook,
                                        MUIA_NList_DisplayHook2, &DisplayStringLI_TextHook,
                                        MUIA_NList_Format, "",
                                        MUIA_NList_Title, FALSE,
                                        MUIA_NList_MinLineHeight, 16,
                                    End,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                                    MUIA_FixHeight, 120,
                                End,
                            End,
                        End,
                    End,
                End,
                Child, HGroup,
                    Child, okButton = MUI_MakeObject(MUIO_Button, STRING_OK, TAG_DONE),
                    Child, cancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL, TAG_DONE),
                    Child, testButton = MUI_MakeObject(MUIO_Button, STRING_TEST, TAG_DONE),
                End,
            End,
        End,
    End;
    // clang-format on

    if (speechProviderSettingsRequesterWindowObject == NULL) {
        displayError(STRING_ERROR_SPEECH_PROVIDER_SETTINGS_WINDOW_CREATE);
        return RETURN_ERROR;
    }
    updateProfileManagementButtonLabels();

    /* Notifications */
    /* Close gadget should behave like Cancel. */
    DoMethod(speechProviderSettingsRequesterWindowObject, MUIM_Notify,
             MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 2,
             MUIM_CallHook, &SpeechProviderCancelHook);

    DoMethod(speechProfileList, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderProfileSelectedHook);

    DoMethod(newProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderNewProfileHook);
    DoMethod(duplicateProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderDuplicateProfileHook);
    DoMethod(deleteProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderDeleteProfileHook);

    DoMethod(speechSystemCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderSystemChangedHook);

    DoMethod(speechAuthorizationTypeCycle, MUIM_Notify, MUIA_Cycle_Active,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderSpeechAuthTypeChangedHook);

    DoMethod(openAiTtsFetchModelsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &OpenAITTSFetchModelsButtonClickedHook);
    DoMethod(elevenLabsFetchModelsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsFetchModelsButtonClickedHook);
    DoMethod(elevenLabsSearchButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsSearchVoicesButtonClickedHook);
    DoMethod(elevenLabsVoiceSearchString, MUIM_Notify, MUIA_String_Contents,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsVoiceSearchChangedHook);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    DoMethod(workbenchAccentBrowseButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderSelectAccentHook);
#endif

    DoMethod(okButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &SpeechProviderOkHook);
    DoMethod(cancelButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderCancelHook);
    DoMethod(testButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderTestHook);

    ensureDefaultElevenLabsModelsJson();
    populateElevenLabsModelList();
    updateElevenLabsSearchButtonLabel();

    return RETURN_OK;
}

void openSpeechProviderSettingsRequesterWindow(void) {
    if (speechProviderSettingsRequesterWindowObject != NULL) {
        /* Reload profiles to pick up any changes */
        loadProfilesFromConfig();
        populateProfileList();
        updateProfileManagementButtonLabels();
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            TRUE);
    }
}

#endif /* !DAEMON */
