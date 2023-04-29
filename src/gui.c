#include "gui.h"
#include <stdio.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/radiobutton.h>
#include <gadgets/texteditor.h>
#include <gadgets/scroller.h>
#include <gadgets/string.h>
#include <devices/conunit.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>
#include <proto/button.h>
#include <proto/radiobutton.h>
#include <proto/window.h>
#include <proto/texteditor.h>
#include <proto/string.h>
#include <proto/asl.h>
#include <proto/scroller.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <classes/window.h>
#include "speech.h"
#include "openai.h"
#include <stdbool.h>

#define MAIN_WIN_WIDTH 640
#define MAIN_WIN_HEIGHT 500
#define SCREEN_SELECT_WINDOW_WIDTH 200
#define SCREEN_SELECT_WINDOW_HEIGHT 50
#define SCREEN_SELECT_RADIO_BUTTON_ID 0
#define SCREEN_SELECT_RADIO_BUTTON_WIDTH 100
#define SCREEN_SELECT_RADIO_BUTTON_HEIGHT 30
#define SCREEN_SELECT_OK_BUTTON_ID 1
#define SCREEN_SELECT_OK_BUTTON_WIDTH 100
#define SCREEN_SELECT_OK_BUTTON_HEIGHT 30
#define SEND_MESSAGE_BUTTON_ID 2
#define SEND_MESSAGE_BUTTON_WIDTH 100
#define SEND_MESSAGE_BUTTON_HEIGHT 20
#define TEXT_INPUT_TEXT_EDITOR_ID 3
#define TEXT_INPUT_TEXT_EDITOR_WIDTH 300
#define TEXT_INPUT_TEXT_EDITOR_HEIGHT 100
#define CHAT_OUTPUT_TEXT_EDITOR_ID 4
#define CHAT_OUTPUT_TEXT_EDITOR_WIDTH 300
#define CHAT_OUTPUT_TEXT_EDITOR_HEIGHT 100
#define CHAT_OUTPUT_SCROLLER_ID 5
#define CHAT_OUTPUT_SCROLLER_WIDTH 20
#define CHAT_OUTPUT_SCROLLER_HEIGHT 100
#define STATUS_BAR_ID 6
#define STATUS_BAR_WIDTH 100
#define STATUS_BAR_HEIGHT 20

#define MENU_ITEM_ABOUT_ID 1
#define MENU_ITEM_PREFERENCES_ID 2
#define MENU_ITEM_QUIT_ID 3
#define MENU_ITEM_SPEECH_ENABLED_ID 4
#define MENU_ITEM_FONT_ID 5
#define MENU_ITEM_SPEECH_SYSTEM_OLD_ID 6
#define MENU_ITEM_SPEECH_SYSTEM_NEW_ID 7

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *AslBase;
static struct Library *WindowBase;
static struct Library *LayoutBase;
static struct Library *ButtonBase;
static struct Library *RadioButtonBase;
static struct Library *TextFieldBase;
static struct Library *ScrollerBase;
static struct Library *StringBase;
static struct Window *mainWindow;
static Object *mainWindowObject;
static Object *mainLayout;
static Object *chatLayout;
static Object *chatInputLayout;
static Object *chatOutputLayout;
static Object *sendMessageButton;
static Object *textInputTextEditor;
static Object *chatOutputTextEditor;
static Object *chatOutputScroller;
static Object *statusBar;
static struct Screen *screen;
static BOOL isPublicScreen;
static UWORD pens[] = {~0};
static BOOL isSpeechEnabled;
static enum SpeechSystem speechSystem;

static struct NewMenu amigaGPTMenu[] = {
	{NM_TITLE, "Project", 0, 0, 0, 0},
	{NM_ITEM, "About", 0, 0, 0, MENU_ITEM_ABOUT_ID},
	{NM_ITEM, "Preferences", "P", 0, 0, MENU_ITEM_PREFERENCES_ID},
	{NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},
	{NM_ITEM, "Quit", "Q", 0, 0, MENU_ITEM_QUIT_ID},
	{NM_TITLE, "View", 0, 0, 0, 0},
	{NM_ITEM, "Font", 0, 0, 0, MENU_ITEM_FONT_ID},
	{NM_TITLE, "Speech", 0, 0, 0, 0},
	{NM_ITEM, "Enabled", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_ENABLED_ID},
	{NM_ITEM, "Speech system", 0, 0, 0, 0},
	{NM_SUB, "Old", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_SYSTEM_OLD_ID},
	{NM_SUB, "New", 0, CHECKIT, 0, MENU_ITEM_SPEECH_SYSTEM_NEW_ID},
	{NM_END, NULL, 0, 0, 0, 0}
};

