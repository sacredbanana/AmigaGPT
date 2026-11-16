#ifndef AGENT_PAGE_H
#define AGENT_PAGE_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include <intuition/screens.h>

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
 * Get the pens the transcript draws reasoning and tool calls with, for the
 * screen the window is on. Pens held from before are released first.
 **/
void agentPageObtainPens(struct Screen *screen);

/**
 * Give back the transcript's pens. Call while the screen is still open.
 **/
void agentPageReleasePens(void);

/**
 * Free what the Agent tab holds
 **/
void freeAgentPage(void);

#endif /* AGENT_PAGE_H */
