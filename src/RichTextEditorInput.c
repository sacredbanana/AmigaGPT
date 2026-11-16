#ifndef DAEMON
/*
 * RichTextEditor - input
 *
 * Keyboard and mouse handling, the clipboard (IFF FTXT through iffparse),
 * keyboard macros and the TextEditor.mcc compatible ARexx command set.
 */
#include <devices/inputevent.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <libraries/iffparse.h>
#include <proto/exec.h>
#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <stdio.h>
#include <string.h>
#include "RichTextEditorPrivate.h"

#ifndef ID_FTXT
#define ID_FTXT MAKE_ID('F', 'T', 'X', 'T')
#endif
#ifndef ID_CHRS
#define ID_CHRS MAKE_ID('C', 'H', 'R', 'S')
#endif

#define RK_UP 0x4c
#define RK_DOWN 0x4d
#define RK_RIGHT 0x4e
#define RK_LEFT 0x4f
#define RK_BACKSPACE 0x41
#define RK_DELETE 0x46
#define RK_RETURN 0x44
#define RK_ENTER 0x43
#define RK_TAB 0x42
#define RK_ESC 0x45
#define RK_HELP 0x5f
#define RK_INSERT 0x47
#define RK_PAGEUP 0x48
#define RK_PAGEDOWN 0x49
#define RK_HOME 0x70
#define RK_END 0x71
#define RK_WHEEL_UP 0x7a
#define RK_WHEEL_DOWN 0x7b

#define QUAL_SHIFT (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)
#define QUAL_ALT (IEQUALIFIER_LALT | IEQUALIFIER_RALT)
#define QUAL_AMIGA (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)
#define QUAL_CTRL IEQUALIFIER_CONTROL
#define QUAL_KEYS (QUAL_SHIFT | QUAL_ALT | QUAL_AMIGA | QUAL_CTRL)

/* ------------------------------------------------------------------------ */
/* Positions                                                                 */
/* ------------------------------------------------------------------------ */

static BOOL rteLayoutReady(struct RTEData *data, Object *obj) {
    if (!data->setup || data->viewWidth <= 0)
        return FALSE;
    rteEnsureLayout(data, obj);
    return TRUE;
}

static struct RTEPos rtePosLeft(struct RTEData *data, struct RTEPos pos,
                                BOOL word) {
    if (pos.offset == 0) {
        if (pos.para > 0) {
            pos.para--;
            pos.offset = data->paras[pos.para].len;
        }
        return pos;
    }
    if (!word) {
        pos.offset--;
        return pos;
    }
    {
        struct RTEPara *p = &data->paras[pos.para];
        while (pos.offset > 0 && !rteIsWordChar(p->text[pos.offset - 1]))
            pos.offset--;
        while (pos.offset > 0 && rteIsWordChar(p->text[pos.offset - 1]))
            pos.offset--;
    }
    return pos;
}

static struct RTEPos rtePosRight(struct RTEData *data, struct RTEPos pos,
                                 BOOL word) {
    struct RTEPara *p = &data->paras[pos.para];
    if (pos.offset >= p->len) {
        if (pos.para + 1 < data->numParas) {
            pos.para++;
            pos.offset = 0;
        }
        return pos;
    }
    if (!word) {
        pos.offset++;
        return pos;
    }
    while (pos.offset < p->len && rteIsWordChar(p->text[pos.offset]))
        pos.offset++;
    while (pos.offset < p->len && !rteIsWordChar(p->text[pos.offset]))
        pos.offset++;
    return pos;
}

/** Start or end of the visual line the position is on */
static struct RTEPos rteLineEdge(struct RTEData *data, Object *obj,
                                 struct RTEPos pos, BOOL end) {
    struct RTEPara *p = &data->paras[pos.para];
    if (rteLayoutReady(data, obj) && p->numLines > 0) {
        struct RTELine *l = &p->lines[rteLineOfOffset(p, pos.offset)];
        if (!end) {
            pos.offset = l->start;
        } else {
            ULONG e = l->start + l->len;
            if (!(l->flags & RTEL_LAST) && e > l->start &&
                p->text[e - 1] == ' ')
                e--;
            pos.offset = e;
        }
    } else {
        pos.offset = end ? p->len : 0;
    }
    return pos;
}

/** Move a position by a number of visual lines, keeping the pixel column */
static struct RTEPos rtePosVertical(struct RTEData *data, Object *obj,
                                    struct RTEPos pos, LONG lines,
                                    BOOL keepWant) {
    struct RTEPara *p;
    ULONG li;
    LONG x;

    if (!rteLayoutReady(data, obj)) {
        LONG para = (LONG)pos.para + lines;
        if (para < 0)
            para = 0;
        if (para >= (LONG)data->numParas)
            para = data->numParas - 1;
        pos.para = para;
        rteClampPos(data, &pos);
        return pos;
    }
    p = &data->paras[pos.para];
    li = rteLineOfOffset(p, pos.offset);
    x = rteOffsetToX(data, p, &p->lines[li], pos.offset);
    if (keepWant && data->wantX >= 0)
        x = data->wantX;
    else
        data->wantX = x;

    while (lines < 0) {
        if (li > 0) {
            li--;
        } else if (pos.para > 0) {
            pos.para--;
            p = &data->paras[pos.para];
            li = p->numLines > 0 ? p->numLines - 1 : 0;
        } else {
            pos.offset = 0;
            return pos;
        }
        lines++;
    }
    while (lines > 0) {
        if (li + 1 < p->numLines) {
            li++;
        } else if (pos.para + 1 < data->numParas) {
            pos.para++;
            p = &data->paras[pos.para];
            li = 0;
        } else {
            pos.offset = p->len;
            return pos;
        }
        lines--;
    }
    if (p->numLines == 0) {
        pos.offset = 0;
        return pos;
    }
    pos.offset = rteXToOffset(data, p, &p->lines[li], x);
    return pos;
}

static LONG rtePageLines(struct RTEData *data) {
    LONG lh = data->lineHeight > 0 ? data->lineHeight : 8;
    LONG n = data->viewHeight / lh - 1;
    return n < 1 ? 1 : n;
}

/** Document position under window coordinates */
static struct RTEPos rtePosAtMouse(struct RTEData *data, Object *obj, LONG mx,
                                   LONG my) {
    struct RTEPos pos;
    LONG docY = data->scrollY + (my - data->viewTop);
    struct RTEPara *p;
    ULONG li;

    pos.para = 0;
    pos.offset = 0;
    if (!rteLayoutReady(data, obj))
        return pos;
    if (docY < 0)
        return pos;
    pos.para = rteParaAtY(data, docY);
    p = &data->paras[pos.para];
    if (docY >= p->y + p->height && pos.para + 1 >= data->numParas) {
        pos.offset = p->len;
        return pos;
    }
    if (p->numLines == 0)
        return pos;
    li = rteLineAtY(p, docY);
    pos.offset = rteXToOffset(data, p, &p->lines[li],
                              mx - data->viewLeft - RTE_PAD_X + data->scrollX);
    return pos;
}

