#ifndef RTE_PRIVATE_H
#define RTE_PRIVATE_H

#include <clib/alib_protos.h>
#include <exec/lists.h>
#include <exec/types.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <libraries/mui.h>
#include <utility/hooks.h>
#include <SDI_compiler.h>
#include <SDI_hook.h>
#include "RichTextEditor.h"

/*
 * Internal data model
 *
 * The text is an array of paragraphs (logical lines without their '\n').
 * Every paragraph owns its own byte buffer, so typing touches one small
 * allocation and never shifts the rest of the document. Buffers come from an
 * exec memory pool and grow and shrink in 16 byte steps with hysteresis.
 *
 * Styling has two independent layers:
 *   attrs  explicit styles and colours set by escape sequences, the style
 *          attributes or MUIM_TextEditor_SetBlock (allocated lazily, one UWORD
 *          per byte: style bits in the low byte, colour index in the high
 *          byte)
 *   md     the Markdown rendering of the text, recomputed on layout
 *
 * Layout turns each paragraph into visual lines. It is cached per paragraph
 * and only redone for paragraphs marked dirty, plus any following paragraphs
 * whose Markdown block state (fenced code) changed as a result.
 */

#define RTE_TAB_DEFAULT 4

/* Explicit style bits (low byte of an attr) */
#define RTE_A_BOLD      0x01
#define RTE_A_ITALIC    0x02
#define RTE_A_UNDERLINE 0x04
#define RTE_A_STYLEMASK 0x07
#define RTE_A_COLOR(a)  (((a) >> 8) & 0xff)
#define RTE_A_MAKE(style, color) ((UWORD)(((color) << 8) | ((style)&0xff)))

/* Rendered style bits (attrs merged with Markdown) */
#define RTE_S_BOLD      0x0001
#define RTE_S_ITALIC    0x0002
#define RTE_S_UNDERLINE 0x0004
#define RTE_S_STRIKE    0x0008
#define RTE_S_CODE      0x0010 /* code font with a tinted background */
#define RTE_S_LINK      0x0020
#define RTE_S_MARKUP    0x0040 /* Markdown syntax */
#define RTE_S_HIDDEN    0x0080 /* takes no space and is not drawn */
#define RTE_S_IMAGE     0x0100 /* the image of a ![alt](src) span is drawn here */
#define RTE_S_CODEFONT  0x0200 /* code font without the tinted background */
#define RTE_S_HEADING   0x0400
#define RTE_S_QUOTE     0x0800

/* Paragraph flags */
#define RTEP_DIRTY      0x01 /* layout must be redone */
#define RTEP_HIGHLIGHT  0x02 /* ESC h */
#define RTEP_MARKDOWN   0x04 /* render as Markdown */

/* Markdown block kinds */
enum {
    RTE_B_TEXT = 0,
    RTE_B_BLANK,
    RTE_B_HEADING,
    RTE_B_RULE,
    RTE_B_LIST,
    RTE_B_QUOTE,
    RTE_B_FENCE_OPEN,
    RTE_B_FENCE_CLOSE,
    RTE_B_CODE,
    RTE_B_TABLE,
    RTE_B_TABLE_RULE
};

/* Block state carried between paragraphs */
#define RTE_ST_NONE  0
#define RTE_ST_FENCE 1 /* inside ``` */
#define RTE_ST_TILDE 2 /* inside ~~~ */

/* Visual line flags */
#define RTEL_LAST        0x01 /* last line of its paragraph */
#define RTEL_BUBBLE_TOP  0x02 /* the bubble starts in this line */
#define RTEL_BUBBLE_BOT  0x04 /* the bubble ends in this line */
#define RTEL_BLOCK_TOP   0x08 /* a code block starts in this line */
#define RTEL_BLOCK_BOT   0x10 /* a code block ends in this line */
#define RTEL_FIRST       0x20 /* first line of its paragraph */

struct RTELine {
    ULONG start;     /* byte offset in the paragraph */
    ULONG len;       /* bytes */
    LONG x;          /* left edge of the content inside the text area */
    UWORD y;         /* top of the band, relative to the paragraph */
    UWORD height;    /* band height */
    UWORD baseline;  /* text baseline from the band top */
    UWORD width;     /* pixel width of the content */
    UWORD avail;     /* width available to the content (for justify) */
    UWORD decoTop;   /* bubble/block decoration top, from the band top */
    UWORD decoBot;   /* bubble/block decoration bottom, from the band top */
    UBYTE flags;
    UBYTE pad;
};

