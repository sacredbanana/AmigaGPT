#ifdef __MORPHOS__

#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <Scintilla/Scintilla.h>
#include <Scintilla/SciLexer.h>
#include <mui/Scintilla_mcc.h>
#include <string.h>
#include <strings.h>
#include "CodeBlocksScintilla.h"

/*
 * MUIP_Scintilla_Command is in the public header but without a #define.
 * Slot +2 is between MUIA_Scintilla_ActiveEditor (+1) and MUIA_Scintilla_Notify (+3).
 */
#ifndef MUIM_Scintilla_Command
#define MUIM_Scintilla_Command (MUIA_Scintilla_dummy + 2)
#endif

#define CODEBLOCKS_SCINTILLA_FONTQUALITY_TTENGINE 1
#define CODEBLOCKS_SCINTILLA_FONT_FACE "DejaVu Sans Mono"
#define CODEBLOCKS_SCINTILLA_FONT_SIZE_POINTS 12

/* Fallback style bytes when MorphOS Scintilla.mcc has no SciLexer build. */
#define STYX_PLAIN 0
#define STYX_COMMENT 1
#define STYX_STRING 2
#define STYX_NUMBER 3

/* Scintilla colours: 0x00BBGGRR */
#define SC_COL_BLACK 0x000000
#define SC_COL_GREEN 0x008000
#define SC_COL_MAROON 0x000080
#define SC_COL_TEAL 0x808000
#define SC_COL_NAVY 0x800000
#define SC_COL_PURPLE 0x800080

typedef struct {
    const char *tag;
    int lexer;
    const char *lexerLanguage;
} CodeBlocksLexerEntry;

typedef enum {
    CB_LANG_C_LIKE,
    CB_LANG_PYTHON
} CodeBlocksLangKind;

static const CodeBlocksLexerEntry codeBlocksLexerMap[] = {
    {"python", SCLEX_PYTHON, "python"},
    {"py", SCLEX_PYTHON, "python"},
    {"c", SCLEX_CPP, "cpp"},
    {"h", SCLEX_CPP, "cpp"},
    {"cpp", SCLEX_CPP, "cpp"},
    {"c++", SCLEX_CPP, "cpp"},
    {"cxx", SCLEX_CPP, "cpp"},
    {"cc", SCLEX_CPP, "cpp"},
    {"hpp", SCLEX_CPP, "cpp"},
    {"csharp", SCLEX_CPP, "cpp"},
    {"cs", SCLEX_CPP, "cpp"},
    {"java", SCLEX_CPP, "cpp"},
    {"go", SCLEX_CPP, "cpp"},
    {"kotlin", SCLEX_CPP, "cpp"},
    {"kt", SCLEX_CPP, "cpp"},
    {"swift", SCLEX_CPP, "cpp"},
    {"javascript", SCLEX_CPP, "cpp"},
    {"js", SCLEX_CPP, "cpp"},
    {"typescript", SCLEX_CPP, "cpp"},
    {"ts", SCLEX_CPP, "cpp"},
    {"json", SCLEX_JSON, "json"},
    {"shell", SCLEX_BASH, "bash"},
    {"bash", SCLEX_BASH, "bash"},
    {"sh", SCLEX_BASH, "bash"},
    {"lua", SCLEX_LUA, "lua"},
    {"html", SCLEX_HTML, "html"},
    {"xml", SCLEX_XML, "xml"},
    {"css", SCLEX_CSS, "css"},
    {"sql", SCLEX_SQL, "sql"},
    {"ruby", SCLEX_RUBY, "ruby"},
    {"rb", SCLEX_RUBY, "ruby"},
    {"rust", SCLEX_RUST, "rust"},
    {"rs", SCLEX_RUST, "rust"},
    {"php", SCLEX_PHPSCRIPT, "php"},
    {"perl", SCLEX_PERL, "perl"},
    {"pl", SCLEX_PERL, "perl"},
    {"r", SCLEX_R, "r"},
    {"yaml", SCLEX_YAML, "yaml"},
    {"yml", SCLEX_YAML, "yaml"},
    {"markdown", SCLEX_MARKDOWN, "markdown"},
    {"md", SCLEX_MARKDOWN, "markdown"},
    {"makefile", SCLEX_MAKEFILE, "makefile"},
    {"rexx", SCLEX_REXX, "rexx"},
    {"arexx", SCLEX_REXX, "rexx"},
};

