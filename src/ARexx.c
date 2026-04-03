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
    if (daemonConversation->lastResponseId != NULL &&
        strlen(daemonConversation->lastResponseId) > 0) {
        json_object_object_add(
            conversationJsonObject, "lastResponseId",
            json_object_new_string(daemonConversation->lastResponseId));
    }

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
    struct json_object *lastResponseIdJsonObject;
    if (json_object_object_get_ex(conversationJsonObject, "lastResponseId",
                                  &lastResponseIdJsonObject)) {
        STRPTR lastResponseId =
            (STRPTR)json_object_get_string(lastResponseIdJsonObject);
        if (lastResponseId != NULL && strlen(lastResponseId) > 0) {
            conversation->lastResponseId =
                AllocVec(strlen(lastResponseId) + 1, MEMF_ANY | MEMF_CLEAR);
            if (conversation->lastResponseId != NULL) {
                strncpy(conversation->lastResponseId, lastResponseId,
                        strlen(lastResponseId));
            }
        }
    }

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
 * Read an entire text file into a null-terminated string.
 * The returned buffer must be freed with FreeVec().
 * @param path path to the file
 * @return allocated null-terminated buffer, or NULL on error
 **/
static STRPTR readTextFileToString(CONST_STRPTR path) {
    if (path == NULL || strlen(path) == 0) {
        return NULL;
    }

    BPTR file = Open(path, MODE_OLDFILE);
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

    if (fileSize < 0 || fileSize > 0x7fffffff) {
        Close(file);
        return NULL;
    }

    STRPTR buf = AllocVec((ULONG)fileSize + 1, MEMF_ANY | MEMF_CLEAR);
    if (buf == NULL) {
        Close(file);
        return NULL;
    }

    if (fileSize > 0) {
        LONG readLen = Read(file, buf, (LONG)fileSize);
        if (readLen != (LONG)fileSize) {
            Close(file);
            FreeVec(buf);
            return NULL;
        }
    }

    Close(file);
    buf[(ULONG)fileSize] = '\0';
    return buf;
}

#define REXX_LOCKED_PROFILE_NAME_OPENAI "OpenAI"
#define REXX_LOCKED_PROFILE_NAME_GEMINI "Google Gemini"
#define REXX_LOCKED_PROFILE_NAME_GROK "xAI Grok"
#define REXX_LOCKED_PROFILE_NAME_ANTHROPIC "Anthropic Claude"
#define REXX_DEFAULT_NARRATOR_RATE 150
#define REXX_DEFAULT_NARRATOR_PITCH 110
#define REXX_DEFAULT_NARRATOR_MODE 0
#define REXX_DEFAULT_NARRATOR_SEX 0

static STRPTR rexxResolvedChatHost = NULL;
static STRPTR rexxResolvedChatApiKey = NULL;
static STRPTR rexxResolvedChatModel = NULL;
static STRPTR rexxResolvedChatApiEndpointUrl = NULL;
static STRPTR rexxResolvedChatCustomHeaders = NULL;
static STRPTR rexxResolvedChatSystem = NULL;

static STRPTR rexxResolvedImageHost = NULL;
static STRPTR rexxResolvedImageApiKey = NULL;
static STRPTR rexxResolvedImageModel = NULL;
static STRPTR rexxResolvedImageApiEndpointUrl = NULL;
static STRPTR rexxResolvedImageCustomHeaders = NULL;

static STRPTR rexxDupStr(CONST_STRPTR s) {
    if (s == NULL)
        return NULL;
    ULONG len = strlen(s);
    STRPTR out = AllocVec(len + 1, MEMF_CLEAR);
    if (out != NULL)
        strncpy(out, s, len);
    return out;
}

static void rexxReplaceString(STRPTR *dest, CONST_STRPTR src) {
    if (dest == NULL)
        return;
    if (*dest != NULL) {
        FreeVec(*dest);
        *dest = NULL;
    }
    *dest = rexxDupStr(src);
}

static CONST_STRPTR rexxJsonGetStringDefault(struct json_object *obj,
                                             CONST_STRPTR key,
                                             CONST_STRPTR def) {
    if (obj == NULL || key == NULL)
        return def;
    struct json_object *v = json_object_object_get(obj, key);
    if (v == NULL)
        return def;
    CONST_STRPTR s = json_object_get_string(v);
    return (s != NULL) ? s : def;
}

static LONG rexxJsonGetIntDefault(struct json_object *obj, CONST_STRPTR key,
                                  LONG def) {
    if (obj == NULL || key == NULL)
        return def;
    struct json_object *v = json_object_object_get(obj, key);
    if (v == NULL)
        return def;
    return json_object_get_int(v);
}

static BOOL rexxJsonGetBoolDefault(struct json_object *obj, CONST_STRPTR key,
                                   BOOL def) {
    if (obj == NULL || key == NULL)
        return def;
    struct json_object *v = json_object_object_get(obj, key);
    if (v == NULL)
        return def;
    return json_object_get_boolean(v) ? TRUE : FALSE;
}

static struct json_object *
rexxParseProfilesJsonArray(CONST_STRPTR profilesStr) {
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

static struct json_object *rexxFindProfileObjectByName(struct json_object *arr,
                                                       CONST_STRPTR name) {
    if (arr == NULL || name == NULL)
        return NULL;
    int len = json_object_array_length(arr);
    struct json_object *caseInsensitiveMatch = NULL;
    for (int i = 0; i < len; i++) {
        struct json_object *profile = json_object_array_get_idx(arr, i);
        if (profile == NULL || !json_object_is_type(profile, json_type_object))
            continue;
        CONST_STRPTR profileName =
            rexxJsonGetStringDefault(profile, "name", NULL);
        if (profileName == NULL)
            continue;
        if (strcmp(profileName, name) == 0)
            return profile;
        if (caseInsensitiveMatch == NULL && strcasecmp(profileName, name) == 0)
            caseInsensitiveMatch = profile;
    }
    return caseInsensitiveMatch;
}

static CONST_STRPTR rexxResolveChatProfileAlias(CONST_STRPTR name) {
    if (name == NULL || strlen(name) == 0)
        return NULL;
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_OPENAI) == 0 ||
        strcasecmp(name, "ChatGPT") == 0 ||
        strcasecmp(name, "OpenAI ChatGPT") == 0) {
        return REXX_LOCKED_PROFILE_NAME_OPENAI;
    }
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_GEMINI) == 0 ||
        strcasecmp(name, "Google") == 0 || strcasecmp(name, "Gemini") == 0) {
        return REXX_LOCKED_PROFILE_NAME_GEMINI;
    }
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_GROK) == 0 ||
        strcasecmp(name, "xAI") == 0 || strcasecmp(name, "Grok") == 0) {
        return REXX_LOCKED_PROFILE_NAME_GROK;
    }
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_ANTHROPIC) == 0 ||
        strcasecmp(name, "Anthropic") == 0 || strcasecmp(name, "Claude") == 0) {
        return REXX_LOCKED_PROFILE_NAME_ANTHROPIC;
    }
    return NULL;
}

static CONST_STRPTR rexxResolveImageProfileAlias(CONST_STRPTR name) {
    if (name == NULL || strlen(name) == 0)
        return NULL;
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_OPENAI) == 0 ||
        strcasecmp(name, "ChatGPT") == 0 ||
        strcasecmp(name, "OpenAI ChatGPT") == 0) {
        return REXX_LOCKED_PROFILE_NAME_OPENAI;
    }
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_GEMINI) == 0 ||
        strcasecmp(name, "Google") == 0 || strcasecmp(name, "Gemini") == 0) {
        return REXX_LOCKED_PROFILE_NAME_GEMINI;
    }
    if (strcasecmp(name, REXX_LOCKED_PROFILE_NAME_GROK) == 0 ||
        strcasecmp(name, "xAI") == 0 || strcasecmp(name, "Grok") == 0) {
        return REXX_LOCKED_PROFILE_NAME_GROK;
    }
    return NULL;
}

static CONST_STRPTR rexxResolveSpeechProfileAlias(CONST_STRPTR name) {
    if (name == NULL || strlen(name) == 0)
        return NULL;
    if (strcasecmp(name, "OpenAI") == 0 || strcasecmp(name, "ChatGPT") == 0 ||
        strcasecmp(name, "OpenAI ChatGPT") == 0) {
        return "OpenAI";
    }
    return NULL;
}

static BOOL rexxIsLockedChatProfileName(CONST_STRPTR name) {
    if (name == NULL)
        return FALSE;
    return (strcmp(name, REXX_LOCKED_PROFILE_NAME_OPENAI) == 0) ||
           (strcmp(name, REXX_LOCKED_PROFILE_NAME_GEMINI) == 0) ||
           (strcmp(name, REXX_LOCKED_PROFILE_NAME_GROK) == 0) ||
           (strcmp(name, REXX_LOCKED_PROFILE_NAME_ANTHROPIC) == 0);
}

static BOOL rexxIsLockedImageProfileName(CONST_STRPTR name) {
    if (name == NULL)
        return FALSE;
    return (strcmp(name, REXX_LOCKED_PROFILE_NAME_OPENAI) == 0) ||
           (strcmp(name, REXX_LOCKED_PROFILE_NAME_GEMINI) == 0) ||
           (strcmp(name, REXX_LOCKED_PROFILE_NAME_GROK) == 0);
}

