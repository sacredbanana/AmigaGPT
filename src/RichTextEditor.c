#include <libraries/mui.h>

/* Defined in the daemon too: it links the settings windows that create
 * these objects, but never builds those windows, so the class itself is only
 * compiled into the GUI */
struct MUI_CustomClass *richTextEditorClass = NULL;

#ifndef DAEMON
/*
 * RichTextEditor - the MUI class
 *
 * Dispatcher, attributes, the TextEditor.mcc compatible methods and the
 * scroll and redraw bookkeeping that ties the other modules together.
 */
#include <clib/alib_protos.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <graphics/gfxmacros.h>
#include <intuition/intuition.h>
#include <libraries/mui.h>
#include <mui/ActionTextEditor_mcc.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <SDI_hook.h>
#include <string.h>
#include "RichTextEditorPrivate.h"

/* Private: horizontal slider position, set by the slider notification */
#define RTE_PRIVATE_HFIRST (RichTextEditor_Dummy + 0x70)

void rteMacroExecute(struct RTEData *data, Object *obj);
static void rteRestoreCycleKeys(struct IClass *cl, struct RTEData *data,
                                Object *obj);

static BOOL rteNotifying = FALSE;

/* ------------------------------------------------------------------------ */
/* Redraw and scrolling                                                      */
/* ------------------------------------------------------------------------ */

void rteInvalidateRange(struct RTEData *data, LONG top, LONG bottom) {
    if (bottom <= top)
        return;
    if (top < data->dirtyTop)
        data->dirtyTop = top;
    if (bottom > data->dirtyBottom)
        data->dirtyBottom = bottom;
}

void rteInvalidatePara(struct RTEData *data, ULONG para) {
    struct RTEPara *p;
    if (para >= data->numParas)
        return;
    p = &data->paras[para];
    rteInvalidateRange(data, p->y, p->y + RTE_MAX(p->height, 1));
}

void rteInvalidateCursorLine(struct RTEData *data) {
    struct RTEPara *p;
    if (data->cursor.para >= data->numParas)
        return;
    p = &data->paras[data->cursor.para];
    if ((p->flags & RTEP_DIRTY) || p->numLines == 0) {
        rteInvalidatePara(data, data->cursor.para);
    } else {
        struct RTELine *l = &p->lines[rteLineOfOffset(p, data->cursor.offset)];
        LONG top = p->y + p->padTop + l->y;
        rteInvalidateRange(data, top, top + l->height);
    }
}

BOOL rteViewAtBottom(struct RTEData *data) {
    if (data->viewHeight <= 0)
        return TRUE;
    return data->scrollY + data->viewHeight >= data->totalHeight - 2;
}

static void rteUpdateGeometry(struct RTEData *data, Object *obj) {
    if (!data->shown)
        return;
    data->viewLeft = _mleft(obj);
    data->viewTop = _mtop(obj);
    data->viewWidth = _mwidth(obj);
    data->viewHeight = _mheight(obj);
}

static void rteClampScroll(struct RTEData *data) {
    LONG max = data->totalHeight - data->viewHeight;
    LONG maxX;
    if (max < 0)
        max = 0;
    if (data->scrollY > max) {
        data->scrollY = max;
        data->drawAll = TRUE;
    }
    if (data->scrollY < 0)
        data->scrollY = 0;
    maxX = data->maxLineWidth + 2 * RTE_PAD_X + RTE_CODE_PAD * 2 -
           data->viewWidth;
    if (data->wrapMode != MUIV_TextEditor_WrapMode_NoWrap || maxX < 0)
        maxX = 0;
    if (data->scrollX > maxX) {
        data->scrollX = maxX;
        data->drawAll = TRUE;
    }
    if (data->scrollX < 0)
        data->scrollX = 0;
}

static void rteNotifyAttr(Object *obj, ULONG attr, IPTR value) {
    rteNotifying = TRUE;
    SetAttrs(obj, attr, value, TAG_DONE);
    rteNotifying = FALSE;
}

static void rteUpdateSliders(struct RTEData *data, Object *obj) {
    LONG entries = RTE_MAX(data->totalHeight, data->viewHeight);
    LONG visible = data->viewHeight;
    LONG first = data->scrollY;

    if (entries != data->nPropEntries || visible != data->nPropVisible ||
        first != data->nPropFirst) {
        if (data->slider != NULL) {
            SetAttrs(data->slider, MUIA_NoNotify, TRUE, MUIA_Prop_Entries,
                     entries, MUIA_Prop_Visible, visible, MUIA_Prop_First,
                     first, MUIA_Prop_DeltaFactor,
                     data->lineHeight > 0 ? data->lineHeight : 1, TAG_DONE);
        }
        if (entries != data->nPropEntries)
            rteNotifyAttr(obj, MUIA_TextEditor_Prop_Entries, entries);
        if (visible != data->nPropVisible)
            rteNotifyAttr(obj, MUIA_TextEditor_Prop_Visible, visible);
        if (first != data->nPropFirst)
            rteNotifyAttr(obj, MUIA_TextEditor_Prop_First, first);
        data->nPropEntries = entries;
        data->nPropVisible = visible;
        data->nPropFirst = first;
    }
    if (data->hslider != NULL) {
        LONG hEntries = RTE_MAX(data->maxLineWidth + 2 * RTE_PAD_X +
                                    2 * RTE_CODE_PAD,
                                data->viewWidth);
        if (hEntries != data->nHPropEntries ||
            data->viewWidth != data->nHPropVisible ||
            data->scrollX != data->nHPropFirst) {
            SetAttrs(data->hslider, MUIA_NoNotify, TRUE, MUIA_Prop_Entries,
                     hEntries, MUIA_Prop_Visible, data->viewWidth,
                     MUIA_Prop_First, data->scrollX, MUIA_Prop_DeltaFactor,
                     RTE_MAX(data->spaceWidth, 1), TAG_DONE);
            data->nHPropEntries = hEntries;
            data->nHPropVisible = data->viewWidth;
            data->nHPropFirst = data->scrollX;
        }
    }
}

void rteEnsureLayout(struct RTEData *data, Object *obj) {
    if (!data->setup)
        return;
    rteUpdateGeometry(data, obj);
    if (data->viewWidth <= 0)
        return;
    rteLayout(data);
    rteClampScroll(data);
}

void rteRequestRedraw(struct RTEData *data, Object *obj) {
    if (data->quiet || !data->shown || data->inDraw)
        return;
    rteEnsureLayout(data, obj);
    rteUpdateSliders(data, obj);
    if (data->drawAll || data->scrollDelta != 0 ||
        data->dirtyBottom > data->dirtyTop)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
}

void rteScrollTo(struct RTEData *data, Object *obj, LONG y) {
    LONG old = data->scrollY;
    if (!data->shown || data->quiet) {
        /* Nothing is laid out yet, so remember it for MUIM_Show */
        data->pendingScroll = y;
        data->hasPendingScroll = TRUE;
        return;
    }
    rteEnsureLayout(data, obj);
    data->scrollY = y;
    rteClampScroll(data);
    if (data->scrollY != old) {
        data->scrollDelta += data->scrollY - old;
        rteRequestRedraw(data, obj);
    }
}

static void rteScrollXTo(struct RTEData *data, Object *obj, LONG x) {
    LONG old = data->scrollX;
    data->scrollX = x;
    rteClampScroll(data);
    if (data->scrollX != old) {
        data->drawAll = TRUE;
        rteRequestRedraw(data, obj);
    }
}

void rteMakeCursorVisible(struct RTEData *data, Object *obj) {
    struct RTEPara *p;
    struct RTELine *l;
    LONG top, bottom;
    ULONG li;
    if (!data->shown || data->quiet || data->readOnly)
        return;
    rteEnsureLayout(data, obj);
    p = &data->paras[data->cursor.para];
    if (p->numLines == 0)
        return;
    li = rteLineOfOffset(p, data->cursor.offset);
    l = &p->lines[li];
    top = p->y + p->padTop + l->y;
    bottom = top + l->height;
    /* show the decoration around the first and last lines too */
    if (li == 0)
        top -= p->padTop;
    if (li + 1 == p->numLines)
        bottom += p->padBottom;
    if (data->cursor.para + 1 == data->numParas && li + 1 == p->numLines)
        bottom += RTE_PAD_Y;
    if (data->wrapMode == MUIV_TextEditor_WrapMode_NoWrap) {
        LONG x = rteOffsetToX(data, p, l, data->cursor.offset);
        LONG w = data->viewWidth - 2 * RTE_PAD_X;
        if (x < data->scrollX)
            rteScrollXTo(data, obj, x - w / 4);
        else if (x >= data->scrollX + w - 2)
            rteScrollXTo(data, obj, x - w + w / 4);
    }
    if (top < data->scrollY)
        rteScrollTo(data, obj, top);
    else if (bottom > data->scrollY + data->viewHeight)
        rteScrollTo(data, obj, bottom - data->viewHeight);
}

