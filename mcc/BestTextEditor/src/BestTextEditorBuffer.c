/*
 * BestTextEditor - document storage
 *
 * Paragraph buffers, smart resizing, insertion and deletion, the import and
 * export hooks, undo and redo, searching and the colour table.
 */
#include <exec/memory.h>
#include <proto/exec.h>
#include <string.h>
#include "BestTextEditorPrivate.h"

#define RTE_POOL_PUDDLE 8192
#define RTE_POOL_THRESH 2048
#define RTE_MAX_UNDO_TEXT (256 * 1024)

/* ------------------------------------------------------------------------ */
/* Memory                                                                    */
/* ------------------------------------------------------------------------ */

static APTR rteAlloc(struct RTEData *data, ULONG size) {
    return AllocPooled(data->pool, size);
}

static void rteFree(struct RTEData *data, APTR memory, ULONG size) {
    if (memory != NULL && size > 0)
        FreePooled(data->pool, memory, size);
}

/**
 * Capacity for a buffer holding len bytes: 16 byte steps for small buffers,
 * then 25% headroom so repeated typing or streaming grows in few steps
 */
static ULONG rteRoundCap(ULONG len) {
    ULONG cap;
    if (len < 64)
        return (len + 15) & ~15UL;
    cap = len + (len >> 2);
    return (cap + 15) & ~15UL;
}

static void rteFreeParaBuffers(struct RTEData *data, struct RTEPara *p) {
    rteFree(data, p->text, p->cap);
    rteFree(data, p->attrs, p->attrCap * sizeof(UWORD));
    rteFree(data, p->md, p->mdCap * sizeof(UWORD));
    rteFree(data, p->glyphs, p->glyphCap * sizeof(UWORD));
    rteFree(data, p->lines, p->maxLines * sizeof(struct RTELine));
    p->text = NULL;
    p->attrs = NULL;
    p->md = NULL;
    p->glyphs = NULL;
    p->lines = NULL;
    p->cap = p->attrCap = p->mdCap = p->glyphCap = p->glyphLen = 0;
    p->maxLines = p->numLines = 0;
}

static BOOL rteInitPara(struct RTEData *data, struct RTEPara *p) {
    memset(p, 0, sizeof(*p));
    p->cap = 16;
    p->text = rteAlloc(data, p->cap);
    if (p->text == NULL) {
        p->cap = 0;
        return FALSE;
    }
    p->text[0] = '\0';
    p->flags = RTEP_DIRTY;
    if (data->markdown)
        p->flags |= RTEP_MARKDOWN;
    return TRUE;
}

/**
 * Make room for len bytes (plus the NUL) in a paragraph, keeping the
 * attribute array the same size when there is one. Buffers that have become
 * four times larger than needed are shrunk.
 */
BOOL rteParaReserve(struct RTEData *data, struct RTEPara *p, ULONG len) {
    ULONG want = len + 1;
    ULONG newCap;
    UBYTE *text;

    if (want <= p->cap && !(p->cap > 256 && want < p->cap / 4))
        return TRUE;
    newCap = rteRoundCap(want);
    if (newCap == p->cap)
        return TRUE;
    text = rteAlloc(data, newCap);
    if (text == NULL)
        return want <= p->cap;
    CopyMem(p->text, text, RTE_MIN(p->len + 1, newCap));
    text[RTE_MIN(p->len, newCap - 1)] = '\0';
    rteFree(data, p->text, p->cap);
    p->text = text;
    p->cap = newCap;

    if (p->attrs != NULL) {
        UWORD *attrs = rteAlloc(data, newCap * sizeof(UWORD));
        if (attrs == NULL)
            return FALSE;
        memset(attrs, 0, newCap * sizeof(UWORD));
        CopyMem(p->attrs, attrs, RTE_MIN(p->len, newCap) * sizeof(UWORD));
        rteFree(data, p->attrs, p->attrCap * sizeof(UWORD));
        p->attrs = attrs;
        p->attrCap = newCap;
    }
    return TRUE;
}

BOOL rteParaEnsureAttrs(struct RTEData *data, struct RTEPara *p) {
    if (p->attrs != NULL)
        return TRUE;
    p->attrs = rteAlloc(data, p->cap * sizeof(UWORD));
    if (p->attrs == NULL)
        return FALSE;
    p->attrCap = p->cap;
    memset(p->attrs, 0, p->cap * sizeof(UWORD));
    return TRUE;
}

/** Drop an attribute array that holds nothing but zeroes */
static void rteParaCompactAttrs(struct RTEData *data, struct RTEPara *p) {
    ULONG i;
    if (p->attrs == NULL)
        return;
    for (i = 0; i < p->len; i++) {
        if (p->attrs[i] != 0)
            return;
    }
    rteFree(data, p->attrs, p->attrCap * sizeof(UWORD));
    p->attrs = NULL;
    p->attrCap = 0;
}

UWORD rteAttrAt(struct RTEPara *p, ULONG offset) {
    if (p->attrs == NULL || offset >= p->len)
        return 0;
    return p->attrs[offset];
}

static BOOL rteReserveParas(struct RTEData *data, ULONG count) {
    struct RTEPara *paras;
    ULONG newMax;
    if (count <= data->maxParas)
        return TRUE;
    newMax = data->maxParas < 16 ? 16 : data->maxParas + (data->maxParas >> 1);
    if (newMax < count)
        newMax = count + 16;
    paras = AllocVec(newMax * sizeof(struct RTEPara), MEMF_ANY | MEMF_CLEAR);
    if (paras == NULL)
        return FALSE;
    if (data->paras != NULL) {
        CopyMem(data->paras, paras, data->numParas * sizeof(struct RTEPara));
        FreeVec(data->paras);
    }
    data->paras = paras;
    data->maxParas = newMax;
    return TRUE;
}

/** Open a gap of count uninitialised paragraphs at index */
static BOOL rteOpenParas(struct RTEData *data, ULONG index, ULONG count) {
    if (!rteReserveParas(data, data->numParas + count))
        return FALSE;
    if (index < data->numParas)
        memmove(&data->paras[index + count], &data->paras[index],
                (data->numParas - index) * sizeof(struct RTEPara));
    data->numParas += count;
    return TRUE;
}

/** Remove count paragraphs at index, freeing their buffers */
static void rteRemoveParas(struct RTEData *data, ULONG index, ULONG count) {
    ULONG i;
    for (i = 0; i < count; i++)
        rteFreeParaBuffers(data, &data->paras[index + i]);
    if (index + count < data->numParas)
        memmove(&data->paras[index], &data->paras[index + count],
                (data->numParas - index - count) * sizeof(struct RTEPara));
    data->numParas -= count;
}

BOOL rteInitDocument(struct RTEData *data) {
    data->pool = CreatePool(MEMF_ANY, RTE_POOL_PUDDLE, RTE_POOL_THRESH);
    if (data->pool == NULL)
        return FALSE;
    if (!rteReserveParas(data, 16))
        return FALSE;
    data->numParas = 1;
    if (!rteInitPara(data, &data->paras[0]))
        return FALSE;
    data->totalLength = 0;
    data->numColors = 1;
    data->colors[0].kind = RTE_COLOR_CMAP;
    data->colors[0].value = 0;
    data->colors[0].pen = -1;
    data->nextBubbleId = 1;
    NewList((struct List *)&data->undoList);
    NewList((struct List *)&data->redoList);
    NewList((struct List *)&data->images);
    NewList((struct List *)&data->emoji);
    return TRUE;
}

void rteFreeDocument(struct RTEData *data) {
    rteFreeUndo(data, FALSE);
    if (data->paras != NULL) {
        FreeVec(data->paras);
        data->paras = NULL;
    }
    data->numParas = data->maxParas = 0;
    if (data->pool != NULL) {
        DeletePool(data->pool);
        data->pool = NULL;
    }
}

/* ------------------------------------------------------------------------ */
/* Positions                                                                 */
/* ------------------------------------------------------------------------ */

void rteMarkDirty(struct RTEData *data, ULONG para) {
    if (para >= data->numParas)
        para = data->numParas - 1;
    data->paras[para].flags |= RTEP_DIRTY;
    if (!data->anyDirty || para < data->firstDirty)
        data->firstDirty = para;
    data->anyDirty = TRUE;
}

void rteMarkAllDirty(struct RTEData *data) {
    ULONG i;
    for (i = 0; i < data->numParas; i++)
        data->paras[i].flags |= RTEP_DIRTY;
    data->firstDirty = 0;
    data->anyDirty = TRUE;
    data->layoutAll = TRUE;
}

