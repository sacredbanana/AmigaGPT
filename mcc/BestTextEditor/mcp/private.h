#ifndef BESTTEXTEDITOR_MCP_PRIVATE_H
#define BESTTEXTEDITOR_MCP_PRIVATE_H

#include <exec/types.h>
#include <libraries/mui.h>
#include "BestTextEditorConfig.h"

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
