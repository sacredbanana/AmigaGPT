#include <json-c/json.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>
#include "agent_internal.h"
#include "gui.h"
#include "openai.h"

/**
 * A Model Context Protocol client for remote servers, speaking JSON-RPC
 * over the Streamable HTTP transport. Servers that are started as a local
 * program and spoken to over its standard input and output are not
 * supported: they are Node or Python programs, which do not run here.
 *
 * Servers are listed in mcp.json in the layout other agents use:
 *
 *   { "mcpServers": {
 *       "docs": { "url": "https://example.com/mcp",
 *                 "headers": { "Authorization": "Bearer ..." } } } }
 **/

#define MCP_PROTOCOL_VERSION "2025-06-18"
#define MCP_TOOL_PREFIX "mcp__"
#define MCP_MAX_TOOL_PAGES 5
#define MCP_MAX_TOOL_NAME 64

/* Array of {"name","url","headers","sessionId","tools","state","error"} */
static struct json_object *servers = NULL;
static struct ChatRequestSettings proxySettings;
static ULONG requestId = 0;

static CONST_STRPTR mcpString(struct json_object *object, CONST_STRPTR key) {
    struct json_object *value;
    CONST_STRPTR text;
    if (object == NULL || !json_object_is_type(object, json_type_object) ||
        !json_object_object_get_ex(object, key, &value) ||
        !json_object_is_type(value, json_type_string))
        return "";
    text = json_object_get_string(value);
    return text != NULL ? text : "";
}

static void mcpSetString(struct json_object *object, CONST_STRPTR key,
                         CONST_STRPTR value) {
    json_object_object_add(object, key,
                           json_object_new_string(value != NULL ? value : ""));
}

/* Tool names may only hold letters, digits, underscores and hyphens */
static void mcpSanitize(STRPTR name) {
    STRPTR p;
    for (p = name; *p != '\0'; p++) {
        UBYTE c = (UBYTE)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            *p = '_';
    }
}

void agentMcpFree(void) {
    if (servers != NULL)
        json_object_put(servers);
    servers = NULL;
}

static struct json_object *findServer(CONST_STRPTR name) {
    ULONG i;
    for (i = 0; servers != NULL && i < json_object_array_length(servers); i++) {
        struct json_object *server = json_object_array_get_idx(servers, i);
        if (strcmp(mcpString(server, "name"), name) == 0)
            return server;
    }
    return NULL;
}

static void loadServerFile(CONST_STRPTR path, struct json_object *previous) {
    STRPTR text;
    struct json_object *root;
    struct json_object *list;

    if (path == NULL)
        return;
    text = agentReadFile(path, NULL);
    if (text == NULL)
        return;
    root = json_tokener_parse(text);
    FreeVec(text);
    if (root == NULL)
        return;
    if (json_object_object_get_ex(root, "mcpServers", &list) &&
        json_object_is_type(list, json_type_object)) {
        json_object_object_foreach(list, name, definition) {
            struct json_object *server;
            struct json_object *headers;
            struct json_object *existing;
            UBYTE safeName[40];
            CONST_STRPTR url = mcpString(definition, "url");
            ULONG i;

            if (url[0] == '\0')
                continue;
            strncpy(safeName, name, sizeof(safeName) - 1);
            safeName[sizeof(safeName) - 1] = '\0';
            mcpSanitize(safeName);

            /* A project server replaces a global one of the same name */
            existing = findServer(safeName);
            if (existing != NULL) {
                for (i = 0; i < json_object_array_length(servers); i++) {
                    if (json_object_array_get_idx(servers, i) == existing) {
                        json_object_array_del_idx(servers, i, 1);
                        break;
                    }
                }
            }

            /* Keep the connection of a server that has not changed, so each
             * turn does not have to initialise it and list its tools again */
            server = NULL;
            for (i = 0;
                 previous != NULL && i < json_object_array_length(previous);
                 i++) {
                struct json_object *old =
                    json_object_array_get_idx(previous, i);
                struct json_object *oldHeaders = NULL;
                struct json_object *newHeaders = NULL;
                json_object_object_get_ex(old, "headers", &oldHeaders);
                json_object_object_get_ex(definition, "headers", &newHeaders);
                if (strcmp(mcpString(old, "name"), safeName) == 0 &&
                    strcmp(mcpString(old, "url"), url) == 0 &&
                    json_object_equal(oldHeaders, newHeaders)) {
                    server = json_object_get(old);
                    break;
                }
            }
            if (server == NULL) {
                server = json_object_new_object();
                mcpSetString(server, "name", safeName);
                mcpSetString(server, "url", url);
                mcpSetString(server, "state", "new");
                if (json_object_object_get_ex(definition, "headers",
                                              &headers) &&
                    json_object_is_type(headers, json_type_object))
                    json_object_object_add(server, "headers",
                                           json_object_get(headers));
            }
            json_object_array_add(servers, server);
        }
    }
    json_object_put(root);
}

