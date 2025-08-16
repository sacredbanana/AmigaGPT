#include <json-c/json.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
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
    STRPTR modelString = (STRPTR)arg[0];
    STRPTR system = (STRPTR)arg[1];
    STRPTR apiKey = (STRPTR)arg[2];
    STRPTR prompt = (STRPTR)arg[3];

    if (apiKey == NULL) {
        apiKey = config.openAiApiKey;
    }
    enum ChatModel model;
    if (modelString == NULL) {
        model = config.chatModel;
    } else {
        for (UBYTE i = 0; CHAT_MODEL_NAMES[i] != NULL; i++) {
            if (CHAT_MODEL_NAMES[i + 1] == NULL) {
                return (RETURN_ERROR);
            }
            if (strcmp(modelString, CHAT_MODEL_NAMES[i]) == 0) {
                model = i;
                break;
            }
        }
    }
    struct Conversation *conversation = newConversation();
    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);
    addTextToConversation(conversation, promptUTF8, "user");
    CodesetsFreeA(promptUTF8, NULL);

    setConversationSystem(conversation, system);

    struct json_object **responses =
        postChatMessageToOpenAI(conversation, model, apiKey, FALSE, FALSE, NULL,
                                0, FALSE, FALSE, NULL, NULL);

    if (responses == NULL) {
        printf(STRING_ERROR_CONNECTING_OPENAI);
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        freeConversation(conversation);
        updateStatusBar(STRING_ERROR, redPen);
        return (RETURN_ERROR);
    }
    struct json_object *response = responses[0];
    UTF8 *contentString = getMessageContentFromJson(response, FALSE);

    if (contentString != NULL) {
        if (strlen(contentString) > 0) {
            STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                (Tag)contentString, CSA_MapForeignChars, TRUE, TAG_DONE);
            set(app, MUIA_Application_RexxString,
                formattedMessageSystemEncoded);
            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
        }
        json_object_put(response);
        FreeVec(responses);
        freeConversation(conversation);
        updateStatusBar(STRING_READY, greenPen);
        return (RETURN_OK);
    }
    FreeVec(responses);
    freeConversation(conversation);
    updateStatusBar(STRING_ERROR, redPen);
    return (RETURN_ERROR);
}
MakeHook(SendMessageHook, SendMessageFunc);

