#include <json-c/json.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "AmigaGPTConfig.h"
#include "ARexx.h"
#include "gui.h"
#include "MainWindow.h"
#include "openai.h"

/* Path to the daemon conversation history file in T: (cleared on reboot) */
#define DAEMON_HISTORY_PATH "T:AmigaGPTD_history.json"

/* Static conversation for the daemon to maintain context across ARexx calls */
static struct Conversation *daemonConversation = NULL;

/**
 * Save the daemon conversation to T:
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG saveDaemonConversation(void) {
    if (daemonConversation == NULL) {
        return RETURN_OK;
    }

    BPTR file = Open(DAEMON_HISTORY_PATH, MODE_NEWFILE);
    if (file == 0) {
        return RETURN_ERROR;
    }

    struct json_object *conversationJsonObject = json_object_new_object();
    struct json_object *messagesJsonArray = json_object_new_array();

    struct ConversationNode *conversationNode;
    for (conversationNode =
             (struct ConversationNode *)daemonConversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        if (!strcmp(conversationNode->role, "system"))
            continue;
        struct json_object *messageJsonObject = json_object_new_object();
        json_object_object_add(messageJsonObject, "role",
                               json_object_new_string(conversationNode->role));
        json_object_object_add(
            messageJsonObject, "content",
            json_object_new_string(conversationNode->content));
        json_object_array_add(messagesJsonArray, messageJsonObject);
    }
    json_object_object_add(conversationJsonObject, "messages",
                           messagesJsonArray);

    STRPTR jsonString = (STRPTR)json_object_to_json_string_ext(
        conversationJsonObject, JSON_C_TO_STRING_PRETTY);

    LONG result = RETURN_OK;
    if (Write(file, jsonString, strlen(jsonString)) !=
        (LONG)strlen(jsonString)) {
        result = RETURN_ERROR;
    }

    Close(file);
    json_object_put(conversationJsonObject);
    return result;
}

/**
 * Load the daemon conversation from T:
 * @return the loaded conversation, or NULL if not found/error
 **/
