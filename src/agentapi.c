#include <json-c/json.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include "agent_internal.h"
#include "gui.h"
#include "openai.h"

/**
 * Tool calling for the four chat wire formats. Each provider gets the
 * neutral history turned into its own shape and has its answer turned back
 * into a neutral assistant item, so the loop in agent.c never sees a
 * provider.
 **/

#define AGENT_MAX_OUTPUT_TOKENS 16000
#define AGENT_SYNTHETIC_ID_PREFIX "amigagpt_"

static ULONG syntheticIdCounter = 0;

static CONST_STRPTR EFFORT_NAMES[] = {NULL, "low", "medium", "high"};

static CONST_STRPTR stringOf(struct json_object *object, CONST_STRPTR key) {
    struct json_object *value;
    CONST_STRPTR text;
    if (object == NULL || !json_object_is_type(object, json_type_object) ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string))
        return "";
    text = json_object_get_string(value);
    return text != NULL ? text : "";
}

static struct json_object *arrayOf(struct json_object *object,
                                   CONST_STRPTR key) {
    struct json_object *value;
    if (object == NULL || !json_object_is_type(object, json_type_object) ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_array))
        return NULL;
    return value;
}

static struct json_object *objectOf(struct json_object *object,
                                    CONST_STRPTR key) {
    struct json_object *value;
    if (object == NULL || !json_object_is_type(object, json_type_object) ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_object))
        return NULL;
    return value;
}

static void addString(struct json_object *object, CONST_STRPTR key,
                      CONST_STRPTR value) {
    json_object_object_add(object, key,
                           json_object_new_string(value != NULL ? value : ""));
}

static struct json_object *textObject(CONST_STRPTR key, CONST_STRPTR text) {
    struct json_object *object = json_object_new_object();
    addString(object, key, text);
    return object;
}

/* A call's arguments as the JSON text the OpenAI formats want */
static CONST_STRPTR argumentsText(struct json_object *call) {
    struct json_object *arguments = objectOf(call, "args");
    if (arguments == NULL)
        return "{}";
    return json_object_to_json_string_ext(arguments, JSON_C_TO_STRING_PLAIN);
}

/* A reference to a call's arguments object, never NULL */
static struct json_object *argumentsObject(struct json_object *call) {
    struct json_object *arguments = objectOf(call, "args");
    if (arguments == NULL)
        return json_object_new_object();
    return json_object_get(arguments);
}

/* Add a call the model made to a neutral assistant item */
static void addCall(struct json_object *calls, CONST_STRPTR id,
                    CONST_STRPTR name, struct json_object *arguments) {
    struct json_object *call = json_object_new_object();
    UBYTE generated[40];
    if (id == NULL || id[0] == '\0') {
        snprintf(generated, sizeof(generated), AGENT_SYNTHETIC_ID_PREFIX "%lu",
                 ++syntheticIdCounter);
        id = generated;
    }
    addString(call, "id", id);
    addString(call, "name", name);
    json_object_object_add(call, "args",
                           arguments != NULL ? arguments
                                             : json_object_new_object());
    json_object_array_add(calls, call);
}

/* Arguments arrive as JSON text from the OpenAI formats */
static struct json_object *parseArguments(CONST_STRPTR text) {
    struct json_object *arguments = NULL;
    if (text != NULL && text[0] != '\0')
        arguments = json_tokener_parse(text);
    if (arguments == NULL || !json_object_is_type(arguments, json_type_object)) {
        if (arguments != NULL)
            json_object_put(arguments);
        arguments = json_object_new_object();
    }
    return arguments;
}

static struct json_object *newAssistant(struct AgentBuffer *text,
                                        struct json_object *calls,
                                        struct json_object *raw,
                                        APIChatEndpoint format) {
    struct json_object *assistant = json_object_new_object();
    addString(assistant, "type", "assistant");
    addString(assistant, "text", text->data != NULL ? text->data : "");
    json_object_object_add(assistant, "calls", calls);
    if (raw != NULL) {
        json_object_object_add(assistant, "raw", json_object_get(raw));
        json_object_object_add(assistant, "rawFormat",
                               json_object_new_int((LONG)format));
    }
    agentBufferFree(text);
    return assistant;
}

