#ifndef BESTTEXTEDITOR_MCP_PRIVATE_H
#define BESTTEXTEDITOR_MCP_PRIVATE_H

#include <exec/types.h>
#include <libraries/mui.h>
#include "BestTextEditorConfig.h"

/* Popimage attributes MUI 3.8, MUI 4 and MUI 5 all have but the public
 * headers leave out; NList.mcp and TextEditor.mcp use the same values */
#ifndef MUIA_Imagedisplay_Spec
#define MUIA_Imagedisplay_Spec 0x8042a547
#endif
#ifndef MUIA_Imageadjust_Type
#define MUIA_Imageadjust_Type 0x80422f2b
#endif
#ifndef MUIV_Imageadjust_Type_Background
#define MUIV_Imageadjust_Type_Background 2
#endif

struct BubbleGadgets {
    Object *side;
    Object *width;
    Object *background;
    Object *autoText;
    Object *text;
};

struct PrefsData {
    Object *textFont;
    Object *codeFont;
    Object *editBack;
    Object *editText;
    Object *readBack;
    Object *readText;
    Object *autoColors;
    Object *penGroup;
    Object *pens[BTE_CFG_PEN_COUNT];
    struct BubbleGadgets bubble[BESTTEXTEDITOR_MAX_BUBBLE_STYLES];
};

/* BestTextEditorPrefs.c */
void prefsInitStrings(void);

/* catalog.c */
void prefsOpenCatalog(void);
void prefsCloseCatalog(void);

#endif
