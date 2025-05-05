#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/text.h>
#include <json-c/json.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "AmigaGPT_cat.h"
#include "config.h"

#define DEFAULT_ACCENT "american.accent"

struct Config config = {
    .speechEnabled = FALSE,
    .speechAccent = NULL,
    .speechSystem = SPEECH_SYSTEM_OPENAI,
    .speechFliteVoice = SPEECH_FLITE_VOICE_KAL,
    .speechSystem = SPEECH_SYSTEM_FLITE,
    .chatSystem = NULL,
    .chatModel = GPT_4o,
    .imageModel = DALL_E_3,
    .imageSizeDallE2 = IMAGE_SIZE_256x256,
    .imageSizeDallE3 = IMAGE_SIZE_1024x1024,
    .imageSizeGptImage1 = IMAGE_SIZE_AUTO,
    .openAITTSModel = OPENAI_TTS_MODEL_GPT_4o_MINI_TTS,
    .openAITTSVoice = OPENAI_TTS_VOICE_ALLOY,
    .openAIVoiceInstructions = NULL,
    .openAiApiKey = NULL,
    .chatModelSetVersion = CHAT_MODEL_SET_VERSION,
    .imageModelSetVersion = IMAGE_MODEL_SET_VERSION,
    .speechSystemSetVersion = SPEECH_SYSTEM_SET_VERSION,
    .openAITTSModelSetVersion = OPENAI_TTS_MODEL_SET_VERSION,
    .openAITTSVoiceSetVersion = OPENAI_TTS_VOICE_SET_VERSION,
    .proxyEnabled = FALSE,
    .proxyHost = NULL,
    .proxyPort = 8080,
    .proxyUsesSSL = FALSE,
    .proxyRequiresAuth = FALSE,
    .proxyUsername = NULL,
    .proxyPassword = NULL};