/* The provider's own blocks of an assistant item, if it came from the same
 * wire format. Replaying them unchanged keeps what the neutral form drops,
 * such as Claude's thinking blocks and Gemini's thought signatures, which
 * both providers want back while tools are in use. */
static struct json_object *rawBlocks(struct json_object *item,
                                     APIChatEndpoint format) {
    struct json_object *value;
    struct json_object *raw;
    if (!json_object_object_get_ex(item, "rawFormat", &value) ||
        json_object_get_int(value) != (LONG)format)
        return NULL;
    if (!json_object_object_get_ex(item, "raw", &raw) ||
        !json_object_is_type(raw, json_type_array) ||
        json_object_array_length(raw) == 0)
        return NULL;
    return raw;
}

/* ------------------------------------------------------------------------
 * OpenAI Responses
 * ---------------------------------------------------------------------- */

static struct json_object *buildResponsesRequest(
    const struct ChatRequestSettings *settings, CONST_STRPTR model,
    CONST_STRPTR system, struct json_object *history, struct json_object *tools,
    AgentEffort effort) {
    struct json_object *request = json_object_new_object();
    struct json_object *input = json_object_new_array();
    struct json_object *toolArray = json_object_new_array();
    ULONG count = (ULONG)json_object_array_length(history);
    ULONG i, j;

    addString(request, "model", model);
    addString(request, "instructions", system);
    /* The whole history is sent each time, so nothing needs to be kept on
     * the server and hosts that do not keep responses work too */
    json_object_object_add(request, "store", json_object_new_boolean(FALSE));

    for (i = 0; i < count; i++) {
        struct json_object *item = json_object_array_get_idx(history, i);
        CONST_STRPTR type = stringOf(item, "type");
        if (strcmp(type, "user") == 0) {
            struct json_object *message = textObject("role", "user");
            addString(message, "content", stringOf(item, "text"));
            json_object_array_add(input, message);
        } else if (strcmp(type, "assistant") == 0) {
            struct json_object *calls = arrayOf(item, "calls");
            CONST_STRPTR text = stringOf(item, "text");
            if (text[0] != '\0') {
                struct json_object *message = textObject("role", "assistant");
                addString(message, "content", text);
                json_object_array_add(input, message);
            }
            for (j = 0; calls != NULL && j < json_object_array_length(calls);
                 j++) {
                struct json_object *call = json_object_array_get_idx(calls, j);
                struct json_object *entry = textObject("type", "function_call");
                addString(entry, "call_id", stringOf(call, "id"));
                addString(entry, "name", stringOf(call, "name"));
                addString(entry, "arguments", argumentsText(call));
                json_object_array_add(input, entry);
            }
        } else if (strcmp(type, "tool") == 0) {
            struct json_object *results = arrayOf(item, "results");
            for (j = 0;
                 results != NULL && j < json_object_array_length(results); j++) {
                struct json_object *result =
                    json_object_array_get_idx(results, j);
                struct json_object *entry =
                    textObject("type", "function_call_output");
                addString(entry, "call_id", stringOf(result, "id"));
                addString(entry, "output", stringOf(result, "output"));
                json_object_array_add(input, entry);
            }
        }
    }
    json_object_object_add(request, "input", input);

    for (i = 0; i < json_object_array_length(tools); i++) {
        struct json_object *tool = json_object_array_get_idx(tools, i);
        struct json_object *entry = textObject("type", "function");
        struct json_object *parameters = objectOf(tool, "parameters");
        addString(entry, "name", stringOf(tool, "name"));
        addString(entry, "description", stringOf(tool, "description"));
        if (parameters != NULL)
            json_object_object_add(entry, "parameters",
                                   json_object_get(parameters));
        json_object_array_add(toolArray, entry);
    }
    /* The hosted web search runs on the provider's side, next to our tools */
    if (settings->webSearchEnabled &&
        (hostIsOpenAICompatible(settings->host, settings->apiEndpointUrl) ||
         strcmp(settings->host, "api.x.ai") == 0))
        json_object_array_add(toolArray, textObject("type", "web_search"));
    if (json_object_array_length(toolArray) > 0)
        json_object_object_add(request, "tools", toolArray);
    else
        json_object_put(toolArray);

    if (EFFORT_NAMES[effort] != NULL)
        json_object_object_add(request, "reasoning",
                               textObject("effort", EFFORT_NAMES[effort]));
    return request;
}

