/*
 * Host-side unit tests for utf8stream (WSL/Linux: make -f Makefile.utf8stream test)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utf8stream.h"

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

static void test_ascii_single_chunk(void) {
    struct UTF8StreamBuffer *s = utf8stream_create(64);
    UBYTE out[32];
    ULONG n;

    ASSERT(s != NULL, "create");
    ASSERT(utf8stream_append(s, (const UBYTE *)"hello", 5), "append");
    n = utf8stream_take_complete(s, out, sizeof(out));
    ASSERT(n == 5 && memcmp(out, "hello", 5) == 0, "ascii complete");
    ASSERT(s->used == 0, "buffer drained");
    utf8stream_free(s);
}

static void test_utf8_split_two_byte(void) {
    /* ö = C3 B6 */
    struct UTF8StreamBuffer *s = utf8stream_create(64);
    UBYTE out[32];
    ULONG n;

    ASSERT(s != NULL, "create");
    ASSERT(utf8stream_append(s, (const UBYTE *)"\xC3", 1), "append byte 1");
    n = utf8stream_take_complete(s, out, sizeof(out));
    ASSERT(n == 0, "incomplete yields nothing");

    ASSERT(utf8stream_append(s, (const UBYTE *)"\xB6", 1), "append byte 2");
    n = utf8stream_take_complete(s, out, sizeof(out));
    ASSERT(n == 2 && out[0] == 0xC3 && out[1] == 0xB6, "ö complete");
    utf8stream_free(s);
}

static void test_utf8_split_three_byte(void) {
    /* € = E2 82 AC */
    struct UTF8StreamBuffer *s = utf8stream_create(64);
    UBYTE out[32];
    ULONG n;

    ASSERT(s != NULL, "create");
    ASSERT(utf8stream_append(s, (const UBYTE *)"\xE2\x82", 2), "partial");
    ASSERT(utf8stream_take_complete(s, out, sizeof(out)) == 0, "wait");

    ASSERT(utf8stream_append(s, (const UBYTE *)"\xAC", 1), "last byte");
    n = utf8stream_take_complete(s, out, sizeof(out));
    ASSERT(n == 3 && out[0] == 0xE2 && out[1] == 0x82 && out[2] == 0xAC,
           "euro complete");
    utf8stream_free(s);
}

static void test_flush_incomplete(void) {
    struct UTF8StreamBuffer *s = utf8stream_create(64);
    UBYTE out[32];
    ULONG n;

    ASSERT(utf8stream_append(s, (const UBYTE *)"\xC3", 1), "append");
    n = utf8stream_flush(s, out, sizeof(out));
    ASSERT(n == 1 && out[0] == 0xC3, "flush trailing byte");
    ASSERT(s->used == 0, "empty after flush");
    utf8stream_free(s);
}

static void test_multiple_chunks_german(void) {
    struct UTF8StreamBuffer *s = utf8stream_create(64);
    UBYTE out[64];
    char acc[32];
    size_t acc_len = 0;
    const char *pieces[] = {"Sch", "\xC3", "\xB6", "n: ", "\xC3", "\xA4", NULL};
    int i;

    acc[0] = '\0';
    for (i = 0; pieces[i] != NULL; i++) {
        ULONG plen = strlen(pieces[i]);
        ULONG n;
        ASSERT(utf8stream_append(s, (const UBYTE *)pieces[i], plen), "append");
        while ((n = utf8stream_take_complete(s, out, sizeof(out))) > 0) {
            memcpy(acc + acc_len, out, n);
            acc_len += n;
        }
    }
    {
        ULONG n = utf8stream_flush(s, out, sizeof(out));
        if (n > 0) {
            memcpy(acc + acc_len, out, n);
            acc_len += n;
        }
        acc[acc_len] = '\0';
    }
    ASSERT(strcmp(acc, "Schön: ä") == 0, "reassembled german");
    utf8stream_free(s);
}

static void test_utf8_contains_cjk(void) {
    const UBYTE zh[] = {0xE4, 0xB8, 0x96, 0xE7, 0x95, 0x8C, 0}; /* 世界 */
    const UBYTE ext_b[] = {0xF0, 0xA0, 0x80, 0x80, 0};          /* U+20000 */
    const UBYTE latin[] = "Hallo";

    ASSERT(utf8_contains_cjk(zh), "bmp cjk");
    ASSERT(utf8_contains_cjk(ext_b), "cjk extension b (4-byte utf-8)");
    ASSERT(!utf8_contains_cjk(latin), "latin not cjk");
    ASSERT(!utf8_contains_cjk((const UBYTE *)"hello"), "ascii not cjk");
}

int main(void) {
    test_ascii_single_chunk();
    test_utf8_split_two_byte();
    test_utf8_split_three_byte();
    test_flush_incomplete();
    test_multiple_chunks_german();
    test_utf8_contains_cjk();

    printf("utf8stream host tests: %d run, %d failed\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