static void rexxFillLockedChatProfileDefaults(struct ChatRequestSettings *out,
                                              CONST_STRPTR profileName) {
    if (strcmp(profileName, REXX_LOCKED_PROFILE_NAME_OPENAI) == 0) {
        out->host = (STRPTR) "api.openai.com";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_RESPONSES;
        out->apiEndpointUrl = "v1";
        out->authorizationType = AUTHORIZATION_TYPE_BEARER;
        out->customHeaders = NULL;
        out->apiKey = configGetOpenAiApiKey();
        STRPTR model = NULL;
        STRPTR system = NULL;
        ULONG webSearch = TRUE;
        ULONG shellTool = FALSE;
        if (configObj) {
            get(configObj, MUIA_AmigaGPTConfig_OpenAiChatModelName, &model);
            get(configObj, MUIA_AmigaGPTConfig_OpenAiWebSearchEnabled,
                &webSearch);
            get(configObj, MUIA_AmigaGPTConfig_OpenAiShellToolEnabled,
                &shellTool);
            get(configObj, MUIA_AmigaGPTConfig_OpenAiChatSystem, &system);
        }
        out->model =
            (model != NULL && strlen(model) > 0) ? model : "gpt-5-chat-latest";
        out->webSearchEnabled = (BOOL)webSearch;
        out->shellToolEnabled = (BOOL)shellTool;
        rexxReplaceString(&rexxResolvedChatSystem, system);
        out->chatSystem = rexxResolvedChatSystem;
    } else if (strcmp(profileName, REXX_LOCKED_PROFILE_NAME_GEMINI) == 0) {
        out->host = (STRPTR) "generativelanguage.googleapis.com";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT;
        out->apiEndpointUrl = "v1beta";
        out->authorizationType = AUTHORIZATION_TYPE_X_GOOGLE_API_KEY;
        out->customHeaders = NULL;
        out->apiKey = configGetGeminiApiKey();
        STRPTR model = NULL;
        STRPTR system = NULL;
        ULONG webSearch = TRUE;
        if (configObj) {
            get(configObj, MUIA_AmigaGPTConfig_GeminiChatModelName, &model);
            get(configObj, MUIA_AmigaGPTConfig_GeminiWebSearchEnabled,
                &webSearch);
            get(configObj, MUIA_AmigaGPTConfig_GeminiChatSystem, &system);
        }
        out->model =
            (model != NULL && strlen(model) > 0) ? model : "gemini-2.5-flash";
        out->webSearchEnabled = (BOOL)webSearch;
        out->shellToolEnabled = FALSE;
        rexxReplaceString(&rexxResolvedChatSystem, system);
        out->chatSystem = rexxResolvedChatSystem;
    } else if (strcmp(profileName, REXX_LOCKED_PROFILE_NAME_GROK) == 0) {
        out->host = (STRPTR) "api.x.ai";
        out->port = 443;
        out->useSSL = TRUE;
        out->apiEndpoint = API_CHAT_ENDPOINT_RESPONSES;
        out->apiEndpointUrl = "v1";
        out->authorizationType = AUTHORIZATION_TYPE_BEARER;
        out->customHeaders = NULL;
        out->apiKey = configGetGrokApiKey();
        STRPTR model = NULL;
        STRPTR system = NULL;
        ULONG webSearch = TRUE;
        if (configObj) {
            get(configObj, MUIA_AmigaGPTConfig_GrokChatModelName, &model);
            get(configObj, MUIA_AmigaGPTConfig_GrokWebSearchEnabled,
                &webSearch);
            get(configObj, MUIA_AmigaGPTConfig_GrokChatSystem, &system);
        }
        out->model = (model != NULL && strlen(model) > 0) ? model : "grok-4";
        out->webSearchEnabled = (BOOL)webSearch;
        out->shellToolEnabled = FALSE;
        rexxReplaceString(&rexxResolvedChatSystem, system);
        out->chatSystem = rexxResolvedChatSystem;
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
        STRPTR system = NULL;
        ULONG webSearch = TRUE;
        if (configObj) {
            get(configObj, MUIA_AmigaGPTConfig_AnthropicChatModelName, &model);
            get(configObj, MUIA_AmigaGPTConfig_AnthropicWebSearchEnabled,
                &webSearch);
            get(configObj, MUIA_AmigaGPTConfig_AnthropicChatSystem, &system);
        }
        out->model = (model != NULL && strlen(model) > 0)
                         ? model
                         : "claude-3-5-sonnet-latest";
        out->webSearchEnabled = (BOOL)webSearch;
        out->shellToolEnabled = FALSE;
        rexxReplaceString(&rexxResolvedChatSystem, system);
        out->chatSystem = rexxResolvedChatSystem;
    }
}

static BOOL rexxResolveChatProfileSettings(CONST_STRPTR profileName,
                                           struct ChatRequestSettings *out) {
    if (out == NULL)
        return FALSE;
    if (profileName == NULL || strlen(profileName) == 0) {
        configGetChatRequestSettingsWithStreamOverride(out, FALSE);
        return TRUE;
    }

    memset(out, 0, sizeof(*out));
    out->useProxy = configGetProxyEnabled();
    out->proxyHost = configGetProxyHost();
    out->proxyPort = (UWORD)configGetProxyPort();
    out->proxyUsesSSL = configGetProxyUsesSSL();
    out->proxyRequiresAuth = configGetProxyRequiresAuth();
    out->proxyUsername = configGetProxyUsername();
    out->proxyPassword = configGetProxyPassword();
    out->profileName = profileName;

    CONST_STRPTR resolvedProfileName = profileName;
    CONST_STRPTR aliasProfileName = rexxResolveChatProfileAlias(profileName);

    if (rexxIsLockedChatProfileName(resolvedProfileName)) {
        rexxFillLockedChatProfileDefaults(out, resolvedProfileName);
        out->stream = FALSE;
        return TRUE;
    }

    struct json_object *arr =
        rexxParseProfilesJsonArray(configGetCustomServerProfiles());
    struct json_object *profile =
        rexxFindProfileObjectByName(arr, resolvedProfileName);
    if (profile == NULL && aliasProfileName != NULL) {
        resolvedProfileName = aliasProfileName;
        out->profileName = resolvedProfileName;
        if (rexxIsLockedChatProfileName(resolvedProfileName)) {
            if (arr != NULL)
                json_object_put(arr);
            rexxFillLockedChatProfileDefaults(out, resolvedProfileName);
            out->stream = FALSE;
            return TRUE;
        }
        profile = rexxFindProfileObjectByName(arr, resolvedProfileName);
    }
    if (profile == NULL) {
        if (arr != NULL)
            json_object_put(arr);
        return FALSE;
    }

    rexxReplaceString(&rexxResolvedChatHost,
                      rexxJsonGetStringDefault(profile, "host", ""));
    out->host = rexxResolvedChatHost;
    out->port = (UWORD)rexxJsonGetIntDefault(profile, "port", 443);
    out->useSSL = rexxJsonGetBoolDefault(profile, "useSSL", TRUE);
    out->authorizationType = (AuthorizationType)rexxJsonGetIntDefault(
        profile, "authorizationType", (LONG)AUTHORIZATION_TYPE_BEARER);
    rexxReplaceString(&rexxResolvedChatApiKey,
                      rexxJsonGetStringDefault(profile, "apiKey", ""));
    out->apiKey = rexxResolvedChatApiKey;
    rexxReplaceString(&rexxResolvedChatModel,
                      rexxJsonGetStringDefault(
                          profile, "chatModel",
                          rexxJsonGetStringDefault(profile, "imageModel", "")));
    out->model = rexxResolvedChatModel;
    out->stream = FALSE;
    out->apiEndpoint = (APIChatEndpoint)rexxJsonGetIntDefault(
        profile, "apiEndpoint", (LONG)API_CHAT_ENDPOINT_CHAT_COMPLETIONS);
    rexxReplaceString(
        &rexxResolvedChatApiEndpointUrl,
        rexxJsonGetStringDefault(profile, "apiEndpointUrl", "v1"));
    out->apiEndpointUrl = rexxResolvedChatApiEndpointUrl;
    rexxReplaceString(&rexxResolvedChatCustomHeaders,
                      rexxJsonGetStringDefault(profile, "customHeaders", ""));
    out->customHeaders = rexxResolvedChatCustomHeaders;
    out->webSearchEnabled =
        rexxJsonGetBoolDefault(profile, "webSearchEnabled", TRUE);
    out->shellToolEnabled =
        rexxJsonGetBoolDefault(profile, "shellToolEnabled", FALSE);
    rexxReplaceString(&rexxResolvedChatSystem,
                      rexxJsonGetStringDefault(profile, "chatSystem", ""));
    out->chatSystem = rexxResolvedChatSystem;

    if (arr != NULL)
        json_object_put(arr);
    return TRUE;
}