static void rteInvalidateSelection(struct RTEData *data) {
    struct RTEPos from, to;
    if (!rteHasSelection(data))
        return;
    rteOrderedSelection(data, &from, &to);
    rteInvalidateRange(data, data->paras[from.para].y,
                       data->paras[to.para].y + data->paras[to.para].height);
}

/* ------------------------------------------------------------------------ */
/* State notifications                                                       */
/* ------------------------------------------------------------------------ */

void rteHandleError(struct RTEData *data, Object *obj, ULONG error) {
    (void)data;
    if (obj != NULL && error != 0)
        DoMethod(obj, MUIM_TextEditor_HandleError, error);
}

void rteNotifyState(struct RTEData *data, Object *obj) {
    BOOL marked = rteHasSelection(data);
    BOOL undo = data->undoList.mlh_Head->mln_Succ != NULL;
    BOOL redo = data->redoList.mlh_Head->mln_Succ != NULL;
    struct RTEPara *p = &data->paras[data->cursor.para];
    UWORD attr = rteAttrAt(p, data->cursor.offset > 0 ? data->cursor.offset - 1
                                                      : data->cursor.offset);
    UBYTE style = (UBYTE)(attr & RTE_A_STYLEMASK);
    UBYTE pen = (UBYTE)RTE_A_COLOR(attr);

    if (data->quiet)
        return;
    if (!marked) {
        style = data->typeStyle;
        if (data->totalLength > 0 && data->cursor.offset > 0)
            style = (UBYTE)(attr & RTE_A_STYLEMASK);
    }
    if (marked != data->nAreaMarked) {
        data->nAreaMarked = marked;
        rteNotifyAttr(obj, MUIA_TextEditor_AreaMarked, marked);
    }
    if (undo != data->nUndo) {
        data->nUndo = undo;
        rteNotifyAttr(obj, MUIA_TextEditor_UndoAvailable, undo);
    }
    if (redo != data->nRedo) {
        data->nRedo = redo;
        rteNotifyAttr(obj, MUIA_TextEditor_RedoAvailable, redo);
    }
    if ((LONG)data->cursor.offset != data->nCursorX) {
        data->nCursorX = data->cursor.offset;
        rteNotifyAttr(obj, MUIA_TextEditor_CursorX, data->cursor.offset);
    }
    if ((LONG)data->cursor.para != data->nCursorY) {
        data->nCursorY = data->cursor.para;
        rteNotifyAttr(obj, MUIA_TextEditor_CursorY, data->cursor.para);
    }
    if (style != data->nStyle) {
        UBYTE changed = style ^ data->nStyle;
        data->nStyle = style;
        if (changed & RTE_A_BOLD)
            rteNotifyAttr(obj, MUIA_TextEditor_StyleBold,
                          (style & RTE_A_BOLD) != 0);
        if (changed & RTE_A_ITALIC)
            rteNotifyAttr(obj, MUIA_TextEditor_StyleItalic,
                          (style & RTE_A_ITALIC) != 0);
        if (changed & RTE_A_UNDERLINE)
            rteNotifyAttr(obj, MUIA_TextEditor_StyleUnderline,
                          (style & RTE_A_UNDERLINE) != 0);
    }
    if (pen != data->nPen) {
        data->nPen = pen;
        rteNotifyAttr(obj, MUIA_TextEditor_Pen,
                      data->colors[pen].kind == RTE_COLOR_CMAP
                          ? data->colors[pen].value
                          : 0);
    }
    if (p->flow != data->nFlow) {
        data->nFlow = p->flow;
        rteNotifyAttr(obj, MUIA_TextEditor_Flow, p->flow);
    }
}

void rteChanged(struct RTEData *data, Object *obj, BOOL contents) {
    if (!data->hasChanged) {
        data->hasChanged = TRUE;
        rteNotifyAttr(obj, MUIA_TextEditor_HasChanged, TRUE);
    }
    if (contents && !data->contentsChanged) {
        data->contentsChanged = TRUE;
        rteNotifyAttr(obj, MUIA_TextEditor_ContentsChanged, TRUE);
    }
    if (!contents && !data->metaChanged) {
        data->metaChanged = TRUE;
        rteNotifyAttr(obj, MUIA_TextEditor_MetaDataChanged, TRUE);
    }
}

void rteAfterEdit(struct RTEData *data, Object *obj) {
    rteClampPos(data, &data->cursor);
    rteClampPos(data, &data->anchor);
    rteChanged(data, obj, TRUE);
    rteMakeCursorVisible(data, obj);
    rteInvalidateCursorLine(data);
    rteRequestRedraw(data, obj);
    rteNotifyState(data, obj);
}

void rteAfterCursorMove(struct RTEData *data, Object *obj, BOOL extend) {
    (void)extend;
    data->undoMergeOK = FALSE;
    rteMakeCursorVisible(data, obj);
    rteRequestRedraw(data, obj);
    rteNotifyState(data, obj);
}

/** Everything changed: relayout and redraw it all */
static void rteRefreshAll(struct RTEData *data, Object *obj) {
    rteMarkAllDirty(data);
    data->drawAll = TRUE;
    rteRequestRedraw(data, obj);
    rteNotifyState(data, obj);
}

/* ------------------------------------------------------------------------ */
/* Event handler                                                             */
/* ------------------------------------------------------------------------ */

void rteUpdateEventHandler(struct RTEData *data, Object *obj, ULONG events) {
    Object *win = _win(obj);
    if (data->ehAdded && win != NULL)
        DoMethod(win, MUIM_Window_RemEventHandler, &data->eh);
    data->ehEvents = events;
    data->eh.ehn_Events = events;
    if (data->ehAdded && win != NULL)
        DoMethod(win, MUIM_Window_AddEventHandler, &data->eh);
}

/* ------------------------------------------------------------------------ */
/* Attributes                                                                */
/* ------------------------------------------------------------------------ */

static STRPTR rteDupString(CONST_STRPTR s) {
    STRPTR copy;
    if (s == NULL)
        return NULL;
    copy = AllocVec(strlen(s) + 1, MEMF_ANY);
    if (copy != NULL)
        strcpy(copy, s);
    return copy;
}

static void rteSetContents(struct RTEData *data, Object *obj,
                           CONST_STRPTR text) {
    struct RTEPos start = {0, 0};
    rteClear(data);
    rteFreeUndo(data, FALSE);
    data->nextBubbleId = 1;
    if (text != NULL && text[0] != '\0')
        rteImport(data, start, text, -1, FALSE, data->importHook, NULL, FALSE);
    data->cursor.para = data->cursor.offset = 0;
    data->anchor = data->cursor;
    data->marking = FALSE;
    if (obj != NULL)
        rteRefreshAll(data, obj);
}

static void rteSetSlider(struct RTEData *data, Object *obj, Object *slider,
                         BOOL horizontal) {
    Object **slot = horizontal ? &data->hslider : &data->slider;
    if (*slot == slider)
        return;
    if (*slot != NULL)
        DoMethod(*slot, MUIM_KillNotifyObj, MUIA_Prop_First, obj);
    *slot = slider;
    if (slider != NULL) {
        DoMethod(slider, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, obj, 3,
                 MUIM_NoNotifySet,
                 horizontal ? RTE_PRIVATE_HFIRST : MUIA_TextEditor_Prop_First,
                 MUIV_TriggerValue);
    }
    data->nPropEntries = data->nPropVisible = data->nPropFirst = -1;
    data->nHPropEntries = data->nHPropVisible = data->nHPropFirst = -1;
}