static struct json_object *parseResponsesAnswer(struct json_object *response) {
    struct json_object *output = arrayOf(response, "output");
    struct json_object *calls = json_object_new_array();
    struct AgentBuffer text = {NULL, 0, 0};
    ULONG i, j;

    for (i = 0; output != NULL && i < json_object_array_length(output); i++) {
        struct json_object *item = json_object_array_get_idx(output, i);
        CONST_STRPTR type = stringOf(item, "type");
        if (strcmp(type, "message") == 0) {
            struct json_object *content = arrayOf(item, "content");
            for (j = 0;
                 content != NULL && j < json_object_array_length(content); j++) {
                struct json_object *part =
                    json_object_array_get_idx(content, j);
                CONST_STRPTR partType = stringOf(part, "type");
                if (strcmp(partType, "output_text") == 0)
                    agentBufferAppend(&text, stringOf(part, "text"));
                else if (strcmp(partType, "refusal") == 0)
                    agentBufferAppend(&text, stringOf(part, "refusal"));
            }
        } else if (strcmp(type, "function_call") == 0) {
            addCall(calls, stringOf(item, "call_id"), stringOf(item, "name"),
                    parseArguments(stringOf(item, "arguments")));
        }
    }
    return newAssistant(&text, calls, NULL, API_CHAT_ENDPOINT_RESPONSES);
}

/* ------------------------------------------------------------------------
 * OpenAI compatible chat/completions
 * ---------------------------------------------------------------------- */

static struct json_object *buildChatCompletionsRequest(
    CONST_STRPTR model, CONST_STRPTR system, struct json_object *history,
    struct json_object *tools, AgentEffort effort) {
    struct json_object *request = json_object_new_object();
    struct json_object *messages = json_object_new_array();
    struct json_object *toolArray = json_object_new_array();
    struct json_object *message;
    ULONG count = (ULONG)json_object_array_length(history);
    ULONG i, j;

    addString(request, "model", model);
    message = textObject("role", "system");
    addString(message, "content", system);
    json_object_array_add(messages, message);

    for (i = 0; i < count; i++) {
        struct json_object *item = json_object_array_get_idx(history, i);
        CONST_STRPTR type = stringOf(item, "type");
        if (strcmp(type, "user") == 0) {
            message = textObject("role", "user");
            addString(message, "content", stringOf(item, "text"));
            json_object_array_add(messages, message);
        } else if (strcmp(type, "assistant") == 0) {
            struct json_object *calls = arrayOf(item, "calls");
            CONST_STRPTR text = stringOf(item, "text");
            BOOL hasCalls = calls != NULL && json_object_array_length(calls) > 0;
            if (text[0] == '\0' && !hasCalls)
                continue;
            message = textObject("role", "assistant");
            if (text[0] != '\0')
                addString(message, "content", text);
            else
                json_object_object_add(message, "content", NULL);
            if (hasCalls) {
                struct json_object *toolCalls = json_object_new_array();
                for (j = 0; j < json_object_array_length(calls); j++) {
                    struct json_object *call =
                        json_object_array_get_idx(calls, j);
                    struct json_object *entry = textObject("type", "function");
                    struct json_object *function =
                        textObject("name", stringOf(call, "name"));
                    addString(function, "arguments", argumentsText(call));
                    addString(entry, "id", stringOf(call, "id"));
                    json_object_object_add(entry, "function", function);
                    json_object_array_add(toolCalls, entry);
                }
                json_object_object_add(message, "tool_calls", toolCalls);
            }
            json_object_array_add(messages, message);
        } else if (strcmp(type, "tool") == 0) {
            struct json_object *results = arrayOf(item, "results");
            for (j = 0;
                 results != NULL && j < json_object_array_length(results); j++) {
                struct json_object *result =
                    json_object_array_get_idx(results, j);
                message = textObject("role", "tool");
                addString(message, "tool_call_id", stringOf(result, "id"));
                addString(message, "content", stringOf(result, "output"));
                json_object_array_add(messages, message);
            }
        }
    }
    json_object_object_add(request, "messages", messages);
    json_object_object_add(request, "stream", json_object_new_boolean(FALSE));

    for (i = 0; i < json_object_array_length(tools); i++) {
        struct json_object *tool = json_object_array_get_idx(tools, i);
        struct json_object *entry = textObject("type", "function");
        struct json_object *function =
            textObject("name", stringOf(tool, "name"));
        struct json_object *parameters = objectOf(tool, "parameters");
        addString(function, "description", stringOf(tool, "description"));
        if (parameters != NULL)
            json_object_object_add(function, "parameters",
                                   json_object_get(parameters));
        json_object_object_add(entry, "function", function);
        json_object_array_add(toolArray, entry);
    }
    if (json_object_array_length(toolArray) > 0)
        json_object_object_add(request, "tools", toolArray);
    else
        json_object_put(toolArray);

    if (EFFORT_NAMES[effort] != NULL)
        addString(request, "reasoning_effort", EFFORT_NAMES[effort]);
    return request;
}