static void sendMessage();
static void closeGUILibraries();
static LONG selectScreen();

LONG openGUILibraries() {
	if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 47)) == NULL) {
		printf("Could not open intuition.library\n");
        return RETURN_ERROR;
	}

	if ((WindowBase = OpenLibrary("window.class", 47)) == NULL) {
		printf("Could not open window.class\n");
        return RETURN_ERROR;
	}

	if ((LayoutBase = OpenLibrary("gadgets/layout.gadget", 47)) == NULL) {
		printf("Could not open layout.gadget\n");
        return RETURN_ERROR;
	}

	if ((ButtonBase = OpenLibrary("gadgets/button.gadget", 47)) == NULL) {
		printf("Could not open button.gadget\n");
        return RETURN_ERROR;
	}

	if ((RadioButtonBase = OpenLibrary("gadgets/radiobutton.gadget", 47)) == NULL) {
		printf("Could not open radiobutton.gadget\n");
        return RETURN_ERROR;
	}

	if ((TextFieldBase = OpenLibrary("gadgets/texteditor.gadget", 47)) == NULL) {
		printf("Could not open texteditor.gadget\n");
        return RETURN_ERROR;
	}

	if ((ScrollerBase = OpenLibrary("gadgets/scroller.gadget", 47)) == NULL) {
		printf("Could not open scroller.gadget\n");
        return RETURN_ERROR;
	}

	if ((StringBase = OpenLibrary("gadgets/string.gadget", 47)) == NULL) {
		printf("Could not open string.gadget\n");
        return RETURN_ERROR;
	}
	
	if ((GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 47)) == NULL) {
		printf( "Could not open graphics.library\n");
        return RETURN_ERROR;
	}

	if ((AslBase = OpenLibrary("asl.library", 47)) == NULL) {
		printf( "Could not open asl.library\n");
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

static void closeGUILibraries() {
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
	CloseLibrary(WindowBase);
	CloseLibrary(LayoutBase);
	CloseLibrary(ButtonBase);
	CloseLibrary(TextFieldBase);
	CloseLibrary(RadioButtonBase);
	CloseLibrary(AslBase);
}

LONG initVideo() {
	if (openGUILibraries() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	if (selectScreen() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	if ((sendMessageButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, SEND_MESSAGE_BUTTON_ID,
		GA_WIDTH, SEND_MESSAGE_BUTTON_WIDTH,
		GA_HEIGHT, SEND_MESSAGE_BUTTON_HEIGHT,
		BUTTON_TextPen, 3,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)"Send",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create send message button\n");
			return RETURN_ERROR;
		}

	if ((chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Width, CHAT_OUTPUT_TEXT_EDITOR_WIDTH,
		GA_Height, CHAT_OUTPUT_TEXT_EDITOR_HEIGHT,
		GA_ReadOnly, TRUE,
		GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_Plain,
		GA_TEXTEDITOR_ExportHook, GV_TEXTEDITOR_ExportHook_Plain,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	if ((statusBar = NewObject(STRING_GetClass(), NULL,
		GA_ID, STATUS_BAR_ID,
		GA_RelVerify, TRUE,
		GA_Width, STATUS_BAR_WIDTH,
		GA_Height, STATUS_BAR_HEIGHT,
		GA_ReadOnly, TRUE,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	if ((textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Text, (ULONG)"",
		GA_Width, TEXT_INPUT_TEXT_EDITOR_WIDTH,
		GA_Height, TEXT_INPUT_TEXT_EDITOR_HEIGHT,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	if ((chatOutputScroller = NewObject(SCROLLER_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_SCROLLER_ID,
		GA_RelVerify, TRUE,
		GA_Width, CHAT_OUTPUT_SCROLLER_WIDTH,
		GA_Height, CHAT_OUTPUT_SCROLLER_HEIGHT,
		SCROLLER_Top, 0,
		SCROLLER_Visible, 100,
		SCROLLER_Total, 100,
		TAG_DONE)) == NULL) {
			printf("Could not create scroller\n");
			return RETURN_ERROR;
	}
	
	if ((chatInputLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
		LAYOUT_HorizAlignment, LALIGN_CENTER,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, textInputTextEditor,
		CHILD_WeightedWidth, 100,
		LAYOUT_AddChild, sendMessageButton,
		CHILD_WeightedWidth, 10,
		TAG_DONE)) == NULL) {
			printf("Could not create chat input layout\n");
			return RETURN_ERROR;
	}

	if ((chatOutputLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
		LAYOUT_HorizAlignment, LALIGN_CENTER,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, chatOutputTextEditor,
		CHILD_WeightedWidth, 100,
		LAYOUT_AddChild, chatOutputScroller,
		CHILD_WeightedWidth, 10,
		TAG_DONE)) == NULL) {
			printf("Could not create chat output layout\n");
			return RETURN_ERROR;
	}

	if ((chatLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, chatOutputLayout,
		CHILD_WeightedHeight, 70,
		LAYOUT_AddChild, chatInputLayout,
		CHILD_WeightedHeight, 20,
		LAYOUT_AddChild, statusBar,
		CHILD_WeightedHeight, 10,
		TAG_DONE)) == NULL) {
			printf("Could not create chat layout\n");
			return RETURN_ERROR;
	}

	if ((mainLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
		LAYOUT_DeferLayout, TRUE,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, chatLayout,
		CHILD_WeightedWidth, 100,
		TAG_DONE)) == NULL) {
			printf("Could not create main layout\n");
			return RETURN_ERROR;
	}
	
	if ((mainWindowObject = NewObject(WINDOW_GetClass(), NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_Activate, TRUE,
		WA_Title, "AmigaGPT",
		WA_InnerWidth, MAIN_WIN_WIDTH,
		WA_InnerHeight, MAIN_WIN_HEIGHT,
		WA_CloseGadget, TRUE,
		WA_DragBar, isPublicScreen,
		WA_SizeGadget, isPublicScreen,
		WA_DepthGadget, isPublicScreen,
		WA_NewLookMenus, TRUE,
		WINDOW_IconifyGadget, isPublicScreen,
		WINDOW_Layout, mainLayout,
		WINDOW_SharedPort, NULL,
		WINDOW_Position, isPublicScreen ? WPOS_CENTERSCREEN : WPOS_FULLSCREEN,
		WINDOW_NewMenu, amigaGPTMenu,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_MENUPICK,
		WA_CustomScreen, screen,
		TAG_DONE)) == NULL) {
			printf("Could not create mainWindow object\n");
			return RETURN_ERROR;
	}

	if ((mainWindow = (struct Window *)DoMethod(mainWindowObject, WM_OPEN, NULL)) == NULL) {
		printf("Could not open mainWindow\n");
		return RETURN_ERROR;
	}

	SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_Pens, 0x00010002, TAG_DONE);
	SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);

	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);

	return RETURN_OK;
}

static LONG selectScreen() {
	Object *screenSelectRadioButton, *selectScreenOkButton, *screenSelectLayout, *screenSelectWindowObject;
	struct Window *screenSelectWindow;
	struct ScreenModeRequester *screenModeRequester;
	screen = LockPubScreen("Workbench");

	STRPTR radioButtonOptions[] = {
		"New screen",
		"Open in Workbench",
		NULL
	};

	if ((screenSelectRadioButton = NewObject(RADIOBUTTON_GetClass(), NULL,
		GA_ID, SCREEN_SELECT_RADIO_BUTTON_ID,
		GA_WIDTH, SCREEN_SELECT_RADIO_BUTTON_WIDTH,
		GA_HEIGHT, SCREEN_SELECT_RADIO_BUTTON_HEIGHT,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)radioButtonOptions,
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create screenSelectRadioButton\n");
			return RETURN_ERROR;
	}
 
	if ((selectScreenOkButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, SCREEN_SELECT_OK_BUTTON_ID,
		GA_WIDTH, SCREEN_SELECT_OK_BUTTON_WIDTH,
		GA_HEIGHT, SCREEN_SELECT_OK_BUTTON_HEIGHT,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)"OK",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create selectScreenOkButton\n");
			return RETURN_ERROR;
	}

	if ((screenSelectLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_DeferLayout, TRUE,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_BottomSpacing, 10,
		LAYOUT_HorizAlignment, LALIGN_CENTER,
		LAYOUT_Label, "Select a screen to open the window in:",
		LAYOUT_LabelPlace, BVJ_TOP_CENTER,
		LAYOUT_AddChild, screenSelectRadioButton,
		CHILD_WeightedHeight, 80,
		LAYOUT_AddChild, selectScreenOkButton,
		CHILD_MaxHeight, 20,
		CHILD_MaxWidth, 50,
		CHILD_WeightedHeight, 20,
		TAG_DONE)) == NULL) {
			printf("Could not create screenSelectLayout\n");
			return RETURN_ERROR;
	}

	if ((screenSelectWindowObject = NewObject(WINDOW_GetClass(), NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_Activate, TRUE,
		WA_Title, "Screen Select",
		WA_Width, SCREEN_SELECT_WINDOW_WIDTH,
		WA_Height, SCREEN_SELECT_WINDOW_HEIGHT,
		WA_CloseGadget, FALSE,
		WINDOW_SharedPort, NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_DragBar, TRUE,
		WINDOW_Layout, screenSelectLayout,
		WA_IDCMP, IDCMP_GADGETUP,
		WA_CustomScreen, screen,
		TAG_DONE)) == NULL) {
			printf("Could not create screenSelectWindow object\n");
			return RETURN_ERROR;
	}

	if ((screenSelectWindow = (struct Window *)DoMethod(screenSelectWindowObject, WM_OPEN, NULL)) == NULL) {
		printf("Could not open screenSelectWindow\n");
		return RETURN_ERROR;
	}

	BOOL done = FALSE;
	ULONG signalMask, winSignal, signals, result;
	WORD code;

	GetAttr(WINDOW_SigMask, screenSelectWindowObject, &winSignal);
	signalMask = winSignal;
    while (!done) {
		signals = Wait(signalMask);
        while ((result = DoMethod(screenSelectWindowObject, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
				case WMHI_GADGETUP:
					switch (result & WMHI_GADGETMASK) {
						case SCREEN_SELECT_OK_BUTTON_ID:
							done = TRUE;
							break;
						default:
							break;
					}
					break;
				default:
					break;
			}  
        }
    }

	LONG selectedRadioButton;
	GetAttr(RADIOBUTTON_Selected, screenSelectRadioButton, &selectedRadioButton);

	if (selectedRadioButton == 0) {
		ULONG displayID = GetVPModeID(&screen->ViewPort);
		// New screen
		if (screenModeRequester = (struct ScreenModeRequester *)AllocAslRequestTags(ASL_ScreenModeRequest,
		ASLSM_DoWidth, TRUE,
		ASLSM_DoHeight, TRUE,
		ASLSM_DoDepth, TRUE,
		ASLSM_DoOverscanType, TRUE,
		ASLSM_DoAutoScroll, TRUE,
		ASLSM_InitialDisplayID, displayID,
		ASLSM_InitialDisplayWidth, screen->Width,
		ASLSM_InitialDisplayHeight, screen->Height,
		ASLSM_InitialOverscanType, OSCAN_TEXT,
		 TAG_DONE)) {
			if (AslRequestTags(screenModeRequester, ASLSM_Window, (ULONG)screenSelectWindow, TAG_DONE)) {
				isPublicScreen = FALSE;
				UnlockPubScreen(NULL, screen);
				if ((screen = OpenScreenTags(NULL,
					SA_Pens, (ULONG)pens,
					SA_DisplayID, screenModeRequester->sm_DisplayID,
					SA_Depth, screenModeRequester->sm_DisplayDepth,
					SA_Overscan, screenModeRequester->sm_OverscanType,
					SA_AutoScroll, screenModeRequester->sm_AutoScroll,
					SA_Width, screenModeRequester->sm_DisplayWidth,
					SA_Height, screenModeRequester->sm_DisplayHeight,
					TAG_DONE)) == NULL) {
						printf("Could not open screen\n");
						return RETURN_ERROR;
				}

				SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x5);
				SetRGB4(&(screen->ViewPort), 1, 0xA, 0x0, 0x0);
				SetRGB4(&(screen->ViewPort), 2, 0x0, 0x0, 0x3);
				SetRGB4(&(screen->ViewPort), 3, 0xB, 0xF, 0x2);
				SetRGB4(&(screen->ViewPort), 4, 0xF, 0xF, 0x0);
			}
		}
	} else {
		// Open in Workbench
		isPublicScreen = TRUE;
	}

	DoMethod(screenSelectWindowObject, WM_CLOSE);

	return RETURN_OK;
}

static void sendMessage() {
	UBYTE newThing[2000];
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, TRUE, TAG_DONE);
	SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Sending", TAG_DONE);
	UBYTE *text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	// SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Pen, 1, TAG_DONE);
	// SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, STRINGA_Pens, 0x00010002, TAG_DONE);
	// DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, text, GV_TEXTEDITOR_InsertText_Bottom);
	// DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n", GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
	UBYTE *exportedText = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	sprintf(newThing, "%s*%s*\n\n", exportedText, text);
	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, newThing, TAG_DONE);
	FreeVec(exportedText);
	UBYTE *response = postMessageToOpenAI(text, "gpt-3.5-turbo", "user");
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, FALSE, TAG_DONE);
	if (response != NULL) {
		exportedText = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
		sprintf(newThing, "%s%s", exportedText, response);
		FreeVec(exportedText);
		SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
		SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TEXTEDITOR_Pen, 0, TAG_DONE);
		SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, newThing, TAG_DONE);
		// DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, response, GV_TEXTEDITOR_InsertText_Bottom);
		// DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n", GV_TEXTEDITOR_InsertText_Bottom);
		if (isSpeechEnabled)
			speakText(response);
		FreeVec(response);
	} else {
		SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "No response from OpenAI", TAG_DONE);
		printf("No response from OpenAI\n");
	}
	FreeVec(text);
}

