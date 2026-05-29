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

void codeBlocksViewerSetAslParentWindow(struct Window *parent);

void codeBlocksViewerSetObjects(Object *list, Object *scintilla);

void codeBlocksViewerSetActionButtons(Object *copyUtf8, Object *copySystem,
                                      Object *saveUtf8, Object *saveSystem);

/** Safe minimal Scintilla setup before ENVARC load (MorphOS startup). */
void codeBlocksViewerPrimeScintillaAtStartup(void);

void codeBlocksViewerAttachListHooks(void);

void codeBlocksViewerAttachActionButtons(void);

/** TRUE if the conversation has at least one parsed code block. */
BOOL codeBlocksConversationHasBlocks(struct Conversation *conv);

/** Fills the block list; returns FALSE if empty or on error. */
BOOL codeBlocksViewerPopulate(struct Conversation *conv);

/** Snapshot before deferred open; invalidated by codeBlocksViewerDismiss(). */
ULONG codeBlocksViewerCaptureOpenToken(void);

/**
 * Open viewer for block index only if openToken is still valid (current chat).
 */
BOOL codeBlocksViewerOpenAtIndexWithToken(ULONG blockIndex, ULONG openToken);

/**
 * Open the code-blocks viewer and select block index (1-based, matches placeholder).
 */
BOOL codeBlocksViewerOpenAtIndex(ULONG blockIndex);

/** Frees NList wrapper rows only; AICodeBlock data stays owned by the conversation. */
void codeBlocksViewerClearList(void);

/** Open viewer after menu action (deferred PushMethod). */
void codeBlocksViewerScheduleOpenWindow(void);

/** Close code viewer and clear list (e.g. chat switch/delete). */
void codeBlocksViewerDismiss(void);

/** Close viewer only — no NList_Clear (safe during app shutdown). */
void codeBlocksViewerCloseWindow(void);

/** Before MUI_DisposeObject(app): drop stale block pointers, clear UI refs. */
void codeBlocksViewerPrepareShutdown(void);
/** Enter shutdown mode: invalidate deferred hooks and ignore new UI actions. */
void codeBlocksViewerBeginShutdown(void);

void codeBlocksViewerSyncSelectionFromList(void);

void codeBlocksViewerShowActiveBlock(void);

/** Default ASL filename: block-<index>.<ext> from language. */
void codeBlocksViewerDefaultFileName(struct AICodeBlock *block, STRPTR buf,
                                     ULONG buflen);

BOOL codeBlocksViewerCopyActiveBlockUtf8(void);

BOOL codeBlocksViewerCopyActiveBlockSystem(void);

BOOL codeBlocksViewerSaveBlock(struct AICodeBlock *block, BOOL systemCharset);

BOOL codeBlocksViewerSaveActiveBlock(BOOL systemCharset);

#endif /* __MORPHOS__ */

#endif