static struct json_object *
parseChatCompletionsAnswer(struct json_object *response) {
    struct json_object *choices = arrayOf(response, "choices");
    struct json_object *message =
        choices != NULL && json_object_array_length(choices) > 0
            ? objectOf(json_object_array_get_idx(choices, 0), "message")
            : NULL;
    struct json_object *toolCalls = arrayOf(message, "tool_calls");
    struct json_object *calls = json_object_new_array();
    struct AgentBuffer text = {NULL, 0, 0};
    ULONG i;

    agentBufferAppend(&text, stringOf(message, "content"));
    for (i = 0; toolCalls != NULL && i < json_object_array_length(toolCalls);
         i++) {
        struct json_object *toolCall = json_object_array_get_idx(toolCalls, i);
        struct json_object *function = objectOf(toolCall, "function");
        addCall(calls, stringOf(toolCall, "id"), stringOf(function, "name"),
                parseArguments(stringOf(function, "arguments")));
    }
    return newAssistant(&text, calls, NULL, API_CHAT_ENDPOINT_CHAT_COMPLETIONS);
}

/* ------------------------------------------------------------------------
 * Anthropic Messages
 * ---------------------------------------------------------------------- */

static struct json_object *buildMessagesRequest(
    const struct ChatRequestSettings *settings, CONST_STRPTR model,
    CONST_STRPTR system, struct json_object *history, struct json_object *tools,
    AgentEffort effort) {
    struct json_object *request = json_object_new_object();
    struct json_object *messages = json_object_new_array();
    struct json_object *toolArray = json_object_new_array();
    ULONG count = (ULONG)json_object_array_length(history);
    ULONG i, j;

    addString(request, "model", model);
    addString(request, "system", system);
    json_object_object_add(request, "max_tokens",
                           json_object_new_int(AGENT_MAX_OUTPUT_TOKENS));

    for (i = 0; i < count; i++) {
        struct json_object *item = json_object_array_get_idx(history, i);
        CONST_STRPTR type = stringOf(item, "type");
        struct json_object *message = NULL;
        if (strcmp(type, "user") == 0) {
            message = textObject("role", "user");
            addString(message, "content", stringOf(item, "text"));
        } else if (strcmp(type, "assistant") == 0) {
            struct json_object *raw =
                rawBlocks(item, API_CHAT_ENDPOINT_MESSAGES);
            struct json_object *content;
            if (raw != NULL) {
                content = json_object_get(raw);
            } else {
                struct json_object *calls = arrayOf(item, "calls");
                CONST_STRPTR text = stringOf(item, "text");
                content = json_object_new_array();
                if (text[0] != '\0') {
                    struct json_object *block = textObject("type", "text");
                    addString(block, "text", text);
                    json_object_array_add(content, block);
                }
                for (j = 0;
                     calls != NULL && j < json_object_array_length(calls); j++) {
                    struct json_object *call =
                        json_object_array_get_idx(calls, j);
                    struct json_object *block = textObject("type", "tool_use");
                    addString(block, "id", stringOf(call, "id"));
                    addString(block, "name", stringOf(call, "name"));
                    json_object_object_add(block, "input",
                                           argumentsObject(call));
                    json_object_array_add(content, block);
                }
                if (json_object_array_length(content) == 0) {
                    json_object_put(content);
                    continue;
                }
            }
            message = textObject("role", "assistant");
            json_object_object_add(message, "content", content);
        } else if (strcmp(type, "tool") == 0) {
            /* Every result of one turn goes back in a single user message */
            struct json_object *results = arrayOf(item, "results");
            struct json_object *content = json_object_new_array();
            for (j = 0;
                 results != NULL && j < json_object_array_length(results); j++) {
                struct json_object *result =
                    json_object_array_get_idx(results, j);
                struct json_object *block = textObject("type", "tool_result");
                struct json_object *isError;
                CONST_STRPTR output = stringOf(result, "output");
                addString(block, "tool_use_id", stringOf(result, "id"));
                addString(block, "content",
                          output[0] != '\0' ? output : "(No output)");
                if (json_object_object_get_ex(result, "isError", &isError) &&
                    json_object_get_boolean(isError))
                    json_object_object_add(block, "is_error",
                                           json_object_new_boolean(TRUE));
                json_object_array_add(content, block);
            }
            message = textObject("role", "user");
            json_object_object_add(message, "content", content);
        }
        if (message != NULL)
            json_object_array_add(messages, message);
    }
    json_object_object_add(request, "messages", messages);

    for (i = 0; i < json_object_array_length(tools); i++) {
        struct json_object *tool = json_object_array_get_idx(tools, i);
        struct json_object *entry = textObject("name", stringOf(tool, "name"));
        struct json_object *parameters = objectOf(tool, "parameters");
        addString(entry, "description", stringOf(tool, "description"));
        json_object_object_add(entry, "input_schema",
                               parameters != NULL
                                   ? json_object_get(parameters)
                                   : textObject("type", "object"));
        json_object_array_add(toolArray, entry);
    }
    if (settings->webSearchEnabled) {
        struct json_object *entry = textObject("type", "web_search_20250305");
        addString(entry, "name", "web_search");
        json_object_object_add(entry, "max_uses", json_object_new_int(5));
        json_object_array_add(toolArray, entry);
    }
    if (json_object_array_length(toolArray) > 0)
        json_object_object_add(request, "tools", toolArray);
    else
        json_object_put(toolArray);

    /* Older Claude models reject the effort setting, so it is only sent
     * when the user picked one */
    if (EFFORT_NAMES[effort] != NULL)
        json_object_object_add(request, "output_config",
                               textObject("effort", EFFORT_NAMES[effort]));
    return request;
}