static void rteInvalidatePosRange(struct RTEData *data, struct RTEPos a,
                                  struct RTEPos b) {
    ULONG from = RTE_MIN(a.para, b.para), to = RTE_MAX(a.para, b.para);
    if (from >= data->numParas || to >= data->numParas)
        return;
    rteInvalidateRange(data, data->paras[from].y,
                       data->paras[to].y + data->paras[to].height);
}

/** Put the cursor somewhere, extending or dropping the selection */
static void rteMoveTo(struct RTEData *data, Object *obj, struct RTEPos pos,
                      BOOL extend) {
    rteClampPos(data, &pos);
    if (extend) {
        if (!data->marking) {
            data->anchor = data->cursor;
            data->marking = TRUE;
        }
        rteInvalidatePosRange(data, data->cursor, pos);
    } else {
        if (rteHasSelection(data))
            rteInvalidatePosRange(data, data->anchor, data->cursor);
        data->marking = FALSE;
        rteInvalidateCursorLine(data);
    }
    data->cursor = pos;
    rteInvalidateCursorLine(data);
    rteAfterCursorMove(data, obj, extend);
}

/* ------------------------------------------------------------------------ */
/* Editing                                                                   */
/* ------------------------------------------------------------------------ */

/** Remove the selection if there is one. TRUE if something was removed. */
static BOOL rteDeleteSelection(struct RTEData *data) {
    struct RTEPos from, to;
    if (!rteHasSelection(data)) {
        data->marking = FALSE;
        return FALSE;
    }
    rteOrderedSelection(data, &from, &to);
    rteInvalidatePosRange(data, from, to);
    rteDeleteRange(data, from, to, TRUE);
    data->cursor = from;
    data->anchor = from;
    data->marking = FALSE;
    return TRUE;
}

static void rteInsertAtCursor(struct RTEData *data, Object *obj,
                              CONST_STRPTR text, LONG len, BOOL raw) {
    struct RTEPos end;
    rteDeleteSelection(data);
    if (data->overwrite && raw && len == 1 && text[0] != '\n') {
        struct RTEPara *p = &data->paras[data->cursor.para];
        if (data->cursor.offset < p->len) {
            struct RTEPos next = data->cursor;
            next.offset++;
            rteDeleteRange(data, data->cursor, next, TRUE);
        }
    }
    if (rteImport(data, data->cursor, text, len, raw,
                  MUIV_TextEditor_ImportHook_Plain, &end, TRUE))
        data->cursor = end;
    data->anchor = data->cursor;
    rteAfterEdit(data, obj);
}

/** Hard wrap: break the cursor's paragraph at the wrap border */
static void rteHardWrap(struct RTEData *data, Object *obj) {
    struct RTEPara *p;
    LONG i;
    if (data->wrapMode != MUIV_TextEditor_WrapMode_HardWrap ||
        data->wrapBorder == 0)
        return;
    p = &data->paras[data->cursor.para];
    if (p->len <= data->wrapBorder)
        return;
    for (i = RTE_MIN((LONG)data->wrapBorder, (LONG)p->len - 1); i > 0; i--) {
        if (p->text[i] == ' ') {
            struct RTEPos at, after;
            ULONG cursorIndex = rtePosToIndex(data, data->cursor);
            at.para = data->cursor.para;
            at.offset = (ULONG)i;
            after = at;
            after.offset++;
            rteDeleteRange(data, at, after, TRUE);
            rteInsertText(data, at, "\n", 1, NULL, NULL, TRUE);
            data->cursor = rteIndexToPos(data, cursorIndex);
            data->anchor = data->cursor;
            rteAfterEdit(data, obj);
            return;
        }
    }
}

/** Check the word before the cursor against MUIA_TextEditor_Keywords */
static void rteCheckKeyword(struct RTEData *data, Object *obj) {
    struct RTEPara *p = &data->paras[data->cursor.para];
    ULONG end = data->cursor.offset;
    ULONG start;
    CONST_STRPTR k;
    ULONG wlen;

    if (data->keywords == NULL)
        return;
    if (end > 0 && (p->text[end - 1] == ' ' || p->text[end - 1] == '\t'))
        end--;
    start = end;
    while (start > 0 && rteIsWordChar(p->text[start - 1]))
        start--;
    wlen = end - start;
    if (wlen == 0 || wlen >= sizeof(data->matchedKeyword))
        return;
    for (k = data->keywords; *k != '\0';) {
        CONST_STRPTR e = k;
        while (*e != '\0' && *e != ',')
            e++;
        if ((ULONG)(e - k) == wlen &&
            strncmp(k, (STRPTR)p->text + start, wlen) == 0) {
            CopyMem((APTR)k, data->matchedKeyword, wlen);
            data->matchedKeyword[wlen] = '\0';
            SetAttrs(obj, MUIA_TextEditor_MatchedKeyword,
                     (IPTR)data->matchedKeyword, TAG_DONE);
            return;
        }
        k = *e == ',' ? e + 1 : e;
    }
}

/* ------------------------------------------------------------------------ */
/* Clipboard                                                                 */
/* ------------------------------------------------------------------------ */