static BOOL rexxResolveImageProfileSettings(CONST_STRPTR profileName,
                                            struct ImageRequestSettings *out) {
    if (out == NULL)
        return FALSE;
    if (profileName == NULL || strlen(profileName) == 0) {
        configGetActiveImageRequestSettings(out);
        return TRUE;
    }

    memset(out, 0, sizeof(*out));
    out->useProxy = configGetProxyEnabled();
    out->proxyHost = configGetProxyHost();
    out->proxyPort = (UWORD)configGetProxyPort();
    out->proxyUsesSSL = configGetProxyUsesSSL();
    out->proxyRequiresAuth = configGetProxyRequiresAuth();
    out->proxyUsername = configGetProxyUsername();
    out->proxyPassword = configGetProxyPassword();
    out->profileName = profileName;

    CONST_STRPTR resolvedProfileName = profileName;
    CONST_STRPTR aliasProfileName = rexxResolveImageProfileAlias(profileName);

    if (rexxIsLockedImageProfileName(resolvedProfileName)) {
        if (strcmp(resolvedProfileName, REXX_LOCKED_PROFILE_NAME_OPENAI) == 0) {
            out->host = (STRPTR) "api.openai.com";
            out->port = 443;
            out->useSSL = TRUE;
            out->apiEndpointUrl = "v1";
            out->authorizationType = AUTHORIZATION_TYPE_BEARER;
            out->customHeaders = NULL;
            out->apiKey = configGetOpenAiImageApiKey();
            out->model = configGetOpenAiImageModelName();
            out->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
        } else if (strcmp(resolvedProfileName,
                          REXX_LOCKED_PROFILE_NAME_GEMINI) == 0) {
            out->host = (STRPTR) "generativelanguage.googleapis.com";
            out->port = 443;
            out->useSSL = TRUE;
            out->apiEndpointUrl = "v1beta";
            out->authorizationType = AUTHORIZATION_TYPE_X_GOOGLE_API_KEY;
            out->customHeaders = NULL;
            out->apiKey = configGetGeminiImageApiKey();
            out->model = configGetGeminiImageModelName();
            out->imageApiEndpoint = API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT;
        } else {
            out->host = (STRPTR) "api.x.ai";
            out->port = 443;
            out->useSSL = TRUE;
            out->apiEndpointUrl = "v1";
            out->authorizationType = AUTHORIZATION_TYPE_BEARER;
            out->customHeaders = NULL;
            out->apiKey = configGetGrokImageApiKey();
            out->model = configGetGrokImageModelName();
            out->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
        }
        return TRUE;
    }

    struct json_object *arr =
        rexxParseProfilesJsonArray(configGetCustomImageServerProfiles());
    struct json_object *profile =
        rexxFindProfileObjectByName(arr, resolvedProfileName);
    if (profile == NULL && aliasProfileName != NULL) {
        resolvedProfileName = aliasProfileName;
        out->profileName = resolvedProfileName;
        if (rexxIsLockedImageProfileName(resolvedProfileName)) {
            if (arr != NULL)
                json_object_put(arr);
            if (strcmp(resolvedProfileName, REXX_LOCKED_PROFILE_NAME_OPENAI) ==
                0) {
                out->host = (STRPTR) "api.openai.com";
                out->port = 443;
                out->useSSL = TRUE;
                out->apiEndpointUrl = "v1";
                out->authorizationType = AUTHORIZATION_TYPE_BEARER;
                out->customHeaders = NULL;
                out->apiKey = configGetOpenAiImageApiKey();
                out->model = configGetOpenAiImageModelName();
                out->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
            } else if (strcmp(resolvedProfileName,
                              REXX_LOCKED_PROFILE_NAME_GEMINI) == 0) {
                out->host = (STRPTR) "generativelanguage.googleapis.com";
                out->port = 443;
                out->useSSL = TRUE;
                out->apiEndpointUrl = "v1beta";
                out->authorizationType = AUTHORIZATION_TYPE_X_GOOGLE_API_KEY;
                out->customHeaders = NULL;
                out->apiKey = configGetGeminiImageApiKey();
                out->model = configGetGeminiImageModelName();
                out->imageApiEndpoint =
                    API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT;
            } else {
                out->host = (STRPTR) "api.x.ai";
                out->port = 443;
                out->useSSL = TRUE;
                out->apiEndpointUrl = "v1";
                out->authorizationType = AUTHORIZATION_TYPE_BEARER;
                out->customHeaders = NULL;
                out->apiKey = configGetGrokImageApiKey();
                out->model = configGetGrokImageModelName();
                out->imageApiEndpoint = API_IMAGE_ENDPOINT_IMAGES_GENERATIONS;
            }
            return TRUE;
        }
        profile = rexxFindProfileObjectByName(arr, resolvedProfileName);
    }
    if (profile == NULL) {
        if (arr != NULL)
            json_object_put(arr);
        return FALSE;
    }

    rexxReplaceString(&rexxResolvedImageHost,
                      rexxJsonGetStringDefault(profile, "host", ""));
    out->host = rexxResolvedImageHost;
    out->port = (UWORD)rexxJsonGetIntDefault(profile, "port", 443);
    out->useSSL = rexxJsonGetBoolDefault(profile, "useSSL", TRUE);
    out->authorizationType = (AuthorizationType)rexxJsonGetIntDefault(
        profile, "authorizationType", (LONG)AUTHORIZATION_TYPE_BEARER);
    rexxReplaceString(&rexxResolvedImageApiKey,
                      rexxJsonGetStringDefault(profile, "apiKey", ""));
    out->apiKey = rexxResolvedImageApiKey;
    rexxReplaceString(&rexxResolvedImageModel,
                      rexxJsonGetStringDefault(
                          profile, "imageModel",
                          rexxJsonGetStringDefault(profile, "chatModel", "")));
    out->model = rexxResolvedImageModel;
    rexxReplaceString(
        &rexxResolvedImageApiEndpointUrl,
        rexxJsonGetStringDefault(profile, "apiEndpointUrl", "v1"));
    out->apiEndpointUrl = rexxResolvedImageApiEndpointUrl;
    rexxReplaceString(&rexxResolvedImageCustomHeaders,
                      rexxJsonGetStringDefault(profile, "customHeaders", ""));
    out->customHeaders = rexxResolvedImageCustomHeaders;
    out->imageApiEndpoint = (APIImageEndpoint)rexxJsonGetIntDefault(
        profile, "apiEndpoint", (LONG)API_IMAGE_ENDPOINT_IMAGES_GENERATIONS);

    if (arr != NULL)
        json_object_put(arr);
    return TRUE;
}

static void rexxApplySpeechSystemDefaults(struct SpeechRequestSettings *out) {
    if (out == NULL)
        return;
    if (out->accentPath != NULL) {
        FreeVec(out->accentPath);
        out->accentPath = NULL;
    }

    if (out->speechSystem == SPEECH_SYSTEM_34) {
        out->accentPath = rexxDupStr(configGetSpeechAccent34());
        out->narratorRate = (UWORD)configGetNarratorRate34();
        out->narratorPitch = (UWORD)configGetNarratorPitch34();
        out->narratorMode = (UWORD)configGetNarratorMode34();
        out->narratorSex = (UWORD)configGetNarratorSex34();
    } else if (out->speechSystem == SPEECH_SYSTEM_37) {
        out->accentPath = rexxDupStr(configGetSpeechAccent37());
        out->narratorRate = (UWORD)configGetNarratorRate37();
        out->narratorPitch = (UWORD)configGetNarratorPitch37();
        out->narratorMode = (UWORD)configGetNarratorMode37();
        out->narratorSex = (UWORD)configGetNarratorSex37();
    } else {
        out->accentPath = rexxDupStr(configGetSpeechAccent());
        out->narratorRate = REXX_DEFAULT_NARRATOR_RATE;
        out->narratorPitch = REXX_DEFAULT_NARRATOR_PITCH;
        out->narratorMode = REXX_DEFAULT_NARRATOR_MODE;
        out->narratorSex = REXX_DEFAULT_NARRATOR_SEX;
    }
}

