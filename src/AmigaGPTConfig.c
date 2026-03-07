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
/* Default narrator.device parameters (Workbench speech) */
#define DEFAULT_NARRATOR_RATE 150
#define DEFAULT_NARRATOR_PITCH 110
#define DEFAULT_NARRATOR_MODE 0 /* natural */
#define DEFAULT_NARRATOR_SEX 0  /* male */
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
    /* narrator.device (Workbench) parameters */
    ULONG narratorRate34;
    ULONG narratorPitch34;
    ULONG narratorMode34;
    ULONG narratorSex34;
    ULONG narratorRate37;
    ULONG narratorPitch37;
    ULONG narratorMode37;
    ULONG narratorSex37;
    ChatModel chatModel;   /* Legacy - kept for migration */
    ImageModel imageModel; /* Legacy - kept for migration */
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
    LONG shellToolEnabled;
    ULONG customChatStreamEnabled;
    ULONG openAiChatStreamEnabled;
    ULONG geminiChatStreamEnabled;
    ULONG grokChatStreamEnabled;
    ULONG anthropicChatStreamEnabled;
    ULONG openAiWebSearchEnabled;
    ULONG geminiWebSearchEnabled;
    ULONG grokWebSearchEnabled;
    ULONG anthropicWebSearchEnabled;
    ULONG openAiShellToolEnabled;
    ULONG geminiShellToolEnabled;
    ULONG grokShellToolEnabled;
    ULONG anthropicShellToolEnabled;
    ULONG customPort;
    ULONG customUseSSL;
    APIChatEndpoint customApiEndpoint;
    AuthorizationType customAuthorizationType;
    /* Custom image provider settings (separate from chat custom provider) */
    ULONG customImagePort;
    ULONG customImageUseSSL;
    AuthorizationType customImageAuthorizationType;
    ImageFormat imageFormat;
    APIImageEndpoint imageApiEndpoint;

    /* Version tracking */
    UWORD configSchemaVersion;
    UWORD chatModelSetVersion;
    UWORD imageModelSetVersion;
    UWORD speechSystemSetVersion;
    UWORD openAITTSModelSetVersion;
    UWORD openAITTSVoiceSetVersion;

    /* String values */
    STRPTR speechAccent;
    STRPTR speechAccent34;
    STRPTR speechAccent37;
    STRPTR speechProfiles;
    STRPTR activeSpeechProfileName;
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
    STRPTR customServerProfiles;
    STRPTR activeProfileName;
    STRPTR customImageHost;
    STRPTR customImageApiKey;
    STRPTR customImageModel;
    STRPTR customImageApiEndpointUrl;
    STRPTR customImageHeaders;
    STRPTR customImageServerProfiles;
    STRPTR activeImageProfileName;
    STRPTR elevenLabsAPIKey;
    STRPTR elevenLabsVoiceID;
    STRPTR elevenLabsVoiceName;
    STRPTR elevenLabsModel;
    STRPTR elevenLabsModelName;

    /* New string-based model names (new in schema v2) */
    STRPTR chatModelName;
    STRPTR imageModelName;

    /* Provider-specific API keys (new in schema v2) */
    STRPTR geminiApiKey;
    STRPTR grokApiKey;
    STRPTR anthropicApiKey;

    /* Provider-specific chat model names */
    STRPTR openAiChatModelName;
    STRPTR geminiChatModelName;
    STRPTR grokChatModelName;
    STRPTR anthropicChatModelName;

    /* Provider-specific image model names (locked image profiles) */
    STRPTR openAiImageModelName;
    STRPTR geminiImageModelName;
    STRPTR grokImageModelName;

    /* Per-locked-profile chat system prompts */
    STRPTR openAiChatSystem;
    STRPTR geminiChatSystem;
    STRPTR grokChatSystem;
    STRPTR anthropicChatSystem;

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
    data->narratorRate34 = DEFAULT_NARRATOR_RATE;
    data->narratorPitch34 = DEFAULT_NARRATOR_PITCH;
    data->narratorMode34 = DEFAULT_NARRATOR_MODE;
    data->narratorSex34 = DEFAULT_NARRATOR_SEX;
    data->narratorRate37 = DEFAULT_NARRATOR_RATE;
    data->narratorPitch37 = DEFAULT_NARRATOR_PITCH;
    data->narratorMode37 = DEFAULT_NARRATOR_MODE;
    data->narratorSex37 = DEFAULT_NARRATOR_SEX;
    data->chatModel = CHATGPT_5_LATEST; /* Legacy */
    data->imageModel = GPT_IMAGE_1;     /* Legacy */
    /* Default image size for OpenAI image generation */
    data->imageSizeDallE2 = IMAGE_SIZE_1024x1024;
    data->imageSizeDallE3 = IMAGE_SIZE_1024x1024;
    data->imageSizeGptImage1 = IMAGE_SIZE_1024x1024;
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
    data->shellToolEnabled = FALSE;
    data->customChatStreamEnabled = FALSE;
    data->openAiChatStreamEnabled = TRUE;
    data->geminiChatStreamEnabled = TRUE;
    data->grokChatStreamEnabled = TRUE;
    data->anthropicChatStreamEnabled = FALSE;
    data->openAiWebSearchEnabled = TRUE;
    data->geminiWebSearchEnabled = TRUE;
    data->grokWebSearchEnabled = TRUE;
    data->anthropicWebSearchEnabled = TRUE;
    data->openAiShellToolEnabled = FALSE;
    data->geminiShellToolEnabled = FALSE;
    data->grokShellToolEnabled = FALSE;
    data->anthropicShellToolEnabled = FALSE;
    data->customPort = 80;
    data->customUseSSL = FALSE;
    data->customApiEndpoint = API_CHAT_ENDPOINT_CHAT_COMPLETIONS;
    data->customAuthorizationType = AUTHORIZATION_TYPE_BEARER;
    data->customImagePort = 80;
    data->customImageUseSSL = FALSE;
    data->customImageAuthorizationType = AUTHORIZATION_TYPE_BEARER;
    data->imageFormat = IMAGE_FORMAT_PNG;
    data->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;

    /* Version tracking */
    data->configSchemaVersion = CONFIG_SCHEMA_VERSION;
    data->chatModelSetVersion = CHAT_MODEL_SET_VERSION;
    data->imageModelSetVersion = IMAGE_MODEL_SET_VERSION;
    data->speechSystemSetVersion = SPEECH_SYSTEM_SET_VERSION;
    data->openAITTSModelSetVersion = OPENAI_TTS_MODEL_SET_VERSION;
    data->openAITTSVoiceSetVersion = OPENAI_TTS_VOICE_SET_VERSION;

    /* String defaults */
    data->speechAccent = copyString(DEFAULT_ACCENT);
    data->speechAccent34 = copyString(DEFAULT_ACCENT);
    data->speechAccent37 = copyString(DEFAULT_ACCENT);
    data->speechProfiles = NULL;
    data->activeSpeechProfileName =
        copyString(SPEECH_SYSTEM_NAMES[data->speechSystem]
                       ? SPEECH_SYSTEM_NAMES[data->speechSystem]
                       : SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI]);
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
    data->customHeaders = NULL;
    data->customServerProfiles = NULL;
    data->activeProfileName = NULL;
    data->customImageHost = NULL;
    data->customImageApiKey = NULL;
    data->customImageModel = NULL;
    data->customImageApiEndpointUrl = NULL;
    data->customImageHeaders = NULL;
    data->customImageServerProfiles = NULL;
    data->activeImageProfileName = NULL;
    data->elevenLabsAPIKey = NULL;
    data->elevenLabsVoiceID = NULL;
    data->elevenLabsVoiceName = NULL;
    data->elevenLabsModel = NULL;
    data->elevenLabsModelName = NULL;

    /* New string-based model names (new in schema v2) */
    data->chatModelName = copyString("gpt-5-chat-latest");
    data->imageModelName = copyString("gpt-image-1.5");

    /* Provider-specific API keys */
    data->geminiApiKey = NULL;
    data->grokApiKey = NULL;
    data->anthropicApiKey = NULL;

    /* Provider-specific chat model names */
    data->openAiChatModelName = copyString(
        OPENAI_CHAT_MODELS[0] ? OPENAI_CHAT_MODELS[0] : "gpt-5-chat-latest");
    data->geminiChatModelName = copyString(
        GEMINI_CHAT_MODELS[0] ? GEMINI_CHAT_MODELS[0] : "gemini-2.5-flash");
    data->grokChatModelName =
        copyString(GROK_CHAT_MODELS[0] ? GROK_CHAT_MODELS[0] : "grok-4");
    data->anthropicChatModelName =
        copyString(ANTHROPIC_CHAT_MODELS[0] ? ANTHROPIC_CHAT_MODELS[0]
                                            : "claude-opus-4-5-20251101");

    /* Provider-specific image model names */
    data->openAiImageModelName = copyString(
        OPENAI_IMAGE_MODELS[0] ? OPENAI_IMAGE_MODELS[0] : "gpt-image-1.5");
    data->geminiImageModelName = copyString(
        GEMINI_IMAGE_MODELS[0] ? GEMINI_IMAGE_MODELS[0] : "imagen-4-ultra");
    data->grokImageModelName = copyString(
        GROK_IMAGE_MODELS[0] ? GROK_IMAGE_MODELS[0] : "grok-2-image");

    data->isDirty = FALSE;
    data->saveInProgress = FALSE;
}

/**
 * Free all allocated strings in config data
 */
