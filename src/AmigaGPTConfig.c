#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include "AmigaGPT_cat.h"
#include "AmigaGPTConfig.h"
#include "gui.h"

#define DEFAULT_ACCENT "american.accent"
#define CONFIG_FILE_PATH "AMIGAGPT:config.json"

struct MUI_CustomClass *amigaGPTConfigClass = NULL;
Object *configObj = NULL;

/**
 * Instance data for the AmigaGPTConfig class
 */
struct AmigaGPTConfigData {
    /* Boolean/Integer values */
    ULONG speechEnabled;
    SpeechSystem speechSystem;
    SpeechFliteVoice speechFliteVoice;
    ChatModel chatModel;
    ImageModel imageModel;
    ImageSize imageSizeDallE2;
    ImageSize imageSizeDallE3;
    ImageSize imageSizeGptImage1;
    OpenAITTSModel openAITTSModel;
    OpenAITTSVoice openAITTSVoice;
    ULONG proxyEnabled;
    ULONG proxyPort;
    ULONG proxyUsesSSL;
    ULONG proxyRequiresAuth;
    ULONG fixedWidthFonts;
    LONG userTextAlignment;
    LONG assistantTextAlignment;
    LONG webSearchEnabled;
    ULONG useCustomServer;
    ULONG customPort;
    ULONG customUseSSL;
    APIEndpoint customApiEndpoint;
    AuthorizationType customAuthorizationType;
    ImageFormat imageFormat;

    /* Version tracking */
    UWORD chatModelSetVersion;
    UWORD imageModelSetVersion;
    UWORD speechSystemSetVersion;
    UWORD openAITTSModelSetVersion;
    UWORD openAITTSVoiceSetVersion;

    /* String values */
    STRPTR speechAccent;
    STRPTR chatSystem;
    STRPTR openAIVoiceInstructions;
    STRPTR openAiApiKey;
    STRPTR proxyHost;
    STRPTR proxyUsername;
    STRPTR proxyPassword;
    STRPTR customHost;
    STRPTR customApiKey;
    STRPTR customChatModel;
    STRPTR customApiEndpointUrl;
    STRPTR customHeaders;
    STRPTR elevenLabsAPIKey;
    STRPTR elevenLabsVoiceID;
    STRPTR elevenLabsVoiceName;
    STRPTR elevenLabsModel;
    STRPTR elevenLabsModelName;

    /* Auto-save state */
    BOOL isDirty;
    BOOL saveInProgress;
};

/* Forward declarations */
static LONG saveConfig(struct AmigaGPTConfigData *data);
static LONG loadConfig(struct AmigaGPTConfigData *data);
static void freeString(STRPTR *str);
static STRPTR copyString(CONST_STRPTR src);

/**
 * Free a string and set pointer to NULL
 */
static void freeString(STRPTR *str) {
    if (*str != NULL) {
        FreeVec(*str);
        *str = NULL;
    }
}

/**
 * Allocate and copy a string
 */
static STRPTR copyString(CONST_STRPTR src) {
    if (src == NULL)
        return NULL;
    ULONG len = strlen(src);
    STRPTR dst = AllocVec(len + 1, MEMF_CLEAR);
    if (dst != NULL) {
        strncpy(dst, src, len);
    }
    return dst;
}

/**
 * Set default values for config data
 */
static void setDefaults(struct AmigaGPTConfigData *data) {
    data->speechEnabled = FALSE;
    data->speechSystem = SPEECH_SYSTEM_OPENAI;
    data->speechFliteVoice = SPEECH_FLITE_VOICE_KAL;
    data->chatModel = CHATGPT_5_LATEST;
    data->imageModel = GPT_IMAGE_1;
    data->imageSizeDallE2 = IMAGE_SIZE_256x256;
    data->imageSizeDallE3 = IMAGE_SIZE_1024x1024;
    data->imageSizeGptImage1 = IMAGE_SIZE_AUTO;
    data->openAITTSModel = OPENAI_TTS_MODEL_GPT_4o_MINI_TTS;
    data->openAITTSVoice = OPENAI_TTS_VOICE_ALLOY;
    data->proxyEnabled = FALSE;
    data->proxyPort = 8080;
    data->proxyUsesSSL = FALSE;
    data->proxyRequiresAuth = FALSE;
    data->fixedWidthFonts = FALSE;
    data->userTextAlignment = ALIGN_RIGHT;
    data->assistantTextAlignment = ALIGN_LEFT;
    data->webSearchEnabled = TRUE;
    data->useCustomServer = FALSE;
    data->customPort = 80;
    data->customUseSSL = FALSE;
    data->customApiEndpoint = API_ENDPOINT_CHAT_COMPLETIONS;
    data->customAuthorizationType = AUTHORIZATION_TYPE_BEARER;
    data->imageFormat = IMAGE_FORMAT_PNG;

    data->chatModelSetVersion = CHAT_MODEL_SET_VERSION;
    data->imageModelSetVersion = IMAGE_MODEL_SET_VERSION;
    data->speechSystemSetVersion = SPEECH_SYSTEM_SET_VERSION;
    data->openAITTSModelSetVersion = OPENAI_TTS_MODEL_SET_VERSION;
    data->openAITTSVoiceSetVersion = OPENAI_TTS_VOICE_SET_VERSION;

    /* String defaults */
    data->speechAccent = copyString(DEFAULT_ACCENT);
    data->chatSystem = NULL;
    data->openAIVoiceInstructions = NULL;
    data->openAiApiKey = NULL;
    data->proxyHost = NULL;
    data->proxyUsername = NULL;
    data->proxyPassword = NULL;
    data->customHost = NULL;
    data->customApiKey = NULL;
    data->customChatModel = NULL;
    data->customApiEndpointUrl = NULL;
    data->elevenLabsAPIKey = NULL;
    data->elevenLabsVoiceID = NULL;
    data->elevenLabsVoiceName = NULL;
    data->elevenLabsModel = NULL;
    data->elevenLabsModelName = NULL;

    data->isDirty = FALSE;
    data->saveInProgress = FALSE;
}