static struct Conversation *loadDaemonConversation(void) {
    BPTR file = Open(DAEMON_HISTORY_PATH, MODE_OLDFILE);
    if (file == 0) {
        return NULL;
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

    if (fileSize <= 0) {
        Close(file);
        return NULL;
    }

    STRPTR jsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
    if (jsonString == NULL) {
        Close(file);
        return NULL;
    }

    if (Read(file, jsonString, fileSize) != fileSize) {
        Close(file);
        FreeVec(jsonString);
        return NULL;
    }
    Close(file);

    struct json_object *conversationJsonObject = json_tokener_parse(jsonString);
    FreeVec(jsonString);

    if (conversationJsonObject == NULL) {
        return NULL;
    }

    struct json_object *messagesJsonArray;
    if (!json_object_object_get_ex(conversationJsonObject, "messages",
                                   &messagesJsonArray)) {
        json_object_put(conversationJsonObject);
        return NULL;
    }

    struct Conversation *conversation = newConversation();

    for (UWORD j = 0; j < json_object_array_length(messagesJsonArray); j++) {
        struct json_object *messageJsonObject =
            json_object_array_get_idx(messagesJsonArray, j);

        struct json_object *roleJsonObject;
        if (!json_object_object_get_ex(messageJsonObject, "role",
                                       &roleJsonObject)) {
            freeConversation(conversation);
            json_object_put(conversationJsonObject);
            return NULL;
        }
        STRPTR role = (STRPTR)json_object_get_string(roleJsonObject);

        struct json_object *contentJsonObject;
        if (!json_object_object_get_ex(messageJsonObject, "content",
                                       &contentJsonObject)) {
            freeConversation(conversation);
            json_object_put(conversationJsonObject);
            return NULL;
        }
        UTF8 *content = (UTF8 *)json_object_get_string(contentJsonObject);

        addTextToConversation(conversation, content, role);
    }

    json_object_put(conversationJsonObject);
    return conversation;
}

/**
 * Get or create the daemon conversation
 * @return the daemon conversation
 **/
static struct Conversation *getDaemonConversation(void) {
    if (daemonConversation == NULL) {
        /* Try to load from T: */
        daemonConversation = loadDaemonConversation();
        if (daemonConversation == NULL) {
            /* Create new if not found */
            daemonConversation = newConversation();
        }
    }
    return daemonConversation;
}

/**
 * Clear the daemon conversation history (start a new chat)
 **/
static void clearDaemonConversation(void) {
    if (daemonConversation != NULL) {
        freeConversation(daemonConversation);
        daemonConversation = NULL;
    }
    /* Delete the history file */
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    DeleteFile(DAEMON_HISTORY_PATH);
#else
    Delete(DAEMON_HISTORY_PATH);
#endif
}

HOOKPROTONH(ReplyCallbackFunc, APTR, Object *obj, struct RexxMsg *rxm) {
    printf("Args[0]: %s\nResult1: %ld   Result2: %ld\n", rxm->rm_Args[0],
           rxm->rm_Result1, rxm->rm_Result2);
}
MakeHook(ReplyCallbackHook, ReplyCallbackFunc);

HOOKPROTONHNO(SendMessageFunc, APTR, ULONG *arg) {
    STRPTR providerString = (STRPTR)arg[0];
    STRPTR model = (STRPTR)arg[1];
    STRPTR system = (STRPTR)arg[2];
    STRPTR host = (STRPTR)arg[3];
    ULONG *port = (ULONG *)arg[4];
    BOOL useSSL = (BOOL)arg[5];
    STRPTR apiKey = (STRPTR)arg[6];
    BOOL useProxy = (BOOL)arg[7];
    STRPTR proxyHost = (STRPTR)arg[8];
    ULONG *proxyPort = (ULONG *)arg[9];
    BOOL proxyUsesSSL = (BOOL)arg[10];
    BOOL proxyRequiresAuth = (BOOL)arg[11];
    STRPTR proxyUsername = (STRPTR)arg[12];
    STRPTR proxyPassword = (STRPTR)arg[13];
    BOOL webSearchEnabled = (BOOL)arg[14];
    STRPTR prompt = (STRPTR)arg[15];

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    /* Determine the provider to use */
    Provider provider = configGetChatProvider();
    if (providerString != NULL && strlen(providerString) > 0) {
        for (UBYTE i = 0; PROVIDER_NAMES[i] != NULL; i++) {
            if (strcasecmp(providerString, PROVIDER_NAMES[i]) == 0) {
                provider = (Provider)i;
                break;
            }
        }
        /* Also check for short names */
        if (strcasecmp(providerString, "openai") == 0) {
            provider = PROVIDER_OPENAI;
        } else if (strcasecmp(providerString, "gemini") == 0) {
            provider = PROVIDER_GEMINI;
        } else if (strcasecmp(providerString, "grok") == 0) {
            provider = PROVIDER_GROK;
        } else if (strcasecmp(providerString, "anthropic") == 0 ||
                   strcasecmp(providerString, "claude") == 0) {
            provider = PROVIDER_ANTHROPIC;
        } else if (strcasecmp(providerString, "custom") == 0) {
            provider = PROVIDER_CUSTOM;
        }
    }

    /* Resolve defaults from the selected profile */
    struct ChatRequestSettings rexxSettings;
    configGetChatRequestSettingsWithStreamOverride(&rexxSettings, provider,
                                                   FALSE);
    BOOL useCustomServer = (provider == PROVIDER_CUSTOM);

    ULONG portValue = port == NULL ? rexxSettings.port : (ULONG)*port;
    ULONG proxyPortValue =
        proxyPort == NULL ? rexxSettings.proxyPort : (ULONG)*proxyPort;

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = rexxSettings.apiKey;
    }

    if (host == NULL || strlen(host) == 0) {
        host = rexxSettings.host;
    }

    if (!useSSL) {
        useSSL = rexxSettings.useSSL;
    }

    if (!proxyUsesSSL) {
        proxyUsesSSL = rexxSettings.proxyUsesSSL;
    }

    if (!proxyRequiresAuth) {
        proxyRequiresAuth = rexxSettings.proxyRequiresAuth;
    }

    if (proxyUsername == NULL || strlen(proxyUsername) == 0) {
        proxyUsername = rexxSettings.proxyUsername;
    }

    if (proxyPassword == NULL || strlen(proxyPassword) == 0) {
        proxyPassword = rexxSettings.proxyPassword;
    }

    if (model == NULL || strlen(model) == 0) {
        model = rexxSettings.model;
    }