void agentMcpLoad(CONST_STRPTR globalPath, CONST_STRPTR projectPath,
                  const struct ChatRequestSettings *settings) {
    struct json_object *previous = servers;
    servers = json_object_new_array();
    if (settings != NULL)
        proxySettings = *settings;
    else
        memset(&proxySettings, 0, sizeof(proxySettings));
    loadServerFile(globalPath, previous);
    loadServerFile(projectPath, previous);
    if (previous != NULL)
        json_object_put(previous);
}

/* Pick the JSON-RPC answer out of a body, which is either plain JSON or a
 * server-sent event stream whose data lines hold JSON messages */
static struct json_object *parseRpcBody(CONST_STRPTR body, CONST_STRPTR headers) {
    struct json_object *message = NULL;
    CONST_STRPTR line = body;

    while (*line == ' ' || *line == '\r' || *line == '\n')
        line++;
    if (*line == '{')
        return json_tokener_parse(line);
    (void)headers;

    while (line != NULL && *line != '\0') {
        if (strncmp(line, "data:", 5) == 0) {
            CONST_STRPTR data = line + 5;
            CONST_STRPTR end = strchr(data, '\n');
            ULONG length = end != NULL ? (ULONG)(end - data) : strlen(data);
            STRPTR copy = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
            if (copy != NULL) {
                struct json_object *candidate;
                struct json_object *ignored;
                CopyMem((APTR)data, copy, length);
                candidate = json_tokener_parse(copy);
                FreeVec(copy);
                /* Notifications and server requests can come first; the
                 * answer is the message with a result or an error */
                if (candidate != NULL &&
                    (json_object_object_get_ex(candidate, "result", &ignored) ||
                     json_object_object_get_ex(candidate, "error", &ignored))) {
                    if (message != NULL)
                        json_object_put(message);
                    message = candidate;
                } else if (candidate != NULL) {
                    json_object_put(candidate);
                }
            }
        }
        line = strchr(line, '\n');
        if (line != NULL)
            line++;
    }
    return message;
}

/**
 * Send one JSON-RPC message to a server
 * @param method the method name
 * @param params the parameters, which this call takes ownership of, or NULL
 * @param isNotification TRUE when no answer is expected
 * @param errorOut receives a short error description
 * @return the "result" object, which the caller must put, or NULL
 **/
