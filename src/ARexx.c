#include <json-c/json.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "ARexx.h"
#include "gui.h"
#include "config.h"
#include "MainWindow.h"

HOOKPROTONH(ReplyCallbackFunc, APTR, Object *obj, struct RexxMsg *rxm) {
    printf("Args[0]: %s\nResult1: %ld   Result2: %ld\n", rxm->rm_Args[0],
           rxm->rm_Result1, rxm->rm_Result2);
}
MakeHook(ReplyCallbackHook, ReplyCallbackFunc);

HOOKPROTONHNO(SendMessageFunc, APTR, ULONG *arg) {
    STRPTR model = (STRPTR)arg[0];
    STRPTR system = (STRPTR)arg[1];
    STRPTR host = (STRPTR)arg[2];
    ULONG *port = (ULONG *)arg[3];
    BOOL useSSL = (BOOL)arg[4];
    STRPTR apiKey = (STRPTR)arg[5];
    BOOL useProxy = (BOOL)arg[6];
    STRPTR proxyHost = (STRPTR)arg[7];
    ULONG *proxyPort = (ULONG *)arg[8];
    BOOL proxyUsesSSL = (BOOL)arg[9];
    BOOL proxyRequiresAuth = (BOOL)arg[10];
    STRPTR proxyUsername = (STRPTR)arg[11];
    STRPTR proxyPassword = (STRPTR)arg[12];
    BOOL webSearchEnabled = (BOOL)arg[13];
    STRPTR prompt = (STRPTR)arg[14];

    ULONG portValue = port == NULL
                          ? (config.useCustomServer ? config.customPort : NULL)
                          : (ULONG)*port;
    ULONG proxyPortValue =
        proxyPort == NULL ? config.proxyPort : (ULONG)*proxyPort;

    if (apiKey == NULL) {
        apiKey =
            config.useCustomServer ? config.customApiKey : config.openAiApiKey;
    }

    if (host == NULL || strlen(host) == 0) {
        host = config.useCustomServer ? config.customHost : NULL;
    }

    if (!useSSL) {
        useSSL = config.useCustomServer ? config.customUseSSL : TRUE;
    }

    if (!proxyUsesSSL) {
        proxyUsesSSL = config.proxyUsesSSL;
    }

    if (!proxyRequiresAuth) {
        proxyRequiresAuth = config.proxyRequiresAuth;
    }

    if (proxyUsername == NULL || strlen(proxyUsername) == 0) {
        proxyUsername = config.proxyUsername;
    }

    if (proxyPassword == NULL || strlen(proxyPassword) == 0) {
        proxyPassword = config.proxyPassword;
    }

    if (model == NULL || strlen(model) == 0) {
        model = config.useCustomServer ? config.customChatModel
                                       : CHAT_MODEL_NAMES[config.chatModel];
    }

    struct Conversation *conversation = newConversation();
    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);
    addTextToConversation(conversation, promptUTF8, "user");
    CodesetsFreeA(promptUTF8, NULL);

    setConversationSystem(conversation, system);

    struct json_object **responses = postChatMessageToOpenAI(
        conversation, host, portValue, useSSL, model, apiKey, FALSE, useProxy,
        proxyHost, proxyPortValue, proxyUsesSSL, proxyRequiresAuth,
        proxyUsername, proxyPassword, webSearchEnabled);

    freeConversation(conversation);

    if (responses == NULL) {
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_OK;
    }

    struct json_object *response = responses[0];
    if (response == NULL) {
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        FreeVec(responses);
        return RETURN_OK;
    }

    struct json_object *error;
    if (json_object_object_get_ex(response, "error", &error) &&
        !json_object_is_type(error, json_type_null)) {
        struct json_object *message = json_object_object_get(error, "message");
        UTF8 *messageString = json_object_get_string(message);
        STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)messageString,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        set(app, MUIA_Application_RexxString, formattedMessageSystemEncoded);
        CodesetsFreeA(formattedMessageSystemEncoded, NULL);
        json_object_put(response);
        FreeVec(responses);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_OK;
    } else {
        UTF8 *contentString = getMessageContentFromJson(response, FALSE, TRUE);

        if (!contentString) {
            updateStatusBar(STRING_ERROR, redPen);
            set(app, MUIA_Application_RexxString,
                STRING_ERROR_CONNECTING_OPENAI);
            json_object_put(response);
            FreeVec(responses);
            return RETURN_OK;
        }

        if (strlen(contentString) == 0) {
            set(app, MUIA_Application_RexxString,
                STRING_ERROR_CONNECTING_OPENAI);
            updateStatusBar(STRING_ERROR, redPen);
            json_object_put(response);
            FreeVec(responses);
            return RETURN_OK;
        }

        STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)contentString,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        set(app, MUIA_Application_RexxString, formattedMessageSystemEncoded);
        CodesetsFreeA(formattedMessageSystemEncoded, NULL);
        json_object_put(response);
        FreeVec(responses);
        updateStatusBar(STRING_READY, greenPen);
        return RETURN_OK;
    }
}
MakeHook(SendMessageHook, SendMessageFunc);