static BOOL
rexxResolveSpeechProfileSettings(CONST_STRPTR profileName,
                                 struct SpeechRequestSettings *out) {
    if (out == NULL)
        return FALSE;
    if (profileName == NULL || strlen(profileName) == 0) {
        configGetSpeechRequestSettings(out);
        return TRUE;
    }

    memset(out, 0, sizeof(*out));
    out->activeProfileName = rexxDupStr(profileName);
    out->speechSystem = (SpeechSystem)configGetSpeechSystem();
    out->fliteVoice = configGetSpeechFliteVoice();
    out->openAiApiKey = rexxDupStr(configGetOpenAiSpeechApiKey());
    out->openAiTtsModel = configGetOpenAITTSModel();
    out->openAiTtsVoice = configGetOpenAITTSVoice();
    out->openAiVoiceInstructions =
        rexxDupStr(configGetOpenAIVoiceInstructions());
    out->elevenLabsApiKey = rexxDupStr(configGetElevenLabsAPIKey());
    out->elevenLabsVoiceID = rexxDupStr(configGetElevenLabsVoiceID());
    out->elevenLabsVoiceName = rexxDupStr(configGetElevenLabsVoiceName());
    out->elevenLabsModel = rexxDupStr(configGetElevenLabsModel());
    out->elevenLabsModelName = rexxDupStr(configGetElevenLabsModelName());
    rexxApplySpeechSystemDefaults(out);

    CONST_STRPTR resolvedProfileName = profileName;
    CONST_STRPTR aliasProfileName = rexxResolveSpeechProfileAlias(profileName);

    for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
        if (strcmp(resolvedProfileName, SPEECH_SYSTEM_NAMES[s]) == 0) {
            out->speechSystem = (SpeechSystem)s;
            rexxApplySpeechSystemDefaults(out);
            return TRUE;
        }
    }

    struct json_object *arr =
        rexxParseProfilesJsonArray(configGetSpeechProfiles());
    struct json_object *profile =
        rexxFindProfileObjectByName(arr, resolvedProfileName);
    if (profile == NULL && aliasProfileName != NULL) {
        resolvedProfileName = aliasProfileName;
        if (out->activeProfileName != NULL)
            FreeVec(out->activeProfileName);
        out->activeProfileName = rexxDupStr(resolvedProfileName);
        for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
            if (strcmp(resolvedProfileName, SPEECH_SYSTEM_NAMES[s]) == 0) {
                if (arr != NULL)
                    json_object_put(arr);
                out->speechSystem = (SpeechSystem)s;
                rexxApplySpeechSystemDefaults(out);
                return TRUE;
            }
        }
        profile = rexxFindProfileObjectByName(arr, resolvedProfileName);
    }
    if (profile == NULL) {
        if (arr != NULL)
            json_object_put(arr);
        configFreeSpeechRequestSettings(out);
        memset(out, 0, sizeof(*out));
        return FALSE;
    }

    struct json_object *sysObj =
        json_object_object_get(profile, "speechSystem");
    if (sysObj != NULL)
        out->speechSystem = (SpeechSystem)json_object_get_int(sysObj);

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
        json_object_object_get(profile, "speechAccent");
    if (accentObj != NULL) {
        if (out->accentPath != NULL) {
            FreeVec(out->accentPath);
            out->accentPath = NULL;
        }
        out->accentPath = rexxDupStr(json_object_get_string(accentObj));
    }

    struct json_object *fliteVoiceObj =
        json_object_object_get(profile, "speechFliteVoice");
    if (fliteVoiceObj != NULL)
        out->fliteVoice = (SpeechFliteVoice)json_object_get_int(fliteVoiceObj);

    struct json_object *narratorRateObj =
        json_object_object_get(profile, "narratorRate");
    if (narratorRateObj != NULL)
        out->narratorRate = (UWORD)json_object_get_int(narratorRateObj);
    struct json_object *narratorPitchObj =
        json_object_object_get(profile, "narratorPitch");
    if (narratorPitchObj != NULL)
        out->narratorPitch = (UWORD)json_object_get_int(narratorPitchObj);
    struct json_object *narratorModeObj =
        json_object_object_get(profile, "narratorMode");
    if (narratorModeObj != NULL)
        out->narratorMode = (UWORD)json_object_get_int(narratorModeObj);
    struct json_object *narratorSexObj =
        json_object_object_get(profile, "narratorSex");
    if (narratorSexObj != NULL)
        out->narratorSex = (UWORD)json_object_get_int(narratorSexObj);

    struct json_object *openAiApiKeyObj =
        json_object_object_get(profile, "openAiApiKey");
    if (openAiApiKeyObj != NULL) {
        if (out->openAiApiKey != NULL)
            FreeVec(out->openAiApiKey);
        out->openAiApiKey = rexxDupStr(json_object_get_string(openAiApiKeyObj));
    }
    struct json_object *openAiModelObj =
        json_object_object_get(profile, "openAITTSModel");
    if (openAiModelObj != NULL)
        out->openAiTtsModel =
            (OpenAITTSModel)json_object_get_int(openAiModelObj);
    struct json_object *openAiVoiceObj =
        json_object_object_get(profile, "openAITTSVoice");
    if (openAiVoiceObj != NULL)
        out->openAiTtsVoice =
            (OpenAITTSVoice)json_object_get_int(openAiVoiceObj);
    struct json_object *openAiInstructionsObj =
        json_object_object_get(profile, "openAIVoiceInstructions");
    if (openAiInstructionsObj != NULL) {
        if (out->openAiVoiceInstructions != NULL)
            FreeVec(out->openAiVoiceInstructions);
        out->openAiVoiceInstructions =
            rexxDupStr(json_object_get_string(openAiInstructionsObj));
    }

    struct json_object *elevenLabsApiKeyObj =
        json_object_object_get(profile, "elevenLabsAPIKey");
    if (elevenLabsApiKeyObj != NULL) {
        if (out->elevenLabsApiKey != NULL)
            FreeVec(out->elevenLabsApiKey);
        out->elevenLabsApiKey =
            rexxDupStr(json_object_get_string(elevenLabsApiKeyObj));
    }
    struct json_object *elevenLabsVoiceIDObj =
        json_object_object_get(profile, "elevenLabsVoiceID");
    if (elevenLabsVoiceIDObj != NULL) {
        if (out->elevenLabsVoiceID != NULL)
            FreeVec(out->elevenLabsVoiceID);
        out->elevenLabsVoiceID =
            rexxDupStr(json_object_get_string(elevenLabsVoiceIDObj));
    }
    struct json_object *elevenLabsVoiceNameObj =
        json_object_object_get(profile, "elevenLabsVoiceName");
    if (elevenLabsVoiceNameObj != NULL) {
        if (out->elevenLabsVoiceName != NULL)
            FreeVec(out->elevenLabsVoiceName);
        out->elevenLabsVoiceName =
            rexxDupStr(json_object_get_string(elevenLabsVoiceNameObj));
    }
    struct json_object *elevenLabsModelObj =
        json_object_object_get(profile, "elevenLabsModel");
    if (elevenLabsModelObj != NULL) {
        if (out->elevenLabsModel != NULL)
            FreeVec(out->elevenLabsModel);
        out->elevenLabsModel =
            rexxDupStr(json_object_get_string(elevenLabsModelObj));
    }
    struct json_object *elevenLabsModelNameObj =
        json_object_object_get(profile, "elevenLabsModelName");
    if (elevenLabsModelNameObj != NULL) {
        if (out->elevenLabsModelName != NULL)
            FreeVec(out->elevenLabsModelName);
        out->elevenLabsModelName =
            rexxDupStr(json_object_get_string(elevenLabsModelNameObj));
    }

    if (arr != NULL)
        json_object_put(arr);
    return TRUE;
}