static struct json_object *mcpRequest(struct json_object *server,
                                      CONST_STRPTR method,
                                      struct json_object *params,
                                      BOOL isNotification, STRPTR errorOut,
                                      ULONG errorSize) {
    struct json_object *message = json_object_new_object();
    struct json_object *answer;
    struct json_object *result = NULL;
    struct json_object *error;
    struct json_object *customHeaders;
    struct AgentBuffer headers = {NULL, 0, 0};
    UBYTE host[256];
    UBYTE path[1024];
    UBYTE responseHeaders[2048];
    UBYTE sessionId[256];
    UWORD port = 0;
    BOOL useSSL = TRUE;
    LONG status = 0;
    CONST_STRPTR body;
    STRPTR responseBody;

    if (errorOut != NULL && errorSize > 0)
        errorOut[0] = '\0';
    if (!parseHttpUrl(mcpString(server, "url"), host, sizeof(host), &port, path,
                      sizeof(path), &useSSL)) {
        json_object_put(message);
        if (params != NULL)
            json_object_put(params);
        if (errorOut != NULL)
            snprintf(errorOut, errorSize, "The server address is not valid.");
        return NULL;
    }

    mcpSetString(message, "jsonrpc", "2.0");
    if (!isNotification)
        json_object_object_add(message, "id",
                               json_object_new_int((LONG)++requestId));
    mcpSetString(message, "method", method);
    if (params != NULL)
        json_object_object_add(message, "params", params);

    agentBufferAppend(&headers,
                      "Content-Type: application/json\r\n"
                      "Accept: application/json, text/event-stream\r\n");
    if (strcmp(method, "initialize") != 0)
        agentBufferAppend(&headers,
                          "MCP-Protocol-Version: " MCP_PROTOCOL_VERSION "\r\n");
    if (mcpString(server, "sessionId")[0] != '\0') {
        agentBufferAppend(&headers, "Mcp-Session-Id: ");
        agentBufferAppend(&headers, mcpString(server, "sessionId"));
        agentBufferAppend(&headers, "\r\n");
    }
    if (json_object_object_get_ex(server, "headers", &customHeaders) &&
        json_object_is_type(customHeaders, json_type_object)) {
        json_object_object_foreach(customHeaders, name, value) {
            CONST_STRPTR text = json_object_get_string(value);
            agentBufferAppend(&headers, name);
            agentBufferAppend(&headers, ": ");
            agentBufferAppend(&headers, text != NULL ? text : "");
            agentBufferAppend(&headers, "\r\n");
        }
    }

    body = json_object_to_json_string_ext(
        message, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE);
    responseBody = httpSendRequest(
        "POST", host, port, useSSL, path, headers.data, body,
        proxySettings.useProxy, proxySettings.proxyHost,
        proxySettings.proxyPort, proxySettings.proxyUsesSSL,
        proxySettings.proxyRequiresAuth, proxySettings.proxyUsername,
        proxySettings.proxyPassword, &status, responseHeaders,
        sizeof(responseHeaders));
    json_object_put(message);
    agentBufferFree(&headers);

    if (responseBody == NULL) {
        if (errorOut != NULL)
            snprintf(errorOut, errorSize, "The server did not answer.");
        return NULL;
    }

    /* The server names the session in its answer to "initialize" */
    {
        CONST_STRPTR line = responseHeaders;
        while (line != NULL && *line != '\0') {
            if (strncasecmp(line, "Mcp-Session-Id:", 15) == 0) {
                CONST_STRPTR value = line + 15;
                ULONG i = 0;
                while (*value == ' ')
                    value++;
                while (value[i] != '\0' && value[i] != '\r' &&
                       value[i] != '\n' && i + 1 < sizeof(sessionId)) {
                    sessionId[i] = value[i];
                    i++;
                }
                sessionId[i] = '\0';
                mcpSetString(server, "sessionId", sessionId);
                break;
            }
            line = strchr(line, '\n');
            if (line != NULL)
                line++;
        }
    }

    if (isNotification) {
        FreeVec(responseBody);
        return NULL;
    }
    if (status == 401 || status == 403) {
        FreeVec(responseBody);
        if (errorOut != NULL)
            snprintf(errorOut, errorSize,
                     "The server refused access (HTTP %ld). Check the "
                     "headers in mcp.json.",
                     status);
        return NULL;
    }

    answer = parseRpcBody(responseBody, responseHeaders);
    FreeVec(responseBody);
    if (answer == NULL) {
        if (errorOut != NULL)
            snprintf(errorOut, errorSize,
                     "The server sent an answer that could not be read "
                     "(HTTP %ld).",
                     status);
        return NULL;
    }
    if (json_object_object_get_ex(answer, "error", &error) && error != NULL) {
        if (errorOut != NULL)
            snprintf(errorOut, errorSize, "%s", mcpString(error, "message"));
        if (errorOut != NULL && errorOut[0] == '\0')
            snprintf(errorOut, errorSize, "The server reported an error.");
    } else if (json_object_object_get_ex(answer, "result", &result) &&
               result != NULL) {
        json_object_get(result);
    }
    json_object_put(answer);
    return result;
}

