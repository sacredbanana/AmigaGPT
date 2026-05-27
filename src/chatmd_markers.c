#include "chatmd_markers.h"

static BOOL chatMdIsAsciiAlnum(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9');
}

static BOOL chatMdIsAsciiDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

static BOOL chatMdIsUnicodeWhitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' ||
           ch == '\v';
}

/* CommonMark 0.30 Unicode punctuation (ASCII subset). */
static BOOL chatMdIsUnicodePunctuation(char ch) {
    switch (ch) {
    case '!':
    case '"':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '(':
    case ')':
    case '*':
    case '+':
    case ',':
    case '-':
    case '.':
    case '/':
    case ':':
    case ';':
    case '<':
    case '=':
    case '>':
    case '?':
    case '@':
    case '[':
    case '\\':
    case ']':
    case '^':
    case '_':
    case '`':
    case '{':
    case '|':
    case '}':
    case '~':
        return TRUE;
    default:
        return FALSE;
    }
}

/** CommonMark: left-flanking delimiter run (for `*` at pos). */
static BOOL chatMdLeftFlanking(const char *input, ULONG pos, ULONG len) {
    if (pos >= len || input[pos] != '*') {
        return FALSE;
    }
    if (pos + 1 < len && chatMdIsUnicodeWhitespace(input[pos + 1])) {
        return FALSE;
    }
    if (pos + 1 < len && chatMdIsUnicodePunctuation(input[pos + 1])) {
        if (pos == 0) {
            return FALSE;
        }
        if (!chatMdIsUnicodeWhitespace(input[pos - 1]) &&
            !chatMdIsUnicodePunctuation(input[pos - 1])) {
            return FALSE;
        }
    }
    return TRUE;
}

/** CommonMark: right-flanking delimiter run (for `*` at pos). */
static BOOL chatMdRightFlanking(const char *input, ULONG pos, ULONG len) {
    if (pos >= len || input[pos] != '*') {
        return FALSE;
    }
    if (pos > 0 && chatMdIsUnicodeWhitespace(input[pos - 1])) {
        return FALSE;
    }
    if (pos > 0 && chatMdIsUnicodePunctuation(input[pos - 1])) {
        if (pos + 1 >= len) {
            return FALSE;
        }
        if (!chatMdIsUnicodeWhitespace(input[pos + 1]) &&
            !chatMdIsUnicodePunctuation(input[pos + 1])) {
            return FALSE;
        }
    }
    return TRUE;
}

/* Inside words (Spieler*innen): never a delimiter. */
static BOOL chatMdItalicStarInsideWord(const char *input, ULONG pos, ULONG len) {
    return pos > 0 && pos + 1 < len && chatMdIsAsciiAlnum(input[pos - 1]) &&
           chatMdIsAsciiAlnum(input[pos + 1]);
}

/*
 * Toggle-parser extra: lone `*` after space before a digit is usually multiply
 * (8 *9=444), not emphasis. CommonMark pairing would differ; we stay pragmatic.
 */
static BOOL chatMdItalicStarMultiplyHeuristic(const char *input, ULONG pos, ULONG len) {
    if (pos > 0 && chatMdIsUnicodeWhitespace(input[pos - 1]) && pos + 1 < len &&
        chatMdIsAsciiDigit(input[pos + 1])) {
        return TRUE;
    }
    return FALSE;
}

BOOL chatMdItalicStarCanOpen(const char *input, ULONG pos, ULONG len) {
    if (input == NULL || pos >= len || input[pos] != '*') {
        return FALSE;
    }
    if (pos + 1 < len && input[pos + 1] == '*') {
        return FALSE;
    }
    if (chatMdItalicStarInsideWord(input, pos, len)) {
        return FALSE;
    }
    if (!chatMdLeftFlanking(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarMultiplyHeuristic(input, pos, len)) {
        return FALSE;
    }
    return TRUE;
}

BOOL chatMdItalicStarCanClose(const char *input, ULONG pos, ULONG len) {
    if (input == NULL || pos >= len || input[pos] != '*') {
        return FALSE;
    }
    if (pos + 1 < len && input[pos + 1] == '*') {
        return FALSE;
    }
    if (chatMdItalicStarInsideWord(input, pos, len)) {
        return FALSE;
    }
    if (!chatMdRightFlanking(input, pos, len)) {
        return FALSE;
    }
    return TRUE;
}