// The main loop of the GUI
LONG startGUIRunLoop() {
	struct FontRequester *fontRequester;
	struct TextAttr *currentFont;
    ULONG signalMask, winSignal, signals, result;
	BOOL done = FALSE;
    WORD code;

	isSpeechEnabled = TRUE;
	speechSystem = SpeechSystemOld;

    GetAttr(WINDOW_SigMask, mainWindowObject, &winSignal);
	signalMask = winSignal;

	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GFLG_SELECTED, TRUE, TAG_DONE);

    while (!done) {
        signals = Wait(signalMask);

		while ((result = DoMethod(mainWindowObject, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (result & WMHI_CLASSMASK) {
				case WMHI_CLOSEWINDOW:
					done = TRUE;
					break;
				case WMHI_GADGETUP:
					sendMessage();
					break;
				case WMHI_MENUPICK:
				{
					struct Menu *menuStrip;
					GetAttr(WINDOW_MenuStrip, mainWindowObject, &menuStrip);
					struct MenuItem *menuItem = ItemAddress(menuStrip, code);
					ULONG itemIndex = GTMENUITEM_USERDATA(menuItem);
					switch (itemIndex) {
						case MENU_ITEM_ABOUT_ID:
							break;
						case MENU_ITEM_PREFERENCES_ID:
							break;
						case MENU_ITEM_QUIT_ID:
							done = TRUE;
							break;
						case MENU_ITEM_FONT_ID:
							if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
								if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
									currentFont = &fontRequester->fo_Attr;
								}
								FreeAslRequest(fontRequester);
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TextAttr, currentFont, TAG_DONE);
								SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TextAttr, currentFont, TAG_DONE);
							}
							break;
						case MENU_ITEM_SPEECH_ENABLED_ID:
							menuItem->Flags ^= CHECKED;
							isSpeechEnabled = !isSpeechEnabled;
							break;
						case MENU_ITEM_SPEECH_SYSTEM_OLD_ID:
							menuItem->Flags |= CHECKED;
							menuItem = ItemAddress(menuStrip, MENU_ITEM_SPEECH_SYSTEM_NEW_ID);
							menuItem->Flags &= ~CHECKED;
							speechSystem = SpeechSystemOld;
							closeSpeech();
							initSpeech(speechSystem);
							break;
						case MENU_ITEM_SPEECH_SYSTEM_NEW_ID:
							menuItem->Flags |= CHECKED;
							menuItem = ItemAddress(menuStrip, MENU_ITEM_SPEECH_SYSTEM_OLD_ID);
							menuItem->Flags &= ~CHECKED;
							speechSystem = SpeechSystemNew;
							closeSpeech();
							initSpeech(speechSystem);
							break;
						default:
							break;
					}
				}
					break;
				default:
					break;
			}
		}
	}

	return RETURN_OK;
}

void shutdownGUI() {
	if (mainWindowObject) {
		DisposeObject(mainWindowObject);
	}
	if (isPublicScreen) {
		UnlockPubScreen(NULL, screen);
	} else {
		CloseScreen(screen);
	}
	closeGUILibraries();
}