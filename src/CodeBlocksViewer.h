#ifndef CODEBLOCKS_VIEWER_H
#define CODEBLOCKS_VIEWER_H

#ifdef __MORPHOS__

#include <exec/types.h>
#include "openai.h"

struct Library;

extern struct Library *ClipboardBase;

struct Hook;

extern struct Hook ConstructCodeBlockListHook;
extern struct Hook DestructCodeBlockListHook;
extern struct Hook DisplayCodeBlockListHook;
extern struct Hook CodeBlockListActiveHook;
extern struct Hook CodeBlocksWindowClosedHook;

void codeBlocksViewerSetObjects(Object *list, Object *scintilla);

void codeBlocksViewerSetCopyButton(Object *button);

void codeBlocksViewerAttachListHooks(void);

void codeBlocksViewerAttachCopyButton(void);

/** TRUE if the conversation has at least one parsed code block. */
BOOL codeBlocksConversationHasBlocks(struct Conversation *conv);

/** Fills the block list; returns FALSE if empty or on error. */
BOOL codeBlocksViewerPopulate(struct Conversation *conv);

/** Frees NList wrapper rows only; AICodeBlock data stays owned by the conversation. */
void codeBlocksViewerClearList(void);

/** Close code viewer and clear list (e.g. chat switch/delete). */
void codeBlocksViewerDismiss(void);

void codeBlocksViewerSyncSelectionFromList(void);

void codeBlocksViewerShowActiveBlock(void);

BOOL codeBlocksViewerCopyActiveBlockUtf8(void);

#endif /* __MORPHOS__ */

#endif
