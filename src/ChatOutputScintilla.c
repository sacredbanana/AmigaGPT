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
/* Midi-markdown overlays on assistant (2–8). */
#define CHAT_OUTPUT_STYLE_MD_BOLD 2
#define CHAT_OUTPUT_STYLE_MD_ITALIC 3
#define CHAT_OUTPUT_STYLE_MD_UNDERLINE 4
#define CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC 5
#define CHAT_OUTPUT_STYLE_MD_BOLD_UL 6
#define CHAT_OUTPUT_STYLE_MD_ITALIC_UL 7
#define CHAT_OUTPUT_STYLE_MD_ALL 8

#define CHAT_MD_MAX_STACK 32
#define CHAT_MD_CODEBLOCK_PREFIX "[Codeblock"

typedef enum {
    CHAT_MD_STYLE_BOLD,
    CHAT_MD_STYLE_ITALIC,
    CHAT_MD_STYLE_UNDERLINE
} ChatMdStyleType;

typedef struct {
    ChatMdStyleType stack[CHAT_MD_MAX_STACK];
    int top;
} ChatMdStyleStack;

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

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_BOLD,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_BOLD,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_BOLD,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_BOLD, 1);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_ITALIC,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_ITALIC,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_ITALIC,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETITALIC, CHAT_OUTPUT_STYLE_MD_ITALIC,
                               1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_ITALIC, 0);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_UNDERLINE,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_UNDERLINE,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_UNDERLINE,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETUNDERLINE, CHAT_OUTPUT_STYLE_MD_UNDERLINE,
                               1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_UNDERLINE, 0);
    codeBlocksScintillaCommand(sci, SCI_STYLESETITALIC, CHAT_OUTPUT_STYLE_MD_UNDERLINE,
                               0);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC,
                               1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETITALIC, CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC,
                               1);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_BOLD_UL,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_BOLD_UL,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_BOLD_UL,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_BOLD_UL, 1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETUNDERLINE, CHAT_OUTPUT_STYLE_MD_BOLD_UL,
                               1);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_ITALIC_UL,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_ITALIC_UL,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_ITALIC_UL,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETITALIC, CHAT_OUTPUT_STYLE_MD_ITALIC_UL,
                               1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETUNDERLINE, CHAT_OUTPUT_STYLE_MD_ITALIC_UL,
                               1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_ITALIC_UL, 0);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_MD_ALL,
                               CHAT_OUTPUT_ASSISTANT_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_MD_ALL,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_MD_ALL,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETBOLD, CHAT_OUTPUT_STYLE_MD_ALL, 1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETITALIC, CHAT_OUTPUT_STYLE_MD_ALL, 1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETUNDERLINE, CHAT_OUTPUT_STYLE_MD_ALL, 1);
}

static void chatMdInitStack(ChatMdStyleStack *s) { s->top = -1; }

static BOOL chatMdPush(ChatMdStyleStack *s, ChatMdStyleType style) {
    if (s->top >= (CHAT_MD_MAX_STACK - 1)) {
        return FALSE;
    }
    s->stack[++s->top] = style;
    return TRUE;
}

static BOOL chatMdPop(ChatMdStyleStack *s, ChatMdStyleType style) {
    if (s->top < 0) {
        return FALSE;
    }
    if (s->stack[s->top] == style) {
        s->top--;
        return TRUE;
    }
    return FALSE;
}

static BOOL chatMdIsTop(const ChatMdStyleStack *s, ChatMdStyleType style) {
    if (s->top < 0) {
        return FALSE;
    }
    return (s->stack[s->top] == style);
}

static BOOL chatMdIsAsciiAlnum(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9');
}

/* Single * is not markdown inside words (e.g. Spieler*innen). */
static BOOL chatMdIsItalicStarMarker(const char *input, ULONG pos, ULONG len) {
    if (pos >= len || input[pos] != '*') {
        return FALSE;
    }
    if (pos + 1 < len && input[pos + 1] == '*') {
        return FALSE;
    }
    if (pos > 0 && chatMdIsAsciiAlnum(input[pos - 1]) && pos + 1 < len &&
        chatMdIsAsciiAlnum(input[pos + 1])) {
        return FALSE;
    }
    return TRUE;
}

