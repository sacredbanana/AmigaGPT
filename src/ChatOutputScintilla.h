#ifndef CHATOUTPUTSCINTILLA_H
#define CHATOUTPUTSCINTILLA_H

#ifdef __MORPHOS__

#include <libraries/mui.h>

/** Clear markdown `[label](http…)` URL span cache (e.g. when display bypasses buildMidiMarkdown). */
void chatOutputScintillaForgetMarkdownLinkSpans(void);

/** Minimal chat Scintilla setup before ENVARC / OM_ADDMEMBER (mirrors code viewer prime). */
void chatOutputScintillaPrimeViewer(Object *sci);

/** Font, role styles, notify — after Application_Load, before main window open. */
void chatOutputScintillaFinishViewerInit(Object *sci);

/** Idempotent full init if finish was skipped (e.g. settings refresh). */
void chatOutputScintillaEnsureViewerReady(Object *sci);

/** Read-only chat output: UTF-8, TTEngine; Mono/Sans from config.fixedWidthFonts. */
void chatOutputScintillaInitViewer(Object *sci);

/** Replace document text; scroll to end (all assistant style). */
void chatOutputScintillaSetUtf8Text(Object *sci, const char *utf8);

/**
 * During live stream: append only buffer tail via SCI_APPENDTEXT (no full SETTEXT).
 * Returns FALSE when doc/buffer are out of sync — caller should SETTEXT instead.
 */
BOOL chatOutputScintillaAppendStreamDelta(Object *sci, const char *utf8, ULONG textLen,
                                          const UBYTE *roleStyles, ULONG roleStyleLen);

/** Reset stream append cursor (e.g. before non-stream full refresh). */
void chatOutputScintillaResetStreamSync(void);

/** Empty document on shutdown (no scroll-to-end, no markdown rebuild). */
void chatOutputScintillaClearDocument(Object *sci);

/**
 * Like SetUtf8Text; roleStyles[i] is 0 (assistant) or 1 (user), length = min(roleStyleLen, strlen(utf8)).
 * preserveViewport: keep approximate scroll line (Markdown toggle); else scroll to document end.
 */
void chatOutputScintillaSetUtf8TextWithRoleStyles(Object *sci, const char *utf8,
                                                const UBYTE *roleStyles,
                                                ULONG roleStyleLen,
                                                BOOL preserveViewport);

/** Add codeblock/URL hotspot style bytes (call once before SetUtf8 when styles are 0/1 only). */
void chatOutputScintillaAugmentStyleBytesHotspots(const char *utf8, UBYTE *styleBytes,
                                                  ULONG byteLen);

/** When TRUE, SetUtf8Text skips scroll-to-end (NList chat switch). */
extern BOOL chatOutputScintillaMorphosSkipViewport;

/** Reapply TTEngine font from config.fixedWidthFonts / config.chatFontSize. */
void chatOutputScintillaRefreshFont(Object *sci);

/** SCI_SETWRAPMODE from config.chatLineWrap (Ansicht-Menü). */
void chatOutputScintillaApplyLineWrap(Object *sci);

/** SCIA_Notify → hotspot release: `[Codeblock n]` opens code viewer; bare http(s):// opens OpenURL. */
void chatOutputScintillaAttachNotify(Object *sci);

/**
 * Pad simple GFM pipe tables in display text (after buildMidiMarkdownDisplay / links).
 * Assistant lines only; skips `[Codeblock n]`; raw_utf8 unchanged. May grow text; updates
 * MD link span positions. Mono aligns best; Sans is approximate.
 */
ULONG chatOutputScintillaFormatPipeTables(char *text, UBYTE *styles, ULONG len, ULONG cap);

/** Main-window EH: SCI_CANCEL after hotspot mouse-down if button-up ends on scrollgroup. */
void chatOutputScintillaInstallMouseUpGuard(void);
void chatOutputScintillaRemoveMouseUpGuard(void);

/** Drop deferred codeblock open (e.g. before code-blocks viewer dismiss). */
void chatOutputScintillaCancelPendingCodeblockOpen(void);

/** Free pending deferred role-style apply (shutdown / before new SetUtf8). */
void chatOutputScintillaCancelDeferredStyles(void);

/** Stop SCN traffic and pending editor work before MUI tears down chat Scintilla. */
void chatOutputScintillaQuiesceForShutdown(Object *sci);

/** TRUE while a deferred role-style PushMethod is still pending. */
BOOL chatOutputScintillaHasDeferredWorkPending(void);

/** Remove SCIA_Notify before chat Scintilla is disposed. */
void chatOutputScintillaDetachNotify(void);

/** After MUI_DisposeObject(app): delete notify sink custom class. */
void chatOutputScintillaDisposeNotifyClass(void);

/**
 * Build display text + style bytes: optional Midi-Markdown (when config.markdownFormatting);
 * always resolves `[label](http…)` / `([label](http…))` for display + URL hotspots.
 * raw_utf8 / export unchanged. Returns output length.
 */
ULONG chatOutputScintillaBuildMidiMarkdownDisplay(const char *inUtf8,
                                                  const UBYTE *inRoleStyles,
                                                  ULONG inLen, char *outUtf8,
                                                  UBYTE *outStyles);

#endif /* __MORPHOS__ */

#endif
