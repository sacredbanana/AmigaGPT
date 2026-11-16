/*
 * BestTextEditor - layout
 *
 * Font metrics, Markdown parsing, line breaking, vertical placement of the
 * paragraphs, bubble geometry, inline images and the coordinate conversions
 * used for hit testing.
 */
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <proto/datatypes.h>
#include <proto/diskfont.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <string.h>
#include "BestTextEditorPrivate.h"

#ifndef PMODE_V43
#define PMODE_V43 1
#endif

#define RTE_NOWRAP_WIDTH 0x3fffffff

/* ------------------------------------------------------------------------ */
/* Fonts                                                                     */
/* ------------------------------------------------------------------------ */

static void rteFillWidths(struct TextFont *font, UWORD *normal, UWORD *bold) {
    struct RastPort rp;
    UWORD c;
    UBYTE ch;
    InitRastPort(&rp);
    SetFont(&rp, font);
    SetSoftStyle(&rp, 0, FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED);
    for (c = 0; c < 256; c++) {
        ch = (UBYTE)c;
        normal[c] = TextLength(&rp, &ch, 1);
    }
    SetSoftStyle(&rp, FSF_BOLD, FSF_BOLD);
    for (c = 0; c < 256; c++) {
        ch = (UBYTE)c;
        bold[c] = TextLength(&rp, &ch, 1);
    }
}

/**
 * Open a font from a "name/size" spec as a font Popasl writes it
 * @return the font, or NULL when the spec is empty or the font is missing
 */
static struct TextFont *rteOpenFontSpec(CONST_STRPTR spec) {
    UBYTE name[72];
    struct TextAttr ta;
    ULONG len = 0, size = 0;
    CONST_STRPTR s = spec;

    if (s == NULL)
        return NULL;
    while (*s != '\0' && *s != '/' && len < sizeof(name) - 6)
        name[len++] = *s++;
    if (*s != '/' || len == 0)
        return NULL;
    for (s++; *s >= '0' && *s <= '9'; s++)
        size = size * 10 + (*s - '0');
    if (size == 0 || size > 255)
        return NULL;
    if (len < 5 || strncmp((const char *)name + len - 5, ".font", 5) != 0) {
        memcpy(name + len, ".font", 5);
        len += 5;
    }
    name[len] = '\0';
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = (UWORD)size;
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    if (DiskfontBase != NULL)
        return OpenDiskFont(&ta);
    return OpenFont(&ta);
}

void rteCloseConfigFonts(struct RTEData *data) {
    if (data->cfgFont != NULL)
        CloseFont(data->cfgFont);
    if (data->cfgCode != NULL)
        CloseFont(data->cfgCode);
    data->cfgFont = NULL;
    data->cfgCode = NULL;
}

void rteSetupFonts(struct RTEData *data, Object *obj) {
    struct TextFont *font = _font(obj);

    if (data->ownCodeFont && data->codeFont != NULL)
        CloseFont(data->codeFont);
    data->ownCodeFont = FALSE;
    data->codeFont = NULL;
    rteCloseConfigFonts(data);

    /* The preferences font, unless the application chose one */
    if (!data->appFont && !data->fixedFont && data->cfgTextFont[0] != '\0') {
        data->cfgFont = rteOpenFontSpec((CONST_STRPTR)data->cfgTextFont);
        if (data->cfgFont != NULL)
            font = data->cfgFont;
    }
    if (data->userCodeFont == NULL && data->cfgCodeFont[0] != '\0')
        data->cfgCode = rteOpenFontSpec((CONST_STRPTR)data->cfgCodeFont);

    if (data->userCodeFont != NULL) {
        data->codeFont = data->userCodeFont;
    } else if (data->cfgCode != NULL) {
        data->codeFont = data->cfgCode;
    } else if (font != NULL && (font->tf_Flags & FPF_PROPORTIONAL) == 0) {
        data->codeFont = font;
    } else {
        /* The system default font is always monospaced */
        struct TextFont *def = ((struct GfxBase *)GfxBase)->DefaultFont;
        if (def != NULL) {
            struct TextAttr ta;
            ta.ta_Name = def->tf_Message.mn_Node.ln_Name;
            ta.ta_YSize = def->tf_YSize;
            ta.ta_Style = 0;
            ta.ta_Flags = 0;
            data->codeFont = OpenFont(&ta);
            data->ownCodeFont = data->codeFont != NULL;
        }
        if (data->codeFont == NULL)
            data->codeFont = font;
    }
    if (data->fixedFont && data->codeFont != NULL)
        font = data->codeFont;
    data->font = font;

    rteFillWidths(data->font, data->widths[0], data->widths[1]);
    rteFillWidths(data->codeFont, data->widths[2], data->widths[3]);
    data->fontHeight = data->font->tf_YSize;
    data->fontBaseline = data->font->tf_Baseline;
    data->codeHeight = data->codeFont->tf_YSize;
    data->codeBaseline = data->codeFont->tf_Baseline;
    data->lineHeight = RTE_MAX(data->fontHeight, data->codeHeight);
    data->spaceWidth = data->widths[0][' '];
    if (data->spaceWidth == 0)
        data->spaceWidth = 4;
}

/* ------------------------------------------------------------------------ */
/* Styles                                                                    */
/* ------------------------------------------------------------------------ */

UWORD rteStyleAt(struct RTEPara *p, ULONG offset) {
    UWORD s = p->md != NULL && offset < p->len ? p->md[offset] : p->mdUniform;
    if (p->attrs != NULL && offset < p->len)
        s |= p->attrs[offset] & RTE_A_STYLEMASK;
    return s;
}

static UWORD *rteWidthTable(struct RTEData *data, UWORD style) {
    UWORD t = (style & (RTE_S_CODE | RTE_S_CODEFONT)) ? 2 : 0;
    if (style & RTE_S_BOLD)
        t++;
    return data->widths[t];
}

/** Indent unit for lists and quotes */
static LONG rteIndentUnit(struct RTEData *data) {
    LONG unit = data->widths[0]['m'] * 2;
    if (unit < 12)
        unit = 12;
    return unit;
}

/* ------------------------------------------------------------------------ */
/* Images                                                                    */
/* ------------------------------------------------------------------------ */

/* Opened once for the whole library in ClassInit() (library.c); NULL when
 * datatypes.library is missing, which only turns images off */
static BOOL rteOpenDataTypes(void) { return rteDataTypesBase != NULL; }

static void rteCloseDataTypes(void) {}

static void rteDisposeImage(struct RTEImage *img) {
    struct Library *DataTypesBase = rteDataTypesBase;
#if defined(__AMIGAOS4__)
    struct DataTypesIFace *IDataTypes = rteIDataTypes;
#endif
    if (img->ownBitmap && img->bitmap != NULL)
        FreeBitMap(img->bitmap);
    img->bitmap = NULL;
    img->ownBitmap = FALSE;
    if (img->dto != NULL && DataTypesBase != NULL) {
        DisposeDTObject(img->dto);
        rteCloseDataTypes();
    }
    img->dto = NULL;
}

void rteFreeImages(struct RTEData *data) {
    struct RTEImage *img;
    while ((img = (struct RTEImage *)RemHead((struct List *)&data->images))) {
        rteDisposeImage(img);
        if (img->source != NULL)
            FreeVec(img->source);
        FreeVec(img);
    }
}

