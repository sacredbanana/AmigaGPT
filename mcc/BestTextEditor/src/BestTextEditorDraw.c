/*
 * BestTextEditor - rendering
 *
 * The document is drawn as horizontal bands: the padding above a paragraph,
 * each visual line, and the padding below it. Every band is rendered into an
 * offscreen bitmap (with the object's MUI background) and then blitted to the
 * window in one go, so typing and streaming never flicker. Only bands that
 * intersect the invalidated document range are redrawn, and scrolling moves
 * the existing pixels with ScrollRasterBF() so just the exposed strip needs
 * rendering.
 */
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#if !defined(__AMIGAOS4__)
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#endif
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <string.h>
#include "BestTextEditorPrivate.h"

#define RTE_DB_MARGIN 32

/* ------------------------------------------------------------------------ */
/* Pens                                                                      */
/* ------------------------------------------------------------------------ */

static void rtePenRGB(struct ColorMap *cm, LONG pen, ULONG *rgb) {
    ULONG c[3];
    GetRGB32(cm, (UWORD)pen, 1, c);
    *rgb = ((c[0] >> 24) << 16) | ((c[1] >> 24) << 8) | (c[2] >> 24);
}

static LONG rteBestPen(struct ColorMap *cm, ULONG rgb, BOOL *allocated) {
    struct TagItem tags[3];
    LONG pen;
    ULONG r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
    tags[0].ti_Tag = OBP_Precision;
    tags[0].ti_Data = PRECISION_GUI;
    tags[1].ti_Tag = OBP_FailIfBad;
    tags[1].ti_Data = FALSE;
    tags[2].ti_Tag = TAG_DONE;
    pen = ObtainBestPenA(cm, r * 0x01010101UL, g * 0x01010101UL,
                         b * 0x01010101UL, tags);
    *allocated = pen >= 0;
    return pen;
}

static ULONG rteMix(ULONG a, ULONG b, ULONG percentB) {
    ULONG out = 0;
    LONG shift;
    for (shift = 16; shift >= 0; shift -= 8) {
        ULONG ca = (a >> shift) & 0xff, cb = (b >> shift) & 0xff;
        ULONG c = (ca * (100 - percentB) + cb * percentB) / 100;
        out |= c << shift;
    }
    return out;
}

static ULONG rteLuminance(ULONG rgb) {
    return (((rgb >> 16) & 0xff) * 299 + ((rgb >> 8) & 0xff) * 587 +
            (rgb & 0xff) * 114) /
           1000;
}

void rteSetDefaultBubbleStyles(struct RTEData *data) {
    UWORD i;
    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++) {
        data->bubbleStyles[i].side = bteDefaultBubbles[i].side;
        data->bubbleStyles[i].background = bteDefaultBubbles[i].background;
        data->bubbleStyles[i].text = bteDefaultBubbles[i].text;
        data->bubbleStyles[i].maxWidth = bteDefaultBubbles[i].maxWidth;
    }
    data->numBubbleStyles = BESTTEXTEDITOR_MAX_BUBBLE_STYLES;
}

/* ------------------------------------------------------------------------ */
/* Preferences (BestTextEditor.mcp)                                          */
/* ------------------------------------------------------------------------ */

/* The stored data of a configuration item, or NULL when it is not set */
static APTR rteConfigItem(Object *obj, ULONG id) {
    APTR item = NULL;
    if (!DoMethod(obj, MUIM_GetConfigItem, id, (IPTR)&item))
        return NULL;
    return item;
}

static BOOL rteConfigBool(Object *obj, ULONG id, BOOL def) {
    ULONG *v = (ULONG *)rteConfigItem(obj, id);
    return v != NULL ? *v != 0 : def;
}

/**
 * The colour a pen spec stands for on this screen. MUI resolves the spec
 * (MUI pen, system pen, palette entry or RGB), and the pen is given back
 * straight away: the caller allocates its own pen for the colour.
 * @param def spec to use when the item is not set, or NULL for none
 */
static BOOL rteConfigRGB(Object *obj, ULONG id, CONST_STRPTR def, ULONG *rgb) {
    struct MUI_PenSpec spec;
    const struct MUI_PenSpec *cfg = (const struct MUI_PenSpec *)rteConfigItem(obj, id);
    LONG pen;

    if (cfg == NULL) {
        if (def == NULL)
            return FALSE;
        memset(&spec, 0, sizeof(spec));
        strncpy(spec.buf, (const char *)def, sizeof(spec.buf) - 1);
        cfg = &spec;
    }
    pen = MUI_ObtainPen(muiRenderInfo(obj), (struct MUI_PenSpec *)cfg, 0);
    if (pen == -1)
        return FALSE;
    rtePenRGB(_screen(obj)->ViewPort.ColorMap, MUIPEN(pen), rgb);
    MUI_ReleasePen(muiRenderInfo(obj), pen);
    return TRUE;
}

static void rteConfigFont(Object *obj, ULONG id, char *buf, ULONG size) {
    CONST_STRPTR s = (CONST_STRPTR)rteConfigItem(obj, id);
    buf[0] = '\0';
    if (s != NULL) {
        strncpy(buf, (const char *)s, size - 1);
        buf[size - 1] = '\0';
    }
}

void rteReadConfig(struct RTEData *data, Object *obj) {
    UWORD i;

    rteConfigFont(obj, MUICFG_BestTextEditor_TextFont, data->cfgTextFont,
                  sizeof(data->cfgTextFont));
    rteConfigFont(obj, MUICFG_BestTextEditor_CodeFont, data->cfgCodeFont,
                  sizeof(data->cfgCodeFont));

    data->cfgAutoColors = rteConfigBool(obj, MUICFG_BestTextEditor_AutoColors,
                                        BTE_DEFAULT_AUTOCOLORS);
    for (i = 0; i < BTE_CFG_PEN_COUNT; i++) {
        data->cfgPenValid[i] =
            !data->cfgAutoColors &&
            rteConfigRGB(obj, MUICFG_BestTextEditor_LinkPen + i,
                         (CONST_STRPTR)bteDefaultPens[i], &data->cfgPenRGB[i]);
    }

    if (data->appBubbles)
        return;
    rteSetDefaultBubbleStyles(data);
    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++) {
        struct BestTextBubbleStyle *bs = &data->bubbleStyles[i];
        ULONG base = BTE_CFG_BUBBLE(i);
        ULONG *v, rgb;
        BOOL autoText = bteDefaultBubbles[i].text == MUIV_BestTextEditor_Color_Auto;

        v = (ULONG *)rteConfigItem(obj, base + BTE_CFG_BUBBLE_SIDE);
        if (v != NULL && *v >= MUIV_BestTextEditor_Bubble_Left &&
            *v <= MUIV_BestTextEditor_Bubble_Full)
            bs->side = *v;
        v = (ULONG *)rteConfigItem(obj, base + BTE_CFG_BUBBLE_WIDTH);
        if (v != NULL && *v >= BTE_MIN_BUBBLE_WIDTH && *v <= 100)
            bs->maxWidth = *v;
        if (rteConfigRGB(obj, base + BTE_CFG_BUBBLE_BACKGROUND, NULL, &rgb))
            bs->background = rgb;
        autoText = rteConfigBool(obj, base + BTE_CFG_BUBBLE_AUTOTEXT, autoText);
        if (autoText) {
            bs->text = MUIV_BestTextEditor_Color_Auto;
        } else if (rteConfigRGB(obj, base + BTE_CFG_BUBBLE_TEXT, NULL, &rgb)) {
            bs->text = rgb;
        } else if (bs->text == MUIV_BestTextEditor_Color_Auto) {
            /* the .mcp shows black for a bubble that had automatic text */
            bs->text = 0x000000;
        }
    }
}