/**
 * Free all allocated strings in config data
 */
static void freeAllStrings(struct AmigaGPTConfigData *data) {
    freeString(&data->speechAccent);
    freeString(&data->chatSystem);
    freeString(&data->openAIVoiceInstructions);
    freeString(&data->openAiApiKey);
    freeString(&data->proxyHost);
    freeString(&data->proxyUsername);
    freeString(&data->proxyPassword);
    freeString(&data->customHost);
    freeString(&data->customApiKey);
    freeString(&data->customChatModel);
    freeString(&data->customApiEndpointUrl);
    freeString(&data->customHeaders);
    freeString(&data->elevenLabsAPIKey);
    freeString(&data->elevenLabsVoiceID);
    freeString(&data->elevenLabsVoiceName);
    freeString(&data->elevenLabsModel);
    freeString(&data->elevenLabsModelName);
}

/**
 * Mark config as dirty and trigger save
 */
static void markDirtyAndSave(struct AmigaGPTConfigData *data) {
    data->isDirty = TRUE;
    if (!data->saveInProgress) {
        data->saveInProgress = TRUE;
        saveConfig(data);
        data->saveInProgress = FALSE;
        data->isDirty = FALSE;
    }
}

/**
 * Set a string attribute, handling memory management
 */
static BOOL setStringAttr(STRPTR *target, CONST_STRPTR newValue) {
    /* Check if value actually changed */
    if (*target == newValue)
        return FALSE;
    if (*target != NULL && newValue != NULL && strcmp(*target, newValue) == 0)
        return FALSE;

    freeString(target);
    if (newValue != NULL) {
        *target = copyString(newValue);
    }
    return TRUE;
}

/**
 * OM_NEW method handler
 */
SAVEDS ULONG mConfigNew(struct IClass *cl, Object *obj, struct opSet *msg) {
    if (!(obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg)))
        return 0;

    struct AmigaGPTConfigData *data = INST_DATA(cl, obj);
    setDefaults(data);

    /* Load config from disk */
    loadConfig(data);

    return (ULONG)obj;
}

/**
 * OM_DISPOSE method handler
 */
SAVEDS ULONG mConfigDispose(struct IClass *cl, Object *obj, Msg msg) {
    struct AmigaGPTConfigData *data = INST_DATA(cl, obj);

    /* Save any pending changes */
    if (data->isDirty) {
        saveConfig(data);
    }

    freeAllStrings(data);

    return DoSuperMethodA(cl, obj, msg);
}

/**
 * OM_GET method handler
 */