/** Scale the datatype's bitmap so it fits maxWidth x maxImageHeight */
static void rteScaleImage(struct RTEData *data, struct RTEImage *img,
                          struct BitMap *src, UWORD srcW, UWORD srcH,
                          LONG maxWidth) {
    ULONG w = srcW, h = srcH;
    ULONG maxH = data->maxImageHeight > 0 ? data->maxImageHeight : 240;

    if (img->ownBitmap && img->bitmap != NULL)
        FreeBitMap(img->bitmap);
    img->ownBitmap = FALSE;
    img->bitmap = src;
    if (maxWidth < 16)
        maxWidth = 16;
    if (w > (ULONG)maxWidth) {
        h = h * (ULONG)maxWidth / w;
        w = (ULONG)maxWidth;
    }
    if (h > maxH) {
        w = w * maxH / h;
        h = maxH;
    }
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    if (w != srcW || h != srcH) {
        struct BitMap *dst =
            AllocBitMap(w, h, GetBitMapAttr(src, BMA_DEPTH), BMF_CLEAR, src);
        if (dst != NULL) {
            struct BitScaleArgs bsa;
            memset(&bsa, 0, sizeof(bsa));
            bsa.bsa_SrcWidth = srcW;
            bsa.bsa_SrcHeight = srcH;
            bsa.bsa_DestWidth = w;
            bsa.bsa_DestHeight = h;
            bsa.bsa_XSrcFactor = srcW;
            bsa.bsa_XDestFactor = w;
            bsa.bsa_YSrcFactor = srcH;
            bsa.bsa_YDestFactor = h;
            /* the factors must be at most 16383 */
            while (bsa.bsa_XSrcFactor > 16383 || bsa.bsa_XDestFactor > 16383) {
                bsa.bsa_XSrcFactor = (bsa.bsa_XSrcFactor + 1) / 2;
                bsa.bsa_XDestFactor = (bsa.bsa_XDestFactor + 1) / 2;
            }
            while (bsa.bsa_YSrcFactor > 16383 || bsa.bsa_YDestFactor > 16383) {
                bsa.bsa_YSrcFactor = (bsa.bsa_YSrcFactor + 1) / 2;
                bsa.bsa_YDestFactor = (bsa.bsa_YDestFactor + 1) / 2;
            }
            bsa.bsa_SrcBitMap = src;
            bsa.bsa_DestBitMap = dst;
            BitMapScale(&bsa);
            img->bitmap = dst;
            img->ownBitmap = TRUE;
        } else {
            /* Not enough memory to scale: show the top left corner */
            w = RTE_MIN(w, srcW);
            h = RTE_MIN(h, srcH);
        }
    }
    img->width = (UWORD)w;
    img->height = (UWORD)h;
    img->maxWidth = (UWORD)RTE_MIN(maxWidth, 0xffff);
}

struct RTEImage *rteGetImage(struct RTEData *data, CONST_STRPTR source,
                             ULONG sourceLen, LONG maxWidth) {
    struct RTEImage *img;
    struct Library *DataTypesBase;
#if defined(__AMIGAOS4__)
    struct DataTypesIFace *IDataTypes;
#endif
    STRPTR path;
    STRPTR sysSource;
    struct BitMap *bm = NULL;
    struct BitMapHeader *bmh = NULL;
    struct TagItem tags[6];
    struct TagItem getTags[3];
    struct gpLayout layout;

    for (img = (struct RTEImage *)data->images.mlh_Head;
         img->node.mln_Succ != NULL;
         img = (struct RTEImage *)img->node.mln_Succ) {
        if (strlen(img->source) == sourceLen &&
            strncmp(img->source, source, sourceLen) == 0) {
            if (img->failed)
                return NULL;
            if (img->maxWidth != (UWORD)RTE_MIN(maxWidth, 0xffff) &&
                img->dto != NULL) {
                DataTypesBase = rteDataTypesBase;
#if defined(__AMIGAOS4__)
                IDataTypes = rteIDataTypes;
#endif
                getTags[0].ti_Tag = PDTA_DestBitMap;
                getTags[0].ti_Data = (ULONG)&bm;
                getTags[1].ti_Tag = PDTA_BitMapHeader;
                getTags[1].ti_Data = (ULONG)&bmh;
                getTags[2].ti_Tag = TAG_DONE;
                GetDTAttrsA(img->dto, getTags);
                if (bm != NULL && bmh != NULL)
                    rteScaleImage(data, img, bm, bmh->bmh_Width,
                                  bmh->bmh_Height, maxWidth);
            }
            return img;
        }
    }
    if (!data->setup || data->obj == NULL)
        return NULL;

    img = AllocVec(sizeof(struct RTEImage), MEMF_ANY | MEMF_CLEAR);
    if (img == NULL)
        return NULL;
    img->source = AllocVec(sourceLen + 1, MEMF_ANY);
    if (img->source == NULL) {
        FreeVec(img);
        return NULL;
    }
    CopyMem((APTR)source, img->source, sourceLen);
    img->source[sourceLen] = '\0';
    AddTail((struct List *)&data->images, (struct Node *)img);
    img->failed = TRUE;

    /* The source is UTF-8 in the document; file names and the hook use the
     * application's encoding */
    sysSource = rteUTF8ToApi(data, img->source, -1, NULL);
    path = sysSource != NULL ? sysSource : img->source;
    if (data->imageHook != NULL) {
        struct BestTextImageMessage msg;
        msg.source = path;
        path = (STRPTR)CallHookPkt(data->imageHook, data->obj, &msg);
    } else if (strncmp(path, "http://", 7) == 0 ||
               strncmp(path, "https://", 8) == 0) {
        path = NULL;
    } else if (data->utf8 && sysSource != NULL) {
        FreeVec(sysSource);
        sysSource = rteFromUTF8(img->source, -1, NULL);
        path = sysSource != NULL ? sysSource : img->source;
    }
    if (path == NULL || path[0] == '\0' || !rteOpenDataTypes()) {
        if (sysSource != NULL)
            FreeVec(sysSource);
        return NULL;
    }
    DataTypesBase = rteDataTypesBase;
#if defined(__AMIGAOS4__)
    IDataTypes = rteIDataTypes;
#endif

    tags[0].ti_Tag = DTA_GroupID;
    tags[0].ti_Data = GID_PICTURE;
    tags[1].ti_Tag = PDTA_Remap;
    tags[1].ti_Data = TRUE;
    tags[2].ti_Tag = PDTA_Screen;
    tags[2].ti_Data = (ULONG)_screen(data->obj);
    tags[3].ti_Tag = PDTA_DestMode;
    tags[3].ti_Data = PMODE_V43;
    tags[4].ti_Tag = PDTA_UseFriendBitMap;
    tags[4].ti_Data = TRUE;
    tags[5].ti_Tag = TAG_DONE;
    img->dto = NewDTObjectA(path, tags);
    if (sysSource != NULL)
        FreeVec(sysSource);
    if (img->dto == NULL) {
        rteCloseDataTypes();
        return NULL;
    }
    layout.MethodID = DTM_PROCLAYOUT;
    layout.gpl_GInfo = NULL;
    layout.gpl_Initial = TRUE;
    DoDTMethodA(img->dto, NULL, NULL, (Msg)&layout);
    getTags[0].ti_Tag = PDTA_DestBitMap;
    getTags[0].ti_Data = (ULONG)&bm;
    getTags[1].ti_Tag = PDTA_BitMapHeader;
    getTags[1].ti_Data = (ULONG)&bmh;
    getTags[2].ti_Tag = TAG_DONE;
    GetDTAttrsA(img->dto, getTags);
    if (bm == NULL || bmh == NULL || bmh->bmh_Width == 0 ||
        bmh->bmh_Height == 0) {
        rteDisposeImage(img);
        return NULL;
    }
    rteScaleImage(data, img, bm, bmh->bmh_Width, bmh->bmh_Height, maxWidth);
    img->failed = FALSE;
    return img;
}

