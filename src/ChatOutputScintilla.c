#ifdef __MORPHOS__

#include <devices/inputevent.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <mui/Scintilla_mcc.h>
#include <Scintilla/Scintilla.h>
#include <Scintilla/SciLexer.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <libraries/openurl.h>
#include <proto/exec.h>
#include <proto/openurl.h>
#include "ChatOutputScintilla.h"
#include "CodeBlocksScintilla.h"
#include "CodeBlocksViewer.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"

/*
 * MorphOS Scintilla.guide: MUIM_Notify on SCIA_Notify → MM_SciHandler on a sink object;
 * struct SCNotification * in msg->scn. Not exported in public Scintilla_mcc.h — slot +9
 * between MUIM_Scintilla_Definition (+8) and MUIA_Scintilla_LexerChanged (+10).
 */
#ifndef MM_SciHandler
#define MM_SciHandler (MUIA_Scintilla_dummy + 9)
#endif

struct MUIP_SciHandler {
    ULONG MethodID;
    struct SCNotification *scn;
};

static struct MUI_CustomClass *chatOutputSciNotifyClass;
static Object *chatOutputSciNotifySink;
static Object *chatOutputSciNotifySource;
static BOOL chatOutputSciNotifyAttached;

static struct MUI_EventHandlerNode *chatOutputSciMouseUpEH;
static BOOL chatOutputSciMouseUpEHInstalled;

static ULONG chatOutputSciHotspotPendingIndex;
static ULONG chatOutputSciHotspotOpenIndex;
static ULONG chatOutputSciHotspotOpenToken;
static BOOL chatOutputSciHotspotUrlArmed;

#define CHAT_OUTPUT_URL_MAX 2048
#define CHAT_OUTPUT_MD_LINK_SPANS_MAX 128

typedef struct {
    ULONG start;
    ULONG end;
    char url[CHAT_OUTPUT_URL_MAX];
} ChatOutputMdLinkSpan;

static ULONG chatOutputMdLinkSpanCount;
static ChatOutputMdLinkSpan chatOutputMdLinkSpans[CHAT_OUTPUT_MD_LINK_SPANS_MAX];

void chatOutputScintillaForgetMarkdownLinkSpans(void) { chatOutputMdLinkSpanCount = 0; }

static void chatOutputScintillaReleaseChatMouse(Object *sci) {
    if (sci == NULL) {
        return;
    }
    codeBlocksScintillaCommand(sci, SCI_CANCEL, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETEMPTYSELECTION,
                               codeBlocksScintillaCommand(sci, SCI_GETCURRENTPOS, 0, 0),
                               0);
}

HOOKPROTONHNONP(ChatOutputSciOpenCodeblockDeferredFunc, void) {
    ULONG blockIndex = chatOutputSciHotspotOpenIndex;
    ULONG openToken = chatOutputSciHotspotOpenToken;

    chatOutputSciHotspotOpenIndex = 0;
    chatOutputSciHotspotOpenToken = 0;
    if (blockIndex > 0) {
        (void)codeBlocksViewerOpenAtIndexWithToken(blockIndex, openToken);
    }
    if (chatOutputSciNotifySource != NULL) {
        chatOutputScintillaReleaseChatMouse(chatOutputSciNotifySource);
    }
}

MakeHook(ChatOutputSciOpenCodeblockDeferredHook, ChatOutputSciOpenCodeblockDeferredFunc);

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
/** Hotspot style for `[Codeblock n]` — open on SCN_HOTSPOTRELEASECLICK (after mouse-up). */
#define CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT 9
/** Hotspot style for bare `http://` / `https://` URLs — OpenURL on release. */
#define CHAT_OUTPUT_STYLE_URL_HOTSPOT 10

#define CHAT_MD_MAX_STACK 32
#define CHAT_OUTPUT_CODEBLOCK_LINK_FORE 0x00CC6600 /* BBGGRR link blue */
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

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT,
                               CHAT_OUTPUT_CODEBLOCK_LINK_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETUNDERLINE,
                               CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT, 1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETHOTSPOT,
                               CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT, 1);

    codeBlocksScintillaCommand(sci, SCI_STYLESETFORE, CHAT_OUTPUT_STYLE_URL_HOTSPOT,
                               CHAT_OUTPUT_CODEBLOCK_LINK_FORE);
    codeBlocksScintillaCommand(sci, SCI_STYLESETFONT, CHAT_OUTPUT_STYLE_URL_HOTSPOT,
                               (sptr_t)fontFace);
    codeBlocksScintillaCommand(sci, SCI_STYLESETSIZE, CHAT_OUTPUT_STYLE_URL_HOTSPOT,
                               CHAT_OUTPUT_SCINTILLA_FONT_SIZE_POINTS);
    codeBlocksScintillaCommand(sci, SCI_STYLESETUNDERLINE, CHAT_OUTPUT_STYLE_URL_HOTSPOT,
                               1);
    codeBlocksScintillaCommand(sci, SCI_STYLESETHOTSPOT, CHAT_OUTPUT_STYLE_URL_HOTSPOT, 1);
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

