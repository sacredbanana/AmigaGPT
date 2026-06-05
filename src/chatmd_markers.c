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

/* Escaped or quoted star (e.g. \*, '*') — not emphasis. */
static BOOL chatMdItalicStarIsEscaped(const char *input, ULONG pos, ULONG len) {
    (void)len;
    return pos > 0 && input[pos - 1] == '\\';
}

static BOOL chatMdItalicStarAdjacentSingleQuote(const char *input, ULONG pos, ULONG len) {
    if (pos > 0 && input[pos - 1] == '\'') {
        return TRUE;
    }
    if (pos + 1 < len && input[pos + 1] == '\'') {
        return TRUE;
    }
    return FALSE;
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

/* Footnote marker "(*)" only — not emphasis; do not block `*word*)` or `(*word*`. */
static BOOL chatMdItalicStarFootnoteParen(const char *input, ULONG pos, ULONG len) {
    return pos > 0 && pos + 1 < len && input[pos - 1] == '(' && input[pos + 1] == ')';
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
    if (chatMdItalicStarIsEscaped(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarAdjacentSingleQuote(input, pos, len)) {
        return FALSE;
    }
    if (!chatMdLeftFlanking(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarMultiplyHeuristic(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarFootnoteParen(input, pos, len)) {
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
    if (chatMdItalicStarIsEscaped(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarAdjacentSingleQuote(input, pos, len)) {
        return FALSE;
    }
    if (!chatMdRightFlanking(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarFootnoteParen(input, pos, len)) {
        return FALSE;
    }
    return TRUE;
}

static BOOL chatMdBoldDoubleStarAt(const char *input, ULONG pos, ULONG len) {
    return pos + 1 < len && input[pos] == '*' && input[pos + 1] == '*';
}

/* Left-flanking for the `**` run (content after both asterisks, not the second `*`). */
static BOOL chatMdBoldDoubleStarLeftFlanking(const char *input, ULONG pos, ULONG len) {
    ULONG after = pos + 2;

    if (after >= len) {
        return FALSE;
    }
    if (chatMdIsUnicodeWhitespace(input[after])) {
        return FALSE;
    }
    if (chatMdIsUnicodePunctuation(input[after])) {
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

/*
 * Exponent / math: `x**2`, `a**10` — not emphasis (GPT often uses ** for powers).
 */
static BOOL chatMdBoldExponentHeuristic(const char *input, ULONG pos, ULONG len) {
    if (pos == 0 || pos + 2 >= len) {
        return FALSE;
    }
    if (!chatMdIsAsciiAlnum(input[pos - 1])) {
        return FALSE;
    }
    return chatMdIsAsciiDigit(input[pos + 2]);
}

/*
 * Bold `**`: open with left-flanking on content after the pair; close always allowed
 * (e.g. satzende.** hier). Reject open for `**.` and `x**2`.
 */
BOOL chatMdBoldDoubleStarCanOpen(const char *input, ULONG pos, ULONG len) {
    if (input == NULL || !chatMdBoldDoubleStarAt(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarIsEscaped(input, pos, len)) {
        return FALSE;
    }
    if (chatMdBoldExponentHeuristic(input, pos, len)) {
        return FALSE;
    }
    if (pos + 2 < len && input[pos + 2] == '.') {
        return FALSE;
    }
    return chatMdBoldDoubleStarLeftFlanking(input, pos, len);
}

BOOL chatMdBoldDoubleStarCanClose(const char *input, ULONG pos, ULONG len) {
    if (input == NULL || !chatMdBoldDoubleStarAt(input, pos, len)) {
        return FALSE;
    }
    if (chatMdItalicStarIsEscaped(input, pos, len)) {
        return FALSE;
    }
    return TRUE;
}

static BOOL chatMdInlineBacktickHasCloseOnLine(const char *input, ULONG pos, ULONG len) {
    ULONG i;

    for (i = pos + 1; i < len && input[i] != '\n'; i++) {
        if (input[i] == '`' && input[i - 1] != '\\') {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * GPT/German text often uses ` instead of apostrophe (Wenn`s). Treat as literal
 * when sandwiched between word chars with no closing ` before whitespace.
 */
static BOOL chatMdInlineBacktickIsApostrophe(const char *input, ULONG pos, ULONG len) {
    ULONG i;

    if (pos == 0 || pos + 1 >= len || input[pos] != '`') {
        return FALSE;
    }
    if (!chatMdIsAsciiAlnum(input[pos - 1]) || !chatMdIsAsciiAlnum(input[pos + 1])) {
        return FALSE;
    }
    for (i = pos + 1; i < len && input[i] != '\n'; i++) {
        if (input[i] == '`') {
            return FALSE;
        }
        if (input[i] == ' ' || input[i] == '\t') {
            break;
        }
    }
    return TRUE;
}

BOOL chatMdInlineBacktickCanOpen(const char *input, ULONG pos, ULONG len) {
    if (input == NULL || pos >= len || input[pos] != '`') {
        return FALSE;
    }
    if (pos > 0 && input[pos - 1] == '\\') {
        return FALSE;
    }
    if (pos + 1 < len && input[pos + 1] == '`') {
        return FALSE;
    }
    if (chatMdInlineBacktickIsApostrophe(input, pos, len)) {
        return FALSE;
    }
    return chatMdInlineBacktickHasCloseOnLine(input, pos, len);
}
