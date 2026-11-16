#ifndef BESTTEXTEDITOR_CONFIG_H
#define BESTTEXTEDITOR_CONFIG_H

/*
 * Configuration items shared by BestTextEditor.mcc and its preferences class
 * BestTextEditor.mcp.
 *
 * The .mcp stores these in the application's MUI configuration (a Dataspace)
 * and the .mcc reads them back with MUIM_GetConfigItem. An item that is
 * missing means "use the default", so the .mcp only stores values that differ
 * from the defaults below. Both sides must agree on these values; they are
 * private and not part of the public header.
 *
 * Item formats:
 *   font   NUL terminated "name/size" string as written by a font Popasl,
 *          empty for the default font
 *   pen    struct MUI_PenSpec
 *   bool   ULONG, 0 or 1
 *   number ULONG
 */

#include <exec/types.h>
#include <mui/BestTextEditor_mcc.h>

#define BTE_CFG_BASE (BestTextEditor_Dummy + 0x100)

#define MUICFG_BestTextEditor_TextFont      (BTE_CFG_BASE + 0x00) /* font */
#define MUICFG_BestTextEditor_CodeFont      (BTE_CFG_BASE + 0x01) /* font */
#define MUICFG_BestTextEditor_AutoColors    (BTE_CFG_BASE + 0x02) /* bool */
#define MUICFG_BestTextEditor_LinkPen       (BTE_CFG_BASE + 0x03) /* pen  */
#define MUICFG_BestTextEditor_CodeBackPen   (BTE_CFG_BASE + 0x04) /* pen  */
#define MUICFG_BestTextEditor_CodeTextPen   (BTE_CFG_BASE + 0x05) /* pen  */
#define MUICFG_BestTextEditor_QuotePen      (BTE_CFG_BASE + 0x06) /* pen  */
#define MUICFG_BestTextEditor_RulePen       (BTE_CFG_BASE + 0x07) /* pen  */
#define MUICFG_BestTextEditor_CursorPen     (BTE_CFG_BASE + 0x08) /* pen  */
#define MUICFG_BestTextEditor_MarkPen       (BTE_CFG_BASE + 0x09) /* pen  */
#define MUICFG_BestTextEditor_MarkTextPen   (BTE_CFG_BASE + 0x0a) /* pen  */

/* Bubble style n (0..7) uses BTE_CFG_BUBBLE(n) + one of the offsets below */
#define BTE_CFG_BUBBLE(n)          (BTE_CFG_BASE + 0x20 + (n) * 0x08)
#define BTE_CFG_BUBBLE_SIDE        0x00 /* number, MUIV_BestTextEditor_Bubble_* */
#define BTE_CFG_BUBBLE_BACKGROUND  0x01 /* pen  */
#define BTE_CFG_BUBBLE_TEXT        0x02 /* pen  */
#define BTE_CFG_BUBBLE_AUTOTEXT    0x03 /* bool */
#define BTE_CFG_BUBBLE_WIDTH       0x04 /* number, percent */

#define BTE_CFG_LAST (BTE_CFG_BUBBLE(BESTTEXTEDITOR_MAX_BUBBLE_STYLES))

#define BTE_IS_CFG_ID(id) ((id) >= BTE_CFG_BASE && (id) < BTE_CFG_LAST)

/* The accent pens set by hand when AutoColors is off, in config ID order */
#define BTE_CFG_PEN_COUNT 8

/* Defaults. Pen specs use MUI's "m<n>" (MUI pen) and
 * "r<RRRRRRRR>,<GGGGGGGG>,<BBBBBBBB>" (RGB) forms, which MUI 3.8 understands. */
#define BTE_DEFAULT_AUTOCOLORS TRUE

static const char *const bteDefaultPens[BTE_CFG_PEN_COUNT] = {
    "r1a1a1a1a,56565656,c8c8c8c8", /* links */
    "m1",                          /* code background: half shine */
    "m5",                          /* code text: text */
    "m3",                          /* block quotes: half shadow */
    "m3",                          /* rules: half shadow */
    "m6",                          /* cursor: fill */
    "m6",                          /* selection: fill */
    "m0"                           /* selected text: shine */
};

struct BteBubbleDefault {
    ULONG side;
    ULONG background; /* 0x00RRGGBB */
    ULONG text;       /* 0x00RRGGBB or MUIV_BestTextEditor_Color_Auto */
    ULONG maxWidth;   /* percent */
};

static const struct BteBubbleDefault
    bteDefaultBubbles[BESTTEXTEDITOR_MAX_BUBBLE_STYLES] = {
        {MUIV_BestTextEditor_Bubble_Right, 0x2f7ff0, MUIV_BestTextEditor_Color_Auto, 80},
        {MUIV_BestTextEditor_Bubble_Left, 0xe6e6ea, MUIV_BestTextEditor_Color_Auto, 92},
        {MUIV_BestTextEditor_Bubble_Center, 0xfff2bf, MUIV_BestTextEditor_Color_Auto, 90},
        {MUIV_BestTextEditor_Bubble_Left, 0xffd9d6, 0x8a1010, 92},
        {MUIV_BestTextEditor_Bubble_Left, 0xdcf5dc, MUIV_BestTextEditor_Color_Auto, 92},
        {MUIV_BestTextEditor_Bubble_Left, 0xece3fa, MUIV_BestTextEditor_Color_Auto, 92},
        {MUIV_BestTextEditor_Bubble_Full, 0xeeeeee, MUIV_BestTextEditor_Color_Auto, 100},
        {MUIV_BestTextEditor_Bubble_Right, 0x34c759, MUIV_BestTextEditor_Color_Auto, 80}};

#define BTE_MIN_BUBBLE_WIDTH 20

/* Writes the RGB pen spec for 0x00RRGGBB into buf (at least 32 bytes) */
static inline void bteRGBToSpec(ULONG rgb, char *buf) {
    static const char hex[] = "0123456789abcdef";
    int c, i, o = 0;
    buf[o++] = 'r';
    for (c = 2; c >= 0; c--) {
        UBYTE v = (UBYTE)(rgb >> (c * 8));
        for (i = 0; i < 4; i++) {
            buf[o++] = hex[v >> 4];
            buf[o++] = hex[v & 15];
        }
        if (c > 0)
            buf[o++] = ',';
    }
    buf[o] = '\0';
}

#endif /* BESTTEXTEDITOR_CONFIG_H */