static UBYTE chatMdParseMarker(const char *input, ULONG pos, ULONG len,
                               ChatMdStyleType *foundStyle) {
    if (pos >= len) {
        return 0;
    }
    if (pos + 1 < len && input[pos] == '_' && input[pos + 1] == '_') {
        *foundStyle = CHAT_MD_STYLE_UNDERLINE;
        return 2;
    }
    if (pos + 1 < len && input[pos] == '*' && input[pos + 1] == '*') {
        *foundStyle = CHAT_MD_STYLE_BOLD;
        return 2;
    }
    if (chatMdIsItalicStarMarker(input, pos, len)) {
        *foundStyle = CHAT_MD_STYLE_ITALIC;
        return 1;
    }
    return 0;
}

static UBYTE chatMdStyleFromStack(const ChatMdStyleStack *s) {
    BOOL bold = FALSE;
    BOOL italic = FALSE;
    BOOL underline = FALSE;
    int i;

    for (i = 0; i <= s->top; i++) {
        switch (s->stack[i]) {
        case CHAT_MD_STYLE_BOLD:
            bold = TRUE;
            break;
        case CHAT_MD_STYLE_ITALIC:
            italic = TRUE;
            break;
        case CHAT_MD_STYLE_UNDERLINE:
            underline = TRUE;
            break;
        }
    }
    if (!bold && !italic && !underline) {
        return CHAT_OUTPUT_STYLE_ASSISTANT;
    }
    if (bold && italic && underline) {
        return CHAT_OUTPUT_STYLE_MD_ALL;
    }
    if (bold && italic) {
        return CHAT_OUTPUT_STYLE_MD_BOLD_ITALIC;
    }
    if (bold && underline) {
        return CHAT_OUTPUT_STYLE_MD_BOLD_UL;
    }
    if (italic && underline) {
        return CHAT_OUTPUT_STYLE_MD_ITALIC_UL;
    }
    if (bold) {
        return CHAT_OUTPUT_STYLE_MD_BOLD;
    }
    if (italic) {
        return CHAT_OUTPUT_STYLE_MD_ITALIC;
    }
    return CHAT_OUTPUT_STYLE_MD_UNDERLINE;
}