/* Initialise a server and fetch its tool list */
static void connectServer(struct json_object *server) {
    struct json_object *params = json_object_new_object();
    struct json_object *clientInfo = json_object_new_object();
    struct json_object *result;
    struct json_object *tools = json_object_new_array();
    UBYTE error[256];
    UBYTE cursor[512];
    ULONG page;

    mcpSetString(params, "protocolVersion", MCP_PROTOCOL_VERSION);
    json_object_object_add(params, "capabilities", json_object_new_object());
    mcpSetString(clientInfo, "name", "AmigaGPT");
    mcpSetString(clientInfo, "version", "1.0");
    json_object_object_add(params, "clientInfo", clientInfo);

    result = mcpRequest(server, "initialize", params, FALSE, error,
                        sizeof(error));
    if (result == NULL) {
        mcpSetString(server, "state", "failed");
        mcpSetString(server, "error", error);
        json_object_put(tools);
        return;
    }
    json_object_put(result);
    mcpRequest(server, "notifications/initialized", NULL, TRUE, NULL, 0);

    cursor[0] = '\0';
    for (page = 0; page < MCP_MAX_TOOL_PAGES; page++) {
        struct json_object *list;
        ULONG i;
        params = NULL;
        if (cursor[0] != '\0') {
            params = json_object_new_object();
            mcpSetString(params, "cursor", cursor);
        }
        result = mcpRequest(server, "tools/list", params, FALSE, error,
                            sizeof(error));
        if (result == NULL)
            break;
        if (json_object_object_get_ex(result, "tools", &list) &&
            json_object_is_type(list, json_type_array)) {
            for (i = 0; i < json_object_array_length(list); i++)
                json_object_array_add(
                    tools, json_object_get(json_object_array_get_idx(list, i)));
        }
        strncpy(cursor, mcpString(result, "nextCursor"), sizeof(cursor) - 1);
        cursor[sizeof(cursor) - 1] = '\0';
        json_object_put(result);
        if (cursor[0] == '\0')
            break;
    }
    json_object_object_add(server, "tools", tools);
    mcpSetString(server, "state", "connected");
    mcpSetString(server, "error", "");
}

/* The name a server's tool goes by in the agent's tool list */
static void exposedToolName(struct json_object *server, CONST_STRPTR toolName,
                            STRPTR out, ULONG size) {
    snprintf(out, size, MCP_TOOL_PREFIX "%s__%s", mcpString(server, "name"),
             toolName);
    if (size > MCP_MAX_TOOL_NAME)
        out[MCP_MAX_TOOL_NAME] = '\0';
    mcpSanitize(out);
}

void agentMcpAppendTools(struct json_object *tools) {
    ULONG i, j;
    for (i = 0; servers != NULL && i < json_object_array_length(servers); i++) {
        struct json_object *server = json_object_array_get_idx(servers, i);
        struct json_object *serverTools;
        if (strcmp(mcpString(server, "state"), "new") == 0 &&
            !isRequestCancelled())
            connectServer(server);
        if (!json_object_object_get_ex(server, "tools", &serverTools) ||
            !json_object_is_type(serverTools, json_type_array))
            continue;
        for (j = 0; j < json_object_array_length(serverTools); j++) {
            struct json_object *serverTool =
                json_object_array_get_idx(serverTools, j);
            struct json_object *tool = json_object_new_object();
            struct json_object *schema;
            UBYTE name[MCP_MAX_TOOL_NAME + 8];
            exposedToolName(server, mcpString(serverTool, "name"), name,
                            sizeof(name));
            mcpSetString(tool, "name", name);
            mcpSetString(tool, "description",
                         mcpString(serverTool, "description"));
            if (json_object_object_get_ex(serverTool, "inputSchema", &schema) &&
                json_object_is_type(schema, json_type_object))
                json_object_object_add(tool, "parameters",
                                       json_object_get(schema));
            else
                json_object_object_add(
                    tool, "parameters",
                    json_tokener_parse(
                        "{\"type\":\"object\",\"properties\":{}}"));
            json_object_array_add(tools, tool);
        }
    }
}