static void freeAllStrings(struct AmigaGPTConfigData *data) {
    freeString(&data->speechAccent);
    freeString(&data->speechAccent34);
    freeString(&data->speechAccent37);
    freeString(&data->speechProfiles);
    freeString(&data->activeSpeechProfileName);
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
    freeString(&data->customServerProfiles);
    freeString(&data->activeProfileName);
    freeString(&data->customImageHost);
    freeString(&data->customImageApiKey);
    freeString(&data->customImageModel);
    freeString(&data->customImageApiEndpointUrl);
    freeString(&data->customImageHeaders);
    freeString(&data->customImageServerProfiles);
    freeString(&data->activeImageProfileName);
    freeString(&data->elevenLabsAPIKey);
    freeString(&data->elevenLabsVoiceID);
    freeString(&data->elevenLabsVoiceName);
    freeString(&data->elevenLabsModel);
    freeString(&data->elevenLabsModelName);
    /* New string-based model names */
    freeString(&data->chatModelName);
    freeString(&data->imageModelName);
    /* Provider-specific API keys */
    freeString(&data->geminiApiKey);
    freeString(&data->grokApiKey);
    freeString(&data->anthropicApiKey);
    /* Provider-specific chat model names */
    freeString(&data->openAiChatModelName);
    freeString(&data->geminiChatModelName);
    freeString(&data->grokChatModelName);
    freeString(&data->anthropicChatModelName);
    freeString(&data->openAiImageModelName);
    freeString(&data->geminiImageModelName);
    freeString(&data->grokImageModelName);
    /* Per-locked-profile chat system prompts */
    freeString(&data->openAiChatSystem);
    freeString(&data->geminiChatSystem);
    freeString(&data->grokChatSystem);
    freeString(&data->anthropicChatSystem);
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
    case MUIA_AmigaGPTConfig_ShellToolEnabled:
        *store = data->shellToolEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomChatStreamEnabled:
        *store = data->customChatStreamEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiChatStreamEnabled:
        *store = data->openAiChatStreamEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiChatStreamEnabled:
        *store = data->geminiChatStreamEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokChatStreamEnabled:
        *store = data->grokChatStreamEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_AnthropicChatStreamEnabled:
        *store = data->anthropicChatStreamEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiWebSearchEnabled:
        *store = data->openAiWebSearchEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiWebSearchEnabled:
        *store = data->geminiWebSearchEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokWebSearchEnabled:
        *store = data->grokWebSearchEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_AnthropicWebSearchEnabled:
        *store = data->anthropicWebSearchEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiShellToolEnabled:
        *store = data->openAiShellToolEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiShellToolEnabled:
        *store = data->geminiShellToolEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokShellToolEnabled:
        *store = data->grokShellToolEnabled;
        return TRUE;
    case MUIA_AmigaGPTConfig_AnthropicShellToolEnabled:
        *store = data->anthropicShellToolEnabled;
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
    case MUIA_AmigaGPTConfig_CustomImagePort:
        *store = data->customImagePort;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageUseSSL:
        *store = data->customImageUseSSL;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageAuthorizationType:
        *store = (ULONG)data->customImageAuthorizationType;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageFormat:
        *store = data->imageFormat;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageApiEndpoint:
        *store = data->imageApiEndpoint;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorRate34:
        *store = data->narratorRate34;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorPitch34:
        *store = data->narratorPitch34;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorMode34:
        *store = data->narratorMode34;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorSex34:
        *store = data->narratorSex34;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorRate37:
        *store = data->narratorRate37;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorPitch37:
        *store = data->narratorPitch37;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorMode37:
        *store = data->narratorMode37;
        return TRUE;
    case MUIA_AmigaGPTConfig_NarratorSex37:
        *store = data->narratorSex37;
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
    case MUIA_AmigaGPTConfig_SpeechAccent34:
        *store = (ULONG)data->speechAccent34;
        return TRUE;
    case MUIA_AmigaGPTConfig_SpeechAccent37:
        *store = (ULONG)data->speechAccent37;
        return TRUE;
    case MUIA_AmigaGPTConfig_SpeechProfiles:
        *store = (ULONG)data->speechProfiles;
        return TRUE;
    case MUIA_AmigaGPTConfig_ActiveSpeechProfileName:
        *store = (ULONG)data->activeSpeechProfileName;
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
    case MUIA_AmigaGPTConfig_CustomServerProfiles:
        *store = (ULONG)data->customServerProfiles;
        return TRUE;
    case MUIA_AmigaGPTConfig_ActiveProfileName:
        *store = (ULONG)data->activeProfileName;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageHost:
        *store = (ULONG)data->customImageHost;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageApiKey:
        *store = (ULONG)data->customImageApiKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageModel:
        *store = (ULONG)data->customImageModel;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageApiEndpointUrl:
        *store = (ULONG)data->customImageApiEndpointUrl;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageHeaders:
        *store = (ULONG)data->customImageHeaders;
        return TRUE;
    case MUIA_AmigaGPTConfig_CustomImageServerProfiles:
        *store = (ULONG)data->customImageServerProfiles;
        return TRUE;
    case MUIA_AmigaGPTConfig_ActiveImageProfileName:
        *store = (ULONG)data->activeImageProfileName;
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

    case MUIA_AmigaGPTConfig_ChatModelName:
        *store = (ULONG)data->chatModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_ImageModelName:
        *store = (ULONG)data->imageModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiApiKey:
        *store = (ULONG)data->geminiApiKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokApiKey:
        *store = (ULONG)data->grokApiKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_AnthropicApiKey:
        *store = (ULONG)data->anthropicApiKey;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiChatModelName:
        *store = (ULONG)data->openAiChatModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiChatModelName:
        *store = (ULONG)data->geminiChatModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokChatModelName:
        *store = (ULONG)data->grokChatModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_AnthropicChatModelName:
        *store = (ULONG)data->anthropicChatModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiImageModelName:
        *store = (ULONG)data->openAiImageModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiImageModelName:
        *store = (ULONG)data->geminiImageModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokImageModelName:
        *store = (ULONG)data->grokImageModelName;
        return TRUE;
    case MUIA_AmigaGPTConfig_OpenAiChatSystem:
        *store = (ULONG)data->openAiChatSystem;
        return TRUE;
    case MUIA_AmigaGPTConfig_GeminiChatSystem:
        *store = (ULONG)data->geminiChatSystem;
        return TRUE;
    case MUIA_AmigaGPTConfig_GrokChatSystem:
        *store = (ULONG)data->grokChatSystem;
        return TRUE;
    case MUIA_AmigaGPTConfig_AnthropicChatSystem:
        *store = (ULONG)data->anthropicChatSystem;
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
        case MUIA_AmigaGPTConfig_ShellToolEnabled:
            if (data->shellToolEnabled != (LONG)ti_Data) {
                data->shellToolEnabled = (LONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomChatStreamEnabled:
            if (data->customChatStreamEnabled != (ULONG)ti_Data) {
                data->customChatStreamEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_OpenAiChatStreamEnabled:
            if (data->openAiChatStreamEnabled != (ULONG)ti_Data) {
                data->openAiChatStreamEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_GeminiChatStreamEnabled:
            if (data->geminiChatStreamEnabled != (ULONG)ti_Data) {
                data->geminiChatStreamEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_GrokChatStreamEnabled:
            if (data->grokChatStreamEnabled != (ULONG)ti_Data) {
                data->grokChatStreamEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_AnthropicChatStreamEnabled:
            if (data->anthropicChatStreamEnabled != (ULONG)ti_Data) {
                data->anthropicChatStreamEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_OpenAiWebSearchEnabled:
            if (data->openAiWebSearchEnabled != (ULONG)ti_Data) {
                data->openAiWebSearchEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_GeminiWebSearchEnabled:
            if (data->geminiWebSearchEnabled != (ULONG)ti_Data) {
                data->geminiWebSearchEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_GrokWebSearchEnabled:
            if (data->grokWebSearchEnabled != (ULONG)ti_Data) {
                data->grokWebSearchEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_AnthropicWebSearchEnabled:
            if (data->anthropicWebSearchEnabled != (ULONG)ti_Data) {
                data->anthropicWebSearchEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_OpenAiShellToolEnabled:
            if (data->openAiShellToolEnabled != (ULONG)ti_Data) {
                data->openAiShellToolEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_GeminiShellToolEnabled:
            if (data->geminiShellToolEnabled != (ULONG)ti_Data) {
                data->geminiShellToolEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_GrokShellToolEnabled:
            if (data->grokShellToolEnabled != (ULONG)ti_Data) {
                data->grokShellToolEnabled = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_AnthropicShellToolEnabled:
            if (data->anthropicShellToolEnabled != (ULONG)ti_Data) {
                data->anthropicShellToolEnabled = (ULONG)ti_Data;
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
            if (data->customApiEndpoint != (APIChatEndpoint)ti_Data) {
                data->customApiEndpoint = (APIChatEndpoint)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomImagePort:
            if (data->customImagePort != (ULONG)ti_Data) {
                data->customImagePort = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomImageUseSSL:
            if (data->customImageUseSSL != (ULONG)ti_Data) {
                data->customImageUseSSL = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_CustomImageAuthorizationType:
            if (data->customImageAuthorizationType !=
                (AuthorizationType)ti_Data) {
                data->customImageAuthorizationType = (AuthorizationType)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageFormat:
            if (data->imageFormat != (ImageFormat)ti_Data) {
                data->imageFormat = (ImageFormat)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_ImageApiEndpoint:
            if (data->imageApiEndpoint != (ULONG)ti_Data) {
                data->imageApiEndpoint = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;

        case MUIA_AmigaGPTConfig_NarratorRate34:
            if (data->narratorRate34 != (ULONG)ti_Data) {
                data->narratorRate34 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorPitch34:
            if (data->narratorPitch34 != (ULONG)ti_Data) {
                data->narratorPitch34 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorMode34:
            if (data->narratorMode34 != (ULONG)ti_Data) {
                data->narratorMode34 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorSex34:
            if (data->narratorSex34 != (ULONG)ti_Data) {
                data->narratorSex34 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorRate37:
            if (data->narratorRate37 != (ULONG)ti_Data) {
                data->narratorRate37 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorPitch37:
            if (data->narratorPitch37 != (ULONG)ti_Data) {
                data->narratorPitch37 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorMode37:
            if (data->narratorMode37 != (ULONG)ti_Data) {
                data->narratorMode37 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;
        case MUIA_AmigaGPTConfig_NarratorSex37:
            if (data->narratorSex37 != (ULONG)ti_Data) {
                data->narratorSex37 = (ULONG)ti_Data;
                changed = TRUE;
            }
            break;

        /* String attributes */
        case MUIA_AmigaGPTConfig_SpeechAccent:
            if (setStringAttr(&data->speechAccent, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_SpeechAccent34:
            if (setStringAttr(&data->speechAccent34, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_SpeechAccent37:
            if (setStringAttr(&data->speechAccent37, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_SpeechProfiles:
            if (setStringAttr(&data->speechProfiles, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ActiveSpeechProfileName:
            if (setStringAttr(&data->activeSpeechProfileName,
                              (CONST_STRPTR)ti_Data))
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
        case MUIA_AmigaGPTConfig_CustomServerProfiles:
            if (setStringAttr(&data->customServerProfiles,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ActiveProfileName:
            if (setStringAttr(&data->activeProfileName, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomImageHost:
            if (setStringAttr(&data->customImageHost, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomImageApiKey:
            if (setStringAttr(&data->customImageApiKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomImageModel:
            if (setStringAttr(&data->customImageModel, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomImageApiEndpointUrl:
            if (setStringAttr(&data->customImageApiEndpointUrl,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomImageHeaders:
            if (setStringAttr(&data->customImageHeaders, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_CustomImageServerProfiles:
            if (setStringAttr(&data->customImageServerProfiles,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ActiveImageProfileName:
            if (setStringAttr(&data->activeImageProfileName,
                              (CONST_STRPTR)ti_Data))
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

        case MUIA_AmigaGPTConfig_ChatModelName:
            if (setStringAttr(&data->chatModelName, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_ImageModelName:
            if (setStringAttr(&data->imageModelName, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GeminiApiKey:
            if (setStringAttr(&data->geminiApiKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GrokApiKey:
            if (setStringAttr(&data->grokApiKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_AnthropicApiKey:
            if (setStringAttr(&data->anthropicApiKey, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_OpenAiChatModelName:
            if (setStringAttr(&data->openAiChatModelName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GeminiChatModelName:
            if (setStringAttr(&data->geminiChatModelName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GrokChatModelName:
            if (setStringAttr(&data->grokChatModelName, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_AnthropicChatModelName:
            if (setStringAttr(&data->anthropicChatModelName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_OpenAiImageModelName:
            if (setStringAttr(&data->openAiImageModelName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GeminiImageModelName:
            if (setStringAttr(&data->geminiImageModelName,
                              (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GrokImageModelName:
            if (setStringAttr(&data->grokImageModelName, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_OpenAiChatSystem:
            if (setStringAttr(&data->openAiChatSystem, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GeminiChatSystem:
            if (setStringAttr(&data->geminiChatSystem, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_GrokChatSystem:
            if (setStringAttr(&data->grokChatSystem, (CONST_STRPTR)ti_Data))
                changed = TRUE;
            break;
        case MUIA_AmigaGPTConfig_AnthropicChatSystem:
            if (setStringAttr(&data->anthropicChatSystem,
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
        configJsonObject, "shellToolEnabled",
        json_object_new_boolean((BOOL)data->shellToolEnabled));
    json_object_object_add(
        configJsonObject, "customChatStreamEnabled",
        json_object_new_boolean((BOOL)data->customChatStreamEnabled));
    json_object_object_add(
        configJsonObject, "openAiChatStreamEnabled",
        json_object_new_boolean((BOOL)data->openAiChatStreamEnabled));
    json_object_object_add(
        configJsonObject, "geminiChatStreamEnabled",
        json_object_new_boolean((BOOL)data->geminiChatStreamEnabled));
    json_object_object_add(
        configJsonObject, "grokChatStreamEnabled",
        json_object_new_boolean((BOOL)data->grokChatStreamEnabled));
    json_object_object_add(
        configJsonObject, "anthropicChatStreamEnabled",
        json_object_new_boolean((BOOL)data->anthropicChatStreamEnabled));
    json_object_object_add(
        configJsonObject, "openAiWebSearchEnabled",
        json_object_new_boolean((BOOL)data->openAiWebSearchEnabled));
    json_object_object_add(
        configJsonObject, "geminiWebSearchEnabled",
        json_object_new_boolean((BOOL)data->geminiWebSearchEnabled));
    json_object_object_add(
        configJsonObject, "grokWebSearchEnabled",
        json_object_new_boolean((BOOL)data->grokWebSearchEnabled));
    json_object_object_add(
        configJsonObject, "anthropicWebSearchEnabled",
        json_object_new_boolean((BOOL)data->anthropicWebSearchEnabled));
    json_object_object_add(
        configJsonObject, "openAiShellToolEnabled",
        json_object_new_boolean((BOOL)data->openAiShellToolEnabled));
    json_object_object_add(
        configJsonObject, "geminiShellToolEnabled",
        json_object_new_boolean((BOOL)data->geminiShellToolEnabled));
    json_object_object_add(
        configJsonObject, "grokShellToolEnabled",
        json_object_new_boolean((BOOL)data->grokShellToolEnabled));
    json_object_object_add(
        configJsonObject, "anthropicShellToolEnabled",
        json_object_new_boolean((BOOL)data->anthropicShellToolEnabled));
    json_object_object_add(configJsonObject, "customPort",
                           json_object_new_int(data->customPort));
    json_object_object_add(configJsonObject, "customUseSSL",
                           json_object_new_boolean((BOOL)data->customUseSSL));
    json_object_object_add(configJsonObject, "customApiEndpoint",
                           json_object_new_int(data->customApiEndpoint));
    json_object_object_add(configJsonObject, "customAuthorizationType",
                           json_object_new_int(data->customAuthorizationType));
    json_object_object_add(configJsonObject, "customImagePort",
                           json_object_new_int(data->customImagePort));
    json_object_object_add(
        configJsonObject, "customImageUseSSL",
        json_object_new_boolean((BOOL)data->customImageUseSSL));
    json_object_object_add(
        configJsonObject, "customImageAuthorizationType",
        json_object_new_int(data->customImageAuthorizationType));
    json_object_object_add(configJsonObject, "imageFormat",
                           json_object_new_int(data->imageFormat));
    json_object_object_add(configJsonObject, "imageApiEndpoint",
                           json_object_new_int((int)data->imageApiEndpoint));
    json_object_object_add(configJsonObject, "narratorRate34",
                           json_object_new_int((int)data->narratorRate34));
    json_object_object_add(configJsonObject, "narratorPitch34",
                           json_object_new_int((int)data->narratorPitch34));
    json_object_object_add(configJsonObject, "narratorMode34",
                           json_object_new_int((int)data->narratorMode34));
    json_object_object_add(configJsonObject, "narratorSex34",
                           json_object_new_int((int)data->narratorSex34));
    json_object_object_add(configJsonObject, "narratorRate37",
                           json_object_new_int((int)data->narratorRate37));
    json_object_object_add(configJsonObject, "narratorPitch37",
                           json_object_new_int((int)data->narratorPitch37));
    json_object_object_add(configJsonObject, "narratorMode37",
                           json_object_new_int((int)data->narratorMode37));
    json_object_object_add(configJsonObject, "narratorSex37",
                           json_object_new_int((int)data->narratorSex37));

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
    json_object_object_add(configJsonObject, "speechAccent34",
                           data->speechAccent34 != NULL
                               ? json_object_new_string(data->speechAccent34)
                               : NULL);
    json_object_object_add(configJsonObject, "speechAccent37",
                           data->speechAccent37 != NULL
                               ? json_object_new_string(data->speechAccent37)
                               : NULL);
    json_object_object_add(configJsonObject, "speechProfiles",
                           data->speechProfiles != NULL
                               ? json_object_new_string(data->speechProfiles)
                               : NULL);
    json_object_object_add(
        configJsonObject, "activeSpeechProfileName",
        data->activeSpeechProfileName != NULL
            ? json_object_new_string(data->activeSpeechProfileName)
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
    json_object_object_add(
        configJsonObject, "customServerProfiles",
        data->customServerProfiles != NULL
            ? json_object_new_string(data->customServerProfiles)
            : NULL);
    json_object_object_add(configJsonObject, "activeProfileName",
                           data->activeProfileName != NULL
                               ? json_object_new_string(data->activeProfileName)
                               : NULL);
    json_object_object_add(configJsonObject, "customImageHost",
                           data->customImageHost != NULL
                               ? json_object_new_string(data->customImageHost)
                               : NULL);
    json_object_object_add(configJsonObject, "customImageApiKey",
                           data->customImageApiKey != NULL
                               ? json_object_new_string(data->customImageApiKey)
                               : NULL);
    json_object_object_add(configJsonObject, "customImageModel",
                           data->customImageModel != NULL
                               ? json_object_new_string(data->customImageModel)
                               : NULL);
    json_object_object_add(
        configJsonObject, "customImageApiEndpointUrl",
        data->customImageApiEndpointUrl != NULL
            ? json_object_new_string(data->customImageApiEndpointUrl)
            : NULL);
    json_object_object_add(
        configJsonObject, "customImageHeaders",
        data->customImageHeaders != NULL
            ? json_object_new_string(data->customImageHeaders)
            : NULL);
    json_object_object_add(
        configJsonObject, "customImageServerProfiles",
        data->customImageServerProfiles != NULL
            ? json_object_new_string(data->customImageServerProfiles)
            : NULL);
    json_object_object_add(
        configJsonObject, "activeImageProfileName",
        data->activeImageProfileName != NULL
            ? json_object_new_string(data->activeImageProfileName)
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

    json_object_object_add(configJsonObject, "configSchemaVersion",
                           json_object_new_int(CONFIG_SCHEMA_VERSION));
    json_object_object_add(configJsonObject, "chatModelName",
                           data->chatModelName != NULL
                               ? json_object_new_string(data->chatModelName)
                               : NULL);
    json_object_object_add(configJsonObject, "imageModelName",
                           data->imageModelName != NULL
                               ? json_object_new_string(data->imageModelName)
                               : NULL);
    json_object_object_add(
        configJsonObject, "openAiImageModelName",
        data->openAiImageModelName != NULL
            ? json_object_new_string(data->openAiImageModelName)
            : NULL);
    json_object_object_add(
        configJsonObject, "geminiImageModelName",
        data->geminiImageModelName != NULL
            ? json_object_new_string(data->geminiImageModelName)
            : NULL);
    json_object_object_add(
        configJsonObject, "grokImageModelName",
        data->grokImageModelName != NULL
            ? json_object_new_string(data->grokImageModelName)
            : NULL);
    json_object_object_add(configJsonObject, "geminiApiKey",
                           data->geminiApiKey != NULL
                               ? json_object_new_string(data->geminiApiKey)
                               : NULL);
    json_object_object_add(configJsonObject, "grokApiKey",
                           data->grokApiKey != NULL
                               ? json_object_new_string(data->grokApiKey)
                               : NULL);
    json_object_object_add(configJsonObject, "anthropicApiKey",
                           data->anthropicApiKey != NULL
                               ? json_object_new_string(data->anthropicApiKey)
                               : NULL);
    json_object_object_add(
        configJsonObject, "openAiChatModelName",
        data->openAiChatModelName != NULL
            ? json_object_new_string(data->openAiChatModelName)
            : NULL);
    json_object_object_add(
        configJsonObject, "geminiChatModelName",
        data->geminiChatModelName != NULL
            ? json_object_new_string(data->geminiChatModelName)
            : NULL);
    json_object_object_add(configJsonObject, "grokChatModelName",
                           data->grokChatModelName != NULL
                               ? json_object_new_string(data->grokChatModelName)
                               : NULL);
    json_object_object_add(
        configJsonObject, "anthropicChatModelName",
        data->anthropicChatModelName != NULL
            ? json_object_new_string(data->anthropicChatModelName)
            : NULL);
    json_object_object_add(configJsonObject, "openAiChatSystem",
                           data->openAiChatSystem != NULL
                               ? json_object_new_string(data->openAiChatSystem)
                               : NULL);
    json_object_object_add(configJsonObject, "geminiChatSystem",
                           data->geminiChatSystem != NULL
                               ? json_object_new_string(data->geminiChatSystem)
                               : NULL);
    json_object_object_add(configJsonObject, "grokChatSystem",
                           data->grokChatSystem != NULL
                               ? json_object_new_string(data->grokChatSystem)
                               : NULL);
    json_object_object_add(
        configJsonObject, "anthropicChatSystem",
        data->anthropicChatSystem != NULL
            ? json_object_new_string(data->anthropicChatSystem)
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
 * Back up config file before migration
 */
static void backupConfigFile(void) {
#define CONFIG_BACKUP_PATH "AMIGAGPT:config.bak"
    BPTR srcFile = Open(CONFIG_FILE_PATH, MODE_OLDFILE);
    if (srcFile == 0)
        return;

    BPTR dstFile = Open(CONFIG_BACKUP_PATH, MODE_NEWFILE);
    if (dstFile == 0) {
        Close(srcFile);
        return;
    }

    UBYTE buffer[4096];
    LONG bytesRead;
    while ((bytesRead = Read(srcFile, buffer, sizeof(buffer))) > 0) {
        Write(dstFile, buffer, bytesRead);
    }

    Close(srcFile);
    Close(dstFile);
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
    BOOL legacyCustomProviderEnabled = FALSE;
    /* Legacy key name kept for migration without embedding it as a string. */
    static const char legacyCustomProviderFlagKey[] = {
        'u', 's', 'e', 'C', 'u', 's', 't', 'o',
        'm', 'S', 'e', 'r', 'v', 'e', 'r', 0};

    /* Check schema version and migrate if needed */
    UWORD loadedSchemaVersion = 0;
    if (json_object_object_get_ex(configJsonObject, "configSchemaVersion",
                                  &valueObj))
        loadedSchemaVersion = json_object_get_int(valueObj);

    BOOL needsMigration = (loadedSchemaVersion < CONFIG_SCHEMA_VERSION);

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

    if (json_object_object_get_ex(configJsonObject, "narratorRate34",
                                  &valueObj))
        data->narratorRate34 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorPitch34",
                                  &valueObj))
        data->narratorPitch34 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorMode34",
                                  &valueObj))
        data->narratorMode34 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorSex34", &valueObj))
        data->narratorSex34 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorRate37",
                                  &valueObj))
        data->narratorRate37 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorPitch37",
                                  &valueObj))
        data->narratorPitch37 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorMode37",
                                  &valueObj))
        data->narratorMode37 = (ULONG)json_object_get_int(valueObj);
    if (json_object_object_get_ex(configJsonObject, "narratorSex37", &valueObj))
        data->narratorSex37 = (ULONG)json_object_get_int(valueObj);

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

    if (json_object_object_get_ex(configJsonObject, "shellToolEnabled",
                                  &valueObj))
        data->shellToolEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "customChatStreamEnabled",
                                  &valueObj))
        data->customChatStreamEnabled =
            (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "openAiChatStreamEnabled",
                                  &valueObj))
        data->openAiChatStreamEnabled =
            (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "geminiChatStreamEnabled",
                                  &valueObj))
        data->geminiChatStreamEnabled =
            (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "grokChatStreamEnabled",
                                  &valueObj))
        data->grokChatStreamEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject,
                                  "anthropicChatStreamEnabled", &valueObj))
        data->anthropicChatStreamEnabled =
            (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "openAiWebSearchEnabled",
                                  &valueObj))
        data->openAiWebSearchEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "geminiWebSearchEnabled",
                                  &valueObj))
        data->geminiWebSearchEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "grokWebSearchEnabled",
                                  &valueObj))
        data->grokWebSearchEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "anthropicWebSearchEnabled",
                                  &valueObj))
        data->anthropicWebSearchEnabled =
            (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "openAiShellToolEnabled",
                                  &valueObj))
        data->openAiShellToolEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "geminiShellToolEnabled",
                                  &valueObj))
        data->geminiShellToolEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "grokShellToolEnabled",
                                  &valueObj))
        data->grokShellToolEnabled = (ULONG)json_object_get_boolean(valueObj);
    if (json_object_object_get_ex(configJsonObject, "anthropicShellToolEnabled",
                                  &valueObj))
        data->anthropicShellToolEnabled =
            (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, legacyCustomProviderFlagKey,
                                  &valueObj))
        legacyCustomProviderEnabled = (BOOL)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customPort", &valueObj))
        data->customPort = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customUseSSL", &valueObj))
        data->customUseSSL = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customApiEndpoint",
                                  &valueObj)) {
        LONG v = json_object_get_int(valueObj);
        if (v >= 0 && v <= (LONG)API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT) {
            data->customApiEndpoint = (APIChatEndpoint)v;
        } else {
            data->customApiEndpoint = API_CHAT_ENDPOINT_CHAT_COMPLETIONS;
        }
    }

    if (json_object_object_get_ex(configJsonObject, "customAuthorizationType",
                                  &valueObj))
        data->customAuthorizationType = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customImagePort",
                                  &valueObj))
        data->customImagePort = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "customImageUseSSL",
                                  &valueObj))
        data->customImageUseSSL = (ULONG)json_object_get_boolean(valueObj);

    if (json_object_object_get_ex(configJsonObject,
                                  "customImageAuthorizationType", &valueObj))
        data->customImageAuthorizationType = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageFormat", &valueObj))
        data->imageFormat = json_object_get_int(valueObj);

    if (json_object_object_get_ex(configJsonObject, "imageApiEndpoint",
                                  &valueObj)) {
        LONG v = json_object_get_int(valueObj);
        /* Accept both new values (0/1) and legacy APIEndpoint values (3/4). */
        if (v == 3) {
            data->imageApiEndpoint = API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT;
        } else if (v == 4) {
            data->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
        } else if (v == (LONG)API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT) {
            data->imageApiEndpoint = API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT;
        } else {
            data->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
        }
    } else {
        /* Default to OpenAI-compatible images generation */
        data->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
    }

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

    readJsonString(configJsonObject, "speechAccent34", &data->speechAccent34);
    readJsonString(configJsonObject, "speechAccent37", &data->speechAccent37);
    /* Migration from legacy speechAccent */
    if (data->speechAccent34 == NULL)
        data->speechAccent34 = copyString(data->speechAccent);
    if (data->speechAccent37 == NULL)
        data->speechAccent37 = copyString(data->speechAccent);

    readJsonString(configJsonObject, "speechProfiles", &data->speechProfiles);
    readJsonString(configJsonObject, "activeSpeechProfileName",
                   &data->activeSpeechProfileName);
    if (data->activeSpeechProfileName == NULL) {
        data->activeSpeechProfileName =
            copyString(SPEECH_SYSTEM_NAMES[data->speechSystem]
                           ? SPEECH_SYSTEM_NAMES[data->speechSystem]
                           : SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI]);
    }

    /* If the active speech profile name matches a built-in system, align the
     * legacy speechSystem field and validate OS support. */
    if (data->activeSpeechProfileName != NULL) {
        for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
            if (strcmp(data->activeSpeechProfileName, SPEECH_SYSTEM_NAMES[s]) ==
                0) {
                data->speechSystem = (SpeechSystem)s;
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
                /* If we had to override, also override the active profile name.
                 */
                if (strcmp(data->activeSpeechProfileName,
                           SPEECH_SYSTEM_NAMES[data->speechSystem]) != 0) {
                    freeString(&data->activeSpeechProfileName);
                    data->activeSpeechProfileName =
                        copyString(SPEECH_SYSTEM_NAMES[data->speechSystem]);
                }
                break;
            }
        }
    }

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
    readJsonString(configJsonObject, "customServerProfiles",
                   &data->customServerProfiles);
    readJsonString(configJsonObject, "activeProfileName",
                   &data->activeProfileName);
    readJsonString(configJsonObject, "customImageHost", &data->customImageHost);
    readJsonString(configJsonObject, "customImageApiKey",
                   &data->customImageApiKey);
    readJsonString(configJsonObject, "customImageModel",
                   &data->customImageModel);
    readJsonString(configJsonObject, "customImageApiEndpointUrl",
                   &data->customImageApiEndpointUrl);
    readJsonString(configJsonObject, "customImageHeaders",
                   &data->customImageHeaders);
    readJsonString(configJsonObject, "customImageServerProfiles",
                   &data->customImageServerProfiles);
    readJsonString(configJsonObject, "activeImageProfileName",
                   &data->activeImageProfileName);
    readJsonString(configJsonObject, "elevenLabsAPIKey",
                   &data->elevenLabsAPIKey);
    readJsonString(configJsonObject, "elevenLabsVoiceID",
                   &data->elevenLabsVoiceID);
    readJsonString(configJsonObject, "elevenLabsVoiceName",
                   &data->elevenLabsVoiceName);
    readJsonString(configJsonObject, "elevenLabsModel", &data->elevenLabsModel);
    readJsonString(configJsonObject, "elevenLabsModelName",
                   &data->elevenLabsModelName);

    readJsonString(configJsonObject, "chatModelName", &data->chatModelName);
    readJsonString(configJsonObject, "imageModelName", &data->imageModelName);
    /* Provider-specific image model names (locked image profiles) */
    readJsonString(configJsonObject, "openAiImageModelName",
                   &data->openAiImageModelName);
    readJsonString(configJsonObject, "geminiImageModelName",
                   &data->geminiImageModelName);
    readJsonString(configJsonObject, "grokImageModelName",
                   &data->grokImageModelName);
    readJsonString(configJsonObject, "geminiApiKey", &data->geminiApiKey);
    readJsonString(configJsonObject, "grokApiKey", &data->grokApiKey);
    readJsonString(configJsonObject, "anthropicApiKey", &data->anthropicApiKey);
    readJsonString(configJsonObject, "openAiChatModelName",
                   &data->openAiChatModelName);
    readJsonString(configJsonObject, "geminiChatModelName",
                   &data->geminiChatModelName);
    readJsonString(configJsonObject, "grokChatModelName",
                   &data->grokChatModelName);
    readJsonString(configJsonObject, "anthropicChatModelName",
                   &data->anthropicChatModelName);
    readJsonString(configJsonObject, "openAiChatSystem",
                   &data->openAiChatSystem);
    readJsonString(configJsonObject, "geminiChatSystem",
                   &data->geminiChatSystem);
    readJsonString(configJsonObject, "grokChatSystem", &data->grokChatSystem);
    readJsonString(configJsonObject, "anthropicChatSystem",
                   &data->anthropicChatSystem);

    /* Migration from old config format (schema v1 to v2) */
    if (needsMigration) {
        printf("Migrating config from schema v%d to v%d...\n",
               loadedSchemaVersion, CONFIG_SCHEMA_VERSION);

        /* Back up old config before migration */
        backupConfigFile();
        printf("Old config backed up to config.bak\n");

        /* Migrate chatModel enum to chatModelName string */
        /* Check bounds by verifying we can reach the index without hitting NULL
         */
        if (data->chatModelName == NULL) {
            BOOL validIndex = TRUE;
            for (UBYTE i = 0; i <= data->chatModel && validIndex; i++) {
                if (CHAT_MODEL_NAMES[i] == NULL) {
                    validIndex = FALSE;
                }
            }
            if (validIndex && CHAT_MODEL_NAMES[data->chatModel] != NULL) {
                data->chatModelName =
                    copyString(CHAT_MODEL_NAMES[data->chatModel]);
            }
        }
        if (data->chatModelName == NULL) {
            data->chatModelName = copyString("gpt-5-chat-latest");
        }

        /* Migrate imageModel enum to imageModelName string */
        /* Check bounds by verifying we can reach the index without hitting NULL
         */
        if (data->imageModelName == NULL) {
            BOOL validIndex = TRUE;
            for (UBYTE i = 0; i <= data->imageModel && validIndex; i++) {
                if (IMAGE_MODEL_NAMES[i] == NULL) {
                    validIndex = FALSE;
                }
            }
            if (validIndex && IMAGE_MODEL_NAMES[data->imageModel] != NULL) {
                data->imageModelName =
                    copyString(IMAGE_MODEL_NAMES[data->imageModel]);
            }
        }
        if (data->imageModelName == NULL) {
            data->imageModelName = copyString("gpt-image-1.5");
        }

        /* Update schema version */
        data->configSchemaVersion = CONFIG_SCHEMA_VERSION;

        /* Save migrated config */
        FreeVec(configJsonString);
        json_object_put(configJsonObject);
        saveConfig(data);

        printf("Config migration complete. Your old settings have been "
               "preserved.\n");
        return RETURN_OK;
    }

    /* Set defaults for any NULL string model names */
    if (data->chatModelName == NULL)
        data->chatModelName = copyString("gpt-5-chat-latest");
    if (data->imageModelName == NULL)
        data->imageModelName = copyString("gpt-image-1.5");
    if (data->openAiChatModelName == NULL) {
        data->openAiChatModelName =
            copyString(data->chatModelName
                           ? data->chatModelName
                           : (OPENAI_CHAT_MODELS[0] ? OPENAI_CHAT_MODELS[0]
                                                    : "gpt-5-chat-latest"));
    }
    if (data->geminiChatModelName == NULL) {
        data->geminiChatModelName = copyString(
            GEMINI_CHAT_MODELS[0] ? GEMINI_CHAT_MODELS[0] : "gemini-2.5-flash");
    }
    if (data->grokChatModelName == NULL) {
        data->grokChatModelName =
            copyString(GROK_CHAT_MODELS[0] ? GROK_CHAT_MODELS[0] : "grok-4");
    }
    if (data->anthropicChatModelName == NULL) {
        data->anthropicChatModelName =
            copyString(ANTHROPIC_CHAT_MODELS[0] ? ANTHROPIC_CHAT_MODELS[0]
                                                : "claude-opus-4-5-20251101");
    }

    /* If provider-specific image model keys are missing (older configs), map
     * legacy imageModelName to whichever locked image profile is active. */
    {
        struct json_object *tmp = NULL;
        BOOL hasOpenAi = json_object_object_get_ex(
            configJsonObject, "openAiImageModelName", &tmp);
        BOOL hasGemini = json_object_object_get_ex(
            configJsonObject, "geminiImageModelName", &tmp);
        BOOL hasGrok = json_object_object_get_ex(configJsonObject,
                                                 "grokImageModelName", &tmp);
        if (!hasOpenAi && !hasGemini && !hasGrok &&
            data->imageModelName != NULL) {
            CONST_STRPTR activeImg = data->activeImageProfileName;
            if (activeImg != NULL && strlen(activeImg) > 0) {
                if (strcmp(activeImg, "Google Gemini") == 0) {
                    freeString(&data->geminiImageModelName);
                    data->geminiImageModelName =
                        copyString(data->imageModelName);
                } else if (strcmp(activeImg, "xAI Grok") == 0) {
                    freeString(&data->grokImageModelName);
                    data->grokImageModelName = copyString(data->imageModelName);
                } else {
                    freeString(&data->openAiImageModelName);
                    data->openAiImageModelName =
                        copyString(data->imageModelName);
                }
            } else {
                freeString(&data->openAiImageModelName);
                data->openAiImageModelName = copyString(data->imageModelName);
            }
        }
    }

    if (data->openAiImageModelName == NULL)
        data->openAiImageModelName = copyString(
            OPENAI_IMAGE_MODELS[0] ? OPENAI_IMAGE_MODELS[0] : "gpt-image-1.5");
    if (data->geminiImageModelName == NULL)
        data->geminiImageModelName =
            copyString(GEMINI_IMAGE_MODELS[0] ? GEMINI_IMAGE_MODELS[0]
                                              : "gemini-2.5-flash-image");
    if (data->grokImageModelName == NULL)
        data->grokImageModelName = copyString(
            GROK_IMAGE_MODELS[0] ? GROK_IMAGE_MODELS[0] : "grok-imagine-image");

    /* Migrate global webSearchEnabled / shellToolEnabled / chatSystem into
     * per-profile fields if this is the first run after the upgrade. */
    {
        struct json_object *tmp = NULL;
        BOOL hasPerProfileKeys = json_object_object_get_ex(
            configJsonObject, "openAiWebSearchEnabled", &tmp);
        if (!hasPerProfileKeys) {
            CONST_STRPTR active = data->activeProfileName;
            BOOL isOpenAi = (active != NULL && strcmp(active, "OpenAI") == 0);
            BOOL isGemini =
                (active != NULL && strcmp(active, "Google Gemini") == 0);
            BOOL isGrok = (active != NULL && strcmp(active, "xAI Grok") == 0);
            BOOL isAnthropic =
                (active != NULL && strcmp(active, "Anthropic Claude") == 0);
            BOOL isLocked = isOpenAi || isGemini || isGrok || isAnthropic;

            if (isLocked) {
                if (isOpenAi) {
                    data->openAiWebSearchEnabled = data->webSearchEnabled;
                    data->openAiShellToolEnabled = data->shellToolEnabled;
                    freeString(&data->openAiChatSystem);
                    data->openAiChatSystem = copyString(data->chatSystem);
                } else if (isGemini) {
                    data->geminiWebSearchEnabled = data->webSearchEnabled;
                    freeString(&data->geminiChatSystem);
                    data->geminiChatSystem = copyString(data->chatSystem);
                } else if (isGrok) {
                    data->grokWebSearchEnabled = data->webSearchEnabled;
                    freeString(&data->grokChatSystem);
                    data->grokChatSystem = copyString(data->chatSystem);
                } else if (isAnthropic) {
                    data->anthropicWebSearchEnabled = data->webSearchEnabled;
                    freeString(&data->anthropicChatSystem);
                    data->anthropicChatSystem = copyString(data->chatSystem);
                }
            } else if (active != NULL && strlen(active) > 0) {
                /* Custom profile -- inject into the JSON array */
                struct json_object *arr = json_tokener_parse(
                    data->customServerProfiles ? data->customServerProfiles
                                               : "[]");
                if (arr != NULL && json_object_is_type(arr, json_type_array)) {
                    int len = json_object_array_length(arr);
                    for (int i = 0; i < len; i++) {
                        struct json_object *p =
                            json_object_array_get_idx(arr, i);
                        struct json_object *nameObj =
                            json_object_object_get(p, "name");
                        if (nameObj != NULL &&
                            strcmp(json_object_get_string(nameObj), active) ==
                                0) {
                            json_object_object_add(
                                p, "webSearchEnabled",
                                json_object_new_boolean(
                                    (BOOL)data->webSearchEnabled));
                            json_object_object_add(
                                p, "shellToolEnabled",
                                json_object_new_boolean(
                                    (BOOL)data->shellToolEnabled));
                            json_object_object_add(
                                p, "chatSystem",
                                json_object_new_string(
                                    data->chatSystem ? data->chatSystem : ""));
                            break;
                        }
                    }
                    CONST_STRPTR arrStr = json_object_to_json_string(arr);
                    freeString(&data->customServerProfiles);
                    data->customServerProfiles = copyString(arrStr);
                    json_object_put(arr);
                }
            } else {
                /* No active profile at all (pre-profiles user) -- default to
                 * OpenAI */
                data->openAiWebSearchEnabled = data->webSearchEnabled;
                data->openAiShellToolEnabled = data->shellToolEnabled;
                freeString(&data->openAiChatSystem);
                data->openAiChatSystem = copyString(data->chatSystem);
            }
            saveConfig(data);
        }
    }

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

STRPTR configGetSpeechAccent34(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechAccent34, &val);
    return val;
}

STRPTR configGetSpeechAccent37(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechAccent37, &val);
    return val;
}

ULONG configGetNarratorRate34(void) {
    ULONG val = DEFAULT_NARRATOR_RATE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorRate34, &val);
    return val;
}
ULONG configGetNarratorPitch34(void) {
    ULONG val = DEFAULT_NARRATOR_PITCH;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorPitch34, &val);
    return val;
}
ULONG configGetNarratorMode34(void) {
    ULONG val = DEFAULT_NARRATOR_MODE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorMode34, &val);
    return val;
}
ULONG configGetNarratorSex34(void) {
    ULONG val = DEFAULT_NARRATOR_SEX;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorSex34, &val);
    return val;
}
ULONG configGetNarratorRate37(void) {
    ULONG val = DEFAULT_NARRATOR_RATE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorRate37, &val);
    return val;
}
ULONG configGetNarratorPitch37(void) {
    ULONG val = DEFAULT_NARRATOR_PITCH;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorPitch37, &val);
    return val;
}
ULONG configGetNarratorMode37(void) {
    ULONG val = DEFAULT_NARRATOR_MODE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorMode37, &val);
    return val;
}
ULONG configGetNarratorSex37(void) {
    ULONG val = DEFAULT_NARRATOR_SEX;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_NarratorSex37, &val);
    return val;
}

STRPTR configGetSpeechProfiles(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechProfiles, &val);
    return val;
}

STRPTR configGetActiveSpeechProfileName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ActiveSpeechProfileName, &val);
    return val;
}

SpeechFliteVoice configGetSpeechFliteVoice(void) {
    SpeechFliteVoice val = SPEECH_FLITE_VOICE_KAL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_SpeechFliteVoice, &val);
    return val;
}

static STRPTR dupStrCfg(CONST_STRPTR s) {
    if (s == NULL)
        return NULL;
    ULONG len = strlen(s);
    STRPTR out = AllocVec(len + 1, MEMF_CLEAR);
    if (out != NULL)
        strncpy(out, s, len);
    return out;
}

void configFreeSpeechRequestSettings(struct SpeechRequestSettings *out) {
    if (out == NULL)
        return;
    if (out->activeProfileName != NULL)
        FreeVec(out->activeProfileName);
    if (out->accentPath != NULL)
        FreeVec(out->accentPath);
    if (out->openAiApiKey != NULL)
        FreeVec(out->openAiApiKey);
    if (out->openAiVoiceInstructions != NULL)
        FreeVec(out->openAiVoiceInstructions);
    if (out->elevenLabsApiKey != NULL)
        FreeVec(out->elevenLabsApiKey);
    if (out->elevenLabsVoiceID != NULL)
        FreeVec(out->elevenLabsVoiceID);
    if (out->elevenLabsVoiceName != NULL)
        FreeVec(out->elevenLabsVoiceName);
    if (out->elevenLabsModel != NULL)
        FreeVec(out->elevenLabsModel);
    if (out->elevenLabsModelName != NULL)
        FreeVec(out->elevenLabsModelName);
    memset(out, 0, sizeof(*out));
}

void configGetSpeechRequestSettings(struct SpeechRequestSettings *out) {
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    out->activeProfileName = dupStrCfg(configGetActiveSpeechProfileName());
    out->speechSystem = (SpeechSystem)configGetSpeechSystem();
    out->fliteVoice = configGetSpeechFliteVoice();
    out->openAiApiKey = dupStrCfg(configGetOpenAiApiKey());
    out->openAiTtsModel = configGetOpenAITTSModel();
    out->openAiTtsVoice = configGetOpenAITTSVoice();
    out->openAiVoiceInstructions =
        dupStrCfg(configGetOpenAIVoiceInstructions());
    out->elevenLabsApiKey = dupStrCfg(configGetElevenLabsAPIKey());
    out->elevenLabsVoiceID = dupStrCfg(configGetElevenLabsVoiceID());
    out->elevenLabsVoiceName = dupStrCfg(configGetElevenLabsVoiceName());
    out->elevenLabsModel = dupStrCfg(configGetElevenLabsModel());
    out->elevenLabsModelName = dupStrCfg(configGetElevenLabsModelName());

    /* Default accent + narrator params based on system */
    if (out->speechSystem == SPEECH_SYSTEM_34) {
        out->accentPath = dupStrCfg(configGetSpeechAccent34());
        out->narratorRate = (UWORD)configGetNarratorRate34();
        out->narratorPitch = (UWORD)configGetNarratorPitch34();
        out->narratorMode = (UWORD)configGetNarratorMode34();
        out->narratorSex = (UWORD)configGetNarratorSex34();
    } else if (out->speechSystem == SPEECH_SYSTEM_37) {
        out->accentPath = dupStrCfg(configGetSpeechAccent37());
        out->narratorRate = (UWORD)configGetNarratorRate37();
        out->narratorPitch = (UWORD)configGetNarratorPitch37();
        out->narratorMode = (UWORD)configGetNarratorMode37();
        out->narratorSex = (UWORD)configGetNarratorSex37();
    } else {
        out->accentPath = dupStrCfg(configGetSpeechAccent());
        out->narratorRate = DEFAULT_NARRATOR_RATE;
        out->narratorPitch = DEFAULT_NARRATOR_PITCH;
        out->narratorMode = DEFAULT_NARRATOR_MODE;
        out->narratorSex = DEFAULT_NARRATOR_SEX;
    }

    /* If an active custom speech profile is selected, override fields from it.
     */
    STRPTR activeName = out->activeProfileName;
    if (activeName == NULL || strlen(activeName) == 0)
        return;

    /* If activeName matches a built-in system name, we're done. */
    for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
        if (strcmp(activeName, SPEECH_SYSTEM_NAMES[s]) == 0) {
            out->speechSystem = (SpeechSystem)s;
            if (out->accentPath != NULL) {
                FreeVec(out->accentPath);
                out->accentPath = NULL;
            }
            if (out->speechSystem == SPEECH_SYSTEM_34)
                out->accentPath = dupStrCfg(configGetSpeechAccent34());
            else if (out->speechSystem == SPEECH_SYSTEM_37)
                out->accentPath = dupStrCfg(configGetSpeechAccent37());
            if (out->speechSystem == SPEECH_SYSTEM_34) {
                out->narratorRate = (UWORD)configGetNarratorRate34();
                out->narratorPitch = (UWORD)configGetNarratorPitch34();
                out->narratorMode = (UWORD)configGetNarratorMode34();
                out->narratorSex = (UWORD)configGetNarratorSex34();
            } else if (out->speechSystem == SPEECH_SYSTEM_37) {
                out->narratorRate = (UWORD)configGetNarratorRate37();
                out->narratorPitch = (UWORD)configGetNarratorPitch37();
                out->narratorMode = (UWORD)configGetNarratorMode37();
                out->narratorSex = (UWORD)configGetNarratorSex37();
            }
            return;
        }
    }

    STRPTR profilesStr = configGetSpeechProfiles();
    if (profilesStr == NULL || strlen(profilesStr) == 0)
        return;

    struct json_object *arr = json_tokener_parse(profilesStr);
    if (arr == NULL || !json_object_is_type(arr, json_type_array)) {
        if (arr != NULL)
            json_object_put(arr);
        return;
    }

    int len = json_object_array_length(arr);
    for (int i = 0; i < len; i++) {
        struct json_object *p = json_object_array_get_idx(arr, i);
        if (p == NULL || !json_object_is_type(p, json_type_object))
            continue;
        struct json_object *nameObj = json_object_object_get(p, "name");
        CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
        if (name == NULL || strcmp(name, activeName) != 0)
            continue;

        struct json_object *sysObj = json_object_object_get(p, "speechSystem");
        if (sysObj != NULL)
            out->speechSystem = (SpeechSystem)json_object_get_int(sysObj);

        /* narrator defaults follow the chosen system unless profile overrides
         */
        if (out->speechSystem == SPEECH_SYSTEM_34) {
            out->narratorRate = (UWORD)configGetNarratorRate34();
            out->narratorPitch = (UWORD)configGetNarratorPitch34();
            out->narratorMode = (UWORD)configGetNarratorMode34();
            out->narratorSex = (UWORD)configGetNarratorSex34();
        } else if (out->speechSystem == SPEECH_SYSTEM_37) {
            out->narratorRate = (UWORD)configGetNarratorRate37();
            out->narratorPitch = (UWORD)configGetNarratorPitch37();
            out->narratorMode = (UWORD)configGetNarratorMode37();
            out->narratorSex = (UWORD)configGetNarratorSex37();
        }

        struct json_object *accentObj =
            json_object_object_get(p, "speechAccent");
        if (accentObj != NULL) {
            if (out->accentPath != NULL) {
                FreeVec(out->accentPath);
                out->accentPath = NULL;
            }
            out->accentPath = dupStrCfg(json_object_get_string(accentObj));
        }

        struct json_object *fvObj =
            json_object_object_get(p, "speechFliteVoice");
        if (fvObj != NULL)
            out->fliteVoice = (SpeechFliteVoice)json_object_get_int(fvObj);

        struct json_object *nr = json_object_object_get(p, "narratorRate");
        if (nr != NULL)
            out->narratorRate = (UWORD)json_object_get_int(nr);
        struct json_object *np = json_object_object_get(p, "narratorPitch");
        if (np != NULL)
            out->narratorPitch = (UWORD)json_object_get_int(np);
        struct json_object *nm = json_object_object_get(p, "narratorMode");
        if (nm != NULL)
            out->narratorMode = (UWORD)json_object_get_int(nm);
        struct json_object *ns = json_object_object_get(p, "narratorSex");
        if (ns != NULL)
            out->narratorSex = (UWORD)json_object_get_int(ns);

        struct json_object *k = json_object_object_get(p, "openAiApiKey");
        if (k != NULL) {
            if (out->openAiApiKey != NULL) {
                FreeVec(out->openAiApiKey);
                out->openAiApiKey = NULL;
            }
            out->openAiApiKey = dupStrCfg(json_object_get_string(k));
        }
        struct json_object *m = json_object_object_get(p, "openAITTSModel");
        if (m != NULL)
            out->openAiTtsModel = (OpenAITTSModel)json_object_get_int(m);
        struct json_object *v = json_object_object_get(p, "openAITTSVoice");
        if (v != NULL)
            out->openAiTtsVoice = (OpenAITTSVoice)json_object_get_int(v);
        struct json_object *instr =
            json_object_object_get(p, "openAIVoiceInstructions");
        if (instr != NULL) {
            if (out->openAiVoiceInstructions != NULL) {
                FreeVec(out->openAiVoiceInstructions);
                out->openAiVoiceInstructions = NULL;
            }
            out->openAiVoiceInstructions =
                dupStrCfg(json_object_get_string(instr));
        }

        struct json_object *eKey =
            json_object_object_get(p, "elevenLabsAPIKey");
        if (eKey != NULL) {
            if (out->elevenLabsApiKey != NULL) {
                FreeVec(out->elevenLabsApiKey);
                out->elevenLabsApiKey = NULL;
            }
            out->elevenLabsApiKey = dupStrCfg(json_object_get_string(eKey));
        }
        struct json_object *eVid =
            json_object_object_get(p, "elevenLabsVoiceID");
        if (eVid != NULL) {
            if (out->elevenLabsVoiceID != NULL) {
                FreeVec(out->elevenLabsVoiceID);
                out->elevenLabsVoiceID = NULL;
            }
            out->elevenLabsVoiceID = dupStrCfg(json_object_get_string(eVid));
        }
        struct json_object *eVn =
            json_object_object_get(p, "elevenLabsVoiceName");
        if (eVn != NULL) {
            if (out->elevenLabsVoiceName != NULL) {
                FreeVec(out->elevenLabsVoiceName);
                out->elevenLabsVoiceName = NULL;
            }
            out->elevenLabsVoiceName = dupStrCfg(json_object_get_string(eVn));
        }
        struct json_object *eMid = json_object_object_get(p, "elevenLabsModel");
        if (eMid != NULL) {
            if (out->elevenLabsModel != NULL) {
                FreeVec(out->elevenLabsModel);
                out->elevenLabsModel = NULL;
            }
            out->elevenLabsModel = dupStrCfg(json_object_get_string(eMid));
        }
        struct json_object *eMn =
            json_object_object_get(p, "elevenLabsModelName");
        if (eMn != NULL) {
            if (out->elevenLabsModelName != NULL) {
                FreeVec(out->elevenLabsModelName);
                out->elevenLabsModelName = NULL;
            }
            out->elevenLabsModelName = dupStrCfg(json_object_get_string(eMn));
        }

        break;
    }

    json_object_put(arr);
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

APIChatEndpoint configGetCustomApiEndpoint(void) {
    APIChatEndpoint val = API_CHAT_ENDPOINT_CHAT_COMPLETIONS;
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

STRPTR configGetCustomServerProfiles(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomServerProfiles, &val);
    return val;
}

STRPTR configGetActiveProfileName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ActiveProfileName, &val);
    return val;
}

STRPTR configGetCustomImageHost(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageHost, &val);
    return val;
}

ULONG configGetCustomImagePort(void) {
    ULONG val = 80;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImagePort, &val);
    return val;
}

ULONG configGetCustomImageUseSSL(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageUseSSL, &val);
    return val;
}

AuthorizationType configGetCustomImageAuthorizationType(void) {
    AuthorizationType val = AUTHORIZATION_TYPE_BEARER;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageAuthorizationType, &val);
    return val;
}

STRPTR configGetCustomImageApiKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageApiKey, &val);
    return val;
}

STRPTR configGetCustomImageModel(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageModel, &val);
    return val;
}

STRPTR configGetCustomImageApiEndpointUrl(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageApiEndpointUrl, &val);
    return val;
}

STRPTR configGetCustomImageHeaders(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageHeaders, &val);
    return val;
}

STRPTR configGetCustomImageServerProfiles(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomImageServerProfiles, &val);
    return val;
}

STRPTR configGetActiveImageProfileName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ActiveImageProfileName, &val);
    return val;
}

ImageFormat configGetImageFormat(void) {
    ImageFormat val = IMAGE_FORMAT_PNG;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageFormat, &val);
    return val;
}

APIImageEndpoint configGetImageApiEndpoint(void) {
    APIImageEndpoint val = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageApiEndpoint, &val);
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

void configSetSpeechAccent34(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechAccent34, (ULONG)value);
}

void configSetSpeechAccent37(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechAccent37, (ULONG)value);
}

void configSetNarratorRate34(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorRate34, value);
}
void configSetNarratorPitch34(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorPitch34, value);
}
void configSetNarratorMode34(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorMode34, value);
}
void configSetNarratorSex34(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorSex34, value);
}
void configSetNarratorRate37(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorRate37, value);
}
void configSetNarratorPitch37(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorPitch37, value);
}
void configSetNarratorMode37(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorMode37, value);
}
void configSetNarratorSex37(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_NarratorSex37, value);
}

void configSetSpeechProfiles(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_SpeechProfiles, (ULONG)value);
}

void configSetActiveSpeechProfileName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ActiveSpeechProfileName,
            (ULONG)value);
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

void configSetImageApiEndpoint(APIImageEndpoint value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageApiEndpoint, value);
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

LONG configGetShellToolEnabled(void) {
    LONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ShellToolEnabled, &val);
    return val;
}

void configSetShellToolEnabled(LONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ShellToolEnabled, value);
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

void configSetCustomApiEndpoint(APIChatEndpoint value) {
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

void configSetCustomServerProfiles(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomServerProfiles, (ULONG)value);
}

void configSetActiveProfileName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ActiveProfileName, (ULONG)value);
}

void configSetCustomImageHost(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageHost, (ULONG)value);
}

void configSetCustomImagePort(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImagePort, value);
}

void configSetCustomImageUseSSL(ULONG value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageUseSSL, value);
}

void configSetCustomImageAuthorizationType(AuthorizationType value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageAuthorizationType,
            (ULONG)value);
}

void configSetCustomImageApiKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageApiKey, (ULONG)value);
}