SAVEDS ULONG mConfigGet(struct IClass *cl, Object *obj, struct opGet *msg) {
    struct AmigaGPTConfigData *data = INST_DATA(cl, obj);
    ULONG *store = msg->opg_Storage;

    switch (msg->opg_AttrID) {
    /* Integer attributes */
    case MUIA_AmigaGPTConfig_SpeechEnabled:
        *store = data->speechEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_SpeechSystem:
        *store = data->speechSystem;
        return TRUE;
    case MUIA_AmigaGPTConfig_SpeechFliteVoice:
        *store = data->speechFliteVoice;
        return TRUE;
    case MUIA_AmigaGPTConfig_ChatModel:
        *store = data->chatModel;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageModel:
        *store = data->imageModel;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageSizeDallE2:
        *store = data->imageSizeDallE2;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageSizeDallE3:
        *store = data->imageSizeDallE3;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageSizeGptImage1:
        *store = data->imageSizeGptImage1;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAITTSModel:
        *store = data->openAITTSModel;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAITTSVoice:
        *store = data->openAITTSVoice;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyEnabled:
        *store = data->proxyEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyPort:
        *store = data->proxyPort;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyUsesSSL:
        *store = data->proxyUsesSSL;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyRequiresAuth:
        *store = data->proxyRequiresAuth;
        return TRUE;
    case MUIA_AmigaGPTConfig_FixedWidthFonts:
        *store = data->fixedWidthFonts;
        return TRUE;
    case MUIA_AmigaGPTConfig_UserTextAlignment:
        *store = data->userTextAlignment;
        return TRUE;
    case MUIA_AmigaGPTConfig_AssistantTextAlignment:
        *store = data->assistantTextAlignment;
        return TRUE;
    case MUIA_AmigaGPTConfig_WebSearchEnabled:
        *store = data->webSearchEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_UseCustomServer:
        *store = data->useCustomServer;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomPort:
        *store = data->customPort;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomUseSSL:
        *store = data->customUseSSL;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomApiEndpoint:
        *store = data->customApiEndpoint;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageFormat:
        *store = data->imageFormat;
        return TRUE;
    case MUIA_AmigaGPTConfig_ChatModelSetVersion:
        *store = data->chatModelSetVersion;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageModelSetVersion:
        *store = data->imageModelSetVersion;
        return TRUE;
    case MUIA_AmigaGPTConfig_SpeechSystemSetVersion:
        *store = data->speechSystemSetVersion;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAITTSModelSetVersion:
        *store = data->openAITTSModelSetVersion;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAITTSVoiceSetVersion:
        *store = data->openAITTSVoiceSetVersion;
        return TRUE;

    /* String attributes */
    case MUIA_AmigaGPTConfig_SpeechAccent:
        *store = (ULONG)data->speechAccent;
        return TRUE;
    case MUIA_AmigaGPTConfig_ChatSystem:
        *store = (ULONG)data->chatSystem;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAIVoiceInstructions:
        *store = (ULONG)data->openAIVoiceInstructions;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiApiKey:
        *store = (ULONG)data->openAiApiKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyHost:
        *store = (ULONG)data->proxyHost;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyUsername:
        *store = (ULONG)data->proxyUsername;
        return TRUE;
    case MUIA_AmigaGPTConfig_ProxyPassword:
        *store = (ULONG)data->proxyPassword;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomHost:
        *store = (ULONG)data->customHost;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomApiKey:
        *store = (ULONG)data->customApiKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomChatModel:
        *store = (ULONG)data->customChatModel;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomApiEndpointUrl:
        *store = (ULONG)data->customApiEndpointUrl;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomHeaders:
        *store = (ULONG)data->customHeaders;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomAuthorizationType:
        *store = (ULONG)data->customAuthorizationType;
        return TRUE;
    case MUIA_AmigaGPTConfig_ElevenLabsAPIKey:
        *store = (ULONG)data->elevenLabsAPIKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_ElevenLabsVoiceID:
        *store = (ULONG)data->elevenLabsVoiceID;
        return TRUE;
    case MUIA_AmigaGPTConfig_ElevenLabsVoiceName:
        *store = (ULONG)data->elevenLabsVoiceName;
        return TRUE;
    case MUIA_AmigaGPTConfig_ElevenLabsModel:
        *store = (ULONG)data->elevenLabsModel;
        return TRUE;
    case MUIA_AmigaGPTConfig_ElevenLabsModelName:
        *store = (ULONG)data->elevenLabsModelName;
        return TRUE;
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/**
 * OM_SET method handler
 */
SAVEDS ULONG mConfigSet(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct AmigaGPTConfigData *data = INST_DATA(cl, obj);
    struct TagItem *tags, *tag;
    BOOL changed = FALSE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
        IPTR ti_Data = tag->ti_Data;
        switch (tag->ti_Tag) {
        /* Integer attributes */
        case MUIA_AmigaGPTConfig_SpeechEnabled:
            if (data->speechEnabled != (ULONG)ti_Data) {
                data->speechEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_SpeechSystem:
            if (data->speechSystem != (SpeechSystem)ti_Data) {
                data->speechSystem = (SpeechSystem)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_SpeechFliteVoice:
            if (data->speechFliteVoice != (SpeechFliteVoice)ti_Data) {
                data->speechFliteVoice = (SpeechFliteVoice)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ChatModel:
            if (data->chatModel != (ChatModel)ti_Data) {
                data->chatModel = (ChatModel)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageModel:
            if (data->imageModel != (ImageModel)ti_Data) {
                data->imageModel = (ImageModel)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageSizeDallE2:
            if (data->imageSizeDallE2 != (ImageSize)ti_Data) {
                data->imageSizeDallE2 = (ImageSize)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageSizeDallE3:
            if (data->imageSizeDallE3 != (ImageSize)ti_Data) {
                data->imageSizeDallE3 = (ImageSize)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageSizeGptImage1:
            if (data->imageSizeGptImage1 != (ImageSize)ti_Data) {
                data->imageSizeGptImage1 = (ImageSize)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_OpenAITTSModel:
            if (data->openAITTSModel != (OpenAITTSModel)ti_Data) {
                data->openAITTSModel = (OpenAITTSModel)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_OpenAITTSVoice:
            if (data->openAITTSVoice != (OpenAITTSVoice)ti_Data) {
                data->openAITTSVoice = (OpenAITTSVoice)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ProxyEnabled:
            if (data->proxyEnabled != (ULONG)ti_Data) {
                data->proxyEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ProxyPort:
            if (data->proxyPort != (ULONG)ti_Data) {
                data->proxyPort = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ProxyUsesSSL:
            if (data->proxyUsesSSL != (ULONG)ti_Data) {
                data->proxyUsesSSL = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ProxyRequiresAuth:
            if (data->proxyRequiresAuth != (ULONG)ti_Data) {
                data->proxyRequiresAuth = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_FixedWidthFonts:
            if (data->fixedWidthFonts != (ULONG)ti_Data) {
                data->fixedWidthFonts = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_UserTextAlignment:
            if (data->userTextAlignment != (LONG)ti_Data) {
                data->userTextAlignment = (LONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_AssistantTextAlignment:
            if (data->assistantTextAlignment != (LONG)ti_Data) {
                data->assistantTextAlignment = (LONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_WebSearchEnabled:
            if (data->webSearchEnabled != (LONG)ti_Data) {
                data->webSearchEnabled = (LONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_UseCustomServer:
            if (data->useCustomServer != (ULONG)ti_Data) {
                data->useCustomServer = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomPort:
            if (data->customPort != (ULONG)ti_Data) {
                data->customPort = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomUseSSL:
            if (data->customUseSSL != (ULONG)ti_Data) {
                data->customUseSSL = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomApiEndpoint:
            if (data->customApiEndpoint != (APIEndpoint)ti_Data) {
                data->customApiEndpoint = (APIEndpoint)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageFormat:
            if (data->imageFormat != (ImageFormat)ti_Data) {
                data->imageFormat = (ImageFormat)ti_Data;
                changed = TRUE;
            }
            break;

        /* String attributes */
        case MUIA_AmigaGPTConfig_SpeechAccent:
            if (setStringAttr(&data->speechAccent, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ChatSystem:
            if (setStringAttr(&data->chatSystem, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_OpenAIVoiceInstructions:
            if (setStringAttr(&data->openAIVoiceInstructions,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_OpenAiApiKey:
            if (setStringAttr(&data->openAiApiKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ProxyHost:
            if (setStringAttr(&data->proxyHost, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ProxyUsername:
            if (setStringAttr(&data->proxyUsername, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ProxyPassword:
            if (setStringAttr(&data->proxyPassword, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomHost:
            if (setStringAttr(&data->customHost, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomApiKey:
            if (setStringAttr(&data->customApiKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomChatModel:
            if (setStringAttr(&data->customChatModel, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomApiEndpointUrl:
            if (setStringAttr(&data->customApiEndpointUrl,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomHeaders:
            if (setStringAttr(&data->customHeaders, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomAuthorizationType:
            if (data->customAuthorizationType != (AuthorizationType)ti_Data) {
                data->customAuthorizationType = (AuthorizationType)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ElevenLabsAPIKey:
            if (setStringAttr(&data->elevenLabsAPIKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ElevenLabsVoiceID:
            if (setStringAttr(&data->elevenLabsVoiceID, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ElevenLabsVoiceName:
            if (setStringAttr(&data->elevenLabsVoiceName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ElevenLabsModel:
            if (setStringAttr(&data->elevenLabsModel, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ElevenLabsModelName:
            if (setStringAttr(&data->elevenLabsModelName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        }
    }

    /* Auto-save if any value changed */
    if (changed) {
        markDirtyAndSave(data);
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

/**
 * Save method handler
 */
SAVEDS ULONG mConfigSave(struct IClass *cl, Object *obj, Msg msg) {
    struct AmigaGPTConfigData *data = INST_DATA(cl, obj);
    return saveConfig(data);
}

/**
 * Load method handler
 */
SAVEDS ULONG mConfigLoad(struct IClass *cl, Object *obj, Msg msg) {
    struct AmigaGPTConfigData *data = INST_DATA(cl, obj);
    return loadConfig(data);
}

/**
 * Save config to disk as JSON
 */
static LONG saveConfig(struct AmigaGPTConfigData *data) {
    BPTR file = Open(CONFIG_FILE_PATH, MODE_NEWFILE);
    if (file == 0) {
        printf(STRING_ERROR_CONFIG_FILE_READ);
        putchar('\n');
        return RETURN_ERROR;
    }

    struct json_object *configJsonObject = json_object_new_object();

    /* Boolean/Integer values */
    json_object_object_add(configJsonObject, "speechEnabled",
                           json_object_new_boolean((BOOL)data->speechEnabled));
    json_object_object_add(configJsonObject, "speechSystem",
                           json_object_new_int(data->speechSystem));
    json_object_object_add(configJsonObject, "speechFliteVoice",
                           json_object_new_int(data->speechFliteVoice));
    json_object_object_add(configJsonObject, "chatModel",
                           json_object_new_int(data->chatModel));
    json_object_object_add(configJsonObject, "imageModel",
                           json_object_new_int(data->imageModel));
    json_object_object_add(configJsonObject, "imageSizeDallE2",
                           json_object_new_int(data->imageSizeDallE2));
    json_object_object_add(configJsonObject, "imageSizeDallE3",
                           json_object_new_int(data->imageSizeDallE3));
    json_object_object_add(configJsonObject, "imageSizeGptImage1",
                           json_object_new_int(data->imageSizeGptImage1));
    json_object_object_add(configJsonObject, "openAITTSModel",
                           json_object_new_int(data->openAITTSModel));
    json_object_object_add(configJsonObject, "openAITTSVoice",
                           json_object_new_int(data->openAITTSVoice));
    json_object_object_add(configJsonObject, "proxyEnabled",
                           json_object_new_boolean((BOOL)data->proxyEnabled));
    json_object_object_add(configJsonObject, "proxyPort",
                           json_object_new_int(data->proxyPort));
    json_object_object_add(configJsonObject, "proxyUsesSSL",
                           json_object_new_boolean((BOOL)data->proxyUsesSSL));
    json_object_object_add(
        configJsonObject, "proxyRequiresAuth",
        json_object_new_boolean((BOOL)data->proxyRequiresAuth));
    json_object_object_add(
        configJsonObject, "fixedWidthFonts",
        json_object_new_boolean((BOOL)data->fixedWidthFonts));
    json_object_object_add(configJsonObject, "userTextAlignment",
                           json_object_new_int(data->userTextAlignment));
    json_object_object_add(configJsonObject, "assistantTextAlignment",
                           json_object_new_int(data->assistantTextAlignment));
    json_object_object_add(
        configJsonObject, "webSearchEnabled",
        json_object_new_boolean((BOOL)data->webSearchEnabled));
    json_object_object_add(
        configJsonObject, "useCustomServer",
        json_object_new_boolean((BOOL)data->useCustomServer));
    json_object_object_add(configJsonObject, "customPort",
                           json_object_new_int(data->customPort));
    json_object_object_add(configJsonObject, "customUseSSL",
                           json_object_new_boolean((BOOL)data->customUseSSL));
    json_object_object_add(configJsonObject, "customApiEndpoint",
                           json_object_new_int(data->customApiEndpoint));
    json_object_object_add(configJsonObject, "customAuthorizationType",
                           json_object_new_int(data->customAuthorizationType));
    json_object_object_add(configJsonObject, "imageFormat",
                           json_object_new_int(data->imageFormat));

    /* Version tracking */
    json_object_object_add(configJsonObject, "chatModelSetVersion",
                           json_object_new_int(CHAT_MODEL_SET_VERSION));
    json_object_object_add(configJsonObject, "imageModelSetVersion",
                           json_object_new_int(IMAGE_MODEL_SET_VERSION));
    json_object_object_add(configJsonObject, "speechSystemSetVersion",
                           json_object_new_int(SPEECH_SYSTEM_SET_VERSION));
    json_object_object_add(configJsonObject, "openAITTSModelSetVersion",
                           json_object_new_int(OPENAI_TTS_MODEL_SET_VERSION));
    json_object_object_add(configJsonObject, "openAITTSVoiceSetVersion",
                           json_object_new_int(OPENAI_TTS_VOICE_SET_VERSION));

    /* String values */
    json_object_object_add(configJsonObject, "speechAccent",
                           data->speechAccent != NULL
                               ? json_object_new_string(data->speechAccent)
                               : NULL);
    json_object_object_add(configJsonObject, "chatSystem",
                           data->chatSystem != NULL
                               ? json_object_new_string(data->chatSystem)
                               : NULL);
    json_object_object_add(
        configJsonObject, "openAIVoiceInstructions",
        data->openAIVoiceInstructions != NULL
            ? json_object_new_string(data->openAIVoiceInstructions)
            : NULL);
    json_object_object_add(configJsonObject, "openAiApiKey",
                           data->openAiApiKey != NULL
                               ? json_object_new_string(data->openAiApiKey)
                               : NULL);
    json_object_object_add(configJsonObject, "proxyHost",
                           data->proxyHost != NULL
                               ? json_object_new_string(data->proxyHost)
                               : NULL);
    json_object_object_add(configJsonObject, "proxyUsername",
                           data->proxyUsername != NULL
                               ? json_object_new_string(data->proxyUsername)
                               : NULL);
    json_object_object_add(configJsonObject, "proxyPassword",
                           data->proxyPassword != NULL
                               ? json_object_new_string(data->proxyPassword)
                               : NULL);
    json_object_object_add(configJsonObject, "customHost",
                           data->customHost != NULL
                               ? json_object_new_string(data->customHost)
                               : NULL);
    json_object_object_add(configJsonObject, "customApiKey",
                           data->customApiKey != NULL
                               ? json_object_new_string(data->customApiKey)
                               : NULL);
    json_object_object_add(configJsonObject, "customChatModel",
                           data->customChatModel != NULL
                               ? json_object_new_string(data->customChatModel)
                               : NULL);
    json_object_object_add(
        configJsonObject, "customApiEndpointUrl",
        data->customApiEndpointUrl != NULL
            ? json_object_new_string(data->customApiEndpointUrl)
            : NULL);
    json_object_object_add(configJsonObject, "customHeaders",
                           data->customHeaders != NULL
                               ? json_object_new_string(data->customHeaders)
                               : NULL);
    json_object_object_add(configJsonObject, "elevenLabsAPIKey",
                           data->elevenLabsAPIKey != NULL
                               ? json_object_new_string(data->elevenLabsAPIKey)
                               : NULL);
    json_object_object_add(configJsonObject, "elevenLabsVoiceID",
                           data->elevenLabsVoiceID != NULL
                               ? json_object_new_string(data->elevenLabsVoiceID)
                               : NULL);
    json_object_object_add(
        configJsonObject, "elevenLabsVoiceName",
        data->elevenLabsVoiceName != NULL
            ? json_object_new_string(data->elevenLabsVoiceName)
            : NULL);
    json_object_object_add(configJsonObject, "elevenLabsModel",
                           data->elevenLabsModel != NULL
                               ? json_object_new_string(data->elevenLabsModel)
                               : NULL);
    json_object_object_add(
        configJsonObject, "elevenLabsModelName",
        data->elevenLabsModelName != NULL
            ? json_object_new_string(data->elevenLabsModelName)
            : NULL);

    STRPTR configJsonString = (STRPTR)json_object_to_json_string_ext(
        configJsonObject, JSON_C_TO_STRING_PRETTY);

    if (Write(file, configJsonString, strlen(configJsonString)) !=
        strlen(configJsonString)) {
        printf(STRING_ERROR_CONFIG_FILE_WRITE);
        putchar('\n');
        Close(file);
        json_object_put(configJsonObject);
        return RETURN_ERROR;
    }

    Close(file);
    json_object_put(configJsonObject);
    return RETURN_OK;
}

/**
 * Helper to read a string value from JSON
 */
static void readJsonString(struct json_object *jsonObj, const char *key,
                           STRPTR *target) {
    struct json_object *valueObj;
    if (json_object_object_get_ex(jsonObj, key, &valueObj)) {
        CONST_STRPTR value = json_object_get_string(valueObj);
        if (value != NULL) {
            freeString(target);
            *target = copyString(value);
        }
    }
}

/**
 * Load config from disk
 */
static LONG loadConfig(struct AmigaGPTConfigData *data) {
    BPTR file = Open(CONFIG_FILE_PATH, MODE_OLDFILE);
    if (file == 0) {
        /* No config exists, save defaults */
        saveConfig(data);
        return RETURN_OK;
    }

#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
#else
#ifdef __AMIGAOS4__
    int64_t fileSize = GetFileSize(file);
#else
    struct FileInfoBlock fib;
    ExamineFH64(file, &fib, NULL);
    int64_t fileSize = fib.fib_Size;
#endif
#endif
    STRPTR configJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
    if (Read(file, configJsonString, fileSize) != fileSize) {
        printf(STRING_ERROR_CONFIG_FILE_READ);
        putchar('\n');
        Close(file);
        FreeVec(configJsonString);
        return RETURN_ERROR;
    }

    Close(file);

    struct json_object *configJsonObject = json_tokener_parse(configJsonString);
    if (configJsonObject == NULL) {
        printf(STRING_ERROR_CONFIG_FILE_PARSE);
        putchar('\n');
        FreeVec(configJsonString);
        return RETURN_ERROR;
    }

    struct json_object *valueObj;

    /* Boolean/Integer values */
    if (json_object_object_get_ex(configJsonObject, "speechEnabled", &valueObj))
        data->speechEnabled = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "speechSystem", &valueObj))
        data->speechSystem = json_object_get_int(valueObj);

#ifdef __AMIGAOS3__
    if (data->speechSystem == SPEECH_SYSTEM_FLITE)
        data->speechSystem = SPEECH_SYSTEM_OPENAI;
#elif defined(__AMIGAOS4__)
    if (data->speechSystem == SPEECH_SYSTEM_34 ||
        data->speechSystem == SPEECH_SYSTEM_37)
        data->speechSystem = SPEECH_SYSTEM_OPENAI;
#elif defined(__MORPHOS__)
    if (data->speechSystem == SPEECH_SYSTEM_34 ||
        data->speechSystem == SPEECH_SYSTEM_37 ||
        data->speechSystem == SPEECH_SYSTEM_FLITE)
        data->speechSystem = SPEECH_SYSTEM_OPENAI;
#endif

    if (json_object_object_get_ex(configJsonObject, "speechFliteVoice",
                                  &valueObj))
        data->speechFliteVoice = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "chatModel", &valueObj) ||
        json_object_object_get_ex(configJsonObject, "model", &valueObj))
        data->chatModel = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageModel", &valueObj))
        data->imageModel = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageSizeDallE2",
                                  &valueObj))
        data->imageSizeDallE2 = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageSizeDallE3",
                                  &valueObj))
        data->imageSizeDallE3 = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageSizeGptImage1",
                                  &valueObj))
        data->imageSizeGptImage1 = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "openAITTSModel",
                                  &valueObj))
        data->openAITTSModel = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "openAITTSVoice",
                                  &valueObj))
        data->openAITTSVoice = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "proxyEnabled", &valueObj))
        data->proxyEnabled = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "proxyPort", &valueObj))
        data->proxyPort = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "proxyUsesSSL", &valueObj))
        data->proxyUsesSSL = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "proxyRequiresAuth",
                                  &valueObj))
        data->proxyRequiresAuth = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "fixedWidthFonts",
                                  &valueObj))
        data->fixedWidthFonts = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "userTextAlignment",
                                  &valueObj))
        data->userTextAlignment = json_object_get_int(valueObj);

    if (data->userTextAlignment != ALIGN_LEFT &&
        data->userTextAlignment != ALIGN_RIGHT &&
        data->userTextAlignment != ALIGN_CENTER)
        data->userTextAlignment = ALIGN_RIGHT;

    if (json_object_object_get_ex(configJsonObject, "assistantTextAlignment",
                                  &valueObj))
        data->assistantTextAlignment = json_object_get_int(valueObj);

    if (data->assistantTextAlignment != ALIGN_LEFT &&
        data->assistantTextAlignment != ALIGN_RIGHT &&
        data->assistantTextAlignment != ALIGN_CENTER)
        data->assistantTextAlignment = ALIGN_LEFT;

    if (json_object_object_get_ex(configJsonObject, "webSearchEnabled",
                                  &valueObj))
        data->webSearchEnabled = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "useCustomServer",
                                  &valueObj))
        data->useCustomServer = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customPort", &valueObj))
        data->customPort = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customUseSSL", &valueObj))
        data->customUseSSL = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customApiEndpoint",
                                  &valueObj))
        data->customApiEndpoint = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customAuthorizationType",
                                  &valueObj))
        data->customAuthorizationType = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageFormat", &valueObj))
        data->imageFormat = json_object_get_int(valueObj);

    /* Version tracking - check and reset defaults if versions changed */
    if (json_object_object_get_ex(configJsonObject, "chatModelSetVersion",
                                  &valueObj))
        data->chatModelSetVersion = json_object_get_int(valueObj);
    else
        data->chatModelSetVersion = 0;

    if (data->chatModelSetVersion != CHAT_MODEL_SET_VERSION)
        data->chatModel = CHATGPT_5_LATEST;

    if (json_object_object_get_ex(configJsonObject, "imageModelSetVersion",
                                  &valueObj))
        data->imageModelSetVersion = json_object_get_int(valueObj);
    else
        data->imageModelSetVersion = 0;

    if (data->imageModelSetVersion != IMAGE_MODEL_SET_VERSION)
        data->imageModel = GPT_IMAGE_1;

    if (json_object_object_get_ex(configJsonObject, "speechSystemSetVersion",
                                  &valueObj))
        data->speechSystemSetVersion = json_object_get_int(valueObj);
    else
        data->speechSystemSetVersion = 0;

    if (data->speechSystemSetVersion != SPEECH_SYSTEM_SET_VERSION)
        data->speechSystem = SPEECH_SYSTEM_OPENAI;

    if (json_object_object_get_ex(configJsonObject, "openAITTSModelSetVersion",
                                  &valueObj))
        data->openAITTSModelSetVersion = json_object_get_int(valueObj);
    else
        data->openAITTSModelSetVersion = 0;

    if (data->openAITTSModelSetVersion != OPENAI_TTS_MODEL_SET_VERSION)
        data->openAITTSModel = OPENAI_TTS_MODEL_GPT_4o_MINI_TTS;

    if (json_object_object_get_ex(configJsonObject, "openAITTSVoiceSetVersion",
                                  &valueObj))
        data->openAITTSVoiceSetVersion = json_object_get_int(valueObj);
    else
        data->openAITTSVoiceSetVersion = 0;

    if (data->openAITTSVoiceSetVersion != OPENAI_TTS_VOICE_SET_VERSION)
        data->openAITTSVoice = OPENAI_TTS_VOICE_ALLOY;

    /* String values */
    readJsonString(configJsonObject, "speechAccent", &data->speechAccent);
    if (data->speechAccent == NULL)
        data->speechAccent = copyString(DEFAULT_ACCENT);

    readJsonString(configJsonObject, "chatSystem", &data->chatSystem);
    readJsonString(configJsonObject, "openAIVoiceInstructions",
                   &data->openAIVoiceInstructions);
    readJsonString(configJsonObject, "openAiApiKey", &data->openAiApiKey);
    readJsonString(configJsonObject, "proxyHost", &data->proxyHost);
    readJsonString(configJsonObject, "proxyUsername", &data->proxyUsername);
    readJsonString(configJsonObject, "proxyPassword", &data->proxyPassword);
    readJsonString(configJsonObject, "customHost", &data->customHost);
    readJsonString(configJsonObject, "customApiKey", &data->customApiKey);
    readJsonString(configJsonObject, "customChatModel", &data->customChatModel);
    readJsonString(configJsonObject, "customApiEndpointUrl",
                   &data->customApiEndpointUrl);
    readJsonString(configJsonObject, "customHeaders", &data->customHeaders);
    readJsonString(configJsonObject, "elevenLabsAPIKey",
                   &data->elevenLabsAPIKey);
    readJsonString(configJsonObject, "elevenLabsVoiceID",
                   &data->elevenLabsVoiceID);
    readJsonString(configJsonObject, "elevenLabsVoiceName",
                   &data->elevenLabsVoiceName);
    readJsonString(configJsonObject, "elevenLabsModel", &data->elevenLabsModel);
    readJsonString(configJsonObject, "elevenLabsModelName",
                   &data->elevenLabsModelName);

    FreeVec(configJsonString);
    json_object_put(configJsonObject);
    return RETURN_OK;
}

/**
 * Class dispatcher
 */
DISPATCHER(ConfigDispatcher) {
    switch (msg->MethodID) {
    case OM_NEW:
        return mConfigNew(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return mConfigDispose(cl, obj, msg);
    case OM_GET:
        return mConfigGet(cl, obj, (struct opGet *)msg);
    case OM_SET:
        return mConfigSet(cl, obj, (struct opSet *)msg);
    case MUIM_AmigaGPTConfig_Load:
        return mConfigLoad(cl, obj, msg);
    case MUIM_AmigaGPTConfig_Save:
        return mConfigSave(cl, obj, msg);
    }

    return DoSuperMethodA(cl, obj, msg);
}

/**
 * Create the AmigaGPTConfig class
 */
LONG createAmigaGPTConfigClass(void) {
    if (!(amigaGPTConfigClass = MUI_CreateCustomClass(
              NULL, MUIC_Notify, NULL, sizeof(struct AmigaGPTConfigData),
              ENTRY(ConfigDispatcher))))
        return RETURN_ERROR;

    amigaGPTConfigClass->mcc_Class->cl_ID = (ClassID) "AmigaGPTConfig";
    return RETURN_OK;
}

/**
 * Delete the AmigaGPTConfig class
 */
void deleteAmigaGPTConfigClass(void) {
    if (amigaGPTConfigClass) {
        MUI_DeleteCustomClass(amigaGPTConfigClass);
        amigaGPTConfigClass = NULL;
    }
}

/**
 * Initialize the global config object
 */
LONG initConfig(void) {
    if (amigaGPTConfigClass == NULL)
        return RETURN_ERROR;

    configObj = NewObject(MUIC_AmigaGPTConfig, NULL, TAG_DONE);
    if (configObj == NULL)
        return RETURN_ERROR;

    return RETURN_OK;
}

/**
 * Cleanup the global config object
 */
void cleanupConfig(void) {
    if (configObj) {
        DisposeObject(configObj);
        configObj = NULL;
    }
}

/**
 * Initialize the MUI config class and create the config object.
 * Call this AFTER MUIMasterBase is open (after initVideo() opens libraries).
 * @return RETURN_OK on success, RETURN_ERROR on failure
 */
LONG initConfigMUI(void) {
    /* Create the MUI class */
    if (createAmigaGPTConfigClass() != RETURN_OK) {
        return RETURN_ERROR;
    }

    /* Create the config object - this will load from disk via OM_NEW */
    if (initConfig() != RETURN_OK) {
        deleteAmigaGPTConfigClass();
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

/**
 * Free the config and cleanup all resources.
 */
void freeConfig(void) {
    cleanupConfig();
    deleteAmigaGPTConfigClass();
}

/* ============================================== */
/* Config accessor getter functions              */
/* ============================================== */

ULONG configGetSpeechEnabled(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechEnabled, &val);
    return val;
}

SpeechSystem configGetSpeechSystem(void) {
    SpeechSystem val = SPEECH_SYSTEM_OPENAI;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechSystem, &val);
    return val;
}

STRPTR configGetSpeechAccent(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechAccent, &val);
    return val;
}

SpeechFliteVoice configGetSpeechFliteVoice(void) {
    SpeechFliteVoice val = SPEECH_FLITE_VOICE_KAL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechFliteVoice, &val);
    return val;
}

STRPTR configGetChatSystem(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ChatSystem, &val);
    return val;
}

ChatModel configGetChatModel(void) {
    ChatModel val = CHATGPT_5_LATEST;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ChatModel, &val);
    return val;
}

ImageModel configGetImageModel(void) {
    ImageModel val = GPT_IMAGE_1;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageModel, &val);
    return val;
}

ImageSize configGetImageSizeDallE2(void) {
    ImageSize val = IMAGE_SIZE_256x256;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageSizeDallE2, &val);
    return val;
}

ImageSize configGetImageSizeDallE3(void) {
    ImageSize val = IMAGE_SIZE_1024x1024;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageSizeDallE3, &val);
    return val;
}

ImageSize configGetImageSizeGptImage1(void) {
    ImageSize val = IMAGE_SIZE_AUTO;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageSizeGptImage1, &val);
    return val;
}

OpenAITTSModel configGetOpenAITTSModel(void) {
    OpenAITTSModel val = OPENAI_TTS_MODEL_GPT_4o_MINI_TTS;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_OpenAITTSModel, &val);
    return val;
}

OpenAITTSVoice configGetOpenAITTSVoice(void) {
    OpenAITTSVoice val = OPENAI_TTS_VOICE_ALLOY;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_OpenAITTSVoice, &val);
    return val;
}

STRPTR configGetOpenAIVoiceInstructions(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_OpenAIVoiceInstructions, &val);
    return val;
}

STRPTR configGetOpenAiApiKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_OpenAiApiKey, &val);
    return val;
}

ULONG configGetProxyEnabled(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyEnabled, &val);
    return val;
}

STRPTR configGetProxyHost(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyHost, &val);
    return val;
}

ULONG configGetProxyPort(void) {
    ULONG val = 8080;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyPort, &val);
    return val;
}

ULONG configGetProxyUsesSSL(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyUsesSSL, &val);
    return val;
}

ULONG configGetProxyRequiresAuth(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyRequiresAuth, &val);
    return val;
}

STRPTR configGetProxyUsername(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyUsername, &val);
    return val;
}

STRPTR configGetProxyPassword(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ProxyPassword, &val);
    return val;
}

ULONG configGetFixedWidthFonts(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_FixedWidthFonts, &val);
    return val;
}

LONG configGetUserTextAlignment(void) {
    LONG val = ALIGN_RIGHT;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_UserTextAlignment, &val);
    return val;
}

LONG configGetAssistantTextAlignment(void) {
    LONG val = ALIGN_LEFT;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_AssistantTextAlignment, &val);
    return val;
}

LONG configGetWebSearchEnabled(void) {
    LONG val = TRUE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_WebSearchEnabled, &val);
    return val;
}

ULONG configGetUseCustomServer(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_UseCustomServer, &val);
    return val;
}

STRPTR configGetCustomHost(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomHost, &val);
    return val;
}

ULONG configGetCustomPort(void) {
    ULONG val = 80;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomPort, &val);
    return val;
}

ULONG configGetCustomUseSSL(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomUseSSL, &val);
    return val;
}

STRPTR configGetCustomApiKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomApiKey, &val);
    return val;
}

STRPTR configGetCustomChatModel(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomChatModel, &val);
    return val;
}

APIEndpoint configGetCustomApiEndpoint(void) {
    APIEndpoint val = API_ENDPOINT_CHAT_COMPLETIONS;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomApiEndpoint, &val);
    return val;
}