static BOOL rteWriteClip(CONST_STRPTR text, ULONG len) {
    struct Library *IFFParseBase = OpenLibrary("iffparse.library", 39);
#if defined(__AMIGAOS4__)
    struct IFFParseIFace *IIFFParse = NULL;
#endif
    struct IFFHandle *iff;
    BOOL ok = FALSE;

    if (IFFParseBase == NULL)
        return FALSE;
#if defined(__AMIGAOS4__)
    IIFFParse = (struct IFFParseIFace *)GetInterface(IFFParseBase, "main", 1,
                                                     NULL);
    if (IIFFParse == NULL) {
        CloseLibrary(IFFParseBase);
        return FALSE;
    }
#endif
    if ((iff = AllocIFF()) != NULL) {
        iff->iff_Stream = (IPTR)OpenClipboard(PRIMARY_CLIP);
        if (iff->iff_Stream != 0) {
            InitIFFasClip(iff);
            if (OpenIFF(iff, IFFF_WRITE) == 0) {
                if (PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN) == 0) {
                    if (PushChunk(iff, 0, ID_CHRS, IFFSIZE_UNKNOWN) == 0) {
                        ok = WriteChunkBytes(iff, (APTR)text, len) == (LONG)len;
                        PopChunk(iff);
                    }
                    PopChunk(iff);
                }
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
#if defined(__AMIGAOS4__)
    DropInterface((struct Interface *)IIFFParse);
#endif
    CloseLibrary(IFFParseBase);
    return ok;
}

/** Read FTXT CHRS from the clipboard. Returns an AllocVec'd string. */
static STRPTR rteReadClip(LONG *error) {
    struct Library *IFFParseBase = OpenLibrary("iffparse.library", 39);
#if defined(__AMIGAOS4__)
    struct IFFParseIFace *IIFFParse = NULL;
#endif
    struct IFFHandle *iff;
    STRPTR text = NULL;
    ULONG len = 0;
    BOOL foundForm = FALSE;

    *error = Error_ClipboardIsEmpty;
    if (IFFParseBase == NULL)
        return NULL;
#if defined(__AMIGAOS4__)
    IIFFParse = (struct IFFParseIFace *)GetInterface(IFFParseBase, "main", 1,
                                                     NULL);
    if (IIFFParse == NULL) {
        CloseLibrary(IFFParseBase);
        return NULL;
    }
#endif
    if ((iff = AllocIFF()) != NULL) {
        iff->iff_Stream = (IPTR)OpenClipboard(PRIMARY_CLIP);
        if (iff->iff_Stream != 0) {
            InitIFFasClip(iff);
            if (OpenIFF(iff, IFFF_READ) == 0) {
                if (StopChunk(iff, ID_FTXT, ID_CHRS) == 0) {
                    while (ParseIFF(iff, IFFPARSE_SCAN) == 0) {
                        struct ContextNode *cn = CurrentChunk(iff);
                        STRPTR bigger;
                        LONG got;
                        foundForm = TRUE;
                        if (cn == NULL || cn->cn_Size <= 0)
                            continue;
                        bigger = AllocVec(len + cn->cn_Size + 1, MEMF_ANY);
                        if (bigger == NULL)
                            break;
                        if (text != NULL) {
                            CopyMem(text, bigger, len);
                            FreeVec(text);
                        }
                        text = bigger;
                        got = ReadChunkBytes(iff, text + len, cn->cn_Size);
                        if (got > 0)
                            len += got;
                        text[len] = '\0';
                    }
                }
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
#if defined(__AMIGAOS4__)
    DropInterface((struct Interface *)IIFFParse);
#endif
    CloseLibrary(IFFParseBase);
    if (text == NULL && foundForm)
        *error = Error_ClipboardIsNotFTXT;
    if (text != NULL)
        *error = 0;
    return text;
}

BOOL rteCopy(struct RTEData *data, Object *obj, BOOL cut) {
    struct RTEPos from, to;
    STRPTR text;
    BOOL ok;
    if (!rteHasSelection(data)) {
        rteHandleError(data, obj, Error_NoAreaMarked);
        return FALSE;
    }
    rteOrderedSelection(data, &from, &to);
    if (data->hideMarkup && data->markdown)
        text = rteExportVisible(data, from, to);
    else
        text = rteExport(data, from, to, MUIV_TextEditor_ExportHook_NoStyle, 0);
    if (text == NULL)
        return FALSE;
    ok = rteWriteClip(text, strlen(text));
    FreeVec(text);
    if (ok && cut && !data->readOnly) {
        rteUndoBeginGroup(data);
        rteDeleteSelection(data);
        rteAfterEdit(data, obj);
    }
    return ok;
}

BOOL rtePaste(struct RTEData *data, Object *obj) {
    LONG error;
    STRPTR text;
    if (data->readOnly)
        return FALSE;
    text = rteReadClip(&error);
    if (text == NULL) {
        rteHandleError(data, obj, (ULONG)error);
        return FALSE;
    }
    rteUndoBeginGroup(data);
    rteInsertAtCursor(data, obj, text, strlen(text), TRUE);
    FreeVec(text);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Keyboard                                                                  */
/* ------------------------------------------------------------------------ */

static LONG rteMapKey(struct IntuiMessage *imsg, UBYTE *buffer, LONG size) {
    struct Library *KeymapBase = OpenLibrary("keymap.library", 37);
#if defined(__AMIGAOS4__)
    struct KeymapIFace *IKeymap = NULL;
#endif
    struct InputEvent ie;
    LONG n = 0;

    if (KeymapBase == NULL)
        return 0;
#if defined(__AMIGAOS4__)
    IKeymap = (struct KeymapIFace *)GetInterface(KeymapBase, "main", 1, NULL);
    if (IKeymap == NULL) {
        CloseLibrary(KeymapBase);
        return 0;
    }
#endif
    memset(&ie, 0, sizeof(ie));
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = imsg->Code;
    ie.ie_Qualifier = imsg->Qualifier;
    if (imsg->IAddress != NULL)
        ie.ie_EventAddress = (APTR) * ((ULONG *)imsg->IAddress);
    n = MapRawKey(&ie, buffer, (WORD)size, NULL);
#if defined(__AMIGAOS4__)
    DropInterface((struct Interface *)IKeymap);
#endif
    CloseLibrary(KeymapBase);
    return n;
}

static void rteScrollBy(struct RTEData *data, Object *obj, LONG dy) {
    rteScrollTo(data, obj, data->scrollY + dy);
}

static BOOL rteSubmit(struct RTEData *data, Object *obj) {
    if (data->submitHook == NULL)
        return FALSE;
    if (data->totalLength == 0 && !data->submitEmpty)
        return TRUE; /* nothing to send: eat the key */
    DoMethod(obj, MUIM_CallHook, data->submitHook, NULL, NULL);
    return TRUE;
}

/**
 * Handle a key press
 * @return TRUE if the key was used
 */
static BOOL rteKey(struct RTEData *data, Object *obj, struct IntuiMessage *imsg) {
    UWORD code = imsg->Code;
    UWORD qual = imsg->Qualifier;
    BOOL shift = (qual & QUAL_SHIFT) != 0;
    BOOL alt = (qual & QUAL_ALT) != 0;
    BOOL ctrl = (qual & QUAL_CTRL) != 0;
    BOOL amigaKey = (qual & QUAL_AMIGA) != 0;
    struct RTEPos pos;
    UBYTE buf[16];
    LONG n;

    if (data->macroRecording && !data->macroPlaying &&
        data->macroLen < RTE_MAX_MACRO) {
        data->macroCodes[data->macroLen] = code;
        data->macroQuals[data->macroLen] = qual;
        data->macroLen++;
    }

    /* Tab is typed in an editable object; Amiga+Tab or Ctrl+Tab move to the
     * next gadget instead (with Shift, the previous one) */
    if (code == RK_TAB && (amigaKey || ctrl) && _win(obj) != NULL) {
        set(_win(obj), MUIA_Window_ActiveObject,
            shift ? MUIV_Window_ActiveObject_Prev
                  : MUIV_Window_ActiveObject_Next);
        return TRUE;
    }

    /* Amiga shortcuts */
    if (amigaKey && !alt && !ctrl) {
        n = rteMapKey(imsg, buf, sizeof(buf));
        if (n == 1) {
            switch (buf[0] | 0x20) {
            case 'c':
                rteCopy(data, obj, FALSE);
                return TRUE;
            case 'a':
                DoMethod(obj, MUIM_TextEditor_MarkText, 0, 0,
                         MUIV_TextEditor_MarkText_All, 0);
                return TRUE;
            case 'x':
                if (data->readOnly)
                    return FALSE;
                rteCopy(data, obj, TRUE);
                return TRUE;
            case 'v':
                if (data->readOnly)
                    return FALSE;
                rtePaste(data, obj);
                return TRUE;
            case 'z':
                if (data->readOnly)
                    return FALSE;
                DoMethod(obj, MUIM_TextEditor_ARexxCmd,
                         shift ? (IPTR) "REDO" : (IPTR) "UNDO");
                return TRUE;
            case 'y':
                if (data->readOnly)
                    return FALSE;
                DoMethod(obj, MUIM_TextEditor_ARexxCmd, (IPTR) "REDO");
                return TRUE;
            }
        }
        return FALSE;
    }

    switch (code) {
    case RK_UP:
    case RK_DOWN: {
        LONG dir = code == RK_UP ? -1 : 1;
        if (data->readOnly && !shift) {
            if (ctrl)
                rteScrollTo(data, obj, dir < 0 ? 0 : 0x3fffffff);
            else
                rteScrollBy(data, obj,
                            dir * (alt ? data->viewHeight - data->lineHeight
                                       : data->lineHeight));
            return TRUE;
        }
        if (ctrl) {
            pos.para = dir < 0 ? 0 : data->numParas - 1;
            pos.offset = dir < 0 ? 0 : data->paras[pos.para].len;
        } else {
            if (dir < 0 && !alt && data->keyUpFocus != NULL &&
                data->cursor.para == 0 && _win(obj) != NULL &&
                rteLineOfOffset(&data->paras[0], data->cursor.offset) == 0) {
                set(_win(obj), MUIA_Window_ActiveObject, data->keyUpFocus);
                return TRUE;
            }
            pos = rtePosVertical(data, obj, data->cursor,
                                 dir * (alt ? rtePageLines(data) : 1), TRUE);
        }
        rteMoveTo(data, obj, pos, shift);
        return TRUE;
    }
    case RK_PAGEUP:
    case RK_PAGEDOWN: {
        LONG dir = code == RK_PAGEUP ? -1 : 1;
        if (data->readOnly && !shift) {
            rteScrollBy(data, obj, dir * (data->viewHeight - data->lineHeight));
            return TRUE;
        }
        pos = rtePosVertical(data, obj, data->cursor, dir * rtePageLines(data),
                             TRUE);
        rteMoveTo(data, obj, pos, shift);
        return TRUE;
    }
    case RK_LEFT:
    case RK_RIGHT:
        if (data->readOnly && !shift)
            return FALSE;
        if (ctrl)
            pos = rteLineEdge(data, obj, data->cursor, code == RK_RIGHT);
        else if (!shift && rteHasSelection(data) && !alt) {
            struct RTEPos from, to;
            rteOrderedSelection(data, &from, &to);
            pos = code == RK_LEFT ? from : to;
        } else if (code == RK_LEFT)
            pos = rtePosLeft(data, data->cursor, alt);
        else
            pos = rtePosRight(data, data->cursor, alt);
        data->wantX = -1;
        rteMoveTo(data, obj, pos, shift);
        return TRUE;
    case RK_HOME:
    case RK_END:
        if (data->readOnly && !shift) {
            rteScrollTo(data, obj, code == RK_HOME ? 0 : 0x3fffffff);
            return TRUE;
        }
        if (ctrl) {
            pos.para = code == RK_HOME ? 0 : data->numParas - 1;
            pos.offset = code == RK_HOME ? 0 : data->paras[pos.para].len;
        } else {
            pos = rteLineEdge(data, obj, data->cursor, code == RK_END);
        }
        data->wantX = -1;
        rteMoveTo(data, obj, pos, shift);
        return TRUE;
    case RK_ESC:
        if (rteHasSelection(data)) {
            rteMoveTo(data, obj, data->cursor, FALSE);
            return TRUE;
        }
        return FALSE;
    case RK_HELP:
        return FALSE;
    }

    if (data->readOnly)
        return FALSE;

    switch (code) {
    case RK_INSERT:
        data->overwrite = !data->overwrite;
        rteInvalidateCursorLine(data);
        rteRequestRedraw(data, obj);
        return TRUE;
    case RK_RETURN:
    case RK_ENTER:
        if (!(qual & (QUAL_SHIFT | QUAL_ALT | QUAL_CTRL)) &&
            data->submitHook != NULL)
            return rteSubmit(data, obj);
        rteUndoBeginGroup(data);
        rteCheckKeyword(data, obj);
        rteInsertAtCursor(data, obj, "\n", 1, TRUE);
        return TRUE;
    case RK_BACKSPACE:
    case RK_DELETE: {
        struct RTEPos from = data->cursor, to = data->cursor;
        rteUndoBeginGroup(data);
        if (!rteDeleteSelection(data)) {
            if (code == RK_BACKSPACE) {
                if (shift)
                    from.offset = 0;
                else
                    from = rtePosLeft(data, data->cursor, alt);
            } else {
                if (shift)
                    to.offset = data->paras[to.para].len;
                else
                    to = rtePosRight(data, data->cursor, alt);
            }
            if (rteComparePos(from, to) != 0) {
                rteDeleteRange(data, from, to, TRUE);
                data->cursor = from;
            }
        }
        data->anchor = data->cursor;
        data->wantX = -1;
        rteAfterEdit(data, obj);
        return TRUE;
    }
    case RK_TAB:
        rteUndoBeginGroup(data);
        rteCheckKeyword(data, obj);
        if (data->convertTabs) {
            UWORD tab = data->tabSize > 0 ? data->tabSize : RTE_TAB_DEFAULT;
            UBYTE spaces[16];
            UWORD count = tab - (data->cursor.offset % tab);
            memset(spaces, ' ', sizeof(spaces));
            rteInsertAtCursor(data, obj, spaces, RTE_MIN(count, 16), TRUE);
        } else {
            rteInsertAtCursor(data, obj, "\t", 1, TRUE);
        }
        return TRUE;
    }

    if (code & IECODE_UP_PREFIX)
        return FALSE;
    if (ctrl && !alt)
        return FALSE;
    n = rteMapKey(imsg, buf, sizeof(buf) - 1);
    if (n > 0) {
        LONG i, out = 0;
        for (i = 0; i < n; i++) {
            if (buf[i] >= 0x20 && buf[i] != 0x7f && !(buf[i] >= 0x80 &&
                                                      buf[i] < 0xa0))
                buf[out++] = buf[i];
        }
        if (out > 0) {
            BOOL space = buf[0] == ' ';
            if (space)
                rteCheckKeyword(data, obj);
            rteUndoBeginGroup(data);
            rteInsertAtCursor(data, obj, buf, out, TRUE);
            data->wantX = -1;
            rteHardWrap(data, obj);
            return TRUE;
        }
    }
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/* Mouse                                                                     */
/* ------------------------------------------------------------------------ */

static BOOL rteInView(struct RTEData *data, LONG mx, LONG my) {
    return mx >= data->viewLeft && mx < data->viewLeft + data->viewWidth &&
           my >= data->viewTop && my < data->viewTop + data->viewHeight;
}

static void rteSelectUnit(struct RTEData *data, struct RTEPos pos,
                          struct RTEPos *from, struct RTEPos *to) {
    struct RTEPara *p = &data->paras[pos.para];
    *from = *to = pos;
    if (data->selectUnit == 1) {
        BOOL word = pos.offset < p->len && rteIsWordChar(p->text[pos.offset]);
        if (!word && pos.offset > 0 &&
            rteIsWordChar(p->text[pos.offset - 1])) {
            pos.offset--;
            word = TRUE;
            *from = *to = pos;
        }
        if (word) {
            while (from->offset > 0 && rteIsWordChar(p->text[from->offset - 1]))
                from->offset--;
            while (to->offset < p->len && rteIsWordChar(p->text[to->offset]))
                to->offset++;
        } else if (pos.offset < p->len) {
            to->offset++;
        }
    } else if (data->selectUnit == 2) {
        from->offset = 0;
        to->offset = p->len;
        if (pos.para + 1 < data->numParas) {
            to->para++;
            to->offset = 0;
        }
    }
}

/** Extend the mouse selection to pos, snapping to the selection unit */
static void rteDragTo(struct RTEData *data, Object *obj, struct RTEPos pos) {
    struct RTEPos from, to, newAnchor, newCursor;
    if (data->selectUnit == 0) {
        newAnchor = data->clickPos;
        newCursor = pos;
    } else {
        rteSelectUnit(data, pos, &from, &to);
        if (rteComparePos(pos, data->selStartWord) < 0) {
            newAnchor = data->selEndWord;
            newCursor = from;
        } else {
            newAnchor = data->selStartWord;
            newCursor = to;
        }
    }
    if (rteComparePos(newAnchor, data->anchor) != 0 ||
        rteComparePos(newCursor, data->cursor) != 0) {
        rteInvalidatePosRange(data, data->anchor, data->cursor);
        rteInvalidatePosRange(data, newAnchor, newCursor);
        data->anchor = newAnchor;
        data->cursor = newCursor;
        data->marking = TRUE;
        rteAfterCursorMove(data, obj, TRUE);
    }
}

static BOOL rteActivateLink(struct RTEData *data, Object *obj,
                            struct RTEPos pos, UWORD qualifier) {
    struct RTEPara *p = &data->paras[pos.para];
    UWORD style;
    ULONG s, e, us, ue;
    struct RichTextLinkMessage msg;
    STRPTR url;
    BOOL image;

    if (pos.offset >= p->len)
        return FALSE;
    style = rteStyleAt(p, pos.offset);
    if (!(style & (RTE_S_LINK | RTE_S_IMAGE)))
        return FALSE;
    image = (style & RTE_S_IMAGE) != 0;
    if (!rteFindSpan(p, pos.offset, image, &s, &e, &us, &ue))
        return FALSE;
    url = AllocVec(ue - us + 1, MEMF_ANY);
    if (url == NULL)
        return FALSE;
    CopyMem(p->text + us, url, ue - us);
    url[ue - us] = '\0';
    if (data->clickedLink != NULL)
        FreeVec(data->clickedLink);
    data->clickedLink = url;
    msg.url = url;
    msg.text = NULL;
    msg.qualifier = qualifier;
    msg.image = image;
    if (data->linkHook != NULL)
        CallHookPkt(data->linkHook, obj, &msg);
    SetAttrs(obj, MUIA_RichTextEditor_ClickedLink, (IPTR)url, TAG_DONE);
    return TRUE;
}

static void rteMouseDown(struct RTEData *data, Object *obj,
                         struct IntuiMessage *imsg) {
    struct RTEPos pos = rtePosAtMouse(data, obj, imsg->MouseX, imsg->MouseY);
    BOOL shift = (imsg->Qualifier & QUAL_SHIFT) != 0;

    if (data->activeOnClick && _win(obj) != NULL && !data->active)
        set(_win(obj), MUIA_Window_ActiveObject, obj);

    if (DoubleClick(data->lastClickSecs, data->lastClickMicros, imsg->Seconds,
                    imsg->Micros) &&
        rteComparePos(pos, data->clickPos) == 0 && data->clickCount < 3)
        data->clickCount++;
    else
        data->clickCount = 1;
    data->lastClickSecs = imsg->Seconds;
    data->lastClickMicros = imsg->Micros;
    data->pressX = imsg->MouseX;
    data->pressY = imsg->MouseY;
    /* The auto scroll timer reads the mouse position, so it must never see
     * where an earlier drag ended */
    data->mouseX = imsg->MouseX;
    data->mouseY = imsg->MouseY;
    data->pressQualifier = imsg->Qualifier;
    data->dragMoved = FALSE;
    data->wantX = -1;

    if (data->clickCount == 1) {
        data->selectUnit = 0;
        if (shift) {
            if (!data->marking)
                data->anchor = data->cursor;
            data->clickPos = data->anchor;
            rteDragTo(data, obj, pos);
        } else {
            data->clickPos = pos;
            rteMoveTo(data, obj, pos, FALSE);
            data->anchor = pos;
            data->marking = TRUE;
        }
    } else {
        struct RTEPos from, to;
        data->selectUnit = data->clickCount == 2 ? 1 : 2;
        if (data->clickCount == 2 && data->doubleClickHook != NULL) {
            struct ClickMessage cm;
            struct RTEPara *p = &data->paras[pos.para];
            cm.LineContents = (STRPTR)p->text;
            cm.ClickPosition = pos.offset;
            cm.Qualifier = imsg->Qualifier;
            if (CallHookPkt(data->doubleClickHook, obj, &cm)) {
                data->selecting = FALSE;
                return;
            }
        }
        rteSelectUnit(data, pos, &from, &to);
        data->selStartWord = from;
        data->selEndWord = to;
        rteInvalidatePosRange(data, data->anchor, data->cursor);
        data->anchor = from;
        data->cursor = to;
        data->marking = TRUE;
        rteInvalidatePosRange(data, from, to);
        rteAfterCursorMove(data, obj, TRUE);
    }
    data->selecting = TRUE;
    rteUpdateEventHandler(data, obj,
                          data->ehEvents | IDCMP_MOUSEMOVE | IDCMP_INTUITICKS);
}

static void rteMouseDrag(struct RTEData *data, Object *obj, LONG mx, LONG my) {
    LONG dx = mx - data->pressX, dy = my - data->pressY;
    data->mouseX = mx;
    data->mouseY = my;
    if (!data->dragMoved && dx * dx + dy * dy < 9)
        return;
    data->dragMoved = TRUE;
    rteDragTo(data, obj, rtePosAtMouse(data, obj, mx, my));
}

/** Stop following the mouse */
void rteEndDrag(struct RTEData *data, Object *obj) {
    if (!data->selecting)
        return;
    data->selecting = FALSE;
    rteUpdateEventHandler(data, obj,
                          data->ehEvents & ~(IDCMP_MOUSEMOVE | IDCMP_INTUITICKS));
}

static void rteMouseUp(struct RTEData *data, Object *obj,
                       struct IntuiMessage *imsg) {
    rteEndDrag(data, obj);
    if (!data->dragMoved && data->clickCount == 1) {
        BOOL follow = data->readOnly ||
                      (data->pressQualifier & (QUAL_CTRL | QUAL_AMIGA)) != 0;
        if (follow) {
            /* a plain click on a link or image activates it */
            LONG docY = data->scrollY + (imsg->MouseY - data->viewTop);
            struct RTEPos pos = data->clickPos;
            ULONG pi = rteParaAtY(data, docY);
            struct RTEPara *p = &data->paras[pi];
            if (p->numLines > 0) {
                struct RTELine *l = &p->lines[rteLineAtY(p, docY)];
                LONG x = imsg->MouseX - data->viewLeft - RTE_PAD_X +
                         data->scrollX;
                LONG cx = rteOffsetToX(data, p, l, l->start);
                ULONG k;
                pos.para = pi;
                pos.offset = (ULONG)-1;
                for (k = l->start; k < l->start + l->len; k++) {
                    LONG w = rteCharWidth(data, p, k, cx);
                    if (x >= cx && x < cx + w) {
                        pos.offset = k;
                        break;
                    }
                    cx += w;
                }
                if (pos.offset != (ULONG)-1 &&
                    rteActivateLink(data, obj, pos, data->pressQualifier))
                    return;
            }
        }
        if (!rteHasSelection(data))
            data->marking = FALSE;
    }
    if (data->readOnly && data->autoClip && rteHasSelection(data))
        rteCopy(data, obj, FALSE);
}

ULONG rteHandleEvent(struct IClass *cl, Object *obj,
                     struct MUIP_HandleEvent *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct IntuiMessage *imsg = msg->imsg;

    if (imsg == NULL || !data->shown)
        return 0;
    if (!data->ehAdded)
        return 0;

    switch (imsg->Class) {
    case IDCMP_MOUSEBUTTONS:
        if (imsg->Code == SELECTDOWN) {
            if (rteInView(data, imsg->MouseX, imsg->MouseY) &&
                !data->disabled &&
                RTE_INOBJECT(obj, imsg->MouseX, imsg->MouseY)) {
                rteMouseDown(data, obj, imsg);
                data->justActivated = FALSE;
                return MUI_EventHandlerRC_Eat;
            }
        } else if (imsg->Code == SELECTUP && data->selecting) {
            rteMouseUp(data, obj, imsg);
            return MUI_EventHandlerRC_Eat;
        }
        break;
    case IDCMP_MOUSEMOVE:
        if (data->selecting) {
            /* The button went up where this window could not see it */
            if (!(imsg->Qualifier & IEQUALIFIER_LEFTBUTTON)) {
                rteEndDrag(data, obj);
                break;
            }
            rteMouseDrag(data, obj, imsg->MouseX, imsg->MouseY);
            return MUI_EventHandlerRC_Eat;
        }
        break;
    case IDCMP_INTUITICKS:
        if (data->selecting) {
            LONG my = data->mouseY;
            if (!(imsg->Qualifier & IEQUALIFIER_LEFTBUTTON)) {
                rteEndDrag(data, obj);
                break;
            }
            /* Only a real drag scrolls, never a click held still */
            if (!data->dragMoved)
                break;
            if (my < data->viewTop) {
                rteScrollBy(data, obj, -data->lineHeight);
                rteDragTo(data, obj, rtePosAtMouse(data, obj, data->mouseX, my));
            } else if (my >= data->viewTop + data->viewHeight) {
                rteScrollBy(data, obj, data->lineHeight);
                rteDragTo(data, obj, rtePosAtMouse(data, obj, data->mouseX, my));
            }
        }
        break;
#if defined(__AMIGAOS4__)
    case IDCMP_EXTENDEDMOUSE:
        if (imsg->Code == IMSGCODE_INTUIWHEELDATA &&
            rteInView(data, imsg->MouseX, imsg->MouseY)) {
            struct IntuiWheelData *wd = (struct IntuiWheelData *)imsg->IAddress;
            if (wd != NULL && wd->WheelY != 0) {
                rteScrollBy(data, obj, wd->WheelY * 3 * data->lineHeight);
                return MUI_EventHandlerRC_Eat;
            }
        }
        break;
#endif
    case IDCMP_RAWKEY:
        if (imsg->Code == RK_WHEEL_UP || imsg->Code == RK_WHEEL_DOWN) {
            if (rteInView(data, imsg->MouseX, imsg->MouseY)) {
                rteScrollBy(data, obj, (imsg->Code == RK_WHEEL_UP ? -3 : 3) *
                                           data->lineHeight);
                return MUI_EventHandlerRC_Eat;
            }
            break;
        }
        if (!data->active || (imsg->Code & IECODE_UP_PREFIX))
            break;
        {
            /* Keys belong to the window's active object. If that is not
             * this object any more, it has lost the focus without being
             * told, so stop showing a cursor and leave the key alone. */
            Object *current = NULL;
            if (_win(obj) != NULL)
                get(_win(obj), MUIA_Window_ActiveObject, &current);
            if (current != obj) {
                data->active = FALSE;
                rteInvalidateCursorLine(data);
                rteRequestRedraw(data, obj);
                break;
            }
        }
        if (data->justActivated) {
            /* MUI hands over the Tab that cycled to this object as well. That
             * Tab comes within moments of the activation; a Tab pressed later,
             * after a program put the cursor here, is typed as usual. */
            LONG ms = ((LONG)imsg->Seconds - (LONG)data->activatedSecs) * 1000 +
                      ((LONG)imsg->Micros - (LONG)data->activatedMicros) / 1000;
            data->justActivated = FALSE;
            if (imsg->Code == RK_TAB && ms > -250 && ms < 250)
                break;
        }
        if (data->disabled)
            break;
        if (rteKey(data, obj, imsg))
            return MUI_EventHandlerRC_Eat;
        break;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* ARexx                                                                     */
/* ------------------------------------------------------------------------ */

static BOOL rteWordIs(CONST_STRPTR s, CONST_STRPTR word) {
    while (*word != '\0') {
        UBYTE a = *s, b = *word;
        if (a >= 'a' && a <= 'z')
            a -= 32;
        if (a != b)
            return FALSE;
        s++;
        word++;
    }
    return *s == '\0' || *s == ' ' || *s == '\t' || *s == '=';
}

static CONST_STRPTR rteNextArg(CONST_STRPTR s) {
    while (*s != '\0' && *s != ' ' && *s != '\t')
        s++;
    while (*s == ' ' || *s == '\t' || *s == '=')
        s++;
    return s;
}

static BOOL rteArgHas(CONST_STRPTR args, CONST_STRPTR word) {
    CONST_STRPTR s = args;
    while (*s != '\0') {
        if (rteWordIs(s, word))
            return TRUE;
        s = rteNextArg(s);
    }
    return FALSE;
}

static LONG rteArgNumber(CONST_STRPTR args) {
    LONG n = 0, sign = 1;
    while (*args == ' ')
        args++;
    if (*args == '-') {
        sign = -1;
        args++;
    }
    while (*args >= '0' && *args <= '9')
        n = n * 10 + (*args++ - '0');
    return n * sign;
}

static STRPTR rteResultString(CONST_STRPTR s, ULONG len) {
    STRPTR r = AllocVec(len + 1, MEMF_ANY);
    if (r != NULL) {
        CopyMem((APTR)s, r, len);
        r[len] = '\0';
    }
    return r;
}

static void rteChangeCase(struct RTEData *data, Object *obj, BOOL upper) {
    struct RTEPos from, to;
    ULONG para;
    if (rteHasSelection(data)) {
        rteOrderedSelection(data, &from, &to);
    } else {
        from = data->cursor;
        to = rtePosRight(data, from, FALSE);
    }
    for (para = from.para; para <= to.para; para++) {
        struct RTEPara *p = &data->paras[para];
        ULONG s = para == from.para ? from.offset : 0;
        ULONG e = para == to.para ? to.offset : p->len;
        ULONG k;
        for (k = s; k < e; k++) {
            UBYTE c = p->text[k];
            if (upper && ((c >= 'a' && c <= 'z') ||
                          (c >= 0xe0 && c <= 0xfe && c != 0xf7)))
                p->text[k] = c - 32;
            else if (!upper && ((c >= 'A' && c <= 'Z') ||
                                (c >= 0xc0 && c <= 0xde && c != 0xd7)))
                p->text[k] = c + 32;
        }
        rteMarkDirty(data, para);
    }
    if (!rteHasSelection(data))
        data->cursor = to;
    rteFreeUndo(data, FALSE);
    rteAfterEdit(data, obj);
}

static BOOL rteIsSentenceEnd(UBYTE c) {
    return c == '.' || c == '!' || c == '?';
}

IPTR rteARexx(struct RTEData *data, Object *obj, CONST_STRPTR command) {
    CONST_STRPTR args;
    struct RTEPos pos;

    if (command == NULL)
        return FALSE;
    while (*command == ' ' || *command == '\t')
        command++;
    args = rteNextArg(command);

    if (rteWordIs(command, "COPY"))
        return rteCopy(data, obj, FALSE) ? TRUE : TRUE;
    if (rteWordIs(command, "SELECTALL")) {
        DoMethod(obj, MUIM_TextEditor_MarkText, 0, 0,
                 MUIV_TextEditor_MarkText_All, 0);
        return TRUE;
    }
    if (rteWordIs(command, "SELECTNONE")) {
        DoMethod(obj, MUIM_TextEditor_MarkText, 0,
                 MUIV_TextEditor_MarkText_None, 0, 0);
        return TRUE;
    }
    if (rteWordIs(command, "GETLINE")) {
        struct RTEPara *p = &data->paras[data->cursor.para];
        return (IPTR)rteResultString(p->text, p->len);
    }
    if (rteWordIs(command, "GETCURSOR")) {
        UBYTE buf[16];
        if (rteArgHas(args, "LINE"))
            snprintf(buf, sizeof(buf), "%lu",
                     (unsigned long)data->cursor.para + 1);
        else
            snprintf(buf, sizeof(buf), "%lu",
                     (unsigned long)data->cursor.offset + 1);
        return (IPTR)rteResultString(buf, strlen(buf));
    }

    /* Movement works in read only objects too (it moves the view) */
    if (rteWordIs(command, "GOTOLINE")) {
        LONG n = rteArgNumber(args) - 1;
        pos.para = n < 0 ? 0 : (ULONG)n;
        pos.offset = 0;
        rteMoveTo(data, obj, pos, FALSE);
        return TRUE;
    }
    if (rteWordIs(command, "GOTOCOLUMN")) {
        LONG n = rteArgNumber(args) - 1;
        pos = data->cursor;
        pos.offset = n < 0 ? 0 : (ULONG)n;
        rteMoveTo(data, obj, pos, FALSE);
        return TRUE;
    }
    if (rteWordIs(command, "LINE")) {
        LONG n = (LONG)data->cursor.para + rteArgNumber(args);
        pos = data->cursor;
        pos.para = n < 0 ? 0 : (ULONG)n;
        rteMoveTo(data, obj, pos, data->marking);
        return TRUE;
    }
    if (rteWordIs(command, "COLUMN")) {
        LONG n = (LONG)data->cursor.offset + rteArgNumber(args);
        pos = data->cursor;
        pos.offset = n < 0 ? 0 : (ULONG)n;
        rteMoveTo(data, obj, pos, data->marking);
        return TRUE;
    }
    if (rteWordIs(command, "CURSOR")) {
        pos = data->cursor;
        if (rteArgHas(args, "UP"))
            pos = rtePosVertical(data, obj, pos, -1, FALSE);
        else if (rteArgHas(args, "DOWN"))
            pos = rtePosVertical(data, obj, pos, 1, FALSE);
        else if (rteArgHas(args, "LEFT"))
            pos = rtePosLeft(data, pos, FALSE);
        else if (rteArgHas(args, "RIGHT"))
            pos = rtePosRight(data, pos, FALSE);
        rteMoveTo(data, obj, pos, data->marking);
        return TRUE;
    }
    if (rteWordIs(command, "NEXT") || rteWordIs(command, "PREVIOUS")) {
        BOOL next = rteWordIs(command, "NEXT");
        pos = data->cursor;
        if (rteArgHas(args, "PAGE")) {
            pos = rtePosVertical(data, obj, pos,
                                 (next ? 1 : -1) * rtePageLines(data), FALSE);
        } else if (rteArgHas(args, "PARAGRAPH")) {
            if (next) {
                if (pos.para + 1 < data->numParas)
                    pos.para++;
                pos.offset = 0;
            } else {
                if (pos.offset == 0 && pos.para > 0)
                    pos.para--;
                pos.offset = 0;
            }
        } else if (rteArgHas(args, "SENTENCE")) {
            struct RTEPara *p = &data->paras[pos.para];
            if (next) {
                while (pos.offset < p->len &&
                       !rteIsSentenceEnd(p->text[pos.offset]))
                    pos.offset++;
                while (pos.offset < p->len &&
                       (rteIsSentenceEnd(p->text[pos.offset]) ||
                        p->text[pos.offset] == ' '))
                    pos.offset++;
            } else {
                if (pos.offset > 0)
                    pos.offset--;
                while (pos.offset > 0 &&
                       (rteIsSentenceEnd(p->text[pos.offset]) ||
                        p->text[pos.offset] == ' '))
                    pos.offset--;
                while (pos.offset > 0 &&
                       !rteIsSentenceEnd(p->text[pos.offset - 1]))
                    pos.offset--;
                while (pos.offset < p->len && p->text[pos.offset] == ' ')
                    pos.offset++;
            }
        } else {
            pos = next ? rtePosRight(data, pos, TRUE) : rtePosLeft(data, pos, TRUE);
        }
        rteMoveTo(data, obj, pos, data->marking);
        return TRUE;
    }
    if (rteWordIs(command, "POSITION")) {
        pos = data->cursor;
        if (rteArgHas(args, "SOF")) {
            pos.para = 0;
            pos.offset = 0;
        } else if (rteArgHas(args, "EOF")) {
            pos.para = data->numParas - 1;
            pos.offset = data->paras[pos.para].len;
        } else if (rteArgHas(args, "SOL")) {
            pos = rteLineEdge(data, obj, pos, FALSE);
        } else if (rteArgHas(args, "EOL")) {
            pos = rteLineEdge(data, obj, pos, TRUE);
        } else if (rteArgHas(args, "SOW")) {
            struct RTEPara *p = &data->paras[pos.para];
            while (pos.offset > 0 && rteIsWordChar(p->text[pos.offset - 1]))
                pos.offset--;
        } else if (rteArgHas(args, "EOW")) {
            struct RTEPara *p = &data->paras[pos.para];
            while (pos.offset < p->len && rteIsWordChar(p->text[pos.offset]))
                pos.offset++;
        } else if (rteArgHas(args, "SOV")) {
            pos = rtePosAtMouse(data, obj, data->viewLeft,
                                data->viewTop + 1);
        } else if (rteArgHas(args, "EOV")) {
            pos = rtePosAtMouse(data, obj, data->viewLeft + data->viewWidth,
                                data->viewTop + data->viewHeight - 2);
        }
        rteMoveTo(data, obj, pos, data->marking);
        return TRUE;
    }
    if (rteWordIs(command, "SETBOOKMARK")) {
        LONG n = rteArgNumber(args);
        if (n >= 0 && n < 10) {
            data->bookmarks[n] = rtePosToIndex(data, data->cursor);
            data->bookmarkSet[n] = TRUE;
        }
        return TRUE;
    }
    if (rteWordIs(command, "GOTOBOOKMARK")) {
        LONG n = rteArgNumber(args);
        if (n >= 0 && n < 10 && data->bookmarkSet[n])
            rteMoveTo(data, obj, rteIndexToPos(data, data->bookmarks[n]),
                      data->marking);
        else
            rteHandleError(data, obj, Error_NoBookmarkInstalled);
        return TRUE;
    }
    if (rteWordIs(command, "MARK")) {
        if (rteArgHas(args, "OFF")) {
            if (rteHasSelection(data))
                rteMoveTo(data, obj, data->cursor, FALSE);
            data->marking = FALSE;
        } else {
            data->anchor = data->cursor;
            data->marking = TRUE;
        }
        return TRUE;
    }

    if (data->readOnly)
        return FALSE;

    if (rteWordIs(command, "CUT")) {
        rteCopy(data, obj, TRUE);
        return TRUE;
    }
    if (rteWordIs(command, "PASTE")) {
        rtePaste(data, obj);
        return TRUE;
    }
    if (rteWordIs(command, "CLEAR")) {
        DoMethod(obj, MUIM_TextEditor_ClearText);
        return TRUE;
    }
    if (rteWordIs(command, "ERASE")) {
        rteUndoBeginGroup(data);
        if (!rteDeleteSelection(data))
            rteHandleError(data, obj, Error_NoAreaMarked);
        rteAfterEdit(data, obj);
        return TRUE;
    }
    if (rteWordIs(command, "TEXT")) {
        rteUndoBeginGroup(data);
        rteInsertAtCursor(data, obj, args, strlen(args), TRUE);
        return TRUE;
    }
    if (rteWordIs(command, "UNDO")) {
        rteInvalidateRange(data, 0, 0x3fffffff);
        if (!rteUndo(data))
            rteHandleError(data, obj, Error_NothingToUndo);
        data->anchor = data->cursor;
        rteAfterEdit(data, obj);
        return TRUE;
    }
    if (rteWordIs(command, "REDO")) {
        rteInvalidateRange(data, 0, 0x3fffffff);
        if (!rteRedo(data))
            rteHandleError(data, obj, Error_NothingToRedo);
        data->anchor = data->cursor;
        rteAfterEdit(data, obj);
        return TRUE;
    }
    if (rteWordIs(command, "DELETE") || rteWordIs(command, "BACKSPACE")) {
        struct RTEPos from = data->cursor, to = data->cursor;
        rteUndoBeginGroup(data);
        if (!rteDeleteSelection(data)) {
            if (rteWordIs(command, "DELETE"))
                to = rtePosRight(data, to, FALSE);
            else
                from = rtePosLeft(data, from, FALSE);
            rteDeleteRange(data, from, to, TRUE);
            data->cursor = from;
        }
        data->anchor = data->cursor;
        rteAfterEdit(data, obj);
        return TRUE;
    }
    if (rteWordIs(command, "KILLLINE")) {
        struct RTEPos from = data->cursor, to = data->cursor;
        from.offset = 0;
        if (to.para + 1 < data->numParas) {
            to.para++;
            to.offset = 0;
        } else {
            to.offset = data->paras[to.para].len;
            if (from.para > 0) {
                from.para--;
                from.offset = data->paras[from.para].len;
            }
        }
        rteUndoBeginGroup(data);
        rteDeleteRange(data, from, to, TRUE);
        data->cursor = from;
        data->anchor = from;
        data->marking = FALSE;
        rteAfterEdit(data, obj);
        return TRUE;
    }
    if (rteWordIs(command, "TOUPPER") || rteWordIs(command, "TOLOWER")) {
        rteChangeCase(data, obj, rteWordIs(command, "TOUPPER"));
        return TRUE;
    }
    return FALSE;
}

/** Replay a recorded keyboard macro */
void rteMacroExecute(struct RTEData *data, Object *obj) {
    UWORD i;
    struct IntuiMessage fake;
    if (data->macroRecording || data->macroLen == 0) {
        rteHandleError(data, obj, Error_NoMacroDefined);
        return;
    }
    memset(&fake, 0, sizeof(fake));
    fake.Class = IDCMP_RAWKEY;
    data->macroPlaying = TRUE;
    for (i = 0; i < data->macroLen; i++) {
        APTR prev = NULL;
        fake.Code = data->macroCodes[i];
        fake.Qualifier = data->macroQuals[i];
        fake.IAddress = &prev;
        rteKey(data, obj, &fake);
    }
    data->macroPlaying = FALSE;
}

#endif /* DAEMON */