HOOKPROTONHNO(CreateImageFunc, APTR, ULONG *arg) {
    STRPTR modelString = (STRPTR)arg[0];
    STRPTR sizeString = (STRPTR)arg[1];
    STRPTR apiKey = (STRPTR)arg[2];
    STRPTR destination = (STRPTR)arg[3];
    STRPTR prompt = (STRPTR)arg[4];

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = config.openAiApiKey;
    }

    ImageModel model;
    if (modelString == NULL || strlen(modelString) == 0) {
        model = GPT_IMAGE_1;
    } else {
        for (UBYTE i = 0; IMAGE_MODEL_NAMES[i] != NULL; i++) {
            if (IMAGE_MODEL_NAMES[i + 1] == NULL) {
                return (RETURN_ERROR);
            }
            if (strcmp(modelString, IMAGE_MODEL_NAMES[i]) == 0) {
                model = i;
                break;
            }
        }
    }
    ImageSize size = IMAGE_SIZE_1024x1024;
    if (sizeString != NULL && strlen(sizeString) > 0) {
        // Set a default size in case no match is found
        switch (model) {
        case DALL_E_2:
            for (UBYTE i = 0; IMAGE_SIZES_DALL_E_2[i] != IMAGE_SIZE_NULL; i++) {
                ImageSize currentSize = IMAGE_SIZES_DALL_E_2[i];
                if (strcmp(sizeString, IMAGE_SIZE_NAMES[currentSize]) == 0) {
                    size = currentSize;
                    break;
                }
            }
            break;
        case DALL_E_3:
            for (UBYTE i = 0; IMAGE_SIZES_DALL_E_3[i] != IMAGE_SIZE_NULL; i++) {
                ImageSize currentSize = IMAGE_SIZES_DALL_E_3[i];
                if (strcmp(sizeString, IMAGE_SIZE_NAMES[currentSize]) == 0) {
                    size = currentSize;
                    break;
                }
            }
            break;
        default:
            for (UBYTE i = 0; IMAGE_SIZES_GPT_IMAGE_1[i] != IMAGE_SIZE_NULL;
                 i++) {
                ImageSize currentSize = IMAGE_SIZES_GPT_IMAGE_1[i];
                if (strcmp(sizeString, IMAGE_SIZE_NAMES[currentSize]) == 0) {
                    size = currentSize;
                    break;
                }
            }
            break;
        }
    }

    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);

    struct json_object *response =
        postImageCreationRequestToOpenAI(promptUTF8, model, size, apiKey, FALSE,
                                         NULL, 0, FALSE, FALSE, NULL, NULL);
    CodesetsFreeA(promptUTF8, NULL);

    if (response == NULL) {
        printf(STRING_ERROR_CONNECTION);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_ERROR;
    }

    struct json_object *error;
    if (json_object_object_get_ex(response, "error", &error)) {
        json_object_put(response);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_ERROR;
    }

    struct array_list *data =
        json_object_get_array(json_object_object_get(response, "data"));
    struct json_object *dataObject = (struct json_object *)data->array[0];

    STRPTR b64 =
        json_object_get_string(json_object_object_get(dataObject, "b64_json"));

    LONG data_len;
    UBYTE *imageData = decodeBase64(b64, &data_len);

    if (destination == NULL) {
        // Generate unique ID for the image
        UBYTE fullPath[30] = "";
        UBYTE id[11] = "";
        CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
        srand(time(NULL));
        for (UBYTE i = 0; i < 9; i++) {
            id[i] = idChars[rand() % strlen(idChars)];
        }
        snprintf(fullPath, sizeof(fullPath), "T:%s.png", id);
        destination = fullPath;
    }

    FILE *file = fopen(destination, "wb");
    fwrite(imageData, 1, data_len, file);
    fclose(file);
    FreeVec(imageData);

    json_object_put(response);
    updateStatusBar(STRING_READY, greenPen);
    set(app, MUIA_Application_RexxString, destination);
    return RETURN_OK;
}
MakeHook(CreateImageHook, CreateImageFunc);