STRPTR configGetCustomApiEndpointUrl(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomApiEndpointUrl, &val);
    return val;
}

AuthorizationType configGetCustomAuthorizationType(void) {
    AuthorizationType val = AUTHORIZATION_TYPE_BEARER;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomAuthorizationType, &val);
    return val;
}

STRPTR configGetCustomHeaders(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomHeaders, &val);
    return val;
}

ImageFormat configGetImageFormat(void) {
    ImageFormat val = IMAGE_FORMAT_PNG;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageFormat, &val);
    return val;
}

STRPTR configGetElevenLabsAPIKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ElevenLabsAPIKey, &val);
    return val;
}

STRPTR configGetElevenLabsVoiceID(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ElevenLabsVoiceID, &val);
    return val;
}

STRPTR configGetElevenLabsVoiceName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ElevenLabsVoiceName, &val);
    return val;
}

STRPTR configGetElevenLabsModel(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ElevenLabsModel, &val);
    return val;
}

STRPTR configGetElevenLabsModelName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ElevenLabsModelName, &val);
    return val;
}

/*
 * Setter functions - all trigger auto-save
 */

void configSetSpeechEnabled(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechEnabled, value);
}

void configSetSpeechSystem(SpeechSystem value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechSystem, value);
}

void configSetSpeechAccent(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechAccent, (ULONG)value);
}