HOOKPROTONHNO(CreateImageFunc, APTR, ULONG *arg) {
    STRPTR modelString = (STRPTR)arg[0];
    STRPTR sizeString = (STRPTR)arg[1];
    STRPTR apiKey = (STRPTR)arg[2];
    STRPTR destination = (STRPTR)arg[3];
    STRPTR prompt = (STRPTR)arg[4];

    if (apiKey == NULL) {
        apiKey = config.openAiApiKey;
    }

    enum ImageModel model;
    if (modelString == NULL) {
        model = config.imageModel;
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
    enum ImageSize size;
    if (sizeString == NULL) {
        switch (model) {
        case DALL_E_2:
            size = config.imageSizeDallE2;
            break;
        case DALL_E_3:
            size = config.imageSizeDallE3;
            break;
        default:
            size = config.imageSizeGptImage1;
            break;
        }
    } else {
        // Set a default size in case no match is found
        switch (model) {
        case DALL_E_2:
            size = IMAGE_SIZES_DALL_E_2[0];
            for (UBYTE i = 0; IMAGE_SIZES_DALL_E_2[i] != IMAGE_SIZE_NULL; i++) {
                enum ImageSize currentSize = IMAGE_SIZES_DALL_E_2[i];
                if (strcmp(sizeString, IMAGE_SIZE_NAMES[currentSize]) == 0) {
                    size = currentSize;
                    break;
                }
            }
            break;
        case DALL_E_3:
            size = IMAGE_SIZES_DALL_E_3[0];
            for (UBYTE i = 0; IMAGE_SIZES_DALL_E_3[i] != IMAGE_SIZE_NULL; i++) {
                enum ImageSize currentSize = IMAGE_SIZES_DALL_E_3[i];
                if (strcmp(sizeString, IMAGE_SIZE_NAMES[currentSize]) == 0) {
                    size = currentSize;
                    break;
                }
            }
            break;
        default:
            size = IMAGE_SIZE_AUTO;
            for (UBYTE i = 0; IMAGE_SIZES_GPT_IMAGE_1[i] != IMAGE_SIZE_NULL;
                 i++) {
                enum ImageSize currentSize = IMAGE_SIZES_GPT_IMAGE_1[i];
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
        return (RETURN_ERROR);
    }

    struct json_object *error;
    if (json_object_object_get_ex(response, "error", &error)) {
        json_object_put(response);
        updateStatusBar(STRING_ERROR, redPen);
        return (RETURN_ERROR);
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
    return (RETURN_OK);
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
    return (RETURN_OK);
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
    return (RETURN_OK);
}
MakeHook(ListImageModelsHook, ListImageModelsFunc);

HOOKPROTONHNO(ListImageSizesFunc, APTR, ULONG *arg) {
    STRPTR sizes = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
    strncat(sizes, "DALL-E 2\n", 1024);
    for (UBYTE i = 0; i < 3; i++) {
        strncat(sizes, IMAGE_SIZE_NAMES[IMAGE_SIZES_DALL_E_2[i]], 1024);
        strncat(sizes, "\n", 1024);
    }
    strncat(sizes, "DALL-E 3\n", 1024);
    for (UBYTE i = 0; i < 3; i++) {
        strncat(sizes, IMAGE_SIZE_NAMES[IMAGE_SIZES_DALL_E_3[i]], 1024);
        strncat(sizes, "\n", 1024);
    }
    strncat(sizes, "GPT Image 1\n", 1024);
    for (UBYTE i = 0; i < 3; i++) {
        strncat(sizes, IMAGE_SIZE_NAMES[IMAGE_SIZES_GPT_IMAGE_1[i]], 1024);
        strncat(sizes, "\n", 1024);
    }
    set(app, MUIA_Application_RexxString, sizes);
    FreeVec(sizes);
    return (RETURN_OK);
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
    return (RETURN_OK);
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
    return (RETURN_OK);
}
MakeHook(ListVoicesHook, ListVoicesFunc);

HOOKPROTONHNO(SpeakTextFunc, APTR, ULONG *arg) {
    STRPTR modelString = (STRPTR)arg[0];
    STRPTR voiceString = (STRPTR)arg[1];
    STRPTR instructions = (STRPTR)arg[2];
    STRPTR apiKey = (STRPTR)arg[3];
    STRPTR prompt = (STRPTR)arg[4];

    if (apiKey == NULL) {
        apiKey = config.openAiApiKey;
    }
    enum OpenAITTSModel model;
    if (modelString == NULL) {
        model = config.openAITTSModel;
    } else {
        for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++) {
            if (strcmp(modelString, OPENAI_TTS_MODEL_NAMES[i]) == 0) {
                model = i;
                break;
            }
        }
    }
    enum OpenAITTSVoice voice;
    if (voiceString == NULL) {
        voice = config.openAITTSVoice;
    } else {
        for (UBYTE i = 0; OPENAI_TTS_VOICE_NAMES[i] != NULL; i++) {
            if (strcmp(voiceString, OPENAI_TTS_VOICE_NAMES[i]) == 0) {
                voice = i;
                break;
            }
        }
    }
    config.speechEnabled = TRUE;
    config.openAiApiKey = apiKey;
    config.speechSystem = SPEECH_SYSTEM_OPENAI;
    config.openAITTSModel = model;
    config.openAITTSVoice = voice;
    config.openAIVoiceInstructions = instructions;
    speakText(prompt);
    readConfig();
    set(app, MUIA_Application_RexxString, prompt);
    updateStatusBar(STRING_READY, greenPen);
    return (RETURN_OK);
}
MakeHook(SpeakTextHook, SpeakTextFunc);

HOOKPROTONHNO(HelpFunc, APTR, ULONG *arg) {
    set(app, MUIA_Application_RexxString,
        "SENDMESSAGE M=MODEL/K,S=SYSTEM/K,K=APIKEY/K,P=PROMPT/F\n"
        "CREATEIMAGE M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F\n"
        "SPEAKTEXT M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,P=PROMPT/F\n"
        "LISTCHATMODELS\n"
        "LISTIMAGEMODELS\n"
        "LISTIMAGESIZES\n"
        "LISTVOICEMODELS\n"
        "LISTVOICES\n");
    return (RETURN_OK);
}
MakeHook(HelpHook, HelpFunc);

struct MUI_Command arexxList[] = {
    {"SENDMESSAGE",
     "M=MODEL/K,S=SYSTEM/K,K=APIKEY/K,P=PROMPT/F",
     4,
     &SendMessageHook,
     {0, 0, 0, 0, 0}},
    {"CREATEIMAGE",
     "M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F",
     5,
     &CreateImageHook,
     {0, 0, 0, 0, 0}},
    {"SPEAKTEXT",
     "M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,P=PROMPT/F",
     5,
     &SpeakTextHook,
     {0, 0, 0, 0, 0}},
    {"LISTCHATMODELS", NULL, NULL, &ListChatModelsHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGEMODELS", NULL, NULL, &ListImageModelsHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGESIZES", NULL, NULL, &ListImageSizesHook, {0, 0, 0, 0, 0}},
    {"LISTVOICEMODELS", NULL, NULL, &ListVoiceModelsHook, {0, 0, 0, 0, 0}},
    {"LISTVOICES", NULL, NULL, &ListVoicesHook, {0, 0, 0, 0, 0}},
    {"?", NULL, NULL, &HelpHook, {0, 0, 0, 0, 0}},
    {NULL, NULL, 0, NULL, {0, 0, 0, 0, 0}}};