static sptr_t scintillaCommand(Object *sci, unsigned int iMessage, uptr_t wParam,
                               sptr_t lParam) {
    struct MUIP_Scintilla_Command cmd;

    if (sci == NULL) {
        return 0;
    }
    cmd.MethodID = MUIM_Scintilla_Command;
    cmd.iMessage = (LONG)iMessage;
    cmd.wParam = (LONG)wParam;
    cmd.lParam = (LONG)lParam;
    return (sptr_t)DoMethodA(sci, (Msg)&cmd);
}

sptr_t codeBlocksScintillaCommand(Object *sci, unsigned int iMessage,
                                  uptr_t wParam, sptr_t lParam) {
    return scintillaCommand(sci, iMessage, wParam, lParam);
}

static void codeBlocksScintillaStyleFore(Object *sci, int style, int fore) {
    scintillaCommand(sci, SCI_STYLESETFORE, style, fore);
}

static void codeBlocksScintillaApplyViewerFont(Object *sci, BOOL clearAll) {
    static const char fontFace[] = CODEBLOCKS_SCINTILLA_FONT_FACE;

    if (sci == NULL) {
        return;
    }

    scintillaCommand(sci, SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)fontFace);
    scintillaCommand(sci, SCI_STYLESETSIZE, STYLE_DEFAULT,
                     CODEBLOCKS_SCINTILLA_FONT_SIZE_POINTS);
    scintillaCommand(sci, SCI_STYLESETFONT, 0, (sptr_t)fontFace);
    scintillaCommand(sci, SCI_STYLESETSIZE, 0, CODEBLOCKS_SCINTILLA_FONT_SIZE_POINTS);
    if (clearAll) {
        scintillaCommand(sci, SCI_STYLECLEARALL, 0, 0);
    }
}

static void codeBlocksScintillaInitFallbackStyleColours(Object *sci) {
    codeBlocksScintillaStyleFore(sci, STYX_PLAIN, SC_COL_BLACK);
    codeBlocksScintillaStyleFore(sci, STYX_COMMENT, SC_COL_GREEN);
    codeBlocksScintillaStyleFore(sci, STYX_STRING, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, STYX_NUMBER, SC_COL_TEAL);
}

static void codeBlocksScintillaApplyCppLexerPalette(Object *sci) {
    codeBlocksScintillaStyleFore(sci, SCE_C_DEFAULT, SC_COL_BLACK);
    codeBlocksScintillaStyleFore(sci, SCE_C_COMMENT, SC_COL_GREEN);
    codeBlocksScintillaStyleFore(sci, SCE_C_COMMENTLINE, SC_COL_GREEN);
    codeBlocksScintillaStyleFore(sci, SCE_C_STRING, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, SCE_C_CHARACTER, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, SCE_C_NUMBER, SC_COL_TEAL);
    codeBlocksScintillaStyleFore(sci, SCE_C_WORD, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_C_PREPROCESSOR, SC_COL_PURPLE);
}

static void codeBlocksScintillaApplyPythonLexerPalette(Object *sci) {
    codeBlocksScintillaStyleFore(sci, SCE_P_DEFAULT, SC_COL_BLACK);
    codeBlocksScintillaStyleFore(sci, SCE_P_COMMENTLINE, SC_COL_GREEN);
    codeBlocksScintillaStyleFore(sci, SCE_P_COMMENTBLOCK, SC_COL_GREEN);
    codeBlocksScintillaStyleFore(sci, SCE_P_STRING, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, SCE_P_CHARACTER, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, SCE_P_NUMBER, SC_COL_TEAL);
    codeBlocksScintillaStyleFore(sci, SCE_P_WORD, SC_COL_NAVY);
}

static void codeBlocksScintillaApplyMarkdownLexerPalette(Object *sci) {
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_DEFAULT, SC_COL_BLACK);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_STRONG1, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_STRONG2, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_EM1, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_EM2, SC_COL_MAROON);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_HEADER1, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_HEADER2, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_HEADER3, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_HEADER4, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_HEADER5, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_HEADER6, SC_COL_NAVY);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_LINK, SC_COL_TEAL);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_CODE, SC_COL_PURPLE);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_CODE2, SC_COL_PURPLE);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_CODEBK, SC_COL_PURPLE);
    codeBlocksScintillaStyleFore(sci, SCE_MARKDOWN_BLOCKQUOTE, SC_COL_GREEN);
}

static const CodeBlocksLexerEntry *
codeBlocksScintillaFindLexer(const char *language) {
    ULONG i;

    if (language == NULL || language[0] == '\0') {
        return NULL;
    }
    for (i = 0; i < sizeof(codeBlocksLexerMap) / sizeof(codeBlocksLexerMap[0]);
         i++) {
        if (strcasecmp(language, codeBlocksLexerMap[i].tag) == 0) {
            return &codeBlocksLexerMap[i];
        }
    }
    return NULL;
}