void configSetSpeechFliteVoice(SpeechFliteVoice value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechFliteVoice, value);
}

void configSetChatSystem(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ChatSystem, (ULONG)value);
}

void configSetChatModel(ChatModel value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ChatModel, value);
}

void configSetImageModel(ImageModel value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageModel, value);
}

void configSetImageSizeDallE2(ImageSize value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageSizeDallE2, value);
}

void configSetImageSizeDallE3(ImageSize value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageSizeDallE3, value);
}

void configSetImageSizeGptImage1(ImageSize value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageSizeGptImage1, value);
}

void configSetImageFormat(ImageFormat value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageFormat, value);
}

void configSetOpenAITTSModel(OpenAITTSModel value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_OpenAITTSModel, value);
}

void configSetOpenAITTSVoice(OpenAITTSVoice value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_OpenAITTSVoice, value);
}

void configSetOpenAIVoiceInstructions(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_OpenAIVoiceInstructions,
            (ULONG)value);
}

void configSetOpenAiApiKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_OpenAiApiKey, (ULONG)value);
}

void configSetProxyEnabled(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyEnabled, value);
}

void configSetProxyHost(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyHost, (ULONG)value);
}

void configSetProxyPort(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyPort, value);
}

void configSetProxyUsesSSL(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyUsesSSL, value);
}

