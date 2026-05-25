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

/** Style 0 = assistant (default); style 1 = user (bold, distinct colour). */
#define CHAT_OUTPUT_STYLE_ASSISTANT 0
#define CHAT_OUTPUT_STYLE_USER 1

/* Scintilla colour 0x00BBGGRR */
#define CHAT_OUTPUT_USER_FORE 0x006000   /* dark blue */
#define CHAT_OUTPUT_USER_BACK 0x00F0F0F0 /* light grey */
#define CHAT_OUTPUT_ASSISTANT_FORE 0x000000

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

static void chatOutputScintillaInitRoleStyles(Object *sci) {
    const char *fontFace = chatOutputScintillaFontFace();

    if (sci == NULL) {
        return;
    }
    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_ASSISTANT,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_ASSISTANT,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_ASSISTANT,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_USER,
                               CHAT_OUTPUT_USER_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_USER,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_USER,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_USER, 1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBACK, CHAT_OUTPUT_STYLE_USER,
                               CHAT_OUTPUT_USER_BACK);
}

static void chatOutputScintillaApplyRoleStyleBytes(Object *sci,
                                                   const UBYTE *styleBytes,
                                                   ULONG byteLen) {
    if (sci == NULL || styleBytes == NULL || byteLen == 0) {
        return;
    }
    codeBlocksScintillaCommand(sci, SCI_SETLEXER, SCLEX_NULL, 0);
    codeBlocksScintillaCommand(sci, SCI_STARTSTYLING, 0, 0xFF);
    codeBlocksScintillaCommand(sci, SCI_SETSTYLINGEX, byteLen, (sptr_t)styleBytes);
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
    chatOutputScintillaInitRoleStyles(sci);
    codeBlocksScintillaCommand(sci, SCI_STYLECLEARALL, 0, 0);
    chatOutputScintillaInitRoleStyles(sci);
    codeBlocksScintillaCommand(sci, SCI_SETUNDOCOLLECTION, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 1, 0);
    codeBlocksScintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)"");
}

void chatOutputScintillaSetUtf8Text(Object *sci, const char *utf8) {
    chatOutputScintillaSetUtf8TextWithRoleStyles(sci, utf8, NULL, 0);
}

void chatOutputScintillaSetUtf8TextWithRoleStyles(Object *sci, const char *utf8,
                                                const UBYTE *roleStyles,
                                                ULONG roleStyleLen) {
    ULONG docLen;
    ULONG textLen;

    if (sci == NULL) {
        return;
    }
    if (utf8 == NULL) {
        utf8 = "";
    }
    textLen = (ULONG)strlen(utf8);

    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)utf8);
    chatOutputScintillaInitRoleStyles(sci);
    if (roleStyles != NULL && roleStyleLen > 0) {
        if (roleStyleLen > textLen) {
            roleStyleLen = textLen;
        }
        chatOutputScintillaApplyRoleStyleBytes(sci, roleStyles, roleStyleLen);
    }
    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 1, 0);

    docLen = (ULONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_GOTOPOS, docLen, 0);
    codeBlocksScintillaCommand(sci, SCI_SCROLLCARET, 0, 0);
}

#endif /* __MORPHOS__ */