void configSetCustomImageModel(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageModel, (ULONG)value);
}

void configSetCustomImageApiEndpointUrl(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageApiEndpointUrl,
            (ULONG)value);
}

void configSetCustomImageHeaders(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageHeaders, (ULONG)value);
}

void configSetCustomImageServerProfiles(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomImageServerProfiles,
            (ULONG)value);
}

void configSetActiveImageProfileName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ActiveImageProfileName,
            (ULONG)value);
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

/* Active server profile request settings (no provider selection) */

#define LOCKED_PROFILE_NAME_OPENAI "OpenAI"
#define LOCKED_PROFILE_NAME_GEMINI "Google Gemini"
#define LOCKED_PROFILE_NAME_GROK "xAI Grok"
#define LOCKED_PROFILE_NAME_ANTHROPIC "Anthropic Claude"

static BOOL isLockedChatProfileName(CONST_STRPTR name) {
    if (name == NULL)
        return FALSE;
    return (strcmp(name, LOCKED_PROFILE_NAME_OPENAI) == 0) ||
           (strcmp(name, LOCKED_PROFILE_NAME_GEMINI) == 0) ||
           (strcmp(name, LOCKED_PROFILE_NAME_GROK) == 0) ||
           (strcmp(name, LOCKED_PROFILE_NAME_ANTHROPIC) == 0);
}