HOOKPROTONHNO(ListChatModelsFunc, APTR, ULONG *arg) {
    STRPTR models = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; CHAT_MODEL_NAMES[i] != NULL; i++) {
        strncat(models, CHAT_MODEL_NAMES[i], 1024);
        strncat(models, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, models);
    FreeVec(models);
    return RETURN_OK;
}
MakeHook(ListChatModelsHook, ListChatModelsFunc);

HOOKPROTONHNO(ListImageModelsFunc, APTR, ULONG *arg) {
    STRPTR models = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; IMAGE_MODEL_NAMES[i] != NULL; i++) {
        strncat(models, IMAGE_MODEL_NAMES[i], 1024);
        strncat(models, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, models);
    FreeVec(models);
    return RETURN_OK;
}
MakeHook(ListImageModelsHook, ListImageModelsFunc);

HOOKPROTONHNO(ListImageSizesFunc, APTR, ULONG *arg) {
    STRPTR sizes = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    strncat(sizes, "DALL-E 2:\n", 1024);
    for (UBYTE i = 0; i < 3; i++) {
        strncat(sizes, IMAGE_SIZE_NAMES[IMAGE_SIZES_DALL_E_2[i]], 1024);
        strncat(sizes, "\n", 1024);
    }
    strncat(sizes, "DALL-E 3:\n", 1024);
    for (UBYTE i = 0; i < 3; i++) {
        strncat(sizes, IMAGE_SIZE_NAMES[IMAGE_SIZES_DALL_E_3[i]], 1024);
        strncat(sizes, "\n", 1024);
    }
    strncat(sizes, "GPT Image 1:\n", 1024);
    for (UBYTE i = 0; i < 3; i++) {
        strncat(sizes, IMAGE_SIZE_NAMES[IMAGE_SIZES_GPT_IMAGE_1[i]], 1024);
        strncat(sizes, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, sizes);
    FreeVec(sizes);
    return RETURN_OK;
}
MakeHook(ListImageSizesHook, ListImageSizesFunc);

HOOKPROTONHNO(ListVoiceModelsFunc, APTR, ULONG *arg) {
    STRPTR models = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++) {
        strncat(models, OPENAI_TTS_MODEL_NAMES[i], 1024);
        strncat(models, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, models);
    FreeVec(models);
    return RETURN_OK;
}
MakeHook(ListVoiceModelsHook, ListVoiceModelsFunc);

HOOKPROTONHNO(ListVoicesFunc, APTR, ULONG *arg) {
    STRPTR voices = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; OPENAI_TTS_VOICE_NAMES[i] != NULL; i++) {
        strncat(voices, OPENAI_TTS_VOICE_NAMES[i], 1024);
        strncat(voices, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, voices);
    FreeVec(voices);
    return RETURN_OK;
}
MakeHook(ListVoicesHook, ListVoicesFunc);

HOOKPROTONHNO(ListAudioFormatsFunc, APTR, ULONG *arg) {
    STRPTR formats = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; AUDIO_FORMAT_NAMES[i] != NULL; i++) {
        strncat(formats, AUDIO_FORMAT_NAMES[i], 1024);
        strncat(formats, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, formats);
    FreeVec(formats);
    return RETURN_OK;
}
MakeHook(ListAudioFormatsHook, ListAudioFormatsFunc);

HOOKPROTONHNO(ListServerModelsFunc, APTR, ULONG *arg) {
    STRPTR host = (STRPTR)arg[0];
    LONG *port = (LONG *)arg[1];
    BOOL useSSL = (BOOL)arg[2];
    STRPTR apiKey = (STRPTR)arg[3];
    BOOL useProxy = (BOOL)arg[4];
    STRPTR proxyHost = (STRPTR)arg[5];
    LONG *proxyPort = (LONG *)arg[6];
    BOOL proxyUsesSSL = (BOOL)arg[7];
    BOOL proxyRequiresAuth = (BOOL)arg[8];
    STRPTR proxyUsername = (STRPTR)arg[9];
    STRPTR proxyPassword = (STRPTR)arg[10];

    ULONG portValue = port == NULL ? (useSSL ? 443 : 80) : (ULONG)*port;
    ULONG proxyPortValue = proxyPort == NULL ? 8080 : (ULONG)*proxyPort;

    if (apiKey == NULL) {
        apiKey = config.openAiApiKey;
    }

    struct json_object *models = getChatModels(
        host, portValue, useSSL, apiKey, useProxy, proxyHost, proxyPortValue,
        proxyUsesSSL, proxyRequiresAuth, proxyUsername, proxyPassword);

    if (models == NULL) {
        return RETURN_ERROR;
    }

    STRPTR modelsString = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; i < json_object_array_length(models); i++) {
        struct json_object *model = json_object_array_get_idx(models, i);
        strncat(modelsString, json_object_get_string(model), 1024);
        strncat(modelsString, "\n", 1024);
    }

    set(app, MUIA_Application_RexxString, modelsString);
    FreeVec(modelsString);
    json_object_put(models);
    return RETURN_OK;
}
MakeHook(ListServerModelsHook, ListServerModelsFunc);

