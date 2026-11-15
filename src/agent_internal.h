#ifndef AGENT_INTERNAL_H
#define AGENT_INTERNAL_H

#include "agent.h"

/**
 * Shared between agent.c, agentapi.c and agentmcp.c. Not for the rest of the
 * program.
 *
 * The session history is a json array of provider-neutral items:
 *
 *   {"type":"user","text":"..."}
 *   {"type":"assistant","text":"...","calls":[{"id","name","args":{}}],
 *    "raw":<the provider's own blocks>,"rawFormat":<APIChatEndpoint>}
 *   {"type":"tool","results":[{"id","name","output","isError"}]}
 *
 * A tool list is a json array of {"name","description","parameters"} where
 * parameters is a JSON Schema object.
 **/

/** A growable string */
struct AgentBuffer {
    STRPTR data;
    ULONG length;
    ULONG capacity;
};

void agentBufferAppend(struct AgentBuffer *buffer, CONST_STRPTR text);
void agentBufferAppendLength(struct AgentBuffer *buffer, CONST_STRPTR text,
                             ULONG length);
void agentBufferFree(struct AgentBuffer *buffer);

/** Copy a string. Free it with FreeVec() */
STRPTR agentDuplicate(CONST_STRPTR text);

/** Read a whole file. Free it with FreeVec() */
STRPTR agentReadFile(CONST_STRPTR path, LONG *sizeOut);

/** Replace a file with the given bytes */
BOOL agentWriteFile(CONST_STRPTR path, CONST_STRPTR data, ULONG length);

/**
 * Send the conversation to the active chat profile and return the model's
 * turn as a neutral assistant item.
 * @param settings the chat profile to talk to
 * @param model the model to use
 * @param system the system prompt
 * @param history the neutral history
 * @param tools the neutral tool list
 * @param effort the reasoning effort
 * @param pauseOut set to TRUE when the provider paused a long turn that
 * should be resumed by sending the history again
 * @param errorOut receives an AllocVec'd error message on failure
 * @return the assistant item or NULL on failure
 **/
struct json_object *agentApiSend(const struct ChatRequestSettings *settings,
                                 CONST_STRPTR model, CONST_STRPTR system,
                                 struct json_object *history,
                                 struct json_object *tools, AgentEffort effort,
                                 BOOL *pauseOut, STRPTR *errorOut);

/**
 * Read the MCP server files. Servers in the project file replace global
 * servers of the same name. Either path may be NULL.
 **/
void agentMcpLoad(CONST_STRPTR globalPath, CONST_STRPTR projectPath,
                  const struct ChatRequestSettings *settings);

/**
 * Connect to every server that is not connected yet and add its tools to
 * the list as mcp__<server>__<tool>
 **/
void agentMcpAppendTools(struct json_object *tools);

BOOL agentMcpOwnsTool(CONST_STRPTR name);

/**
 * Call an MCP tool
 * @return the text of the result in UTF-8. Free it with FreeVec()
 **/
STRPTR agentMcpCall(CONST_STRPTR name, struct json_object *arguments,
                    BOOL *isErrorOut);

/** Describe the servers and their state for the /mcp command */
void agentMcpDescribe(struct AgentBuffer *buffer);

void agentMcpFree(void);

#endif /* AGENT_INTERNAL_H */