static BOOL isLockedImageProfileName(CONST_STRPTR name) {
    if (name == NULL)
        return FALSE;
    /* Anthropic doesn't support images in this app */
    return (strcmp(name, LOCKED_PROFILE_NAME_OPENAI) == 0) ||
           (strcmp(name, LOCKED_PROFILE_NAME_GEMINI) == 0) ||
           (strcmp(name, LOCKED_PROFILE_NAME_GROK) == 0);
}

static struct json_object *parseProfilesJsonArray(CONST_STRPTR profilesStr) {
    if (profilesStr == NULL || strlen(profilesStr) == 0)
        return NULL;
    struct json_object *arr = json_tokener_parse(profilesStr);
    if (arr == NULL || !json_object_is_type(arr, json_type_array)) {
        if (arr != NULL)
            json_object_put(arr);
        return NULL;
    }
    return arr;
}

static struct json_object *findProfileObjectByName(struct json_object *arr,
                                                   CONST_STRPTR name) {
    if (arr == NULL || !json_object_is_type(arr, json_type_array) ||
        name == NULL)
        return NULL;
    int len = json_object_array_length(arr);
    for (int i = 0; i < len; i++) {
        struct json_object *p = json_object_array_get_idx(arr, i);
        if (p == NULL)
            continue;
        struct json_object *nameObj = json_object_object_get(p, "name");
        if (nameObj == NULL)
            continue;
        CONST_STRPTR n = json_object_get_string(nameObj);
        if (n != NULL && strcmp(n, name) == 0)
            return p;
    }
    return NULL;
}