/* ------------------------------------------------------------------------ */
/* Markdown                                                                  */
/* ------------------------------------------------------------------------ */

static BOOL rteIsSpace(UBYTE c) { return c == ' ' || c == '\t'; }

static BOOL rteIsPunct(UBYTE c) {
    return (c >= 33 && c <= 47) || (c >= 58 && c <= 64) ||
           (c >= 91 && c <= 96) || (c >= 123 && c <= 126);
}

static BOOL rteEnsureMd(struct RTEData *data, struct RTEPara *p) {
    if (p->len == 0)
        return FALSE;
    if (p->md != NULL && p->mdCap >= p->len)
        return TRUE;
    if (p->md != NULL)
        FreePooled(data->pool, p->md, p->mdCap * sizeof(UWORD));
    p->mdCap = (p->len + 15) & ~15UL;
    p->md = AllocPooled(data->pool, p->mdCap * sizeof(UWORD));
    if (p->md == NULL)
        p->mdCap = 0;
    return p->md != NULL;
}

static void rteMdFill(struct RTEPara *p, ULONG from, ULONG to, UWORD style) {
    ULONG i;
    if (p->md == NULL)
        return;
    for (i = from; i < to && i < p->len; i++)
        p->md[i] = style;
}

static void rteMdOr(struct RTEPara *p, ULONG from, ULONG to, UWORD style) {
    ULONG i;
    if (p->md == NULL)
        return;
    for (i = from; i < to && i < p->len; i++)
        p->md[i] |= style;
}

/** Leading spaces, up to max */
static ULONG rteLeadingSpaces(struct RTEPara *p, ULONG max) {
    ULONG i = 0;
    while (i < p->len && i < max && p->text[i] == ' ')
        i++;
    return i;
}

/** Is the paragraph a fence line (``` or ~~~)? Returns the fence char */
static UBYTE rteFenceChar(struct RTEPara *p, ULONG *runOut) {
    ULONG i = rteLeadingSpaces(p, 3);
    ULONG run = 0;
    UBYTE c;
    if (i >= p->len)
        return 0;
    c = p->text[i];
    if (c != '`' && c != '~')
        return 0;
    while (i + run < p->len && p->text[i + run] == c)
        run++;
    if (run < 3)
        return 0;
    if (runOut != NULL)
        *runOut = run;
    return c;
}

static BOOL rteOnlySpacesFrom(struct RTEPara *p, ULONG i) {
    for (; i < p->len; i++) {
        if (!rteIsSpace(p->text[i]))
            return FALSE;
    }
    return TRUE;
}

static BOOL rteIsRule(struct RTEPara *p) {
    ULONG i = rteLeadingSpaces(p, 3);
    UBYTE c;
    ULONG count = 0;
    if (i >= p->len)
        return FALSE;
    c = p->text[i];
    if (c != '-' && c != '*' && c != '_')
        return FALSE;
    for (; i < p->len; i++) {
        if (p->text[i] == c)
            count++;
        else if (!rteIsSpace(p->text[i]))
            return FALSE;
    }
    return count >= 3;
}

static BOOL rteIsTableRule(struct RTEPara *p) {
    ULONG i;
    BOOL dash = FALSE, pipe = FALSE;
    for (i = 0; i < p->len; i++) {
        UBYTE c = p->text[i];
        if (c == '-')
            dash = TRUE;
        else if (c == '|')
            pipe = TRUE;
        else if (c != ':' && !rteIsSpace(c))
            return FALSE;
    }
    return dash && pipe;
}

/**
 * Work out the block kind of a paragraph from its leading characters
 */
static void rteParseBlock(struct RTEData *data, struct RTEPara *p) {
    ULONG i;
    UBYTE fence;
    ULONG run;

    p->block = RTE_B_TEXT;
    p->level = 0;
    p->contentStart = 0;
    p->taskState = 0;
    p->listNumber = 0;
    p->stateOut = RTE_ST_NONE;

    if (!(p->flags & RTEP_MARKDOWN))
        return;

    fence = rteFenceChar(p, &run);
    if (p->stateIn != RTE_ST_NONE) {
        UBYTE open = p->stateIn == RTE_ST_FENCE ? '`' : '~';
        if (fence == open &&
            rteOnlySpacesFrom(p, rteLeadingSpaces(p, 3) + run)) {
            p->block = RTE_B_FENCE_CLOSE;
            p->stateOut = RTE_ST_NONE;
        } else {
            p->block = RTE_B_CODE;
            p->stateOut = p->stateIn;
        }
        return;
    }
    if (fence != 0) {
        p->block = RTE_B_FENCE_OPEN;
        p->stateOut = fence == '`' ? RTE_ST_FENCE : RTE_ST_TILDE;
        return;
    }
    if (p->len == 0 || rteOnlySpacesFrom(p, 0)) {
        p->block = RTE_B_BLANK;
        return;
    }

    i = rteLeadingSpaces(p, 3);
    if (p->text[i] == '#') {
        ULONG level = 0;
        while (i + level < p->len && p->text[i + level] == '#')
            level++;
        if (level <= 6 &&
            (i + level == p->len || rteIsSpace(p->text[i + level]))) {
            p->block = RTE_B_HEADING;
            p->level = (UBYTE)level;
            i += level;
            while (i < p->len && rteIsSpace(p->text[i]))
                i++;
            p->contentStart = (UWORD)RTE_MIN(i, 0xffff);
            return;
        }
    }
    if (rteIsRule(p)) {
        p->block = RTE_B_RULE;
        return;
    }
    if (p->text[i] == '|' || (rteIsTableRule(p))) {
        p->block = rteIsTableRule(p) ? RTE_B_TABLE_RULE : RTE_B_TABLE;
        return;
    }
    if (p->text[i] == '>') {
        UBYTE level = 0;
        while (i < p->len && p->text[i] == '>') {
            level++;
            i++;
            if (i < p->len && p->text[i] == ' ')
                i++;
            while (i < p->len && p->text[i] == ' ' && i + 1 < p->len &&
                   p->text[i + 1] == '>')
                i++;
        }
        p->block = RTE_B_QUOTE;
        p->level = level;
        p->contentStart = (UWORD)RTE_MIN(i, 0xffff);
        return;
    }

    /* Lists: any indentation, then - * + or 1. 1) and a space */
    i = 0;
    while (i < p->len && p->text[i] == ' ')
        i++;
    if (i < p->len) {
        ULONG indent = i;
        UBYTE c = p->text[i];
        ULONG markerEnd = 0;
        if ((c == '-' || c == '*' || c == '+') && i + 1 < p->len &&
            p->text[i + 1] == ' ') {
            markerEnd = i + 2;
        } else if (c >= '0' && c <= '9') {
            ULONG j = i;
            ULONG number = 0;
            while (j < p->len && j - i < 9 && p->text[j] >= '0' &&
                   p->text[j] <= '9') {
                number = number * 10 + (p->text[j] - '0');
                j++;
            }
            if (j < p->len && (p->text[j] == '.' || p->text[j] == ')') &&
                (j + 1 == p->len || p->text[j + 1] == ' ')) {
                markerEnd = RTE_MIN(j + 2, p->len);
                p->listNumber = (UWORD)RTE_MIN(number, 0xffff);
            }
        }
        if (markerEnd > 0) {
            p->block = RTE_B_LIST;
            p->level = (UBYTE)RTE_MIN(indent / 2, 6);
            if (p->listNumber == 0 && markerEnd + 3 <= p->len &&
                p->text[markerEnd] == '[' && p->text[markerEnd + 2] == ']' &&
                (markerEnd + 3 == p->len || p->text[markerEnd + 3] == ' ')) {
                UBYTE mark = p->text[markerEnd + 1];
                if (mark == ' ')
                    p->taskState = 1;
                else if (mark == 'x' || mark == 'X')
                    p->taskState = 2;
                if (p->taskState != 0)
                    markerEnd = RTE_MIN(markerEnd + 4, p->len);
            }
            p->contentStart = (UWORD)RTE_MIN(markerEnd, 0xffff);
            return;
        }
    }
}

