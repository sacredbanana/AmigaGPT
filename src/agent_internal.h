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
 *   {"type":"tool","results":[{"id","name","output","isError",
 *    "image":{"mime","data"}}]}
 *
 * "image" is a picture the tool produced, base64 encoded, which is shown to
 * the model. Only the newest one is kept, because each is large.
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

/**
 * Take a picture of the front screen, scaled down to a size a model can use
 * @param base64Out receives the PNG file, base64 encoded. Free it with
 * FreeVec()
 * @param description receives a sentence about the picture for the model
 * @param savePath a file to write the PNG to as well, or NULL
 * @return TRUE if the picture was taken
 **/
BOOL agentDesktopScreenshot(STRPTR *base64Out, STRPTR description,
                            ULONG descriptionSize, CONST_STRPTR savePath);

/**
 * Move the pointer on the front screen and use a mouse button
 * @param action move, click, double_click or drag
 * @param button left, right or middle
 * @param x,y the position in pixels of the screenshot picture
 * @param toX,toY where a drag ends
 * @param error receives the reason on failure
 **/
BOOL agentDesktopMouse(CONST_STRPTR action, CONST_STRPTR button, LONG x,
                       LONG y, LONG toX, LONG toY, STRPTR error,
                       ULONG errorSize);

/**
 * Press keys as if they were typed
 * @param text text to type in the local character set, or NULL
 * @param key a named key or a single character, or NULL
 * @param qualifiers IEQUALIFIER_ flags held with the key
 * @param repeat how often to press the key
 * @param error receives the reason on failure, or a note about characters
 * that could not be typed
 **/
BOOL agentDesktopKeyboard(CONST_STRPTR text, CONST_STRPTR key,
                          UWORD qualifiers, LONG repeat, STRPTR error,
                          ULONG errorSize);

#endif /* AGENT_INTERNAL_H */