static CONST_STRPTR jsonGetStringDefault(struct json_object *obj,
                                         CONST_STRPTR key, CONST_STRPTR def) {
    if (obj == NULL || key == NULL)
        return def;
    struct json_object *v = json_object_object_get(obj, key);
    if (v == NULL)
        return def;
    CONST_STRPTR s = json_object_get_string(v);
    return (s != NULL) ? s : def;
}

static LONG jsonGetIntDefault(struct json_object *obj, CONST_STRPTR key,
                              LONG def) {
    if (obj == NULL || key == NULL)
        return def;
    struct json_object *v = json_object_object_get(obj, key);
    if (v == NULL)
        return def;
    return json_object_get_int(v);
}

static BOOL jsonGetBoolDefault(struct json_object *obj, CONST_STRPTR key,
                               BOOL def) {
    if (obj == NULL || key == NULL)
        return def;
    struct json_object *v = json_object_object_get(obj, key);
    if (v == NULL)
        return def;
    return json_object_get_boolean(v) ? TRUE : FALSE;
}

static APIImageEndpoint coerceImageEndpointFromStoredInt(LONG v) {
    /* Accept both new values (0/1) and legacy APIEndpoint values (3/4). */
    if (v == (LONG)API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT || v == 3) {
        return API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT;
    }
    if (v == (LONG)API_IMAGE_ENDPOINT_IMAGES_GENERATIONS || v == 4) {
        return API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
    }
    return API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
}