static struct json_object *parseMessagesAnswer(struct json_object *response,
                                               BOOL *pauseOut) {
    struct json_object *content = arrayOf(response, "content");
    struct json_object *calls = json_object_new_array();
    struct AgentBuffer text = {NULL, 0, 0};
    CONST_STRPTR stopReason = stringOf(response, "stop_reason");
    ULONG i;

    for (i = 0; content != NULL && i < json_object_array_length(content); i++) {
        struct json_object *block = json_object_array_get_idx(content, i);
        CONST_STRPTR type = stringOf(block, "type");
        if (strcmp(type, "text") == 0) {
            agentBufferAppend(&text, stringOf(block, "text"));
        } else if (strcmp(type, "tool_use") == 0) {
            struct json_object *input = objectOf(block, "input");
            addCall(calls, stringOf(block, "id"), stringOf(block, "name"),
                    input != NULL ? json_object_get(input) : NULL);
        }
    }
    /* A long turn with server tools can be paused by the API. Sending the
     * history again, ending in this turn, resumes it. */
    if (strcmp(stopReason, "pause_turn") == 0)
        *pauseOut = TRUE;
    if (strcmp(stopReason, "refusal") == 0 && text.data == NULL)
        agentBufferAppend(&text, "The model declined this request.");
    return newAssistant(&text, calls, content, API_CHAT_ENDPOINT_MESSAGES);
}

/* ------------------------------------------------------------------------
 * Gemini generateContent
 * ---------------------------------------------------------------------- */

/* Gemini accepts a subset of JSON Schema and rejects requests that carry
 * keywords outside it, which MCP servers often send */
