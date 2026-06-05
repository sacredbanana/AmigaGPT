/*
 * Host-side tests for chatMdItalicStarCanOpen/Close (WSL: make -f Makefile.chatmd_markers test)
 */
#include <stdio.h>
#include <string.h>

#include "../chatmd_markers.h"

static int tests_run;
static int tests_failed;

#define ASSERT(cond, msg)                                                      \
    do {                                                                       \
        tests_run++;                                                           \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);    \
            tests_failed++;                                                    \
        }                                                                      \
    } while (0)

static ULONG star_at(const char *s) {
    const char *p = strchr(s, '*');

    return p ? (ULONG)(p - s) : (ULONG)-1;
}

static ULONG last_star_at(const char *s) {
    const char *p = strrchr(s, '*');

    return p ? (ULONG)(p - s) : (ULONG)-1;
}

static void test_not_open(void) {
    const char *s;
    ULONG p;

    s = "Spieler*innen";
    p = star_at(s);
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "Spieler*innen");

    s = "8 *9=444";
    p = star_at(s);
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "8 *9=444");

    s = "Preis * Menge";
    p = star_at(s);
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "Preis * Menge");

    s = "5*6";
    p = star_at(s);
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "5*6");
}

static void test_open_and_close(void) {
    const char *s = "*kursiv*";

    ASSERT(chatMdItalicStarCanOpen(s, 0, (ULONG)strlen(s)), "*kursiv open");
    ASSERT(chatMdItalicStarCanClose(s, 7, (ULONG)strlen(s)), "*kursiv close");
}

static void test_mid_sentence_italic(void) {
    const char *s = "ein *kursiver Abschnitt*";
    ULONG openAt = star_at(s);
    ULONG closeAt = last_star_at(s);

    ASSERT(openAt == 4, "open star index");
    ASSERT(closeAt == 23, "close star index");
    ASSERT(chatMdItalicStarCanOpen(s, openAt, (ULONG)strlen(s)),
           "space before kursiver still opens");
    ASSERT(chatMdItalicStarCanClose(s, closeAt, (ULONG)strlen(s)), "close");
}

static void test_bold_not_single_italic(void) {
    const char *s = "**bold**";

    ASSERT(!chatMdItalicStarCanOpen(s, 0, (ULONG)strlen(s)), "** at 0");
}

static ULONG double_star_at(const char *s) {
    const char *p = strstr(s, "**");

    return p ? (ULONG)(p - s) : (ULONG)-1;
}

static void test_bold_open_close(void) {
    const char *s = "**fett**";
    ULONG openAt = 0;
    ULONG closeAt = 6;

    ASSERT(chatMdBoldDoubleStarCanOpen(s, openAt, (ULONG)strlen(s)), "open");
    ASSERT(!chatMdBoldDoubleStarCanOpen(s, closeAt, (ULONG)strlen(s)),
           "no open on close");
    ASSERT(chatMdBoldDoubleStarCanClose(s, closeAt, (ULONG)strlen(s)), "close");
}

static void test_bold_close_after_period(void) {
    const char *s =
        "Normal formatierter anfang mit **fettem text am satzende.** hier";
    ULONG closeAt = strstr(s, "satzende.") - s + 9;

    ASSERT(closeAt < strlen(s), "close index");
    ASSERT(chatMdBoldDoubleStarCanClose(s, closeAt, (ULONG)strlen(s)),
           "satzende.** closes");
    ASSERT(chatMdBoldDoubleStarCanOpen(s, double_star_at(s), (ULONG)strlen(s)),
           "opening ** after mit");
}

static void test_bold_no_open_on_star_dot(void) {
    const char *s = "foo **. bar";
    ULONG p = strstr(s, "**") - s;

    ASSERT(!chatMdBoldDoubleStarCanOpen(s, p, (ULONG)strlen(s)), "**. not open");
}

static void test_bold_no_open_exponent(void) {
    const char *s;
    ULONG p;

    s = "x**2";
    p = strstr(s, "**") - s;
    ASSERT(!chatMdBoldDoubleStarCanOpen(s, p, (ULONG)strlen(s)), "x**2 not open");

    s = "a**10 and **bold**";
    p = strstr(s, "**") - s;
    ASSERT(!chatMdBoldDoubleStarCanOpen(s, p, (ULONG)strlen(s)), "a**10 not open");
    p = (ULONG)(strstr(s, "and ") - s + 4);
    ASSERT(chatMdBoldDoubleStarCanOpen(s, p, (ULONG)strlen(s)), "**bold** opens");
}