static void rexxSetProfileNotFoundError(CONST_STRPTR kind,
                                        CONST_STRPTR profileName) {
    UBYTE errMsg[512];
    CONST_STRPTR displayKind =
        kind != NULL ? kind : (CONST_STRPTR)STRING_REXX_REQUESTED;
    snprintf(errMsg, sizeof(errMsg), STRING_ERROR_REXX_PROFILE_NOT_FOUND,
             displayKind, profileName != NULL ? profileName : "");
    set(app, MUIA_Application_RexxString, errMsg);
    updateStatusBar(STRING_ERROR, redPen);
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
    STRPTR profileString = (STRPTR)arg[0];
    STRPTR model = (STRPTR)arg[1];
    STRPTR system = (STRPTR)arg[2];
    STRPTR systemFile = (STRPTR)arg[3];
    STRPTR host = (STRPTR)arg[4];
    ULONG *port = (ULONG *)arg[5];
    BOOL useSSL = (BOOL)arg[6];
    STRPTR apiKey = (STRPTR)arg[7];
    BOOL useProxy = (BOOL)arg[8];
    STRPTR proxyHost = (STRPTR)arg[9];
    ULONG *proxyPort = (ULONG *)arg[10];
    BOOL proxyUsesSSL = (BOOL)arg[11];
    BOOL proxyRequiresAuth = (BOOL)arg[12];
    STRPTR proxyUsername = (STRPTR)arg[13];
    STRPTR proxyPassword = (STRPTR)arg[14];
    BOOL webSearchRequested = (BOOL)arg[15];
    STRPTR prompt = (STRPTR)arg[16];

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    /* Resolve defaults from the selected chat profile. */
    struct ChatRequestSettings rexxSettings;
    if (!rexxResolveChatProfileSettings(profileString, &rexxSettings)) {
        rexxSetProfileNotFoundError(STRING_MENU_CHAT, profileString);
        return RETURN_OK;
    }
    ULONG portValue = port == NULL ? rexxSettings.port : (ULONG)*port;
    ULONG proxyPortValue =
        proxyPort == NULL ? rexxSettings.proxyPort : (ULONG)*proxyPort;

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = rexxSettings.apiKey;
    }

    if (host == NULL || strlen(host) == 0) {
        host = rexxSettings.host;
    }

    if (!useProxy) {
        useProxy = rexxSettings.useProxy;
    }

    if (!useSSL) {
        useSSL = rexxSettings.useSSL;
    }

    if (proxyHost == NULL || strlen(proxyHost) == 0) {
        proxyHost = rexxSettings.proxyHost;
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

    BOOL webSearchEnabled = rexxSettings.webSearchEnabled || webSearchRequested;

    /* Get the persistent daemon conversation (load from T: or create new) */
    struct Conversation *conversation = getDaemonConversation();
    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);
    addTextToConversation(conversation, promptUTF8, "user");
    CodesetsFreeA(promptUTF8, NULL);

    /* Resolve effective system message from SYSTEMFILE and SYSTEM. */
    STRPTR systemFromFile = NULL;
    STRPTR combinedSystem = NULL;
    CONST_STRPTR effectiveSystem = (system != NULL && strlen(system) > 0)
                                       ? system
                                       : rexxSettings.chatSystem;

    if (systemFile != NULL && strlen(systemFile) > 0) {
        systemFromFile = readTextFileToString(systemFile);
        if (systemFromFile == NULL) {
            UBYTE errMsg[512];
            snprintf(errMsg, sizeof(errMsg), STRING_ERROR_REXX_SYSTEM_FILE_READ,
                     systemFile);
            set(app, MUIA_Application_RexxString, errMsg);
            updateStatusBar(STRING_ERROR, redPen);
            return RETURN_OK;
        }

        effectiveSystem = systemFromFile;
        if (system != NULL && strlen(system) > 0) {
            ULONG need =
                (ULONG)strlen(systemFromFile) + (ULONG)strlen(system) + 1;
            combinedSystem = AllocVec(need, MEMF_ANY | MEMF_CLEAR);
            if (combinedSystem == NULL) {
                FreeVec(systemFromFile);
                set(app, MUIA_Application_RexxString,
                    STRING_ERROR_REXX_SYSTEM_FILE_MEMORY);
                updateStatusBar(STRING_ERROR, redPen);
                return RETURN_OK;
            }
            strncat(combinedSystem, systemFromFile, need - 1);
            strncat(combinedSystem, system, need - strlen(combinedSystem) - 1);
            effectiveSystem = combinedSystem;
        }
    }

    setConversationSystem(conversation, effectiveSystem);
    if (combinedSystem != NULL) {
        FreeVec(combinedSystem);
    }
    if (systemFromFile != NULL) {
        FreeVec(systemFromFile);
    }

    /* Determine API endpoint and auth type based on selected profile */
    APIChatEndpoint apiEndpoint = rexxSettings.apiEndpoint;
    AuthorizationType authType = rexxSettings.authorizationType;
    CONST_STRPTR customHeaders = rexxSettings.customHeaders;
    CONST_STRPTR apiEndpointUrl = rexxSettings.apiEndpointUrl;

    struct json_object **responses = postChatMessageToOpenAI(
        conversation, host, portValue, useSSL, model, apiKey, FALSE, useProxy,
        proxyHost, proxyPortValue, proxyUsesSSL, proxyRequiresAuth,
        proxyUsername, proxyPassword, webSearchEnabled,
        rexxSettings.shellToolEnabled, apiEndpoint, apiEndpointUrl, authType,
        customHeaders);

    /* Note: Don't free the conversation here - we keep it for context */

    if (responses == NULL) {
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_OK;
    }

    /* Handle shell tool calls if enabled - loop to handle multiple sequential
     * commands */
    while (rexxSettings.shellToolEnabled && hasPendingToolCall()) {
        STRPTR command = getPendingToolCommand();
        STRPTR callId = getPendingToolCallId();
        STRPTR responseId = getPendingResponseId();

        /* Ask user for confirmation before executing the command */
        UBYTE confirmMsg[4096];
        snprintf(confirmMsg, sizeof(confirmMsg),
                 STRING_SHELL_TOOL_CONFIRMATION_BODY, command);
        LONG confirmResult = MUI_Request(app, NULL,
#ifdef __MORPHOS__
                                         NULL,
#else
                                         MUIV_Requester_Image_Warning,
#endif
                                         STRING_SHELL_TOOL_CONFIRMATION_TITLE,
                                         STRING_SHELL_TOOL_CONFIRMATION_BUTTONS,
                                         confirmMsg, TAG_DONE);

        if (confirmResult != 1) {
            /* User denied - clear pending tool call and return error */
            clearPendingToolCall();
            for (UWORD i = 0; responses[i] != NULL; i++) {
                json_object_put(responses[i]);
            }
            FreeVec(responses);
            set(app, MUIA_Application_RexxString,
                STRING_SHELL_TOOL_DENIED_BANNER);
            updateStatusBar(STRING_READY, greenPen);
            return RETURN_OK;
        }

        /* User allowed - proceed with execution */
        updateStatusBar(STRING_EXECUTING_COMMAND, yellowPen);

        /* Execute the shell command */
        LONG exitCode = 0;
        STRPTR output = executeShellCommand(command, &exitCode);

        /* Build output string with exit code */
        UBYTE toolOutput[8192];
        snprintf(toolOutput, sizeof(toolOutput),
                 STRING_SHELL_TOOL_TOOL_OUTPUT_FORMAT, exitCode,
                 output != NULL ? output : (STRPTR)STRING_SHELL_TOOL_NO_OUTPUT);

        /* Send the tool result back to the API - this may set a new pending
         * tool call if OpenAI wants to run another command */
        struct json_object *toolResponse = postToolResultToOpenAI(
            responseId, callId, toolOutput, model, (STRPTR)host,
            (UWORD)portValue, useSSL, apiKey, useProxy, proxyHost,
            proxyPortValue, proxyUsesSSL, proxyRequiresAuth, proxyUsername,
            proxyPassword, rexxSettings.shellToolEnabled, apiEndpointUrl,
            authType, customHeaders);

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

        conversationSyncLastResponseIdFromPayload(conversation, toolResponse);

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
        UTF8 *toolContentString =
            getMessageContentFromJson(toolResponse, FALSE, TRUE, apiEndpoint);
        if (toolContentString != NULL && strlen(toolContentString) > 0) {
            /* Create a copy and strip unnecessary escape backslashes. */
            STRPTR cleanedContent = AllocVec(
                strlen((STRPTR)toolContentString) + 1, MEMF_ANY | MEMF_CLEAR);
            if (cleanedContent != NULL) {
                STRPTR rPtr = (STRPTR)toolContentString;
                STRPTR wPtr = cleanedContent;
                while (*rPtr) {
                    if (*rPtr == '\\' &&
                        (*(rPtr + 1) == '"' || *(rPtr + 1) == '\'' ||
                         *(rPtr + 1) == '*' || *(rPtr + 1) == '_' ||
                         *(rPtr + 1) == '`')) {
                        rPtr++; /* Skip the escape backslash. */
                    }
                    *wPtr++ = *rPtr++;
                }
                *wPtr = '\0';

                /* Add response to conversation history for context. */
                addTextToConversation(conversation, cleanedContent,
                                      "assistant");
                saveDaemonConversation();

                STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)cleanedContent, CSA_MapForeignChars, TRUE, TAG_DONE);
                set(app, MUIA_Application_RexxString,
                    formattedMessageSystemEncoded);
                CodesetsFreeA(formattedMessageSystemEncoded, NULL);
                FreeVec(cleanedContent);
            }
            json_object_put(toolResponse);
            updateStatusBar(STRING_READY, greenPen);
            return RETURN_OK;
        }
        json_object_put(toolResponse);

        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_OK;
    }

    if (responses[0] == NULL) {
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        FreeVec(responses);
        return RETURN_OK;
    }

    /* Concatenate all assistant text across response objects. Some providers
     * return web-search results as additional message objects. */
    ULONG combinedLen = 1;
    UWORD ri = 0;
    struct json_object *response = NULL;
    while ((response = responses[ri++]) != NULL) {
        struct json_object *error;
        if (json_object_object_get_ex(response, "error", &error) &&
            !json_object_is_type(error, json_type_null)) {
            struct json_object *message =
                json_object_object_get(error, "message");
            UTF8 *messageString = json_object_get_string(message);
            STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                (Tag)messageString, CSA_MapForeignChars, TRUE, TAG_DONE);
            set(app, MUIA_Application_RexxString,
                formattedMessageSystemEncoded);
            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
            /* Cleanup */
            ri = 0;
            while ((response = responses[ri++]) != NULL) {
                json_object_put(response);
            }
            FreeVec(responses);
            updateStatusBar(STRING_ERROR, redPen);
            return RETURN_OK;
        }
        UTF8 *part =
            getMessageContentFromJson(response, FALSE, TRUE, apiEndpoint);
        if (part != NULL)
            combinedLen += strlen(part) + 1;
    }

    STRPTR combined = AllocVec(combinedLen, MEMF_ANY | MEMF_CLEAR);
    if (combined == NULL) {
        updateStatusBar(STRING_ERROR, redPen);
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        ri = 0;
        while ((response = responses[ri++]) != NULL) {
            json_object_put(response);
        }
        FreeVec(responses);
        return RETURN_OK;
    }

    ri = 0;
    while ((response = responses[ri++]) != NULL) {
        UTF8 *part =
            getMessageContentFromJson(response, FALSE, TRUE, apiEndpoint);
        if (part != NULL && strlen(part) > 0) {
            strncat(combined, part, combinedLen - strlen(combined) - 1);
        }
    }

    if (strlen(combined) == 0) {
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        updateStatusBar(STRING_ERROR, redPen);
        ri = 0;
        while ((response = responses[ri++]) != NULL) {
            json_object_put(response);
        }
        FreeVec(responses);
        FreeVec(combined);
        return RETURN_OK;
    }

    /* Remove escaped markdown/shell characters the LLM may emit. */
    STRPTR readPtr = combined;
    STRPTR writePtr = combined;
    while (*readPtr) {
        if (*readPtr == '\\' &&
            (*(readPtr + 1) == '"' || *(readPtr + 1) == '\'' ||
             *(readPtr + 1) == '*' || *(readPtr + 1) == '_' ||
             *(readPtr + 1) == '`')) {
            readPtr++; /* Skip the escape backslash. */
        }
        *writePtr++ = *readPtr++;
    }
    *writePtr = '\0';

    /* Add response to conversation history for context. */
    addTextToConversation(conversation, combined, "assistant");
    saveDaemonConversation();

    STRPTR formattedMessageSystemEncoded =
        CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                          (Tag)combined, CSA_MapForeignChars, TRUE, TAG_DONE);
    set(app, MUIA_Application_RexxString, formattedMessageSystemEncoded);
    CodesetsFreeA(formattedMessageSystemEncoded, NULL);

    ri = 0;
    while ((response = responses[ri++]) != NULL) {
        json_object_put(response);
    }
    FreeVec(responses);
    FreeVec(combined);
    updateStatusBar(STRING_READY, greenPen);
    return RETURN_OK;
}
MakeHook(SendMessageHook, SendMessageFunc);