static void stripUnsupportedSchemaKeys(struct json_object *schema) {
    static CONST_STRPTR unsupported[] = {"$schema", "additionalProperties",
                                         "$id", "$defs", "definitions", NULL};
    ULONG i;
    if (schema == NULL)
        return;
    if (json_object_is_type(schema, json_type_array)) {
        for (i = 0; i < json_object_array_length(schema); i++)
            stripUnsupportedSchemaKeys(json_object_array_get_idx(schema, i));
        return;
    }
    if (!json_object_is_type(schema, json_type_object))
        return;
    for (i = 0; unsupported[i] != NULL; i++)
        json_object_object_del(schema, unsupported[i]);
    {
        json_object_object_foreach(schema, key, value) {
            (void)key;
            stripUnsupportedSchemaKeys(value);
        }
    }
}

static struct json_object *buildGeminiRequest(CONST_STRPTR system,
                                              struct json_object *history,
                                              struct json_object *tools,
                                              AgentEffort effort) {
    struct json_object *request = json_object_new_object();
    struct json_object *contents = json_object_new_array();
    struct json_object *declarations = json_object_new_array();
    struct json_object *systemInstruction = json_object_new_object();
    struct json_object *systemParts = json_object_new_array();
    ULONG count = (ULONG)json_object_array_length(history);
    ULONG i, j;

    json_object_array_add(systemParts, textObject("text", system));
    json_object_object_add(systemInstruction, "parts", systemParts);
    json_object_object_add(request, "systemInstruction", systemInstruction);

    for (i = 0; i < count; i++) {
        struct json_object *item = json_object_array_get_idx(history, i);
        CONST_STRPTR type = stringOf(item, "type");
        struct json_object *parts = NULL;
        CONST_STRPTR role = "user";
        if (strcmp(type, "user") == 0) {
            parts = json_object_new_array();
            json_object_array_add(parts,
                                  textObject("text", stringOf(item, "text")));
        } else if (strcmp(type, "assistant") == 0) {
            struct json_object *raw =
                rawBlocks(item, API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT);
            role = "model";
            if (raw != NULL) {
                parts = json_object_get(raw);
            } else {
                struct json_object *calls = arrayOf(item, "calls");
                CONST_STRPTR text = stringOf(item, "text");
                parts = json_object_new_array();
                if (text[0] != '\0')
                    json_object_array_add(parts, textObject("text", text));
                for (j = 0;
                     calls != NULL && j < json_object_array_length(calls); j++) {
                    struct json_object *call =
                        json_object_array_get_idx(calls, j);
                    struct json_object *part = json_object_new_object();
                    struct json_object *functionCall =
                        textObject("name", stringOf(call, "name"));
                    json_object_object_add(functionCall, "args",
                                           argumentsObject(call));
                    json_object_object_add(part, "functionCall", functionCall);
                    json_object_array_add(parts, part);
                }
                if (json_object_array_length(parts) == 0) {
                    json_object_put(parts);
                    continue;
                }
            }
        } else if (strcmp(type, "tool") == 0) {
            struct json_object *results = arrayOf(item, "results");
            parts = json_object_new_array();
            for (j = 0;
                 results != NULL && j < json_object_array_length(results); j++) {
                struct json_object *result =
                    json_object_array_get_idx(results, j);
                struct json_object *part = json_object_new_object();
                struct json_object *functionResponse =
                    textObject("name", stringOf(result, "name"));
                CONST_STRPTR id = stringOf(result, "id");
                /* Only ids Gemini issued itself are sent back */
                if (strncmp(id, AGENT_SYNTHETIC_ID_PREFIX,
                            strlen(AGENT_SYNTHETIC_ID_PREFIX)) != 0)
                    addString(functionResponse, "id", id);
                json_object_object_add(
                    functionResponse, "response",
                    textObject("result", stringOf(result, "output")));
                json_object_object_add(part, "functionResponse",
                                       functionResponse);
                json_object_array_add(parts, part);
            }
        }
        if (parts != NULL) {
            struct json_object *content = textObject("role", role);
            json_object_object_add(content, "parts", parts);
            json_object_array_add(contents, content);
        }
    }
    json_object_object_add(request, "contents", contents);

    for (i = 0; i < json_object_array_length(tools); i++) {
        struct json_object *tool = json_object_array_get_idx(tools, i);
        struct json_object *declaration =
            textObject("name", stringOf(tool, "name"));
        struct json_object *parameters = objectOf(tool, "parameters");
        struct json_object *properties = objectOf(parameters, "properties");
        addString(declaration, "description", stringOf(tool, "description"));
        /* An object schema with no properties is rejected, so a tool that
         * takes no arguments is declared without parameters */
        if (properties != NULL && json_object_object_length(properties) > 0) {
            struct json_object *copy = json_tokener_parse(
                json_object_to_json_string_ext(parameters,
                                               JSON_C_TO_STRING_PLAIN));
            if (copy != NULL) {
                stripUnsupportedSchemaKeys(copy);
                json_object_object_add(declaration, "parameters", copy);
            }
        }
        json_object_array_add(declarations, declaration);
    }
    if (json_object_array_length(declarations) > 0) {
        struct json_object *toolArray = json_object_new_array();
        struct json_object *entry = json_object_new_object();
        json_object_object_add(entry, "functionDeclarations", declarations);
        json_object_array_add(toolArray, entry);
        json_object_object_add(request, "tools", toolArray);
    } else {
        json_object_put(declarations);
    }

    if (EFFORT_NAMES[effort] != NULL) {
        struct json_object *generationConfig = json_object_new_object();
        json_object_object_add(generationConfig, "thinkingConfig",
                               textObject("thinkingLevel",
                                          EFFORT_NAMES[effort]));
        json_object_object_add(request, "generationConfig", generationConfig);
    }
    return request;
}