void rteObtainPens(struct RTEData *data, Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    UWORD *mp = _pens(obj);
    ULONG bg, fg, link;
    UWORD i;

    for (i = 0; i < RTE_PEN_COUNT; i++)
        data->penAlloc[i] = FALSE;
    data->bgSampled = FALSE;
    data->pens[RTE_PEN_TEXT] = mp[MPEN_TEXT];
    data->pens[RTE_PEN_DIM] = mp[MPEN_HALFSHADOW];
    data->pens[RTE_PEN_MARK] = mp[MPEN_FILL];
    data->pens[RTE_PEN_MARKTEXT] = mp[MPEN_SHINE];
    data->pens[RTE_PEN_CURSOR] = mp[MPEN_FILL];
    data->pens[RTE_PEN_HIGHLIGHT] = mp[MPEN_FILL];
    data->pens[RTE_PEN_SHINE] = mp[MPEN_SHINE];
    data->pens[RTE_PEN_SHADOW] = mp[MPEN_SHADOW];
    data->pens[RTE_PEN_CODETEXT] = mp[MPEN_TEXT];

    /* Tints derived from the actual text and background colours, so dark
     * and light MUI themes both get readable accents */
    rtePenRGB(cm, mp[MPEN_BACKGROUND], &bg);
    rtePenRGB(cm, mp[MPEN_TEXT], &fg);
    link = rteLuminance(bg) > 110 ? 0x1a56c8 : 0x7fb2ff;
    data->pens[RTE_PEN_LINK] = rteBestPen(cm, link, &data->penAlloc[RTE_PEN_LINK]);
    data->pens[RTE_PEN_CODEBG] =
        rteBestPen(cm, rteMix(bg, fg, 9), &data->penAlloc[RTE_PEN_CODEBG]);
    data->pens[RTE_PEN_QUOTE] =
        rteBestPen(cm, rteMix(bg, fg, 35), &data->penAlloc[RTE_PEN_QUOTE]);
    data->pens[RTE_PEN_RULE] =
        rteBestPen(cm, rteMix(bg, fg, 25), &data->penAlloc[RTE_PEN_RULE]);
    /* Colours chosen in the preferences replace the automatic ones */
    if (!data->cfgAutoColors) {
        static const UBYTE cfgPen[BTE_CFG_PEN_COUNT] = {
            RTE_PEN_LINK,   RTE_PEN_CODEBG, RTE_PEN_CODETEXT, RTE_PEN_QUOTE,
            RTE_PEN_RULE,   RTE_PEN_CURSOR, RTE_PEN_MARK,     RTE_PEN_MARKTEXT};
        for (i = 0; i < BTE_CFG_PEN_COUNT; i++) {
            UBYTE p = cfgPen[i];
            if (!data->cfgPenValid[i])
                continue;
            if (data->penAlloc[p])
                ReleasePen(cm, data->pens[p]);
            data->pens[p] = rteBestPen(cm, data->cfgPenRGB[i], &data->penAlloc[p]);
        }
    }
    for (i = 0; i < RTE_PEN_COUNT; i++) {
        if (data->pens[i] < 0) {
            data->pens[i] = mp[MPEN_TEXT];
            data->penAlloc[i] = FALSE;
        }
    }

    for (i = 0; i < data->numBubbleStyles; i++) {
        const struct BestTextBubbleStyle *bs = &data->bubbleStyles[i];
        ULONG textRGB = bs->text;
        ULONG actualBg = bs->background;
        data->bubbleBgPen[i] =
            rteBestPen(cm, bs->background, &data->bubbleBgAlloc[i]);
        if (data->bubbleBgPen[i] < 0)
            data->bubbleBgPen[i] = mp[MPEN_HALFSHINE];
        else
            rtePenRGB(cm, data->bubbleBgPen[i], &actualBg);
        if (textRGB == MUIV_BestTextEditor_Color_Auto)
            textRGB = rteLuminance(actualBg) > 140 ? 0x000000 : 0xffffff;
        data->bubbleTextPen[i] = rteBestPen(cm, textRGB, &data->bubbleTextAlloc[i]);
        if (data->bubbleTextPen[i] < 0)
            data->bubbleTextPen[i] = mp[MPEN_TEXT];
        /* Code inside a bubble is tinted from the bubble's own colours */
        data->bubbleCodePen[i] = rteBestPen(cm, rteMix(actualBg, textRGB, 14),
                                            &data->bubbleCodeAlloc[i]);
        if (data->bubbleCodePen[i] < 0)
            data->bubbleCodePen[i] = data->bubbleBgPen[i];
    }
}

void rteReleasePens(struct RTEData *data, Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    UWORD i;
    for (i = 0; i < RTE_PEN_COUNT; i++) {
        if (data->penAlloc[i])
            ReleasePen(cm, data->pens[i]);
        data->penAlloc[i] = FALSE;
    }
    for (i = 0; i < data->numBubbleStyles; i++) {
        if (data->bubbleBgAlloc[i])
            ReleasePen(cm, data->bubbleBgPen[i]);
        if (data->bubbleTextAlloc[i])
            ReleasePen(cm, data->bubbleTextPen[i]);
        if (data->bubbleCodeAlloc[i])
            ReleasePen(cm, data->bubbleCodePen[i]);
        data->bubbleBgAlloc[i] = data->bubbleTextAlloc[i] = FALSE;
        data->bubbleCodeAlloc[i] = FALSE;
    }
    for (i = 0; i < data->numColors; i++) {
        if (data->colors[i].allocated)
            ReleasePen(cm, data->colors[i].pen);
        data->colors[i].allocated = FALSE;
        data->colors[i].pen = -1;
    }
}