HOOKPROTONHNO(CreateImageFunc, APTR, ULONG *arg) {
    STRPTR profileString = (STRPTR)arg[0];
    STRPTR modelString = (STRPTR)arg[1];
    STRPTR sizeString = (STRPTR)arg[2];
    STRPTR apiKey = (STRPTR)arg[3];
    STRPTR destination = (STRPTR)arg[4];
    STRPTR prompt = (STRPTR)arg[5];

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    /* Resolve defaults from the selected image profile. */
    struct ImageRequestSettings imgSettings;
    if (!rexxResolveImageProfileSettings(profileString, &imgSettings)) {
        rexxSetProfileNotFoundError(STRING_IMAGE, profileString);
        return RETURN_ERROR;
    }

    if (apiKey == NULL || strlen(apiKey) == 0) {
        apiKey = (STRPTR)imgSettings.apiKey;
    }

    /* Model is a string for OpenAI-compatible image endpoints */
    CONST_STRPTR modelName = modelString;
    if (modelName == NULL || strlen(modelName) == 0) {
        modelName = imgSettings.model;
        if (modelName == NULL || strlen(modelName) == 0) {
            modelName = OPENAI_IMAGE_MODELS[0];
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

    /* Use selected image profile connection settings. */
    CONST_STRPTR resolvedHost = imgSettings.host;
    UWORD resolvedPort = imgSettings.port;
    BOOL resolvedUseSSL = imgSettings.useSSL;
    CONST_STRPTR resolvedEndpointUrl = imgSettings.apiEndpointUrl;
    AuthorizationType resolvedAuthType = imgSettings.authorizationType;
    CONST_STRPTR resolvedHeaders = imgSettings.customHeaders;
    APIImageEndpoint imageEndpoint = imgSettings.imageApiEndpoint;

    struct json_object *response = postImageCreationRequestToOpenAI(
        promptUTF8, resolvedHost, resolvedPort, resolvedUseSSL,
        resolvedEndpointUrl, resolvedAuthType, resolvedHeaders, modelName, size,
        apiKey, imgSettings.useProxy, imgSettings.proxyHost,
        imgSettings.proxyPort, imgSettings.proxyUsesSSL,
        imgSettings.proxyRequiresAuth, imgSettings.proxyUsername,
        imgSettings.proxyPassword, IMAGE_FORMAT_JPG, imageEndpoint);
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

    UTF8 *imageData = CodesetsDecodeB64(
        CSA_B64SourceString, (Tag)b64, CSA_B64SourceLen, (Tag)strlen(b64),
        CSA_B64DestPtr, (Tag)&imageData, TAG_DONE);
    if (imageData == NULL) {
        updateStatusBar(STRING_ERROR, redPen);
        return RETURN_ERROR;
    }
    LONG data_len = strlen(imageData);

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
    CodesetsFreeA(imageData, NULL);

    json_object_put(response);
    updateStatusBar(STRING_READY, greenPen);
    set(app, MUIA_Application_RexxString, destination);
    return RETURN_OK;
}
MakeHook(CreateImageHook, CreateImageFunc);

static void rexxAppendText(STRPTR buffer, ULONG bufferSize, CONST_STRPTR text) {
    if (buffer == NULL || text == NULL || bufferSize == 0)
        return;
    ULONG currentLength = strlen(buffer);
    if (currentLength >= bufferSize - 1)
        return;
    strncat(buffer, text, bufferSize - currentLength - 1);
}

static void rexxAppendLine(STRPTR buffer, ULONG bufferSize, CONST_STRPTR text) {
    rexxAppendText(buffer, bufferSize, text);
    rexxAppendText(buffer, bufferSize, "\n");
}

static void rexxAppendSectionHeader(STRPTR buffer, ULONG bufferSize,
                                    CONST_STRPTR title) {
    if (buffer == NULL || title == NULL)
        return;
    if (strlen(buffer) > 0)
        rexxAppendText(buffer, bufferSize, "\n");
    rexxAppendText(buffer, bufferSize, title);
    rexxAppendText(buffer, bufferSize, ":\n");
}

static void rexxAppendStringArray(STRPTR buffer, ULONG bufferSize,
                                  CONST_STRPTR values[]) {
    if (buffer == NULL || values == NULL)
        return;
    for (UBYTE i = 0; values[i] != NULL; i++) {
        rexxAppendLine(buffer, bufferSize, values[i]);
    }
}

static void rexxAppendMutableStringArray(STRPTR buffer, ULONG bufferSize,
                                         STRPTR const values[]) {
    if (buffer == NULL || values == NULL)
        return;
    for (UBYTE i = 0; values[i] != NULL; i++) {
        rexxAppendLine(buffer, bufferSize, values[i]);
    }
}

static void rexxAppendProfileNamesFromJson(STRPTR buffer, ULONG bufferSize,
                                           CONST_STRPTR profilesStr) {
    struct json_object *arr = rexxParseProfilesJsonArray(profilesStr);
    if (arr == NULL)
        return;
    int len = json_object_array_length(arr);
    for (int i = 0; i < len; i++) {
        struct json_object *profile = json_object_array_get_idx(arr, i);
        if (profile == NULL || !json_object_is_type(profile, json_type_object))
            continue;
        CONST_STRPTR name = rexxJsonGetStringDefault(profile, "name", NULL);
        if (name != NULL && strlen(name) > 0)
            rexxAppendLine(buffer, bufferSize, name);
    }
    json_object_put(arr);
}

static void rexxAppendModelNamesFromJsonArray(STRPTR buffer, ULONG bufferSize,
                                              struct json_object *models) {
    if (buffer == NULL || models == NULL ||
        !json_object_is_type(models, json_type_array)) {
        return;
    }
    int len = json_object_array_length(models);
    for (int i = 0; i < len; i++) {
        struct json_object *model = json_object_array_get_idx(models, i);
        if (model == NULL)
            continue;
        CONST_STRPTR name = json_object_get_string(model);
        if (name != NULL && strlen(name) > 0)
            rexxAppendLine(buffer, bufferSize, name);
    }
}

static void rexxAppendElevenLabsModelNames(STRPTR buffer, ULONG bufferSize,
                                           CONST_STRPTR apiKey) {
    struct json_object *response = makeHttpsGetRequest(
        "api.elevenlabs.io", 443, "/v1/models", apiKey, "xi-api-key", FALSE,
        configGetProxyEnabled(), configGetProxyHost(), configGetProxyPort(),
        configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
        configGetProxyUsername(), configGetProxyPassword());
    if (response == NULL) {
        rexxAppendLine(buffer, bufferSize, STRING_ERROR_FETCHING_MODELS);
        return;
    }

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(response, "models", &modelsArray)) {
        if (json_object_is_type(response, json_type_array)) {
            modelsArray = response;
        } else {
            json_object_put(response);
            rexxAppendLine(buffer, bufferSize, STRING_ERROR_FETCHING_MODELS);
            return;
        }
    }

    LONG modelCount = json_object_array_length(modelsArray);
    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        if (modelObj == NULL ||
            !json_object_is_type(modelObj, json_type_object))
            continue;
        CONST_STRPTR name = rexxJsonGetStringDefault(modelObj, "name", NULL);
        CONST_STRPTR id = rexxJsonGetStringDefault(modelObj, "model_id", NULL);
        if (name != NULL && strlen(name) > 0) {
            rexxAppendLine(buffer, bufferSize, name);
        } else if (id != NULL && strlen(id) > 0) {
            rexxAppendLine(buffer, bufferSize, id);
        }
    }

    json_object_put(response);
}

static void rexxAppendElevenLabsVoiceNames(STRPTR buffer, ULONG bufferSize,
                                           CONST_STRPTR apiKey) {
    struct json_object *response = makeHttpsGetRequest(
        "api.elevenlabs.io", 443, "/v2/voices", apiKey, "xi-api-key", FALSE,
        configGetProxyEnabled(), configGetProxyHost(), configGetProxyPort(),
        configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
        configGetProxyUsername(), configGetProxyPassword());
    if (response == NULL) {
        rexxAppendLine(buffer, bufferSize, STRING_ERROR_REXX_FETCHING_VOICES);
        return;
    }

    struct json_object *voicesArray = NULL;
    if (!json_object_object_get_ex(response, "voices", &voicesArray)) {
        if (json_object_is_type(response, json_type_array)) {
            voicesArray = response;
        } else {
            json_object_put(response);
            rexxAppendLine(buffer, bufferSize,
                           STRING_ERROR_REXX_FETCHING_VOICES);
            return;
        }
    }
    if (!json_object_is_type(voicesArray, json_type_array)) {
        json_object_put(response);
        rexxAppendLine(buffer, bufferSize, STRING_ERROR_REXX_FETCHING_VOICES);
        return;
    }

    LONG voiceCount = json_object_array_length(voicesArray);
    for (LONG i = 0; i < voiceCount; i++) {
        struct json_object *voiceObj =
            json_object_array_get_idx(voicesArray, i);
        if (voiceObj == NULL ||
            !json_object_is_type(voiceObj, json_type_object))
            continue;
        CONST_STRPTR name = rexxJsonGetStringDefault(voiceObj, "name", NULL);
        CONST_STRPTR id = rexxJsonGetStringDefault(voiceObj, "voice_id", NULL);
        if (name != NULL && strlen(name) > 0) {
            rexxAppendLine(buffer, bufferSize, name);
        } else if (id != NULL && strlen(id) > 0) {
            rexxAppendLine(buffer, bufferSize, id);
        }
    }

    json_object_put(response);
}