/* Delimiter runs for emphasis matching */
struct RTEDelim {
    ULONG pos;
    UWORD len;
    UBYTE c;
    UBYTE canOpen;
    UBYTE canClose;
    UBYTE pad;
    UWORD used; /* bytes consumed from the inner end */
};

#define RTE_MAX_DELIMS 64

/** Find the end of a link destination "(...)" starting at '(' */
static ULONG rteFindParen(struct RTEPara *p, ULONG open) {
    ULONG i;
    LONG depth = 0;
    for (i = open; i < p->len; i++) {
        if (p->text[i] == '(')
            depth++;
        else if (p->text[i] == ')') {
            if (--depth == 0)
                return i;
        } else if (p->text[i] == ' ' && depth == 1) {
            /* a title may follow; keep scanning */
        }
    }
    return (ULONG)-1;
}

/** Find the ']' closing the bracket at open */
static ULONG rteFindBracket(struct RTEPara *p, ULONG open) {
    ULONG i;
    LONG depth = 0;
    for (i = open; i < p->len; i++) {
        if (p->text[i] == '\\') {
            i++;
            continue;
        }
        if (p->text[i] == '[')
            depth++;
        else if (p->text[i] == ']') {
            if (--depth == 0)
                return i;
        }
    }
    return (ULONG)-1;
}

/** The target of a link whose text ends at close and destination at end */
static void rteLinkTarget(struct RTEPara *p, ULONG close, ULONG end,
                          ULONG *urlStart, ULONG *urlEnd) {
    ULONG t;
    *urlStart = close + 2;
    *urlEnd = end;
    if (*urlStart < end && p->text[*urlStart] == '<') {
        (*urlStart)++;
        for (t = *urlStart; t < end; t++) {
            if (p->text[t] == '>') {
                *urlEnd = t;
                return;
            }
        }
        return;
    }
    for (t = *urlStart; t < end; t++) {
        if (p->text[t] == ' ') {
            *urlEnd = t;
            return;
        }
    }
}

static BOOL rteStartsUrl(struct RTEPara *p, ULONG i) {
    ULONG rest = p->len - i;
    if (i > 0 && rteIsWordChar(p->text[i - 1]))
        return FALSE;
    return (rest > 8 && strncmp((STRPTR)p->text + i, "https://", 8) == 0) ||
           (rest > 7 && strncmp((STRPTR)p->text + i, "http://", 7) == 0) ||
           (rest > 4 && strncmp((STRPTR)p->text + i, "www.", 4) == 0);
}

static ULONG rteUrlEnd(struct RTEPara *p, ULONG i) {
    while (i < p->len && p->text[i] != ' ' && p->text[i] != '\t' &&
           p->text[i] != '<' && p->text[i] != '>' && p->text[i] != '"')
        i++;
    /* Trailing punctuation is not part of the address */
    while (i > 0 && (p->text[i - 1] == '.' || p->text[i - 1] == ',' ||
                     p->text[i - 1] == ';' || p->text[i - 1] == ':' ||
                     p->text[i - 1] == '!' || p->text[i - 1] == '?' ||
                     p->text[i - 1] == ')' || p->text[i - 1] == '*' ||
                     p->text[i - 1] == '_'))
        i--;
    return i;
}

/**
 * Inline Markdown between from and the end of the paragraph
 */
static void rteParseInline(struct RTEData *data, struct RTEPara *p,
                           ULONG from, UWORD base, LONG imageWidth) {
    struct RTEDelim delims[RTE_MAX_DELIMS];
    UWORD numDelims = 0;
    UWORD markup = RTE_S_MARKUP | (data->hideMarkup ? RTE_S_HIDDEN : 0);
    ULONG i = from;
    UWORD d, k;

    while (i < p->len) {
        UBYTE c = p->text[i];

        if (c == '\\' && i + 1 < p->len && rteIsPunct(p->text[i + 1])) {
            p->md[i] = markup;
            p->md[i + 1] = base;
            i += 2;
            continue;
        }
        if (c == '`') {
            ULONG run = 0, j, close = (ULONG)-1;
            while (i + run < p->len && p->text[i + run] == '`')
                run++;
            for (j = i + run; j < p->len;) {
                ULONG r = 0;
                while (j + r < p->len && p->text[j + r] == '`')
                    r++;
                if (r == run) {
                    close = j;
                    break;
                }
                j += r > 0 ? r : 1;
            }
            if (close != (ULONG)-1) {
                rteMdFill(p, i, i + run, markup | RTE_S_CODE);
                rteMdFill(p, i + run, close, (base & ~RTE_S_CODEFONT) | RTE_S_CODE);
                rteMdFill(p, close, close + run, markup | RTE_S_CODE);
                i = close + run;
                continue;
            }
            rteMdFill(p, i, i + run, base);
            i += run;
            continue;
        }
        if ((c == '!' && i + 1 < p->len && p->text[i + 1] == '[') || c == '[') {
            ULONG open = c == '!' ? i + 1 : i;
            ULONG close = rteFindBracket(p, open);
            if (close != (ULONG)-1 && close + 1 < p->len &&
                p->text[close + 1] == '(') {
                ULONG end = rteFindParen(p, close + 1);
                if (end != (ULONG)-1) {
                    ULONG urlStart, urlEnd;
                    rteLinkTarget(p, close, end, &urlStart, &urlEnd);
                    if (c == '!') {
                        struct RTEImage *img = NULL;
                        if (data->hideMarkup && imageWidth > 0)
                            img = rteGetImage(data, (STRPTR)p->text + urlStart,
                                              urlEnd - urlStart, imageWidth);
                        if (img != NULL) {
                            rteMdFill(p, i, end + 1, markup | RTE_S_HIDDEN);
                            p->md[i] = RTE_S_IMAGE | RTE_S_LINK;
                        } else {
                            rteMdFill(p, i, open + 1, markup);
                            rteMdFill(p, open + 1, close,
                                      base | RTE_S_LINK | RTE_S_ITALIC);
                            rteMdFill(p, close, end + 1, markup);
                        }
                    } else {
                        p->md[i] = markup;
                        rteMdFill(p, i + 1, close, base | RTE_S_LINK);
                        rteMdFill(p, close, end + 1, markup);
                    }
                    i = end + 1;
                    continue;
                }
            }
        }
        if (c == '<' && i + 1 < p->len) {
            ULONG j = i + 1;
            if (rteStartsUrl(p, j)) {
                while (j < p->len && p->text[j] != '>' && p->text[j] != ' ')
                    j++;
                if (j < p->len && p->text[j] == '>') {
                    p->md[i] = markup;
                    rteMdFill(p, i + 1, j, base | RTE_S_LINK);
                    p->md[j] = markup;
                    i = j + 1;
                    continue;
                }
            }
        }
        if ((c == 'h' || c == 'w') && rteStartsUrl(p, i)) {
            ULONG end = rteUrlEnd(p, i);
            if (end > i) {
                rteMdFill(p, i, end, base | RTE_S_LINK);
                i = end;
                continue;
            }
        }
        if (c == '*' || c == '_' || c == '~') {
            ULONG run = 0;
            UBYTE before = i > from ? p->text[i - 1] : ' ';
            UBYTE after;
            BOOL leftFlanking, rightFlanking;
            while (i + run < p->len && p->text[i + run] == c)
                run++;
            after = i + run < p->len ? p->text[i + run] : ' ';
            leftFlanking = !rteIsSpace(after) &&
                           (!rteIsPunct(after) || rteIsSpace(before) ||
                            rteIsPunct(before));
            rightFlanking = !rteIsSpace(before) &&
                            (!rteIsPunct(before) || rteIsSpace(after) ||
                             rteIsPunct(after));
            rteMdFill(p, i, i + run, base);
            if (numDelims < RTE_MAX_DELIMS && (c != '~' || run == 2)) {
                struct RTEDelim *dl = &delims[numDelims++];
                dl->pos = i;
                dl->len = (UWORD)RTE_MIN(run, 0xffff);
                dl->c = c;
                dl->canOpen = leftFlanking;
                dl->canClose = rightFlanking;
                if (c == '_') {
                    dl->canOpen = leftFlanking &&
                                  (!rightFlanking || rteIsPunct(before));
                    dl->canClose = rightFlanking &&
                                   (!leftFlanking || rteIsPunct(after));
                }
                dl->used = 0;
            }
            i += run;
            continue;
        }
        p->md[i] = base;
        i++;
    }

    /* Match emphasis closers with the nearest opener of the same kind */
    for (d = 0; d < numDelims; d++) {
        struct RTEDelim *closer = &delims[d];
        if (!closer->canClose)
            continue;
        k = d;
        while (k > 0 && closer->len > closer->used) {
            struct RTEDelim *opener = &delims[--k];
            UWORD n;
            UWORD style;
            ULONG innerStart, innerEnd;
            if (opener->c != closer->c || !opener->canOpen ||
                opener->len == opener->used)
                continue;
            if (closer->c == '~') {
                n = 2;
                style = RTE_S_STRIKE;
            } else {
                UWORD oa = opener->len - opener->used;
                UWORD ca = closer->len - closer->used;
                n = (oa >= 2 && ca >= 2) ? 2 : 1;
                style = n == 2 ? RTE_S_BOLD : RTE_S_ITALIC;
            }
            /* opener chars consumed from its inner (right) end */
            innerStart = opener->pos + opener->len - opener->used;
            innerEnd = closer->pos + closer->used;
            rteMdFill(p, innerStart - n, innerStart, markup);
            rteMdFill(p, innerEnd, innerEnd + n, markup);
            rteMdOr(p, innerStart, innerEnd, style);
            opener->used += n;
            closer->used += n;
            /* delimiters between the pair can no longer match */
            {
                UWORD m;
                for (m = k + 1; m < d; m++)
                    delims[m].canOpen = delims[m].canClose = FALSE;
            }
            if (opener->len > opener->used)
                k++; /* the opener may still match another closer */
        }
    }
}

