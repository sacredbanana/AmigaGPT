#ifdef __MORPHOS__

#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <Scintilla/Scintilla.h>
#include <Scintilla/SciLexer.h>
#include <string.h>
#include "ChatOutputScintilla.h"
#include "CodeBlocksScintilla.h"
#include "config.h"

#define CHAT_OUTPUT_SCINTILLA_FONTQUALITY_TTENGINE 1
#define CHAT_OUTPUT_SCINTILLA_FONT_MONO "DejaVu Sans Mono"
#define CHAT_OUTPUT_SCINTILLA_FONT_SANS "DejaVu Sans"
#define CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS 12

static const char *chatOutputScintillaFontFace(void) {
    return config.fixedWidthFonts ? CHAT_OUTPUT_SCINTILLA_FONT_MONO
                                 : CHAT_OUTPUT_SCINTILLA_FONT_SANS;
}

static void chatOutputScintillaApplyFont(Object *sci) {
    const char *fontFace = chatOutputScintillaFontFace();

    if (sci == NULL) {
        return;
    }
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, STYLE_DEFAULT,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, STYLE_DEFAULT,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, 0, (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, 0,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
}

void chatOutputScintillaRefreshFont(Object *sci) {
    if (sci == NULL) {
        return;
    }
    chatOutputScintillaApplyFont(sci);
}

void chatOutputScintillaInitViewer(Object *sci) {
    if (sci == NULL) {
        return;
    }
    codeBlocksScintillaCommand(sci, SCI_SETFONTQUALITY,
                               CHAT_OUTPUT_SCINTILLA_FONTQUALITY_TTENGINE, 0);
    codeBlocksScintillaCommand(sci, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    codeBlocksScintillaCommand(sci, SCI_SETLEXER, SCLEX_NULL, 0);
    chatOutputScintillaApplyFont(sci);
    codeBlocksScintillaCommand(sci, SCI_STYLECLEARALL, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETUNDOCOLLECTION, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 1, 0);
    codeBlocksScintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)"");
}

void chatOutputScintillaSetUtf8Text(Object *sci, const char *utf8) {
    ULONG docLen;

    if (sci == NULL) {
        return;
    }
    if (utf8 == NULL) {
        utf8 = "";
    }

    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)utf8);
    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 1, 0);

    docLen = (ULONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_GOTOPOS, docLen, 0);
    codeBlocksScintillaCommand(sci, SCI_SCROLLCARET, 0, 0);
}

#endif /* __MORPHOS__ */