struct RTEPara {
    UBYTE *text;  /* NUL terminated, cap includes the NUL */
    UWORD *attrs; /* NULL when the paragraph has no explicit attributes */
    UWORD *md;    /* rendered styles, NULL when uniform (see mdUniform) */
    ULONG len;
    ULONG cap;
    ULONG attrCap;
    ULONG mdCap;

    /* metadata */
    UBYTE flow;       /* MUIV_TextEditor_Flow_* */
    UBYTE separator;  /* LNSF_* */
    UBYTE flags;      /* RTEP_* */
    UBYTE bubble;     /* bubble style 1..n, 0 = none */
    UBYTE bubbleSide; /* side override, 0 = the style's side */
    UBYTE pad0;
    UWORD bubbleId;   /* paragraphs with the same id share a bubble */

    /* layout (valid when !(flags & RTEP_DIRTY)) */
    UBYTE block;
    UBYTE level;      /* heading level, list depth or quote depth */
    UBYTE stateIn;
    UBYTE stateOut;
    UWORD mdUniform;  /* rendered style of every byte when md == NULL */
    UWORD contentStart; /* bytes of leading block markup (list marker, #) */
    LONG y;           /* top in document coordinates */
    LONG height;      /* padTop + linesHeight + padBottom */
    LONG linesHeight;
    UWORD padTop;     /* space above the first line */
    UWORD padBottom;  /* space below the last line */
    UBYTE bubbleFirst; /* first paragraph of its bubble */
    UBYTE bubbleLast;  /* last paragraph of its bubble */
    UWORD firstX;     /* indent of the first line */
    UWORD contX;      /* indent of the following lines */
    LONG wrapWidth;   /* width the lines were broken for */
    LONG bubbleX;     /* bubble left edge inside the text area */
    LONG bubbleW;     /* bubble width */
    LONG contentW;    /* widest visual line of this paragraph */
    UWORD numLines;
    UWORD maxLines;
    struct RTELine *lines;
    UWORD listNumber; /* for ordered lists */
    UBYTE taskState;  /* 0 none, 1 open, 2 done */
    UBYTE pad1;
};

struct RTEPos {
    ULONG para;
    ULONG offset;
};

/* Colours used by attrs. Index 0 is the normal text colour. */
#define RTE_COLOR_CMAP 0 /* value is a colour map number (ESC p[n]) */
#define RTE_COLOR_RGB  1 /* value is 0x00RRGGBB (ESC P[RRGGBB]) */

struct RTEColor {
    ULONG value;
    LONG pen; /* resolved pen while shown, -1 when not resolved */
    UBYTE kind;
    UBYTE allocated; /* pen came from ObtainBestPen and must be released */
    UWORD pad;
};

#define RTE_MAX_COLORS 256

/* Undo records */
#define RTE_U_INSERT 1
#define RTE_U_DELETE 2

struct RTEUndo {
    struct MinNode node;
    UBYTE type;
    UBYTE mergeable;
    UWORD group;
    ULONG index; /* global character index */
    ULONG len;
    UBYTE *text;
    UWORD *attrs;
    ULONG cursorBefore;
    ULONG cursorAfter;
};

/* Inline images */
struct RTEImage {
    struct MinNode node;
    STRPTR source;
    Object *dto;
    struct BitMap *bitmap; /* scaled copy, or the datatype's own bitmap */
    BOOL ownBitmap;
    UWORD width;
    UWORD height;
    UWORD maxWidth; /* the width limit the bitmap was scaled for */
    BOOL failed;
};

/* Pens resolved while the object is set up */
enum {
    RTE_PEN_TEXT = 0,
    RTE_PEN_DIM,
    RTE_PEN_LINK,
    RTE_PEN_CODEBG,
    RTE_PEN_CODETEXT,
    RTE_PEN_QUOTE,
    RTE_PEN_RULE,
    RTE_PEN_MARK,
    RTE_PEN_MARKTEXT,
    RTE_PEN_CURSOR,
    RTE_PEN_HIGHLIGHT,
    RTE_PEN_SHINE,
    RTE_PEN_SHADOW,
    RTE_PEN_COUNT
};

#define RTE_MAX_MACRO 512

struct RTEData {
    Object *obj;
    APTR pool;

    /* document */
    struct RTEPara *paras;
    ULONG numParas;
    ULONG maxParas;
    ULONG totalLength; /* bytes including '\n' between paragraphs */

