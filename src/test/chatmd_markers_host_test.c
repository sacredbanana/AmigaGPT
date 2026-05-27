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

int main(void) {
    test_not_open();
    test_open_and_close();
    test_mid_sentence_italic();
    test_bold_not_single_italic();

    printf("chatmd_markers_host_test: %d run, %d failed\n", tests_run, tests_failed);
    return tests_failed != 0 ? 1 : 0;
}