/** Apply a style or colour to the selection, or to what is typed next */
static void rteApplyStyle(struct RTEData *data, Object *obj, ULONG op,
                          ULONG value) {
    if (rteHasSelection(data)) {
        struct RTEPos from, to;
        rteOrderedSelection(data, &from, &to);
        if (op == MUIF_TextEditor_SetBlock_Color) {
            UBYTE color = (UBYTE)rteColorIndex(data, RTE_COLOR_CMAP, value);
            ULONG para;
            for (para = from.para; para <= to.para; para++) {
                struct RTEPara *p = &data->paras[para];
                ULONG s = para == from.para ? from.offset : 0;
                ULONG e = para == to.para ? to.offset : p->len;
                ULONG i;
                if (e <= s || !rteParaEnsureAttrs(data, p))
                    continue;
                for (i = s; i < e; i++)
                    p->attrs[i] = RTE_A_MAKE(p->attrs[i] & 0xff, color);
                rteMarkDirty(data, para);
            }
        } else {
            rteSetBlockAttrs(data, from, to, op, value);
        }
        rteChanged(data, obj, FALSE);
        rteRequestRedraw(data, obj);
    } else {
        if (op == MUIF_TextEditor_SetBlock_StyleBold)
            data->typeStyle = value ? data->typeStyle | RTE_A_BOLD
                                    : data->typeStyle & ~RTE_A_BOLD;
        else if (op == MUIF_TextEditor_SetBlock_StyleItalic)
            data->typeStyle = value ? data->typeStyle | RTE_A_ITALIC
                                    : data->typeStyle & ~RTE_A_ITALIC;
        else if (op == MUIF_TextEditor_SetBlock_StyleUnderline)
            data->typeStyle = value ? data->typeStyle | RTE_A_UNDERLINE
                                    : data->typeStyle & ~RTE_A_UNDERLINE;
        else if (op == MUIF_TextEditor_SetBlock_Color)
            data->typePen = (UBYTE)rteColorIndex(data, RTE_COLOR_CMAP, value);
    }
}

static void rteSetFlow(struct RTEData *data, Object *obj, UBYTE flow) {
    if (data->globalFlow) {
        data->globalFlowValue = flow;
        rteMarkAllDirty(data);
        data->drawAll = TRUE;
    } else {
        struct RTEPos from = data->cursor, to = data->cursor;
        ULONG para;
        if (rteHasSelection(data))
            rteOrderedSelection(data, &from, &to);
        for (para = from.para; para <= to.para; para++) {
            data->paras[para].flow = flow;
            rteInvalidatePara(data, para);
        }
    }
    rteChanged(data, obj, FALSE);
    rteRequestRedraw(data, obj);
}

static void rteSetAttrs(struct IClass *cl, Object *obj,
                        struct TagItem *taglist, BOOL init) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct TagItem *tags = taglist, *tag;
    BOOL relayout = FALSE;
    CONST_STRPTR contents = NULL;
    BOOL setContents = FALSE;

    while ((tag = NextTagItem(&tags)) != NULL) {
        IPTR v = tag->ti_Data;
        switch (tag->ti_Tag) {
        case MUIA_TextEditor_ActiveObjectOnClick:
            data->activeOnClick = v != 0;
            data->activeOnClickSet = TRUE;
            break;
        case MUIA_TextEditor_AutoClip:
            data->autoClip = v != 0;
            break;
        case MUIA_TextEditor_ColorMap:
            data->colorMap = (LONG *)v;
            if (!init) {
                UWORD i;
                for (i = 1; i < data->numColors; i++) {
                    if (data->colors[i].kind == RTE_COLOR_CMAP &&
                        !data->colors[i].allocated)
                        data->colors[i].pen = -1;
                }
                data->drawAll = TRUE;
            }
            break;
        case MUIA_TextEditor_Columns:
            data->columns = v;
            break;
        case MUIA_TextEditor_Contents:
            contents = (CONST_STRPTR)v;
            setContents = TRUE;
            break;
        case MUIA_TextEditor_ContentsChanged:
            if (!rteNotifying)
                data->contentsChanged = v != 0;
            break;
        case MUIA_TextEditor_ConvertTabs:
            data->convertTabs = v != 0;
            break;
        case MUIA_TextEditor_CursorIndex:
            if (!rteNotifying) {
                data->cursor = rteIndexToPos(data, v);
                data->anchor = data->cursor;
                data->marking = FALSE;
                if (!init) {
                    data->drawAll = TRUE;
                    rteAfterCursorMove(data, obj, FALSE);
                }
            }
            break;
        case MUIA_TextEditor_CursorX:
        case MUIA_TextEditor_CursorY:
            if (!rteNotifying) {
                if (tag->ti_Tag == MUIA_TextEditor_CursorX)
                    data->cursor.offset = v;
                else
                    data->cursor.para = v;
                rteClampPos(data, &data->cursor);
                data->anchor = data->cursor;
                data->marking = FALSE;
                if (!init) {
                    data->drawAll = TRUE;
                    rteAfterCursorMove(data, obj, FALSE);
                }
            }
            break;
        case MUIA_TextEditor_DoubleClickHook:
            data->doubleClickHook = (struct Hook *)v;
            break;
        case MUIA_TextEditor_ExportHook:
            data->exportHook = v;
            break;
        case MUIA_TextEditor_ExportWrap:
            data->exportWrap = v;
            break;
        case MUIA_TextEditor_FixedFont:
            if ((v != 0) != data->fixedFont) {
                data->fixedFont = v != 0;
                if (!init && data->setup) {
                    rteSetupFonts(data, obj);
                    relayout = TRUE;
                }
            }
            break;
        case MUIA_TextEditor_Flow:
            if (!rteNotifying && !init)
                rteSetFlow(data, obj, (UBYTE)v);
            break;
        case MUIA_TextEditor_GlobalFlow:
            data->globalFlow = v != 0;
            relayout = TRUE;
            break;
        case MUIA_TextEditor_HasChanged:
            if (!rteNotifying) {
                data->hasChanged = v != 0;
                data->contentsChanged = v != 0;
                data->metaChanged = v != 0;
            }
            break;
        case MUIA_TextEditor_HorizontalSlider:
            rteSetSlider(data, obj, (Object *)v, TRUE);
            break;
        case MUIA_TextEditor_ImportHook:
            data->importHook = v;
            break;
        case MUIA_TextEditor_ImportWrap:
            data->importWrap = v < 4 ? 4 : (v > 1024 ? 1024 : v);
            break;
        case MUIA_TextEditor_InactiveContents:
            if (data->inactiveContents != NULL)
                FreeVec(data->inactiveContents);
            data->inactiveContents = rteDupString((CONST_STRPTR)v);
            data->drawAll = TRUE;
            break;
        case MUIA_TextEditor_InsertMode:
            data->overwrite = v == 0;
            break;
        case MUIA_TextEditor_KeyUpFocus:
            data->keyUpFocus = (Object *)v;
            break;
        case MUIA_TextEditor_Keywords:
            if (data->keywords != NULL)
                FreeVec(data->keywords);
            data->keywords = rteDupString((CONST_STRPTR)v);
            break;
        case MUIA_TextEditor_MetaDataChanged:
            if (!rteNotifying)
                data->metaChanged = v != 0;
            break;
        case MUIA_TextEditor_PasteColors:
            data->pasteColors = v != 0;
            break;
        case MUIA_TextEditor_PasteStyles:
            data->pasteStyles = v != 0;
            break;
        case MUIA_TextEditor_Pen:
            if (!rteNotifying && !init)
                rteApplyStyle(data, obj, MUIF_TextEditor_SetBlock_Color, v);
            break;
        case MUIA_TextEditor_Prop_First:
            if (!rteNotifying && !init)
                rteScrollTo(data, obj, (LONG)v);
            break;
        case RTE_PRIVATE_HFIRST:
            rteScrollXTo(data, obj, (LONG)v);
            break;
        case MUIA_TextEditor_Quiet:
            data->quiet = v != 0;
            if (!data->quiet && !init) {
                data->drawAll = TRUE;
                if (data->hasPendingScroll && data->shown) {
                    data->hasPendingScroll = FALSE;
                    rteEnsureLayout(data, obj);
                    data->scrollY = data->pendingScroll;
                    rteClampScroll(data);
                }
                rteRequestRedraw(data, obj);
                rteNotifyState(data, obj);
            }
            break;
        case MUIA_TextEditor_ReadOnly:
            data->readOnly = v != 0;
            if (!data->activeOnClickSet || !init)
                data->activeOnClick = !data->readOnly;
            if (!data->hideMarkupSet) {
                if (data->hideMarkup != data->readOnly)
                    relayout = TRUE;
                data->hideMarkup = data->readOnly;
            }
            data->drawAll = TRUE;
            break;
        case MUIA_TextEditor_RGBMode:
            data->rgbMode = v != 0;
            break;
        case MUIA_TextEditor_Rows:
            data->rows = v;
            break;
        case MUIA_TextEditor_Separator:
            if (!init) {
                data->paras[data->cursor.para].separator = (UBYTE)v;
                rteMarkDirty(data, data->cursor.para);
                rteChanged(data, obj, FALSE);
                rteRequestRedraw(data, obj);
            }
            break;
        case MUIA_TextEditor_Slider:
            rteSetSlider(data, obj, (Object *)v, FALSE);
            break;
        case MUIA_TextEditor_StyleBold:
            if (!rteNotifying && !init)
                rteApplyStyle(data, obj, MUIF_TextEditor_SetBlock_StyleBold, v);
            break;
        case MUIA_TextEditor_StyleItalic:
            if (!rteNotifying && !init)
                rteApplyStyle(data, obj, MUIF_TextEditor_SetBlock_StyleItalic,
                              v);
            break;
        case MUIA_TextEditor_StyleUnderline:
            if (!rteNotifying && !init)
                rteApplyStyle(data, obj,
                              MUIF_TextEditor_SetBlock_StyleUnderline, v);
            break;
        case MUIA_TextEditor_TabSize:
            data->tabSize = v == MUIV_TextEditor_TabSize_Default
                                ? RTE_TAB_DEFAULT
                                : (v < 2 ? 2 : (v > 12 ? 12 : v));
            data->tabSizeSet = TRUE;
            relayout = TRUE;
            break;
        case MUIA_TextEditor_TypeAndSpell:
            data->typeAndSpell = v != 0;
            break;
        case MUIA_TextEditor_UndoLevels:
            data->undoLevels = v;
            if (v == 0)
                rteFreeUndo(data, FALSE);
            break;
        case MUIA_TextEditor_WrapBorder:
            data->wrapBorder = v;
            relayout = TRUE;
            break;
        case MUIA_TextEditor_WrapMode:
            data->wrapMode = v;
            relayout = TRUE;
            break;
        case MUIA_TextEditor_WrapWords:
            data->wrapWords = v != 0;
            relayout = TRUE;
            break;

        case MUIA_RichTextEditor_SubmitHook:
        case MUIA_ActionTextEditor_SubmitHook:
            data->submitHook = (struct Hook *)v;
            break;
        case MUIA_RichTextEditor_SubmitEmpty:
            data->submitEmpty = v != 0;
            break;
        case MUIA_RichTextEditor_Markdown: {
            ULONG i;
            data->markdown = v != 0;
            for (i = 0; i < data->numParas; i++) {
                if (data->markdown)
                    data->paras[i].flags |= RTEP_MARKDOWN;
                else
                    data->paras[i].flags &= ~RTEP_MARKDOWN;
            }
            relayout = TRUE;
            break;
        }
        case MUIA_RichTextEditor_HideMarkup:
            data->hideMarkup = v != 0;
            data->hideMarkupSet = TRUE;
            relayout = TRUE;
            break;
        case MUIA_RichTextEditor_LinkHook:
            data->linkHook = (struct Hook *)v;
            break;
        case MUIA_RichTextEditor_StickToBottom:
            data->stickToBottom = v != 0;
            break;
        case MUIA_RichTextEditor_MaxLength:
            data->maxLength = v;
            if (!init) {
                rteTrimToMaxLength(data);
                relayout = TRUE;
            }
            break;
        case MUIA_RichTextEditor_BubbleStyles: {
            const struct RichTextBubbleStyle *bs =
                (const struct RichTextBubbleStyle *)v;
            UWORD n = 0;
            BOOL wasSetup = data->setup;
            if (wasSetup)
                rteReleasePens(data, obj);
            if (bs == NULL) {
                rteSetDefaultBubbleStyles(data);
            } else {
                while (n < RICHTEXTEDITOR_MAX_BUBBLE_STYLES &&
                       bs[n].side != MUIV_RichTextEditor_Bubble_End) {
                    data->bubbleStyles[n] = bs[n];
                    n++;
                }
                if (n > 0)
                    data->numBubbleStyles = n;
            }
            if (wasSetup)
                rteObtainPens(data, obj);
            relayout = TRUE;
            break;
        }
        case MUIA_RichTextEditor_ImageHook:
            data->imageHook = (struct Hook *)v;
            break;
        case MUIA_RichTextEditor_MaxImageHeight:
            data->maxImageHeight = v;
            relayout = TRUE;
            break;
        case MUIA_RichTextEditor_CodeFont:
            data->userCodeFont = (struct TextFont *)v;
            break;
        case MUIA_Disabled:
            data->disabled = v != 0;
            data->drawAll = TRUE;
            break;
        }
    }

    if (setContents)
        rteSetContents(data, init ? NULL : obj, contents);
    if (relayout && !init) {
        rteMarkAllDirty(data);
        data->drawAll = TRUE;
        rteRequestRedraw(data, obj);
    }
}