HOOKPROTONHNO(SpeakTextFunc, APTR, ULONG *arg) {
    STRPTR modelString = (STRPTR)arg[0];
    STRPTR voiceString = (STRPTR)arg[1];
    STRPTR instructions = (STRPTR)arg[2];
    STRPTR apiKey = (STRPTR)arg[3];
    STRPTR output = (STRPTR)arg[4];
    STRPTR audioFormatString = (STRPTR)arg[5];
    STRPTR prompt = (STRPTR)arg[6];

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = config.openAiApiKey;
    }
    OpenAITTSModel model;
    if (modelString == NULL || strlen(modelString) == 0) {
        model = OPENAI_TTS_MODEL_GPT_4o_MINI_TTS;
    } else {
        for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++) {
            if (strcasecmp(modelString, OPENAI_TTS_MODEL_NAMES[i]) == 0) {
                model = i;
                break;
            }
        }
    }
    OpenAITTSVoice voice;
    if (voiceString == NULL || strlen(voiceString) == 0) {
        voice = OPENAI_TTS_VOICE_ALLOY;
    } else {
        for (UBYTE i = 0; OPENAI_TTS_VOICE_NAMES[i] != NULL; i++) {
            if (strcasecmp(voiceString, OPENAI_TTS_VOICE_NAMES[i]) == 0) {
                voice = i;
                break;
            }
        }
    }
    AudioFormat audioFormat;
    if (audioFormatString == NULL || strlen(audioFormatString) == 0) {
        audioFormat = AUDIO_FORMAT_MP3;
    } else {
        for (UBYTE i = 0; AUDIO_FORMAT_NAMES[i] != NULL; i++) {
            printf("AUDIO_FORMAT_NAMES[%d]: %s\n", i, AUDIO_FORMAT_NAMES[i]);
            if (strcasecmp(audioFormatString, AUDIO_FORMAT_NAMES[i]) == 0) {
                audioFormat = i;
                break;
            }
        }
    }

    writeConfig();
    config.openAiApiKey = apiKey;
    config.speechSystem = SPEECH_SYSTEM_OPENAI;
    config.openAITTSModel = model;
    config.openAITTSVoice = voice;
    config.openAIVoiceInstructions = instructions;
    speakText(prompt, output, &audioFormat);
    readConfig();
    set(app, MUIA_Application_RexxString, prompt);
    updateStatusBar(STRING_READY, greenPen);
    return RETURN_OK;
}
MakeHook(SpeakTextHook, SpeakTextFunc);

