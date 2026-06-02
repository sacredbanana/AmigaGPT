#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#include "codefence.h"
#include "AmigaGPT_cat.h"
#include "openai.h"

/** CommonMark: fenced code may be indented by up to 3 columns on the line. */
#define FENCE_MAX_INDENT_COLS 3

static ULONG fence_line_indent_cols(const UBYTE *line_start,
                                    const UBYTE *fence) {
    ULONG cols = 0;
    const UBYTE *q;

    for (q = line_start; q < fence; q++) {
        if (*q == ' ') {
            cols++;
        } else if (*q == '\t') {
            cols += 4 - (cols % 4);
        } else {
            return (ULONG)-1;
        }
        if (cols > FENCE_MAX_INDENT_COLS) {
            return (ULONG)-1;
        }
    }
    return cols;
}

static BOOL is_fence_line(const UBYTE *raw_base, const UBYTE *fence) {
    const UBYTE *line_start;

    if (fence < raw_base) {
        return FALSE;
    }
    line_start = fence;
    while (line_start > raw_base && line_start[-1] != '\n') {
        line_start--;
    }
    return fence_line_indent_cols(line_start, fence) != (ULONG)-1;
}

static ULONG fence_tick_run_len(const UBYTE *p, const UBYTE *end) {
    ULONG n = 0;

    while (p < end && *p == '`') {
        n++;
        p++;
    }
    return n;
}

static void clear_codeblocks(struct ConversationNode *node) {
    struct AICodeBlock *block;

    if (node == NULL || node->codeblocks == NULL) {
        return;
    }
    while ((block = (struct AICodeBlock *)RemHead(
                (struct List *)node->codeblocks)) != NULL) {
        if (block->language != NULL) {
            FreeVec(block->language);
        }
        if (block->raw_code != NULL) {
            FreeVec(block->raw_code);
        }
        FreeVec(block);
    }
}

static STRPTR dup_range(const UBYTE *start, ULONG len) {
    STRPTR s;

    s = AllocVec(len + 1, MEMF_CLEAR);
    if (s == NULL) {
        return NULL;
    }
    if (len > 0) {
        CopyMem((APTR)start, s, len);
    }
    s[len] = '\0';
    return s;
}

static ULONG count_codeblocks(const struct ConversationNode *node) {
    struct MinNode *mn;
    ULONG n = 0;

    if (node == NULL || node->codeblocks == NULL) {
        return 0;
    }
    for (mn = node->codeblocks->mlh_Head; mn->mln_Succ != NULL;
         mn = mn->mln_Succ) {
        n++;
    }
    return n;
}

static ULONG conversation_codeblock_offset(struct Conversation *conv,
                                         struct ConversationNode *node) {
    struct MinNode *mn;
    ULONG offset = 0;

    if (conv == NULL || conv->messages == NULL || node == NULL) {
        return 0;
    }
    for (mn = conv->messages->mlh_Head; mn->mln_Succ != NULL;
         mn = mn->mln_Succ) {
        struct ConversationNode *m = (struct ConversationNode *)mn;
        if (m == node) {
            break;
        }
        offset += count_codeblocks(m);
    }
    return offset;
}

static BOOL minlist_is_empty(const struct MinList *list) {
    if (list == NULL) {
        return TRUE;
    }
    /* Same as gui.c codeblocks_empty / Exec NEWLIST empty test */
    return (APTR)list->mlh_TailPred == (APTR)list;
}

#define CODEBLOCK_PLACEHOLDER_MAX 48

static ULONG format_codeblock_placeholder(UBYTE *buf, ULONG buf_len,
                                          ULONG index) {
    int n;

    n = snprintf((char *)buf, (size_t)buf_len,
                 (const char *)STRING_CHAT_CODEBLOCK_PLACEHOLDER,
                 (long)index);
    if (n < 0) {
        return 0;
    }
    if ((ULONG)n >= buf_len) {
        return buf_len - 1;
    }
    return (ULONG)n;
}