static CodeBlocksLangKind codeBlocksScintillaLangKind(const char *language,
                                                      const CodeBlocksLexerEntry
                                                          *entry) {
    if (entry != NULL && entry->lexer == SCLEX_PYTHON) {
        return CB_LANG_PYTHON;
    }
    if (language != NULL) {
        if (strcasecmp(language, "python") == 0 ||
            strcasecmp(language, "py") == 0) {
            return CB_LANG_PYTHON;
        }
    }
    return CB_LANG_C_LIKE;
}

static BOOL codeBlocksScintillaIsDigitChar(UBYTE c) {
    return (c >= '0' && c <= '9');
}

static void codeBlocksScintillaFillStylesPython(const char *text, ULONG len,
                                              UBYTE *styles) {
    ULONG i;
    BOOL inString = FALSE;
    char stringQuote = 0;

    for (i = 0; i < len; i++) {
        UBYTE c = (UBYTE)text[i];
        styles[i] = STYX_PLAIN;

        if (inString) {
            styles[i] = STYX_STRING;
            if (c == '\\' && i + 1 < len) {
                styles[++i] = STYX_STRING;
                continue;
            }
            if (c == stringQuote) {
                inString = FALSE;
            }
            continue;
        }

        if (c == '#') {
            styles[i] = STYX_COMMENT;
            for (i++; i < len && text[i] != '\n'; i++) {
                styles[i] = STYX_COMMENT;
            }
            if (i < len) {
                styles[i] = STYX_COMMENT;
            }
            continue;
        }

        if (c == '"' || c == '\'') {
            inString = TRUE;
            stringQuote = (char)c;
            styles[i] = STYX_STRING;
            continue;
        }

        if (codeBlocksScintillaIsDigitChar(c)) {
            styles[i] = STYX_NUMBER;
        }
    }
}

static void codeBlocksScintillaFillStylesCLike(const char *text, ULONG len,
                                             UBYTE *styles) {
    ULONG i;
    BOOL inLineComment = FALSE;
    BOOL inBlockComment = FALSE;
    BOOL inString = FALSE;

    for (i = 0; i < len; i++) {
        UBYTE c = (UBYTE)text[i];
        styles[i] = STYX_PLAIN;

        if (inBlockComment) {
            styles[i] = STYX_COMMENT;
            if (c == '*' && i + 1 < len && text[i + 1] == '/') {
                styles[++i] = STYX_COMMENT;
                inBlockComment = FALSE;
            }
            continue;
        }

        if (inLineComment) {
            styles[i] = STYX_COMMENT;
            if (c == '\n') {
                inLineComment = FALSE;
            }
            continue;
        }

        if (inString) {
            styles[i] = STYX_STRING;
            if (c == '\\' && i + 1 < len) {
                styles[++i] = STYX_STRING;
                continue;
            }
            if (c == '"') {
                inString = FALSE;
            }
            continue;
        }

        if (c == '/' && i + 1 < len) {
            if (text[i + 1] == '/') {
                styles[i] = STYX_COMMENT;
                styles[i + 1] = STYX_COMMENT;
                i++;
                inLineComment = TRUE;
                continue;
            }
            if (text[i + 1] == '*') {
                styles[i] = STYX_COMMENT;
                styles[i + 1] = STYX_COMMENT;
                i++;
                inBlockComment = TRUE;
                continue;
            }
        }

        if (c == '"') {
            inString = TRUE;
            styles[i] = STYX_STRING;
            continue;
        }

        if (codeBlocksScintillaIsDigitChar(c)) {
            styles[i] = STYX_NUMBER;
        }
    }
}

static void codeBlocksScintillaApplySimpleHighlight(Object *sci,
                                                    const char *utf8,
                                                    CodeBlocksLangKind kind) {
    ULONG len;
    UBYTE *styles;

    if (sci == NULL || utf8 == NULL) {
        return;
    }

    len = (ULONG)strlen(utf8);
    if (len == 0) {
        return;
    }

    styles = AllocVec(len, MEMF_ANY);
    if (styles == NULL) {
        return;
    }

    if (kind == CB_LANG_PYTHON) {
        codeBlocksScintillaFillStylesPython(utf8, len, styles);
    } else {
        codeBlocksScintillaFillStylesCLike(utf8, len, styles);
    }

    scintillaCommand(sci, SCI_SETLEXER, SCLEX_NULL, 0);
    codeBlocksScintillaInitFallbackStyleColours(sci);
    scintillaCommand(sci, SCI_STARTSTYLING, 0, 0xFF);
    scintillaCommand(sci, SCI_SETSTYLINGEX, len, (sptr_t)styles);
    FreeVec(styles);
}