ULONG rtePosToIndex(struct RTEData *data, struct RTEPos pos) {
    ULONG index = 0;
    ULONG i;
    if (pos.para >= data->numParas)
        return data->totalLength;
    for (i = 0; i < pos.para; i++)
        index += data->paras[i].len + 1;
    return index + RTE_MIN(pos.offset, data->paras[pos.para].len);
}

struct RTEPos rteIndexToPos(struct RTEData *data, ULONG index) {
    struct RTEPos pos;
    ULONG i;
    for (i = 0; i < data->numParas; i++) {
        if (index <= data->paras[i].len) {
            pos.para = i;
            pos.offset = index;
            return pos;
        }
        index -= data->paras[i].len + 1;
    }
    pos.para = data->numParas - 1;
    pos.offset = data->paras[pos.para].len;
    return pos;
}

LONG rteComparePos(struct RTEPos a, struct RTEPos b) {
    if (a.para != b.para)
        return a.para < b.para ? -1 : 1;
    if (a.offset != b.offset)
        return a.offset < b.offset ? -1 : 1;
    return 0;
}

void rteClampPos(struct RTEData *data, struct RTEPos *pos) {
    if (pos->para >= data->numParas) {
        pos->para = data->numParas - 1;
        pos->offset = data->paras[pos->para].len;
    }
    if (pos->offset > data->paras[pos->para].len)
        pos->offset = data->paras[pos->para].len;
}

BOOL rteHasSelection(struct RTEData *data) {
    return data->marking && rteComparePos(data->anchor, data->cursor) != 0;
}

void rteOrderedSelection(struct RTEData *data, struct RTEPos *from,
                         struct RTEPos *to) {
    if (rteComparePos(data->anchor, data->cursor) <= 0) {
        *from = data->anchor;
        *to = data->cursor;
    } else {
        *from = data->cursor;
        *to = data->anchor;
    }
}

BOOL rteIsWordChar(UBYTE c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_')
        return TRUE;
    /* Every byte of a UTF-8 sequence: letters of other scripts are words,
     * and a word boundary never falls inside a character */
    return c >= 0x80;
}

static void rteShiftBookmarks(struct RTEData *data, ULONG index, LONG delta,
                              ULONG deletedEnd) {
    ULONG i;
    for (i = 0; i < 10; i++) {
        if (!data->bookmarkSet[i] || data->bookmarks[i] < index)
            continue;
        if (delta < 0 && data->bookmarks[i] < deletedEnd)
            data->bookmarks[i] = index;
        else
            data->bookmarks[i] += delta;
    }
}

/* ------------------------------------------------------------------------ */
/* Undo                                                                      */
/* ------------------------------------------------------------------------ */

static void rteFreeUndoRecord(struct RTEUndo *u) {
    if (u->text != NULL)
        FreeVec(u->text);
    if (u->attrs != NULL)
        FreeVec(u->attrs);
    FreeVec(u);
}

void rteFreeUndo(struct RTEData *data, BOOL redoOnly) {
    struct RTEUndo *u;
    while ((u = (struct RTEUndo *)RemHead((struct List *)&data->redoList)))
        rteFreeUndoRecord(u);
    if (redoOnly)
        return;
    while ((u = (struct RTEUndo *)RemHead((struct List *)&data->undoList)))
        rteFreeUndoRecord(u);
    data->undoCount = 0;
}

void rteUndoBeginGroup(struct RTEData *data) { data->undoGroup++; }

static BOOL rteAnyAttrs(const UWORD *attrs, ULONG len) {
    ULONG i;
    if (attrs == NULL)
        return FALSE;
    for (i = 0; i < len; i++) {
        if (attrs[i] != 0)
            return TRUE;
    }
    return FALSE;
}

static void rteRecordUndo(struct RTEData *data, UBYTE type, ULONG index,
                          CONST_STRPTR text, const UWORD *attrs, ULONG len,
                          ULONG cursorBefore, ULONG cursorAfter,
                          BOOL mergeable) {
    struct RTEUndo *u;
    BOOL hasAttrs;

    if (data->inUndo || data->undoLevels == 0 || len == 0)
        return;
    if (len > RTE_MAX_UNDO_TEXT) {
        /* Too big to keep: the history before this point is lost */
        rteFreeUndo(data, FALSE);
        rteHandleError(data, data->obj, Error_NotEnoughUndoMem);
        return;
    }
    rteFreeUndo(data, TRUE);
    hasAttrs = rteAnyAttrs(attrs, len);

    u = (struct RTEUndo *)data->undoList.mlh_TailPred;
    if (mergeable && data->undoMergeOK && u->node.mln_Pred != NULL &&
        u->mergeable && u->type == type && !hasAttrs && u->attrs == NULL) {
        BOOL adjacent = FALSE;
        BOOL prepend = FALSE;
        if (type == RTE_U_INSERT && u->index + u->len == index)
            adjacent = TRUE;
        else if (type == RTE_U_DELETE && index == u->index)
            adjacent = TRUE; /* forward delete */
        else if (type == RTE_U_DELETE && index + len == u->index) {
            adjacent = TRUE; /* backspace */
            prepend = TRUE;
        }
        /* Start a new step at word boundaries so undo removes words, not
         * whole paragraphs of typing */
        if (adjacent && type == RTE_U_INSERT && len == 1 && u->len > 0 &&
            text[0] != ' ' && u->text[u->len - 1] == ' ')
            adjacent = FALSE;
        if (adjacent) {
            UBYTE *merged = AllocVec(u->len + len + 1, MEMF_ANY);
            if (merged != NULL) {
                if (prepend) {
                    CopyMem((APTR)text, merged, len);
                    CopyMem(u->text, merged + len, u->len);
                    u->index = index;
                } else {
                    CopyMem(u->text, merged, u->len);
                    CopyMem((APTR)text, merged + u->len, len);
                }
                u->len += len;
                merged[u->len] = '\0';
                FreeVec(u->text);
                u->text = merged;
                u->cursorAfter = cursorAfter;
                return;
            }
        }
    }

    u = AllocVec(sizeof(struct RTEUndo), MEMF_ANY | MEMF_CLEAR);
    if (u == NULL)
        return;
    u->text = AllocVec(len + 1, MEMF_ANY);
    if (u->text == NULL) {
        FreeVec(u);
        return;
    }
    CopyMem((APTR)text, u->text, len);
    u->text[len] = '\0';
    if (hasAttrs) {
        u->attrs = AllocVec(len * sizeof(UWORD), MEMF_ANY);
        if (u->attrs != NULL)
            CopyMem((APTR)attrs, u->attrs, len * sizeof(UWORD));
    }
    u->type = type;
    u->index = index;
    u->len = len;
    u->mergeable = mergeable;
    u->group = data->undoGroup;
    u->cursorBefore = cursorBefore;
    u->cursorAfter = cursorAfter;
    AddTail((struct List *)&data->undoList, (struct Node *)u);
    data->undoCount++;
    data->undoMergeOK = mergeable;

    while (data->undoCount > data->undoLevels) {
        struct RTEUndo *old =
            (struct RTEUndo *)RemHead((struct List *)&data->undoList);
        if (old == NULL)
            break;
        rteFreeUndoRecord(old);
        data->undoCount--;
    }
}

static void rteApplyUndoRecord(struct RTEData *data, struct RTEUndo *u,
                               BOOL forward) {
    BOOL insert = (u->type == RTE_U_INSERT) == forward;
    struct RTEPos from = rteIndexToPos(data, u->index);
    if (insert) {
        rteInsertText(data, from, u->text, u->len, u->attrs, NULL, FALSE);
    } else {
        struct RTEPos to = rteIndexToPos(data, u->index + u->len);
        rteDeleteRange(data, from, to, FALSE);
    }
}

BOOL rteUndo(struct RTEData *data) {
    struct RTEUndo *u = (struct RTEUndo *)data->undoList.mlh_TailPred;
    UWORD group;
    ULONG cursor = 0;

    if (u->node.mln_Pred == NULL)
        return FALSE;
    group = u->group;
    data->inUndo = TRUE;
    while (u->node.mln_Pred != NULL && u->group == group) {
        struct RTEUndo *prev = (struct RTEUndo *)u->node.mln_Pred;
        Remove((struct Node *)u);
        data->undoCount--;
        rteApplyUndoRecord(data, u, FALSE);
        cursor = u->cursorBefore;
        AddHead((struct List *)&data->redoList, (struct Node *)u);
        u = prev;
    }
    data->inUndo = FALSE;
    data->undoMergeOK = FALSE;
    data->marking = FALSE;
    data->cursor = rteIndexToPos(data, cursor);
    return TRUE;
}