/**
 * If codeblocks is non-empty, set display_text to raw_utf8 with each fenced
 * region replaced by a short UTF-8 placeholder (Phase 5.1 chat vs. code).
 * Otherwise display_text stays NULL (use raw in UI).
 */
static void conversationNodeBuildDisplayOmittingCode(
    struct ConversationNode *node, ULONG index_offset) {
    ULONG placeholder_len;
    const UBYTE *raw;
    const UBYTE *end;
    const UBYTE *p;
    const UBYTE *segment_start;
    ULONG out_cap;
    STRPTR out;
    ULONG out_pos;
    ULONG tail_len;
    ULONG block_index;

    if (node == NULL || node->raw_utf8 == NULL || node->codeblocks == NULL) {
        return;
    }

    if (minlist_is_empty(node->codeblocks)) {
        return;
    }

    raw = (const UBYTE *)node->raw_utf8;
    if (node->raw_length == 0) {
        return;
    }
    end = raw + node->raw_length;
    out_cap = node->raw_length + 256;
    out = AllocVec(out_cap, MEMF_CLEAR);
    if (out == NULL) {
        return;
    }
    out_pos = 0;
    segment_start = raw;
    p = raw;
    block_index = index_offset;

    while (p < end) {
        const UBYTE *fence;
        ULONG opening_ticks;
        const UBYTE *closing;
        const UBYTE *q;
        ULONG seg_len;
        UBYTE ph_buf[CODEBLOCK_PLACEHOLDER_MAX];

        fence = p;
        opening_ticks = 0;
        for (; fence < end; fence++) {
            opening_ticks = fence_tick_run_len(fence, end);
            if (opening_ticks >= 3) {
                break;
            }
        }
        if (fence >= end) {
            break;
        }

        if (!is_fence_line(raw, fence)) {
            p = fence + opening_ticks;
            continue;
        }

        p = fence + opening_ticks;
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        while (p < end && *p != '\n' && *p != '\r') {
            p++;
        }
        if (p >= end) {
            break;
        }
        if (p < end && *p == '\r') {
            p++;
        }
        if (p < end && *p == '\n') {
            p++;
        }

        closing = NULL;
        for (q = p; q < end; q++) {
            ULONG closing_ticks = fence_tick_run_len(q, end);

            if (closing_ticks >= opening_ticks && is_fence_line(raw, q)) {
                const UBYTE *r = q + closing_ticks;

                while (r < end && (*r == ' ' || *r == '\t')) {
                    r++;
                }
                if (r >= end || *r == '\n' || *r == '\r') {
                    closing = q;
                    break;
                }
            }
        }
        if (closing == NULL) {
            break;
        }

        block_index++;
        placeholder_len =
            format_codeblock_placeholder(ph_buf, CODEBLOCK_PLACEHOLDER_MAX,
                                         block_index);
        if (placeholder_len == 0) {
            goto fail;
        }

        seg_len = (ULONG)(fence - segment_start);
        if (out_pos + seg_len + placeholder_len + 1 >= out_cap) {
            goto fail;
        }
        if (seg_len > 0) {
            CopyMem((APTR)segment_start, out + out_pos, seg_len);
            out_pos += seg_len;
        }
        CopyMem(ph_buf, out + out_pos, placeholder_len);
        out_pos += placeholder_len;

        p = closing + opening_ticks;
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        if (p < end && *p == '\r') {
            p++;
        }
        if (p < end && *p == '\n') {
            p++;
        }
        segment_start = p;
    }

    tail_len = (ULONG)(end - segment_start);
    if (out_pos + tail_len + 1 >= out_cap) {
        goto fail;
    }
    if (tail_len > 0) {
        CopyMem((APTR)segment_start, out + out_pos, tail_len);
        out_pos += tail_len;
    }
    out[out_pos] = '\0';

    if (node->display_text != NULL) {
        FreeVec(node->display_text);
    }
    node->display_text = (UTF8 *)out;
    return;

fail:
    if (out != NULL) {
        FreeVec(out);
    }
}