static struct json_object *parseGeminiAnswer(struct json_object *response) {
    struct json_object *candidates = arrayOf(response, "candidates");
    struct json_object *content =
        candidates != NULL && json_object_array_length(candidates) > 0
            ? objectOf(json_object_array_get_idx(candidates, 0), "content")
            : NULL;
    struct json_object *parts = arrayOf(content, "parts");
    struct json_object *calls = json_object_new_array();
    struct AgentBuffer text = {NULL, 0, 0};
    ULONG i;

    for (i = 0; parts != NULL && i < json_object_array_length(parts); i++) {
        struct json_object *part = json_object_array_get_idx(parts, i);
        struct json_object *functionCall = objectOf(part, "functionCall");
        struct json_object *thought;
        if (functionCall != NULL) {
            struct json_object *arguments = objectOf(functionCall, "args");
            addCall(calls, stringOf(functionCall, "id"),
                    stringOf(functionCall, "name"),
                    arguments != NULL ? json_object_get(arguments) : NULL);
        } else if (!(json_object_object_get_ex(part, "thought", &thought) &&
                     json_object_get_boolean(thought))) {
            agentBufferAppend(&text, stringOf(part, "text"));
        }
    }
    return newAssistant(&text, calls, parts,
                        API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT);
}

/* ------------------------------------------------------------------------
 * Sending
 * ---------------------------------------------------------------------- */

static STRPTR errorText(CONST_STRPTR text) { return agentDuplicate(text); }

