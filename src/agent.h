#ifndef AGENT_H
#define AGENT_H

#include <exec/types.h>
#include "openai.h"
#include "AmigaGPTConfig.h"

struct json_object;

/**
 * Agent mode: the model works towards a goal by calling tools (AmigaDOS
 * shell, files, ARexx, skills, subagents and MCP servers) in a loop until it
 * has an answer. Everything the agent stores lives as plain files so it can
 * be read and edited with any text editor:
 *
 *   AMIGAGPT:agent/settings.json    approval mode, effort, model, projects
 *   AMIGAGPT:agent/session.json     the current session history
 *   AMIGAGPT:agent/AGENTS.md        instructions for every project
 *   AMIGAGPT:agent/mcp.json         MCP servers for every project
 *   AMIGAGPT:agent/skills/<name>/SKILL.md
 *   AMIGAGPT:agent/commands/<name>.md
 *   AMIGAGPT:agent/agents/<name>.md
 *
 * A project is a drawer. It may hold its own AGENTS.md, and .agents/skills,
 * .agents/commands, .agents/agents and .agents/mcp.json (.claude/ is read
 * too), which are added to the global ones while the project is active.
 **/
#define AGENT_DIR "AMIGAGPT:agent"
#define AGENT_SETTINGS_PATH AGENT_DIR "/settings.json"
#define AGENT_SESSION_PATH AGENT_DIR "/session.json"
#define AGENT_GLOBAL_INSTRUCTIONS_PATH AGENT_DIR "/AGENTS.md"
#define AGENT_GLOBAL_MCP_PATH AGENT_DIR "/mcp.json"
#define AGENT_GLOBAL_SKILLS_DIR AGENT_DIR "/skills"
#define AGENT_GLOBAL_COMMANDS_DIR AGENT_DIR "/commands"
#define AGENT_GLOBAL_AGENTS_DIR AGENT_DIR "/agents"

#define AGENT_PATH_LENGTH 512
#define AGENT_TOOL_OUTPUT_MAX 16000
#define AGENT_DEFAULT_MAX_STEPS 30

/**
 * How much the agent may do without asking
 **/
typedef enum {
    /* Reading is free. Shell, ARexx, MCP and file changes ask first */
    AGENT_APPROVAL_ASK = 0,
    /* Nothing that changes the system is allowed to run */
    AGENT_APPROVAL_READ_ONLY,
    /* File writes and edits are allowed. Shell, ARexx and MCP ask first */
    AGENT_APPROVAL_ACCEPT_EDITS,
    /* Everything runs without asking */
    AGENT_APPROVAL_FULL_AUTO,
    AGENT_APPROVAL_COUNT
} AgentApprovalMode;

/**
 * Reasoning effort requested from the model. Default sends nothing and
 * leaves the choice to the provider
 **/
typedef enum {
    AGENT_EFFORT_DEFAULT = 0,
    AGENT_EFFORT_LOW,
    AGENT_EFFORT_MEDIUM,
    AGENT_EFFORT_HIGH,
    AGENT_EFFORT_COUNT
} AgentEffort;

/**
 * Receives text for the transcript as the agent produces it
 * @param text UTF-8 text to append
 **/
typedef void (*AgentOutputFunc)(CONST_STRPTR text);

/** Convert UTF-8 to the local character set. Free it with FreeVec() */
STRPTR agentToLocal(CONST_STRPTR utf8);

/** Convert the local character set to UTF-8. Free it with FreeVec() */
STRPTR agentToUtf8(CONST_STRPTR local);

/**
 * Create the agent drawers, load the settings and the saved session
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG agentInit(void);

/**
 * Save the session and free everything the agent holds
 **/
void agentShutdown(void);

/**
 * The window requesters open over, or NULL for none
 **/
void agentSetRequesterWindow(APTR windowObject);

AgentApprovalMode agentGetApprovalMode(void);
void agentSetApprovalMode(AgentApprovalMode mode);
AgentEffort agentGetEffort(void);
void agentSetEffort(AgentEffort effort);

/**
 * The model the agent uses. An empty string means the chat model of the
 * active chat profile
 **/
CONST_STRPTR agentGetModel(void);
void agentSetModel(CONST_STRPTR model);

/**
 * The model a request would use right now
 **/
CONST_STRPTR agentEffectiveModel(void);

ULONG agentProjectCount(void);
CONST_STRPTR agentProjectName(ULONG index);
CONST_STRPTR agentProjectPath(ULONG index);

/**
 * @return the index of the active project or -1 when there is none
 **/
LONG agentGetActiveProject(void);
void agentSetActiveProject(LONG index);

/**
 * Add a drawer as a project and make it the active one
 * @param path the drawer in the local character set
 * @return the index of the project or -1 on failure
 **/
LONG agentAddProject(CONST_STRPTR path);
void agentRemoveProject(ULONG index);

/**
 * The instructions file of the active project, or the global one when no
 * project is active. The file is created empty if it does not exist.
 * @param buffer receives the path
 **/
void agentInstructionsPath(STRPTR buffer, ULONG size);

/**
 * The MCP server file of the active project, or the global one when no
 * project is active. A file with an empty server list is created if it does
 * not exist.
 * @param buffer receives the path
 **/
void agentMcpConfigPath(STRPTR buffer, ULONG size);

/**
 * Copy a skill into the global skills drawer
 * @param source a drawer holding SKILL.md, a SKILL.md file, or an http(s)
 * URL of a SKILL.md file, in the local character set
 * @param nameOut receives the name of the skill
 * @return TRUE if the skill was imported
 **/
BOOL agentImportSkill(CONST_STRPTR source, STRPTR nameOut, ULONG nameSize);

/**
 * Forget the current session
 **/
void agentNewSession(void);

/**
 * Write the user and assistant text of the saved session to output
 **/
void agentRenderSession(AgentOutputFunc output);

/**
 * Run one user turn: a slash command, or a prompt the agent works on until
 * it has an answer, is cancelled, or reaches the step limit.
 * @param prompt the prompt in UTF-8
 * @param output receives transcript text
 * @return TRUE if the turn finished normally
 **/
BOOL agentRun(CONST_STRPTR prompt, AgentOutputFunc output);

#endif /* AGENT_H */
