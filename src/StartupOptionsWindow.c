#include <graphics/text.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <proto/muimaster.h>
#include <SDI_hook.h>
#include "AboutAmigaGPTWindow.h"
#include "APIKeyRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "StartupOptionsWindow.h"
#include "MainWindow.h"

Object *startupOptionsWindowObject;
static Object *startupOptionsOkButton;
static Object *screenSelectRadioButton;

static CONST_STRPTR radioButtonOptions[] = {
		"Open in Workbench",
		"New screen",
		NULL
};

HOOKPROTONH(StartupOptionsOkButtonClickedFunc, void, Object *screenSelectRadioButton, Object *startupOptionsWindowObjecet) {
	LONG selectedRadioButton;
	get(screenSelectRadioButton, MUIA_Radio_Active, &selectedRadioButton);
	
	if (selectedRadioButton == 0) {
		// Open in Workbench
		isPublicScreen = TRUE;
	} else {
		// New screen
		ULONG displayID = GetVPModeID(&screen->ViewPort);
		struct ScreenModeRequester *screenModeRequester;
		if (screenModeRequester = (struct ScreenModeRequester *)MUI_AllocAslRequestTags(ASL_ScreenModeRequest,
		ASLSM_DoWidth, TRUE,
		ASLSM_DoHeight, TRUE,
		ASLSM_DoDepth, TRUE,
		ASLSM_DoOverscanType, TRUE,
		ASLSM_DoAutoScroll, TRUE,
		ASLSM_InitialDisplayID, displayID,
		ASLSM_InitialDisplayWidth, screen->Width,
		ASLSM_InitialDisplayHeight, screen->Height,
		ASLSM_InitialOverscanType, OSCAN_TEXT,
		ASLSM_InitialDisplayDepth, 4,
		ASLSM_MinDepth, 4,
		ASLSM_NegativeText, NULL,
		TAG_DONE)) {
			if (MUI_AslRequestTags(screenModeRequester, TAG_DONE)) {
				isPublicScreen = FALSE;
				UnlockPubScreen(NULL, screen);
				for (WORD i = 0; i < NUMDRIPENS; i++) {
					pens[i]= 1;
				}
				pens[DETAILPEN] = 4; // nothing?
				pens[BLOCKPEN] = 4; // nothing?
				pens[TEXTPEN] = 1; // text colour
				pens[SHINEPEN] = 1; // gadget top and left borders
				pens[SHADOWPEN] = 1; // gadget bottom and right borders
				pens[FILLPEN] = 2; // button text
				pens[FILLTEXTPEN] = 4; // title bar text
				pens[BACKGROUNDPEN] = 3; // background
				pens[HIGHLIGHTTEXTPEN] = 4; // nothing?
				pens[BARDETAILPEN] = 1; // menu text
				pens[BARBLOCKPEN] = 0; // menu background
				pens[BARTRIMPEN] = 1; // nothing?
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

				if ((screen = OpenScreenTags(NULL,
					SA_Pens, (ULONG)pens,
					SA_LikeWorkbench, TRUE,
					SA_DisplayID, screenModeRequester->sm_DisplayID,
					SA_Depth, screenModeRequester->sm_DisplayDepth,
					SA_Overscan, screenModeRequester->sm_OverscanType,
					SA_AutoScroll, screenModeRequester->sm_AutoScroll,
					SA_Width, screenModeRequester->sm_DisplayWidth,
					SA_Height, screenModeRequester->sm_DisplayHeight,
					SA_Font, &screenFont,
					SA_Colors32, config.colors,
					TAG_DONE)) == NULL) {
						displayError("Could not open screen");
						MUI_FreeAslRequest(screenModeRequester);
						return RETURN_ERROR;
				}
				MUI_FreeAslRequest(screenModeRequester);
			}
		}
	}

	set(startupOptionsWindowObject, MUIA_Window_Open, FALSE);
	set(aboutAmigaGPTWindowObject, MUIA_Window_Screen, screen);	
	set(apiKeyRequesterWindowObject, MUIA_Window_Screen, screen);
	set(mainWindowObject, MUIA_Window_DepthGadget, isPublicScreen);
	set(mainWindowObject, MUIA_Window_SizeGadget, isPublicScreen);
	set(mainWindowObject, MUIA_Window_DragBar, isPublicScreen);
	set(mainWindowObject, MUIA_Window_Screen, screen);
	set(mainWindowObject, MUIA_Window_Width, MUIV_Window_Width_Visible(isPublicScreen ? 90 : 100));
	set(mainWindowObject, MUIA_Window_Height, MUIV_Window_Height_Visible(isPublicScreen ? 90 : 100));
	set(mainWindowObject, MUIA_Window_Open, TRUE);
}
MakeHook(StartupOptionsOkButtonClickedHook, StartupOptionsOkButtonClickedFunc);

/**
 * Requester window for the screen the application should open on
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG createStartupOptionsWindow() {
	struct ScreenModeRequester *screenModeRequester;
	screen = LockPubScreen("Workbench");

	if (config.uiFontName != NULL) {
		screenFont.ta_Name = config.uiFontName;
		screenFont.ta_YSize = config.uiFontSize;
		screenFont.ta_Style = config.uiFontStyle;
		screenFont.ta_Flags = config.uiFontFlags;
	}

	if (!(startupOptionsWindowObject = WindowObject,
		MUIA_Window_Title, "Startup Options",
		MUIA_Window_CloseGadget, TRUE,
		MUIA_Window_DepthGadget, FALSE,
		MUIA_Window_SizeGadget, FALSE,
		WindowContents, VGroup,	
			Child, screenSelectRadioButton = RadioObject,
				MUIA_Frame, MUIV_Frame_Group,
				MUIA_FrameTitle, "Screuen to open:",
				MUIA_HelpNode, "radioButton",
				MUIA_Radio_Entries, radioButtonOptions,
			End,
			Child, MUI_MakeObject(MUIO_HBar,10),
			Child, startupOptionsOkButton = MUI_MakeObject(MUIO_Button, "OK"),
		End,
    End)) {
		displayError("Could not create startupOptionsWindowObject\n");
		return RETURN_ERROR;
	}

	set(startupOptionsOkButton, MUIA_CycleChain, 1);

	return RETURN_OK;
}

void addStartupOptionsWindowActions() {
    DoMethod(startupOptionsWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
	  app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

	DoMethod(startupOptionsOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
          screenSelectRadioButton, 2, MUIM_CallHook, &StartupOptionsOkButtonClickedHook, startupOptionsWindowObject);
}