static void test_escaped_and_quoted_star(void) {
    const char *s;
    ULONG p;

    s = "\\*";
    p = 1;
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "\\* not italic open");
    ASSERT(!chatMdItalicStarCanClose(s, p, (ULONG)strlen(s)), "\\* not italic close");

    s = "'*'";
    p = 1;
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "'*' not open");
    ASSERT(!chatMdItalicStarCanClose(s, p, (ULONG)strlen(s)), "'*' not close");

    s = "Use '\\*' for '*', then normal.";
    p = strchr(s, '*') - s;
    while (p < strlen(s)) {
        ASSERT(!chatMdItalicStarCanOpen(s, (ULONG)p, (ULONG)strlen(s)),
               "no spurious italic in doc line");
        p = strchr(s + p + 1, '*') - s;
        if (p == (ULONG)-1) {
            break;
        }
    }
}

static void test_footnote_star(void) {
    const char *s = "2-1 (*)";
    ULONG p = star_at(s);

    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "(* not open");
    ASSERT(!chatMdItalicStarCanClose(s, p, (ULONG)strlen(s)), "(* not close");

    s = "(*) Hinweis";
    p = star_at(s);
    ASSERT(!chatMdItalicStarCanOpen(s, p, (ULONG)strlen(s)), "(*) line not open");
}

static void test_perlio_encoding_paren(void) {
    const char *s = "*PerlIO::encoding*)";
    ULONG openAt = 0;
    ULONG closeAt = last_star_at(s);

    ASSERT(chatMdItalicStarCanOpen(s, openAt, (ULONG)strlen(s)),
           "*PerlIO::encoding*) open");
    ASSERT(chatMdItalicStarCanClose(s, closeAt, (ULONG)strlen(s)),
           "*PerlIO::encoding*) close");

    s = "(*PerlIO::encoding*)";
    openAt = star_at(s);
    closeAt = last_star_at(s);
    ASSERT(chatMdItalicStarCanOpen(s, openAt, (ULONG)strlen(s)),
           "(*PerlIO::encoding*) open");
    ASSERT(chatMdItalicStarCanClose(s, closeAt, (ULONG)strlen(s)),
           "(*PerlIO::encoding*) close");
}

static void test_inline_backtick_open(void) {
    const char *s;
    ULONG p;

    s = "Wenn`s an deutschen Monatsnamen liegt";
    p = strchr(s, '`') - s;
    ASSERT(!chatMdInlineBacktickCanOpen(s, p, (ULONG)strlen(s)),
           "Wenn`s apostrophe not code");

    s = "im `Januar` und `Februar`";
    p = strchr(s, '`') - s;
    ASSERT(chatMdInlineBacktickCanOpen(s, p, (ULONG)strlen(s)), "Januar open");
    p = (ULONG)(strstr(s, "Februar") - s - 1);
    ASSERT(chatMdInlineBacktickCanOpen(s, p, (ULONG)strlen(s)), "Februar open");

    s = "use `foo()` here";
    p = strchr(s, '`') - s;
    ASSERT(chatMdInlineBacktickCanOpen(s, p, (ULONG)strlen(s)), "foo() open");

    s = "orphan ` at end";
    p = strchr(s, '`') - s;
    ASSERT(!chatMdInlineBacktickCanOpen(s, p, (ULONG)strlen(s)), "orphan not open");
}

static void test_italic_still_works(void) {
    const char *s = "see *emphasis* here";
    ULONG openAt = strchr(s, '*') - s;
    ULONG closeAt = strrchr(s, '*') - s;

    ASSERT(chatMdItalicStarCanOpen(s, openAt, (ULONG)strlen(s)), "emphasis open");
    ASSERT(chatMdItalicStarCanClose(s, closeAt, (ULONG)strlen(s)), "emphasis close");
}

int main(void) {
    test_not_open();
    test_open_and_close();
    test_mid_sentence_italic();
    test_bold_not_single_italic();
    test_bold_open_close();
    test_bold_close_after_period();
    test_bold_no_open_on_star_dot();
    test_bold_no_open_exponent();
    test_inline_backtick_open();
    test_escaped_and_quoted_star();
    test_footnote_star();
    test_perlio_encoding_paren();
    test_italic_still_works();

    printf("chatmd_markers_host_test: %d run, %d failed\n", tests_run, tests_failed);
    return tests_failed != 0 ? 1 : 0;
}