void conversationNodeParseCodeFences(struct Conversation *conv,
                                     struct ConversationNode *node) {
    const UBYTE *raw;
    const UBYTE *end;
    const UBYTE *p;
    ULONG index_offset;
    ULONG block_index;

    if (node == NULL || node->raw_utf8 == NULL || node->codeblocks == NULL) {
        return;
    }

    if (node->display_text != NULL) {
        FreeVec(node->display_text);
        node->display_text = NULL;
    }

    clear_codeblocks(node);

    index_offset = conversation_codeblock_offset(conv, node);
    block_index = index_offset;

    raw = (const UBYTE *)node->raw_utf8;
    if (node->raw_length == 0) {
        return;
    }
    end = raw + node->raw_length;
    p = raw;

    while (p < end) {
        const UBYTE *fence;
        ULONG opening_ticks;
        const UBYTE *lang_start;
        const UBYTE *lang_end;
        ULONG lang_len;
        const UBYTE *code_start;
        const UBYTE *closing;
        const UBYTE *q;
        ULONG code_len;
        struct AICodeBlock *block;

        fence = p;
        opening_ticks = 0;
        for (; fence < end; fence++) {
            opening_ticks = fence_tick_run_len(fence, end);
            if (opening_ticks >= 3) {
                break;
            }
        }
        if (fence >= end) {
            break;
        }

        if (!is_fence_line(raw, fence)) {
            p = fence + opening_ticks;
            continue;
        }

        p = fence + opening_ticks;
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }

        lang_start = p;
        while (p < end && *p != '\n' && *p != '\r') {
            p++;
        }
        if (p >= end) {
            break;
        }

        lang_end = p;
        while (lang_end > lang_start &&
               (lang_end[-1] == ' ' || lang_end[-1] == '\t')) {
            lang_end--;
        }
        while (lang_start < lang_end &&
               (*lang_start == ' ' || *lang_start == '\t')) {
            lang_start++;
        }
        lang_len = (ULONG)(lang_end - lang_start);

        if (p < end && *p == '\r') {
            p++;
        }
        if (p < end && *p == '\n') {
            p++;
        }

        code_start = p;
        closing = NULL;
        for (q = code_start; q < end; q++) {
            ULONG closing_ticks = fence_tick_run_len(q, end);

            if (closing_ticks >= opening_ticks && is_fence_line(raw, q)) {
                const UBYTE *r = q + closing_ticks;

                while (r < end && (*r == ' ' || *r == '\t')) {
                    r++;
                }
                if (r >= end || *r == '\n' || *r == '\r') {
                    closing = q;
                    break;
                }
            }
        }
        if (closing == NULL) {
            break;
        }

        code_len = (ULONG)(closing - code_start);
        while (code_len > 0 &&
               (code_start[code_len - 1] == '\n' ||
                code_start[code_len - 1] == '\r')) {
            code_len--;
        }

        block = AllocVec(sizeof(struct AICodeBlock), MEMF_CLEAR);
        if (block == NULL) {
            return;
        }

        if (lang_len > 0) {
            block->language = dup_range(lang_start, lang_len);
        } else {
            block->language = AllocVec(1, MEMF_CLEAR);
            if (block->language != NULL) {
                block->language[0] = '\0';
            }
        }

        if (block->language == NULL) {
            FreeVec(block);
            return;
        }

        block->raw_code = dup_range(code_start, code_len);
        if (block->raw_code == NULL) {
            FreeVec(block->language);
            FreeVec(block);
            return;
        }
        block->code_length = code_len;

        block_index++;
        block->index = block_index;

        AddTail((struct List *)node->codeblocks, (struct Node *)block);

        p = closing + opening_ticks;
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        if (p < end && *p == '\r') {
            p++;
        }
        if (p < end && *p == '\n') {
            p++;
        }
    }

    conversationNodeBuildDisplayOmittingCode(node, index_offset);
}
