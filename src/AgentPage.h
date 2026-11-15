#ifndef AGENT_PAGE_H
#define AGENT_PAGE_H

#include <exec/types.h>
#include <intuition/classusr.h>

/**
 * The Agent tab of the main window. GUI build only.
 **/

/**
 * Build the contents of the Agent tab
 * @return the group to add to the tab register, or NULL on failure
 **/
Object *createAgentPage(void);

/**
 * Connect the gadgets of the Agent tab. Call once the main window exists.
 **/
void addAgentPageActions(void);

/**
 * The prompt editor of the Agent tab
 **/
Object *agentPagePromptEditor(void);

/**
 * Show the saved session and colour the buttons. Call once the main window
 * is open and the pens are allocated.
 **/
void agentPageWindowOpened(void);

/**
 * Lock or unlock the Agent tab while a request is running
 **/
void agentPageSetBusy(BOOL busy);

/**
 * Free what the Agent tab holds
 **/
void freeAgentPage(void);

#endif /* AGENT_PAGE_H */