static APIChatEndpoint coerceChatEndpointFromStoredInt(LONG v) {
    if (v == (LONG)API_CHAT_ENDPOINT_RESPONSES)
        return API_CHAT_ENDPOINT_RESPONSES;
    if (v == (LONG)API_CHAT_ENDPOINT_CHAT_COMPLETIONS)
        return API_CHAT_ENDPOINT_CHAT_COMPLETIONS;
    if (v == (LONG)API_CHAT_ENDPOINT_MESSAGES)
        return API_CHAT_ENDPOINT_MESSAGES;
    if (v == (LONG)API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT)
        return API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT;
    return API_CHAT_ENDPOINT_CHAT_COMPLETIONS;
}

static STRPTR resolvedChatSystem = NULL;

static void fillLockedChatProfileDefaults(struct ChatRequestSettings *out,
                                          CONST_STRPTR profileName) {
    /* Connection defaults */
    if (strcmp(profileName, LOCKED_PROFILE_NAME_OPENAI) == 0) {
        out->host = (STRPTR) "api.openai.com";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_RESPONSES;
        out->apiEndpointUrl = "v1";
        out->authorizationType = AUTHORIZATION_TYPE_BEARER;
        out->customHeaders = NULL;
        out->apiKey = configGetOpenAiApiKey();
        STRPTR model = NULL;
        if (configObj)
            get(configObj, MUIA_AmigaGPTConfig_OpenAiChatModelName, &model);
        out->model =
            (model != NULL && strlen(model) > 0) ? model : "gpt-5-chat-latest";
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_OpenAiChatStreamEnabled, &v);
            out->stream = (BOOL)v;
        }
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_OpenAiWebSearchEnabled, &v);
            out->webSearchEnabled = (BOOL)v;
        }
        {
            ULONG v = FALSE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_OpenAiShellToolEnabled, &v);
            out->shellToolEnabled = (BOOL)v;
        }
        {
            STRPTR s = NULL;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_OpenAiChatSystem, &s);
            freeString(&resolvedChatSystem);
            resolvedChatSystem = copyString(s);
            out->chatSystem = resolvedChatSystem;
        }
    } else if (strcmp(profileName, LOCKED_PROFILE_NAME_GEMINI) == 0) {
        out->host = (STRPTR) "generativelanguage.googleapis.com";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT;
        out->apiEndpointUrl = "v1beta";
        out->authorizationType = AUTHORIZATION_TYPE_X_GOOGLE_API_KEY;
        out->customHeaders = NULL;
        out->apiKey = configGetGeminiApiKey();
        STRPTR model = NULL;
        if (configObj)
            get(configObj, MUIA_AmigaGPTConfig_GeminiChatModelName, &model);
        out->model =
            (model != NULL && strlen(model) > 0) ? model : "gemini-2.5-flash";
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_GeminiChatStreamEnabled, &v);
            out->stream = (BOOL)v;
        }
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_GeminiWebSearchEnabled, &v);
            out->webSearchEnabled = (BOOL)v;
        }
        out->shellToolEnabled = FALSE;
        {
            STRPTR s = NULL;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_GeminiChatSystem, &s);
            freeString(&resolvedChatSystem);
            resolvedChatSystem = copyString(s);
            out->chatSystem = resolvedChatSystem;
        }
    } else if (strcmp(profileName, LOCKED_PROFILE_NAME_GROK) == 0) {
        out->host = (STRPTR) "api.x.ai";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_RESPONSES;
        out->apiEndpointUrl = "v1";
        out->authorizationType = AUTHORIZATION_TYPE_BEARER;
        out->customHeaders = NULL;
        out->apiKey = configGetGrokApiKey();
        STRPTR model = NULL;
        if (configObj)
            get(configObj, MUIA_AmigaGPTConfig_GrokChatModelName, &model);
        out->model = (model != NULL && strlen(model) > 0) ? model : "grok-4";
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_GrokChatStreamEnabled, &v);
            out->stream = (BOOL)v;
        }
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_GrokWebSearchEnabled, &v);
            out->webSearchEnabled = (BOOL)v;
        }
        out->shellToolEnabled = FALSE;
        {
            STRPTR s = NULL;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_GrokChatSystem, &s);
            freeString(&resolvedChatSystem);
            resolvedChatSystem = copyString(s);
            out->chatSystem = resolvedChatSystem;
        }
    } else {
        out->host = (STRPTR) "api.anthropic.com";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_MESSAGES;
        out->apiEndpointUrl = "v1";
        out->authorizationType = AUTHORIZATION_TYPE_X_API_KEY;
        out->customHeaders = "anthropic-version: 2023-06-01";
        out->apiKey = configGetAnthropicApiKey();
        STRPTR model = NULL;
        if (configObj)
            get(configObj, MUIA_AmigaGPTConfig_AnthropicChatModelName, &model);
        out->model = (model != NULL && strlen(model) > 0)
                         ? model
                         : "claude-3-5-sonnet-latest";
        {
            ULONG v = FALSE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_AnthropicChatStreamEnabled,
                    &v);
            out->stream = (BOOL)v;
        }
        {
            ULONG v = TRUE;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_AnthropicWebSearchEnabled,
                    &v);
            out->webSearchEnabled = (BOOL)v;
        }
        out->shellToolEnabled = FALSE;
        {
            STRPTR s = NULL;
            if (configObj)
                get(configObj, MUIA_AmigaGPTConfig_AnthropicChatSystem, &s);
            freeString(&resolvedChatSystem);
            resolvedChatSystem = copyString(s);
            out->chatSystem = resolvedChatSystem;
        }
    }
}