BOOL rteRedo(struct RTEData *data) {
    struct RTEUndo *u = (struct RTEUndo *)data->redoList.mlh_Head;
    UWORD group;
    ULONG cursor = 0;

    if (u->node.mln_Succ == NULL)
        return FALSE;
    group = u->group;
    data->inUndo = TRUE;
    while (u->node.mln_Succ != NULL && u->group == group) {
        struct RTEUndo *next = (struct RTEUndo *)u->node.mln_Succ;
        Remove((struct Node *)u);
        rteApplyUndoRecord(data, u, TRUE);
        cursor = u->cursorAfter;
        AddTail((struct List *)&data->undoList, (struct Node *)u);
        data->undoCount++;
        u = next;
    }
    data->inUndo = FALSE;
    data->undoMergeOK = FALSE;
    data->marking = FALSE;
    data->cursor = rteIndexToPos(data, cursor);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Editing primitives                                                        */
/* ------------------------------------------------------------------------ */

static void rteCopyMeta(struct RTEPara *dst, const struct RTEPara *src) {
    dst->flow = src->flow;
    dst->bubble = src->bubble;
    dst->bubbleSide = src->bubbleSide;
    dst->bubbleId = src->bubbleId;
    dst->flags = (dst->flags & ~RTEP_MARKDOWN) | (src->flags & RTEP_MARKDOWN);
}

/** Insert raw bytes without newlines into a paragraph */
static BOOL rteParaInsert(struct RTEData *data, struct RTEPara *p, ULONG off,
                          CONST_STRPTR text, const UWORD *attrs, ULONG len) {
    if (len == 0)
        return TRUE;
    if (!rteParaReserve(data, p, p->len + len))
        return FALSE;
    if (attrs != NULL && p->attrs == NULL && rteAnyAttrs(attrs, len)) {
        if (!rteParaEnsureAttrs(data, p))
            return FALSE;
    }
    memmove(p->text + off + len, p->text + off, p->len - off + 1);
    CopyMem((APTR)text, p->text + off, len);
    p->glyphLen = 0;
    if (p->attrs != NULL) {
        memmove(p->attrs + off + len, p->attrs + off,
                (p->len - off) * sizeof(UWORD));
        if (attrs != NULL)
            CopyMem((APTR)attrs, p->attrs + off, len * sizeof(UWORD));
        else
            memset(p->attrs + off, 0, len * sizeof(UWORD));
    }
    p->len += len;
    return TRUE;
}

/** Remove bytes [from, to) from a paragraph */
static void rteParaRemove(struct RTEData *data, struct RTEPara *p, ULONG from,
                          ULONG to) {
    if (to <= from)
        return;
    memmove(p->text + from, p->text + to, p->len - to + 1);
    p->glyphLen = 0;
    if (p->attrs != NULL)
        memmove(p->attrs + from, p->attrs + to, (p->len - to) * sizeof(UWORD));
    p->len -= to - from;
    rteParaReserve(data, p, p->len);
}

/**
 * Copy the text (with '\n' between paragraphs) and attributes of a range
 */
static UBYTE *rteCopyRange(struct RTEData *data, struct RTEPos from,
                           struct RTEPos to, ULONG *lenOut, UWORD **attrsOut) {
    ULONG len = rtePosToIndex(data, to) - rtePosToIndex(data, from);
    UBYTE *text = AllocVec(len + 1, MEMF_ANY);
    UWORD *attrs = NULL;
    BOOL anyAttrs = FALSE;
    ULONG para, o = 0;

    if (text == NULL)
        return NULL;
    for (para = from.para; para <= to.para; para++) {
        struct RTEPara *p = &data->paras[para];
        if (p->attrs != NULL)
            anyAttrs = TRUE;
    }
    if (anyAttrs && attrsOut != NULL)
        attrs = AllocVec((len + 1) * sizeof(UWORD), MEMF_ANY | MEMF_CLEAR);
    for (para = from.para; para <= to.para; para++) {
        struct RTEPara *p = &data->paras[para];
        ULONG s = para == from.para ? from.offset : 0;
        ULONG e = para == to.para ? to.offset : p->len;
        if (e > s) {
            CopyMem(p->text + s, text + o, e - s);
            if (attrs != NULL && p->attrs != NULL)
                CopyMem(p->attrs + s, attrs + o, (e - s) * sizeof(UWORD));
            o += e - s;
        }
        if (para != to.para)
            text[o++] = '\n';
    }
    text[o] = '\0';
    *lenOut = o;
    if (attrsOut != NULL)
        *attrsOut = attrs;
    return text;
}

BOOL rteInsertText(struct RTEData *data, struct RTEPos pos, CONST_STRPTR text,
                   ULONG len, const UWORD *attrs, struct RTEPos *endPos,
                   BOOL recordUndo) {
    struct RTEPara *p;
    ULONG newlines = 0;
    ULONG i;
    ULONG index = 0;

    rteClampPos(data, &pos);
    if (recordUndo)
        index = rtePosToIndex(data, pos);
    for (i = 0; i < len; i++) {
        if (text[i] == '\n')
            newlines++;
    }

    p = &data->paras[pos.para];
    if (newlines == 0) {
        if (!rteParaInsert(data, p, pos.offset, text, attrs, len))
            return FALSE;
        rteMarkDirty(data, pos.para);
        if (endPos != NULL) {
            endPos->para = pos.para;
            endPos->offset = pos.offset + len;
        }
    } else {
        ULONG tailLen = p->len - pos.offset;
        UBYTE *tail = NULL;
        UWORD *tailAttrs = NULL;
        ULONG segStart = 0;
        ULONG para = pos.para;
        struct RTEPara meta = *p;

        if (tailLen > 0) {
            tail = AllocVec(tailLen, MEMF_ANY);
            if (tail == NULL)
                return FALSE;
            CopyMem(p->text + pos.offset, tail, tailLen);
            if (p->attrs != NULL) {
                tailAttrs = AllocVec(tailLen * sizeof(UWORD), MEMF_ANY);
                if (tailAttrs != NULL)
                    CopyMem(p->attrs + pos.offset, tailAttrs,
                            tailLen * sizeof(UWORD));
            }
        }
        if (!rteOpenParas(data, pos.para + 1, newlines)) {
            if (tail != NULL)
                FreeVec(tail);
            if (tailAttrs != NULL)
                FreeVec(tailAttrs);
            return FALSE;
        }
        for (i = 1; i <= newlines; i++) {
            struct RTEPara *np = &data->paras[pos.para + i];
            rteInitPara(data, np);
            rteCopyMeta(np, &meta);
        }
        p = &data->paras[pos.para];
        p->len = pos.offset;
        p->text[p->len] = '\0';
        p->glyphLen = 0;

        for (i = 0; i <= len; i++) {
            if (i == len || text[i] == '\n') {
                struct RTEPara *cur = &data->paras[para];
                rteParaInsert(data, cur, cur->len, text + segStart,
                              attrs != NULL ? attrs + segStart : NULL,
                              i - segStart);
                segStart = i + 1;
                if (i < len)
                    para++;
            }
        }
        if (endPos != NULL) {
            endPos->para = para;
            endPos->offset = data->paras[para].len;
        }
        if (tailLen > 0) {
            struct RTEPara *last = &data->paras[para];
            rteParaInsert(data, last, last->len, tail, tailAttrs, tailLen);
            FreeVec(tail);
            if (tailAttrs != NULL)
                FreeVec(tailAttrs);
        }
        rteParaCompactAttrs(data, &data->paras[pos.para]);
        for (i = pos.para; i <= para; i++)
            data->paras[i].flags |= RTEP_DIRTY;
        rteMarkDirty(data, pos.para);
    }

    data->totalLength += len;
    rteShiftBookmarks(data, rtePosToIndex(data, pos), (LONG)len, 0);
    if (recordUndo) {
        ULONG before = rtePosToIndex(data, data->cursor);
        rteRecordUndo(data, RTE_U_INSERT, index, text, attrs, len, before,
                      index + len, len == 1 && newlines == 0);
    }
    return TRUE;
}

BOOL rteDeleteRange(struct RTEData *data, struct RTEPos from, struct RTEPos to,
                    BOOL recordUndo) {
    ULONG fromIndex, toIndex;

    rteClampPos(data, &from);
    rteClampPos(data, &to);
    if (rteComparePos(from, to) > 0) {
        struct RTEPos t = from;
        from = to;
        to = t;
    }
    if (rteComparePos(from, to) == 0)
        return TRUE;

    fromIndex = rtePosToIndex(data, from);
    toIndex = rtePosToIndex(data, to);

    if (recordUndo && !data->inUndo && data->undoLevels > 0) {
        ULONG len;
        UWORD *attrs = NULL;
        UBYTE *text = rteCopyRange(data, from, to, &len, &attrs);
        if (text != NULL) {
            ULONG cursorIndex = rtePosToIndex(data, data->cursor);
            rteRecordUndo(data, RTE_U_DELETE, fromIndex, text, attrs, len,
                          cursorIndex, fromIndex, len == 1);
            FreeVec(text);
        }
        if (attrs != NULL)
            FreeVec(attrs);
    }

    if (from.para == to.para) {
        rteParaRemove(data, &data->paras[from.para], from.offset, to.offset);
    } else {
        struct RTEPara *first = &data->paras[from.para];
        struct RTEPara *last = &data->paras[to.para];
        ULONG tailLen = last->len - to.offset;
        first->len = from.offset;
        first->text[first->len] = '\0';
        first->glyphLen = 0;
        if (tailLen > 0) {
            if (last->attrs != NULL && first->attrs == NULL)
                rteParaEnsureAttrs(data, first);
            rteParaInsert(data, first, first->len, last->text + to.offset,
                          last->attrs != NULL ? last->attrs + to.offset : NULL,
                          tailLen);
        }
        rteRemoveParas(data, from.para + 1, to.para - from.para);
        rteParaReserve(data, &data->paras[from.para],
                       data->paras[from.para].len);
    }
    rteParaCompactAttrs(data, &data->paras[from.para]);
    rteMarkDirty(data, from.para);
    data->totalLength -= toIndex - fromIndex;
    rteShiftBookmarks(data, fromIndex, -(LONG)(toIndex - fromIndex), toIndex);
    return TRUE;
}

void rteClear(struct RTEData *data) {
    data->appendStyle = 0;
    data->appendColor = 0;
    if (data->numParas > 1)
        rteRemoveParas(data, 1, data->numParas - 1);
    rteFreeParaBuffers(data, &data->paras[0]);
    rteInitPara(data, &data->paras[0]);
    data->totalLength = 0;
    data->cursor.para = data->cursor.offset = 0;
    data->anchor = data->cursor;
    data->marking = FALSE;
    data->scrollY = data->scrollX = 0;
    memset(data->bookmarkSet, 0, sizeof(data->bookmarkSet));
    rteMarkAllDirty(data);
}

/**
 * Drop whole paragraphs from the top until the text fits the maximum length.
 * Used for transcripts that only ever grow at the end.
 */
void rteTrimToMaxLength(struct RTEData *data) {
    ULONG drop = 0;
    ULONG removed = 0;
    LONG removedHeight = 0;
    ULONG i;

    if (data->maxLength == 0 || data->totalLength <= data->maxLength)
        return;
    while (drop + 1 < data->numParas &&
           data->totalLength - removed > data->maxLength) {
        removed += data->paras[drop].len + 1;
        removedHeight += data->paras[drop].height;
        drop++;
    }
    /* Keep a bubble intact by dropping the rest of a cut bubble too */
    while (drop + 1 < data->numParas && data->paras[drop].bubble != 0 &&
           drop > 0 && data->paras[drop].bubbleId ==
                           data->paras[drop - 1].bubbleId) {
        removed += data->paras[drop].len + 1;
        removedHeight += data->paras[drop].height;
        drop++;
    }
    if (drop == 0)
        return;
    rteRemoveParas(data, 0, drop);
    data->totalLength -= removed;
    for (i = 0; i < 10; i++) {
        if (data->bookmarkSet[i]) {
            if (data->bookmarks[i] < removed)
                data->bookmarkSet[i] = FALSE;
            else
                data->bookmarks[i] -= removed;
        }
    }
    if (data->cursor.para < drop) {
        data->cursor.para = 0;
        data->cursor.offset = 0;
    } else {
        data->cursor.para -= drop;
    }
    if (data->anchor.para < drop) {
        data->anchor.para = 0;
        data->anchor.offset = 0;
    } else {
        data->anchor.para -= drop;
    }
    data->scrollY -= removedHeight;
    if (data->scrollY < 0)
        data->scrollY = 0;
    rteFreeUndo(data, FALSE);
    data->firstDirty = 0;
    data->anyDirty = TRUE;
    data->paras[0].flags |= RTEP_DIRTY;
    data->drawAll = TRUE;
}

/* ------------------------------------------------------------------------ */
/* Colours                                                                   */
/* ------------------------------------------------------------------------ */

UWORD rteColorIndex(struct RTEData *data, UBYTE kind, ULONG value) {
    UWORD i;
    if (kind == RTE_COLOR_CMAP && value == 0)
        return 0;
    for (i = 1; i < data->numColors; i++) {
        if (data->colors[i].kind == kind && data->colors[i].value == value)
            return i;
    }
    if (data->numColors >= RTE_MAX_COLORS)
        return 0;
    i = data->numColors++;
    data->colors[i].kind = kind;
    data->colors[i].value = value;
    data->colors[i].pen = -1;
    data->colors[i].allocated = FALSE;
    return i;
}

/* ------------------------------------------------------------------------ */
/* Import                                                                    */
/* ------------------------------------------------------------------------ */

/* A line produced by the importer before it is spliced into the document */
struct RTEImportLine {
    UBYTE *text;
    UWORD *attrs;
    ULONG len;
    ULONG cap;
    UBYTE flow;
    UBYTE separator;
    UBYTE flags;
    UBYTE bubble;
    UBYTE bubbleSide;
    BOOL metaSet; /* the line contained paragraph level escapes */
    BOOL anyAttr;
    UWORD bubbleId;
};

struct RTEImporter {
    struct RTEImportLine *lines;
    ULONG numLines;
    ULONG maxLines;
    /* running state */
    UBYTE style;
    UBYTE color;
    UBYTE flow;
    UBYTE flags;
    UBYTE bubble;
    UBYTE bubbleSide;
    UWORD bubbleId;
    BOOL failed;
};

static struct RTEImportLine *rteImpLine(struct RTEImporter *imp) {
    return &imp->lines[imp->numLines - 1];
}

static BOOL rteImpNewLine(struct RTEImporter *imp) {
    struct RTEImportLine *l;
    if (imp->numLines == imp->maxLines) {
        ULONG newMax = imp->maxLines == 0 ? 16 : imp->maxLines * 2;
        struct RTEImportLine *lines =
            AllocVec(newMax * sizeof(struct RTEImportLine), MEMF_ANY | MEMF_CLEAR);
        if (lines == NULL) {
            imp->failed = TRUE;
            return FALSE;
        }
        if (imp->lines != NULL) {
            CopyMem(imp->lines, lines,
                    imp->numLines * sizeof(struct RTEImportLine));
            FreeVec(imp->lines);
        }
        imp->lines = lines;
        imp->maxLines = newMax;
    }
    l = &imp->lines[imp->numLines++];
    memset(l, 0, sizeof(*l));
    l->flow = imp->flow;
    l->flags = imp->flags;
    l->bubble = imp->bubble;
    l->bubbleSide = imp->bubbleSide;
    l->bubbleId = imp->bubbleId;
    /* Soft styles and colours do not carry over to the next line */
    imp->style = 0;
    imp->color = 0;
    return TRUE;
}

static void rteImpPut(struct RTEImporter *imp, UBYTE c) {
    struct RTEImportLine *l = rteImpLine(imp);
    UWORD attr = RTE_A_MAKE(imp->style, imp->color);
    if (l->len + 1 >= l->cap) {
        ULONG newCap = l->cap < 64 ? 64 : l->cap * 2;
        UBYTE *text = AllocVec(newCap, MEMF_ANY);
        UWORD *attrs = AllocVec(newCap * sizeof(UWORD), MEMF_ANY);
        if (text == NULL || attrs == NULL) {
            if (text != NULL)
                FreeVec(text);
            if (attrs != NULL)
                FreeVec(attrs);
            imp->failed = TRUE;
            return;
        }
        if (l->text != NULL) {
            CopyMem(l->text, text, l->len);
            CopyMem(l->attrs, attrs, l->len * sizeof(UWORD));
            FreeVec(l->text);
            FreeVec(l->attrs);
        }
        l->text = text;
        l->attrs = attrs;
        l->cap = newCap;
    }
    l->text[l->len] = c;
    l->attrs[l->len] = attr;
    if (attr != 0)
        l->anyAttr = TRUE;
    l->len++;
}

static void rteImpFree(struct RTEImporter *imp) {
    ULONG i;
    for (i = 0; i < imp->numLines; i++) {
        if (imp->lines[i].text != NULL)
            FreeVec(imp->lines[i].text);
        if (imp->lines[i].attrs != NULL)
            FreeVec(imp->lines[i].attrs);
    }
    if (imp->lines != NULL)
        FreeVec(imp->lines);
}

static LONG rteHexValue(UBYTE c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

/**
 * Parse one escape sequence starting at text[i] (the ESC).
 * @return the number of bytes consumed
 */
static ULONG rteImpEscape(struct RTEData *data, struct RTEImporter *imp,
                          CONST_STRPTR text, ULONG i, ULONG len) {
    struct RTEImportLine *l = rteImpLine(imp);
    UBYTE c;
    if (i + 1 >= len)
        return 1;
    c = text[i + 1];
    switch (c) {
    case 'b':
        imp->style |= RTE_A_BOLD;
        return 2;
    case 'i':
        imp->style |= RTE_A_ITALIC;
        return 2;
    case 'u':
        imp->style |= RTE_A_UNDERLINE;
        return 2;
    case 'n':
        imp->style = 0;
        return 2;
    case 'h':
        l->flags |= RTEP_HIGHLIGHT;
        l->metaSet = TRUE;
        return 2;
    case 'l':
        imp->flow = l->flow = MUIV_TextEditor_Flow_Left;
        l->metaSet = TRUE;
        return 2;
    case 'r':
        imp->flow = l->flow = MUIV_TextEditor_Flow_Right;
        l->metaSet = TRUE;
        return 2;
    case 'c':
        imp->flow = l->flow = MUIV_TextEditor_Flow_Center;
        l->metaSet = TRUE;
        return 2;
    case 'j':
        imp->flow = l->flow = MUIV_TextEditor_Flow_Justified;
        l->metaSet = TRUE;
        return 2;
    case 'p':
    case 'P': {
        /* ESC p[n] colour map entry, ESC P[RRGGBB] direct colour */
        ULONG j = i + 2;
        ULONG value = 0;
        ULONG digits = 0;
        if (j >= len || text[j] != '[')
            return 2;
        j++;
        while (j < len && text[j] != ']') {
            LONG v = rteHexValue(text[j]);
            if (c == 'p') {
                if (text[j] < '0' || text[j] > '9')
                    break;
                value = value * 10 + (text[j] - '0');
            } else {
                if (v < 0)
                    break;
                value = (value << 4) | (ULONG)v;
            }
            digits++;
            j++;
        }
        if (j < len && text[j] == ']')
            j++;
        if (c == 'p')
            imp->color = rteColorIndex(data, RTE_COLOR_CMAP, value);
        else if (digits > 0)
            imp->color = rteColorIndex(data, RTE_COLOR_RGB, value & 0xffffff);
        return j - i;
    }
    case '[': {
        /* ESC [s:x] separator, ESC [B:..] bubble, ESC [M:x] markdown */
        ULONG j = i + 2;
        UBYTE kind;
        ULONG value = 0;
        UBYTE side = 0;
        if (j + 1 >= len || text[j + 1] != ':')
            return 2;
        kind = text[j];
        j += 2;
        while (j < len && text[j] >= '0' && text[j] <= '9') {
            value = value * 10 + (text[j] - '0');
            j++;
        }
        if (j < len && text[j] != ']') {
            switch (text[j]) {
            case 'l':
                side = MUIV_BestTextEditor_Bubble_Left;
                break;
            case 'r':
                side = MUIV_BestTextEditor_Bubble_Right;
                break;
            case 'c':
                side = MUIV_BestTextEditor_Bubble_Center;
                break;
            case 'f':
                side = MUIV_BestTextEditor_Bubble_Full;
                break;
            }
            while (j < len && text[j] != ']')
                j++;
        }
        if (j < len && text[j] == ']')
            j++;
        switch (kind) {
        case 's':
            l->separator = (UBYTE)value;
            l->metaSet = TRUE;
            break;
        case 'B':
            if (value == 0) {
                imp->bubble = 0;
                imp->bubbleSide = 0;
                imp->bubbleId = 0;
            } else {
                if (value > BESTTEXTEDITOR_MAX_BUBBLE_STYLES)
                    value = BESTTEXTEDITOR_MAX_BUBBLE_STYLES;
                imp->bubble = (UBYTE)value;
                imp->bubbleSide = side;
                imp->bubbleId = data->nextBubbleId++;
                if (data->nextBubbleId == 0)
                    data->nextBubbleId = 1;
            }
            l->bubble = imp->bubble;
            l->bubbleSide = imp->bubbleSide;
            l->bubbleId = imp->bubbleId;
            l->metaSet = TRUE;
            break;
        case 'M':
            if (value)
                imp->flags |= RTEP_MARKDOWN;
            else
                imp->flags &= ~RTEP_MARKDOWN;
            l->flags = (l->flags & ~RTEP_MARKDOWN) | (imp->flags & RTEP_MARKDOWN);
            l->metaSet = TRUE;
            break;
        }
        return j - i;
    }
    }
    return 2;
}

/**
 * The e-mail style soft styles: *bold*, /italic/, _underline_ and #colored#
 * around a word. Returns TRUE and the style when text[i] opens one.
 */
static BOOL rteEmailMarker(CONST_STRPTR text, ULONG i, ULONG len,
                           ULONG lineStart, UBYTE *style, ULONG *closeAt) {
    UBYTE c = text[i];
    ULONG j;
    if (c != '*' && c != '/' && c != '_' && c != '#')
        return FALSE;
    if (i > lineStart && rteIsWordChar(text[i - 1]))
        return FALSE;
    if (i + 1 >= len || text[i + 1] == ' ' || text[i + 1] == c ||
        text[i + 1] == '\n')
        return FALSE;
    for (j = i + 1; j < len && text[j] != '\n'; j++) {
        if (text[j] == c) {
            if (text[j - 1] == ' ')
                return FALSE;
            if (j + 1 < len && rteIsWordChar(text[j + 1]))
                return FALSE;
            *closeAt = j;
            switch (c) {
            case '*':
                *style = RTE_A_BOLD;
                break;
            case '/':
                *style = RTE_A_ITALIC;
                break;
            case '_':
                *style = RTE_A_UNDERLINE;
                break;
            default:
                *style = 0x80; /* coloured */
                break;
            }
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * Convert text through one of the built-in import hooks into import lines
 */
static void rteImpParse(struct RTEData *data, struct RTEImporter *imp,
                        CONST_STRPTR text, ULONG len, BOOL raw, ULONG hook) {
    ULONG i = 0;
    ULONG lineStart = 0;
    ULONG column = 0;
    BOOL email = !raw && (hook == MUIV_TextEditor_ImportHook_EMail ||
                          hook == MUIV_TextEditor_ImportHook_MIME ||
                          hook == MUIV_TextEditor_ImportHook_MIMEQuoted);
    BOOL mime = !raw && (hook == MUIV_TextEditor_ImportHook_MIME ||
                         hook == MUIV_TextEditor_ImportHook_MIMEQuoted);
    BOOL quoted = !raw && hook == MUIV_TextEditor_ImportHook_MIMEQuoted;
    ULONG emailClose = (ULONG)-1;
    UBYTE emailStyle = 0;
    UBYTE savedColor = 0;
    ULONG wrap = email ? data->importWrap : 0;
    ULONG lastSpace = (ULONG)-1;

    if (!rteImpNewLine(imp))
        return;
    while (i < len && !imp->failed) {
        UBYTE c = text[i];
        struct RTEImportLine *l = rteImpLine(imp);

        if (column == 0 && i == lineStart && email) {
            /* Line level conversions for the e-mail hooks */
            if (quoted) {
                rteImpPut(imp, '>');
                rteImpPut(imp, ' ');
                column += 2;
                l->flags |= RTEP_HIGHLIGHT;
            } else if (c == '>') {
                l->flags |= RTEP_HIGHLIGHT;
            }
            if (len - i >= 4 && strncmp(text + i, "<sb>", 4) == 0 &&
                (i + 4 == len || text[i + 4] == '\n')) {
                l->separator = LNSF_Middle;
                l->metaSet = TRUE;
                i += 4;
                continue;
            }
            if (len - i >= 5 && strncmp(text + i, "<tsb>", 5) == 0 &&
                (i + 5 == len || text[i + 5] == '\n')) {
                l->separator = LNSF_Middle | LNSF_Thick;
                l->metaSet = TRUE;
                i += 5;
                continue;
            }
        }

        if (c == '\r') {
            i++;
            continue;
        }
        if (mime && c == '=' && i + 1 < len) {
            /* Quoted printable: soft line break or an encoded byte */
            if (text[i + 1] == '\n') {
                i += 2;
                continue;
            }
            if (i + 2 < len && rteHexValue(text[i + 1]) >= 0 &&
                rteHexValue(text[i + 2]) >= 0) {
                c = (UBYTE)((rteHexValue(text[i + 1]) << 4) |
                            rteHexValue(text[i + 2]));
                i += 2;
            }
        }
        if (c == '\n') {
            if (!rteImpNewLine(imp))
                return;
            i++;
            lineStart = i;
            column = 0;
            lastSpace = (ULONG)-1;
            emailClose = (ULONG)-1;
            continue;
        }
        if (c == 0x1b && !raw) {
            i += rteImpEscape(data, imp, text, i, len);
            continue;
        }
        if (email) {
            UBYTE style;
            ULONG closeAt;
            if (i == emailClose) {
                if (emailStyle == 0x80)
                    imp->color = savedColor;
                else
                    imp->style &= ~emailStyle;
                emailClose = (ULONG)-1;
                i++;
                continue;
            }
            if (emailClose == (ULONG)-1 &&
                rteEmailMarker(text, i, len, lineStart, &style, &closeAt)) {
                emailStyle = style;
                emailClose = closeAt;
                if (style == 0x80) {
                    savedColor = imp->color;
                    imp->color = rteColorIndex(data, RTE_COLOR_CMAP, 6);
                } else {
                    imp->style |= style;
                }
                i++;
                continue;
            }
        }
        if (c == '\t' && data->convertTabs) {
            UWORD tab = data->tabSize > 0 ? data->tabSize : RTE_TAB_DEFAULT;
            do {
                rteImpPut(imp, ' ');
                column++;
            } while (column % tab != 0);
            i++;
            continue;
        }
        rteImpPut(imp, c);
        if (c == ' ')
            lastSpace = l->len - 1;
        if ((c & 0xc0) != 0x80)
            column++;
        i++;

        if (wrap > 0 && column > wrap && lastSpace != (ULONG)-1) {
            /* Hard wrap at the last space: move the rest to a new line */
            struct RTEImportLine *cur = rteImpLine(imp);
            ULONG restLen = cur->len - lastSpace - 1;
            UBYTE rest[1024];
            UWORD restAttrs[1024];
            ULONG k;
            if (restLen < sizeof(rest)) {
                CopyMem(cur->text + lastSpace + 1, rest, restLen);
                CopyMem(cur->attrs + lastSpace + 1, restAttrs,
                        restLen * sizeof(UWORD));
                cur->len = lastSpace;
                if (!rteImpNewLine(imp))
                    return;
                cur = rteImpLine(imp);
                for (k = 0; k < restLen; k++) {
                    UBYTE savedStyle = imp->style;
                    UBYTE savedCol = imp->color;
                    imp->style = (UBYTE)(restAttrs[k] & 0xff);
                    imp->color = (UBYTE)(restAttrs[k] >> 8);
                    rteImpPut(imp, rest[k]);
                    imp->style = savedStyle;
                    imp->color = savedCol;
                }
                column = restLen;
                lastSpace = (ULONG)-1;
            }
        }
    }
}

BOOL rteImport(struct RTEData *data, struct RTEPos pos, CONST_STRPTR text,
               LONG length, BOOL raw, ULONG hook, struct RTEPos *endPos,
               BOOL recordUndo) {
    struct RTEImporter imp;
    struct RTEPara *target;
    ULONG len;
    ULONG tailLen;
    UBYTE *tail = NULL;
    UWORD *tailAttrs = NULL;
    ULONG i;
    ULONG plainLen = 0;
    ULONG startIndex;
    BOOL ok = TRUE;

    if (text == NULL)
        text = (CONST_STRPTR) "";
    len = length < 0 ? strlen(text) : (ULONG)length;
    rteClampPos(data, &pos);

    /* Fast path: plain text without escapes goes straight in */
    if (raw || hook == MUIV_TextEditor_ImportHook_Plain) {
        BOOL simple = TRUE;
        for (i = 0; i < len; i++) {
            UBYTE c = text[i];
            if (c == 0x1b || c == '\r' || (c == '\t' && data->convertTabs)) {
                simple = FALSE;
                break;
            }
        }
        if (simple) {
            UWORD *attrs = NULL;
            UWORD fill = RTE_A_MAKE(data->typeStyle, data->typePen);
            BOOL res;
            BOOL newline = FALSE;
            if (data->importCarry) {
                fill = RTE_A_MAKE(data->appendStyle, data->appendColor);
                raw = TRUE;
                for (i = 0; i < len; i++) {
                    if (text[i] == '\n') {
                        /* a line break ends the carried style */
                        newline = TRUE;
                    }
                }
                if (newline) {
                    data->appendStyle = 0;
                    data->appendColor = 0;
                }
            }
            if (raw && fill != 0 && len > 0) {
                attrs = AllocVec(len * sizeof(UWORD), MEMF_ANY);
                if (attrs != NULL) {
                    /* Styles end with their line, so a carried style only
                     * covers the bytes up to the first line break */
                    BOOL inFirstLine = TRUE;
                    for (i = 0; i < len; i++) {
                        attrs[i] = (inFirstLine || !data->importCarry) ? fill
                                                                       : 0;
                        if (text[i] == '\n')
                            inFirstLine = FALSE;
                    }
                }
            }
            res = rteInsertText(data, pos, text, len, attrs, endPos, recordUndo);
            if (attrs != NULL)
                FreeVec(attrs);
            return res;
        }
    }

    memset(&imp, 0, sizeof(imp));
    target = &data->paras[pos.para];
    imp.flow = target->flow;
    imp.flags = target->flags & RTEP_MARKDOWN;
    imp.bubble = target->bubble;
    imp.bubbleSide = target->bubbleSide;
    imp.bubbleId = target->bubbleId;
    if (data->importCarry) {
        imp.style = data->appendStyle;
        imp.color = data->appendColor;
    } else if (raw) {
        imp.style = data->typeStyle;
        imp.color = data->typePen;
    }
    rteImpParse(data, &imp, text, len, raw, hook);
    if (data->importCarry) {
        data->appendStyle = imp.style;
        data->appendColor = imp.color;
    }
    if (imp.failed || imp.numLines == 0) {
        rteImpFree(&imp);
        rteHandleError(data, data->obj, Error_MemoryAllocationFailed);
        return FALSE;
    }

    startIndex = rtePosToIndex(data, pos);
    target = &data->paras[pos.para];
    tailLen = target->len - pos.offset;
    if (tailLen > 0 && imp.numLines > 1) {
        tail = AllocVec(tailLen, MEMF_ANY);
        if (tail != NULL) {
            CopyMem(target->text + pos.offset, tail, tailLen);
            if (target->attrs != NULL) {
                tailAttrs = AllocVec(tailLen * sizeof(UWORD), MEMF_ANY);
                if (tailAttrs != NULL)
                    CopyMem(target->attrs + pos.offset, tailAttrs,
                            tailLen * sizeof(UWORD));
            }
        } else {
            ok = FALSE;
        }
    }
    if (ok && imp.numLines > 1 &&
        !rteOpenParas(data, pos.para + 1, imp.numLines - 1))
        ok = FALSE;
    if (!ok) {
        if (tail != NULL)
            FreeVec(tail);
        if (tailAttrs != NULL)
            FreeVec(tailAttrs);
        rteImpFree(&imp);
        rteHandleError(data, data->obj, Error_MemoryAllocationFailed);
        return FALSE;
    }

    for (i = 0; i < imp.numLines; i++) {
        struct RTEImportLine *l = &imp.lines[i];
        struct RTEPara *p = &data->paras[pos.para + i];
        if (i == 0) {
            if (imp.numLines > 1) {
                p->len = pos.offset;
                p->text[p->len] = '\0';
                p->glyphLen = 0;
            }
            rteParaInsert(data, p, pos.offset, l->text,
                          l->anyAttr ? l->attrs : NULL, l->len);
            if (l->metaSet || (pos.offset == 0 && p->len == l->len)) {
                p->flow = l->flow;
                p->separator = l->separator;
                p->flags = (p->flags & ~(RTEP_HIGHLIGHT | RTEP_MARKDOWN)) |
                           (l->flags & (RTEP_HIGHLIGHT | RTEP_MARKDOWN));
                p->bubble = l->bubble;
                p->bubbleSide = l->bubbleSide;
                p->bubbleId = l->bubbleId;
            }
        } else {
            rteInitPara(data, p);
            rteParaInsert(data, p, 0, l->text, l->anyAttr ? l->attrs : NULL,
                          l->len);
            p->flow = l->flow;
            p->separator = l->separator;
            p->flags = RTEP_DIRTY | (l->flags & (RTEP_HIGHLIGHT | RTEP_MARKDOWN));
            p->bubble = l->bubble;
            p->bubbleSide = l->bubbleSide;
            p->bubbleId = l->bubbleId;
        }
        p->flags |= RTEP_DIRTY;
        plainLen += l->len + (i + 1 < imp.numLines ? 1 : 0);
    }
    if (tail != NULL) {
        struct RTEPara *last = &data->paras[pos.para + imp.numLines - 1];
        rteParaInsert(data, last, last->len, tail, tailAttrs, tailLen);
        FreeVec(tail);
        if (tailAttrs != NULL)
            FreeVec(tailAttrs);
    }
    if (endPos != NULL) {
        endPos->para = pos.para + imp.numLines - 1;
        endPos->offset = imp.lines[imp.numLines - 1].len +
                         (imp.numLines == 1 ? pos.offset : 0);
    }
    rteMarkDirty(data, pos.para);
    data->totalLength += plainLen;
    rteShiftBookmarks(data, startIndex, (LONG)plainLen, 0);

    if (recordUndo && plainLen > 0) {
        struct RTEPos end = rteIndexToPos(data, startIndex + plainLen);
        ULONG copied;
        UWORD *attrs = NULL;
        UBYTE *copy = rteCopyRange(data, pos, end, &copied, &attrs);
        if (copy != NULL) {
            rteRecordUndo(data, RTE_U_INSERT, startIndex, copy, attrs, copied,
                          rtePosToIndex(data, data->cursor),
                          startIndex + copied, FALSE);
            FreeVec(copy);
        }
        if (attrs != NULL)
            FreeVec(attrs);
    }
    rteImpFree(&imp);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Export                                                                    */
/* ------------------------------------------------------------------------ */

struct RTEOut {
    STRPTR buf;
    ULONG len;
    ULONG cap;
    BOOL failed;
};

static void rteOutPut(struct RTEOut *o, CONST_STRPTR s, ULONG n) {
    if (o->failed)
        return;
    if (o->len + n + 1 > o->cap) {
        ULONG newCap = o->cap < 256 ? 256 : o->cap;
        STRPTR buf;
        while (newCap < o->len + n + 1)
            newCap *= 2;
        buf = AllocVec(newCap, MEMF_ANY);
        if (buf == NULL) {
            o->failed = TRUE;
            return;
        }
        if (o->buf != NULL) {
            CopyMem(o->buf, buf, o->len);
            FreeVec(o->buf);
        }
        o->buf = buf;
        o->cap = newCap;
    }
    CopyMem((APTR)s, o->buf + o->len, n);
    o->len += n;
    o->buf[o->len] = '\0';
}

static void rteOutStr(struct RTEOut *o, CONST_STRPTR s) {
    rteOutPut(o, s, strlen(s));
}

static void rteOutChar(struct RTEOut *o, UBYTE c) { rteOutPut(o, &c, 1); }

static void rteExportStyle(struct RTEData *data, struct RTEOut *o,
                           UWORD from, UWORD to) {
    UBYTE fs = from & RTE_A_STYLEMASK, ts = to & RTE_A_STYLEMASK;
    UBYTE fc = RTE_A_COLOR(from), tc = RTE_A_COLOR(to);
    UBYTE buf[24];
    if (fs != ts) {
        if ((fs & ~ts) != 0) {
            rteOutStr(o, "\033n");
            fs = 0;
        }
        if ((ts & RTE_A_BOLD) && !(fs & RTE_A_BOLD))
            rteOutStr(o, "\033b");
        if ((ts & RTE_A_ITALIC) && !(fs & RTE_A_ITALIC))
            rteOutStr(o, "\033i");
        if ((ts & RTE_A_UNDERLINE) && !(fs & RTE_A_UNDERLINE))
            rteOutStr(o, "\033u");
    }
    if (fc != tc) {
        struct RTEColor *col = &data->colors[tc];
        if (tc == 0 || col->kind == RTE_COLOR_CMAP)
            rteFormat(buf, "\033p[", tc == 0 ? 0UL : col->value, 10, 1, "]");
        else
            rteFormat(buf, "\033P[", col->value, 16, 6, "]");
        rteOutStr(o, buf);
    }
}

/**
 * Append one paragraph slice to the export buffer, wrapping it at the export
 * wrap column when set
 */
static void rteExportSlice(struct RTEData *data, struct RTEOut *o,
                           struct RTEPara *p, ULONG s, ULONG e, ULONG hook,
                           ULONG wrap) {
    UWORD cur = 0;
    ULONG i;
    ULONG column = 0;
    ULONG lineOut = o->len;
    ULONG lastSpaceOut = (ULONG)-1;
    UBYTE emailOpen = 0;

    for (i = s; i < e; i++) {
        UWORD a = rteAttrAt(p, i);
        UBYTE c = p->text[i];
        if (hook == MUIV_TextEditor_ExportHook_Plain) {
            if (a != cur) {
                rteExportStyle(data, o, cur, a);
                cur = a;
            }
        } else if (hook == MUIV_TextEditor_ExportHook_EMail) {
            UBYTE want = 0;
            if (a & RTE_A_BOLD)
                want = '*';
            else if (a & RTE_A_ITALIC)
                want = '/';
            else if (a & RTE_A_UNDERLINE)
                want = '_';
            else if (RTE_A_COLOR(a) != 0)
                want = '#';
            if (want != emailOpen) {
                if (emailOpen != 0)
                    rteOutChar(o, emailOpen);
                if (want != 0)
                    rteOutChar(o, want);
                emailOpen = want;
            }
        }
        rteOutChar(o, c);
        if ((c & 0xc0) != 0x80)
            column++;
        if (c == ' ')
            lastSpaceOut = o->len - 1;
        if (wrap > 0 && column > wrap && lastSpaceOut != (ULONG)-1 &&
            lastSpaceOut >= lineOut) {
            o->buf[lastSpaceOut] = '\n';
            column = o->len - lastSpaceOut - 1;
            lineOut = lastSpaceOut + 1;
            lastSpaceOut = (ULONG)-1;
        }
    }
    if (emailOpen != 0)
        rteOutChar(o, emailOpen);
    if (hook == MUIV_TextEditor_ExportHook_Plain && cur != 0)
        rteExportStyle(data, o, cur, 0);
}

STRPTR rteExport(struct RTEData *data, struct RTEPos from, struct RTEPos to,
                 ULONG hook, ULONG wrap) {
    struct RTEOut o;
    ULONG para;
    UBYTE prevFlow = MUIV_TextEditor_Flow_Left;
    UBYTE prevBubble = 0;
    UWORD prevBubbleId = 0;
    BOOL prevMd = data->markdown;

    memset(&o, 0, sizeof(o));
    rteOutPut(&o, "", 0);
    rteClampPos(data, &from);
    rteClampPos(data, &to);
    for (para = from.para; para <= to.para && !o.failed; para++) {
        struct RTEPara *p = &data->paras[para];
        ULONG s = para == from.para ? from.offset : 0;
        ULONG e = para == to.para ? to.offset : p->len;
        UBYTE buf[32];

        if (hook == MUIV_TextEditor_ExportHook_Plain) {
            BOOL md = (p->flags & RTEP_MARKDOWN) != 0;
            if (p->bubbleId != prevBubbleId || p->bubble != prevBubble) {
                if (p->bubble == 0)
                    rteOutStr(&o, "\033[B:0]");
                else {
                    static const char *const sides[] = {"]", "l]", "r]",
                                                        "c]", "f]"};
                    rteFormat(buf, "\033[B:", p->bubble, 10, 1,
                              (CONST_STRPTR)sides[p->bubbleSide < 5
                                                      ? p->bubbleSide
                                                      : 0]);
                    rteOutStr(&o, buf);
                }
                prevBubble = p->bubble;
                prevBubbleId = p->bubbleId;
            }
            if (md != prevMd) {
                rteOutStr(&o, md ? "\033[M:1]" : "\033[M:0]");
                prevMd = md;
            }
            if (p->flow != prevFlow) {
                static const char flows[] = "lcrj";
                buf[0] = 0x1b;
                buf[1] = flows[p->flow & 3];
                rteOutPut(&o, buf, 2);
                prevFlow = p->flow;
            }
            if (p->flags & RTEP_HIGHLIGHT)
                rteOutStr(&o, "\033h");
            if (p->separator != 0) {
                rteFormat(buf, "\033[s:", p->separator, 10, 1, "]");
                rteOutStr(&o, buf);
            }
        } else if (p->separator != 0 && p->len == 0) {
            rteOutStr(&o, (p->separator & LNSF_Thick) ? "<tsb>" : "<sb>");
        }
        rteExportSlice(data, &o, p, s, e, hook, wrap);
        if (para != to.para)
            rteOutChar(&o, '\n');
    }
    if (o.failed) {
        if (o.buf != NULL)
            FreeVec(o.buf);
        return NULL;
    }
    return o.buf;
}

/**
 * The text as the reader sees it: Markdown syntax that is hidden is left out
 */
STRPTR rteExportVisible(struct RTEData *data, struct RTEPos from,
                        struct RTEPos to) {
    struct RTEOut o;
    ULONG para;
    memset(&o, 0, sizeof(o));
    rteOutPut(&o, "", 0);
    rteClampPos(data, &from);
    rteClampPos(data, &to);
    for (para = from.para; para <= to.para && !o.failed; para++) {
        struct RTEPara *p = &data->paras[para];
        ULONG s = para == from.para ? from.offset : 0;
        ULONG e = para == to.para ? to.offset : p->len;
        ULONG i;
        BOOL wholeHidden = p->len > 0;
        for (i = 0; i < p->len; i++) {
            if (!(rteStyleAt(p, i) & RTE_S_HIDDEN)) {
                wholeHidden = FALSE;
                break;
            }
        }
        if (wholeHidden && !(p->block == RTE_B_RULE))
            continue;
        for (i = s; i < e; i++) {
            if (!(rteStyleAt(p, i) & RTE_S_HIDDEN))
                rteOutChar(&o, p->text[i]);
        }
        if (para != to.para)
            rteOutChar(&o, '\n');
    }
    if (o.failed) {
        if (o.buf != NULL)
            FreeVec(o.buf);
        return NULL;
    }
    return o.buf;
}

/* ------------------------------------------------------------------------ */
/* Block operations                                                          */
/* ------------------------------------------------------------------------ */

void rteSetBlockAttrs(struct RTEData *data, struct RTEPos from,
                      struct RTEPos to, ULONG operation, ULONG value) {
    ULONG para;
    UBYTE color = 0;
    rteClampPos(data, &from);
    rteClampPos(data, &to);
    if (operation & MUIF_TextEditor_SetBlock_Color)
        color = (UBYTE)rteColorIndex(data, RTE_COLOR_CMAP, value);
    for (para = from.para; para <= to.para; para++) {
        struct RTEPara *p = &data->paras[para];
        ULONG s = para == from.para ? from.offset : 0;
        ULONG e = para == to.para ? to.offset : p->len;
        ULONG i;
        if (operation & MUIF_TextEditor_SetBlock_Flow)
            p->flow = (UBYTE)value;
        if ((operation & (MUIF_TextEditor_SetBlock_Color |
                          MUIF_TextEditor_SetBlock_StyleBold |
                          MUIF_TextEditor_SetBlock_StyleItalic |
                          MUIF_TextEditor_SetBlock_StyleUnderline)) &&
            e > s) {
            if (!rteParaEnsureAttrs(data, p))
                continue;
            for (i = s; i < e; i++) {
                UWORD a = p->attrs[i];
                UBYTE st = a & 0xff;
                UBYTE co = RTE_A_COLOR(a);
                if (operation & MUIF_TextEditor_SetBlock_StyleBold)
                    st = value ? st | RTE_A_BOLD : st & ~RTE_A_BOLD;
                if (operation & MUIF_TextEditor_SetBlock_StyleItalic)
                    st = value ? st | RTE_A_ITALIC : st & ~RTE_A_ITALIC;
                if (operation & MUIF_TextEditor_SetBlock_StyleUnderline)
                    st = value ? st | RTE_A_UNDERLINE : st & ~RTE_A_UNDERLINE;
                if (operation & MUIF_TextEditor_SetBlock_Color)
                    co = color;
                p->attrs[i] = RTE_A_MAKE(st, co);
            }
            rteParaCompactAttrs(data, p);
        }
        rteMarkDirty(data, para);
    }
}

/* ------------------------------------------------------------------------ */
/* Search                                                                    */
/* ------------------------------------------------------------------------ */

/**
 * Lower case of the byte at s[k] for matching: ASCII, and the Latin-1
 * letters, whose UTF-8 form is 0xc3 and a second byte 0x80-0x9e
 */
static UBYTE rteFold(CONST_STRPTR s, ULONG k) {
    UBYTE c = s[k];
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (k > 0 && (UBYTE)s[k - 1] == 0xc3 && c >= 0x80 && c <= 0x9e &&
        c != 0x97)
        return c + 32;
    return c;
}

static BOOL rteMatchAt(CONST_STRPTR text, ULONG len, ULONG at,
                       CONST_STRPTR pattern, ULONG plen, BOOL caseSensitive) {
    ULONG k;
    if (at + plen > len)
        return FALSE;
    /* never match from the middle of a character */
    if (((UBYTE)text[at] & 0xc0) == 0x80)
        return FALSE;
    for (k = 0; k < plen; k++) {
        UBYTE a = text[at + k], b = pattern[k];
        if (caseSensitive ? a != b
                          : rteFold(text + at, k) != rteFold(pattern, k))
            return FALSE;
    }
    return TRUE;
}

BOOL rteSearch(struct RTEData *data, CONST_STRPTR string, ULONG flags) {
    ULONG plen;
    BOOL cs = (flags & MUIF_TextEditor_Search_CaseSensitive) != 0;
    BOOL back = (flags & MUIF_TextEditor_Search_Backwards) != 0;
    struct RTEPos start;
    LONG para;

    if (string == NULL || (plen = strlen(string)) == 0)
        return FALSE;
    if (flags & MUIF_TextEditor_Search_FromTop) {
        if (back) {
            start.para = data->numParas - 1;
            start.offset = data->paras[start.para].len;
        } else {
            start.para = 0;
            start.offset = 0;
        }
    } else {
        struct RTEPos from, to;
        if (rteHasSelection(data)) {
            rteOrderedSelection(data, &from, &to);
            start = back ? from : to;
            if (!back && (flags & MUIF_TextEditor_Search_Next) == 0)
                start = from;
        } else {
            start = data->cursor;
        }
    }

    if (!back) {
        for (para = start.para; para < (LONG)data->numParas; para++) {
            struct RTEPara *p = &data->paras[para];
            ULONG i = (ULONG)para == start.para ? start.offset : 0;
            for (; i + plen <= p->len; i++) {
                if (rteMatchAt(p->text, p->len, i, string, plen, cs)) {
                    data->anchor.para = data->cursor.para = para;
                    data->anchor.offset = i;
                    data->cursor.offset = i + plen;
                    data->marking = TRUE;
                    return TRUE;
                }
            }
        }
    } else {
        for (para = start.para; para >= 0; para--) {
            struct RTEPara *p = &data->paras[para];
            LONG i = (ULONG)para == start.para ? (LONG)start.offset - (LONG)plen
                                               : (LONG)p->len - (LONG)plen;
            for (; i >= 0; i--) {
                if (rteMatchAt(p->text, p->len, i, string, plen, cs)) {
                    data->anchor.para = data->cursor.para = para;
                    data->anchor.offset = i;
                    data->cursor.offset = i + plen;
                    data->marking = TRUE;
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

/**
 * Write prefix, value and suffix into out. This stands in for snprintf(),
 * which a shared library cannot use without the C library's startup code.
 * @param base 10 or 16 (upper case hex digits)
 * @param digits minimum number of digits, padded with zeroes
 * @return out
 */
UBYTE *rteFormat(UBYTE *out, CONST_STRPTR prefix, ULONG value, UWORD base,
                 UWORD digits, CONST_STRPTR suffix) {
    static const char hex[] = "0123456789ABCDEF";
    UBYTE tmp[12];
    UWORD n = 0;
    UBYTE *o = out;

    while (*prefix != '\0')
        *o++ = *prefix++;
    do {
        tmp[n++] = hex[value % base];
        value /= base;
    } while (value != 0 && n < sizeof(tmp));
    while (n < digits && n < sizeof(tmp))
        tmp[n++] = '0';
    while (n > 0)
        *o++ = tmp[--n];
    while (*suffix != '\0')
        *o++ = *suffix++;
    *o = '\0';
    return out;
}
