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

void codeBlocksViewerSetActionButtons(Object *copyUtf8, Object *copySystem,
                                      Object *saveUtf8, Object *saveSystem);

void codeBlocksViewerAttachListHooks(void);

void codeBlocksViewerAttachActionButtons(void);

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

/** Default ASL filename: block-<index>.<ext> from language. */
void codeBlocksViewerDefaultFileName(struct AICodeBlock *block, STRPTR buf,
                                     ULONG buflen);

BOOL codeBlocksViewerCopyActiveBlockUtf8(void);

BOOL codeBlocksViewerCopyActiveBlockSystem(void);

BOOL codeBlocksViewerSaveActiveBlock(BOOL systemCharset);

#endif /* __MORPHOS__ */

#endif