/* NOTE: When resolving custom profiles from JSON, json-c strings are owned by
 * the json_object tree. Since we release that tree before returning, we must
 * copy strings into persistent buffers here. These are overwritten on each
 * call. */
static STRPTR resolvedChatHost = NULL;
static STRPTR resolvedChatApiKey = NULL;
static STRPTR resolvedChatModel = NULL;
static STRPTR resolvedChatApiEndpointUrl = NULL;
static STRPTR resolvedChatCustomHeaders = NULL;

static STRPTR resolvedImageHost = NULL;
static STRPTR resolvedImageApiKey = NULL;
static STRPTR resolvedImageModel = NULL;
static STRPTR resolvedImageApiEndpointUrl = NULL;
static STRPTR resolvedImageCustomHeaders = NULL;

void configGetActiveChatRequestSettings(struct ChatRequestSettings *out) {
    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));

    out->useProxy = configGetProxyEnabled();
    out->proxyHost = configGetProxyHost();
    out->proxyPort = (UWORD)configGetProxyPort();
    out->proxyUsesSSL = configGetProxyUsesSSL();
    out->proxyRequiresAuth = configGetProxyRequiresAuth();
    out->proxyUsername = configGetProxyUsername();
    out->proxyPassword = configGetProxyPassword();

    CONST_STRPTR activeName = configGetActiveProfileName();
    if (activeName == NULL || strlen(activeName) == 0) {
        activeName = LOCKED_PROFILE_NAME_OPENAI;
    }
    out->profileName = activeName;

    if (isLockedChatProfileName(activeName)) {
        fillLockedChatProfileDefaults(out, activeName);
        return;
    }

    struct json_object *arr =
        parseProfilesJsonArray(configGetCustomServerProfiles());
    struct json_object *profile = findProfileObjectByName(arr, activeName);
    if (profile != NULL) {
        freeString(&resolvedChatHost);
        resolvedChatHost =
            copyString(jsonGetStringDefault(profile, "host", ""));
        out->host = resolvedChatHost;
        out->port = (UWORD)jsonGetIntDefault(profile, "port", 443);
        out->useSSL = jsonGetBoolDefault(profile, "useSSL", TRUE);
        out->authorizationType = (AuthorizationType)jsonGetIntDefault(
            profile, "authorizationType", (LONG)AUTHORIZATION_TYPE_BEARER);
        freeString(&resolvedChatApiKey);
        resolvedChatApiKey =
            copyString(jsonGetStringDefault(profile, "apiKey", ""));
        out->apiKey = resolvedChatApiKey;

        freeString(&resolvedChatModel);
        resolvedChatModel = copyString(jsonGetStringDefault(
            profile, "chatModel",
            jsonGetStringDefault(profile, "imageModel", "")));
        out->model = resolvedChatModel;
        out->stream = jsonGetBoolDefault(profile, "streaming", FALSE);
        out->apiEndpoint = coerceChatEndpointFromStoredInt(jsonGetIntDefault(
            profile, "apiEndpoint", (LONG)API_CHAT_ENDPOINT_CHAT_COMPLETIONS));
        freeString(&resolvedChatApiEndpointUrl);
        resolvedChatApiEndpointUrl =
            copyString(jsonGetStringDefault(profile, "apiEndpointUrl", "v1"));
        out->apiEndpointUrl = resolvedChatApiEndpointUrl;

        freeString(&resolvedChatCustomHeaders);
        resolvedChatCustomHeaders =
            copyString(jsonGetStringDefault(profile, "customHeaders", ""));
        out->customHeaders = resolvedChatCustomHeaders;

        out->webSearchEnabled =
            jsonGetBoolDefault(profile, "webSearchEnabled", TRUE);
        out->shellToolEnabled =
            jsonGetBoolDefault(profile, "shellToolEnabled", FALSE);
        freeString(&resolvedChatSystem);
        resolvedChatSystem =
            copyString(jsonGetStringDefault(profile, "chatSystem", ""));
        out->chatSystem = resolvedChatSystem;
    } else {
        /* Fallback to custom settings */
        out->host = configGetCustomHost();
        out->port = (UWORD)configGetCustomPort();
        out->useSSL = configGetCustomUseSSL();
        out->authorizationType = configGetCustomAuthorizationType();
        out->apiKey = configGetCustomApiKey();
        out->model = configGetCustomChatModel();
        out->stream = configGetCustomChatStreamEnabled();
        out->apiEndpoint = configGetCustomApiEndpoint();
        out->apiEndpointUrl = configGetCustomApiEndpointUrl();
        out->customHeaders = configGetCustomHeaders();
        out->webSearchEnabled = TRUE;
        out->shellToolEnabled = FALSE;
        out->chatSystem = "";
    }
    if (arr != NULL)
        json_object_put(arr);
}

