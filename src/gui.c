#include "gui.h"
#include "config.h"
#include <stdio.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/radiobutton.h>
#include <gadgets/texteditor.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>
#include <proto/button.h>
#include <proto/radiobutton.h>
#include <proto/window.h>
#include <proto/texteditor.h>
#include <libraries/gadtools.h>
#include <classes/window.h>
#include "speech.h"
#include "openai.h"
#include "amiga.h"
#include <stdbool.h>

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

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct Library *WindowBase;
struct Library *LayoutBase;
struct Library *ButtonBase;
struct Library *RadioButtonBase;
struct Library *TextFieldBase;
struct Library *GadToolsBase;
struct GfxBase *GfxBase;
struct Window *mainWindow;
Object *mainWindowObject;
Object *mainLayout;
Object *chatLayout;
Object *chatInputLayout;
Object *sendMessageButton;
Object *textInputTextEditor;
Object *chatOutputTextEditor;
struct Screen *screen;
struct Gadget *gadget;
struct Menu *menuStrip;
APTR *visualInfo;
BOOL isPublicScreen;
UWORD pens[] = {~0};


typedef enum {
	EXIT_APPLICATION, ALERT_BUTTON_PRESSED, SPEAK_BUTTON_PRESSED
} Action;

struct NewMenu amigaGPTMenu[] = {
	{NM_TITLE, "Project", 0, 0, 0, 0},
	{NM_ITEM, "About", 0, 0, 0, 0},
	{NM_ITEM, "Preferences", "P", 0, 0, 0},
	{NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},
	{NM_ITEM, "Quit", "Q", 0, 0, 0},
	{NM_END, NULL, 0, 0, 0, 0}
};

static void sendMessage();
static LONG selectScreen();

LONG openGUILibraries() {
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 47);
	if (IntuitionBase == NULL)  {
		printf("Could not open intuition.library\n");
        return RETURN_ERROR;
	}

	GadToolsBase = OpenLibrary("gadtools.library", 47);
	if (GadToolsBase == NULL)  {
		printf("Could not open gadtools.library\n");
        return RETURN_ERROR;
	}

	WindowBase = OpenLibrary("window.class", 47);
	if (IntuitionBase == NULL)  {
		printf("Could not open mainWindow.class\n");
        return RETURN_ERROR;
	}

	LayoutBase = OpenLibrary("gadgets/layout.gadget", 47);
	if (LayoutBase == NULL)  {
		printf("Could not open layout.gadget\n");
        return RETURN_ERROR;
	}

	ButtonBase = OpenLibrary("gadgets/button.gadget", 47);
	if (ButtonBase == NULL)  {
		printf("Could not open button.gadget\n");
        return RETURN_ERROR;
	}

	RadioButtonBase = OpenLibrary("gadgets/radiobutton.gadget", 47);
	if (RadioButtonBase == NULL)  {
		printf("Could not open radiobutton.gadget\n");
        return RETURN_ERROR;
	}

	TextFieldBase = OpenLibrary("gadgets/texteditor.gadget", 47);
	if (TextFieldBase == NULL)  {
		printf("Could not open texteditor.gadget\n");
        return RETURN_ERROR;
	}
	
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 47);
	if (GfxBase == NULL) {
		printf( "Could not open graphics.library\n");
        return RETURN_ERROR;
	}

	return RETURN_OK;
}

void closeGUILibraries() {
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
	CloseLibrary(WindowBase);
	CloseLibrary(LayoutBase);
	CloseLibrary(ButtonBase);
	CloseLibrary(TextFieldBase);
}

LONG initVideo() {
	if (selectScreen() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	sendMessageButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, SEND_MESSAGE_BUTTON_ID,
		GA_WIDTH, SEND_MESSAGE_BUTTON_WIDTH,
		GA_HEIGHT, SEND_MESSAGE_BUTTON_HEIGHT,
		BUTTON_TextPen, 3,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)"Send",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE);

	chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Text, (ULONG)"",
		GA_Width, CHAT_OUTPUT_TEXT_EDITOR_WIDTH,
		GA_Height, CHAT_OUTPUT_TEXT_EDITOR_HEIGHT,
		GA_ReadOnly, TRUE,
		TAG_DONE
	);

	textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Text, (ULONG)"",
		GA_Width, TEXT_INPUT_TEXT_EDITOR_WIDTH,
		GA_Height, TEXT_INPUT_TEXT_EDITOR_HEIGHT,
		TAG_DONE
	);

	if (textInputTextEditor == NULL) {
		printf("Could not create text editor\n");
		return RETURN_ERROR;
	}
	
	chatInputLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
		LAYOUT_HorizAlignment, LALIGN_CENTER,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, textInputTextEditor,
		CHILD_WeightedWidth, 100,
		LAYOUT_AddChild, sendMessageButton,
		CHILD_WeightedWidth, 10,
		TAG_DONE);

	chatLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, chatOutputTextEditor,
		CHILD_WeightedHeight, 80,
		LAYOUT_AddChild, chatInputLayout,
		CHILD_WeightedHeight, 20,
		TAG_DONE);

	mainLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
		LAYOUT_DeferLayout, TRUE,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, chatLayout,
		CHILD_WeightedWidth, 100,
		TAG_DONE);
	
	mainWindowObject = NewObject(WINDOW_GetClass(), NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_Activate, TRUE,
		WA_Title, "AmigaGPT",
		WA_InnerWidth, WIN_WIDTH,
		WA_InnerHeight, WIN_HEIGHT,
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
		TAG_DONE);

	if (mainWindowObject == NULL) {
		printf("Could not create mainWindow object\n");
		return RETURN_ERROR;
	}

	mainWindow = (struct Window *)DoMethod(mainWindowObject, WM_OPEN, NULL);

	if (mainWindow == NULL) {
		printf("Could not open mainWindow\n");
		return RETURN_ERROR;
	}

	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);

	return RETURN_OK;
}

