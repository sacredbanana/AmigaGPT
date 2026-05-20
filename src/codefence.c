#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <string.h>

#include "codefence.h"
#include "AmigaGPT_cat.h"
#include "openai.h"

static BOOL is_line_start(const UBYTE *raw_base, const UBYTE *pos) {
    if (pos < raw_base) {
        return FALSE;
    }
    if (pos == raw_base) {
        return TRUE;
    }
    return pos[-1] == '\n';
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

static BOOL minlist_is_empty(const struct MinList *list) {
    if (list == NULL) {
        return TRUE;
    }
    return list->mlh_Head->mln_Succ == (struct MinNode *)&list->mlh_Tail;
}

/**
 * If codeblocks is non-empty, set display_text to raw_utf8 with each fenced
 * region replaced by a short UTF-8 placeholder (Phase 5.1 chat vs. code).
 * Otherwise display_text stays NULL (use raw in UI).
 */
static void conversationNodeBuildDisplayOmittingCode(
    struct ConversationNode *node) {
    CONST_STRPTR ph;
    ULONG placeholder_len;
    const UBYTE *raw;
    const UBYTE *end;
    const UBYTE *p;
    const UBYTE *segment_start;
    ULONG out_cap;
    STRPTR out;
    ULONG out_pos;
    ULONG tail_len;

    if (node == NULL || node->raw_utf8 == NULL || node->codeblocks == NULL) {
        return;
    }

    if (minlist_is_empty(node->codeblocks)) {
        return;
    }

    ph = (CONST_STRPTR)STRING_CHAT_CODEBLOCK_PLACEHOLDER;
    placeholder_len = (ULONG)strlen((const char *)ph);
    raw = (const UBYTE *)node->raw_utf8;
    if (node->raw_length == 0) {
        return;
    }
    end = raw + node->raw_length;
    out_cap = node->raw_length * 3 + 256;
    segment_start = raw;
    p = raw;

    while (p < end) {
        const UBYTE *fence;
        const UBYTE *closing;
        const UBYTE *q;
        ULONG seg_len;

        fence = p;
        for (; fence + 3 <= end; fence++) {
            if (fence[0] == '`' && fence[1] == '`' && fence[2] == '`') {
                break;
            }
        }
        if (fence + 3 > end) {
            break;
        }

        if (!is_line_start(raw, fence)) {
            p = fence + 3;
            continue;
        }

        seg_len = (ULONG)(fence - segment_start);
        if (out_pos + seg_len + placeholder_len + 1 >= out_cap) {
            goto fail;
        }
        if (seg_len > 0) {
            CopyMem((APTR)segment_start, out + out_pos, seg_len);
            out_pos += seg_len;
        }
        CopyMem((APTR)ph, out + out_pos, placeholder_len);
        out_pos += placeholder_len;

        p = fence + 3;
        while (p < end && (*p == ' ' || *p == '\t')) {
            p++;
        }
        while (p < end && *p != '\n' && *p != '\r') {
            p++;
        }
        if (p >= end) {
            goto fail;
        }
        if (p < end && *p == '\r') {
            p++;
        }
        if (p < end && *p == '\n') {
            p++;
        }

        closing = NULL;
        for (q = p; q + 3 <= end; q++) {
            if (q[0] == '`' && q[1] == '`' && q[2] == '`' &&
                is_line_start(raw, q)) {
                const UBYTE *r = q + 3;

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
            goto fail;
        }

        p = closing + 3;
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
    FreeVec(out);
}

void conversationNodeParseCodeFences(struct ConversationNode *node) {
    const UBYTE *raw;
    const UBYTE *end;
    const UBYTE *p;

    if (node == NULL || node->raw_utf8 == NULL || node->codeblocks == NULL) {
        return;
    }

    if (node->display_text != NULL) {
        FreeVec(node->display_text);
        node->display_text = NULL;
    }

    clear_codeblocks(node);

    raw = (const UBYTE *)node->raw_utf8;
    if (node->raw_length == 0) {
        return;
    }
    end = raw + node->raw_length;
    p = raw;

    while (p < end) {
        const UBYTE *fence;
        const UBYTE *lang_start;
        const UBYTE *lang_end;
        ULONG lang_len;
        const UBYTE *code_start;
        const UBYTE *closing;
        const UBYTE *q;
        ULONG code_len;
        struct AICodeBlock *block;

        fence = p;
        for (; fence + 3 <= end; fence++) {
            if (fence[0] == '`' && fence[1] == '`' && fence[2] == '`') {
                break;
            }
        }
        if (fence + 3 > end) {
            break;
        }

        if (!is_line_start(raw, fence)) {
            p = fence + 3;
            continue;
        }

        p = fence + 3;
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
        for (q = code_start; q + 3 <= end; q++) {
            if (q[0] == '`' && q[1] == '`' && q[2] == '`' &&
                is_line_start(raw, q)) {
                const UBYTE *r = q + 3;

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

        AddTail((struct List *)node->codeblocks, (struct Node *)block);

        p = closing + 3;
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

    conversationNodeBuildDisplayOmittingCode(node);
}