HOOKPROTONHNO(HelpFunc, APTR, ULONG *arg) {
    set(app, MUIA_Application_RexxString,
        "SENDMESSAGE "
        "M=MODEL/K,S=SYSTEM/K,H=HOST/K,P=PORT/N,S=SSL/S,K=APIKEY/K,U=USEPROXY/"
        "S,PH=PROXYHOST/"
        "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
        "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K,W=WEBSEARCH/S,P=PROMPT/F\n"
        "CREATEIMAGE M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F\n"
        "SPEAKTEXT "
        "M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,O=OUTPUT/K,P=PROMPT/"
        "F\n"
        "LISTSERVERMODELS "
        "H=HOST/K,P=PORT/N,S=SSL/S,K=APIKEY/K,U=USEPROXY/S,PH=PROXYHOST/"
        "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
        "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K\n"
        "LISTAUDIOFORMATS\n"
        "LISTCHATMODELS\n"
        "LISTIMAGEMODELS\n"
        "LISTIMAGESIZES\n"
        "LISTVOICEMODELS\n"
        "LISTVOICES\n");
    return RETURN_OK;
}
MakeHook(HelpHook, HelpFunc);

struct MUI_Command arexxList[] = {
    {"SENDMESSAGE",
     "M=MODEL/K,S=SYSTEM/K,H=HOST/K,PO=PORT/N,S=SSL/S,K=APIKEY/K,U=USEPROXY/"
     "S,PH=PROXYHOST/"
     "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
     "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K,W=WEBSEARCH/S,P=PROMPT/F",
     15,
     &SendMessageHook,
     {0, 0, 0, 0, 0}},
    {"CREATEIMAGE",
     "M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F",
     5,
     &CreateImageHook,
     {0, 0, 0, 0, 0}},
    {"SPEAKTEXT",
     "M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,O=OUTPUT/K,F=FORMAT/"
     "K,P=PROMPT/F",
     7,
     &SpeakTextHook,
     {0, 0, 0, 0, 0}},
    {"LISTAUDIOFORMATS", NULL, NULL, &ListAudioFormatsHook, {0, 0, 0, 0, 0}},
    {"LISTSERVERMODELS",
     "H=HOST/K,PO=PORT/N,S=SSL/S,K=APIKEY/K,U=USEPROXY/S,PH=PROXYHOST/K,"
     "PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/S,"
     "PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K",
     11,
     &ListServerModelsHook,
     {0, 0, 0, 0, 0}},
    {"LISTCHATMODELS", NULL, NULL, &ListChatModelsHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGEMODELS", NULL, NULL, &ListImageModelsHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGESIZES", NULL, NULL, &ListImageSizesHook, {0, 0, 0, 0, 0}},
    {"LISTVOICEMODELS", NULL, NULL, &ListVoiceModelsHook, {0, 0, 0, 0, 0}},
    {"LISTVOICES", NULL, NULL, &ListVoicesHook, {0, 0, 0, 0, 0}},
    {"?", NULL, NULL, &HelpHook, {0, 0, 0, 0, 0}},
    {NULL, NULL, 0, NULL, {0, 0, 0, 0, 0}}};