static LONG selectScreen() {
	screen = LockPubScreen("Workbench");

	STRPTR radioButtonOptions[] = {
		"New screen",
		"Open in Workbench",
		NULL
	};

	Object *screenSelectRadioButton = NewObject(RADIOBUTTON_GetClass(), NULL,
		GA_ID, SCREEN_SELECT_RADIO_BUTTON_ID,
		GA_WIDTH, SCREEN_SELECT_RADIO_BUTTON_WIDTH,
		GA_HEIGHT, SCREEN_SELECT_RADIO_BUTTON_HEIGHT,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)radioButtonOptions,
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE);

	if (screenSelectRadioButton == NULL) {
		printf("Could not create screenSelectRadioButton\n");
		return RETURN_ERROR;
	}
 
	Object *selectScreenOkButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, SCREEN_SELECT_OK_BUTTON_ID,
		GA_WIDTH, SCREEN_SELECT_OK_BUTTON_WIDTH,
		GA_HEIGHT, SCREEN_SELECT_OK_BUTTON_HEIGHT,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)"OK",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE);

	if (selectScreenOkButton == NULL) {
		printf("Could not create selectScreenOkButton\n");
		return RETURN_ERROR;
	}

	Object *screenSelectLayout = NewObject(LAYOUT_GetClass(), NULL,
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
		TAG_DONE);

	if (screenSelectLayout == NULL) {
		printf("Could not create screenSelectLayout\n");
		return RETURN_ERROR;
	}

	Object *screenSelectWindowObject = NewObject(WINDOW_GetClass(), NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_Activate, TRUE,
		WA_Title, "Screen Select",
		WA_Width, 200,
		WA_Height, 100,
		WA_CloseGadget, FALSE,
		WINDOW_SharedPort, NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_DragBar, TRUE,
		WINDOW_Layout, screenSelectLayout,
		WA_IDCMP, IDCMP_GADGETUP,
		WA_CustomScreen, screen,
		TAG_DONE);

	if (screenSelectWindowObject == NULL) {
		printf("Could not create screenSelectWindow object\n");
		return RETURN_ERROR;
	}

	struct Window *screenSelectWindow = (struct Window *)DoMethod(screenSelectWindowObject, WM_OPEN, NULL);

	if (screenSelectWindow == NULL) {
		printf("Could not open screenSelectWindow\n");
		return RETURN_ERROR;
	}

	if (screen == NULL) {
		printf("Could not open screen\n");
		return RETURN_ERROR;
	}

	BOOL done = FALSE;
	ULONG signalMask, winSignal, signals, result;
	ULONG code;

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

	DoMethod(screenSelectWindowObject, WM_CLOSE);

	LONG selectedRadioButton;
	GetAttr(RADIOBUTTON_Selected, screenSelectRadioButton, &selectedRadioButton);

	if (selectedRadioButton == 0) {
		// New screen
		isPublicScreen = FALSE;
		UnlockPubScreen(NULL, screen);
		screen = OpenScreenTags(NULL,
		SA_Pens, (ULONG)pens,
		SA_DisplayID, HIRES_KEY,
		SA_Depth, 3,
		SA_Title, (ULONG)"AmigaGPT",
		TAG_DONE);

		if (screen == NULL) {
			printf("Could not open screen\n");
			return RETURN_ERROR;
		}

		SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x5);
		SetRGB4(&(screen->ViewPort), 1, 0xA, 0x3, 0x0);
		SetRGB4(&(screen->ViewPort), 3, 0xB, 0xF, 0x2);
		SetRGB4(&(screen->ViewPort), 4, 0xF, 0xF, 0x0);
	} else {
		// Open in Workbench
		isPublicScreen = TRUE;
	}

	return RETURN_OK;
}

static void sendMessage() {
	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, TRUE, TAG_DONE);
	UBYTE *text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, text, GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n", GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	printf("Sending message:\n%s\n", text);
	UBYTE *response = postMessageToOpenAI(text, "gpt-3.5-turbo", "user");
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, FALSE, TAG_DONE);
	if (response != NULL) {
		DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, response, GV_TEXTEDITOR_InsertText_Bottom);
		DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n", GV_TEXTEDITOR_InsertText_Bottom);
		speakText(response);
		Delay(50);
		FreeVec(response);
	} else {
		printf("No response from OpenAI\n");
	}
	FreeVec(text);
}

// The main loop of the GUI
LONG startGUIRunLoop() {
    ULONG signalMask, winSignal, signals, result;
	BOOL done = FALSE;
    Action action;

    GetAttr(WINDOW_SigMask, mainWindowObject, &winSignal);
	signalMask = winSignal;

	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GFLG_SELECTED, TRUE, TAG_DONE);

    while (!done) {
        signals = Wait(signalMask);

		while ((result = DoMethod(mainWindowObject, WM_HANDLEINPUT, &action)) != WMHI_LASTMSG) {
			switch (result & WMHI_CLASSMASK) {
				case WMHI_CLOSEWINDOW:
					done = TRUE;
					break;
				case WMHI_GADGETUP:
					sendMessage();
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
}