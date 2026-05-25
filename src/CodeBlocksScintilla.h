#ifndef CODEBLOCKS_SCINTILLA_H
#define CODEBLOCKS_SCINTILLA_H

#ifdef __MORPHOS__

#include <libraries/mui.h>

/**
 * MorphOS Scintilla.mcc: send a Scintilla message (SCI_*).
 * Returns the message result (sptr_t), or 0 if sci is NULL.
 */
sptr_t codeBlocksScintillaCommand(Object *sci, unsigned int iMessage,
                                  uptr_t wParam, sptr_t lParam);

/** Apply SCI_SETLEXER / SCI_SETLEXERLANGUAGE from a fence language tag (may be NULL). */
void codeBlocksScintillaApplyLexer(Object *sci, const char *language);

/** Replace document text with a NUL-terminated UTF-8 string and optional lexer. */
void codeBlocksScintillaSetUtf8Text(Object *sci, const char *utf8,
                                    const char *language);

/** Read-only viewer defaults (UTF-8 code page, no editing). */
void codeBlocksScintillaInitViewer(Object *sci);

/** SCI_GETLENGTH via resolved MUIM_Scintilla_Command (0 if unavailable). */
ULONG codeBlocksScintillaDocumentLength(Object *sci);

#endif /* __MORPHOS__ */

#endif
