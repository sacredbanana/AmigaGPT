/*
 * BestTextEditor.mcp - the BestTextEditor page of MUI preferences
 *
 * Fonts, accent colours and the eight chat bubble styles. Values are kept in
 * the application's MUI configuration under the MUICFG_BestTextEditor_* IDs
 * from BestTextEditorConfig.h, and only when they differ from the defaults,
 * so a missing item always means "default" on the .mcc side.
 */
#include <clib/alib_protos.h>
#include <exec/memory.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <string.h>
#include <SDI_compiler.h>
#include <SDI_hook.h>
#include <mui/BestTextEditor_mcc.h>
#include "BestTextEditorConfig.h"
#include "BestTextEditor_mcp_cat.h"
#include "private.h"

#define FONT_SPEC_LENGTH 64

/* Localised labels, filled once by prefsInitStrings() after the catalog is
 * open. They are shared by every prefs object and never change. */
static CONST_STRPTR pageTitles[3];
static CONST_STRPTR bubbleTitles[BESTTEXTEDITOR_MAX_BUBBLE_STYLES + 1];
static CONST_STRPTR sideNames[5];
static CONST_STRPTR penLabels[BTE_CFG_PEN_COUNT];

void prefsInitStrings(void) {
    static const char *const numbers[] = {"1", "2", "3", "4",
                                          "5", "6", "7", "8"};
    UWORD i;

    pageTitles[0] = MCP_PAGE_TEXT;
    pageTitles[1] = MCP_PAGE_BUBBLES;
    pageTitles[2] = NULL;
    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++)
        bubbleTitles[i] = (CONST_STRPTR)numbers[i];
    bubbleTitles[BESTTEXTEDITOR_MAX_BUBBLE_STYLES] = NULL;
    /* in MUIV_BestTextEditor_Bubble_* order, starting at Left (1) */
    sideNames[0] = MCP_ALIGN_LEFT;
    sideNames[1] = MCP_ALIGN_RIGHT;
    sideNames[2] = MCP_ALIGN_CENTER;
    sideNames[3] = MCP_ALIGN_FULL;
    sideNames[4] = NULL;
    penLabels[0] = MCP_PEN_LINKS;
    penLabels[1] = MCP_PEN_CODE_BACKGROUND;
    penLabels[2] = MCP_PEN_CODE_TEXT;
    penLabels[3] = MCP_PEN_QUOTES;
    penLabels[4] = MCP_PEN_RULES;
    penLabels[5] = MCP_PEN_CURSOR;
    penLabels[6] = MCP_PEN_SELECTION;
    penLabels[7] = MCP_PEN_SELECTED_TEXT;
}

/* ------------------------------------------------------------------------ */
/* Gadgets                                                                   */
/* ------------------------------------------------------------------------ */

static Object *makeFontPopup(Object **string, CONST_STRPTR title,
                             BOOL fixedOnly) {
    // clang-format off
    return PopaslObject,
        MUIA_Popstring_String, *string = StringObject,
            StringFrame,
            MUIA_String_MaxLen, FONT_SPEC_LENGTH,
            MUIA_CycleChain, TRUE,
            MUIA_ShortHelp, MCP_FONT_HELP,
        End,
        MUIA_Popstring_Button, PopButton(MUII_PopUp),
        MUIA_Popasl_Type, ASL_FontRequest,
        ASLFO_TitleText, title,
        ASLFO_FixedWidthOnly, fixedOnly,
    End;
    // clang-format on
}

static Object *makePen(CONST_STRPTR title) {
    // clang-format off
    return PoppenObject,
        MUIA_Window_Title, title,
        MUIA_CycleChain, TRUE,
        MUIA_ShortHelp, title,
    End;
    // clang-format on
}

static Object *makeBubblePage(struct BubbleGadgets *g) {
    // clang-format off
    return ColGroup(2),
        Child, Label1(MCP_ALIGNMENT),
        Child, g->side = CycleObject,
            MUIA_Cycle_Entries, sideNames,
            MUIA_CycleChain, TRUE,
        End,
        Child, Label1(MCP_MAX_WIDTH),
        Child, g->width = SliderObject,
            MUIA_Numeric_Min, BTE_MIN_BUBBLE_WIDTH,
            MUIA_Numeric_Max, 100,
            MUIA_Numeric_Format, "%ld %%",
            MUIA_CycleChain, TRUE,
        End,
        Child, Label1(MCP_BACKGROUND),
        Child, g->background = makePen(MCP_BACKGROUND),
        Child, Label1(MCP_AUTO_TEXT),
        Child, HGroup,
            Child, g->autoText = CheckMark(TRUE),
            Child, HSpace(0),
        End,
        Child, Label1(MCP_TEXT_COLOR),
        Child, g->text = makePen(MCP_TEXT_COLOR),
    End;
    // clang-format on
}