static BOOL codeBlocksScintillaTryBuiltinLexer(Object *sci,
                                               const CodeBlocksLexerEntry *entry) {
    int before;
    int after;

    if (sci == NULL || entry == NULL) {
        return FALSE;
    }

    before = (int)scintillaCommand(sci, SCI_GETLEXER, 0, 0);

    if (entry->lexerLanguage != NULL && entry->lexerLanguage[0] != '\0') {
        scintillaCommand(sci, SCI_SETLEXERLANGUAGE, 0,
                         (sptr_t)entry->lexerLanguage);
    }
    scintillaCommand(sci, SCI_SETLEXER, (uptr_t)entry->lexer, 0);

    after = (int)scintillaCommand(sci, SCI_GETLEXER, 0, 0);
    if (after != entry->lexer || after == SCLEX_NULL) {
        return FALSE;
    }

    if (entry->lexer == SCLEX_PYTHON) {
        codeBlocksScintillaApplyPythonLexerPalette(sci);
    } else if (entry->lexer == SCLEX_CPP || entry->lexer == SCLEX_JSON) {
        codeBlocksScintillaApplyCppLexerPalette(sci);
    } else if (entry->lexer == SCLEX_MARKDOWN) {
        codeBlocksScintillaApplyMarkdownLexerPalette(sci);
    }

    return TRUE;
}

void codeBlocksScintillaApplyLexer(Object *sci, const char *language) {
    const CodeBlocksLexerEntry *entry;

    if (sci == NULL) {
        return;
    }

    scintillaCommand(sci, SCI_CLEARDOCUMENTSTYLE, 0, 0);
    entry = codeBlocksScintillaFindLexer(language);
    if (entry != NULL) {
        (void)codeBlocksScintillaTryBuiltinLexer(sci, entry);
        return;
    }

    scintillaCommand(sci, SCI_SETLEXER, SCLEX_NULL, 0);
    if (language != NULL && language[0] != '\0') {
        scintillaCommand(sci, SCI_SETLEXERLANGUAGE, 0, (sptr_t)language);
    }
}

ULONG codeBlocksScintillaDocumentLength(Object *sci) {
    return (ULONG)scintillaCommand(sci, SCI_GETLENGTH, 0, 0);
}

void codeBlocksScintillaSetUtf8Text(Object *sci, const char *utf8,
                                    const char *language) {
    const CodeBlocksLexerEntry *entry;
    BOOL builtinOk = FALSE;

    if (sci == NULL) {
        return;
    }
    if (utf8 == NULL) {
        utf8 = "";
    }

    entry = codeBlocksScintillaFindLexer(language);

    scintillaCommand(sci, SCI_SETFONTQUALITY, CODEBLOCKS_SCINTILLA_FONTQUALITY_TTENGINE, 0);
    scintillaCommand(sci, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    scintillaCommand(sci, SCI_CLEARDOCUMENTSTYLE, 0, 0);

    if (entry != NULL) {
        builtinOk = codeBlocksScintillaTryBuiltinLexer(sci, entry);
    } else {
        scintillaCommand(sci, SCI_SETLEXER, SCLEX_NULL, 0);
    }

    codeBlocksScintillaApplyViewerFont(sci, FALSE);

    scintillaCommand(sci, SCI_SETREADONLY, 0, 0);
    scintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)utf8);

    if (builtinOk) {
        scintillaCommand(sci, SCI_COLOURISE, 0, -1);
        scintillaCommand(sci, SCI_CHANGELEXERSTATE, 0, -1);
    } else if (utf8[0] != '\0') {
        codeBlocksScintillaApplySimpleHighlight(
            sci, utf8, codeBlocksScintillaLangKind(language, entry));
    }

    scintillaCommand(sci, SCI_SETREADONLY, 1, 0);
}

void codeBlocksScintillaInitViewer(Object *sci) {
    if (sci == NULL) {
        return;
    }
    scintillaCommand(sci, SCI_SETFONTQUALITY, CODEBLOCKS_SCINTILLA_FONTQUALITY_TTENGINE, 0);
    scintillaCommand(sci, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    codeBlocksScintillaApplyViewerFont(sci, TRUE);
    codeBlocksScintillaInitFallbackStyleColours(sci);
    scintillaCommand(sci, SCI_SETLEXER, SCLEX_NULL, 0);
    scintillaCommand(sci, SCI_SETUNDOCOLLECTION, 0, 0);
}

#endif /* __MORPHOS__ */