void configSetProxyRequiresAuth(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyRequiresAuth, value);
}

void configSetProxyUsername(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyUsername, (ULONG)value);
}

void configSetProxyPassword(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ProxyPassword, (ULONG)value);
}

void configSetFixedWidthFonts(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_FixedWidthFonts, value);
}

void configSetUserTextAlignment(LONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_UserTextAlignment, value);
}

void configSetAssistantTextAlignment(LONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_AssistantTextAlignment, value);
}

void configSetWebSearchEnabled(LONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_WebSearchEnabled, value);
}

void configSetUseCustomServer(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_UseCustomServer, value);
}

void configSetCustomHost(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomHost, (ULONG)value);
}

void configSetCustomPort(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomPort, value);
}

void configSetCustomUseSSL(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomUseSSL, value);
}

void configSetCustomApiKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomApiKey, (ULONG)value);
}

void configSetCustomChatModel(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomChatModel, (ULONG)value);
}

void configSetCustomApiEndpoint(APIEndpoint value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomApiEndpoint, value);
}

void configSetCustomApiEndpointUrl(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomApiEndpointUrl, (ULONG)value);
}

void configSetCustomAuthorizationType(AuthorizationType value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomAuthorizationType,
            (ULONG)value);
}

void configSetCustomHeaders(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomHeaders, (ULONG)value);
}

void configSetElevenLabsAPIKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ElevenLabsAPIKey, (ULONG)value);
}

void configSetElevenLabsVoiceID(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ElevenLabsVoiceID, (ULONG)value);
}

void configSetElevenLabsVoiceName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ElevenLabsVoiceName, (ULONG)value);
}

void configSetElevenLabsModel(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ElevenLabsModel, (ULONG)value);
}

void configSetElevenLabsModelName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ElevenLabsModelName, (ULONG)value);
}