    /* cursor and selection */
    struct RTEPos cursor;
    struct RTEPos anchor;
    BOOL marking;
    LONG wantX; /* preferred pixel column for up/down */

    UWORD nextBubbleId;
    BOOL undoMergeOK;

    /* options */
    BOOL readOnly;
    BOOL fixedFont;
    BOOL markdown;
    BOOL hideMarkup;
    BOOL hideMarkupSet;
    BOOL autoClip;
    BOOL activeOnClick;
    BOOL activeOnClickSet;
    BOOL convertTabs;
    BOOL wrapWords;
    BOOL pasteStyles;
    BOOL pasteColors;
    BOOL quiet;
    BOOL stickToBottom;
    BOOL globalFlow;
    BOOL overwrite;
    BOOL typeAndSpell;
    BOOL rgbMode;
    BOOL submitEmpty;
    UBYTE globalFlowValue;
    UWORD tabSize;
    BOOL tabSizeSet;
    ULONG wrapMode;
    ULONG wrapBorder;
    ULONG exportWrap;
    ULONG importWrap;
    ULONG importHook;
    ULONG exportHook;
    ULONG rows;
    ULONG columns;
    ULONG undoLevels;
    ULONG maxLength;
    ULONG maxImageHeight;
    struct Hook *submitHook;
    struct Hook *doubleClickHook;
    struct Hook *linkHook;
    struct Hook *imageHook;
    LONG *colorMap;
    Object *slider;
    Object *hslider;
    Object *keyUpFocus;
    STRPTR keywords;
    STRPTR inactiveContents;
    UBYTE matchedKeyword[64];
    STRPTR clickedLink;
    struct TextFont *userCodeFont;

    /* style used for typed text */
    UBYTE typeStyle;
    UBYTE typePen;

    /* soft style and colour at the end of the text, carried from one
     * MUIM_RichTextEditor_Append to the next like a terminal stream */
    UBYTE appendStyle;
    UBYTE appendColor;
    BOOL importCarry;

    /* change tracking */
    BOOL hasChanged;
    BOOL contentsChanged;
    BOOL metaChanged;

    /* notification cache */
    BOOL nAreaMarked;
    BOOL nUndo;
    BOOL nRedo;
    LONG nCursorX;
    LONG nCursorY;
    UBYTE nStyle;
    UBYTE nPen;
    UBYTE nFlow;
    UBYTE nPad;
    LONG nPropEntries;
    LONG nPropVisible;
    LONG nPropFirst;
    LONG nHPropEntries;
    LONG nHPropVisible;
    LONG nHPropFirst;

    /* undo */
    struct MinList undoList;
    struct MinList redoList;
    ULONG undoCount;
    UWORD undoGroup;
    BOOL inUndo;

    /* colours */
    struct RTEColor colors[RTE_MAX_COLORS];
    UWORD numColors;

    /* bubbles */
    struct RichTextBubbleStyle bubbleStyles[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];
    UWORD numBubbleStyles;
    LONG bubbleBgPen[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];
    LONG bubbleTextPen[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];
    BOOL bubbleBgAlloc[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];
    BOOL bubbleTextAlloc[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];
    LONG bubbleCodePen[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];
    BOOL bubbleCodeAlloc[RICHTEXTEDITOR_MAX_BUBBLE_STYLES];

    /* render state, valid between MUIM_Setup and MUIM_Cleanup */
    BOOL setup;
    BOOL shown;
    BOOL active;
    BOOL justActivated; /* the key that activated it is not typed */
    ULONG activatedSecs; /* when it became active */
    BOOL keysDisabled; /* MUI's Tab cycling is off while it is active */
    ULONG activatedMicros;
    BOOL disabled;
    struct TextFont *font;
    struct TextFont *codeFont;
    BOOL ownCodeFont;
    UWORD widths[4][256]; /* normal, bold, code, code bold */
    UWORD fontHeight;
    UWORD fontBaseline;
    UWORD codeHeight;
    UWORD codeBaseline;
    UWORD lineHeight; /* max of both fonts */
    UWORD spaceWidth;
    LONG pens[RTE_PEN_COUNT];
    BOOL bgSampled; /* the real background colour has been looked at */
    ULONG bgRGB;
    BOOL penAlloc[RTE_PEN_COUNT];
    struct MUI_EventHandlerNode eh;
    BOOL ehAdded;
    ULONG ehEvents;