BOOL agentMcpOwnsTool(CONST_STRPTR name) {
    return name != NULL &&
           strncmp(name, MCP_TOOL_PREFIX, strlen(MCP_TOOL_PREFIX)) == 0;
}

STRPTR agentMcpCall(CONST_STRPTR name, struct json_object *arguments,
                    BOOL *isErrorOut) {
    ULONG i, j;
    *isErrorOut = TRUE;
    for (i = 0; servers != NULL && i < json_object_array_length(servers); i++) {
        struct json_object *server = json_object_array_get_idx(servers, i);
        struct json_object *serverTools;
        if (!json_object_object_get_ex(server, "tools", &serverTools) ||
            !json_object_is_type(serverTools, json_type_array))
            continue;
        for (j = 0; j < json_object_array_length(serverTools); j++) {
            struct json_object *serverTool =
                json_object_array_get_idx(serverTools, j);
            struct json_object *params;
            struct json_object *result;
            struct json_object *content;
            struct json_object *flag;
            struct AgentBuffer text = {NULL, 0, 0};
            UBYTE exposed[MCP_MAX_TOOL_NAME + 8];
            UBYTE error[256];
            ULONG k;

            exposedToolName(server, mcpString(serverTool, "name"), exposed,
                            sizeof(exposed));
            if (strcmp(exposed, name) != 0)
                continue;

            params = json_object_new_object();
            mcpSetString(params, "name", mcpString(serverTool, "name"));
            json_object_object_add(params, "arguments",
                                   arguments != NULL
                                       ? json_object_get(arguments)
                                       : json_object_new_object());
            result = mcpRequest(server, "tools/call", params, FALSE, error,
                                sizeof(error));
            if (result == NULL) {
                agentBufferAppend(&text, "Error: ");
                agentBufferAppend(&text, error);
                return text.data != NULL ? text.data : agentDuplicate("Error");
            }
            if (json_object_object_get_ex(result, "content", &content) &&
                json_object_is_type(content, json_type_array)) {
                for (k = 0; k < json_object_array_length(content); k++) {
                    struct json_object *block =
                        json_object_array_get_idx(content, k);
                    CONST_STRPTR type = mcpString(block, "type");
                    if (strcmp(type, "text") == 0) {
                        agentBufferAppend(&text, mcpString(block, "text"));
                        agentBufferAppend(&text, "\n");
                    } else if (type[0] != '\0') {
                        agentBufferAppend(&text, "[");
                        agentBufferAppend(&text, type);
                        agentBufferAppend(&text, " content was not shown]\n");
                    }
                }
            }
            *isErrorOut = json_object_object_get_ex(result, "isError", &flag) &&
                          json_object_get_boolean(flag)
                              ? TRUE
                              : FALSE;
            json_object_put(result);
            return text.data != NULL ? text.data : agentDuplicate("(No output)");
        }
    }
    return agentDuplicate("Error: this MCP tool is no longer available.");
}

void agentMcpDescribe(struct AgentBuffer *buffer) {
    ULONG i, j;
    for (i = 0; servers != NULL && i < json_object_array_length(servers); i++) {
        struct json_object *server = json_object_array_get_idx(servers, i);
        struct json_object *serverTools;
        agentBufferAppend(buffer, "- ");
        agentBufferAppend(buffer, mcpString(server, "name"));
        agentBufferAppend(buffer, " (");
        agentBufferAppend(buffer, mcpString(server, "url"));
        agentBufferAppend(buffer, "): ");
        agentBufferAppend(buffer, mcpString(server, "state"));
        if (mcpString(server, "error")[0] != '\0') {
            agentBufferAppend(buffer, " - ");
            agentBufferAppend(buffer, mcpString(server, "error"));
        }
        agentBufferAppend(buffer, "\n");
        if (!json_object_object_get_ex(server, "tools", &serverTools) ||
            !json_object_is_type(serverTools, json_type_array))
            continue;
        for (j = 0; j < json_object_array_length(serverTools); j++) {
            agentBufferAppend(buffer, "    ");
            agentBufferAppend(
                buffer,
                mcpString(json_object_array_get_idx(serverTools, j), "name"));
            agentBufferAppend(buffer, "\n");
        }
    }
}