/* ------------------------------------------------------------------------ */
/* Configuration <-> gadgets                                                 */
/* ------------------------------------------------------------------------ */

static void setPen(Object *pen, CONST_STRPTR spec) {
    struct MUI_PenSpec ps;
    memset(&ps, 0, sizeof(ps));
    strncpy(ps.buf, (const char *)spec, sizeof(ps.buf) - 1);
    set(pen, MUIA_Pendisplay_Spec, &ps);
}

/* The pen spec a bubble's colour gadget shows by default */
static void defaultBubbleSpec(ULONG rgb, char *buf) {
    if (rgb == MUIV_BestTextEditor_Color_Auto)
        rgb = 0x000000;
    bteRGBToSpec(rgb, buf);
}

static ULONG findULong(Object *config, ULONG id, ULONG def) {
    ULONG *v = (ULONG *)DoMethod(config, MUIM_Dataspace_Find, id);
    return v != NULL ? *v : def;
}

static CONST_STRPTR findString(Object *config, ULONG id, CONST_STRPTR def) {
    CONST_STRPTR s = (CONST_STRPTR)DoMethod(config, MUIM_Dataspace_Find, id);
    return s != NULL ? s : def;
}

static IPTR mConfigToGadgets(struct IClass *cl, Object *obj,
                             struct MUIP_Mccprefs_ConfigToGadgets *msg) {
    struct PrefsData *data = INST_DATA(cl, obj);
    Object *config = msg->configdata;
    char def[32];
    UWORD i;

    set(data->textFont, MUIA_String_Contents,
        findString(config, MUICFG_BestTextEditor_TextFont, ""));
    set(data->codeFont, MUIA_String_Contents,
        findString(config, MUICFG_BestTextEditor_CodeFont, ""));
    set(data->autoColors, MUIA_Selected,
        findULong(config, MUICFG_BestTextEditor_AutoColors,
                  BTE_DEFAULT_AUTOCOLORS) != 0);
    for (i = 0; i < BTE_CFG_PEN_COUNT; i++)
        setPen(data->pens[i],
               findString(config, MUICFG_BestTextEditor_LinkPen + i,
                          (CONST_STRPTR)bteDefaultPens[i]));

    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++) {
        const struct BteBubbleDefault *d = &bteDefaultBubbles[i];
        struct BubbleGadgets *g = &data->bubble[i];
        ULONG base = BTE_CFG_BUBBLE(i);
        ULONG side = findULong(config, base + BTE_CFG_BUBBLE_SIDE, d->side);

        if (side < MUIV_BestTextEditor_Bubble_Left ||
            side > MUIV_BestTextEditor_Bubble_Full)
            side = d->side;
        set(g->side, MUIA_Cycle_Active, side - MUIV_BestTextEditor_Bubble_Left);
        set(g->width, MUIA_Numeric_Value,
            findULong(config, base + BTE_CFG_BUBBLE_WIDTH, d->maxWidth));
        bteRGBToSpec(d->background, def);
        setPen(g->background,
               findString(config, base + BTE_CFG_BUBBLE_BACKGROUND,
                          (CONST_STRPTR)def));
        set(g->autoText, MUIA_Selected,
            findULong(config, base + BTE_CFG_BUBBLE_AUTOTEXT,
                      d->text == MUIV_BestTextEditor_Color_Auto) != 0);
        defaultBubbleSpec(d->text, def);
        setPen(g->text, findString(config, base + BTE_CFG_BUBBLE_TEXT,
                                   (CONST_STRPTR)def));
    }
    return 0;
}

/* Store a value, or remove it when it is the default */
static void storeULong(Object *config, ULONG id, ULONG value, ULONG def) {
    if (value == def)
        DoMethod(config, MUIM_Dataspace_Remove, id);
    else
        DoMethod(config, MUIM_Dataspace_Add, (IPTR)&value, sizeof(value), id);
}

static void storeString(Object *config, ULONG id, CONST_STRPTR value) {
    if (value == NULL || value[0] == '\0')
        DoMethod(config, MUIM_Dataspace_Remove, id);
    else
        DoMethod(config, MUIM_Dataspace_Add, (IPTR)value,
                 strlen((const char *)value) + 1, id);
}

static void storePen(Object *config, ULONG id, Object *pen,
                     CONST_STRPTR def) {
    struct MUI_PenSpec *spec = NULL;
    get(pen, MUIA_Pendisplay_Spec, &spec);
    if (spec == NULL || strcmp(spec->buf, (const char *)def) == 0)
        DoMethod(config, MUIM_Dataspace_Remove, id);
    else
        DoMethod(config, MUIM_Dataspace_Add, (IPTR)spec, sizeof(*spec), id);
}