    /* geometry of the text area (window coordinates, valid while shown) */
    LONG viewLeft;
    LONG viewTop;
    LONG viewWidth;
    LONG viewHeight;
    LONG layoutWidth; /* width the layout was made for, 0 = never */
    LONG scrollY;
    LONG scrollX;
    LONG pendingScroll; /* asked for before the object was shown */
    BOOL hasPendingScroll;
    LONG totalHeight;
    LONG maxLineWidth;
    BOOL layoutAll;
    ULONG firstDirty; /* lowest paragraph index marked dirty */
    BOOL anyDirty;

    /* redraw bookkeeping */
    BOOL drawAll;
    LONG dirtyTop; /* document y range to redraw */
    LONG dirtyBottom;
    LONG scrollDelta;
    BOOL inDraw;

    /* double buffer */
    struct BitMap *dbBitMap;
    struct RastPort dbRP;
    LONG dbWidth;
    LONG dbHeight;
    BOOL dbFailed;

    /* mouse */
    BOOL selecting;
    UBYTE clickCount;
    UBYTE selectUnit; /* 0 char, 1 word, 2 paragraph */
    ULONG lastClickSecs;
    ULONG lastClickMicros;
    struct RTEPos clickPos;
    struct RTEPos selStartWord; /* the word/paragraph clicked first */
    struct RTEPos selEndWord;
    LONG pressX;
    LONG pressY;
    LONG mouseX;
    LONG mouseY;
    BOOL dragMoved;
    UWORD pressQualifier;

    /* images */
    struct MinList images;

    /* bookmarks (global character indexes) */
    ULONG bookmarks[10];
    BOOL bookmarkSet[10];

    /* keyboard macro */
    BOOL macroRecording;
    UWORD macroLen;
    UWORD macroCodes[RTE_MAX_MACRO];
    UWORD macroQuals[RTE_MAX_MACRO];
    BOOL macroPlaying;
};

/* RichTextEditorBuffer.c */
BOOL rteInitDocument(struct RTEData *data);
void rteFreeDocument(struct RTEData *data);
BOOL rteParaReserve(struct RTEData *data, struct RTEPara *p, ULONG len);
BOOL rteParaEnsureAttrs(struct RTEData *data, struct RTEPara *p);
void rteMarkDirty(struct RTEData *data, ULONG para);
void rteMarkAllDirty(struct RTEData *data);
ULONG rtePosToIndex(struct RTEData *data, struct RTEPos pos);
struct RTEPos rteIndexToPos(struct RTEData *data, ULONG index);
LONG rteComparePos(struct RTEPos a, struct RTEPos b);
void rteClampPos(struct RTEData *data, struct RTEPos *pos);
void rteOrderedSelection(struct RTEData *data, struct RTEPos *from,
                         struct RTEPos *to);
BOOL rteHasSelection(struct RTEData *data);
BOOL rteInsertText(struct RTEData *data, struct RTEPos pos, CONST_STRPTR text,
                   ULONG len, const UWORD *attrs, struct RTEPos *endPos,
                   BOOL recordUndo);
BOOL rteDeleteRange(struct RTEData *data, struct RTEPos from, struct RTEPos to,
                    BOOL recordUndo);
BOOL rteImport(struct RTEData *data, struct RTEPos pos, CONST_STRPTR text,
               LONG length, BOOL raw, ULONG hook, struct RTEPos *endPos,
               BOOL recordUndo);
STRPTR rteExport(struct RTEData *data, struct RTEPos from, struct RTEPos to,
                 ULONG hook, ULONG wrap);
STRPTR rteExportVisible(struct RTEData *data, struct RTEPos from,
                        struct RTEPos to);
void rteClear(struct RTEData *data);
UWORD rteColorIndex(struct RTEData *data, UBYTE kind, ULONG value);
void rteUndoBeginGroup(struct RTEData *data);
BOOL rteUndo(struct RTEData *data);
BOOL rteRedo(struct RTEData *data);
void rteFreeUndo(struct RTEData *data, BOOL redoOnly);
BOOL rteSearch(struct RTEData *data, CONST_STRPTR string, ULONG flags);
void rteTrimToMaxLength(struct RTEData *data);
UWORD rteAttrAt(struct RTEPara *p, ULONG offset);
void rteSetBlockAttrs(struct RTEData *data, struct RTEPos from,
                      struct RTEPos to, ULONG operation, ULONG value);
BOOL rteIsWordChar(UBYTE c);