static void rexxAppendChatModelsForProfile(STRPTR buffer, ULONG bufferSize,
                                           CONST_STRPTR requestedProfile) {
    struct ChatRequestSettings settings;
    if (!rexxResolveChatProfileSettings(requestedProfile, &settings))
        return;

    CONST_STRPTR titleProfile = settings.profileName != NULL
                                    ? settings.profileName
                                    : (CONST_STRPTR)STRING_MENU_CHAT;
    UBYTE title[256];
    snprintf(title, sizeof(title), "%s (%s)", STRING_MENU_CHAT, titleProfile);
    rexxAppendSectionHeader(buffer, bufferSize, title);

    struct json_object *models = getChatModels(
        settings.host, settings.port, settings.useSSL, settings.apiKey,
        settings.useProxy, settings.proxyHost, settings.proxyPort,
        settings.proxyUsesSSL, settings.proxyRequiresAuth,
        settings.proxyUsername, settings.proxyPassword, settings.apiEndpointUrl,
        settings.authorizationType, settings.customHeaders);
    if (models == NULL) {
        rexxAppendLine(buffer, bufferSize, STRING_ERROR_FETCHING_MODELS);
        return;
    }
    rexxAppendModelNamesFromJsonArray(buffer, bufferSize, models);
    json_object_put(models);
}

static void rexxAppendImageModelsForProfile(STRPTR buffer, ULONG bufferSize,
                                            CONST_STRPTR requestedProfile) {
    struct ImageRequestSettings settings;
    if (!rexxResolveImageProfileSettings(requestedProfile, &settings))
        return;

    CONST_STRPTR titleProfile = settings.profileName != NULL
                                    ? settings.profileName
                                    : (CONST_STRPTR)STRING_IMAGE;
    UBYTE title[256];
    snprintf(title, sizeof(title), "%s (%s)", STRING_IMAGE, titleProfile);
    rexxAppendSectionHeader(buffer, bufferSize, title);

    struct json_object *models = getChatModels(
        settings.host, settings.port, settings.useSSL, settings.apiKey,
        settings.useProxy, settings.proxyHost, settings.proxyPort,
        settings.proxyUsesSSL, settings.proxyRequiresAuth,
        settings.proxyUsername, settings.proxyPassword, settings.apiEndpointUrl,
        settings.authorizationType, settings.customHeaders);
    if (models == NULL) {
        rexxAppendLine(buffer, bufferSize, STRING_ERROR_FETCHING_MODELS);
        return;
    }
    rexxAppendModelNamesFromJsonArray(buffer, bufferSize, models);
    json_object_put(models);
}

static void rexxAppendSpeechModelsForProfile(STRPTR buffer, ULONG bufferSize,
                                             CONST_STRPTR requestedProfile) {
    struct SpeechRequestSettings settings;
    if (!rexxResolveSpeechProfileSettings(requestedProfile, &settings))
        return;

    CONST_STRPTR titleProfile = settings.activeProfileName != NULL
                                    ? settings.activeProfileName
                                    : (CONST_STRPTR)STRING_MENU_SPEECH;
    UBYTE title[256];
    snprintf(title, sizeof(title), "%s (%s)", STRING_MENU_SPEECH, titleProfile);
    rexxAppendSectionHeader(buffer, bufferSize, title);

    if (settings.speechSystem == SPEECH_SYSTEM_OPENAI) {
        struct json_object *models = getChatModels(
            (STRPTR) "api.openai.com", 443, TRUE, settings.openAiApiKey,
            configGetProxyEnabled(), configGetProxyHost(), configGetProxyPort(),
            configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
            configGetProxyUsername(), configGetProxyPassword(), "v1",
            AUTHORIZATION_TYPE_BEARER, NULL);
        if (models == NULL) {
            rexxAppendLine(buffer, bufferSize, STRING_ERROR_FETCHING_MODELS);
        } else {
            rexxAppendModelNamesFromJsonArray(buffer, bufferSize, models);
            json_object_put(models);
        }
    } else if (settings.speechSystem == SPEECH_SYSTEM_ELEVENLABS) {
        rexxAppendElevenLabsModelNames(buffer, bufferSize,
                                       settings.elevenLabsApiKey);
    } else {
        rexxAppendLine(buffer, bufferSize,
                       STRING_REXX_NO_SELECTABLE_SPEECH_MODELS);
    }

    configFreeSpeechRequestSettings(&settings);
}

static void rexxAppendVoicesForSpeechProfile(STRPTR buffer, ULONG bufferSize,
                                             CONST_STRPTR requestedProfile) {
    struct SpeechRequestSettings settings;
    if (!rexxResolveSpeechProfileSettings(requestedProfile, &settings))
        return;

    if (settings.speechSystem == SPEECH_SYSTEM_OPENAI) {
        rexxAppendStringArray(buffer, bufferSize, OPENAI_TTS_VOICE_NAMES);
    } else if (settings.speechSystem == SPEECH_SYSTEM_ELEVENLABS) {
        rexxAppendElevenLabsVoiceNames(buffer, bufferSize,
                                       settings.elevenLabsApiKey);
    } else if (settings.speechSystem == SPEECH_SYSTEM_34 ||
               settings.speechSystem == SPEECH_SYSTEM_37) {
        rexxAppendLine(buffer, bufferSize, "male");
        rexxAppendLine(buffer, bufferSize, "female");
        rexxAppendLine(buffer, bufferSize, "male robot");
        rexxAppendLine(buffer, bufferSize, "female robot");
    } else if (settings.speechSystem == SPEECH_SYSTEM_FLITE) {
        rexxAppendMutableStringArray(buffer, bufferSize,
                                     SPEECH_FLITE_VOICE_NAMES);
    } else {
        rexxAppendLine(buffer, bufferSize,
                       STRING_REXX_NO_SELECTABLE_SPEECH_VOICES);
    }

    configFreeSpeechRequestSettings(&settings);
}

static BOOL
rexxApplyWorkbenchVoiceOverride(struct SpeechRequestSettings *settings,
                                CONST_STRPTR voiceString) {
    if (settings == NULL || voiceString == NULL || strlen(voiceString) == 0)
        return FALSE;
    if (settings->speechSystem != SPEECH_SYSTEM_34 &&
        settings->speechSystem != SPEECH_SYSTEM_37)
        return FALSE;

    if (strcasecmp(voiceString, "male") == 0) {
        settings->narratorSex = 0;
        settings->narratorMode = 0;
        return TRUE;
    }
    if (strcasecmp(voiceString, "female") == 0) {
        settings->narratorSex = 1;
        settings->narratorMode = 0;
        return TRUE;
    }
    if (strcasecmp(voiceString, "male robot") == 0) {
        settings->narratorSex = 0;
        settings->narratorMode = 1;
        return TRUE;
    }
    if (strcasecmp(voiceString, "female robot") == 0) {
        settings->narratorSex = 1;
        settings->narratorMode = 1;
        return TRUE;
    }

    return FALSE;
}

HOOKPROTONHNO(ListProfilesFunc, APTR, ULONG *arg) {
    STRPTR profiles = AllocVec(16384, MEMF_ANY | MEMF_CLEAR);
    if (profiles == NULL)
        return RETURN_ERROR;

    rexxAppendSectionHeader(profiles, 16384, STRING_MENU_CHAT);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_OPENAI);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_GEMINI);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_GROK);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_ANTHROPIC);
    rexxAppendProfileNamesFromJson(profiles, 16384,
                                   configGetCustomServerProfiles());

    rexxAppendSectionHeader(profiles, 16384, STRING_IMAGE);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_OPENAI);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_GEMINI);
    rexxAppendLine(profiles, 16384, REXX_LOCKED_PROFILE_NAME_GROK);
    rexxAppendProfileNamesFromJson(profiles, 16384,
                                   configGetCustomImageServerProfiles());

    rexxAppendSectionHeader(profiles, 16384, STRING_MENU_SPEECH);
    for (UBYTE i = 0; SPEECH_SYSTEM_NAMES[i] != NULL; i++) {
        rexxAppendLine(profiles, 16384, SPEECH_SYSTEM_NAMES[i]);
    }
    rexxAppendProfileNamesFromJson(profiles, 16384, configGetSpeechProfiles());

    set(app, MUIA_Application_RexxString, profiles);
    FreeVec(profiles);
    return RETURN_OK;
}
MakeHook(ListProfilesHook, ListProfilesFunc);

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