static BOOL chatMdIsCodeblockPlaceholder(const char *text, ULONG len, ULONG pos) {
    static const char prefix[] = CHAT_MD_CODEBLOCK_PREFIX;
    ULONG prefixLen = sizeof(prefix) - 1;
    ULONG i;

    if (pos >= len || text[pos] != '[') {
        return FALSE;
    }
    if (pos + prefixLen > len) {
        return FALSE;
    }
    for (i = 0; i < prefixLen; i++) {
        if (text[pos + i] != prefix[i]) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL chatMdInSkippedRegion(const char *text, ULONG len, ULONG pos) {
    ULONG lineStart = pos;

    while (lineStart > 0 && text[lineStart - 1] != '\n') {
        lineStart--;
    }
    return chatMdIsCodeblockPlaceholder(text, len, lineStart);
}

static ULONG chatMdHeadingContentStart(const char *text, ULONG len, ULONG lineStart) {
    ULONG h = lineStart;
    ULONG hashes = 0;

    while (h < len && text[h] == '#' && hashes < 6) {
        hashes++;
        h++;
    }
    if (hashes == 0 || hashes > 6 || h >= len || text[h] != ' ') {
        return 0;
    }
    h++;
    while (h < len && text[h] == ' ') {
        h++;
    }
    return h;
}

static void chatMdEmit(ULONG *outPos, char *outUtf8, UBYTE *outStyles, char ch,
                       UBYTE style) {
    outUtf8[*outPos] = ch;
    outStyles[*outPos] = style;
    (*outPos)++;
}

static void chatMdEmitCString(ULONG *outPos, char *outUtf8, UBYTE *outStyles,
                              const char *text, UBYTE style) {
    ULONG t;

    if (text == NULL) {
        return;
    }
    for (t = 0; text[t] != '\0'; t++) {
        chatMdEmit(outPos, outUtf8, outStyles, text[t], style);
    }
}

/*
 * Display-only substitutes when midi-markdown is active (TTEngine/DejaVu: no color
 * emoji glyphs). raw_utf8 / export unchanged. Longest match wins.
 */
typedef struct {
    const char *emojiUtf8;
    const char *displayText;
} ChatMdEmojiMapEntry;

static const ChatMdEmojiMapEntry chatMdEmojiMap[] = {
    { "👍", "(+1)" },
    { "👎", "(-1)" },
    { "👋", "(Hallo)" },
    { "👀", "(sehen)" },
    { "🙏", "(Danke)" },
    { "💪", "(stark)" },
    { "🌍", "[Welt]" },
    { "🌎", "[Welt]" },
    { "🌏", "[Welt]" },
    { "🚀", "[Rakete]" },
    { "💻", "[PC]" },
    { "📝", "[Notiz]" },
    { "📌", "[Pin]" },
    { "📎", "[Anhang]" },
    { "🔧", "[Werkzeug]" },
    { "🛠", "[Werkzeug]" },
    { "🛠️", "[Werkzeug]" },
    { "✅", "[OK]" },
    { "❌", "[X]" },
    { "⚠", "[!]" },
    { "⚠️", "[!]" },
    { "⭐", "(*)" },
    { "🎯", "[Ziel]" },
    { "🎉", "[Feier]" },
    { "🔥", "[heiss]" },
    { "💡", "(Idee)" },
    { "✨", "(*)" },
    { "❤️", "(Herz)" },
    { "❤", "(Herz)" },
    { "😎", "(cool)" },
    { "🤔", "(?)" },
    { "🙂", ":)" },
    { "😊", "X)" },
    { "😉", ";)" },
    { "😄", "XD" },
    { "😃", ":D" },
    { "😀", ":D" },
    { "😁", ":D" },
    { "😢", ":'(" },
    { "😭", ":'(" },
};

static ULONG chatMdEmitEmojiSubstitute(const char *inUtf8, ULONG pos, ULONG len,
                                       ULONG *outPos, char *outUtf8,
                                       UBYTE *outStyles, UBYTE style) {
    ULONG bestLen = 0;
    const char *bestText = NULL;
    ULONG n;

    for (n = 0; n < sizeof(chatMdEmojiMap) / sizeof(chatMdEmojiMap[0]); n++) {
        ULONG emojiLen = (ULONG)strlen(chatMdEmojiMap[n].emojiUtf8);

        if (emojiLen == 0 || pos + emojiLen > len) {
            continue;
        }
        if (memcmp(inUtf8 + pos, chatMdEmojiMap[n].emojiUtf8, emojiLen) != 0) {
            continue;
        }
        if (emojiLen > bestLen) {
            bestLen = emojiLen;
            bestText = chatMdEmojiMap[n].displayText;
        }
    }
    if (bestLen == 0 || bestText == NULL) {
        return 0;
    }
    chatMdEmitCString(outPos, outUtf8, outStyles, bestText, style);
    return bestLen;
}

static void chatMdEmitMarkerLiteral(ULONG *outPos, char *outUtf8, UBYTE *outStyles,
                                    const char *inUtf8, ULONG pos, UBYTE markerLen,
                                    UBYTE style) {
    ULONG m;

    for (m = 0; m < markerLen; m++) {
        chatMdEmit(outPos, outUtf8, outStyles, inUtf8[pos + m], style);
    }
}

static BOOL chatMdHandleMarker(ChatMdStyleStack *styleStack, const char *inUtf8,
                               ULONG *i, ULONG len, ULONG *outPos, char *outUtf8,
                               UBYTE *outStyles, UBYTE mdStyle) {
    ChatMdStyleType styleFound;
    UBYTE markerLen = chatMdParseMarker(inUtf8, *i, len, &styleFound);

    if (markerLen == 0) {
        return FALSE;
    }
    if (chatMdIsTop(styleStack, styleFound)) {
        chatMdPop(styleStack, styleFound);
    } else if (!chatMdPush(styleStack, styleFound)) {
        chatMdEmitMarkerLiteral(outPos, outUtf8, outStyles, inUtf8, *i, markerLen,
                                mdStyle);
    }
    *i += markerLen - 1;
    return TRUE;
}

ULONG chatOutputScintillaBuildMidiMarkdownDisplay(const char *inUtf8,
                                                  const UBYTE *inRoleStyles,
                                                  ULONG inLen, char *outUtf8,
                                                  UBYTE *outStyles) {
    ChatMdStyleStack styleStack;
    ULONG i;
    ULONG outPos = 0;

    if (inUtf8 == NULL || outUtf8 == NULL || outStyles == NULL || inLen == 0) {
        if (outUtf8 != NULL) {
            outUtf8[0] = '\0';
        }
        return 0;
    }

    chatMdInitStack(&styleStack);

    for (i = 0; i < inLen; i++) {
        ULONG lineStart;
        ULONG contentStart;
        ULONG lineEnd;
        UBYTE role =
            (inRoleStyles != NULL) ? inRoleStyles[i] : CHAT_OUTPUT_STYLE_ASSISTANT;

        if (role == CHAT_OUTPUT_STYLE_USER) {
            chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                       CHAT_OUTPUT_STYLE_USER);
            continue;
        }
        if (chatMdInSkippedRegion(inUtf8, inLen, i)) {
            chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                       CHAT_OUTPUT_STYLE_ASSISTANT);
            continue;
        }
        if (inUtf8[i] == '\n') {
            chatMdEmit(&outPos, outUtf8, outStyles, '\n',
                       CHAT_OUTPUT_STYLE_ASSISTANT);
            continue;
        }

        lineStart = i;
        while (lineStart > 0 && inUtf8[lineStart - 1] != '\n') {
            lineStart--;
        }
        contentStart = chatMdHeadingContentStart(inUtf8, inLen, lineStart);
        if (contentStart > 0) {
            lineEnd = lineStart;
            while (lineEnd < inLen && inUtf8[lineEnd] != '\n') {
                lineEnd++;
            }
            if (i < contentStart) {
                continue;
            }
            if (i < lineEnd) {
                {
                    ULONG emojiLen = chatMdEmitEmojiSubstitute(
                        inUtf8, i, inLen, &outPos, outUtf8, outStyles,
                        CHAT_OUTPUT_STYLE_MD_BOLD);
                    if (emojiLen > 0) {
                        i += emojiLen - 1;
                        continue;
                    }
                }
                if (inUtf8[i] == '\\' && i + 1 < inLen) {
                    i++;
                    chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                               CHAT_OUTPUT_STYLE_MD_BOLD);
                    continue;
                }
                if (chatMdHandleMarker(&styleStack, inUtf8, &i, inLen, &outPos,
                                       outUtf8, outStyles,
                                       CHAT_OUTPUT_STYLE_MD_BOLD)) {
                    continue;
                }
                chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                           CHAT_OUTPUT_STYLE_MD_BOLD);
                continue;
            }
        }

        {
            ULONG emojiLen = chatMdEmitEmojiSubstitute(
                inUtf8, i, inLen, &outPos, outUtf8, outStyles,
                chatMdStyleFromStack(&styleStack));
            if (emojiLen > 0) {
                i += emojiLen - 1;
                continue;
            }
        }

        if (inUtf8[i] == '\\' && i + 1 < inLen) {
            i++;
            chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                       chatMdStyleFromStack(&styleStack));
            continue;
        }

        if (chatMdHandleMarker(&styleStack, inUtf8, &i, inLen, &outPos, outUtf8,
                               outStyles, chatMdStyleFromStack(&styleStack))) {
            continue;
        }

        chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                   chatMdStyleFromStack(&styleStack));
    }

    outUtf8[outPos] = '\0';
    return outPos;
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