static ULONG getULong(Object *o, ULONG attr) {
    IPTR v = 0;
    get(o, attr, &v);
    return (ULONG)v;
}

static IPTR mGadgetsToConfig(struct IClass *cl, Object *obj,
                             struct MUIP_Mccprefs_GadgetsToConfig *msg) {
    struct PrefsData *data = INST_DATA(cl, obj);
    Object *config = msg->configdata;
    STRPTR s = NULL;
    char def[32];
    UWORD i;

    get(data->textFont, MUIA_String_Contents, &s);
    storeString(config, MUICFG_BestTextEditor_TextFont, s);
    s = NULL;
    get(data->codeFont, MUIA_String_Contents, &s);
    storeString(config, MUICFG_BestTextEditor_CodeFont, s);
    storeULong(config, MUICFG_BestTextEditor_AutoColors,
               getULong(data->autoColors, MUIA_Selected) != 0,
               BTE_DEFAULT_AUTOCOLORS);
    for (i = 0; i < BTE_CFG_PEN_COUNT; i++)
        storePen(config, MUICFG_BestTextEditor_LinkPen + i, data->pens[i],
                 (CONST_STRPTR)bteDefaultPens[i]);

    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++) {
        const struct BteBubbleDefault *d = &bteDefaultBubbles[i];
        struct BubbleGadgets *g = &data->bubble[i];
        ULONG base = BTE_CFG_BUBBLE(i);

        storeULong(config, base + BTE_CFG_BUBBLE_SIDE,
                   getULong(g->side, MUIA_Cycle_Active) +
                       MUIV_BestTextEditor_Bubble_Left,
                   d->side);
        storeULong(config, base + BTE_CFG_BUBBLE_WIDTH,
                   getULong(g->width, MUIA_Numeric_Value), d->maxWidth);
        bteRGBToSpec(d->background, def);
        storePen(config, base + BTE_CFG_BUBBLE_BACKGROUND, g->background,
                 (CONST_STRPTR)def);
        storeULong(config, base + BTE_CFG_BUBBLE_AUTOTEXT,
                   getULong(g->autoText, MUIA_Selected) != 0,
                   d->text == MUIV_BestTextEditor_Color_Auto);
        defaultBubbleSpec(d->text, def);
        storePen(config, base + BTE_CFG_BUBBLE_TEXT, g->text,
                 (CONST_STRPTR)def);
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Object                                                                    */
/* ------------------------------------------------------------------------ */

/* MUI 4 and later add a context menu (defaults, last saved, presets) and
 * real time preview to registered gadgets. MUI 3.8 has no such method. */
static void registerGadget(Object *obj, Object *gadget, ULONG id,
                           CONST_STRPTR title, ULONG attr) {
    if (MUIMasterBase->lib_Version < 20)
        return;
    DoMethod(obj, MUIM_Mccprefs_RegisterGadget, (IPTR)gadget, id, 2,
             (IPTR)title, attr);
}

static IPTR mNew(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct PrefsData tmp;
    struct PrefsData *data;
    struct TagItem tags[3];
    struct opSet superMsg;
    Object *bubblePages[BESTTEXTEDITOR_MAX_BUBBLE_STYLES];
    Object *page;
    UWORD i;

    memset(&tmp, 0, sizeof(tmp));
    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++)
        bubblePages[i] = makeBubblePage(&tmp.bubble[i]);

    // clang-format off
    page = RegisterGroup(pageTitles),
        MUIA_CycleChain, TRUE,

        Child, VGroup,
            Child, ColGroup(2), GroupFrameT(MCP_GROUP_FONTS),
                Child, Label2(MCP_TEXT_FONT),
                Child, makeFontPopup(&tmp.textFont, MCP_TEXT_FONT, FALSE),
                Child, Label2(MCP_CODE_FONT),
                Child, makeFontPopup(&tmp.codeFont, MCP_CODE_FONT, TRUE),
            End,
            Child, VGroup, GroupFrameT(MCP_GROUP_COLORS),
                Child, HGroup,
                    Child, tmp.autoColors = CheckMark(BTE_DEFAULT_AUTOCOLORS),
                    Child, LLabel1(MCP_AUTO_COLORS),
                    Child, HSpace(0),
                End,
                Child, tmp.penGroup = ColGroup(4),
                    Child, Label1(penLabels[0]), Child, tmp.pens[0] = makePen(penLabels[0]),
                    Child, Label1(penLabels[1]), Child, tmp.pens[1] = makePen(penLabels[1]),
                    Child, Label1(penLabels[2]), Child, tmp.pens[2] = makePen(penLabels[2]),
                    Child, Label1(penLabels[3]), Child, tmp.pens[3] = makePen(penLabels[3]),
                    Child, Label1(penLabels[4]), Child, tmp.pens[4] = makePen(penLabels[4]),
                    Child, Label1(penLabels[5]), Child, tmp.pens[5] = makePen(penLabels[5]),
                    Child, Label1(penLabels[6]), Child, tmp.pens[6] = makePen(penLabels[6]),
                    Child, Label1(penLabels[7]), Child, tmp.pens[7] = makePen(penLabels[7]),
                End,
            End,
            Child, VSpace(0),
        End,

        Child, VGroup,
            Child, TextObject,
                MUIA_Text_Contents, MCP_BUBBLES_INFO,
                MUIA_Text_SetMin, FALSE,
            End,
            Child, RegisterGroup(bubbleTitles),
                MUIA_CycleChain, TRUE,
                Child, bubblePages[0],
                Child, bubblePages[1],
                Child, bubblePages[2],
                Child, bubblePages[3],
                Child, bubblePages[4],
                Child, bubblePages[5],
                Child, bubblePages[6],
                Child, bubblePages[7],
            End,
            Child, VSpace(0),
        End,
    End;
    // clang-format on

    if (page == NULL)
        return 0;

    tags[0].ti_Tag = Child;
    tags[0].ti_Data = (IPTR)page;
    tags[1].ti_Tag = TAG_MORE;
    tags[1].ti_Data = (IPTR)msg->ops_AttrList;
    superMsg.MethodID = OM_NEW;
    superMsg.ops_AttrList = tags;
    superMsg.ops_GInfo = msg->ops_GInfo;
    obj = (Object *)DoSuperMethodA(cl, obj, (Msg)&superMsg);
    if (obj == NULL) {
        MUI_DisposeObject(page);
        return 0;
    }
    data = INST_DATA(cl, obj);
    *data = tmp;

    /* Hand picked colours only matter while Automatic colours is off */
    DoMethod(data->autoColors, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
             (IPTR)data->penGroup, 3, MUIM_Set, MUIA_Disabled,
             MUIV_TriggerValue);
    set(data->penGroup, MUIA_Disabled, BTE_DEFAULT_AUTOCOLORS);

    registerGadget(obj, data->textFont, MUICFG_BestTextEditor_TextFont,
                   MCP_TEXT_FONT, MUIA_String_Contents);
    registerGadget(obj, data->codeFont, MUICFG_BestTextEditor_CodeFont,
                   MCP_CODE_FONT, MUIA_String_Contents);
    registerGadget(obj, data->autoColors, MUICFG_BestTextEditor_AutoColors,
                   MCP_AUTO_COLORS, MUIA_Selected);
    for (i = 0; i < BTE_CFG_PEN_COUNT; i++)
        registerGadget(obj, data->pens[i], MUICFG_BestTextEditor_LinkPen + i,
                       penLabels[i], MUIA_Pendisplay_Spec);

    for (i = 0; i < BESTTEXTEDITOR_MAX_BUBBLE_STYLES; i++) {
        struct BubbleGadgets *g = &data->bubble[i];
        ULONG base = BTE_CFG_BUBBLE(i);
        BOOL autoText = bteDefaultBubbles[i].text == MUIV_BestTextEditor_Color_Auto;

        DoMethod(g->autoText, MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
                 (IPTR)g->text, 3, MUIM_Set, MUIA_Disabled, MUIV_TriggerValue);
        set(g->autoText, MUIA_Selected, autoText);
        set(g->text, MUIA_Disabled, autoText);

        registerGadget(obj, g->side, base + BTE_CFG_BUBBLE_SIDE, MCP_ALIGNMENT,
                       MUIA_Cycle_Active);
        registerGadget(obj, g->width, base + BTE_CFG_BUBBLE_WIDTH,
                       MCP_MAX_WIDTH, MUIA_Numeric_Value);
        registerGadget(obj, g->background, base + BTE_CFG_BUBBLE_BACKGROUND,
                       MCP_BACKGROUND, MUIA_Pendisplay_Spec);
        registerGadget(obj, g->autoText, base + BTE_CFG_BUBBLE_AUTOTEXT,
                       MCP_AUTO_TEXT, MUIA_Selected);
        registerGadget(obj, g->text, base + BTE_CFG_BUBBLE_TEXT,
                       MCP_TEXT_COLOR, MUIA_Pendisplay_Spec);
    }
    return (IPTR)obj;
}

DISPATCHER(_DispatcherP) {
    switch (msg->MethodID) {
    case OM_NEW:
        return mNew(cl, obj, (struct opSet *)msg);
    case MUIM_Mccprefs_ConfigToGadgets:
        return mConfigToGadgets(cl, obj,
                                (struct MUIP_Mccprefs_ConfigToGadgets *)msg);
    case MUIM_Mccprefs_GadgetsToConfig:
        return mGadgetsToConfig(cl, obj,
                                (struct MUIP_Mccprefs_GadgetsToConfig *)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}