/* ------------------------------------------------------------------------ */
/* Methods                                                                   */
/* ------------------------------------------------------------------------ */

static IPTR mNew(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct RTEData *data;
    struct TagItem tags[6];
    struct opSet superMsg;
    UWORD n = 0;
    BOOL readOnly =
        GetTagData(MUIA_TextEditor_ReadOnly, FALSE, msg->ops_AttrList) != 0;
    BOOL fixed =
        GetTagData(MUIA_TextEditor_FixedFont, FALSE, msg->ops_AttrList) != 0;

    tags[n].ti_Tag = MUIA_FillArea;
    tags[n++].ti_Data = FALSE;
    if (FindTagItem(MUIA_Frame, msg->ops_AttrList) == NULL) {
        tags[n].ti_Tag = MUIA_Frame;
        tags[n++].ti_Data = readOnly ? MUIV_Frame_ReadList : MUIV_Frame_String;
    }
    if (FindTagItem(MUIA_Background, msg->ops_AttrList) == NULL) {
        tags[n].ti_Tag = MUIA_Background;
        tags[n++].ti_Data = readOnly ? MUII_ReadListBack : MUII_TextBack;
    }
    if (fixed && FindTagItem(MUIA_Font, msg->ops_AttrList) == NULL) {
        tags[n].ti_Tag = MUIA_Font;
        tags[n++].ti_Data = (ULONG)MUIV_Font_Fixed;
    }
    tags[n].ti_Tag = TAG_MORE;
    tags[n].ti_Data = (ULONG)msg->ops_AttrList;
    superMsg.MethodID = OM_NEW;
    superMsg.ops_AttrList = tags;
    superMsg.ops_GInfo = msg->ops_GInfo;

    obj = (Object *)DoSuperMethodA(cl, obj, (Msg)&superMsg);
    if (obj == NULL)
        return 0;
    data = INST_DATA(cl, obj);
    memset(data, 0, sizeof(*data));
    data->obj = obj;
    data->autoClip = TRUE;
    data->convertTabs = TRUE;
    data->wrapWords = TRUE;
    data->pasteStyles = TRUE;
    data->pasteColors = TRUE;
    data->stickToBottom = TRUE;
    data->tabSize = RTE_TAB_DEFAULT;
    data->wrapMode = MUIV_TextEditor_WrapMode_HardWrap;
    data->importWrap = 1023;
    data->undoLevels = 500;
    data->wantX = -1;
    data->activeOnClick = TRUE;
    data->dirtyTop = 0x3fffffff;
    data->dirtyBottom = -1;
    data->nCursorX = data->nCursorY = -1;
    data->nPropEntries = data->nPropVisible = data->nPropFirst = -1;
    data->nHPropEntries = data->nHPropVisible = data->nHPropFirst = -1;
    rteSetDefaultBubbleStyles(data);
    if (!rteInitDocument(data)) {
        CoerceMethod(cl, obj, OM_DISPOSE);
        return 0;
    }
    rteSetAttrs(cl, obj, msg->ops_AttrList, TRUE);
    return (IPTR)obj;
}