void configGetChatRequestSettingsWithStreamOverride(
    struct ChatRequestSettings *out, BOOL streamOverride) {
    configGetActiveChatRequestSettings(out);
    if (out != NULL) {
        out->stream = streamOverride;
    }
}

void configGetActiveImageRequestSettings(struct ImageRequestSettings *out) {
    if (out == NULL)
        return;
    memset(out, 0, sizeof(*out));

    out->useProxy = configGetProxyEnabled();
    out->proxyHost = configGetProxyHost();
    out->proxyPort = (UWORD)configGetProxyPort();
    out->proxyUsesSSL = configGetProxyUsesSSL();
    out->proxyRequiresAuth = configGetProxyRequiresAuth();
    out->proxyUsername = configGetProxyUsername();
    out->proxyPassword = configGetProxyPassword();

    CONST_STRPTR activeName = configGetActiveImageProfileName();
    if (activeName == NULL || strlen(activeName) == 0) {
        activeName = LOCKED_PROFILE_NAME_OPENAI;
    }
    out->profileName = activeName;

    if (isLockedImageProfileName(activeName)) {
        if (strcmp(activeName, LOCKED_PROFILE_NAME_OPENAI) == 0) {
            out->host = (STRPTR) "api.openai.com";
            out->port = 443;
            out->useSSL = TRUE;
            out->apiEndpointUrl = "v1";
            out->authorizationType = AUTHORIZATION_TYPE_BEARER;
            out->customHeaders = NULL;
            out->apiKey = configGetOpenAiApiKey();
            out->model = configGetOpenAiImageModelName();
            out->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
        } else if (strcmp(activeName, LOCKED_PROFILE_NAME_GEMINI) == 0) {
            out->host = (STRPTR) "generativelanguage.googleapis.com";
            out->port = 443;
            out->useSSL = TRUE;
            out->apiEndpointUrl = "v1beta";
            out->authorizationType = AUTHORIZATION_TYPE_X_GOOGLE_API_KEY;
            out->customHeaders = NULL;
            out->apiKey = configGetGeminiApiKey();
            out->model = configGetGeminiImageModelName();
            out->imageApiEndpoint = API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT;
        } else {
            out->host = (STRPTR) "api.x.ai";
            out->port = 443;
            out->useSSL = TRUE;
            out->apiEndpointUrl = "v1";
            out->authorizationType = AUTHORIZATION_TYPE_BEARER;
            out->customHeaders = NULL;
            out->apiKey = configGetGrokApiKey();
            out->model = configGetGrokImageModelName();
            out->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
        }
        return;
    }

    struct json_object *arr =
        parseProfilesJsonArray(configGetCustomImageServerProfiles());
    struct json_object *profile = findProfileObjectByName(arr, activeName);
    if (profile != NULL) {
        freeString(&resolvedImageHost);
        resolvedImageHost =
            copyString(jsonGetStringDefault(profile, "host", ""));
        out->host = resolvedImageHost;
        out->port = (UWORD)jsonGetIntDefault(profile, "port", 443);
        out->useSSL = jsonGetBoolDefault(profile, "useSSL", TRUE);
        out->authorizationType = (AuthorizationType)jsonGetIntDefault(
            profile, "authorizationType", (LONG)AUTHORIZATION_TYPE_BEARER);
        freeString(&resolvedImageApiKey);
        resolvedImageApiKey =
            copyString(jsonGetStringDefault(profile, "apiKey", ""));
        out->apiKey = resolvedImageApiKey;

        freeString(&resolvedImageModel);
        resolvedImageModel = copyString(jsonGetStringDefault(
            profile, "imageModel",
            jsonGetStringDefault(profile, "chatModel", "")));
        out->model = resolvedImageModel;

        freeString(&resolvedImageApiEndpointUrl);
        resolvedImageApiEndpointUrl =
            copyString(jsonGetStringDefault(profile, "apiEndpointUrl", "v1"));
        out->apiEndpointUrl = resolvedImageApiEndpointUrl;

        freeString(&resolvedImageCustomHeaders);
        resolvedImageCustomHeaders =
            copyString(jsonGetStringDefault(profile, "customHeaders", ""));
        out->customHeaders = resolvedImageCustomHeaders;
        out->imageApiEndpoint = coerceImageEndpointFromStoredInt(
            jsonGetIntDefault(profile, "apiEndpoint",
                              (LONG)API_IMAGE_ENDPOINT_IMAGES_GENERATIONS));
    } else {
        out->host = configGetCustomImageHost();
        out->port = (UWORD)configGetCustomImagePort();
        out->useSSL = configGetCustomImageUseSSL();
        out->authorizationType = configGetCustomImageAuthorizationType();
        out->apiKey = configGetCustomImageApiKey();
        out->model = configGetCustomImageModel();
        out->apiEndpointUrl = configGetCustomImageApiEndpointUrl();
        out->customHeaders = configGetCustomImageHeaders();
        out->imageApiEndpoint = configGetImageApiEndpoint();
    }
    if (arr != NULL)
        json_object_put(arr);
}

STRPTR configGetImageModelName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_ImageModelName, &val);
    return val;
}

void configSetImageModelName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_ImageModelName, (ULONG)value);
}

STRPTR configGetOpenAiImageModelName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_OpenAiImageModelName, &val);
    return val;
}

void configSetOpenAiImageModelName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_OpenAiImageModelName, (ULONG)value);
}

STRPTR configGetGeminiImageModelName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_GeminiImageModelName, &val);
    return val;
}

void configSetGeminiImageModelName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_GeminiImageModelName, (ULONG)value);
}

STRPTR configGetGrokImageModelName(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_GrokImageModelName, &val);
    return val;
}

void configSetGrokImageModelName(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_GrokImageModelName, (ULONG)value);
}

BOOL configGetCustomChatStreamEnabled(void) {
    ULONG val = FALSE;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_CustomChatStreamEnabled, &val);
    return (BOOL)val;
}

void configSetCustomChatStreamEnabled(BOOL enabled) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_CustomChatStreamEnabled, enabled);
}

STRPTR configGetGeminiApiKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_GeminiApiKey, &val);
    return val;
}

void configSetGeminiApiKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_GeminiApiKey, (ULONG)value);
}

STRPTR configGetGrokApiKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_GrokApiKey, &val);
    return val;
}

void configSetGrokApiKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_GrokApiKey, (ULONG)value);
}

STRPTR configGetAnthropicApiKey(void) {
    STRPTR val = NULL;
    if (configObj)
        get(configObj, MUIA_AmigaGPTConfig_AnthropicApiKey, &val);
    return val;
}

void configSetAnthropicApiKey(CONST_STRPTR value) {
    if (configObj)
        set(configObj, MUIA_AmigaGPTConfig_AnthropicApiKey, (ULONG)value);
}