LONG rteResolveColor(struct RTEData *data, Object *obj, UWORD index) {
    struct RTEColor *col;
    UWORD *mp = _pens(obj);
    if (index == 0 || index >= data->numColors)
        return data->pens[RTE_PEN_TEXT];
    col = &data->colors[index];
    if (col->pen >= 0)
        return col->pen;
    if (col->kind == RTE_COLOR_RGB) {
        BOOL alloc;
        col->pen = rteBestPen(_screen(obj)->ViewPort.ColorMap, col->value,
                              &alloc);
        col->allocated = alloc;
        if (col->pen < 0)
            col->pen = data->pens[RTE_PEN_TEXT];
    } else {
        ULONG n = col->value;
        if (data->colorMap != NULL && n > 0) {
            col->pen = data->colorMap[n - 1];
        } else {
            static const UBYTE map[] = {MPEN_TEXT,      MPEN_SHINE,
                                        MPEN_HALFSHINE, MPEN_BACKGROUND,
                                        MPEN_HALFSHADOW, MPEN_SHADOW,
                                        MPEN_TEXT,      MPEN_FILL,
                                        MPEN_FILL};
            if (n < sizeof(map))
                col->pen = mp[map[n]];
            else
                col->pen = (LONG)(n - 9);
        }
    }
    return col->pen;
}

/* ------------------------------------------------------------------------ */
/* Double buffer                                                             */
/* ------------------------------------------------------------------------ */

void rteFreeDoubleBuffer(struct RTEData *data) {
    if (data->dbBitMap != NULL) {
        WaitBlit();
        FreeBitMap(data->dbBitMap);
    }
    data->dbBitMap = NULL;
    data->dbWidth = data->dbHeight = 0;
}