HOOKPROTONHNO(ListVoicesFunc, APTR, ULONG *arg) {
    STRPTR profileString = (STRPTR)arg[0];
    STRPTR voices = AllocVec(16384, MEMF_ANY | MEMF_CLEAR);
    if (voices == NULL)
        return RETURN_ERROR;

    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    if (profileString == NULL || strlen(profileString) == 0) {
        rexxAppendVoicesForSpeechProfile(voices, 16384,
                                         configGetActiveSpeechProfileName());
    } else {
        struct SpeechRequestSettings speechSettings;
        if (!rexxResolveSpeechProfileSettings(profileString, &speechSettings)) {
            FreeVec(voices);
            rexxSetProfileNotFoundError(STRING_MENU_SPEECH, profileString);
            return RETURN_ERROR;
        }
        configFreeSpeechRequestSettings(&speechSettings);
        rexxAppendVoicesForSpeechProfile(voices, 16384, profileString);
    }

    if (strlen(voices) == 0) {
        FreeVec(voices);
        rexxSetProfileNotFoundError(STRING_MENU_SPEECH, profileString);
        return RETURN_ERROR;
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

HOOKPROTONHNO(ListModelsFunc, APTR, ULONG *arg) {
    STRPTR profileString = (STRPTR)arg[0];
    STRPTR models = AllocVec(16384, MEMF_ANY | MEMF_CLEAR);
    BOOL foundAny = FALSE;
    if (models == NULL)
        return RETURN_ERROR;

    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    if (profileString == NULL || strlen(profileString) == 0) {
        rexxAppendChatModelsForProfile(models, 16384,
                                       configGetActiveProfileName());
        rexxAppendImageModelsForProfile(models, 16384,
                                        configGetActiveImageProfileName());
        rexxAppendSpeechModelsForProfile(models, 16384,
                                         configGetActiveSpeechProfileName());
        foundAny = strlen(models) > 0;
    } else {
        struct ChatRequestSettings chatSettings;
        if (rexxResolveChatProfileSettings(profileString, &chatSettings)) {
            rexxAppendChatModelsForProfile(models, 16384, profileString);
            foundAny = TRUE;
        }

        struct ImageRequestSettings imageSettings;
        if (rexxResolveImageProfileSettings(profileString, &imageSettings)) {
            rexxAppendImageModelsForProfile(models, 16384, profileString);
            foundAny = TRUE;
        }

        struct SpeechRequestSettings speechSettings;
        if (rexxResolveSpeechProfileSettings(profileString, &speechSettings)) {
            configFreeSpeechRequestSettings(&speechSettings);
            rexxAppendSpeechModelsForProfile(models, 16384, profileString);
            foundAny = TRUE;
        }
    }

    if (!foundAny || strlen(models) == 0) {
        FreeVec(models);
        rexxSetProfileNotFoundError(STRING_PROFILES, profileString);
        return RETURN_ERROR;
    }

    set(app, MUIA_Application_RexxString, models);
    FreeVec(models);
    return RETURN_OK;
}
MakeHook(ListModelsHook, ListModelsFunc);

HOOKPROTONHNO(SpeakTextFunc, APTR, ULONG *arg) {
    STRPTR profileString = (STRPTR)arg[0];
    STRPTR modelString = (STRPTR)arg[1];
    STRPTR voiceString = (STRPTR)arg[2];
    STRPTR instructions = (STRPTR)arg[3];
    STRPTR apiKey = (STRPTR)arg[4];
    STRPTR output = (STRPTR)arg[5];
    STRPTR audioFormatString = (STRPTR)arg[6];
    STRPTR prompt = (STRPTR)arg[7];

    /* Reload config from disk to pick up any changes from the main app */
    DoMethod(configObj, MUIM_AmigaGPTConfig_Load);

    struct SpeechRequestSettings speechSettings;
    if (!rexxResolveSpeechProfileSettings(profileString, &speechSettings)) {
        rexxSetProfileNotFoundError(STRING_MENU_SPEECH, profileString);
        return RETURN_ERROR;
    }

    if (apiKey != NULL && strlen(apiKey) > 0) {
        if (speechSettings.speechSystem == SPEECH_SYSTEM_ELEVENLABS) {
            if (speechSettings.elevenLabsApiKey != NULL)
                FreeVec(speechSettings.elevenLabsApiKey);
            speechSettings.elevenLabsApiKey = rexxDupStr(apiKey);
        } else {
            if (speechSettings.openAiApiKey != NULL)
                FreeVec(speechSettings.openAiApiKey);
            speechSettings.openAiApiKey = rexxDupStr(apiKey);
        }
    }

    OpenAITTSModel model = speechSettings.openAiTtsModel;
    if (modelString != NULL && strlen(modelString) > 0) {
        for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++) {
            if (strcasecmp(modelString, OPENAI_TTS_MODEL_NAMES[i]) == 0) {
                model = i;
                break;
            }
        }
    }
    OpenAITTSVoice voice = speechSettings.openAiTtsVoice;
    if (voiceString != NULL && strlen(voiceString) > 0) {
        if (!rexxApplyWorkbenchVoiceOverride(&speechSettings, voiceString)) {
            for (UBYTE i = 0; OPENAI_TTS_VOICE_NAMES[i] != NULL; i++) {
                if (strcasecmp(voiceString, OPENAI_TTS_VOICE_NAMES[i]) == 0) {
                    voice = i;
                    break;
                }
            }
        }
    }
    AudioFormat audioFormat = AUDIO_FORMAT_MP3;
    if (audioFormatString != NULL && strlen(audioFormatString) > 0) {
        for (UBYTE i = 0; AUDIO_FORMAT_NAMES[i] != NULL; i++) {
            printf("AUDIO_FORMAT_NAMES[%d]: %s\n", i, AUDIO_FORMAT_NAMES[i]);
            if (strcasecmp(audioFormatString, AUDIO_FORMAT_NAMES[i]) == 0) {
                audioFormat = i;
                break;
            }
        }
    }

    speechSettings.openAiTtsModel = model;
    speechSettings.openAiTtsVoice = voice;
    if (instructions != NULL && strlen(instructions) > 0) {
        if (speechSettings.openAiVoiceInstructions != NULL)
            FreeVec(speechSettings.openAiVoiceInstructions);
        speechSettings.openAiVoiceInstructions = rexxDupStr(instructions);
    }

    speakTextWithSettings(prompt, output, &audioFormat, &speechSettings);
    configFreeSpeechRequestSettings(&speechSettings);
    set(app, MUIA_Application_RexxString, prompt);
    updateStatusBar(STRING_READY, greenPen);
    return RETURN_OK;
}
MakeHook(SpeakTextHook, SpeakTextFunc);

HOOKPROTONHNO(NewChatFunc, APTR, ULONG *arg) {
    clearDaemonConversation();
    set(app, MUIA_Application_RexxString, STRING_REXX_CONVERSATION_CLEARED);
    return RETURN_OK;
}
MakeHook(NewChatHook, NewChatFunc);

HOOKPROTONHNO(HelpFunc, APTR, ULONG *arg) {
    set(app, MUIA_Application_RexxString,
        "SENDMESSAGE "
        "PR=PROFILE/K,M=MODEL/K,S=SYSTEM/K,SF=SYSTEMFILE/K,H=HOST/K,P=PORT/N,"
        "S=SSL/S,K=APIKEY/"
        "K,U=USEPROXY/"
        "S,PH=PROXYHOST/"
        "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
        "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K,W=WEBSEARCH/S,P=PROMPT/F\n"
        "CREATEIMAGE "
        "PR=PROFILE/K,M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/"
        "F\n"
        "SPEAKTEXT "
        "PR=PROFILE/K,M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,"
        "O=OUTPUT/K,F=FORMAT/K,P=PROMPT/F\n"
        "LISTMODELS PR=PROFILE/K\n"
        "NEWCHAT\n"
        "LISTPROFILES\n"
        "LISTAUDIOFORMATS\n"
        "LISTIMAGESIZES\n"
        "LISTVOICES PR=PROFILE/K\n");
    return RETURN_OK;
}
MakeHook(HelpHook, HelpFunc);

struct MUI_Command arexxList[] = {
    {"SENDMESSAGE",
     "PR=PROFILE/K,M=MODEL/K,S=SYSTEM/K,SF=SYSTEMFILE/K,H=HOST/K,PO=PORT/N,"
     "S=SSL/S,K=APIKEY/"
     "K,U=USEPROXY/"
     "S,PH=PROXYHOST/"
     "K,PP=PROXYPORT/N,PS=PROXYUSESSSL/S,PA=PROXYREQUIRESAUTH/"
     "S,PU=PROXYUSERNAME/K,PP=PROXYPASSWORD/K,W=WEBSEARCH/S,P=PROMPT/F",
     17,
     &SendMessageHook,
     {0, 0, 0, 0, 0}},
    {"CREATEIMAGE",
     "PR=PROFILE/K,M=MODEL/K,S=SIZE/K,K=APIKEY/K,D=DESTINATION/K,P=PROMPT/F",
     6,
     &CreateImageHook,
     {0, 0, 0, 0, 0}},
    {"SPEAKTEXT",
     "PR=PROFILE/K,M=MODEL/K,V=VOICE/K,I=INSTRUCTIONS/K,K=APIKEY/K,"
     "O=OUTPUT/K,F=FORMAT/K,P=PROMPT/F",
     8,
     &SpeakTextHook,
     {0, 0, 0, 0, 0}},
    {"NEWCHAT", NULL, NULL, &NewChatHook, {0, 0, 0, 0, 0}},
    {"LISTAUDIOFORMATS", NULL, NULL, &ListAudioFormatsHook, {0, 0, 0, 0, 0}},
    {"LISTMODELS", "PR=PROFILE/K", 1, &ListModelsHook, {0, 0, 0, 0, 0}},
    {"LISTPROFILES", NULL, NULL, &ListProfilesHook, {0, 0, 0, 0, 0}},
    {"LISTIMAGESIZES", NULL, NULL, &ListImageSizesHook, {0, 0, 0, 0, 0}},
    {"LISTVOICES", "PR=PROFILE/K", 1, &ListVoicesHook, {0, 0, 0, 0, 0}},
    {"?", NULL, NULL, &HelpHook, {0, 0, 0, 0, 0}},
    {NULL, NULL, 0, NULL, {0, 0, 0, 0, 0}}};