/**
 * Compute the rendered styles of a paragraph. imageWidth is the width
 * available to images in the paragraph.
 */
static void rteStyleParagraph(struct RTEData *data, struct RTEPara *p,
                              LONG imageWidth) {
    UWORD markup = RTE_S_MARKUP | (data->hideMarkup ? RTE_S_HIDDEN : 0);
    UWORD base = 0;

    p->mdUniform = 0;
    if (!(p->flags & RTEP_MARKDOWN) || p->len == 0) {
        if (p->md != NULL) {
            FreePooled(data->pool, p->md, p->mdCap * sizeof(UWORD));
            p->md = NULL;
            p->mdCap = 0;
        }
        if (p->block == RTE_B_CODE)
            p->mdUniform = RTE_S_CODEFONT;
        return;
    }
    switch (p->block) {
    case RTE_B_CODE:
        if (p->md != NULL) {
            FreePooled(data->pool, p->md, p->mdCap * sizeof(UWORD));
            p->md = NULL;
            p->mdCap = 0;
        }
        p->mdUniform = RTE_S_CODEFONT;
        return;
    case RTE_B_FENCE_OPEN:
    case RTE_B_FENCE_CLOSE:
    case RTE_B_RULE:
        if (p->md != NULL) {
            FreePooled(data->pool, p->md, p->mdCap * sizeof(UWORD));
            p->md = NULL;
            p->mdCap = 0;
        }
        p->mdUniform = markup | RTE_S_CODEFONT;
        return;
    case RTE_B_TABLE_RULE:
        if (p->md != NULL) {
            FreePooled(data->pool, p->md, p->mdCap * sizeof(UWORD));
            p->md = NULL;
            p->mdCap = 0;
        }
        p->mdUniform = RTE_S_MARKUP | RTE_S_CODEFONT;
        return;
    default:
        break;
    }
    if (!rteEnsureMd(data, p))
        return;
    if (p->block == RTE_B_HEADING)
        base = RTE_S_HEADING | RTE_S_BOLD;
    else if (p->block == RTE_B_QUOTE)
        base = RTE_S_QUOTE;
    else if (p->block == RTE_B_TABLE)
        base = RTE_S_CODEFONT;

    if (p->contentStart > 0) {
        rteMdFill(p, 0, p->contentStart, markup);
        if (p->block == RTE_B_LIST && p->listNumber > 0) {
            /* numbered markers stay visible, only the indentation hides */
            ULONG i = rteLeadingSpaces(p, p->len);
            rteMdFill(p, i, p->contentStart, 0);
        }
    }
    rteParseInline(data, p, p->contentStart, base, imageWidth);
    if (p->block == RTE_B_TABLE) {
        ULONG i;
        for (i = 0; i < p->len; i++) {
            if (p->text[i] == '|' && !(p->md[i] & RTE_S_CODE))
                p->md[i] = RTE_S_MARKUP | RTE_S_CODEFONT;
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Line breaking                                                             */
/* ------------------------------------------------------------------------ */

/** Width of the image drawn at an RTE_S_IMAGE byte */
static LONG rteImageWidthAt(struct RTEData *data, struct RTEPara *p,
                            ULONG offset, UWORD *height) {
    ULONG s, e, us, ue;
    struct RTEImage *img;
    if (!rteFindSpan(p, offset, TRUE, &s, &e, &us, &ue))
        return 0;
    img = rteGetImage(data, (STRPTR)p->text + us, ue - us,
                      p->wrapWidth > 0 ? p->wrapWidth : 200);
    if (img == NULL)
        return 0;
    if (height != NULL)
        *height = img->height;
    return img->width;
}

/** Height of the emoji drawn in text of a style */
LONG rteEmojiSize(struct RTEData *data, UWORD style) {
    LONG h = (style & (RTE_S_CODE | RTE_S_CODEFONT)) ? data->codeHeight
                                                     : data->fontHeight;
    return h < 6 ? 6 : h;
}

/** Width of the box drawn for a character no font or emoji has */
LONG rteBoxWidth(struct RTEData *data, UWORD style) {
    LONG w = (rteEmojiSize(data, style) * 2) / 3 + 2;
    return w < 5 ? 5 : w;
}

/** Width of what a glyph (RTE_G_*) draws in a style */
LONG rteGlyphWidth(struct RTEData *data, UWORD style, UWORD glyph) {
    UWORD *tab;
    switch (RTE_G_KIND(glyph)) {
    case RTE_G_GLYPH:
        return rteWidthTable(data, style)[RTE_G_VALUE(glyph) & 0xff];
    case RTE_G_TRANS: {
        UBYTE b[8];
        ULONG n = rteGlyphBytes(glyph, b), i;
        LONG w = 0;
        tab = rteWidthTable(data, style);
        for (i = 0; i < n; i++)
            w += tab[b[i]];
        return w;
    }
    case RTE_G_EMOJI:
        return rteEmojiSize(data, style) + 1;
    case RTE_G_MISSING:
        return rteBoxWidth(data, style);
    }
    return 0; /* RTE_G_CONT: the cluster's first byte has the width */
}

LONG rteCharWidth(struct RTEData *data, struct RTEPara *p, ULONG offset,
                  LONG x) {
    UWORD style = rteStyleAt(p, offset);
    UBYTE c = p->text[offset];
    if (style & RTE_S_HIDDEN)
        return 0;
    if (style & RTE_S_IMAGE)
        return rteImageWidthAt(data, p, offset, NULL);
    if (c == '\t') {
        LONG stop = (data->tabSize > 0 ? data->tabSize : RTE_TAB_DEFAULT) *
                    data->spaceWidth;
        if (stop <= 0)
            stop = 8;
        return stop - (x % stop);
    }
    if (p->glyphLen == 0 && c < 0x80 &&
        (offset + 1 >= p->len || p->text[offset + 1] < 0x80))
        return rteWidthTable(data, style)[c];
    return rteGlyphWidth(data, style, rteGlyphAt(p, offset));
}

LONG rteBubbleSide(struct RTEData *data, struct RTEPara *p) {
    if (p->bubble == 0)
        return 0;
    if (p->bubbleSide != 0)
        return p->bubbleSide;
    if (p->bubble <= data->numBubbleStyles)
        return data->bubbleStyles[p->bubble - 1].side;
    return MUIV_BestTextEditor_Bubble_Left;
}

LONG rteParaFlow(struct RTEData *data, struct RTEPara *p) {
    if (data->globalFlow)
        return data->globalFlowValue;
    return p->flow;
}

/** Width of the text area the paragraph may use, before indents */
static LONG rteAreaWidth(struct RTEData *data, struct RTEPara *p) {
    LONG area = data->viewWidth - 2 * RTE_PAD_X;
    if (area < 16)
        area = 16;
    if (p->bubble != 0) {
        LONG side = rteBubbleSide(data, p);
        ULONG pct = 0;
        LONG max;
        if (p->bubble <= data->numBubbleStyles)
            pct = data->bubbleStyles[p->bubble - 1].maxWidth;
        if (pct == 0 || pct > 100)
            pct = side == MUIV_BestTextEditor_Bubble_Full ? 100 : 80;
        max = (area - 2 * RTE_BUBBLE_TAIL) * (LONG)pct / 100;
        area = max - 2 * RTE_BUBBLE_PAD_X;
        if (area < 16)
            area = 16;
    }
    return area;
}

static BOOL rteAddLine(struct RTEData *data, struct RTEPara *p,
                       struct RTELine *l) {
    if (p->numLines == p->maxLines) {
        UWORD newMax = p->maxLines == 0 ? 2 : p->maxLines * 2;
        struct RTELine *lines =
            AllocPooled(data->pool, newMax * sizeof(struct RTELine));
        if (lines == NULL)
            return FALSE;
        if (p->lines != NULL) {
            CopyMem(p->lines, lines, p->numLines * sizeof(struct RTELine));
            FreePooled(data->pool, p->lines, p->maxLines * sizeof(struct RTELine));
        }
        p->lines = lines;
        p->maxLines = newMax;
    }
    p->lines[p->numLines++] = *l;
    return TRUE;
}

/** Height and baseline of a line holding bytes [start, end) */
static void rteMeasureLineHeight(struct RTEData *data, struct RTEPara *p,
                                 ULONG start, ULONG end, struct RTELine *l) {
    LONG ascent = 0, descent = 0;
    ULONG i;
    BOOL any = FALSE;
    for (i = start; i < end; i++) {
        UWORD style = rteStyleAt(p, i);
        LONG a, d;
        if (style & RTE_S_HIDDEN)
            continue;
        any = TRUE;
        if (style & RTE_S_IMAGE) {
            UWORD h = 0;
            rteImageWidthAt(data, p, i, &h);
            a = h + 2;
            d = 2;
        } else if (style & (RTE_S_CODE | RTE_S_CODEFONT)) {
            a = data->codeBaseline;
            d = data->codeHeight - data->codeBaseline;
        } else {
            a = data->fontBaseline;
            d = data->fontHeight - data->fontBaseline;
        }
        if (style & RTE_S_CODE) {
            /* room for the tinted box around inline code */
            a += 1;
            d += 1;
        }
        ascent = RTE_MAX(ascent, a);
        descent = RTE_MAX(descent, d);
    }
    if (!any) {
        if (p->block == RTE_B_CODE || p->block == RTE_B_TABLE) {
            ascent = data->codeBaseline;
            descent = data->codeHeight - data->codeBaseline;
        } else {
            ascent = data->fontBaseline;
            descent = data->fontHeight - data->fontBaseline;
        }
    }
    l->baseline = (UWORD)ascent;
    l->height = (UWORD)(ascent + descent);
}

/**
 * Break one paragraph into visual lines
 */
void rteLayoutParagraph(struct RTEData *data, ULONG index) {
    struct RTEPara *p = &data->paras[index];
    LONG area, wrapW, unit;
    LONG firstX = 0, contX = 0;
    ULONG lineStart, i;
    LONG x;
    ULONG lastBreak;
    LONG xAtBreak = 0;
    ULONG chars = 0;
    LONG linesH = 0;
    BOOL noWrap = data->wrapMode == MUIV_TextEditor_WrapMode_NoWrap;
    struct RTELine l;

    p->stateIn = index > 0 ? data->paras[index - 1].stateOut : RTE_ST_NONE;
    if (index > 0 && !(data->paras[index - 1].flags & RTEP_MARKDOWN))
        p->stateIn = RTE_ST_NONE;
    rteAnnotate(data, p);
    rteParseBlock(data, p);

    area = rteAreaWidth(data, p);
    unit = rteIndentUnit(data);
    if (data->hideMarkup || p->block == RTE_B_QUOTE) {
        if (p->block == RTE_B_LIST) {
            firstX = p->level * unit;
            if (p->listNumber == 0)
                firstX += unit;
        } else if (p->block == RTE_B_QUOTE) {
            firstX = p->level * RTE_QUOTE_INDENT;
        }
    }
    if (p->block == RTE_B_CODE || p->block == RTE_B_FENCE_OPEN ||
        p->block == RTE_B_FENCE_CLOSE)
        firstX = RTE_CODE_PAD;
    wrapW = area - (p->block == RTE_B_CODE ? RTE_CODE_PAD : 0);
    if (noWrap)
        wrapW = RTE_NOWRAP_WIDTH;
    p->wrapWidth = wrapW - firstX;

    rteStyleParagraph(data, p, wrapW - firstX);

    /* Continuation lines line up with the text after the list marker */
    contX = firstX;
    if (p->block == RTE_B_LIST || p->block == RTE_B_QUOTE) {
        LONG w = 0;
        for (i = 0; i < p->contentStart && i < p->len; i++)
            w += rteCharWidth(data, p, i, w);
        contX = firstX + w;
        if (contX > wrapW / 2)
            contX = firstX;
    }
    p->firstX = (UWORD)firstX;
    p->contX = (UWORD)contX;
    p->numLines = 0;
    p->contentW = 0;

    lineStart = 0;
    x = 0;
    lastBreak = (ULONG)-1;
    i = 0;
    memset(&l, 0, sizeof(l));
    for (;;) {
        LONG indent = p->numLines == 0 ? firstX : contX;
        LONG limit = wrapW - indent;
        BOOL breakHere = FALSE;
        ULONG breakAt = i;

        if (i < p->len) {
            LONG w = rteCharWidth(data, p, i, x);
            UBYTE c = p->text[i];
            BOOL counts = w > 0;
            /* a line never breaks inside a character or emoji */
            BOOL inside = (c & 0xc0) == 0x80 ||
                          RTE_G_KIND(rteGlyphAt(p, i)) == RTE_G_CONT;
            if (i > lineStart && !inside &&
                ((x + w > limit && c != ' ') ||
                                  (data->wrapBorder > 0 &&
                                   data->wrapMode !=
                                       MUIV_TextEditor_WrapMode_NoWrap &&
                                   chars >= data->wrapBorder && c != ' '))) {
                breakHere = TRUE;
                if (data->wrapWords && lastBreak != (ULONG)-1 &&
                    lastBreak > lineStart) {
                    breakAt = lastBreak;
                } else {
                    breakAt = i;
                }
            } else {
                x += w;
                if (counts)
                    chars++;
                if (c == ' ' || c == '\t' || c == '-' || c == '/') {
                    lastBreak = i + 1;
                    xAtBreak = x;
                }
                i++;
                continue;
            }
        }

        /* emit [lineStart, breakAt) */
        if (i >= p->len && !breakHere)
            breakAt = p->len;
        l.start = lineStart;
        l.len = breakAt - lineStart;
        l.x = indent;
        rteMeasureLineHeight(data, p, lineStart, breakAt, &l);
        {
            /* content width without trailing spaces */
            LONG w = 0;
            LONG trailing = 0;
            ULONG k;
            for (k = lineStart; k < breakAt; k++) {
                LONG cw = rteCharWidth(data, p, k, w);
                w += cw;
                if (p->text[k] == ' ')
                    trailing += cw;
                else if (cw > 0)
                    trailing = 0;
            }
            l.width = (UWORD)RTE_MIN(w - trailing, 0xffff);
        }
        l.avail = (UWORD)RTE_MIN(RTE_MAX(limit, 0), 0xffff);
        l.flags = 0;
        if (p->numLines == 0)
            l.flags |= RTEL_FIRST;
        l.y = (UWORD)RTE_MIN(linesH, 0xffff);
        if (!rteAddLine(data, p, &l))
            break;
        if (indent + l.width > p->contentW)
            p->contentW = indent + l.width;
        linesH += l.height;
        (void)xAtBreak;

        if (!breakHere)
            break;
        lineStart = breakAt;
        i = breakAt;
        x = 0;
        chars = 0;
        lastBreak = (ULONG)-1;
    }
    if (p->numLines > 0)
        p->lines[p->numLines - 1].flags |= RTEL_LAST;

    /* Blocks whose text is hidden still take some room */
    if (data->hideMarkup && p->numLines == 1) {
        struct RTELine *only = &p->lines[0];
        if (p->block == RTE_B_FENCE_OPEN || p->block == RTE_B_FENCE_CLOSE) {
            linesH = RTE_CODE_PAD;
            only->height = RTE_CODE_PAD;
            only->baseline = 0;
        } else if (p->block == RTE_B_BLANK) {
            UWORD h = (UWORD)RTE_MAX(data->fontHeight / 2, 2);
            linesH = h;
            only->height = h;
            only->baseline = 0;
        } else if (p->block == RTE_B_TABLE_RULE) {
            linesH = 5;
            only->height = 5;
            only->baseline = 0;
            only->len = 0;
        }
    }
    p->linesHeight = linesH;
    p->flags &= ~RTEP_DIRTY;
}

/* ------------------------------------------------------------------------ */
/* Vertical placement                                                        */
/* ------------------------------------------------------------------------ */

static void rteComputePadding(struct RTEData *data, ULONG index) {
    struct RTEPara *p = &data->paras[index];
    struct RTEPara *prev = index > 0 ? &data->paras[index - 1] : NULL;
    struct RTEPara *next =
        index + 1 < data->numParas ? &data->paras[index + 1] : NULL;
    LONG top = 0, bottom = 0;

    p->bubbleFirst = p->bubbleLast = FALSE;
    if (p->bubble != 0) {
        p->bubbleFirst = prev == NULL || prev->bubble == 0 ||
                         prev->bubbleId != p->bubbleId;
        p->bubbleLast = next == NULL || next->bubble == 0 ||
                        next->bubbleId != p->bubbleId;
        if (p->bubbleFirst)
            top += RTE_BUBBLE_GAP + RTE_BUBBLE_PAD_Y;
        if (p->bubbleLast)
            bottom += RTE_BUBBLE_PAD_Y + RTE_BUBBLE_GAP;
    }
    if (p->block == RTE_B_HEADING && (p->flags & RTEP_MARKDOWN)) {
        if (prev != NULL && !(p->bubbleFirst))
            top += p->level <= 2 ? data->fontHeight / 2 : data->fontHeight / 3;
        bottom += p->level == 1 ? 5 : 2;
    }
    if (p->block == RTE_B_RULE && (p->flags & RTEP_MARKDOWN)) {
        top += 2;
        bottom += 2;
    }
    p->padTop = (UWORD)top;
    p->padBottom = (UWORD)bottom;
}

ULONG rteParaAtY(struct RTEData *data, LONG y) {
    ULONG lo = 0, hi = data->numParas;
    while (hi - lo > 1) {
        ULONG mid = (lo + hi) / 2;
        if (data->paras[mid].y <= y)
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

ULONG rteLineAtY(struct RTEPara *p, LONG y) {
    UWORD i;
    LONG rel = y - p->y - p->padTop;
    for (i = 0; i + 1 < p->numLines; i++) {
        if (rel < p->lines[i].y + p->lines[i].height)
            return i;
    }
    return p->numLines > 0 ? p->numLines - 1 : 0;
}

ULONG rteLineOfOffset(struct RTEPara *p, ULONG offset) {
    UWORD i;
    for (i = 0; i + 1 < p->numLines; i++) {
        if (offset < p->lines[i + 1].start)
            return i;
    }
    return p->numLines > 0 ? p->numLines - 1 : 0;
}

void rteLayout(struct RTEData *data) {
    ULONG start, i;
    LONG y;
    BOOL heightsChanged = FALSE;
    LONG width = data->viewWidth;

    if (!data->setup || data->font == NULL)
        return;
    if (width != data->layoutWidth) {
        rteMarkAllDirty(data);
        data->layoutWidth = width;
        data->drawAll = TRUE;
    }
    if (!data->anyDirty)
        return;

    start = data->firstDirty;
    if (start >= data->numParas)
        start = data->numParas - 1;
    /* The paragraph above may gain or lose its bubble end padding */
    if (start > 0)
        start--;
    /* Bubble widths depend on the whole bubble */
    while (start > 0 && data->paras[start].bubble != 0 &&
           data->paras[start - 1].bubbleId == data->paras[start].bubbleId &&
           data->paras[start - 1].bubble != 0)
        start--;

    y = start > 0 ? data->paras[start - 1].y + data->paras[start - 1].height
                  : RTE_PAD_Y;
    for (i = start; i < data->numParas; i++) {
        struct RTEPara *p = &data->paras[i];
        LONG oldY = p->y, oldH = p->height;
        if (p->flags & RTEP_DIRTY) {
            UBYTE oldOut = p->stateOut;
            rteLayoutParagraph(data, i);
            if (p->stateOut != oldOut && i + 1 < data->numParas)
                data->paras[i + 1].flags |= RTEP_DIRTY;
            rteInvalidateRange(data, y, y + RTE_MAX(oldH, 1));
        }
        rteComputePadding(data, i);
        p->y = y;
        p->height = p->padTop + p->linesHeight + p->padBottom;
        if (p->y != oldY || p->height != oldH) {
            if (!heightsChanged) {
                heightsChanged = TRUE;
                rteInvalidateRange(data, RTE_MIN(p->y, oldY), 0x3fffffff);
            }
        }
        y += p->height;
    }
    data->totalHeight = y + RTE_PAD_Y;

    /* Bubble geometry */
    data->maxLineWidth = 0;
    for (i = start; i < data->numParas;) {
        struct RTEPara *p = &data->paras[i];
        if (p->bubble != 0) {
            ULONG j = i;
            LONG maxW = 0;
            LONG side = rteBubbleSide(data, p);
            LONG area = data->viewWidth - 2 * RTE_PAD_X;
            LONG bw, bx;
            while (j < data->numParas && data->paras[j].bubble != 0 &&
                   data->paras[j].bubbleId == p->bubbleId) {
                struct RTEPara *q = &data->paras[j];
                LONG w = q->contentW;
                if (q->block == RTE_B_CODE || q->block == RTE_B_FENCE_OPEN ||
                    q->block == RTE_B_FENCE_CLOSE || q->block == RTE_B_RULE)
                    w = q->wrapWidth + q->firstX;
                maxW = RTE_MAX(maxW, w);
                j++;
            }
            bw = maxW + 2 * RTE_BUBBLE_PAD_X;
            if (side == MUIV_BestTextEditor_Bubble_Full)
                bw = area - 2 * RTE_BUBBLE_TAIL;
            if (bw < 2 * RTE_BUBBLE_RADIUS + 4)
                bw = 2 * RTE_BUBBLE_RADIUS + 4;
            if (bw > area - 2 * RTE_BUBBLE_TAIL)
                bw = area - 2 * RTE_BUBBLE_TAIL;
            switch (side) {
            case MUIV_BestTextEditor_Bubble_Right:
                bx = area - RTE_BUBBLE_TAIL - bw;
                break;
            case MUIV_BestTextEditor_Bubble_Center:
                bx = (area - bw) / 2;
                break;
            default:
                bx = RTE_BUBBLE_TAIL;
                break;
            }
            for (; i < j; i++) {
                struct RTEPara *q = &data->paras[i];
                if (q->bubbleX != bx || q->bubbleW != bw) {
                    q->bubbleX = bx;
                    q->bubbleW = bw;
                    rteInvalidateRange(data, q->y, q->y + q->height);
                }
            }
            continue;
        }
        i++;
    }
    if (data->wrapMode == MUIV_TextEditor_WrapMode_NoWrap) {
        for (i = 0; i < data->numParas; i++)
            data->maxLineWidth =
                RTE_MAX(data->maxLineWidth, data->paras[i].contentW);
    }
    data->anyDirty = FALSE;
    data->layoutAll = FALSE;
}

/* ------------------------------------------------------------------------ */
/* Coordinates                                                               */
/* ------------------------------------------------------------------------ */

/**
 * Left edge of a line's content relative to the text area, and the extra
 * pixels given to each space when the paragraph is justified
 */
void rteLineGeometry(struct RTEData *data, struct RTEPara *p,
                     struct RTELine *l, LONG *left, LONG *extra,
                     LONG *extraRemainder) {
    LONG flow = rteParaFlow(data, p);
    LONG base = p->bubble != 0 ? p->bubbleX + RTE_BUBBLE_PAD_X : 0;
    LONG avail = l->avail;
    LONG x = l->x;

    *extra = 0;
    *extraRemainder = 0;
    if (p->bubble != 0)
        avail = p->bubbleW - 2 * RTE_BUBBLE_PAD_X - l->x;
    if (data->wrapMode != MUIV_TextEditor_WrapMode_NoWrap &&
        avail > l->width) {
        switch (flow) {
        case MUIV_TextEditor_Flow_Center:
            x += (avail - l->width) / 2;
            break;
        case MUIV_TextEditor_Flow_Right:
            x += avail - l->width;
            break;
        case MUIV_TextEditor_Flow_Justified:
            if (!(l->flags & RTEL_LAST)) {
                ULONG k, spaces = 0;
                ULONG end = l->start + l->len;
                while (end > l->start && p->text[end - 1] == ' ')
                    end--;
                for (k = l->start; k < end; k++) {
                    if (p->text[k] == ' ' &&
                        !(rteStyleAt(p, k) & RTE_S_HIDDEN))
                        spaces++;
                }
                if (spaces > 0) {
                    *extra = (avail - l->width) / spaces;
                    *extraRemainder = (avail - l->width) % spaces;
                }
            }
            break;
        }
    }
    *left = base + x;
}

LONG rteOffsetToX(struct RTEData *data, struct RTEPara *p, struct RTELine *l,
                  ULONG offset) {
    LONG left, extra, rem;
    LONG x = 0;
    ULONG k;
    ULONG spaces = 0;
    rteLineGeometry(data, p, l, &left, &extra, &rem);
    for (k = l->start; k < offset && k < l->start + l->len; k++) {
        x += rteCharWidth(data, p, k, x);
        if (extra > 0 && p->text[k] == ' ') {
            x += extra + (spaces < (ULONG)rem ? 1 : 0);
            spaces++;
        }
    }
    return left + x;
}

ULONG rteXToOffset(struct RTEData *data, struct RTEPara *p, struct RTELine *l,
                   LONG x) {
    LONG left, extra, rem;
    LONG cx = 0;
    ULONG k;
    ULONG end = l->start + l->len;
    ULONG spaces = 0;
    rteLineGeometry(data, p, l, &left, &extra, &rem);
    x -= left;
    if (x <= 0)
        return l->start;
    /* A soft wrapped line ends before its trailing space */
    if (!(l->flags & RTEL_LAST) && end > l->start && p->text[end - 1] == ' ')
        end--;
    for (k = l->start; k < end; k++) {
        LONG w = rteCharWidth(data, p, k, cx);
        if (extra > 0 && p->text[k] == ' ') {
            w += extra + (spaces < (ULONG)rem ? 1 : 0);
            spaces++;
        }
        if (w == 0)
            continue;
        if (x < cx + w / 2)
            return k;
        cx += w;
    }
    return end;
}

/**
 * Find a link or image span around offset.
 * spanStart/spanEnd cover the whole construct, urlStart/urlEnd the target.
 */
BOOL rteFindSpan(struct RTEPara *p, ULONG offset, BOOL image, ULONG *spanStart,
                 ULONG *spanEnd, ULONG *urlStart, ULONG *urlEnd) {
    ULONG i;
    (void)image;
    for (i = 0; i < p->len; i++) {
        UBYTE c = p->text[i];
        if (c == '[' || (c == '!' && i + 1 < p->len && p->text[i + 1] == '[')) {
            ULONG open = c == '!' ? i + 1 : i;
            ULONG close = rteFindBracket(p, open);
            if (close != (ULONG)-1 && close + 1 < p->len &&
                p->text[close + 1] == '(') {
                ULONG end = rteFindParen(p, close + 1);
                if (end != (ULONG)-1) {
                    if (offset >= i && offset <= end) {
                        *spanStart = i;
                        *spanEnd = end + 1;
                        rteLinkTarget(p, close, end, urlStart, urlEnd);
                        return TRUE;
                    }
                    i = end;
                    continue;
                }
            }
        }
        if (rteStartsUrl(p, i)) {
            ULONG end = rteUrlEnd(p, i);
            if (offset >= i && offset < end) {
                *spanStart = *urlStart = i;
                *spanEnd = *urlEnd = end;
                return TRUE;
            }
            if (end > i)
                i = end - 1;
        }
    }
    return FALSE;
}
