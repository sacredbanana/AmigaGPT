/*
 * Host test: documents ownership rules for code-block UI memory (no MUI).
 * Run: make -f Makefile.codeblocks_memory && ./codeblocks_memory_host_test
 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AICodeBlock {
    char *language;
    char *raw_code;
    unsigned long code_length;
} AICodeBlock;

typedef struct CodeBlockListItem {
    AICodeBlock *block;
    char label[64];
} CodeBlockListItem;

static AICodeBlock *make_block(const char *lang, const char *code) {
    AICodeBlock *b = calloc(1, sizeof(*b));
    b->language = strdup(lang);
    b->raw_code = strdup(code);
    b->code_length = strlen(code);
    return b;
}

static void free_block(AICodeBlock *b) {
    if (b == NULL) {
        return;
    }
    free(b->language);
    free(b->raw_code);
    free(b);
}

static void free_list_item(CodeBlockListItem *item) {
    free(item);
}

/* Simulates NList Destruct: only the list row wrapper is freed. */
static void clear_list_rows(CodeBlockListItem **rows, size_t *n) {
    size_t i;

    for (i = 0; i < *n; i++) {
        free_list_item(rows[i]);
    }
    *n = 0;
}

int main(void) {
    AICodeBlock *owned_blocks[2];
    CodeBlockListItem *rows[4];
    size_t row_count = 0;
    CodeBlockListItem *item;

    owned_blocks[0] = make_block("python", "print('a')\n");
    owned_blocks[1] = make_block("c", "int main(){}\n");

    item = calloc(1, sizeof(*item));
    item->block = owned_blocks[0];
    snprintf(item->label, sizeof(item->label), "1:python");
    rows[row_count++] = item;

    item = calloc(1, sizeof(*item));
    item->block = owned_blocks[1];
    snprintf(item->label, sizeof(item->label), "2:c");
    rows[row_count++] = item;

    assert(row_count == 2);

    /* Viewer clear: rows freed, blocks remain (conversation still owns them). */
    clear_list_rows(rows, &row_count);
    assert(owned_blocks[0]->raw_code[0] == 'p');

    /* Re-populate with same block pointers (no duplicate free of AICodeBlock). */
    item = calloc(1, sizeof(*item));
    item->block = owned_blocks[0];
    rows[row_count++] = item;

    clear_list_rows(rows, &row_count);

    /* Conversation teardown frees blocks once. */
    free_block(owned_blocks[0]);
    free_block(owned_blocks[1]);

    printf("codeblocks_memory_host_test: OK\n");
    return 0;
}