static BOOL rteEnsureDoubleBuffer(struct RTEData *data, Object *obj,
                                  LONG width, LONG height) {
    struct BitMap *friendBM = _rp(obj)->BitMap;
    LONG w = width + 2 * RTE_DB_MARGIN;
    LONG h = height + 8;

    if (data->dbFailed)
        return FALSE;
    if (data->dbBitMap != NULL && data->dbWidth >= w && data->dbHeight >= h)
        return TRUE;
    if (data->dbBitMap != NULL) {
        if (data->dbWidth > w)
            w = data->dbWidth;
        if (data->dbHeight > h)
            h = data->dbHeight;
    }
    rteFreeDoubleBuffer(data);
    data->dbBitMap = AllocBitMap(w, h, GetBitMapAttr(friendBM, BMA_DEPTH),
                                 BMF_MINPLANES, friendBM);
    if (data->dbBitMap == NULL) {
        data->dbFailed = TRUE;
        return FALSE;
    }
    InitRastPort(&data->dbRP);
    data->dbRP.BitMap = data->dbBitMap;
    data->dbWidth = w;
    data->dbHeight = h;
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Primitives with clipping                                                  */
/* ------------------------------------------------------------------------ */

/* The target of a band: a rastport, its clip rectangle, and where the
 * band's text area origin is inside it */
struct RTETarget {
    struct RastPort *rp;
    LONG clipL, clipT, clipR, clipB; /* inclusive bounds */
    LONG ox;     /* x of text area x = 0 */
    LONG oy;     /* y of the band top */
    LONG docTop; /* document y of the band top */
    LONG height;
    BOOL buffered;
};

static void rteFill(struct RTETarget *t, LONG pen, LONG x0, LONG y0, LONG x1,
                    LONG y1) {
    if (x0 < t->clipL)
        x0 = t->clipL;
    if (y0 < t->clipT)
        y0 = t->clipT;
    if (x1 > t->clipR)
        x1 = t->clipR;
    if (y1 > t->clipB)
        y1 = t->clipB;
    if (x1 < x0 || y1 < y0)
        return;
    SetAPen(t->rp, pen);
    RectFill(t->rp, x0, y0, x1, y1);
}

/* Inset of a rounded corner for row r (0 = the outermost row) */
static LONG rteCornerInset(LONG r) {
    static const UBYTE inset[RTE_BUBBLE_RADIUS] = {5, 3, 2, 1, 1, 0, 0, 0};
    if (r < 0 || r >= RTE_BUBBLE_RADIUS)
        return 0;
    return inset[r];
}

/* ------------------------------------------------------------------------ */
/* Band decorations                                                          */
/* ------------------------------------------------------------------------ */

static LONG rteAreaWidthPx(struct RTEData *data) {
    return data->viewWidth - 2 * RTE_PAD_X;
}

/** Left and right (exclusive) of the content box of a paragraph */
static void rteContentBox(struct RTEData *data, struct RTEPara *p, LONG *l,
                          LONG *r) {
    if (p->bubble != 0) {
        *l = p->bubbleX + RTE_BUBBLE_PAD_X;
        *r = p->bubbleX + p->bubbleW - RTE_BUBBLE_PAD_X;
    } else {
        *l = 0;
        *r = rteAreaWidthPx(data);
        if (data->wrapMode == MUIV_TextEditor_WrapMode_NoWrap)
            *r = RTE_MAX(*r, data->maxLineWidth + RTE_CODE_PAD * 2);
    }
}

static LONG rteCodeBgPen(struct RTEData *data, struct RTEPara *p) {
    if (p->bubble != 0 && p->bubble <= data->numBubbleStyles)
        return data->bubbleCodePen[p->bubble - 1];
    return data->pens[RTE_PEN_CODEBG];
}

static void rteDrawBubble(struct RTEData *data, struct RTETarget *t,
                          struct RTEPara *p) {
    LONG top = p->y + (p->bubbleFirst ? RTE_BUBBLE_GAP : 0);
    LONG bottom = p->y + p->height - (p->bubbleLast ? RTE_BUBBLE_GAP : 0);
    LONG pen;
    LONG side = rteBubbleSide(data, p);
    LONG bx0 = t->ox + p->bubbleX, bx1 = t->ox + p->bubbleX + p->bubbleW - 1;
    LONG y;
    LONG bandTop = t->docTop, bandBot = t->docTop + t->height;

    if (p->bubble == 0 || p->bubble > data->numBubbleStyles)
        return;
    pen = data->bubbleBgPen[p->bubble - 1];
    if (bandBot <= top || bandTop >= bottom)
        return;
    /* rows inside the band that belong to the bubble */
    for (y = RTE_MAX(top, bandTop); y < RTE_MIN(bottom, bandBot);) {
        LONG fromTop = y - top;
        LONG fromBot = bottom - 1 - y;
        LONG inset = 0;
        LONG rowsSame;
        LONG ty = t->oy + (y - bandTop);
        BOOL tail = FALSE;
        if (p->bubbleFirst && fromTop < RTE_BUBBLE_RADIUS) {
            inset = rteCornerInset(fromTop);
            rowsSame = 1;
        } else if (p->bubbleLast && fromBot < RTE_BUBBLE_RADIUS) {
            inset = rteCornerInset(fromBot);
            rowsSame = 1;
            tail = TRUE;
        } else {
            LONG limit = RTE_MIN(bottom, bandBot);
            if (p->bubbleLast)
                limit = RTE_MIN(limit, bottom - RTE_BUBBLE_RADIUS);
            rowsSame = RTE_MAX(limit - y, 1);
        }
        if (tail && side != MUIV_BestTextEditor_Bubble_Center &&
            side != MUIV_BestTextEditor_Bubble_Full) {
            /* The corner on the tail side stays square and a small wedge
             * points out of the bubble, like a speech bubble */
            LONG wedge = RTE_BUBBLE_TAIL - fromBot;
            if (wedge < 0)
                wedge = 0;
            if (fromBot <= 1)
                wedge = RTE_BUBBLE_TAIL - 1 + (1 - fromBot);
            if (side == MUIV_BestTextEditor_Bubble_Right) {
                rteFill(t, pen, bx0 + inset, ty, bx1 + wedge, ty);
            } else {
                rteFill(t, pen, bx0 - wedge, ty, bx1 - inset, ty);
            }
        } else {
            rteFill(t, pen, bx0 + inset, ty, bx1 - inset, ty + rowsSame - 1);
        }
        y += rowsSame;
    }
}

static void rteDrawSeparator(struct RTEData *data, struct RTETarget *t,
                             LONG y, LONG x0, LONG x1, BOOL thick) {
    rteFill(t, data->pens[RTE_PEN_SHADOW], x0, y, x1, y + (thick ? 1 : 0));
    rteFill(t, data->pens[RTE_PEN_SHINE], x0, y + (thick ? 2 : 1), x1,
            y + (thick ? 3 : 1));
}

/** Decorations behind the text: code blocks, quotes, rules */
static void rteDrawBlockDeco(struct RTEData *data, struct RTETarget *t,
                             struct RTEPara *p, struct RTELine *l) {
    LONG cl, cr;
    BOOL md = (p->flags & RTEP_MARKDOWN) != 0;
    rteContentBox(data, p, &cl, &cr);

    if (md && (p->block == RTE_B_CODE || p->block == RTE_B_FENCE_OPEN ||
               p->block == RTE_B_FENCE_CLOSE)) {
        LONG y0 = t->oy, y1 = t->oy + t->height - 1;
        if (l == NULL) {
            /* padding bands of a code paragraph are not part of the box */
            return;
        }
        if (p->block == RTE_B_FENCE_OPEN && data->hideMarkup)
            y0 += 1;
        if (p->block == RTE_B_FENCE_CLOSE && data->hideMarkup)
            y1 -= 1;
        rteFill(t, rteCodeBgPen(data, p), t->ox + cl, y0, t->ox + cr - 1, y1);
        return;
    }
    if (md && p->block == RTE_B_QUOTE && l != NULL) {
        UBYTE k;
        for (k = 0; k < p->level; k++) {
            LONG x = t->ox + cl + k * RTE_QUOTE_INDENT + 1;
            rteFill(t, data->pens[RTE_PEN_QUOTE], x, t->oy, x + 2,
                    t->oy + t->height - 1);
        }
    }
    if (md && p->block == RTE_B_RULE && l != NULL) {
        LONG y = t->oy + t->height / 2;
        rteFill(t, data->pens[RTE_PEN_RULE], t->ox + cl, y, t->ox + cr - 1,
                y);
    }
    if (md && p->block == RTE_B_TABLE_RULE && l != NULL && data->hideMarkup) {
        LONG y = t->oy + t->height / 2;
        rteFill(t, data->pens[RTE_PEN_RULE], t->ox + cl, y,
                t->ox + cl + p->contentW, y);
    }
    if (md && p->block == RTE_B_HEADING && l == NULL &&
        t->docTop >= p->y + p->height - p->padBottom && p->level <= 2) {
        /* underline below level 1 and 2 headings */
        LONG y = t->oy + (p->level == 1 ? 2 : 0);
        rteFill(t, data->pens[RTE_PEN_RULE], t->ox + cl, y, t->ox + cr - 1, y);
    }
    if (p->separator != 0 && l != NULL) {
        BOOL thick = (p->separator & LNSF_Thick) != 0;
        LONG x0 = t->ox + cl, x1 = t->ox + cr - 1;
        if ((p->separator & LNSF_Top) && (l->flags & RTEL_FIRST))
            rteDrawSeparator(data, t, t->oy, x0, x1, thick);
        if ((p->separator & LNSF_Middle) && (l->flags & RTEL_FIRST))
            rteDrawSeparator(data, t, t->oy + t->height / 2 - 1, x0, x1, thick);
        if ((p->separator & LNSF_Bottom) && (l->flags & RTEL_LAST))
            rteDrawSeparator(data, t, t->oy + t->height - (thick ? 4 : 2), x0,
                             x1, thick);
    }
}

/** Bullet, checkbox or nothing in front of a list item */
static void rteDrawListMarker(struct RTEData *data, struct RTETarget *t,
                              struct RTEPara *p, struct RTELine *l,
                              LONG left, LONG pen) {
    LONG unit, cx, cy, r, row;
    if (!(p->flags & RTEP_MARKDOWN) || p->block != RTE_B_LIST ||
        !data->hideMarkup || p->listNumber != 0 || !(l->flags & RTEL_FIRST))
        return;
    unit = RTE_MAX(data->widths[0]['m'] * 2, 12);
    cx = t->ox + left - unit / 2 - 1;
    cy = t->oy + l->baseline - (data->fontBaseline * 2) / 5;
    if (p->taskState != 0) {
        LONG s = RTE_MAX(data->fontBaseline - 2, 6);
        LONG x0 = cx - s / 2, y0 = cy - s / 2;
        rteFill(t, pen, x0, y0, x0 + s - 1, y0);
        rteFill(t, pen, x0, y0 + s - 1, x0 + s - 1, y0 + s - 1);
        rteFill(t, pen, x0, y0, x0, y0 + s - 1);
        rteFill(t, pen, x0 + s - 1, y0, x0 + s - 1, y0 + s - 1);
        if (p->taskState == 2)
            rteFill(t, pen, x0 + 2, y0 + 2, x0 + s - 3, y0 + s - 3);
        return;
    }
    r = RTE_MAX(data->fontHeight / 6, 2);
    if (p->level > 0)
        r = RTE_MAX(r - 1, 1);
    for (row = -r; row <= r; row++) {
        LONG half = r;
        if (row == -r || row == r)
            half = r - 1 > 0 ? r - 1 : 0;
        if (p->level % 2 == 1 && row != -r && row != r && r > 1) {
            /* hollow bullet for the second level */
            rteFill(t, pen, cx - half, cy + row, cx - half, cy + row);
            rteFill(t, pen, cx + half, cy + row, cx + half, cy + row);
        } else {
            rteFill(t, pen, cx - half, cy + row, cx + half, cy + row);
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Text                                                                      */
/* ------------------------------------------------------------------------ */

static void rteDrawImageAt(struct RTEData *data, struct RTETarget *t,
                           struct RTEPara *p, ULONG offset, LONG x,
                           LONG baseline) {
    ULONG s, e, us, ue;
    struct RTEImage *img;
    LONG dx, dy, sx = 0, sy = 0, w, h;
    if (!rteFindSpan(p, offset, TRUE, &s, &e, &us, &ue))
        return;
    img = rteGetImage(data, (STRPTR)p->text + us, ue - us, p->wrapWidth);
    if (img == NULL || img->bitmap == NULL)
        return;
    dx = x;
    dy = baseline - img->height;
    w = img->width;
    h = img->height;
    if (dx < t->clipL) {
        sx = t->clipL - dx;
        w -= sx;
        dx = t->clipL;
    }
    if (dy < t->clipT) {
        sy = t->clipT - dy;
        h -= sy;
        dy = t->clipT;
    }
    if (dx + w - 1 > t->clipR)
        w = t->clipR - dx + 1;
    if (dy + h - 1 > t->clipB)
        h = t->clipB - dy + 1;
    if (w > 0 && h > 0)
        BltBitMapRastPort(img->bitmap, sx, sy, t->rp, dx, dy, w, h, 0xc0);
    (void)data;
}

/** Selection range of a paragraph, or FALSE when it has none */
static BOOL rteSelectionInPara(struct RTEData *data, ULONG para, ULONG *s,
                               ULONG *e, BOOL *toEnd) {
    struct RTEPos from, to;
    if (!rteHasSelection(data))
        return FALSE;
    rteOrderedSelection(data, &from, &to);
    if (para < from.para || para > to.para)
        return FALSE;
    *s = para == from.para ? from.offset : 0;
    *e = para == to.para ? to.offset : data->paras[para].len;
    *toEnd = para != to.para;
    return TRUE;
}

/* What is drawn for byte j of an annotated paragraph (pure ASCII
 * paragraphs have no glyph array: their bytes are drawn as they are) */
#define RTE_GLYPH_FAST(p, j)                                                   \
    ((p)->glyphs != NULL && (p)->glyphLen == (p)->len ? (p)->glyphs[j]         \
                                                      : (UWORD)(p)->text[j])

#define RTE_DRAW_BUF 128

/** Width of a text glyph in a run, from the run's width table */
static LONG rteRunGlyphWidth(UWORD *tab, UWORD g) {
    switch (RTE_G_KIND(g)) {
    case RTE_G_GLYPH:
        return tab[RTE_G_VALUE(g) & 0xff];
    case RTE_G_TRANS: {
        UBYTE b[8];
        ULONG n = rteGlyphBytes(g, b), i;
        LONG w = 0;
        for (i = 0; i < n; i++)
            w += tab[b[i]];
        return w;
    }
    }
    return 0;
}

/** The box drawn for a character that no font or emoji covers */
static void rteDrawBox(struct RTETarget *t, LONG pen, LONG x, LONG baseY,
                       LONG w, LONG h) {
    LONG x0 = x + 1, x1 = x + w - 2, y0 = baseY - h, y1 = baseY - 1;
    if (x1 <= x0 || y1 <= y0)
        return;
    rteFill(t, pen, x0, y0, x1, y0);
    rteFill(t, pen, x0, y1, x1, y1);
    rteFill(t, pen, x0, y0, x0, y1);
    rteFill(t, pen, x1, y0, x1, y1);
}

static void rteDrawLineText(struct RTEData *data, Object *obj,
                            struct RTETarget *t, ULONG paraIndex,
                            struct RTEPara *p, struct RTELine *l) {
    struct RastPort *rp = t->rp;
    LONG left, extra, rem;
    LONG x;
    ULONG k, end = l->start + l->len;
    ULONG spaces = 0;
    LONG baseY = t->oy + l->baseline;
    ULONG selS = 0, selE = 0;
    BOOL selToEnd = FALSE;
    BOOL hasSel = rteSelectionInPara(data, paraIndex, &selS, &selE, &selToEnd);
    LONG normalPen = data->pens[RTE_PEN_TEXT];
    BOOL highlight = (p->flags & RTEP_HIGHLIGHT) != 0;
    LONG markBg = data->pens[RTE_PEN_MARK];
    LONG markText = data->pens[RTE_PEN_MARKTEXT];
    struct TextFont *curFont = NULL;
    ULONG curSoft = 0xffff;
    BOOL inCodeBox = (p->flags & RTEP_MARKDOWN) &&
                     (p->block == RTE_B_CODE || p->block == RTE_B_FENCE_OPEN ||
                      p->block == RTE_B_FENCE_CLOSE);

    if (p->bubble != 0 && p->bubble <= data->numBubbleStyles)
        normalPen = data->bubbleTextPen[p->bubble - 1];
    if (!data->active && !data->readOnly) {
        markBg = data->pens[RTE_PEN_DIM];
        markText = data->pens[RTE_PEN_SHINE];
    }

    rteLineGeometry(data, p, l, &left, &extra, &rem);
    x = t->ox + left;

    /* Selection background, including the line break when selected */
    if (hasSel) {
        LONG sx0 = -1, sx1 = -1;
        LONG cx = x;
        ULONG sp = 0;
        for (k = l->start; k <= end; k++) {
            if (k == selS && sx0 < 0)
                sx0 = cx;
            if (k == selE) {
                sx1 = cx;
                break;
            }
            if (k == end)
                break;
            cx += rteCharWidth(data, p, k, cx - x - (t->ox + left - x));
            if (extra > 0 && p->text[k] == ' ') {
                cx += extra + (sp < (ULONG)rem ? 1 : 0);
                sp++;
            }
        }
        if (selS < l->start)
            sx0 = x;
        if (sx0 >= 0) {
            if (sx1 < 0) {
                if ((l->flags & RTEL_LAST) && selToEnd)
                    sx1 = cx + data->spaceWidth;
                else if (!(l->flags & RTEL_LAST) || selE > end)
                    sx1 = cx;
                else
                    sx1 = cx;
            }
            if (sx1 > sx0)
                rteFill(t, markBg, sx0, t->oy, sx1 - 1, t->oy + t->height - 1);
        }
    }

    SetDrMd(rp, JAM1);
    k = l->start;
    while (k < end) {
        UWORD style = rteStyleAt(p, k);
        UWORD attr = rteAttrAt(p, k);
        UWORD glyph = RTE_GLYPH_FAST(p, k);
        UWORD kind = RTE_G_KIND(glyph);
        BOOL selected = hasSel && k >= selS && k < selE;
        ULONG runEnd = k + 1;
        LONG runW;
        LONG pen;
        struct TextFont *font;
        ULONG soft = 0;
        LONG runX = x;
        LONG fontBase;
        UWORD *tab;
        ULONG j;

        if (style & RTE_S_HIDDEN) {
            k++;
            continue;
        }
        if (style & RTE_S_IMAGE) {
            LONG w = rteCharWidth(data, p, k, x - (t->ox + left));
            rteDrawImageAt(data, t, p, k, x, baseY);
            x += w;
            k++;
            continue;
        }
        if (kind == RTE_G_CONT) {
            /* the rest of a cluster whose first byte was hidden */
            k++;
            continue;
        }
        if (p->text[k] == '\t') {
            x += rteCharWidth(data, p, k, x - (t->ox + left));
            k++;
            continue;
        }
        /* extend the run over characters that render the same way; the
         * later bytes of a character always go with its first */
        if (kind != RTE_G_EMOJI && kind != RTE_G_MISSING) {
            while (runEnd < end) {
                UWORD g2 = RTE_GLYPH_FAST(p, runEnd);
                UWORD s2;
                if (RTE_G_KIND(g2) == RTE_G_CONT) {
                    runEnd++;
                    continue;
                }
                s2 = rteStyleAt(p, runEnd);
                if (s2 != style || rteAttrAt(p, runEnd) != attr ||
                    (hasSel && (runEnd >= selS && runEnd < selE) != selected) ||
                    p->text[runEnd] == '\t' ||
                    RTE_G_KIND(g2) == RTE_G_EMOJI ||
                    RTE_G_KIND(g2) == RTE_G_MISSING ||
                    (extra > 0 && p->text[runEnd - 1] == ' '))
                    break;
                runEnd++;
            }
        } else {
            runEnd = rteClusterEnd(p, k);
            if (runEnd > end)
                runEnd = end;
        }

        font = (style & (RTE_S_CODE | RTE_S_CODEFONT)) ? data->codeFont
                                                         : data->font;
        fontBase = font == data->codeFont ? data->codeBaseline
                                          : data->fontBaseline;
        if ((style & RTE_S_BOLD) && !(font->tf_Style & FSF_BOLD))
            soft |= FSF_BOLD;
        if (style & RTE_S_ITALIC)
            soft |= FSF_ITALIC;
        if ((style & RTE_S_UNDERLINE) ||
            ((style & RTE_S_LINK) && !(style & RTE_S_MARKUP)))
            soft |= FSF_UNDERLINED;

        /* pen priority */
        if (selected)
            pen = markText;
        else if (RTE_A_COLOR(attr) != 0)
            pen = rteResolveColor(data, obj, RTE_A_COLOR(attr));
        else if (style & RTE_S_MARKUP)
            pen = p->bubble != 0 ? normalPen : data->pens[RTE_PEN_DIM];
        else if (style & RTE_S_LINK)
            pen = p->bubble != 0 ? normalPen : data->pens[RTE_PEN_LINK];
        else if (inCodeBox)
            pen = p->bubble != 0 ? normalPen : data->pens[RTE_PEN_CODETEXT];
        else if (style & RTE_S_QUOTE)
            pen = p->bubble != 0 ? normalPen : data->pens[RTE_PEN_DIM];
        else if (highlight)
            pen = data->pens[RTE_PEN_HIGHLIGHT];
        else
            pen = normalPen;

        if (kind == RTE_G_EMOJI || kind == RTE_G_MISSING) {
            /* a colour emoji, or a box for a character nothing can draw */
            LONG w = rteGlyphWidth(data, style, glyph);
            LONG size = rteEmojiSize(data, style);
            LONG top = baseY - fontBase;
            BOOL drawn = FALSE;
            if ((style & RTE_S_CODE) && !selected)
                rteFill(t, rteCodeBgPen(data, p), x - 1, top - 1, x + w,
                        top + font->tf_YSize);
            if (kind == RTE_G_EMOJI)
                drawn = rteDrawEmoji(data, obj, rp, RTE_G_VALUE(glyph), size,
                                     x, top, t->clipL, t->clipT, t->clipR,
                                     t->clipB);
            if (!drawn)
                rteDrawBox(t, pen, x, baseY, w,
                           RTE_MAX(fontBase - 1, 4));
            if (soft & FSF_UNDERLINED)
                rteFill(t, pen, x, baseY + 1, x + w - 1, baseY + 1);
            x += w;
            k = runEnd;
            continue;
        }

        tab = data->widths[(font == data->codeFont ? 2 : 0) +
                           ((style & RTE_S_BOLD) ? 1 : 0)];
        runW = 0;
        for (j = k; j < runEnd; j++)
            runW += rteRunGlyphWidth(tab, RTE_GLYPH_FAST(p, j));

        if ((style & RTE_S_CODE) && !selected) {
            rteFill(t, rteCodeBgPen(data, p), runX - 1,
                    baseY - fontBase - 1, runX + runW,
                    baseY - fontBase + font->tf_YSize);
        }

        if (font != curFont) {
            SetFont(rp, font);
            curFont = font;
            curSoft = 0xffff;
        }
        if (soft != curSoft) {
            SetSoftStyle(rp, soft, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
            curSoft = soft;
        }
        SetAPen(rp, pen);

        /* The run's characters in the system codeset, clipped to what the
         * target can hold and drawn in pieces of a small buffer */
        {
            UBYTE buf[RTE_DRAW_BUF];
            ULONG nb = 0;
            LONG cx = runX, bx = runX;
            LONG limitL = t->buffered ? t->clipL - RTE_DB_MARGIN + 2 : t->clipL;
            LONG limitR = t->buffered ? t->clipR + RTE_DB_MARGIN - 8 : t->clipR;
            BOOL started = FALSE, done = FALSE;
            for (j = k; j < runEnd && !done; j++) {
                UBYTE b[8];
                ULONG n, q;
                UWORD g = RTE_GLYPH_FAST(p, j);
                if (RTE_G_KIND(g) == RTE_G_CONT)
                    continue;
                n = rteGlyphBytes(g, b);
                for (q = 0; q < n; q++) {
                    LONG w = tab[b[q]];
                    if (!started) {
                        if (cx < limitL) {
                            cx += w;
                            continue;
                        }
                        started = TRUE;
                        bx = cx;
                    }
                    if (cx + w > limitR) {
                        done = TRUE;
                        break;
                    }
                    if (nb == sizeof(buf)) {
                        if (bx <= t->clipR) {
                            Move(rp, bx, baseY);
                            Text(rp, (STRPTR)buf, nb);
                        }
                        bx = cx;
                        nb = 0;
                    }
                    buf[nb++] = b[q];
                    cx += w;
                }
            }
            if (nb > 0 && bx <= t->clipR) {
                Move(rp, bx, baseY);
                Text(rp, (STRPTR)buf, nb);
            }
        }
        if (style & RTE_S_STRIKE) {
            LONG sy = baseY - fontBase / 3;
            rteFill(t, pen, runX, sy, runX + runW - 1, sy);
        }
        x += runW;
        if (extra > 0 && p->text[runEnd - 1] == ' ') {
            x += extra + (spaces < (ULONG)rem ? 1 : 0);
            spaces++;
        }
        k = runEnd;
    }
    SetSoftStyle(rp, 0, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);

    rteDrawListMarker(data, t, p, l, left, normalPen);

    /* Cursor */
    if (!data->readOnly && data->active && data->cursor.para == paraIndex &&
        rteLineOfOffset(p, data->cursor.offset) ==
            (ULONG)(l - p->lines)) {
        LONG cx = t->ox + rteOffsetToX(data, p, l, data->cursor.offset);
        LONG pen = data->pens[RTE_PEN_CURSOR];
        if (data->overwrite) {
            LONG w = data->cursor.offset < p->len
                         ? rteCharWidth(data, p, data->cursor.offset, 0)
                         : data->spaceWidth;
            rteFill(t, pen, cx, t->oy + l->height - 2, cx + RTE_MAX(w, 2) - 1,
                    t->oy + l->height - 1);
        } else {
            rteFill(t, pen, cx, t->oy, cx + 1, t->oy + l->height - 1);
        }
    }
}


/* ------------------------------------------------------------------------ */
/* Matching the real background                                              */
/* ------------------------------------------------------------------------ */

/** Colour of a pixel of a rastport as 0x00RRGGBB */
static BOOL rteReadPixelRGB(Object *obj, struct RastPort *rp, LONG x, LONG y,
                            ULONG *rgb) {
#if defined(__AMIGAOS4__)
    (void)obj;
    *rgb = ReadPixelColor(rp, x, y) & 0xffffff;
    return TRUE;
#else
    struct Library *CyberGfxBase = OpenLibrary("cybergraphics.library", 40);
    BOOL done = FALSE;
    if (CyberGfxBase != NULL) {
        if (rp->BitMap != NULL &&
            GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX)) {
            *rgb = ReadRGBPixel(rp, x, y) & 0xffffff;
            done = TRUE;
        }
        CloseLibrary(CyberGfxBase);
    }
    if (!done) {
        LONG pen = ReadPixel(rp, x, y);
        if (pen < 0)
            return FALSE;
        rtePenRGB(_screen(obj)->ViewPort.ColorMap, pen, rgb);
    }
    return TRUE;
#endif
}

static void rteSetDerivedPen(struct RTEData *data, struct ColorMap *cm,
                             UWORD slot, ULONG rgb) {
    BOOL alloc;
    LONG pen = rteBestPen(cm, rgb, &alloc);
    if (pen < 0)
        return;
    if (data->penAlloc[slot])
        ReleasePen(cm, data->pens[slot]);
    data->pens[slot] = pen;
    data->penAlloc[slot] = alloc;
}

/**
 * MUI themes do not always pair the text pen with every background (a dark
 * theme may keep black text for white string gadgets but give lists a black
 * background). So the background the object really has is looked at once,
 * and the text and accent colours are chosen to read well on it.
 * @return TRUE if pens were changed
 */
static BOOL rteAdaptToBackground(struct RTEData *data, Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    struct RastPort *saved;
    ULONG bg, fg;
    LONG diff;

    data->bgSampled = TRUE;
    if (data->dbBitMap == NULL)
        return FALSE;
    saved = muiRenderInfo(obj)->mri_RastPort;
    muiRenderInfo(obj)->mri_RastPort = &data->dbRP;
    DoMethod(obj, MUIM_DrawBackground, RTE_DB_MARGIN, 0, 8, 8,
             data->viewLeft, data->viewTop, 0);
    muiRenderInfo(obj)->mri_RastPort = saved;
    if (!rteReadPixelRGB(obj, &data->dbRP, RTE_DB_MARGIN + 3, 3, &bg))
        return FALSE;
    data->bgRGB = bg;
    rtePenRGB(cm, data->pens[RTE_PEN_TEXT], &fg);
    diff = (LONG)rteLuminance(fg) - (LONG)rteLuminance(bg);
    if (diff < 0)
        diff = -diff;
    if (diff < 100) {
        fg = rteLuminance(bg) > 128 ? 0x000000 : 0xffffff;
        rteSetDerivedPen(data, cm, RTE_PEN_TEXT, fg);
        data->pens[RTE_PEN_CODETEXT] = data->pens[RTE_PEN_TEXT];
    }
    rteSetDerivedPen(data, cm, RTE_PEN_DIM, rteMix(bg, fg, 50));
    rteSetDerivedPen(data, cm, RTE_PEN_CODEBG, rteMix(bg, fg, 12));
    rteSetDerivedPen(data, cm, RTE_PEN_QUOTE, rteMix(bg, fg, 35));
    rteSetDerivedPen(data, cm, RTE_PEN_RULE, rteMix(bg, fg, 25));
    rteSetDerivedPen(data, cm, RTE_PEN_LINK,
                     rteLuminance(bg) > 110 ? 0x1a56c8 : 0x7fb2ff);
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* Bands                                                                     */
/* ------------------------------------------------------------------------ */

static void rteDrawBackground(Object *obj, struct RTEData *data,
                              struct RTETarget *t, LONG winX, LONG winY,
                              LONG w, LONG h) {
    if (t->buffered) {
        struct RastPort *saved = muiRenderInfo(obj)->mri_RastPort;
        muiRenderInfo(obj)->mri_RastPort = t->rp;
        DoMethod(obj, MUIM_DrawBackground, t->clipL, t->oy, w, h, winX, winY,
                 0);
        muiRenderInfo(obj)->mri_RastPort = saved;
    } else {
        DoMethod(obj, MUIM_DrawBackground, winX, winY, w, h, winX, winY, 0);
    }
    (void)data;
}

/**
 * Render one band of a paragraph. line is NULL for the padding bands.
 * docTop/height is the band, visTop/visBottom the part to show.
 */
static void rteRenderBand(struct RTEData *data, Object *obj, ULONG paraIndex,
                          struct RTEPara *p, struct RTELine *l, LONG docTop,
                          LONG height, LONG visTop, LONG visBottom) {
    struct RTETarget t;
    LONG winY = data->viewTop + (docTop - data->scrollY);
    LONG winX = data->viewLeft;
    LONG w = data->viewWidth;
    LONG srcY, showH;

    if (height <= 0)
        return;
    memset(&t, 0, sizeof(t));
    t.docTop = docTop;
    t.height = height;
    srcY = visTop - docTop;
    showH = visBottom - visTop;
    if (showH <= 0)
        return;

    if (rteEnsureDoubleBuffer(data, obj, w, height)) {
        t.rp = &data->dbRP;
        t.buffered = TRUE;
        t.clipL = RTE_DB_MARGIN;
        t.clipR = RTE_DB_MARGIN + w - 1;
        t.clipT = 0;
        t.clipB = height - 1;
        t.oy = 0;
        t.ox = RTE_DB_MARGIN + RTE_PAD_X - data->scrollX;
        rteDrawBackground(obj, data, &t, winX, winY, w, height);
    } else {
        t.rp = _rp(obj);
        t.buffered = FALSE;
        t.clipL = winX;
        t.clipR = winX + w - 1;
        t.clipT = data->viewTop + (visTop - data->scrollY);
        t.clipB = t.clipT + showH - 1;
        t.oy = winY;
        t.ox = winX + RTE_PAD_X - data->scrollX;
        rteDrawBackground(obj, data, &t, winX, t.clipT, w, showH);
    }

    if (p != NULL) {
        APTR clip = NULL;
        if (!t.buffered)
            clip = MUI_AddClipping(muiRenderInfo(obj), t.clipL, t.clipT, w,
                                   showH);
        rteDrawBubble(data, &t, p);
        rteDrawBlockDeco(data, &t, p, l);
        if (l != NULL)
            rteDrawLineText(data, obj, &t, paraIndex, p, l);
        if (!t.buffered)
            MUI_RemoveClipping(muiRenderInfo(obj), clip);
    }

    if (t.buffered) {
        BltBitMapRastPort(data->dbBitMap, RTE_DB_MARGIN, srcY, _rp(obj), winX,
                          winY + srcY, w, showH, 0xc0);
    }
}

static void rteDrawPlaceholder(struct RTEData *data, Object *obj) {
    struct RastPort *rp = _rp(obj);
    LONG x = data->viewLeft + RTE_PAD_X;
    LONG y = data->viewTop + RTE_PAD_Y + data->fontBaseline - data->scrollY;
    STRPTR text = data->inactiveContents;
    STRPTR converted = NULL;
    ULONG len;
    struct TextExtent te;
    ULONG fit;
    APTR clip;
    if (data->utf8 && (converted = rteFromUTF8(text, -1, NULL)) != NULL)
        text = converted;
    len = strlen(text);
    clip = MUI_AddClipping(muiRenderInfo(obj), data->viewLeft,
                                data->viewTop, data->viewWidth,
                                data->viewHeight);
    SetFont(rp, data->font);
    SetSoftStyle(rp, FSF_ITALIC, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
    SetAPen(rp, data->pens[RTE_PEN_DIM]);
    SetDrMd(rp, JAM1);
    fit = TextFit(rp, text, len, &te, NULL, 1,
                  RTE_MAX(data->viewWidth - 2 * RTE_PAD_X, 1),
                  data->fontHeight + 1);
    Move(rp, x, y);
    Text(rp, text, fit);
    SetSoftStyle(rp, 0, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
    MUI_RemoveClipping(muiRenderInfo(obj), clip);
    if (converted != NULL)
        FreeVec(converted);
}

/**
 * Bring the visible part of the object up to date
 */
void rteDrawUpdate(struct RTEData *data, Object *obj, BOOL all) {
    LONG viewTopDoc, viewBotDoc, top, bottom;
    ULONG i;
    struct RastPort *rp = _rp(obj);
    BOOL placeholder;

    if (!data->shown || data->viewWidth <= 0 || data->viewHeight <= 0)
        return;
    data->inDraw = TRUE;
    rteEnsureLayout(data, obj);
    if (!data->bgSampled &&
        rteEnsureDoubleBuffer(data, obj, data->viewWidth, data->lineHeight) &&
        rteAdaptToBackground(data, obj))
        all = TRUE;

    viewTopDoc = data->scrollY;
    viewBotDoc = data->scrollY + data->viewHeight;

    if (all || data->drawAll) {
        top = viewTopDoc;
        bottom = viewBotDoc;
    } else {
        if (data->scrollDelta != 0) {
            LONG d = data->scrollDelta;
            if (d > -data->viewHeight && d < data->viewHeight &&
                rp->Layer != NULL) {
                ScrollRasterBF(rp, 0, d, data->viewLeft, data->viewTop,
                               data->viewLeft + data->viewWidth - 1,
                               data->viewTop + data->viewHeight - 1);
                if (rp->Layer->Flags & LAYERREFRESH) {
                    rteInvalidateRange(data, viewTopDoc, viewBotDoc);
                } else if (d > 0) {
                    rteInvalidateRange(data, viewBotDoc - d, viewBotDoc);
                } else {
                    rteInvalidateRange(data, viewTopDoc, viewTopDoc - d);
                }
            } else {
                rteInvalidateRange(data, viewTopDoc, viewBotDoc);
            }
        }
        top = RTE_MAX(data->dirtyTop, viewTopDoc);
        bottom = RTE_MIN(data->dirtyBottom, viewBotDoc);
    }
    data->scrollDelta = 0;
    data->drawAll = FALSE;
    data->dirtyTop = 0x3fffffff;
    data->dirtyBottom = -1;

    placeholder = data->inactiveContents != NULL && !data->active &&
                  data->totalLength == 0 && data->inactiveContents[0] != '\0';

    if (bottom > top) {
        LONG docEnd = data->numParas > 0
                          ? data->paras[data->numParas - 1].y +
                                data->paras[data->numParas - 1].height
                          : RTE_PAD_Y;
        /* above the first paragraph */
        if (top < RTE_PAD_Y)
            rteRenderBand(data, obj, 0, NULL, NULL, 0, RTE_PAD_Y,
                          RTE_MAX(top, 0), RTE_MIN(bottom, RTE_PAD_Y));
        for (i = rteParaAtY(data, top); i < data->numParas; i++) {
            struct RTEPara *p = &data->paras[i];
            LONG y = p->y;
            UWORD n;
            if (y >= bottom)
                break;
            if (y + p->height <= top)
                continue;
            if (p->padTop > 0 && y + p->padTop > top && y < bottom)
                rteRenderBand(data, obj, i, p, NULL, y, p->padTop,
                              RTE_MAX(top, y), RTE_MIN(bottom, y + p->padTop));
            y += p->padTop;
            for (n = 0; n < p->numLines; n++) {
                struct RTELine *l = &p->lines[n];
                LONG ly = y + l->y;
                if (ly >= bottom)
                    break;
                if (ly + l->height > top)
                    rteRenderBand(data, obj, i, p, l, ly, l->height,
                                  RTE_MAX(top, ly),
                                  RTE_MIN(bottom, ly + l->height));
            }
            y += p->linesHeight;
            if (p->padBottom > 0 && y < bottom && y + p->padBottom > top)
                rteRenderBand(data, obj, i, p, NULL, y, p->padBottom,
                              RTE_MAX(top, y),
                              RTE_MIN(bottom, y + p->padBottom));
        }
        /* below the last paragraph */
        if (bottom > docEnd) {
            LONG from = RTE_MAX(top, docEnd);
            DoMethod(obj, MUIM_DrawBackground, data->viewLeft,
                     data->viewTop + (from - data->scrollY), data->viewWidth,
                     bottom - from, data->viewLeft,
                     data->viewTop + (from - data->scrollY), 0);
        }
        if (placeholder)
            rteDrawPlaceholder(data, obj);
    }
    data->inDraw = FALSE;
}