struct json_object *agentApiSend(const struct ChatRequestSettings *settings,
                                 CONST_STRPTR model, CONST_STRPTR system,
                                 struct json_object *history,
                                 struct json_object *tools, AgentEffort effort,
                                 BOOL *pauseOut, STRPTR *errorOut) {
    struct json_object *request;
    struct json_object *response;
    struct json_object *assistant = NULL;
    struct json_object *error;
    struct AgentBuffer headers = {NULL, 0, 0};
    UBYTE path[512];
    CONST_STRPTR endpointUrl;
    CONST_STRPTR body;
    STRPTR responseBody;
    LONG status = 0;

    *pauseOut = FALSE;
    *errorOut = NULL;
    if (effort < 0 || effort >= AGENT_EFFORT_COUNT)
        effort = AGENT_EFFORT_DEFAULT;
    if (settings == NULL || settings->host == NULL ||
        settings->host[0] == '\0') {
        *errorOut = errorText("No host is set for the active chat profile.");
        return NULL;
    }
    if (model == NULL || model[0] == '\0') {
        *errorOut = errorText("No model is selected.");
        return NULL;
    }
    endpointUrl =
        settings->apiEndpointUrl != NULL ? settings->apiEndpointUrl : "";

    switch (settings->apiEndpoint) {
    case API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT:
        request = buildGeminiRequest(system, history, tools, effort);
        snprintf(path, sizeof(path), "%s%s/models/%s:generateContent",
                 endpointUrl[0] != '\0' ? "/" : "", endpointUrl, model);
        break;
    case API_CHAT_ENDPOINT_MESSAGES:
        request =
            buildMessagesRequest(settings, model, system, history, tools, effort);
        snprintf(path, sizeof(path), "%s%s/messages",
                 endpointUrl[0] != '\0' ? "/" : "", endpointUrl);
        break;
    case API_CHAT_ENDPOINT_CHAT_COMPLETIONS:
        request =
            buildChatCompletionsRequest(model, system, history, tools, effort);
        snprintf(path, sizeof(path), "%s%s/chat/completions",
                 endpointUrl[0] != '\0' ? "/" : "", endpointUrl);
        break;
    default:
        request = buildResponsesRequest(settings, model, system, history, tools,
                                        effort);
        snprintf(path, sizeof(path), "%s%s/responses",
                 endpointUrl[0] != '\0' ? "/" : "", endpointUrl);
        break;
    }

    agentBufferAppend(&headers, "Content-Type: application/json\r\n");
    if (settings->authorizationType != AUTHORIZATION_TYPE_NONE &&
        settings->apiKey != NULL && settings->apiKey[0] != '\0') {
        agentBufferAppend(&headers,
                          AUTHORIZATION_TYPE_NAMES[settings->authorizationType]);
        agentBufferAppend(&headers, " ");
        agentBufferAppend(&headers, settings->apiKey);
        agentBufferAppend(&headers, "\r\n");
    }
    if (settings->customHeaders != NULL && settings->customHeaders[0] != '\0') {
        agentBufferAppend(&headers, settings->customHeaders);
        agentBufferAppend(&headers, "\r\n");
    }

    body = json_object_to_json_string_ext(
        request, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE);
    responseBody = httpSendRequest(
        "POST", settings->host, settings->port, settings->useSSL, path,
        headers.data, body, settings->useProxy, settings->proxyHost,
        settings->proxyPort, settings->proxyUsesSSL,
        settings->proxyRequiresAuth, settings->proxyUsername,
        settings->proxyPassword, &status, NULL, 0);
    json_object_put(request);
    agentBufferFree(&headers);

    if (responseBody == NULL) {
        if (!isRequestCancelled())
            *errorOut = errorText("No response was received from the server.");
        return NULL;
    }
    response = json_tokener_parse(responseBody);
    if (response == NULL || !json_object_is_type(response, json_type_object)) {
        UBYTE message[96];
        snprintf(message, sizeof(message),
                 "The server sent an answer that could not be read (HTTP %ld).",
                 status);
        *errorOut = errorText(message);
        if (response != NULL)
            json_object_put(response);
        FreeVec(responseBody);
        return NULL;
    }
    FreeVec(responseBody);

    if ((json_object_object_get_ex(response, "error", &error) &&
         error != NULL) ||
        status >= 400) {
        UTF8 *apiMessage = getApiErrorMessageFromJson(response);
        if (apiMessage != NULL && apiMessage[0] != '\0') {
            *errorOut = errorText((CONST_STRPTR)apiMessage);
        } else {
            UBYTE message[64];
            snprintf(message, sizeof(message), "The request failed (HTTP %ld).",
                     status);
            *errorOut = errorText(message);
        }
        json_object_put(response);
        return NULL;
    }

    switch (settings->apiEndpoint) {
    case API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT:
        assistant = parseGeminiAnswer(response);
        break;
    case API_CHAT_ENDPOINT_MESSAGES:
        assistant = parseMessagesAnswer(response, pauseOut);
        break;
    case API_CHAT_ENDPOINT_CHAT_COMPLETIONS:
        assistant = parseChatCompletionsAnswer(response);
        break;
    default:
        assistant = parseResponsesAnswer(response);
        break;
    }
    json_object_put(response);
    return assistant;
}