static IPTR mDispose(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    rteFreeImages(data);
    rteFreeDocument(data);
    if (data->keywords != NULL)
        FreeVec(data->keywords);
    if (data->inactiveContents != NULL)
        FreeVec(data->inactiveContents);
    if (data->clickedLink != NULL)
        FreeVec(data->clickedLink);
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR mSet(struct IClass *cl, Object *obj, struct opSet *msg) {
    rteSetAttrs(cl, obj, msg->ops_AttrList, FALSE);
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static IPTR mGet(struct IClass *cl, Object *obj, struct opGet *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    IPTR *store = (IPTR *)msg->opg_Storage;

    switch (msg->opg_AttrID) {
    case MUIA_TextEditor_ActiveObjectOnClick:
        *store = data->activeOnClick;
        return TRUE;
    case MUIA_TextEditor_AreaMarked:
        *store = rteHasSelection(data);
        return TRUE;
    case MUIA_TextEditor_AutoClip:
        *store = data->autoClip;
        return TRUE;
    case MUIA_TextEditor_ContentsChanged:
        *store = data->contentsChanged;
        return TRUE;
    case MUIA_TextEditor_ConvertTabs:
        *store = data->convertTabs;
        return TRUE;
    case MUIA_TextEditor_CursorIndex:
        *store = rtePosToIndex(data, data->cursor);
        return TRUE;
    case MUIA_TextEditor_CursorPosition: {
        static struct Rectangle rect;
        struct RTEPara *p = &data->paras[data->cursor.para];
        memset(&rect, 0, sizeof(rect));
        if (data->shown && p->numLines > 0) {
            struct RTELine *l;
            rteEnsureLayout(data, obj);
            l = &p->lines[rteLineOfOffset(p, data->cursor.offset)];
            rect.MinX = data->viewLeft + RTE_PAD_X - data->scrollX +
                        rteOffsetToX(data, p, l, data->cursor.offset);
            rect.MinY =
                data->viewTop + p->y + p->padTop + l->y - data->scrollY;
            rect.MaxX = rect.MinX + 1;
            rect.MaxY = rect.MinY + l->height - 1;
        }
        *store = (IPTR)&rect;
        return TRUE;
    }
    case MUIA_TextEditor_CursorX:
        *store = data->cursor.offset;
        return TRUE;
    case MUIA_TextEditor_CursorY:
        *store = data->cursor.para;
        return TRUE;
    case MUIA_TextEditor_ExportWrap:
        *store = data->exportWrap;
        return TRUE;
    case MUIA_TextEditor_FixedFont:
        *store = data->fixedFont;
        return TRUE;
    case MUIA_TextEditor_Flow:
        *store = data->globalFlow ? data->globalFlowValue
                                  : data->paras[data->cursor.para].flow;
        return TRUE;
    case MUIA_TextEditor_GlobalFlow:
        *store = data->globalFlow;
        return TRUE;
    case MUIA_TextEditor_HasChanged:
        *store = data->hasChanged;
        return TRUE;
    case MUIA_TextEditor_ImportWrap:
        *store = data->importWrap;
        return TRUE;
    case MUIA_TextEditor_InactiveContents:
        *store = (IPTR)data->inactiveContents;
        return TRUE;
    case MUIA_TextEditor_InsertMode:
        *store = !data->overwrite;
        return TRUE;
    case MUIA_TextEditor_KeyUpFocus:
        *store = (IPTR)data->keyUpFocus;
        return TRUE;
    case MUIA_TextEditor_MatchedKeyword:
        *store = (IPTR)data->matchedKeyword;
        return TRUE;
    case MUIA_TextEditor_MetaDataChanged:
        *store = data->metaChanged;
        return TRUE;
    case MUIA_TextEditor_PasteColors:
        *store = data->pasteColors;
        return TRUE;
    case MUIA_TextEditor_PasteStyles:
        *store = data->pasteStyles;
        return TRUE;
    case MUIA_TextEditor_Pen: {
        struct RTEPara *p = &data->paras[data->cursor.para];
        UWORD a = rteAttrAt(p, data->cursor.offset > 0 ? data->cursor.offset - 1
                                                       : 0);
        UBYTE c = RTE_A_COLOR(a);
        *store = data->colors[c].kind == RTE_COLOR_CMAP ? data->colors[c].value
                                                        : 0;
        return TRUE;
    }
    case MUIA_TextEditor_Prop_DeltaFactor:
        *store = data->lineHeight > 0 ? data->lineHeight : 1;
        return TRUE;
    case MUIA_TextEditor_Prop_Entries:
        *store = RTE_MAX(data->totalHeight, data->viewHeight);
        return TRUE;
    case MUIA_TextEditor_Prop_First:
        *store = data->scrollY;
        return TRUE;
    case MUIA_TextEditor_Prop_Visible:
        *store = data->viewHeight;
        return TRUE;
    case MUIA_TextEditor_Quiet:
        *store = data->quiet;
        return TRUE;
    case MUIA_TextEditor_ReadOnly:
        *store = data->readOnly;
        return TRUE;
    case MUIA_TextEditor_RedoAvailable:
        *store = data->redoList.mlh_Head->mln_Succ != NULL;
        return TRUE;
    case MUIA_TextEditor_RGBMode:
        *store = data->rgbMode;
        return TRUE;
    case MUIA_TextEditor_Separator:
        *store = data->paras[data->cursor.para].separator;
        return TRUE;
    case MUIA_TextEditor_StyleBold:
    case MUIA_TextEditor_StyleItalic:
    case MUIA_TextEditor_StyleUnderline: {
        struct RTEPara *p = &data->paras[data->cursor.para];
        UBYTE style = (UBYTE)(rteAttrAt(p, data->cursor.offset > 0
                                               ? data->cursor.offset - 1
                                               : 0) &
                              RTE_A_STYLEMASK);
        UBYTE bit = msg->opg_AttrID == MUIA_TextEditor_StyleBold
                        ? RTE_A_BOLD
                        : (msg->opg_AttrID == MUIA_TextEditor_StyleItalic
                               ? RTE_A_ITALIC
                               : RTE_A_UNDERLINE);
        if (data->cursor.offset == 0 && !rteHasSelection(data))
            style = data->typeStyle;
        *store = (style & bit) != 0;
        return TRUE;
    }
    case MUIA_TextEditor_TabSize:
        *store = data->tabSize;
        return TRUE;
    case MUIA_TextEditor_TypeAndSpell:
        *store = data->typeAndSpell;
        return TRUE;
    case MUIA_TextEditor_UndoAvailable:
        *store = data->undoList.mlh_Head->mln_Succ != NULL;
        return TRUE;
    case MUIA_TextEditor_UndoLevels:
        *store = data->undoLevels;
        return TRUE;
    case MUIA_TextEditor_WrapBorder:
        *store = data->wrapBorder;
        return TRUE;
    case MUIA_TextEditor_WrapMode:
        *store = data->wrapMode;
        return TRUE;
    case MUIA_TextEditor_WrapWords:
        *store = data->wrapWords;
        return TRUE;
    case MUIA_RichTextEditor_SubmitHook:
    case MUIA_ActionTextEditor_SubmitHook:
        *store = (IPTR)data->submitHook;
        return TRUE;
    case MUIA_RichTextEditor_SubmitEmpty:
        *store = data->submitEmpty;
        return TRUE;
    case MUIA_RichTextEditor_Markdown:
        *store = data->markdown;
        return TRUE;
    case MUIA_RichTextEditor_HideMarkup:
        *store = data->hideMarkup;
        return TRUE;
    case MUIA_RichTextEditor_ClickedLink:
        *store = (IPTR)data->clickedLink;
        return TRUE;
    case MUIA_RichTextEditor_StickToBottom:
        *store = data->stickToBottom;
        return TRUE;
    case MUIA_RichTextEditor_MaxLength:
        *store = data->maxLength;
        return TRUE;
    case MUIA_RichTextEditor_MaxImageHeight:
        *store = data->maxImageHeight;
        return TRUE;
    case MUIA_RichTextEditor_Length:
        *store = data->totalLength;
        return TRUE;
    case MUIA_RichTextEditor_Paragraphs:
        *store = data->numParas;
        return TRUE;
    case MUIA_RichTextEditor_AtBottom:
        rteEnsureLayout(data, obj);
        *store = rteViewAtBottom(data);
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static IPTR mSetup(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg))
        return FALSE;
    rteSetupFonts(data, obj);
    rteObtainPens(data, obj);
    data->setup = TRUE;
    data->dbFailed = FALSE;
    data->layoutWidth = 0;
    rteMarkAllDirty(data);

    memset(&data->eh, 0, sizeof(data->eh));
    data->eh.ehn_Class = cl;
    data->eh.ehn_Object = obj;
    data->eh.ehn_Priority = 0;
    data->eh.ehn_Flags = MUI_EHF_GUIMODE | MUI_EHF_ALWAYSKEYS;
    data->ehEvents = IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY;
#if defined(__AMIGAOS4__)
    data->ehEvents |= IDCMP_EXTENDEDMOUSE;
#endif
    data->eh.ehn_Events = data->ehEvents;
    DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->eh);
    data->ehAdded = TRUE;
    return TRUE;
}

static IPTR mCleanup(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    rteRestoreCycleKeys(cl, data, obj);
    if (data->ehAdded) {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->eh);
        data->ehAdded = FALSE;
    }
    data->selecting = FALSE;
    rteFreeDoubleBuffer(data);
    rteFreeImages(data);
    rteReleasePens(data, obj);
    if (data->ownCodeFont && data->codeFont != NULL)
        CloseFont(data->codeFont);
    data->ownCodeFont = FALSE;
    data->codeFont = NULL;
    data->font = NULL;
    data->setup = FALSE;
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR mAskMinMax(struct IClass *cl, Object *obj,
                       struct MUIP_AskMinMax *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct MUI_MinMax *mi;
    LONG lh = data->lineHeight > 0 ? data->lineHeight : 8;
    LONG cw = data->widths[0]['n'] > 0 ? data->widths[0]['n'] : 8;
    LONG minW, defW, minH, defH;

    DoSuperMethodA(cl, obj, (Msg)msg);
    mi = msg->MinMaxInfo;
    minW = (data->columns > 0 ? data->columns : 6) * cw + 2 * RTE_PAD_X;
    defW = (data->columns > 0 ? data->columns : 40) * cw + 2 * RTE_PAD_X;
    if (data->rows > 0) {
        minH = data->rows * lh + 2 * RTE_PAD_Y;
        defH = minH;
    } else {
        minH = lh + 2 * RTE_PAD_Y;
        defH = 5 * lh + 2 * RTE_PAD_Y;
    }
    mi->MinWidth += minW;
    mi->DefWidth += defW;
    mi->MaxWidth = MUI_MAXMAX;
    mi->MinHeight += minH;
    mi->DefHeight += defH;
    mi->MaxHeight = MUI_MAXMAX;
    return 0;
}

static IPTR mShow(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    if (!DoSuperMethodA(cl, obj, msg))
        return FALSE;
    data->shown = TRUE;
    data->drawAll = TRUE;
    rteEnsureLayout(data, obj);
    if (data->hasPendingScroll) {
        data->hasPendingScroll = FALSE;
        data->scrollY = data->pendingScroll;
        rteClampScroll(data);
    }
    rteUpdateSliders(data, obj);
    return TRUE;
}

static IPTR mHide(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    data->shown = FALSE;
    data->selecting = FALSE;
    rteFreeDoubleBuffer(data);
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR mDraw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    DoSuperMethodA(cl, obj, (Msg)msg);
    if (!data->shown)
        return 0;
    rteUpdateGeometry(data, obj);
    if (msg->flags & MADF_DRAWOBJECT)
        rteDrawUpdate(data, obj, TRUE);
    else if (msg->flags & MADF_DRAWUPDATE)
        rteDrawUpdate(data, obj, FALSE);
    rteUpdateSliders(data, obj);
    return 0;
}

/* Set while this class makes an object the window's active object */
static BOOL rteActivating = FALSE;

#define RTE_CYCLE_KEYS (MUIKEYF_GADGET_NEXT | MUIKEYF_GADGET_PREV)

/**
 * Tab is typed into an editable object, so MUI must not use it to move to
 * the next gadget while one is active (Amiga+Tab and Ctrl+Tab still do)
 */
static void rteDisableCycleKeys(struct RTEData *data, Object *obj) {
    Object *win = _win(obj);
    ULONG keys = 0;
    if (data->readOnly || win == NULL || data->keysDisabled)
        return;
    get(win, MUIA_Window_DisableKeys, &keys);
    set(win, MUIA_Window_DisableKeys, keys | RTE_CYCLE_KEYS);
    data->keysDisabled = TRUE;
}

static void rteRestoreCycleKeys(struct IClass *cl, struct RTEData *data,
                                Object *obj) {
    Object *win = _win(obj);
    Object *current = NULL;
    ULONG keys = 0;
    if (!data->keysDisabled || win == NULL)
        return;
    data->keysDisabled = FALSE;
    /* If focus moved straight to another editable editor, it wants them off
     * too: MUI may activate it before this one is told it is inactive */
    get(win, MUIA_Window_ActiveObject, &current);
    if (current != NULL && current != obj && OCLASS(current) == cl) {
        struct RTEData *other = INST_DATA(cl, current);
        if (other->keysDisabled)
            return;
    }
    get(win, MUIA_Window_DisableKeys, &keys);
    set(win, MUIA_Window_DisableKeys, keys & ~RTE_CYCLE_KEYS);
}

static IPTR mGoActive(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    Object *win = data->shown ? _win(obj) : NULL;

    /* Programs often call MUIM_GoActive directly to put the cursor in an
     * editor. Unless the window agrees that this is its active object, MUI
     * keeps navigation keys such as Return for whatever it thinks is active,
     * so make it so. MUI then sends MUIM_GoActive again. */
    if (win != NULL && !rteActivating) {
        Object *current = NULL;
        get(win, MUIA_Window_ActiveObject, &current);
        if (current != obj) {
            rteActivating = TRUE;
            set(win, MUIA_Window_ActiveObject, obj);
            rteActivating = FALSE;
            if (data->active)
                return 0;
        }
    }
    data->active = TRUE;
    data->justActivated = TRUE;
    rteDisableCycleKeys(data, obj);
    CurrentTime(&data->activatedSecs, &data->activatedMicros);
    rteInvalidateSelection(data);
    rteInvalidateCursorLine(data);
    if (data->inactiveContents != NULL && data->totalLength == 0)
        data->drawAll = TRUE;
    rteRequestRedraw(data, obj);
    return DoSuperMethodA(cl, obj, msg);
}

static IPTR mGoInactive(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    data->active = FALSE;
    rteEndDrag(data, obj);
    rteRestoreCycleKeys(cl, data, obj);
    rteInvalidateSelection(data);
    rteInvalidateCursorLine(data);
    if (data->inactiveContents != NULL && data->totalLength == 0)
        data->drawAll = TRUE;
    rteRequestRedraw(data, obj);
    return DoSuperMethodA(cl, obj, msg);
}

/* TextEditor.mcc methods */

static IPTR mClearText(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    (void)msg;
    rteClear(data);
    rteFreeUndo(data, FALSE);
    rteRefreshAll(data, obj);
    return TRUE;
}

static IPTR mExportText(struct IClass *cl, Object *obj, Msg msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos from = {0, 0}, to;
    (void)msg;
    (void)obj;
    to.para = data->numParas - 1;
    to.offset = data->paras[to.para].len;
    return (IPTR)rteExport(data, from, to, data->exportHook, data->exportWrap);
}

static void rteResolveBlockCoords(struct RTEData *data, LONG sx, LONG sy,
                                  LONG ex, LONG ey, struct RTEPos *from,
                                  struct RTEPos *to) {
    if (sy == MUIV_TextEditor_SetBlock_Min || sy < 0)
        sy = 0;
    if (ey == MUIV_TextEditor_SetBlock_Max || ey < 0 ||
        ey >= (LONG)data->numParas)
        ey = data->numParas - 1;
    if (sy >= (LONG)data->numParas)
        sy = data->numParas - 1;
    from->para = sy;
    to->para = ey;
    from->offset = (sx == MUIV_TextEditor_SetBlock_Min || sx < 0) ? 0 : sx;
    to->offset = ex == MUIV_TextEditor_SetBlock_Max || ex < 0
                     ? data->paras[ey].len
                     : (ULONG)ex;
    rteClampPos(data, from);
    rteClampPos(data, to);
}

static IPTR mExportBlock(struct IClass *cl, Object *obj,
                         struct MUIP_TextEditor_ExportBlock *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos from, to;
    (void)obj;
    if (msg->flags & MUIF_TextEditor_ExportBlock_TakeBlock) {
        rteResolveBlockCoords(data, msg->startx, msg->starty, msg->stopx,
                              msg->stopy, &from, &to);
    } else if (rteHasSelection(data)) {
        rteOrderedSelection(data, &from, &to);
    } else if (msg->flags & MUIF_TextEditor_ExportBlock_FullLines) {
        from = to = data->cursor;
    } else {
        STRPTR empty = AllocVec(1, MEMF_ANY | MEMF_CLEAR);
        return (IPTR)empty;
    }
    if (msg->flags & MUIF_TextEditor_ExportBlock_FullLines) {
        from.offset = 0;
        to.offset = data->paras[to.para].len;
    }
    return (IPTR)rteExport(data, from, to, data->exportHook, data->exportWrap);
}

static IPTR mInsertText(struct IClass *cl, Object *obj,
                        struct MUIP_TextEditor_InsertText *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos pos, end;
    if (msg->text == NULL)
        return 0;
    switch (msg->pos) {
    case MUIV_TextEditor_InsertText_Top:
        pos.para = 0;
        pos.offset = 0;
        break;
    case MUIV_TextEditor_InsertText_Bottom:
        pos.para = data->numParas - 1;
        pos.offset = data->paras[pos.para].len;
        break;
    default:
        if (rteHasSelection(data)) {
            struct RTEPos from, to;
            rteUndoBeginGroup(data);
            rteOrderedSelection(data, &from, &to);
            rteDeleteRange(data, from, to, TRUE);
            data->cursor = from;
            data->marking = FALSE;
        }
        pos = data->cursor;
        break;
    }
    rteUndoBeginGroup(data);
    if (rteImport(data, pos, msg->text, -1, FALSE, data->importHook, &end,
                  TRUE) &&
        msg->pos == MUIV_TextEditor_InsertText_Cursor) {
        data->cursor = end;
        data->anchor = end;
    }
    rteAfterEdit(data, obj);
    return 0;
}

static IPTR mAppend(struct IClass *cl, Object *obj,
                    struct MUIP_RichTextEditor_Append *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos pos;
    BOOL follow;
    BOOL cursorAtEnd;
    BOOL ok;

    if (msg->text == NULL)
        return FALSE;
    if (data->shown && !data->quiet)
        rteEnsureLayout(data, obj);
    follow = data->stickToBottom && rteViewAtBottom(data) &&
             !(msg->flags & MUIF_RichTextEditor_Append_NoScroll);
    pos.para = data->numParas - 1;
    pos.offset = data->paras[pos.para].len;
    cursorAtEnd = rteComparePos(data->cursor, pos) == 0;
    data->importCarry = TRUE;
    ok = rteImport(data, pos, msg->text, msg->length,
                   (msg->flags & MUIF_RichTextEditor_Append_Raw) != 0,
                   MUIV_TextEditor_ImportHook_Plain, NULL, FALSE);
    data->importCarry = FALSE;
    if (!ok)
        return FALSE;
    if (cursorAtEnd && !data->readOnly) {
        data->cursor.para = data->numParas - 1;
        data->cursor.offset = data->paras[data->cursor.para].len;
        data->anchor = data->cursor;
    }
    rteTrimToMaxLength(data);
    rteChanged(data, obj, TRUE);
    if (data->quiet || !data->shown)
        return TRUE;
    rteEnsureLayout(data, obj);
    if (follow)
        rteScrollTo(data, obj, 0x3fffffff);
    rteRequestRedraw(data, obj);
    return TRUE;
}

static IPTR mMarkText(struct IClass *cl, Object *obj,
                      struct MUIP_TextEditor_MarkText *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    rteInvalidateRange(data, 0, 0x3fffffff);
    if (msg->start_crsr_y == MUIV_TextEditor_MarkText_None &&
        msg->stop_crsr_x != MUIV_TextEditor_MarkText_All) {
        data->marking = FALSE;
        data->anchor = data->cursor;
    } else if (msg->stop_crsr_x == MUIV_TextEditor_MarkText_All ||
               msg->stop_crsr_y == MUIV_TextEditor_MarkText_All) {
        data->anchor.para = 0;
        data->anchor.offset = 0;
        data->cursor.para = data->numParas - 1;
        data->cursor.offset = data->paras[data->cursor.para].len;
        data->marking = TRUE;
    } else {
        rteResolveBlockCoords(data, msg->start_crsr_x, msg->start_crsr_y,
                              msg->stop_crsr_x, msg->stop_crsr_y,
                              &data->anchor, &data->cursor);
        data->marking = TRUE;
    }
    rteRequestRedraw(data, obj);
    rteNotifyState(data, obj);
    if (data->readOnly && data->autoClip && rteHasSelection(data) &&
        data->active)
        rteCopy(data, obj, FALSE);
    return TRUE;
}

static IPTR mBlockInfo(struct IClass *cl, Object *obj,
                       struct MUIP_TextEditor_BlockInfo *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos from, to;
    (void)obj;
    if (!rteHasSelection(data))
        return FALSE;
    rteOrderedSelection(data, &from, &to);
    if (msg->startx != NULL)
        *msg->startx = from.offset;
    if (msg->starty != NULL)
        *msg->starty = from.para;
    if (msg->stopx != NULL)
        *msg->stopx = to.offset;
    if (msg->stopy != NULL)
        *msg->stopy = to.para;
    return TRUE;
}

static IPTR mCursorXYToIndex(struct IClass *cl, Object *obj,
                             struct MUIP_TextEditor_CursorXYToIndex *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos pos;
    BOOL valid = msg->y >= 0 && (ULONG)msg->y < data->numParas && msg->x >= 0;
    (void)obj;
    pos.para = msg->y < 0 ? 0 : msg->y;
    pos.offset = msg->x < 0 ? 0 : msg->x;
    if (valid && (ULONG)msg->x > data->paras[msg->y].len)
        valid = FALSE;
    rteClampPos(data, &pos);
    if (msg->index != NULL)
        *msg->index = rtePosToIndex(data, pos);
    return valid;
}

static IPTR mIndexToCursorXY(struct IClass *cl, Object *obj,
                             struct MUIP_TextEditor_IndexToCursorXY *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos pos = rteIndexToPos(data, msg->index < 0 ? 0 : msg->index);
    (void)obj;
    if (msg->x != NULL)
        *msg->x = pos.offset;
    if (msg->y != NULL)
        *msg->y = pos.para;
    return msg->index >= 0 && (ULONG)msg->index <= data->totalLength;
}

static IPTR mSearch(struct IClass *cl, Object *obj,
                    struct MUIP_TextEditor_Search *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    BOOL found;
    rteInvalidateRange(data, 0, 0x3fffffff);
    found = rteSearch(data, msg->SearchString, msg->Flags);
    if (found) {
        rteMakeCursorVisible(data, obj);
        if (data->readOnly && data->shown) {
            struct RTEPara *p = &data->paras[data->cursor.para];
            rteEnsureLayout(data, obj);
            if (p->y < data->scrollY ||
                p->y + p->height > data->scrollY + data->viewHeight)
                rteScrollTo(data, obj, p->y - data->viewHeight / 3);
        }
    } else {
        rteHandleError(data, obj, Error_StringNotFound);
    }
    rteRequestRedraw(data, obj);
    rteNotifyState(data, obj);
    return found;
}

static IPTR mReplace(struct IClass *cl, Object *obj,
                     struct MUIP_TextEditor_Replace *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos from, to, end;
    if (!rteHasSelection(data) || data->readOnly)
        return FALSE;
    rteOrderedSelection(data, &from, &to);
    rteUndoBeginGroup(data);
    rteDeleteRange(data, from, to, TRUE);
    data->cursor = from;
    if (msg->NewString != NULL &&
        rteImport(data, from, msg->NewString, -1, TRUE,
                  MUIV_TextEditor_ImportHook_Plain, &end, TRUE))
        data->cursor = end;
    data->anchor = data->cursor;
    data->marking = FALSE;
    rteAfterEdit(data, obj);
    return TRUE;
}

static IPTR mSetBlock(struct IClass *cl, Object *obj,
                      struct MUIP_TextEditor_SetBlock *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    struct RTEPos from, to;
    if (msg->starty >= (LONG)data->numParas && msg->starty >= 0)
        return FALSE;
    rteResolveBlockCoords(data, msg->startx, msg->starty, msg->stopx,
                          msg->stopy, &from, &to);
    if (rteComparePos(from, to) > 0)
        return FALSE;
    rteSetBlockAttrs(data, from, to, msg->operation, msg->value);
    rteChanged(data, obj, FALSE);
    rteRequestRedraw(data, obj);
    return TRUE;
}

static IPTR mTestPos(struct IClass *cl, Object *obj,
                     struct MUIP_TextEditor_TestPos *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    LONG mx = msg->mx, my = msg->my;
    if (msg->index != NULL)
        *msg->index = -1;
    if (msg->cursorx != NULL)
        *msg->cursorx = -1;
    if (msg->cursory != NULL)
        *msg->cursory = -1;
    if (!data->shown || !RTE_INOBJECT(obj, mx, my))
        return FALSE;
    rteEnsureLayout(data, obj);
    {
        LONG docY = data->scrollY + (my - data->viewTop);
        ULONG pi = rteParaAtY(data, docY);
        struct RTEPara *p = &data->paras[pi];
        struct RTEPos pos;
        pos.para = pi;
        pos.offset = 0;
        if (p->numLines > 0)
            pos.offset = rteXToOffset(
                data, p, &p->lines[rteLineAtY(p, docY)],
                mx - data->viewLeft - RTE_PAD_X + data->scrollX);
        if (msg->index != NULL)
            *msg->index = rtePosToIndex(data, pos);
        if (msg->cursorx != NULL)
            *msg->cursorx = pos.offset;
        if (msg->cursory != NULL)
            *msg->cursory = pos.para;
    }
    return TRUE;
}

/* Key bindings reported by MUIM_TextEditor_QueryKeyAction */
static const struct MUIP_TextEditor_Keybinding rteKeyBindings[] = {
    {0x4c, 0, MUIV_TextEditor_KeyAction_Up},
    {0x4d, 0, MUIV_TextEditor_KeyAction_Down},
    {0x4f, 0, MUIV_TextEditor_KeyAction_Left},
    {0x4e, 0, MUIV_TextEditor_KeyAction_Right},
    {0x4c, IEQUALIFIER_LALT, MUIV_TextEditor_KeyAction_PageUp},
    {0x4d, IEQUALIFIER_LALT, MUIV_TextEditor_KeyAction_PageDown},
    {0x4f, IEQUALIFIER_CONTROL, MUIV_TextEditor_KeyAction_StartOfLine},
    {0x4e, IEQUALIFIER_CONTROL, MUIV_TextEditor_KeyAction_EndOfLine},
    {0x4c, IEQUALIFIER_CONTROL, MUIV_TextEditor_KeyAction_Top},
    {0x4d, IEQUALIFIER_CONTROL, MUIV_TextEditor_KeyAction_Bottom},
    {0x4f, IEQUALIFIER_LALT, MUIV_TextEditor_KeyAction_PrevWord},
    {0x4e, IEQUALIFIER_LALT, MUIV_TextEditor_KeyAction_NextWord},
    {0x41, 0, MUIV_TextEditor_KeyAction_Backspace},
    {0x46, 0, MUIV_TextEditor_KeyAction_Delete},
    {0x44, IEQUALIFIER_LSHIFT, MUIV_TextEditor_KeyAction_Return},
    {0x42, 0, MUIV_TextEditor_KeyAction_Tab},
    {500 + 'x', IEQUALIFIER_RCOMMAND, MUIV_TextEditor_KeyAction_Cut},
    {500 + 'c', IEQUALIFIER_RCOMMAND, MUIV_TextEditor_KeyAction_Copy},
    {500 + 'v', IEQUALIFIER_RCOMMAND, MUIV_TextEditor_KeyAction_Paste},
    {500 + 'z', IEQUALIFIER_RCOMMAND, MUIV_TextEditor_KeyAction_Undo},
    {500 + 'z', IEQUALIFIER_RCOMMAND | IEQUALIFIER_LSHIFT,
     MUIV_TextEditor_KeyAction_Redo},
    {0x41, IEQUALIFIER_LSHIFT, MUIV_TextEditor_KeyAction_DelBOL},
    {0x46, IEQUALIFIER_LSHIFT, MUIV_TextEditor_KeyAction_DelEOL},
    {0x41, IEQUALIFIER_LALT, MUIV_TextEditor_KeyAction_DelBOW},
    {0x46, IEQUALIFIER_LALT, MUIV_TextEditor_KeyAction_DelEOW},
    {0x42, IEQUALIFIER_RCOMMAND, MUIV_TextEditor_KeyAction_NextGadget},
    {500 + 'a', IEQUALIFIER_RCOMMAND, MUIV_TextEditor_KeyAction_SelectAll},
    {0x45, 0, MUIV_TextEditor_KeyAction_SelectNone},
};

static IPTR mQueryKeyAction(struct IClass *cl, Object *obj,
                            struct MUIP_TextEditor_QueryKeyAction *msg) {
    ULONG i;
    (void)cl;
    (void)obj;
    for (i = 0; i < sizeof(rteKeyBindings) / sizeof(rteKeyBindings[0]); i++) {
        if (rteKeyBindings[i].action == msg->keyAction)
            return (IPTR)&rteKeyBindings[i];
    }
    return 0;
}

static IPTR mScrollTo(struct IClass *cl, Object *obj,
                      struct MUIP_RichTextEditor_ScrollTo *msg) {
    struct RTEData *data = INST_DATA(cl, obj);
    switch (msg->where) {
    case MUIV_RichTextEditor_ScrollTo_Top:
        rteScrollTo(data, obj, 0);
        break;
    case MUIV_RichTextEditor_ScrollTo_Bottom:
        rteScrollTo(data, obj, 0x3fffffff);
        break;
    case MUIV_RichTextEditor_ScrollTo_Cursor: {
        BOOL ro = data->readOnly;
        data->readOnly = FALSE;
        rteMakeCursorVisible(data, obj);
        data->readOnly = ro;
        break;
    }
    default:
        rteScrollTo(data, obj, msg->where);
        break;
    }
    return TRUE;
}

DISPATCHER(rteDispatcher) {
    switch (msg->MethodID) {
    case OM_NEW:
        return mNew(cl, obj, (struct opSet *)msg);
    case OM_DISPOSE:
        return mDispose(cl, obj, msg);
    case OM_SET:
        return mSet(cl, obj, (struct opSet *)msg);
    case OM_GET:
        return mGet(cl, obj, (struct opGet *)msg);
    case MUIM_Setup:
        return mSetup(cl, obj, msg);
    case MUIM_Cleanup:
        return mCleanup(cl, obj, msg);
    case MUIM_AskMinMax:
        return mAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
    case MUIM_Show:
        return mShow(cl, obj, msg);
    case MUIM_Hide:
        return mHide(cl, obj, msg);
    case MUIM_Draw:
        return mDraw(cl, obj, (struct MUIP_Draw *)msg);
    case MUIM_HandleEvent:
        return rteHandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
    case MUIM_GoActive:
        return mGoActive(cl, obj, msg);
    case MUIM_GoInactive:
        return mGoInactive(cl, obj, msg);

    case MUIM_TextEditor_ARexxCmd: {
        struct RTEData *data = INST_DATA(cl, obj);
        return rteARexx(data, obj,
                        ((struct MUIP_TextEditor_ARexxCmd *)msg)->command);
    }
    case MUIM_TextEditor_BlockInfo:
        return mBlockInfo(cl, obj, (struct MUIP_TextEditor_BlockInfo *)msg);
    case MUIM_TextEditor_ClearText:
        return mClearText(cl, obj, msg);
    case MUIM_TextEditor_CursorXYToIndex:
        return mCursorXYToIndex(cl, obj,
                                (struct MUIP_TextEditor_CursorXYToIndex *)msg);
    case MUIM_TextEditor_ExportBlock:
        return mExportBlock(cl, obj, (struct MUIP_TextEditor_ExportBlock *)msg);
    case MUIM_TextEditor_ExportText:
        return mExportText(cl, obj, msg);
    case MUIM_TextEditor_HandleError:
        return 0;
    case MUIM_TextEditor_IndexToCursorXY:
        return mIndexToCursorXY(cl, obj,
                                (struct MUIP_TextEditor_IndexToCursorXY *)msg);
    case MUIM_TextEditor_InsertText:
        return mInsertText(cl, obj, (struct MUIP_TextEditor_InsertText *)msg);
    case MUIM_TextEditor_MacroBegin: {
        struct RTEData *data = INST_DATA(cl, obj);
        data->macroRecording = TRUE;
        data->macroLen = 0;
        return TRUE;
    }
    case MUIM_TextEditor_MacroEnd: {
        struct RTEData *data = INST_DATA(cl, obj);
        data->macroRecording = FALSE;
        return TRUE;
    }
    case MUIM_TextEditor_MacroExecute: {
        struct RTEData *data = INST_DATA(cl, obj);
        rteMacroExecute(data, obj);
        return TRUE;
    }
    case MUIM_TextEditor_MarkText:
        return mMarkText(cl, obj, (struct MUIP_TextEditor_MarkText *)msg);
    case MUIM_TextEditor_QueryKeyAction:
        return mQueryKeyAction(cl, obj,
                               (struct MUIP_TextEditor_QueryKeyAction *)msg);
    case MUIM_TextEditor_Replace:
        return mReplace(cl, obj, (struct MUIP_TextEditor_Replace *)msg);
    case MUIM_TextEditor_Search:
        return mSearch(cl, obj, (struct MUIP_TextEditor_Search *)msg);
    case MUIM_TextEditor_SetBlock:
        return mSetBlock(cl, obj, (struct MUIP_TextEditor_SetBlock *)msg);
    case MUIM_TextEditor_TestPos:
        return mTestPos(cl, obj, (struct MUIP_TextEditor_TestPos *)msg);
    case MUIM_TextEditor_Redraw: {
        struct RTEData *data = INST_DATA(cl, obj);
        rteRefreshAll(data, obj);
        return TRUE;
    }

    case MUIM_RichTextEditor_Append:
        return mAppend(cl, obj, (struct MUIP_RichTextEditor_Append *)msg);
    case MUIM_RichTextEditor_ScrollTo:
        return mScrollTo(cl, obj, (struct MUIP_RichTextEditor_ScrollTo *)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

LONG createRichTextEditorClass(void) {
    if (richTextEditorClass != NULL)
        return RETURN_OK;
    richTextEditorClass =
        MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct RTEData),
                              ENTRY(rteDispatcher));
    /* No cl_ID: MUI 3.8 treats a class with an ID as public when disposing
     * its objects, which crashes a private class at shutdown */
    return richTextEditorClass != NULL ? RETURN_OK : RETURN_ERROR;
}

void deleteRichTextEditorClass(void) {
    if (richTextEditorClass == NULL)
        return;
    MUI_DeleteCustomClass(richTextEditorClass);
    richTextEditorClass = NULL;
}

#endif /* DAEMON */