static BOOL chatMdIsUrlBodyByte(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return TRUE;
    }
    switch (c) {
    case '-':
    case '_':
    case '.':
    case '~':
    case ':':
    case '/':
    case '?':
    case '#':
    case '[':
    case ']':
    case '@':
    case '!':
    case '$':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case ';':
    case '=':
    case '%':
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOL chatMdUrlSchemeOkPrefix(const char *t, ULONG pos) {
    if (pos == 0) {
        return TRUE;
    }
    {
        unsigned char p = (unsigned char)t[pos - 1];

        if (p <= ' ') {
            return TRUE;
        }
        if (strchr("([{\"'<", (char)p) != NULL) {
            return TRUE;
        }
        if (chatMdIsAsciiAlnum((char)p) || p == '_' || p == '.') {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * If `t[pos…]` starts http(s):// with a sane prefix, set *outEnd to byte after the URL
 * and return scheme length (7 or 8); else 0 and *outEnd == pos.
 */
static ULONG chatMdUrlSchemeSpanAt(const char *t, ULONG len, ULONG pos, ULONG *outEnd) {
    ULONG scheme = 0;

    *outEnd = pos;
    if (pos + 8 <= len && strncmp(t + pos, "https://", 8) == 0) {
        scheme = 8;
    } else if (pos + 7 <= len && strncmp(t + pos, "http://", 7) == 0) {
        scheme = 7;
    } else {
        return 0;
    }
    if (!chatMdUrlSchemeOkPrefix(t, pos)) {
        return 0;
    }
    *outEnd = pos + scheme;
    while (*outEnd < len && chatMdIsUrlBodyByte((unsigned char)t[*outEnd])) {
        (*outEnd)++;
    }
    while (*outEnd > pos + scheme) {
        unsigned char c = (unsigned char)t[*outEnd - 1];

        if (strchr(".,;:!?)]}", (char)c) != NULL) {
            (*outEnd)--;
        } else {
            break;
        }
    }
    if (*outEnd <= pos + scheme) {
        *outEnd = pos + scheme;
    }
    return scheme;
}

static void chatOutputMdLinkSpansPush(ULONG docStart, ULONG docEndExclusive, const char *url,
                                      ULONG urlLen) {
    if (chatOutputMdLinkSpanCount >= CHAT_OUTPUT_MD_LINK_SPANS_MAX || url == NULL ||
        urlLen >= CHAT_OUTPUT_URL_MAX) {
        return;
    }
    memcpy(chatOutputMdLinkSpans[chatOutputMdLinkSpanCount].url, url, urlLen);
    chatOutputMdLinkSpans[chatOutputMdLinkSpanCount].url[urlLen] = '\0';
    chatOutputMdLinkSpans[chatOutputMdLinkSpanCount].start = docStart;
    chatOutputMdLinkSpans[chatOutputMdLinkSpanCount].end = docEndExclusive;
    chatOutputMdLinkSpanCount++;
}

static void chatMdEmit(ULONG *outPos, char *outUtf8, UBYTE *outStyles, char ch, UBYTE style);

static void chatMdEmitMarkdownLinkDisplay(const char *inUtf8, ULONG labelStart, ULONG labelLen,
                                         const char *urlBytes, ULONG urlLen, ULONG *outPos,
                                         char *outUtf8, UBYTE *outStyles) {
    ULONG ds = *outPos;
    ULONG k;

    if (labelLen > 0) {
        for (k = 0; k < labelLen; k++) {
            chatMdEmit(outPos, outUtf8, outStyles, inUtf8[labelStart + k],
                       CHAT_OUTPUT_STYLE_URL_HOTSPOT);
        }
        chatOutputMdLinkSpansPush(ds, *outPos, urlBytes, urlLen);
    } else {
        for (k = 0; k < urlLen; k++) {
            chatMdEmit(outPos, outUtf8, outStyles, urlBytes[k], CHAT_OUTPUT_STYLE_URL_HOTSPOT);
        }
        chatOutputMdLinkSpansPush(ds, *outPos, urlBytes, urlLen);
    }
}

/**
 * Parse `[label](http…url…)` with url ending at first `)`. `bracketPos` must index `[`.
 * On success *outAfter is the index after the closing `)` of the URL.
 */
static BOOL chatMdParseInnerMarkdownHttpLink(const char *t, ULONG len, ULONG bracketPos,
                                            ULONG *outAfter, ULONG *outLabelStart,
                                            ULONG *outLabelLen, ULONG *outUrlStart,
                                            ULONG *outUrlLen) {
    ULONG labelStart;
    ULONG closeBracket;
    ULONG urlStart;
    ULONG p;

    if (bracketPos >= len || t[bracketPos] != '[') {
        return FALSE;
    }
    labelStart = bracketPos + 1;
    closeBracket = labelStart;
    while (closeBracket < len && t[closeBracket] != ']') {
        if (t[closeBracket] == '\n' || t[closeBracket] == '\r') {
            return FALSE;
        }
        closeBracket++;
    }
    if (closeBracket >= len || t[closeBracket] != ']' || closeBracket + 1 >= len ||
        t[closeBracket + 1] != '(') {
        return FALSE;
    }
    urlStart = closeBracket + 2;
    if (urlStart + 7 > len) {
        return FALSE;
    }
    if (strncmp(t + urlStart, "https://", 8) != 0 && strncmp(t + urlStart, "http://", 7) != 0) {
        return FALSE;
    }
    p = urlStart;
    while (p < len) {
        if (t[p] == ')') {
            *outAfter = p + 1;
            *outLabelStart = labelStart;
            *outLabelLen = closeBracket - labelStart;
            *outUrlStart = urlStart;
            *outUrlLen = p - urlStart;
            return TRUE;
        }
        if (!chatMdIsUrlBodyByte((unsigned char)t[p])) {
            return FALSE;
        }
        p++;
    }
    return FALSE;
}

static ULONG chatMdTryConsumeMarkdownHttpLinks(const char *inUtf8, ULONG len, ULONG i,
                                              ULONG *outPos, char *outUtf8, UBYTE *outStyles,
                                              UBYTE parenStyle) {
    ULONG after;
    ULONG ls;
    ULONG ll;
    ULONG urlStart;
    ULONG urlLen;

    if (i + 2 < len && inUtf8[i] == '(' && inUtf8[i + 1] == '[') {
        if (!chatMdParseInnerMarkdownHttpLink(inUtf8, len, i + 1, &after, &ls, &ll, &urlStart,
                                              &urlLen)) {
            return 0;
        }
        if (after >= len || inUtf8[after] != ')') {
            return 0;
        }
        chatMdEmit(outPos, outUtf8, outStyles, '(', parenStyle);
        chatMdEmitMarkdownLinkDisplay(inUtf8, ls, ll, inUtf8 + urlStart, urlLen, outPos, outUtf8,
                                      outStyles);
        chatMdEmit(outPos, outUtf8, outStyles, ')', parenStyle);
        return after + 1 - i;
    }
    if (inUtf8[i] == '[') {
        if (!chatMdParseInnerMarkdownHttpLink(inUtf8, len, i, &after, &ls, &ll, &urlStart,
                                              &urlLen)) {
            return 0;
        }
        chatMdEmitMarkdownLinkDisplay(inUtf8, ls, ll, inUtf8 + urlStart, urlLen, outPos, outUtf8,
                                      outStyles);
        return after - i;
    }
    return 0;
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

/** Past leading whitespace and markdown list markers (-, *, +, 1. ). */
static ULONG chatMdSkipLinePrefixBounded(const char *text, ULONG len, ULONG pos) {
    BOOL advanced;

    while (pos < len && text[pos] != '\n' &&
           (text[pos] == ' ' || text[pos] == '\t')) {
        pos++;
    }
    do {
        advanced = FALSE;
        if (pos + 1 < len && text[pos] != '\n') {
            if ((text[pos] == '-' || text[pos] == '*' || text[pos] == '+') &&
                text[pos + 1] == ' ') {
                pos += 2;
                advanced = TRUE;
            } else if (text[pos] >= '0' && text[pos] <= '9') {
                ULONG digitStart = pos;

                while (pos < len && text[pos] >= '0' && text[pos] <= '9') {
                    pos++;
                }
                if (pos > digitStart && pos + 1 < len && text[pos] == '.' &&
                    text[pos + 1] == ' ') {
                    pos += 2;
                    advanced = TRUE;
                } else {
                    pos = digitStart;
                }
            }
        }
        if (advanced) {
            while (pos < len && text[pos] != '\n' &&
                   (text[pos] == ' ' || text[pos] == '\t')) {
                pos++;
            }
        }
    } while (advanced);
    return pos;
}

static const char *chatMdSkipLinePrefixZ(const char *line) {
    const char *p = line;
    BOOL advanced;

    if (line == NULL) {
        return "";
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    do {
        advanced = FALSE;
        if ((*p == '-' || *p == '*' || *p == '+') && p[1] == ' ') {
            p += 2;
            advanced = TRUE;
        } else if (*p >= '0' && *p <= '9') {
            const char *digitStart = p;

            while (*p >= '0' && *p <= '9') {
                p++;
            }
            if (p > digitStart && *p == '.' && p[1] == ' ') {
                p += 2;
                advanced = TRUE;
            } else {
                p = digitStart;
            }
        }
        if (advanced) {
            while (*p == ' ' || *p == '\t') {
                p++;
            }
        }
    } while (advanced);
    return p;
}

/** TRUE if the line contains `[Codeblock n]` (indent / list prefix allowed). */
static BOOL chatMdFindCodeblockPlaceholderOnLine(const char *text, ULONG len,
                                                 ULONG lineStart, ULONG *outPos) {
    ULONG lineEnd = lineStart;
    ULONG p;

    while (lineEnd < len && text[lineEnd] != '\n') {
        lineEnd++;
    }
    p = chatMdSkipLinePrefixBounded(text, len, lineStart);
    if (p < lineEnd && chatMdIsCodeblockPlaceholder(text, len, p)) {
        if (outPos != NULL) {
            *outPos = p;
        }
        return TRUE;
    }
    for (p = lineStart; p < lineEnd; p++) {
        if (text[p] == '[' && chatMdIsCodeblockPlaceholder(text, len, p)) {
            if (outPos != NULL) {
                *outPos = p;
            }
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL chatMdInSkippedRegion(const char *text, ULONG len, ULONG pos) {
    ULONG lineStart = pos;

    while (lineStart > 0 && text[lineStart - 1] != '\n') {
        lineStart--;
    }
    return chatMdFindCodeblockPlaceholderOnLine(text, len, lineStart, NULL);
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
    const BOOL stripMidi = config.markdownFormatting;

    if (inUtf8 == NULL || outUtf8 == NULL || outStyles == NULL || inLen == 0) {
        if (outUtf8 != NULL) {
            outUtf8[0] = '\0';
        }
        return 0;
    }

    chatOutputScintillaForgetMarkdownLinkSpans();
    chatMdInitStack(&styleStack);

    /*
     * Heading detection used to rescan to line start for every byte when
     * stripMidi is on — O(n²) on long single lines (huge assistant payloads),
     * which freezes the UI when switching chats. Refresh line bounds only at
     * each physical line start.
     */
    ULONG mdLineStart = 0;
    ULONG mdLineEnd = 0;
    ULONG mdContentStart = 0;

    for (i = 0; i < inLen; i++) {
        ULONG linkEat;
        UBYTE role =
            (inRoleStyles != NULL) ? inRoleStyles[i] : CHAT_OUTPUT_STYLE_ASSISTANT;

        if (role == CHAT_OUTPUT_STYLE_USER) {
            linkEat = chatMdTryConsumeMarkdownHttpLinks(inUtf8, inLen, i, &outPos, outUtf8,
                                                       outStyles, CHAT_OUTPUT_STYLE_USER);
            if (linkEat > 0) {
                i += linkEat - 1;
                continue;
            }
            chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i], CHAT_OUTPUT_STYLE_USER);
            continue;
        }
        if (chatMdInSkippedRegion(inUtf8, inLen, i)) {
            chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i],
                       CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT);
            continue;
        }
        if (inUtf8[i] == '\n') {
            chatMdEmit(&outPos, outUtf8, outStyles, '\n',
                       CHAT_OUTPUT_STYLE_ASSISTANT);
            continue;
        }

        if (stripMidi) {
            if (i == 0 || inUtf8[i - 1] == '\n') {
                mdLineStart = i;
                mdLineEnd = i;
                while (mdLineEnd < inLen && inUtf8[mdLineEnd] != '\n') {
                    mdLineEnd++;
                }
                mdContentStart =
                    chatMdHeadingContentStart(inUtf8, inLen, mdLineStart);
            }
            if (mdContentStart > 0) {
                if (i < mdContentStart) {
                    continue;
                }
                if (i < mdLineEnd) {
                    linkEat = chatMdTryConsumeMarkdownHttpLinks(
                        inUtf8, inLen, i, &outPos, outUtf8, outStyles,
                        CHAT_OUTPUT_STYLE_MD_BOLD);
                    if (linkEat > 0) {
                        i += linkEat - 1;
                        continue;
                    }
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

            linkEat = chatMdTryConsumeMarkdownHttpLinks(
                inUtf8, inLen, i, &outPos, outUtf8, outStyles,
                chatMdStyleFromStack(&styleStack));
            if (linkEat > 0) {
                i += linkEat - 1;
                continue;
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
            continue;
        }

        linkEat = chatMdTryConsumeMarkdownHttpLinks(inUtf8, inLen, i, &outPos, outUtf8,
                                                   outStyles, CHAT_OUTPUT_STYLE_ASSISTANT);
        if (linkEat > 0) {
            i += linkEat - 1;
            continue;
        }
        chatMdEmit(&outPos, outUtf8, outStyles, inUtf8[i], CHAT_OUTPUT_STYLE_ASSISTANT);
    }

    outUtf8[outPos] = '\0';
    return outPos;

#define CHAT_MD_TABLE_MAX_COLS 16
#define CHAT_MD_TABLE_MAX_ROWS 64

/** UTF-8 codepoint count (display width for pipe-table padding). */
static ULONG chatMdUtf8CharCount(const char *s, ULONG byteLen) {
    ULONG i = 0;
    ULONG n = 0;

    while (i < byteLen) {
        unsigned char c = (unsigned char)s[i];

        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < byteLen) {
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < byteLen) {
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < byteLen) {
            i += 4;
        } else {
            i++;
        }
        n++;
    }
    return n;
}

static void chatMdShiftMdLinkSpansFrom(ULONG docPos, LONG delta) {
    ULONG n;

    if (delta == 0) {
        return;
    }
    for (n = 0; n < chatOutputMdLinkSpanCount; n++) {
        if (chatOutputMdLinkSpans[n].start >= docPos) {
            chatOutputMdLinkSpans[n].start =
                (ULONG)((LONG)chatOutputMdLinkSpans[n].start + delta);
        }
        if (chatOutputMdLinkSpans[n].end >= docPos) {
            chatOutputMdLinkSpans[n].end =
                (ULONG)((LONG)chatOutputMdLinkSpans[n].end + delta);
        }
    }
}

static void chatMdRemapMdLinkSpansOnLine(ULONG oldLineStart, ULONG oldLineLen,
                                         ULONG newLineStart, ULONG newLineLen,
                                         const char *oldLine, const char *newLine) {
    ULONG oldLineEnd = oldLineStart + oldLineLen;
    ULONG n;

    for (n = 0; n < chatOutputMdLinkSpanCount; n++) {
        ULONG s = chatOutputMdLinkSpans[n].start;
        ULONG e = chatOutputMdLinkSpans[n].end;

        if (e <= oldLineStart || s >= oldLineEnd) {
            continue;
        }
        if (s >= oldLineStart && e <= oldLineEnd) {
            ULONG relS = s - oldLineStart;
            ULONG relE = e - oldLineStart;
            ULONG mapS = 0;
            ULONG mapE = 0;
            ULONG oi = 0;
            ULONG ni = 0;
            BOOL haveS = FALSE;
            BOOL haveE = FALSE;

            while (oi < oldLineLen && ni < newLineLen) {
                if (!haveS && oi == relS) {
                    mapS = ni;
                    haveS = TRUE;
                }
                if (!haveE && oi == relE) {
                    mapE = ni;
                    haveE = TRUE;
                }
                if (oi < oldLineLen && ni < newLineLen &&
                    oldLine[oi] == newLine[ni]) {
                    oi++;
                    ni++;
                    continue;
                }
                if (oi < oldLineLen && oldLine[oi] == ' ') {
                    oi++;
                    continue;
                }
                if (ni < newLineLen && newLine[ni] == ' ') {
                    ni++;
                    continue;
                }
                break;
            }
            if (!haveS && oi == relS && ni <= newLineLen) {
                mapS = ni;
            }
            if (!haveE && oi == relE && ni <= newLineLen) {
                mapE = ni;
            }
            if (haveS && haveE && mapE >= mapS) {
                chatOutputMdLinkSpans[n].start = newLineStart + mapS;
                chatOutputMdLinkSpans[n].end = newLineStart + mapE;
            }
        }
    }
}

static BOOL chatMdLineIsUserRole(const UBYTE *styles, ULONG lineStart, ULONG lineEnd) {
    ULONG p;

    if (styles == NULL) {
        return FALSE;
    }
    for (p = lineStart; p < lineEnd; p++) {
        if (styles[p] == CHAT_OUTPUT_STYLE_USER) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL chatMdLineSkipsPipeTable(const char *text, ULONG len, ULONG lineStart) {
    ULONG lineEnd = lineStart;

    while (lineEnd < len && text[lineEnd] != '\n') {
        lineEnd++;
    }
    if (chatMdFindCodeblockPlaceholderOnLine(text, len, lineStart, NULL)) {
        return TRUE;
    }
    return FALSE;
}

/** After list/indent prefix, line must contain `|`. */
static BOOL chatMdIsPipeTableRowLine(const char *text, ULONG len, ULONG lineStart,
                                   ULONG lineEnd) {
    ULONG p = chatMdSkipLinePrefixBounded(text, len, lineStart);
    ULONG q;

    if (p >= lineEnd) {
        return FALSE;
    }
    for (q = p; q < lineEnd; q++) {
        if (text[q] == '|') {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL chatMdIsPipeTableSeparatorLine(const char *text, ULONG len, ULONG lineStart,
                                           ULONG lineEnd) {
    ULONG p = chatMdSkipLinePrefixBounded(text, len, lineStart);
    BOOL seenPipe = FALSE;

    if (p >= lineEnd || text[p] != '|') {
        return FALSE;
    }
    p++;
    while (p < lineEnd) {
        if (text[p] == '|') {
            seenPipe = TRUE;
            p++;
            continue;
        }
        if (text[p] == '-' || text[p] == ':' || text[p] == ' ') {
            p++;
            continue;
        }
        return FALSE;
    }
    return seenPipe;
}

static ULONG chatMdTrimAsciiSpaces(const char *s, ULONG len, ULONG *outStart) {
    ULONG start = 0;
    ULONG end = len;

    while (start < end && (s[start] == ' ' || s[start] == '\t')) {
        start++;
    }
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        end--;
    }
    *outStart = start;
    return end - start;
}

/**
 * Split a pipe row into cells (no nested `|`). Returns column count or 0 on failure.
 */
static ULONG chatMdParsePipeRowCells(const char *text, ULONG len, ULONG lineStart,
                                     ULONG lineEnd, ULONG cellStarts[CHAT_MD_TABLE_MAX_COLS],
                                     ULONG cellLens[CHAT_MD_TABLE_MAX_COLS]) {
    ULONG p = chatMdSkipLinePrefixBounded(text, len, lineStart);
    ULONG col = 0;

    if (p >= lineEnd || text[p] != '|') {
        return 0;
    }
    p++;
    while (p <= lineEnd && col < CHAT_MD_TABLE_MAX_COLS) {
        ULONG cellStart = p;
        ULONG cellEnd = p;

        while (cellEnd < lineEnd && text[cellEnd] != '|') {
            cellEnd++;
        }
        {
            ULONG trimAt;
            ULONG trimLen = chatMdTrimAsciiSpaces(text + cellStart, cellEnd - cellStart,
                                                  &trimAt);

            cellStarts[col] = cellStart + trimAt;
            cellLens[col] = trimLen;
            col++;
        }
        if (cellEnd >= lineEnd) {
            break;
        }
        p = cellEnd + 1;
    }
    if (col == 0) {
        return 0;
    }
    return col;
}

static BOOL chatMdSeparatorMatchesColumns(const char *text, ULONG len, ULONG lineStart,
                                          ULONG lineEnd, ULONG ncol) {
    ULONG cellStarts[CHAT_MD_TABLE_MAX_COLS];
    ULONG cellLens[CHAT_MD_TABLE_MAX_COLS];
    ULONG c;
    ULONG got = chatMdParsePipeRowCells(text, len, lineStart, lineEnd, cellStarts, cellLens);
    BOOL seenDashSegment = FALSE;

    if (got != ncol) {
        return FALSE;
    }
    for (c = 0; c < ncol; c++) {
        ULONG i;

        /* GFM: leading/trailing `|` → empty first/last cell; still a valid separator row */
        if (cellLens[c] == 0) {
            continue;
        }
        for (i = 0; i < cellLens[c]; i++) {
            char ch = text[cellStarts[c] + i];

            if (ch != '-' && ch != ':' && ch != ' ') {
                return FALSE;
            }
        }
        for (i = 0; i < cellLens[c]; i++) {
            if (text[cellStarts[c] + i] == '-') {
                seenDashSegment = TRUE;
                break;
            }
        }
        if (i == cellLens[c]) {
            return FALSE;
        }
    }
    return seenDashSegment;
}

static void chatMdCopyEmit(ULONG *outPos, char *outUtf8, UBYTE *outStyles, const char *src,
                           const UBYTE *srcStyles, ULONG srcOff, ULONG count, UBYTE padStyle) {
    ULONG k;

    for (k = 0; k < count; k++) {
        UBYTE st = (srcStyles != NULL) ? srcStyles[srcOff + k] : padStyle;

        outUtf8[*outPos] = src[srcOff + k];
        outStyles[*outPos] = st;
        (*outPos)++;
    }
}

/** True if column `colIdx` is empty (trimmed) on every line; `ncolPerRow` = parsed cells per line (constant). */
static BOOL chatMdPipeTableColumnAllEmpty(const char *text, ULONG len,
                                          const ULONG *lineStarts, const ULONG *lineEnds,
                                          ULONG rowCount, ULONG ncolPerRow, ULONG colIdx) {
    ULONG r;
    ULONG cs[CHAT_MD_TABLE_MAX_COLS];
    ULONG cl[CHAT_MD_TABLE_MAX_COLS];

    if (colIdx >= ncolPerRow) {
        return FALSE;
    }
    for (r = 0; r < rowCount; r++) {
        ULONG got = chatMdParsePipeRowCells(text, len, lineStarts[r], lineEnds[r], cs, cl);

        if (got != ncolPerRow) {
            return FALSE;
        }
        if (cl[colIdx] != 0) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL chatMdEmitPaddedPipeRow(const char *text, ULONG len, const UBYTE *styles,
                                    ULONG lineStart, ULONG lineEnd, ULONG ncol,
                                    const ULONG colWidths[CHAT_MD_TABLE_MAX_COLS],
                                    ULONG *outPos, char *outUtf8, UBYTE *outStyles) {
    ULONG cellStarts[CHAT_MD_TABLE_MAX_COLS];
    ULONG cellLens[CHAT_MD_TABLE_MAX_COLS];
    ULONG c;
    ULONG pipeAt = chatMdSkipLinePrefixBounded(text, len, lineStart);
    ULONG got = chatMdParsePipeRowCells(text, len, lineStart, lineEnd, cellStarts, cellLens);
    UBYTE lineStyle = CHAT_OUTPUT_STYLE_ASSISTANT;

    if (got < ncol || pipeAt >= lineEnd || text[pipeAt] != '|') {
        return FALSE;
    }
    if (styles != NULL && lineStart < lineEnd) {
        lineStyle = styles[lineStart];
    }
    if (pipeAt > lineStart) {
        chatMdCopyEmit(outPos, outUtf8, outStyles, text, styles, lineStart, pipeAt - lineStart,
                       lineStyle);
    }
    chatMdEmit(outPos, outUtf8, outStyles, '|', lineStyle);
    for (c = 0; c < ncol; c++) {
        UBYTE cellStyle = lineStyle;
        ULONG pad;

        chatMdEmit(outPos, outUtf8, outStyles, ' ', lineStyle);
        if (cellLens[c] > 0 && styles != NULL) {
            cellStyle = styles[cellStarts[c]];
        }
        chatMdCopyEmit(outPos, outUtf8, outStyles, text, styles, cellStarts[c], cellLens[c],
                       cellStyle);
        pad = colWidths[c] - chatMdUtf8CharCount(text + cellStarts[c], cellLens[c]);
        while (pad > 0) {
            chatMdEmit(outPos, outUtf8, outStyles, ' ', cellStyle);
            pad--;
        }
        chatMdEmit(outPos, outUtf8, outStyles, ' ', lineStyle);
        chatMdEmit(outPos, outUtf8, outStyles, '|', lineStyle);
    }
    return TRUE;
}

/**
 * Separator cell: preserve GFM leading/trailing ':' when present; fill to `targetChars`
 * with '-' (ASCII width = UTF-8 count for these chars).
 */
static void chatMdEmitSeparatorCellInterior(ULONG *outPos, char *outUtf8, UBYTE *outStyles,
                                            const char *cellBase, ULONG cellLen, ULONG targetChars,
                                            UBYTE lineStyle) {
    BOOL leadColon = FALSE;
    BOOL trailColon = FALSE;
    ULONG dashCount;
    ULONG k;

    if (targetChars == 0) {
        return;
    }
    if (cellLen == 0) {
        for (k = 0; k < targetChars; k++) {
            chatMdEmit(outPos, outUtf8, outStyles, '-', lineStyle);
        }
        return;
    }
    if (cellLen >= 1 && cellBase[0] == ':') {
        leadColon = TRUE;
    }
    if (cellLen >= 2 && cellBase[cellLen - 1] == ':') {
        trailColon = TRUE;
    }
    if (leadColon && trailColon && targetChars < 2) {
        leadColon = FALSE;
        trailColon = FALSE;
    }
    dashCount = targetChars;
    if (leadColon && dashCount > 0) {
        dashCount--;
    }
    if (trailColon && dashCount > 0) {
        dashCount--;
    }
    if (leadColon && targetChars > 0) {
        chatMdEmit(outPos, outUtf8, outStyles, ':', lineStyle);
    }
    for (k = 0; k < dashCount; k++) {
        chatMdEmit(outPos, outUtf8, outStyles, '-', lineStyle);
    }
    if (trailColon && targetChars > 0) {
        chatMdEmit(outPos, outUtf8, outStyles, ':', lineStyle);
    }
}

/** Separator row: match column widths; optional ':' from original GFM alignment. */
static BOOL chatMdEmitSyntheticSeparatorRow(const char *text, ULONG len, const UBYTE *styles,
                                           ULONG lineStart, ULONG lineEnd, ULONG ncol,
                                           const ULONG colWidths[CHAT_MD_TABLE_MAX_COLS],
                                           ULONG *outPos, char *outUtf8, UBYTE *outStyles) {
    ULONG pipeAt = chatMdSkipLinePrefixBounded(text, len, lineStart);
    ULONG c;
    UBYTE lineStyle = CHAT_OUTPUT_STYLE_ASSISTANT;
    ULONG scs[CHAT_MD_TABLE_MAX_COLS];
    ULONG scl[CHAT_MD_TABLE_MAX_COLS];
    ULONG gotSep;

    if (pipeAt >= lineEnd || text[pipeAt] != '|') {
        return FALSE;
    }
    if (styles != NULL && lineStart < lineEnd) {
        lineStyle = styles[lineStart];
    }
    gotSep = chatMdParsePipeRowCells(text, len, lineStart, lineEnd, scs, scl);
    if (gotSep < ncol) {
        return FALSE;
    }
    if (pipeAt > lineStart) {
        chatMdCopyEmit(outPos, outUtf8, outStyles, text, styles, lineStart, pipeAt - lineStart,
                       lineStyle);
    }
    chatMdEmit(outPos, outUtf8, outStyles, '|', lineStyle);
    for (c = 0; c < ncol; c++) {
        chatMdEmit(outPos, outUtf8, outStyles, ' ', lineStyle);
        chatMdEmitSeparatorCellInterior(outPos, outUtf8, outStyles, text + scs[c], scl[c],
                                        colWidths[c], lineStyle);
        chatMdEmit(outPos, outUtf8, outStyles, ' ', lineStyle);
        chatMdEmit(outPos, outUtf8, outStyles, '|', lineStyle);
    }
    return TRUE;
}

static ULONG chatMdFormatPipeTableBlock(const char *text, const UBYTE *styles, ULONG len,
                                        ULONG blockStart, ULONG blockEnd, ULONG *outPos,
                                        char *outUtf8, UBYTE *outStyles, ULONG cap) {
    ULONG lineStarts[CHAT_MD_TABLE_MAX_ROWS];
    ULONG lineEnds[CHAT_MD_TABLE_MAX_ROWS];
    ULONG rowCount = 0;
    ULONG pos = blockStart;
    ULONG ncol;
    ULONG colWidths[CHAT_MD_TABLE_MAX_COLS];
    ULONG cellStarts[CHAT_MD_TABLE_MAX_COLS];
    ULONG cellLens[CHAT_MD_TABLE_MAX_COLS];
    ULONG r;
    ULONG c;
    ULONG oldBlockLen = blockEnd - blockStart;
    ULONG newBlockStart = *outPos;

    while (pos < blockEnd && rowCount < CHAT_MD_TABLE_MAX_ROWS) {
        ULONG lineEnd = pos;

        while (lineEnd < len && text[lineEnd] != '\n') {
            lineEnd++;
        }
        lineStarts[rowCount] = pos;
        lineEnds[rowCount] = lineEnd;
        rowCount++;
        pos = (lineEnd < len) ? lineEnd + 1 : lineEnd;
    }
    if (rowCount < 2) {
        return 0;
    }
    ncol = chatMdParsePipeRowCells(text, len, lineStarts[0], lineEnds[0], cellStarts, cellLens);
    if (ncol < 1 || ncol > CHAT_MD_TABLE_MAX_COLS) {
        return 0;
    }
    {
        ULONG ncol0 = ncol;

        if (!chatMdIsPipeTableSeparatorLine(text, len, lineStarts[1], lineEnds[1]) ||
            !chatMdSeparatorMatchesColumns(text, len, lineStarts[1], lineEnds[1], ncol0)) {
            return 0;
        }
        for (r = 0; r < rowCount; r++) {
            ULONG got;

            if (r == 1) {
                continue;
            }
            if (!chatMdIsPipeTableRowLine(text, len, lineStarts[r], lineEnds[r])) {
                return 0;
            }
            got = chatMdParsePipeRowCells(text, len, lineStarts[r], lineEnds[r], cellStarts,
                                          cellLens);
            if (got != ncol0) {
                return 0;
            }
        }
        while (ncol > 1 &&
               chatMdPipeTableColumnAllEmpty(text, len, lineStarts, lineEnds, rowCount, ncol0,
                                             ncol - 1)) {
            ncol--;
        }
        if (ncol == 0) {
            return 0;
        }
    }
    for (c = 0; c < ncol; c++) {
        colWidths[c] = 0;
    }
    for (r = 0; r < rowCount; r++) {
        if (r == 1) {
            continue;
        }
        {
            chatMdParsePipeRowCells(text, len, lineStarts[r], lineEnds[r], cellStarts, cellLens);
            for (c = 0; c < ncol; c++) {
                ULONG w = chatMdUtf8CharCount(text + cellStarts[c], cellLens[c]);

                if (w > colWidths[c]) {
                    colWidths[c] = w;
                }
            }
        }
    }
    for (r = 0; r < rowCount; r++) {
        ULONG oldLineStart = lineStarts[r];
        ULONG oldLineLen = lineEnds[r] - lineStarts[r];
        ULONG newLineStart = *outPos;
        char oldLineBuf[4096];
        char newLineBuf[4096];
        ULONG rowBudget;
        ULONG pipeAtB = chatMdSkipLinePrefixBounded(text, len, lineStarts[r]);
        ULONG b;

        rowBudget = (pipeAtB > lineStarts[r]) ? (pipeAtB - lineStarts[r]) : 0;
        rowBudget += 2;
        for (b = 0; b < ncol; b++) {
            rowBudget += 4 + colWidths[b];
        }
        if (*outPos + rowBudget > cap) {
            return 0;
        }
        if (r == 1) {
            if (!chatMdEmitSyntheticSeparatorRow(text, len, styles, lineStarts[r], lineEnds[r],
                                                 ncol, colWidths, outPos, outUtf8, outStyles)) {
                return 0;
            }
        } else if (!chatMdEmitPaddedPipeRow(text, len, styles, lineStarts[r], lineEnds[r], ncol,
                                            colWidths, outPos, outUtf8, outStyles)) {
            return 0;
        }
        if (oldLineLen < sizeof(oldLineBuf)) {
            memcpy(oldLineBuf, text + oldLineStart, oldLineLen);
            oldLineBuf[oldLineLen] = '\0';
        }
        {
            ULONG newLineLen = *outPos - newLineStart;

            if (newLineLen < sizeof(newLineBuf)) {
                memcpy(newLineBuf, outUtf8 + newLineStart, newLineLen);
                newLineBuf[newLineLen] = '\0';
            }
            if (oldLineLen < sizeof(oldLineBuf) && newLineLen < sizeof(newLineBuf)) {
                chatMdRemapMdLinkSpansOnLine(oldLineStart, oldLineLen, newLineStart, newLineLen,
                                             oldLineBuf, newLineBuf);
            }
        }
        if (lineEnds[r] < len) {
            ULONG nlStyle = (styles != NULL) ? styles[lineEnds[r]] : CHAT_OUTPUT_STYLE_ASSISTANT;

            chatMdEmit(outPos, outUtf8, outStyles, '\n', nlStyle);
        }
    }
  {
    LONG blockDelta = (LONG)(*outPos - newBlockStart) - (LONG)oldBlockLen;

    if (blockDelta != 0) {
        chatMdShiftMdLinkSpansFrom(blockEnd, blockDelta);
    }
  }
    return blockEnd - blockStart;
}

ULONG chatOutputScintillaFormatPipeTables(char *text, UBYTE *styles, ULONG len, ULONG cap) {
    char *outUtf8;
    UBYTE *outStyles;
    ULONG outPos = 0;
    ULONG pos = 0;

    if (text == NULL || styles == NULL || len == 0 || cap < len + 1) {
        return len;
    }
    outUtf8 = (char *)AllocVec(cap, MEMF_ANY);
    outStyles = (UBYTE *)AllocVec(cap, MEMF_ANY);
    if (outUtf8 == NULL || outStyles == NULL) {
        if (outUtf8 != NULL) {
            FreeVec(outUtf8);
        }
        if (outStyles != NULL) {
            FreeVec(outStyles);
        }
        return len;
    }

    while (pos < len) {
        ULONG lineEnd = pos;
        ULONG blockEnd;
        ULONG consumed;

        while (lineEnd < len && text[lineEnd] != '\n') {
            lineEnd++;
        }
        if (chatMdLineIsUserRole(styles, pos, lineEnd) ||
            chatMdLineSkipsPipeTable(text, len, pos) ||
            !chatMdIsPipeTableRowLine(text, len, pos, lineEnd)) {
            if (outPos + (lineEnd - pos) + 1 > cap) {
                break;
            }
            chatMdCopyEmit(&outPos, outUtf8, outStyles, text, styles, pos, lineEnd - pos,
                           CHAT_OUTPUT_STYLE_ASSISTANT);
            if (lineEnd < len) {
                chatMdEmit(&outPos, outUtf8, outStyles, '\n', styles[lineEnd]);
            }
            pos = (lineEnd < len) ? lineEnd + 1 : lineEnd;
            continue;
        }
        blockEnd = (lineEnd < len) ? lineEnd + 1 : lineEnd;
        if (blockEnd < len) {
            ULONG line2End = blockEnd;

            while (line2End < len && text[line2End] != '\n') {
                line2End++;
            }
            if (line2End > blockEnd &&
                chatMdIsPipeTableSeparatorLine(text, len, blockEnd, line2End) &&
                !chatMdLineIsUserRole(styles, blockEnd, line2End) &&
                !chatMdLineSkipsPipeTable(text, len, blockEnd)) {
                ULONG scan = (line2End < len) ? line2End + 1 : line2End;

                blockEnd = scan;
                while (scan < len) {
                    ULONG rowEnd = scan;

                    while (rowEnd < len && text[rowEnd] != '\n') {
                        rowEnd++;
                    }
                    if (rowEnd == scan) {
                        break;
                    }
                    if (text[scan] == '\n' && rowEnd == scan) {
                        break;
                    }
                    if (!chatMdIsPipeTableRowLine(text, len, scan, rowEnd) ||
                        chatMdLineIsUserRole(styles, scan, rowEnd) ||
                        chatMdLineSkipsPipeTable(text, len, scan)) {
                        break;
                    }
                    blockEnd = (rowEnd < len) ? rowEnd + 1 : rowEnd;
                    scan = blockEnd;
                }
                consumed = chatMdFormatPipeTableBlock(text, styles, len, pos, blockEnd,
                                                      &outPos, outUtf8, outStyles, cap);
                if (consumed > 0) {
                    pos = blockEnd;
                    continue;
                }
            }
        }
        if (outPos + (lineEnd - pos) + 1 > cap) {
            break;
        }
        chatMdCopyEmit(&outPos, outUtf8, outStyles, text, styles, pos, lineEnd - pos,
                       CHAT_OUTPUT_STYLE_ASSISTANT);
        if (lineEnd < len) {
            chatMdEmit(&outPos, outUtf8, outStyles, '\n', styles[lineEnd]);
        }
        pos = (lineEnd < len) ? lineEnd + 1 : lineEnd;
    }

    if (pos < len || outPos == 0) {
        FreeVec(outUtf8);
        FreeVec(outStyles);
        return len;
    }
    memcpy(text, outUtf8, outPos);
    memcpy(styles, outStyles, outPos);
    text[outPos] = '\0';
    FreeVec(outUtf8);
    FreeVec(outStyles);
    return outPos;
}

/** Mark each `[Codeblock n]` line with the hotspot style byte. */
static void chatOutputScintillaApplyCodeblockHotspotStyles(const char *utf8,
                                                           UBYTE *styleBytes,
                                                           ULONG byteLen) {
    ULONG pos = 0;

    if (utf8 == NULL || styleBytes == NULL || byteLen == 0) {
        return;
    }

    while (pos < byteLen) {
        ULONG lineStart = pos;
        ULONG lineEnd = pos;

        while (lineEnd < byteLen && utf8[lineEnd] != '\n') {
            lineEnd++;
        }
        if (chatMdFindCodeblockPlaceholderOnLine(utf8, byteLen, lineStart, NULL)) {
            memset(styleBytes + lineStart, CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT,
                   lineEnd - lineStart);
        }
        if (lineEnd >= byteLen) {
            break;
        }
        pos = lineEnd + 1;
    }
}

/** Mark bare http(s):// spans (ASCII path) with URL hotspot; skips codeblock lines. */
static void chatOutputScintillaApplyUrlHotspotStyles(const char *utf8, UBYTE *styleBytes,
                                                     ULONG byteLen) {
    ULONG pos = 0;

    if (utf8 == NULL || styleBytes == NULL || byteLen == 0) {
        return;
    }
    while (pos < byteLen) {
        ULONG end = pos;
        ULONG schemeLen;

        if (styleBytes[pos] == CHAT_OUTPUT_STYLE_CODEBLOCK_HOTSPOT) {
            pos++;
            continue;
        }
        schemeLen = chatMdUrlSchemeSpanAt(utf8, byteLen, pos, &end);
        if (schemeLen == 0) {
            pos++;
            continue;
        }
        if (end > pos) {
            memset(styleBytes + pos, CHAT_OUTPUT_STYLE_URL_HOTSPOT, end - pos);
            pos = end;
        } else {
            pos++;
        }
    }
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
    chatOutputScintillaApplyLineWrap(sci);
}

void chatOutputScintillaApplyLineWrap(Object *sci) {
    if (sci == NULL) {
        return;
    }
    codeBlocksScintillaCommand(
        sci, SCI_SETWRAPMODE,
        config.chatLineWrap ? (uptr_t)SC_WRAP_WORD : (uptr_t)SC_WRAP_NONE, 0);
}

static ULONG chatOutputScintillaParseCodeblockIndexAt(const char *atBracket) {
    static const char prefix[] = CHAT_MD_CODEBLOCK_PREFIX;
    const ULONG prefixLen = sizeof(prefix) - 1;
    ULONG idx = 0;
    const char *p;

    if (atBracket == NULL || atBracket[0] != '[' ||
        strncmp(atBracket, prefix, prefixLen) != 0) {
        return 0;
    }
    p = atBracket + prefixLen;
    while (*p == ' ') {
        p++;
    }
    if (*p < '0' || *p > '9') {
        return 0;
    }
    while (*p >= '0' && *p <= '9') {
        idx = idx * 10 + (ULONG)(*p - '0');
        p++;
    }
    if (idx == 0) {
        return 0;
    }
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p != ']') {
        return 0;
    }
    p++;
    if (*p != '\0' && *p != '\n' && *p != '\r') {
        return 0;
    }
    return idx;
}

static ULONG chatOutputScintillaCodeblockIndexFromLine(const char *line) {
    ULONG idx;
    const char *p;

    if (line == NULL) {
        return 0;
    }
    idx = chatOutputScintillaParseCodeblockIndexAt(chatMdSkipLinePrefixZ(line));
    if (idx > 0) {
        return idx;
    }
    for (p = line; *p != '\0'; p++) {
        if (*p == '[') {
            idx = chatOutputScintillaParseCodeblockIndexAt(p);
            if (idx > 0) {
                return idx;
            }
        }
    }
    return 0;
}

static ULONG chatOutputScintillaCodeblockIndexAtPos(Object *sci, Sci_Position pos) {
    Sci_Position line;
    Sci_Position lineStart;
    Sci_Position lineEnd;
    ULONG lineLen;
    char *buf;
    ULONG idx;

    if (sci == NULL || pos < 0) {
        return 0;
    }
    line = codeBlocksScintillaCommand(sci, SCI_LINEFROMPOSITION, pos, 0);
    lineStart = codeBlocksScintillaCommand(sci, SCI_POSITIONFROMLINE, line, 0);
    lineEnd = codeBlocksScintillaCommand(sci, SCI_GETLINEENDPOSITION, line, 0);
    if (lineEnd <= lineStart) {
        return 0;
    }
    lineLen = (ULONG)(lineEnd - lineStart);
    if (lineLen > 255) {
        return 0;
    }
    buf = (char *)AllocVec(lineLen + 2, MEMF_ANY);
    if (buf == NULL) {
        return 0;
    }
    buf[0] = '\0';
    codeBlocksScintillaCommand(sci, SCI_GETLINE, line, (sptr_t)buf);
    idx = chatOutputScintillaCodeblockIndexFromLine(buf);
    FreeVec(buf);
    return idx;
}

static BOOL chatOutputScintillaUrlSpanContaining(const char *line, ULONG lineLen,
                                                 ULONG relClick, ULONG *spanStart,
                                                 ULONG *spanEnd) {
    ULONG pos = 0;

    while (pos < lineLen) {
        ULONG end;
        ULONG schemeLen = chatMdUrlSchemeSpanAt(line, lineLen, pos, &end);

        if (schemeLen > 0) {
            if (relClick >= pos && relClick < end) {
                *spanStart = pos;
                *spanEnd = end;
                return TRUE;
            }
            pos = end;
        } else {
            pos++;
        }
    }
    return FALSE;
}

static BOOL chatOutputScintillaMdLinkUrlAtDocPos(Sci_Position pos, char *urlBuf,
                                                 ULONG urlBufLen) {
    ULONG p = (ULONG)pos;
    ULONG n;
    ULONG len;

    if (urlBuf == NULL || urlBufLen == 0) {
        return FALSE;
    }
    for (n = 0; n < chatOutputMdLinkSpanCount; n++) {
        if (p >= chatOutputMdLinkSpans[n].start && p < chatOutputMdLinkSpans[n].end) {
            len = (ULONG)strlen(chatOutputMdLinkSpans[n].url);
            if (len >= urlBufLen) {
                len = urlBufLen - 1;
            }
            memcpy(urlBuf, chatOutputMdLinkSpans[n].url, len);
            urlBuf[len] = '\0';
            return TRUE;
        }
    }
    return FALSE;
}

static void chatOutputScintillaOpenUrlAtSciPos(Object *sci, Sci_Position pos) {
    Sci_Position line;
    Sci_Position lineStart;
    Sci_Position lineEnd;
    ULONG lineLen;
    ULONG spanStart;
    ULONG spanEnd;
    ULONG urlLen;
    char *buf = NULL;
    char *urlBuf = NULL;
    char mdUrl[CHAT_OUTPUT_URL_MAX];

    if (sci == NULL || pos < 0) {
        return;
    }
    if (chatOutputScintillaMdLinkUrlAtDocPos(pos, mdUrl, (ULONG)sizeof mdUrl)) {
        {
            struct Library *OpenURLBase = OpenLibrary((CONST_STRPTR)OPENURLNAME, 0);

            if (OpenURLBase != NULL) {
                (void)URL_OpenA((STRPTR)mdUrl, NULL);
                CloseLibrary(OpenURLBase);
            }
        }
        chatOutputScintillaReleaseChatMouse(sci);
        return;
    }
    line = codeBlocksScintillaCommand(sci, SCI_LINEFROMPOSITION, pos, 0);
    lineStart = codeBlocksScintillaCommand(sci, SCI_POSITIONFROMLINE, line, 0);
    lineEnd = codeBlocksScintillaCommand(sci, SCI_GETLINEENDPOSITION, line, 0);
    if (lineEnd <= lineStart) {
        return;
    }
    lineLen = (ULONG)(lineEnd - lineStart);
    buf = (char *)AllocVec(lineLen + 2, MEMF_ANY);
    if (buf == NULL) {
        return;
    }
    buf[0] = '\0';
    codeBlocksScintillaCommand(sci, SCI_GETLINE, line, (sptr_t)buf);
    if (!chatOutputScintillaUrlSpanContaining(buf, lineLen, (ULONG)(pos - lineStart),
                                              &spanStart, &spanEnd)) {
        FreeVec(buf);
        return;
    }
    urlLen = spanEnd - spanStart;
    if (urlLen == 0 || urlLen >= CHAT_OUTPUT_URL_MAX) {
        FreeVec(buf);
        return;
    }
    urlBuf = (char *)AllocVec(urlLen + 1, MEMF_ANY);
    if (urlBuf == NULL) {
        FreeVec(buf);
        return;
    }
    memcpy(urlBuf, buf + spanStart, urlLen);
    urlBuf[urlLen] = '\0';
    FreeVec(buf);

    {
        struct Library *OpenURLBase = OpenLibrary((CONST_STRPTR)OPENURLNAME, 0);

        if (OpenURLBase != NULL) {
            (void)URL_OpenA((STRPTR)urlBuf, NULL);
            CloseLibrary(OpenURLBase);
        }
    }
    FreeVec(urlBuf);
    chatOutputScintillaReleaseChatMouse(sci);
}

static void chatOutputScintillaScheduleCodeblockOpen(ULONG blockIndex) {
    if (blockIndex == 0) {
        return;
    }
    chatOutputSciHotspotOpenIndex = blockIndex;
    chatOutputSciHotspotOpenToken = codeBlocksViewerCaptureOpenToken();
    if (app != NULL) {
        DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
                 &ChatOutputSciOpenCodeblockDeferredHook);
    } else {
        ChatOutputSciOpenCodeblockDeferredFunc();
    }
}

static void chatOutputScintillaOnSciNotify(struct SCNotification *scn) {
    ULONG blockIndex;
    Object *sci;
    int styleAt;

    if (scn == NULL || chatOutputSciNotifySource == NULL) {
        return;
    }
    sci = chatOutputSciNotifySource;

    if (scn->nmhdr.code == SCN_HOTSPOTCLICK) {
        styleAt = (int)codeBlocksScintillaCommand(sci, SCI_GETSTYLEAT, scn->position, 0);
        if (styleAt == CHAT_OUTPUT_STYLE_URL_HOTSPOT) {
            chatOutputSciHotspotUrlArmed = TRUE;
            chatOutputSciHotspotPendingIndex = 0;
            codeBlocksScintillaCommand(sci, SCI_SETEMPTYSELECTION, scn->position, 0);
            codeBlocksScintillaCommand(sci, SCI_CANCEL, 0, 0);
            return;
        }
        chatOutputSciHotspotUrlArmed = FALSE;
        blockIndex = chatOutputScintillaCodeblockIndexAtPos(sci, scn->position);
        if (blockIndex > 0) {
            chatOutputSciHotspotPendingIndex = blockIndex;
            codeBlocksScintillaCommand(sci, SCI_SETEMPTYSELECTION, scn->position, 0);
            codeBlocksScintillaCommand(sci, SCI_CANCEL, 0, 0);
        }
        return;
    }

    if (scn->nmhdr.code == SCN_HOTSPOTRELEASECLICK) {
        BOOL openedUrl = FALSE;

        if (chatOutputSciHotspotUrlArmed) {
            chatOutputSciHotspotUrlArmed = FALSE;
            styleAt = (int)codeBlocksScintillaCommand(sci, SCI_GETSTYLEAT, scn->position, 0);
            if (styleAt == CHAT_OUTPUT_STYLE_URL_HOTSPOT) {
                chatOutputScintillaOpenUrlAtSciPos(sci, scn->position);
                chatOutputSciHotspotPendingIndex = 0;
                openedUrl = TRUE;
            }
        }
        if (openedUrl) {
            return;
        }
        blockIndex = chatOutputScintillaCodeblockIndexAtPos(sci, scn->position);
        if (blockIndex == 0) {
            blockIndex = chatOutputSciHotspotPendingIndex;
        }
        chatOutputSciHotspotPendingIndex = 0;
        if (blockIndex > 0) {
            chatOutputScintillaScheduleCodeblockOpen(blockIndex);
        }
    }
}

SAVEDS static ULONG ChatOutputSciNotifySink_MouseUpEvent(struct IClass *cl, Object *obj,
                                                         struct MUIP_HandleEvent *msg) {
    if (msg != NULL && msg->imsg != NULL && msg->imsg->Class == IECLASS_NEWMOUSE &&
        (msg->imsg->Code & IECODE_UP_PREFIX) != 0 && chatOutputSciNotifySource != NULL) {
        chatOutputScintillaReleaseChatMouse(chatOutputSciNotifySource);
        chatOutputSciHotspotPendingIndex = 0;
        chatOutputSciHotspotUrlArmed = FALSE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

SAVEDS static ULONG ChatOutputSciNotifySink_Dispatcher(struct IClass *cl, Object *obj,
                                                       Msg msg) {
    switch (msg->MethodID) {
    case MM_SciHandler:
        chatOutputScintillaOnSciNotify(((struct MUIP_SciHandler *)msg)->scn);
        return 0;
    case MUIM_HandleEvent:
        return ChatOutputSciNotifySink_MouseUpEvent(
            cl, obj, (struct MUIP_HandleEvent *)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

DISPATCHER(ChatOutputSciNotifySink_Dispatcher);

static LONG chatOutputScintillaEnsureNotifySink(void) {
    if (chatOutputSciNotifyClass == NULL) {
        chatOutputSciNotifyClass = MUI_CreateCustomClass(
            NULL, MUIC_Area, NULL, 0, ENTRY(ChatOutputSciNotifySink_Dispatcher));
        if (chatOutputSciNotifyClass == NULL) {
            return RETURN_ERROR;
        }
    }
    if (chatOutputSciNotifySink == NULL) {
        chatOutputSciNotifySink =
            (Object *)NewObject(chatOutputSciNotifyClass->mcc_Class, NULL, TAG_DONE);
        if (chatOutputSciNotifySink == NULL) {
            return RETURN_ERROR;
        }
    }
    return RETURN_OK;
}

void chatOutputScintillaCancelPendingCodeblockOpen(void) {
    chatOutputSciHotspotPendingIndex = 0;
    chatOutputSciHotspotOpenIndex = 0;
    chatOutputSciHotspotOpenToken = 0;
    chatOutputSciHotspotUrlArmed = FALSE;
}

void chatOutputScintillaRemoveMouseUpGuard(void) {
    if (chatOutputSciMouseUpEHInstalled && mainWindowObject != NULL &&
        chatOutputSciMouseUpEH != NULL) {
        DoMethod(mainWindowObject, MUIM_Window_RemEventHandler, chatOutputSciMouseUpEH);
    }
    chatOutputSciMouseUpEHInstalled = FALSE;
    if (chatOutputSciMouseUpEH != NULL) {
        FreeVec(chatOutputSciMouseUpEH);
        chatOutputSciMouseUpEH = NULL;
    }
    chatOutputSciHotspotPendingIndex = 0;
    chatOutputSciHotspotOpenIndex = 0;
    chatOutputSciHotspotOpenToken = 0;
    chatOutputSciHotspotUrlArmed = FALSE;
}

void chatOutputScintillaInstallMouseUpGuard(void) {
    Object *target;

    if (chatOutputSciMouseUpEHInstalled || mainWindowObject == NULL) {
        return;
    }
    if (chatOutputScintillaEnsureNotifySink() != RETURN_OK) {
        return;
    }
    target = chatOutputScroller != NULL ? chatOutputScroller : chatOutputTextEditor;
    if (target == NULL) {
        return;
    }
    if (chatOutputSciMouseUpEH == NULL) {
        chatOutputSciMouseUpEH = (struct MUI_EventHandlerNode *)AllocVec(
            sizeof(struct MUI_EventHandlerNode), MEMF_PUBLIC | MEMF_CLEAR);
        if (chatOutputSciMouseUpEH == NULL) {
            return;
        }
        chatOutputSciMouseUpEH->ehn_Class = chatOutputSciNotifyClass->mcc_Class;
        chatOutputSciMouseUpEH->ehn_Object = target;
        chatOutputSciMouseUpEH->ehn_Events = IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS;
        chatOutputSciMouseUpEH->ehn_Flags = MUI_EHF_GUIMODE;
        chatOutputSciMouseUpEH->ehn_Priority = 200;
    }
    DoMethod(mainWindowObject, MUIM_Window_AddEventHandler, chatOutputSciMouseUpEH);
    chatOutputSciMouseUpEHInstalled = TRUE;
}

void chatOutputScintillaDetachNotify(void) {
    chatOutputScintillaRemoveMouseUpGuard();
    if (chatOutputSciNotifyAttached && chatOutputSciNotifySource != NULL) {
        DoMethod(chatOutputSciNotifySource, MUIM_KillNotify, SCIA_Notify);
    }
    chatOutputSciNotifyAttached = FALSE;
    chatOutputSciNotifySource = NULL;
}

void chatOutputScintillaDisposeNotifyClass(void) {
    chatOutputScintillaDetachNotify();
    if (chatOutputSciNotifySink != NULL) {
        MUI_DisposeObject(chatOutputSciNotifySink);
        chatOutputSciNotifySink = NULL;
    }
    if (chatOutputSciNotifyClass != NULL) {
        MUI_DeleteCustomClass(chatOutputSciNotifyClass);
        chatOutputSciNotifyClass = NULL;
    }
}

void chatOutputScintillaAttachNotify(Object *sci) {
    if (sci == NULL) {
        return;
    }
    if (chatOutputSciNotifyAttached) {
        if (chatOutputSciNotifySource == sci) {
            return;
        }
        chatOutputScintillaDetachNotify();
    }
    if (chatOutputScintillaEnsureNotifySink() != RETURN_OK) {
        return;
    }
    chatOutputSciNotifySource = sci;
    DoMethod(sci, MUIM_Notify, SCIA_Notify, MUIV_EveryTime, chatOutputSciNotifySink, 3,
             MM_SciHandler, MUIV_TriggerValue, FALSE);
    chatOutputSciNotifyAttached = TRUE;
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
    chatOutputScintillaApplyLineWrap(sci);
    /* Read-only chat: no mouse capture → no selection drag / scroll while moving mouse. */
    codeBlocksScintillaCommand(sci, SCI_SETMOUSEDOWNCAPTURES, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETHOTSPOTSINGLELINE, 1, 0);
    codeBlocksScintillaCommand(sci, SCI_SETTEXT, 0, (sptr_t)"");
    chatOutputScintillaAttachNotify(sci);
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
        UBYTE *styleBuf = NULL;

        if (roleStyleLen > textLen) {
            roleStyleLen = textLen;
        }
        styleBuf = (UBYTE *)AllocVec(roleStyleLen, MEMF_ANY);
        if (styleBuf != NULL) {
            memcpy(styleBuf, roleStyles, roleStyleLen);
            chatOutputScintillaApplyCodeblockHotspotStyles(utf8, styleBuf, roleStyleLen);
            chatOutputScintillaApplyUrlHotspotStyles(utf8, styleBuf, roleStyleLen);
            chatOutputScintillaApplyRoleStyleBytes(sci, styleBuf, roleStyleLen);
            FreeVec(styleBuf);
        } else {
            chatOutputScintillaApplyRoleStyleBytes(sci, roleStyles, roleStyleLen);
        }
    } else if (textLen > 0) {
        UBYTE *styleBuf = (UBYTE *)AllocVec(textLen, MEMF_ANY);

        if (styleBuf != NULL) {
            memset(styleBuf, CHAT_OUTPUT_STYLE_ASSISTANT, textLen);
            chatOutputScintillaApplyCodeblockHotspotStyles(utf8, styleBuf, textLen);
            chatOutputScintillaApplyUrlHotspotStyles(utf8, styleBuf, textLen);
            chatOutputScintillaApplyRoleStyleBytes(sci, styleBuf, textLen);
            FreeVec(styleBuf);
        }
    }
    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 1, 0);

    docLen = (ULONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_GOTOPOS, docLen, 0);
    codeBlocksScintillaCommand(sci, SCI_SCROLLCARET, 0, 0);
}

#endif /* __MORPHOS__ */