/**
 * Write the config to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG writeConfig() {
    BPTR file = Open(PROGDIR "config.json", MODE_NEWFILE);
    if (file == 0) {
        printf(STRING_ERROR_CONFIG_FILE_READ);
        putchar('\n');
        return RETURN_ERROR;
    }

    struct json_object *configJsonObject = json_object_new_object();

    json_object_object_add(configJsonObject, "speechEnabled",
                           json_object_new_boolean((BOOL)config.speechEnabled));
    json_object_object_add(configJsonObject, "speechSystem",
                           json_object_new_int(config.speechSystem));
    json_object_object_add(configJsonObject, "speechAccent",
                           config.speechAccent != NULL
                               ? json_object_new_string(config.speechAccent)
                               : NULL);
    json_object_object_add(configJsonObject, "speechFliteVoice",
                           json_object_new_int(config.speechFliteVoice));
    json_object_object_add(configJsonObject, "chatSystem",
                           config.chatSystem != NULL
                               ? json_object_new_string(config.chatSystem)
                               : NULL);
    json_object_object_add(configJsonObject, "chatModel",
                           json_object_new_int(config.chatModel));
    json_object_object_add(configJsonObject, "imageModel",
                           json_object_new_int(config.imageModel));
    json_object_object_add(configJsonObject, "imageSizeDallE2",
                           json_object_new_int(config.imageSizeDallE2));
    json_object_object_add(configJsonObject, "imageSizeDallE3",
                           json_object_new_int(config.imageSizeDallE3));
    json_object_object_add(configJsonObject, "imageSizeGptImage1",
                           json_object_new_int(config.imageSizeGptImage1));
    json_object_object_add(configJsonObject, "openAITTSModel",
                           json_object_new_int(config.openAITTSModel));
    json_object_object_add(configJsonObject, "openAITTSVoice",
                           json_object_new_int(config.openAITTSVoice));
    json_object_object_add(configJsonObject, "openAiApiKey",
                           config.openAiApiKey != NULL
                               ? json_object_new_string(config.openAiApiKey)
                               : NULL);
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
    json_object_object_add(
        configJsonObject, "openAIVoiceInstructions",
        config.openAIVoiceInstructions != NULL
            ? json_object_new_string(config.openAIVoiceInstructions)
            : NULL);
    json_object_object_add(configJsonObject, "proxyEnabled",
                           json_object_new_boolean((BOOL)config.proxyEnabled));
    json_object_object_add(configJsonObject, "proxyHost",
                           config.proxyHost != NULL
                               ? json_object_new_string(config.proxyHost)
                               : NULL);
    json_object_object_add(configJsonObject, "proxyPort",
                           json_object_new_int(config.proxyPort));
    json_object_object_add(configJsonObject, "proxyUsesSSL",
                           json_object_new_boolean((BOOL)config.proxyUsesSSL));
    json_object_object_add(
        configJsonObject, "proxyRequiresAuth",
        json_object_new_boolean((BOOL)config.proxyRequiresAuth));
    json_object_object_add(configJsonObject, "proxyUsername",
                           config.proxyUsername != NULL
                               ? json_object_new_string(config.proxyUsername)
                               : NULL);
    json_object_object_add(configJsonObject, "proxyPassword",
                           config.proxyPassword != NULL
                               ? json_object_new_string(config.proxyPassword)
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
 * Read the config from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG readConfig() {
    BPTR file = Open(PROGDIR "config.json", MODE_OLDFILE);
    if (file == 0) {
        // No config exists. Create a new one from defaults
        writeConfig();
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

    struct json_object *speechEnabledObj;
    if (json_object_object_get_ex(configJsonObject, "speechEnabled",
                                  &speechEnabledObj)) {
        config.speechEnabled = (ULONG)json_object_get_boolean(speechEnabledObj);
    }

    struct json_object *speechSystemObj;
    if (json_object_object_get_ex(configJsonObject, "speechSystem",
                                  &speechSystemObj)) {
        config.speechSystem = json_object_get_int(speechSystemObj);
    }

#ifdef __AMIGAOS3__
    if (config.speechSystem == SPEECH_SYSTEM_FLITE) {
        config.speechSystem = SPEECH_SYSTEM_OPENAI;
    }
#else
#ifdef __AMIGAOS4__
    if (config.speechSystem == SPEECH_SYSTEM_34 ||
        config.speechSystem == SPEECH_SYSTEM_37) {
        config.speechSystem = SPEECH_SYSTEM_FLITE;
    }
#else
#ifdef __MORPHOS__
    if (config.speechSystem == SPEECH_SYSTEM_34 ||
        config.speechSystem == SPEECH_SYSTEM_37 ||
        config.speechSystem == SPEECH_SYSTEM_FLITE) {
        config.speechSystem = SPEECH_SYSTEM_OPENAI;
    }
#endif
#endif
#endif

    if (config.speechAccent != NULL) {
        FreeVec(config.speechAccent);
        config.speechAccent = NULL;
    }
    struct json_object *speechAccentObj;
    if (json_object_object_get_ex(configJsonObject, "speechAccent",
                                  &speechAccentObj)) {
        CONST_STRPTR speechAccent = json_object_get_string(speechAccentObj);
        if (speechAccent != NULL) {
            config.speechAccent =
                AllocVec(strlen(speechAccent) + 1, MEMF_CLEAR);
            strncpy(config.speechAccent, speechAccent, strlen(speechAccent));
        }
    }
    if (config.speechAccent == NULL) {
        config.speechAccent = AllocVec(strlen(DEFAULT_ACCENT) + 1, MEMF_CLEAR);
        strncpy(config.speechAccent, DEFAULT_ACCENT, strlen(DEFAULT_ACCENT));
    }

    struct json_object *speechVoiceObj;
    if (json_object_object_get_ex(configJsonObject, "speechFliteVoice",
                                  &speechVoiceObj)) {
        config.speechFliteVoice = json_object_get_int(speechVoiceObj);
    }

    if (config.chatSystem != NULL) {
        FreeVec(config.chatSystem);
        config.chatSystem = NULL;
    }
    struct json_object *chatSystemObj;
    if (json_object_object_get_ex(configJsonObject, "chatSystem",
                                  &chatSystemObj)) {
        CONST_STRPTR chatSystem = json_object_get_string(chatSystemObj);
        if (chatSystem != NULL) {
            config.chatSystem = AllocVec(strlen(chatSystem) + 1, MEMF_CLEAR);
            strncpy(config.chatSystem, chatSystem, strlen(chatSystem));
        }
    }

    struct json_object *chatModelObj;
    if (json_object_object_get_ex(configJsonObject, "chatModel",
                                  &chatModelObj) ||
        json_object_object_get_ex(configJsonObject, "model", &chatModelObj)) {
        config.chatModel = json_object_get_int(chatModelObj);
    }

    struct json_object *imageModelObj;
    if (json_object_object_get_ex(configJsonObject, "imageModel",
                                  &imageModelObj)) {
        config.imageModel = json_object_get_int(imageModelObj);
    }

    struct json_object *imageSizeDallE2Obj;
    if (json_object_object_get_ex(configJsonObject, "imageSizeDallE2",
                                  &imageSizeDallE2Obj)) {
        config.imageSizeDallE2 = json_object_get_int(imageSizeDallE2Obj);
    }

    struct json_object *imageSizeDallE3Obj;
    if (json_object_object_get_ex(configJsonObject, "imageSizeDallE3",
                                  &imageSizeDallE3Obj)) {
        config.imageSizeDallE3 = json_object_get_int(imageSizeDallE3Obj);
    }

    struct json_object *imageSizeGptImage1Obj;
    if (json_object_object_get_ex(configJsonObject, "imageSizeGptImage1",
                                  &imageSizeGptImage1Obj)) {
        config.imageSizeGptImage1 = json_object_get_int(imageSizeGptImage1Obj);
    }

    struct json_object *openAITTSModelObj;
    if (json_object_object_get_ex(configJsonObject, "openAITTSModel",
                                  &openAITTSModelObj)) {
        config.openAITTSModel = json_object_get_int(openAITTSModelObj);
    }

    struct json_object *openAITTSVoiceObj;
    if (json_object_object_get_ex(configJsonObject, "openAITTSVoice",
                                  &openAITTSVoiceObj)) {
        config.openAITTSVoice = json_object_get_int(openAITTSVoiceObj);
    }

    if (config.openAIVoiceInstructions != NULL) {
        FreeVec(config.openAIVoiceInstructions);
        config.openAIVoiceInstructions = NULL;
    }
    struct json_object *openAIVoiceInstructionsObj;
    if (json_object_object_get_ex(configJsonObject, "openAIVoiceInstructions",
                                  &openAIVoiceInstructionsObj)) {
        CONST_STRPTR openAIVoiceInstructions =
            json_object_get_string(openAIVoiceInstructionsObj);
        if (openAIVoiceInstructions != NULL) {
            config.openAIVoiceInstructions =
                AllocVec(strlen(openAIVoiceInstructions) + 1, MEMF_CLEAR);
            strncpy(config.openAIVoiceInstructions, openAIVoiceInstructions,
                    strlen(openAIVoiceInstructions));
        }
    }

    if (config.openAiApiKey != NULL) {
        FreeVec(config.openAiApiKey);
        config.openAiApiKey = NULL;
    }
    struct json_object *openAiApiKeyObj;
    if (json_object_object_get_ex(configJsonObject, "openAiApiKey",
                                  &openAiApiKeyObj)) {
        CONST_STRPTR openAiApiKey = json_object_get_string(openAiApiKeyObj);
        if (openAiApiKey != NULL) {
            config.openAiApiKey =
                AllocVec(strlen(openAiApiKey) + 1, MEMF_CLEAR);
            strncpy(config.openAiApiKey, openAiApiKey, strlen(openAiApiKey));
        }
    }

    struct json_object *chatModelSetVersionObj;
    if (json_object_object_get_ex(configJsonObject, "chatModelSetVersion",
                                  &chatModelSetVersionObj)) {
        config.chatModelSetVersion =
            json_object_get_int(chatModelSetVersionObj);
    } else {
        config.chatModelSetVersion = 0;
    }

    if (config.chatModelSetVersion != CHAT_MODEL_SET_VERSION) {
        config.chatModel = GPT_4o;
    }

    struct json_object *imageModelSetVersionObj;
    if (json_object_object_get_ex(configJsonObject, "imageModelSetVersion",
                                  &imageModelSetVersionObj)) {
        config.imageModelSetVersion =
            json_object_get_int(imageModelSetVersionObj);
    } else {
        config.imageModelSetVersion = 0;
    }

    if (config.imageModelSetVersion != IMAGE_MODEL_SET_VERSION) {
        config.imageModel = DALL_E_3;
    }

    struct json_object *speechSystemSetVersionObj;
    if (json_object_object_get_ex(configJsonObject, "speechSystemSetVersion",
                                  &speechSystemSetVersionObj)) {
        config.speechSystemSetVersion =
            json_object_get_int(speechSystemSetVersionObj);
    } else {
        config.speechSystemSetVersion = 0;
    }

    if (config.speechSystemSetVersion != SPEECH_SYSTEM_SET_VERSION) {
#ifdef __AMIGAOS3__
        config.speechSystem = SPEECH_SYSTEM_34;
#else
#ifdef __AMIGAOS4__
        config.speechSystem = SPEECH_SYSTEM_FLITE;
#else
        config.speechSystem = SPEECH_SYSTEM_OPENAI;
#endif
#endif
    }

    struct json_object *openAITTSModelSetVersionObj;
    if (json_object_object_get_ex(configJsonObject, "openAITTSModelSetVersion",
                                  &openAITTSModelSetVersionObj)) {
        config.openAITTSModelSetVersion =
            json_object_get_int(openAITTSModelSetVersionObj);
    } else {
        config.openAITTSModelSetVersion = 0;
    }

    if (config.openAITTSModelSetVersion != OPENAI_TTS_MODEL_SET_VERSION) {
        config.openAITTSModel = OPENAI_TTS_MODEL_GPT_4o_MINI_TTS;
    }

    struct json_object *openAITTSVoiceSetVersionObj;
    if (json_object_object_get_ex(configJsonObject, "openAITTSVoiceSetVersion",
                                  &openAITTSVoiceSetVersionObj)) {
        config.openAITTSVoiceSetVersion =
            json_object_get_int(openAITTSVoiceSetVersionObj);
    } else {
        config.openAITTSVoiceSetVersion = 0;
    }

    if (config.openAITTSVoiceSetVersion != OPENAI_TTS_VOICE_SET_VERSION) {
        config.openAITTSVoice = OPENAI_TTS_VOICE_ALLOY;
    }

    struct json_object *proxyEnabledObj;
    if (json_object_object_get_ex(configJsonObject, "proxyEnabled",
                                  &proxyEnabledObj)) {
        config.proxyEnabled = (ULONG)json_object_get_boolean(proxyEnabledObj);
    } else {
        config.proxyEnabled = FALSE;
    }

    if (config.proxyHost != NULL) {
        FreeVec(config.proxyHost);
        config.proxyHost = NULL;
    }

    struct json_object *proxyHostObj;
    if (json_object_object_get_ex(configJsonObject, "proxyHost",
                                  &proxyHostObj)) {
        CONST_STRPTR proxyHost = json_object_get_string(proxyHostObj);
        if (proxyHost != NULL) {
            config.proxyHost = AllocVec(strlen(proxyHost) + 1, MEMF_CLEAR);
            strncpy(config.proxyHost, proxyHost, strlen(proxyHost));
        }
    }

    struct json_object *proxyPortObj;
    if (json_object_object_get_ex(configJsonObject, "proxyPort",
                                  &proxyPortObj)) {
        config.proxyPort = json_object_get_int(proxyPortObj);
    } else {
        config.proxyPort = 8080;
    }

    struct json_object *proxyUsesSSLObj;
    if (json_object_object_get_ex(configJsonObject, "proxyUsesSSL",
                                  &proxyUsesSSLObj)) {
        config.proxyUsesSSL = (ULONG)json_object_get_boolean(proxyUsesSSLObj);
    } else {
        config.proxyUsesSSL = FALSE;
    }

    struct json_object *proxyRequiresAuthObj;
    if (json_object_object_get_ex(configJsonObject, "proxyRequiresAuth",
                                  &proxyRequiresAuthObj)) {
        config.proxyRequiresAuth =
            (ULONG)json_object_get_boolean(proxyRequiresAuthObj);
    } else {
        config.proxyRequiresAuth = FALSE;
    }

    if (config.proxyUsername != NULL) {
        FreeVec(config.proxyUsername);
        config.proxyUsername = NULL;
    }

    struct json_object *proxyUsernameObj;
    if (json_object_object_get_ex(configJsonObject, "proxyUsername",
                                  &proxyUsernameObj)) {
        CONST_STRPTR proxyUsername = json_object_get_string(proxyUsernameObj);
        if (proxyUsername != NULL) {
            config.proxyUsername =
                AllocVec(strlen(proxyUsername) + 1, MEMF_CLEAR);
            strncpy(config.proxyUsername, proxyUsername, strlen(proxyUsername));
        }
    }

    if (config.proxyPassword != NULL) {
        FreeVec(config.proxyPassword);
        config.proxyPassword = NULL;
    }

    struct json_object *proxyPasswordObj;
    if (json_object_object_get_ex(configJsonObject, "proxyPassword",
                                  &proxyPasswordObj)) {
        CONST_STRPTR proxyPassword = json_object_get_string(proxyPasswordObj);
        if (proxyPassword != NULL) {
            config.proxyPassword =
                AllocVec(strlen(proxyPassword) + 1, MEMF_CLEAR);
            strncpy(config.proxyPassword, proxyPassword, strlen(proxyPassword));
        }
    }

    FreeVec(configJsonString);
    json_object_put(configJsonObject);
    return RETURN_OK;
}

/**
 * Free the config
 **/
void freeConfig() {
    if (config.speechAccent != NULL) {
        FreeVec(config.speechAccent);
        config.speechAccent = NULL;
    }
    if (config.chatSystem != NULL) {
        FreeVec(config.chatSystem);
        config.chatSystem = NULL;
    }
    if (config.openAIVoiceInstructions != NULL) {
        FreeVec(config.openAIVoiceInstructions);
        config.openAIVoiceInstructions = NULL;
    }
    if (config.openAiApiKey != NULL) {
        FreeVec(config.openAiApiKey);
        config.openAiApiKey = NULL;
    }
    if (config.proxyHost != NULL) {
        FreeVec(config.proxyHost);
        config.proxyHost = NULL;
    }
    if (config.proxyUsername != NULL) {
        FreeVec(config.proxyUsername);
        config.proxyUsername = NULL;
    }
    if (config.proxyPassword != NULL) {
        FreeVec(config.proxyPassword);
        config.proxyPassword = NULL;
    }
}