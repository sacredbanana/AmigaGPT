/*
 * Host-side tests for conversationNodeParseCodeFences (WSL: tools/test-codefence.sh)
 */
#include <stdio.h>
#include <string.h>

#include "codefence_host_stubs.h"

#define CODEFENCE_HOST 1
#include "../codefence.c"

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

static struct MinList codeblocks;
static struct Conversation conv;
static struct ConversationNode node;

static void reset_node(const char *raw) {
    struct AICodeBlock *block;

    if (node.display_text != NULL) {
        FreeVec(node.display_text);
        node.display_text = NULL;
    }
    while ((block = (struct AICodeBlock *)RemHead((struct List *)&codeblocks)) !=
           NULL) {
        if (block->language != NULL) {
            FreeVec(block->language);
        }
        if (block->raw_code != NULL) {
            FreeVec(block->raw_code);
        }
        FreeVec(block);
    }
    init_minlist(&codeblocks);
    node.raw_utf8 = (UTF8 *)raw;
    node.raw_length = (ULONG)strlen(raw);
    node.codeblocks = &codeblocks;
}

static ULONG block_count(void) {
    struct MinNode *mn;
    ULONG n = 0;

    for (mn = codeblocks.mlh_Head; mn->mln_Succ != NULL; mn = mn->mln_Succ) {
        n++;
    }
    return n;
}

static struct AICodeBlock *first_block(void) {
    if (block_count() == 0) {
        return NULL;
    }
    return (struct AICodeBlock *)codeblocks.mlh_Head;
}

static void test_triple_fence(void) {
    const char *raw = "```python\nprint('hi')\n```";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 1, "triple fence: one block");
    if (first_block() != NULL) {
        ASSERT(strcmp(first_block()->language, "python") == 0,
               "triple fence: language");
        ASSERT(strcmp((char *)first_block()->raw_code, "print('hi')") == 0,
               "triple fence: code body");
        ASSERT(first_block()->index == 1, "triple fence: index");
    }
    ASSERT(node.display_text != NULL, "triple fence: display_text set");
    ASSERT(strstr((char *)node.display_text, "[Codeblock 1]") != NULL,
           "triple fence: placeholder");
}

static void test_quad_fence(void) {
    const char *raw = "````c\nint x;\n````";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 1, "quad fence: one block");
    if (first_block() != NULL) {
        ASSERT(strcmp(first_block()->language, "c") == 0, "quad fence: language");
        ASSERT(strcmp((char *)first_block()->raw_code, "int x;") == 0,
               "quad fence: code body");
    }
}

static void test_close_longer_than_open(void) {
    const char *raw = "````\nbody\n`````";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 1, "longer close: one block");
    if (first_block() != NULL) {
        ASSERT(strcmp((char *)first_block()->raw_code, "body") == 0,
               "longer close: code body");
    }
}

static void test_close_shorter_than_open(void) {
    const char *raw = "````\nbody\n```";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 0, "short close: no block");
    ASSERT(node.display_text == NULL, "short close: no display_text");
}

static void test_incomplete_trailing_fence(void) {
    const char *raw = "```python\nstill open";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 0, "incomplete: no block");
}

static void test_two_blocks(void) {
    const char *raw = "```a\n1\n```\n\n````b\n2\n````";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 2, "two blocks: count");
}

static void test_inline_ticks_not_fence(void) {
    const char *raw = "use ``` inline not a fence line";

    reset_node(raw);
    conversationNodeParseCodeFences(&conv, &node);
    ASSERT(block_count() == 0, "inline ticks: no block");
}

int main(void) {
    init_minlist(&codeblocks);
    conv.messages = NULL;

    test_triple_fence();
    test_quad_fence();
    test_close_longer_than_open();
    test_close_shorter_than_open();
    test_incomplete_trailing_fence();
    test_two_blocks();
    test_inline_ticks_not_fence();

    printf("codefence_host_test: %d run, %d failed\n", tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
