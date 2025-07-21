#include <graphics/text.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <SDI_hook.h>
#include "AboutAmigaGPTWindow.h"
#include "APIKeyRequesterWindow.h"
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "StartupOptionsWindow.h"
#include "MainWindow.h"

Object *startupOptionsWindowObject;
static Object *startupOptionsOkButton;
static Object *screenSelectRadioButton;

HOOKPROTONH(StartupOptionsOkButtonClickedFunc, void,
            Object *screenSelectRadioButton,
            Object *startupOptionsWindowObjecet) {
    LONG selectedRadioButton;
    get(screenSelectRadioButton, MUIA_Radio_Active, &selectedRadioButton);

    if (selectedRadioButton == 0) {
        // Open in Workbench
        screen = LockPubScreen("Workbench");
        isPublicScreen = TRUE;
    } else {
        // New screen
        ULONG displayID = GetVPModeID(&screen->ViewPort);
        struct ScreenModeRequester *screenModeRequester;
        if (screenModeRequester =
                (struct ScreenModeRequester *)MUI_AllocAslRequestTags(
                    ASL_ScreenModeRequest, ASLSM_DoWidth, TRUE, ASLSM_DoHeight,
                    TRUE, ASLSM_DoDepth, TRUE, ASLSM_DoOverscanType, TRUE,
                    ASLSM_DoAutoScroll, TRUE, ASLSM_InitialDisplayID, displayID,
                    ASLSM_InitialDisplayWidth, screen->Width,
                    ASLSM_InitialDisplayHeight, screen->Height,
                    ASLSM_InitialOverscanType, OSCAN_TEXT,
                    ASLSM_InitialDisplayDepth, 4, ASLSM_MinDepth, 4,
                    ASLSM_NegativeText, NULL, TAG_DONE)) {
            if (MUI_AslRequestTags(screenModeRequester, TAG_DONE)) {
                isPublicScreen = FALSE;
                for (WORD i = 0; i < NUMDRIPENS; i++) {
                    pens[i] = 1;
                }
                pens[DETAILPEN] = 4;        // nothing?
                pens[BLOCKPEN] = 4;         // nothing?
                pens[TEXTPEN] = 1;          // text colour
                pens[SHINEPEN] = 1;         // gadget top and left borders
                pens[SHADOWPEN] = 1;        // gadget bottom and right borders
                pens[FILLPEN] = 2;          // button text
                pens[FILLTEXTPEN] = 4;      // title bar text
                pens[BACKGROUNDPEN] = 3;    // background
                pens[HIGHLIGHTTEXTPEN] = 4; // nothing?
                pens[BARDETAILPEN] = 1;     // menu text
                pens[BARBLOCKPEN] = 0;      // menu background
                pens[BARTRIMPEN] = 1;       // nothing?
#ifdef __AMIGAOS4__
                pens[FOREGROUNDPEN] = 0;
                pens[DISABLEDPEN] = 8;
                pens[DISABLEDSHADOWPEN] = 7;
                pens[DISABLEDSHINEPEN] = 6;
                pens[DISABLEDTEXTPEN] = 3;
                pens[MENUBACKGROUNDPEN] = 9;
                pens[MENUTEXTPEN] = 3;
                pens[MENUSHINEPEN] = 8;
                pens[MENUSHADOWPEN] = 0;
                pens[SELECTPEN] = 2;
                pens[SELECTTEXTPEN] = 4;
#endif
                pens[NUMDRIPENS] = ~0;

                if ((screen = OpenScreenTags(
                         NULL, SA_Pens, (ULONG)pens, SA_LikeWorkbench, TRUE,
                         SA_DisplayID, screenModeRequester->sm_DisplayID,
                         SA_Depth, screenModeRequester->sm_DisplayDepth,
                         SA_Overscan, screenModeRequester->sm_OverscanType,
                         SA_AutoScroll, screenModeRequester->sm_AutoScroll,
                         SA_Width, screenModeRequester->sm_DisplayWidth,
                         SA_Height, screenModeRequester->sm_DisplayHeight,
                         TAG_DONE)) == NULL) {
                    displayError(STRING_ERROR_SCREEN);
                }
                MUI_FreeAslRequest(screenModeRequester);
            }
        }
    }

    redPen = ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFFFFFF, 0, 0,
                           OBP_Precision, PRECISION_GUI, TAG_DONE);
    greenPen = ObtainBestPen(screen->ViewPort.ColorMap, 0, 0xBBBBBBBB, 0,
                             OBP_Precision, PRECISION_GUI, TAG_DONE);
    bluePen = ObtainBestPen(screen->ViewPort.ColorMap, 0, 0, 0xFFFFFFFF,
                            OBP_Precision, PRECISION_GUI, TAG_DONE);
    yellowPen = ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFFFFFF, 0xFFFFFFFF,
                              0, OBP_Precision, PRECISION_GUI, TAG_DONE);

    set(startupOptionsWindowObject, MUIA_Window_Open, FALSE);
    set(aboutAmigaGPTWindowObject, MUIA_Window_Screen, screen);
    set(apiKeyRequesterWindowObject, MUIA_Window_Screen, screen);
    set(chatSystemRequesterWindowObject, MUIA_Window_Screen, screen);
    set(mainWindowObject, MUIA_Window_Screen, screen);
    set(mainWindowObject, MUIA_Window_Width, screen->Width);
    set(mainWindowObject, MUIA_Window_Height, screen->Height);
}
MakeHook(StartupOptionsOkButtonClickedHook, StartupOptionsOkButtonClickedFunc);

/**
 * Requester window for the screen the application should open on
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createStartupOptionsWindow() {
    struct ScreenModeRequester *screenModeRequester;
    screen = LockPubScreen("Workbench");
    static STRPTR radioButtonOptions[3] = {NULL};
    radioButtonOptions[0] = STRING_OPEN_IN_WORKBENCH;
    radioButtonOptions[1] = STRING_NEW_SCREEN;

    if (!(startupOptionsWindowObject = WindowObject, MUIA_Window_Title,
          STRING_STARTUP_OPTIONS, MUIA_Window_DepthGadget, FALSE,
          MUIA_Window_SizeGadget, FALSE, WindowContents, VGroup, Child,
          screenSelectRadioButton = RadioObject, MUIA_Frame, MUIV_Frame_Group,
          MUIA_FrameTitle, STRING_SCREEN_TO_OPEN, MUIA_Radio_Entries,
          radioButtonOptions, End, Child, MUI_MakeObject(MUIO_HBar, 10), Child,
          startupOptionsOkButton = MUI_MakeObject(MUIO_Button, STRING_OK), End,
          End)) {
        displayError(STRING_ERROR_STARTUP_OPTIONS);
        return RETURN_ERROR;
    }

    set(startupOptionsOkButton, MUIA_CycleChain, 1);

    return RETURN_OK;
}

void addStartupOptionsWindowActions() {
    DoMethod(startupOptionsWindowObject, MUIM_Notify, MUIA_Window_CloseRequest,
             TRUE, MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);

    DoMethod(startupOptionsOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
             screenSelectRadioButton, 3, MUIM_CallHook,
             &StartupOptionsOkButtonClickedHook, startupOptionsWindowObject);
}