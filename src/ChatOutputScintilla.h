#ifndef CHATOUTPUTSCINTILLA_H
#define CHATOUTPUTSCINTILLA_H

#ifdef __MORPHOS__

#include <libraries/mui.h>

/** Clear markdown `[label](http…)` URL span cache (e.g. when display bypasses buildMidiMarkdown). */
void chatOutputScintillaForgetMarkdownLinkSpans(void);

/** Read-only chat output: UTF-8, TTEngine, font from config.fixedWidthFonts. */
void chatOutputScintillaInitViewer(Object *sci);

/** Replace document text; scroll caret into view (all assistant style). */
void chatOutputScintillaSetUtf8Text(Object *sci, const char *utf8);

/**
 * Like SetUtf8Text; roleStyles[i] is 0 (assistant) or 1 (user), length = min(roleStyleLen, strlen(utf8)).
 */
void chatOutputScintillaSetUtf8TextWithRoleStyles(Object *sci, const char *utf8,
                                                const UBYTE *roleStyles,
                                                ULONG roleStyleLen);

/** Reapply TTEngine font from config.fixedWidthFonts (no full window rebuild). */
void chatOutputScintillaRefreshFont(Object *sci);

/** SCIA_Notify → hotspot release: `[Codeblock n]` opens code viewer; bare http(s):// opens OpenURL. */
void chatOutputScintillaAttachNotify(Object *sci);

/**
 * Pad simple GFM pipe tables in display text (after buildMidiMarkdownDisplay / links).
 * Assistant lines only; skips `[Codeblock n]`; raw_utf8 unchanged. May grow text; updates
 * MD link span positions. Mono aligns best; Sans is approximate.
 */
ULONG chatOutputScintillaFormatPipeTables(char *text, UBYTE *styles, ULONG len, ULONG cap);

/** Main-window EH: clear stuck mouse-down on chat scrollgroup after button-up. */
void chatOutputScintillaInstallMouseUpGuard(void);
void chatOutputScintillaRemoveMouseUpGuard(void);

/** Drop deferred codeblock open (e.g. before code-blocks viewer dismiss). */
void chatOutputScintillaCancelPendingCodeblockOpen(void);

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