    /* Get the persistent daemon conversation (load from T: or create new) */
    struct Conversation *conversation = getDaemonConversation();
    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);
    addTextToConversation(conversation, promptUTF8, "user");
    CodesetsFreeA(promptUTF8, NULL);

    setConversationSystem(conversation, system);

    /* Determine API endpoint and auth type based on selected profile */
    APIEndpoint apiEndpoint = rexxSettings.apiEndpoint;
    AuthorizationType authType = rexxSettings.authorizationType;
    CONST_STRPTR customHeaders = rexxSettings.customHeaders;
    CONST_STRPTR apiEndpointUrl = rexxSettings.apiEndpointUrl;

    struct json_object **responses = postChatMessageToOpenAI(
        conversation, host, portValue, useSSL, model, apiKey, FALSE, useProxy,
        proxyHost, proxyPortValue, proxyUsesSSL, proxyRequiresAuth,
        proxyUsername, proxyPassword, webSearchEnabled, apiEndpoint,
        apiEndpointUrl, authType, customHeaders);

    /* Note: Don't free the conversation here - we keep it for context */

    if (responses == NULL) {
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_OK;
    }

    /* Handle shell tool calls if enabled - loop to handle multiple sequential
     * commands */
    while (!useCustomServer && configGetShellToolEnabled() &&
           hasPendingToolCall()) {
        STRPTR command = getPendingToolCommand();
        STRPTR callId = getPendingToolCallId();
        STRPTR responseId = getPendingResponseId();

        /* Ask user for confirmation before executing the command */
        UBYTE confirmMsg[4096];
        snprintf(confirmMsg, sizeof(confirmMsg),
                 "The AI wants to execute the following shell "
                 "command:\n\n%s\n\nAllow this command to run?",
                 command);
        LONG confirmResult = MUI_Request(app, NULL,
#ifdef __MORPHOS__
                                         NULL,
#else
                                         MUIV_Requester_Image_Warning,
#endif
                                         "Shell Command Confirmation",
                                         "*_Allow|_Deny", confirmMsg, TAG_DONE);

        if (confirmResult != 1) {
            /* User denied - clear pending tool call and return error */
            clearPendingToolCall();
            for (UWORD i = 0; responses[i] != NULL; i++) {
                json_object_put(responses[i]);
            }
            FreeVec(responses);
            set(app, MUIA_Application_RexxString,
                "Shell command denied by user");
            updateStatusBar(STRING_READY, greenPen);
            return RETURN_OK;
        }

        /* User allowed - proceed with execution */

        /* Execute the shell command */
        LONG exitCode = 0;
        STRPTR output = executeShellCommand(command, &exitCode);

        /* Build output string with exit code */
        UBYTE toolOutput[8192];
        snprintf(toolOutput, sizeof(toolOutput), "Exit code: %ld\nOutput:\n%s",
                 exitCode, output != NULL ? output : "(No output)");

        /* Send the tool result back to the API - this may set a new pending
         * tool call if OpenAI wants to run another command */
        struct json_object *toolResponse = postToolResultToOpenAI(
            responseId, callId, toolOutput, NULL, 0, TRUE, apiKey, useProxy,
            proxyHost, proxyPortValue, proxyUsesSSL, proxyRequiresAuth,
            proxyUsername, proxyPassword);

        if (output != NULL) {
            FreeVec(output);
        }

        /* Free the original responses */
        for (UWORD i = 0; responses[i] != NULL; i++) {
            json_object_put(responses[i]);
        }
        FreeVec(responses);
        responses = NULL;

        if (toolResponse == NULL) {
            clearPendingToolCall();
            set(app, MUIA_Application_RexxString,
                STRING_ERROR_CONNECTING_OPENAI);
            updateStatusBar(STRING_ERROR, redPen);
            return RETURN_OK;
        }

        /* Check for errors */
        struct json_object *error;
        if (json_object_object_get_ex(toolResponse, "error", &error) &&
            !json_object_is_type(error, json_type_null)) {
            clearPendingToolCall();
            struct json_object *message =
                json_object_object_get(error, "message");
            UTF8 *messageString = json_object_get_string(message);
            STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                (Tag)messageString, CSA_MapForeignChars, TRUE, TAG_DONE);
            set(app, MUIA_Application_RexxString,
                formattedMessageSystemEncoded);
            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
            json_object_put(toolResponse);
            updateStatusBar(STRING_ERROR, redPen);
            return RETURN_OK;
        }

        /* Check if there's another tool call in this response -
         * postToolResultToOpenAI will have set pendingToolCall if so */
        if (hasPendingToolCall()) {
            /* There's another tool call - loop will continue */
            json_object_put(toolResponse);
            /* Allocate a dummy responses array for the loop */
            responses = AllocVec(2 * sizeof(struct json_object *), MEMF_CLEAR);
            if (responses == NULL) {
                clearPendingToolCall();
                set(app, MUIA_Application_RexxString,
                    STRING_ERROR_CONNECTING_OPENAI);
                updateStatusBar(STRING_ERROR, redPen);
                return RETURN_OK;
            }
            responses[0] = NULL;
            responses[1] = NULL;
            continue;
        }

        /* No more tool calls - get the final response text */
        UTF8 *toolContentString = getMessageContentFromJson(
            toolResponse, FALSE, TRUE, API_ENDPOINT_RESPONSES);
        if (toolContentString != NULL && strlen(toolContentString) > 0) {
            /* Add response to conversation for context */
            addTextToConversation(conversation, toolContentString, "assistant");
            saveDaemonConversation();

            STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                (Tag)toolContentString, CSA_MapForeignChars, TRUE, TAG_DONE);
            set(app, MUIA_Application_RexxString,
                formattedMessageSystemEncoded);
            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
            json_object_put(toolResponse);
            updateStatusBar(STRING_READY, greenPen);
            return RETURN_OK;
        }
        json_object_put(toolResponse);

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
        UTF8 *contentString = getMessageContentFromJson(response, FALSE, TRUE,
                                                        API_ENDPOINT_RESPONSES);

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

        /* Add response to conversation for context */
        addTextToConversation(conversation, contentString, "assistant");
        saveDaemonConversation();

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
    STRPTR providerString = (STRPTR)arg[0];
    STRPTR modelString = (STRPTR)arg[1];
    STRPTR sizeString = (STRPTR)arg[2];
    STRPTR apiKey = (STRPTR)arg[3];
    STRPTR destination = (STRPTR)arg[4];
    STRPTR prompt = (STRPTR)arg[5];

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    /* Determine the provider to use */
    Provider provider = configGetImageProvider();
    if (providerString != NULL && strlen(providerString) > 0) {
        for (UBYTE i = 0; PROVIDER_NAMES[i] != NULL; i++) {
            if (strcasecmp(providerString, PROVIDER_NAMES[i]) == 0) {
                provider = (Provider)i;
                break;
            }
        }
        /* Also check for short names */
        if (strcasecmp(providerString, "openai") == 0) {
            provider = PROVIDER_OPENAI;
        } else if (strcasecmp(providerString, "gemini") == 0) {
            provider = PROVIDER_GEMINI;
        } else if (strcasecmp(providerString, "grok") == 0) {
            provider = PROVIDER_GROK;
        } else if (strcasecmp(providerString, "custom") == 0) {
            provider = PROVIDER_CUSTOM;
        }
    }

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = configGetApiKeyForProvider(provider);
    }

    /* Model is a string for OpenAI-compatible image endpoints */
    CONST_STRPTR modelName = modelString;
    if (modelName == NULL || strlen(modelName) == 0) {
        switch (provider) {
        case PROVIDER_GEMINI:
            modelName = GEMINI_IMAGE_MODELS[0];
            break;
        case PROVIDER_GROK:
            modelName = GROK_IMAGE_MODELS[0];
            break;
        case PROVIDER_OPENAI:
        case PROVIDER_CUSTOM:
        default:
            modelName = OPENAI_IMAGE_MODELS[0];
            break;
        }
    }

    ImageSize size = IMAGE_SIZE_1024x1024;
    if (sizeString != NULL && strlen(sizeString) > 0) {
        for (UBYTE i = 0; IMAGE_SIZE_NAMES[i] != NULL; i++) {
            if (strcmp(sizeString, IMAGE_SIZE_NAMES[i]) == 0) {
                size = (ImageSize)i;
                break;
            }
        }
    }

    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);

    /* Resolve provider connection settings */
    CONST_STRPTR host = NULL;
    UWORD port = 443;
    BOOL useSSL = TRUE;
    CONST_STRPTR apiEndpointUrl = "v1";
    AuthorizationType authType = AUTHORIZATION_TYPE_BEARER;
    CONST_STRPTR customHeaders = NULL;

    if (provider == PROVIDER_CUSTOM) {
        host = configGetCustomImageHost();
        port = (UWORD)configGetCustomImagePort();
        useSSL = configGetCustomImageUseSSL();
        apiEndpointUrl = configGetCustomImageApiEndpointUrl();
        authType = configGetCustomImageAuthorizationType();
        customHeaders = configGetCustomImageHeaders();
    } else {
        struct ProviderConfig *cfg = getProviderConfig(provider);
        if (cfg != NULL) {
            host = cfg->host;
            port = (UWORD)cfg->port;
            useSSL = cfg->useSSL;
            apiEndpointUrl = cfg->apiEndpointUrl;
            authType = cfg->authorizationType;
            customHeaders = cfg->customHeaders;
        }
    }

    struct json_object *response = postImageCreationRequestToOpenAIWithServer(
        promptUTF8, host, port, useSSL, apiEndpointUrl, authType, customHeaders,
        modelName, size, apiKey, FALSE, NULL, 0, FALSE, FALSE, NULL, NULL,
        IMAGE_FORMAT_JPG, provider);
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
    STRPTR models = AllocVec(4096, MEMF_ANY | MEMF_CLEAR);
    strncat(models, "OpenAI:\n", 4096);
    for (UBYTE i = 0; OPENAI_CHAT_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, OPENAI_CHAT_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    strncat(models, "\nGoogle Gemini:\n", 4096);
    for (UBYTE i = 0; GEMINI_CHAT_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, GEMINI_CHAT_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    strncat(models, "\nxAI Grok:\n", 4096);
    for (UBYTE i = 0; GROK_CHAT_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, GROK_CHAT_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    strncat(models, "\nAnthropic Claude:\n", 4096);
    for (UBYTE i = 0; ANTHROPIC_CHAT_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, ANTHROPIC_CHAT_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    set(app, MUIA_Application_RexxString, models);
    FreeVec(models);
    return RETURN_OK;
}
MakeHook(ListChatModelsHook, ListChatModelsFunc);

HOOKPROTONHNO(ListImageModelsFunc, APTR, ULONG *arg) {
    STRPTR models = AllocVec(4096, MEMF_ANY | MEMF_CLEAR);
    strncat(models, "OpenAI:\n", 4096);
    for (UBYTE i = 0; OPENAI_IMAGE_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, OPENAI_IMAGE_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    strncat(models, "\nGoogle Gemini:\n", 4096);
    for (UBYTE i = 0; GEMINI_IMAGE_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, GEMINI_IMAGE_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    strncat(models, "\nxAI Grok:\n", 4096);
    for (UBYTE i = 0; GROK_IMAGE_MODELS[i] != NULL; i++) {
        strncat(models, "  ", 4096);
        strncat(models, GROK_IMAGE_MODELS[i], 4096);
        strncat(models, "\n", 4096);
    }
    set(app, MUIA_Application_RexxString, models);
    FreeVec(models);
    return RETURN_OK;
}
MakeHook(ListImageModelsHook, ListImageModelsFunc);

HOOKPROTONHNO(ListProvidersFunc, APTR, ULONG *arg) {
    STRPTR providers = AllocVec(512, MEMF_ANY | MEMF_CLEAR);
    for (UBYTE i = 0; PROVIDER_NAMES[i] != NULL; i++) {
        strncat(providers, PROVIDER_NAMES[i], 512);
        strncat(providers, "\n", 512);
    }
    set(app, MUIA_Application_RexxString, providers);
    FreeVec(providers);
    return RETURN_OK;
}
MakeHook(ListProvidersHook, ListProvidersFunc);

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

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    ULONG portValue = port == NULL ? (useSSL ? 443 : 80) : (ULONG)*port;
    ULONG proxyPortValue = proxyPort == NULL ? 8080 : (ULONG)*proxyPort;

    if (apiKey == NULL) {
        apiKey = configGetOpenAiApiKey();
    }

    struct json_object *models = getChatModels(
        host, portValue, useSSL, apiKey, useProxy, proxyHost, proxyPortValue,
        proxyUsesSSL, proxyRequiresAuth, proxyUsername, proxyPassword, NULL,
        AUTHORIZATION_TYPE_BEARER, NULL);

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

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = configGetOpenAiApiKey();
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

    /* Temporarily set config for speakText */
    configSetOpenAiApiKey(apiKey);
    configSetSpeechSystem(SPEECH_SYSTEM_OPENAI);
    configSetOpenAITTSModel(model);
    configSetOpenAITTSVoice(voice);
    configSetOpenAIVoiceInstructions(instructions);
    speakText(prompt, output, &audioFormat);
    set(app, MUIA_Application_RexxString, prompt);
    updateStatusBar(STRING_READY, greenPen);
    return RETURN_OK;
}
MakeHook(SpeakTextHook, SpeakTextFunc);

HOOKPROTONHNO(NewChatFunc, APTR, ULONG *arg) {
    clearDaemonConversation();
    set(app, MUIA_Application_RexxString, "Conversation history cleared");
    return RETURN_OK;
}
MakeHook(NewChatHook, NewChatFunc);

HOOKPROTONHNO(HelpFunc, APTR, ULONG *arg) {
    set(app, MUIA_Application_RexxString,
        "SENDMESSAGE "
        "PR=PROVIDER/K,M=MODEL/K,S=SYSTEM/K,H=HOST/K,P=PORT/N,S=SSL/S,K=APIKEY/"
        "K,U=USEPROXY/"
        "S,PH=PROXYHOST/"
        "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
        "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K,W=WEBSEARCH/S,P=PROMPT/F\n"
        "CREATEIMAGE "
        "PR=PROVIDER/K,M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/"
        "F\n"
        "SPEAKTEXT "
        "M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,O=OUTPUT/K,P=PROMPT/"
        "F\n"
        "LISTSERVERMODELS "
        "H=HOST/K,P=PORT/N,S=SSL/S,K=APIKEY/K,U=USEPROXY/S,PH=PROXYHOST/"
        "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
        "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K\n"
        "NEWCHAT - Clear conversation history and start a new chat\n"
        "LISTPROVIDERS\n"
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
     "PR=PROVIDER/K,M=MODEL/K,S=SYSTEM/K,H=HOST/K,PO=PORT/N,S=SSL/S,K=APIKEY/"
     "K,U=USEPROXY/"
     "S,PH=PROXYHOST/"
     "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
     "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K,W=WEBSEARCH/S,P=PROMPT/F",
     16,
     &SendMessageHook,
     {0, 0, 0, 0, 0}},
    {"CREATEIMAGE",
     "PR=PROVIDER/K,M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F",
     6,
     &CreateImageHook,
     {0, 0, 0, 0, 0}},
    {"SPEAKTEXT",
     "M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,O=OUTPUT/K,F=FORMAT/"
     "K,P=PROMPT/F",
     7,
     &SpeakTextHook,
     {0, 0, 0, 0, 0}},
    {"NEWCHAT", NULL, NULL, &NewChatHook, {0, 0, 0, 0, 0}},
    {"LISTAUDIOFORMATS", NULL, NULL, &ListAudioFormatsHook, {0, 0, 0, 0, 0}},
    {"LISTSERVERMODELS",
     "H=HOST/K,PO=PORT/N,S=SSL/S,K=APIKEY/K,U=USEPROXY/S,PH=PROXYHOST/K,"
     "PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/S,"
     "PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K",
     11,
     &ListServerModelsHook,
     {0, 0, 0, 0, 0}},
    {"LISTPROVIDERS", NULL, NULL, &ListProvidersHook, {0, 0, 0, 0, 0}},
    {"LISTCHATMODELS", NULL, NULL, &ListChatModelsHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGEMODELS", NULL, NULL, &ListImageModelsHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGESIZES", NULL, NULL, &ListImageSizesHook, {0, 0, 0, 0, 0}},
    {"LISTVOICEMODELS", NULL, NULL, &ListVoiceModelsHook, {0, 0, 0, 0, 0}},
    {"LISTVOICES", NULL, NULL, &ListVoicesHook, {0, 0, 0, 0, 0}},
    {"?", NULL, NULL, &HelpHook, {0, 0, 0, 0, 0}},
    {NULL, NULL, 0, NULL, {0, 0, 0, 0, 0}}};