/* RichTextEditorLayout.c */
void rteSetupFonts(struct RTEData *data, Object *obj);
void rteLayout(struct RTEData *data);
void rteLayoutParagraph(struct RTEData *data, ULONG index);
void rteLineGeometry(struct RTEData *data, struct RTEPara *p,
                     struct RTELine *l, LONG *left, LONG *extra,
                     LONG *extraRemainder);
LONG rteCharWidth(struct RTEData *data, struct RTEPara *p, ULONG offset,
                  LONG x);
UWORD rteStyleAt(struct RTEPara *p, ULONG offset);
ULONG rteParaAtY(struct RTEData *data, LONG y);
ULONG rteLineAtY(struct RTEPara *p, LONG y);
ULONG rteLineOfOffset(struct RTEPara *p, ULONG offset);
LONG rteOffsetToX(struct RTEData *data, struct RTEPara *p, struct RTELine *l,
                  ULONG offset);
ULONG rteXToOffset(struct RTEData *data, struct RTEPara *p, struct RTELine *l,
                   LONG x);
struct RTEImage *rteGetImage(struct RTEData *data, CONST_STRPTR source,
                             ULONG sourceLen, LONG maxWidth);
void rteFreeImages(struct RTEData *data);
BOOL rteFindSpan(struct RTEPara *p, ULONG offset, BOOL image, ULONG *spanStart,
                 ULONG *spanEnd, ULONG *urlStart, ULONG *urlEnd);
LONG rteBubbleSide(struct RTEData *data, struct RTEPara *p);
LONG rteParaFlow(struct RTEData *data, struct RTEPara *p);

/* RichTextEditorDraw.c */
void rteDrawUpdate(struct RTEData *data, Object *obj, BOOL all);
void rteFreeDoubleBuffer(struct RTEData *data);
void rteObtainPens(struct RTEData *data, Object *obj);
void rteSetDefaultBubbleStyles(struct RTEData *data);
void rteReleasePens(struct RTEData *data, Object *obj);
LONG rteResolveColor(struct RTEData *data, Object *obj, UWORD index);

/* RichTextEditorInput.c */
ULONG rteHandleEvent(struct IClass *cl, Object *obj,
                     struct MUIP_HandleEvent *msg);
BOOL rteCopy(struct RTEData *data, Object *obj, BOOL cut);
BOOL rtePaste(struct RTEData *data, Object *obj);
IPTR rteARexx(struct RTEData *data, Object *obj, CONST_STRPTR command);
void rteUpdateEventHandler(struct RTEData *data, Object *obj, ULONG events);
void rteEndDrag(struct RTEData *data, Object *obj);

/* RichTextEditor.c */
void rteChanged(struct RTEData *data, Object *obj, BOOL contents);
void rteRequestRedraw(struct RTEData *data, Object *obj);
void rteInvalidateRange(struct RTEData *data, LONG top, LONG bottom);
void rteInvalidatePara(struct RTEData *data, ULONG para);
void rteInvalidateCursorLine(struct RTEData *data);
void rteAfterEdit(struct RTEData *data, Object *obj);
void rteAfterCursorMove(struct RTEData *data, Object *obj, BOOL extend);
void rteMakeCursorVisible(struct RTEData *data, Object *obj);
void rteScrollTo(struct RTEData *data, Object *obj, LONG y);
void rteNotifyState(struct RTEData *data, Object *obj);
void rteHandleError(struct RTEData *data, Object *obj, ULONG error);
void rteEnsureLayout(struct RTEData *data, Object *obj);
BOOL rteViewAtBottom(struct RTEData *data);

#define RTE_INOBJECT(o, x, y)                                                  \
    ((x) >= _mleft(o) && (x) <= _mright(o) && (y) >= _mtop(o) &&               \
     (y) <= _mbottom(o))

#define RTE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define RTE_MAX(a, b) ((a) > (b) ? (a) : (b))

/* Spacing in pixels */
#define RTE_PAD_X 3
#define RTE_PAD_Y 2
#define RTE_BUBBLE_PAD_X 8
#define RTE_BUBBLE_PAD_Y 4
#define RTE_BUBBLE_GAP 6
#define RTE_BUBBLE_RADIUS 8
#define RTE_BUBBLE_TAIL 5
#define RTE_CODE_PAD 4
#define RTE_LIST_INDENT_CHARS 2
#define RTE_QUOTE_INDENT 10

#endif /* RTE_PRIVATE_H */
