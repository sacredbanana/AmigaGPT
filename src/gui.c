#include "gui.h"
#include "config.h"
#include <stdio.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <gadgets/layout.h>
#include <gadgets/button.h>
#include <gadgets/texteditor.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>
#include <proto/button.h>
#include <proto/window.h>
#include <proto/texteditor.h>
#include <classes/window.h>
#include "speech.h"
#include "openai.h"
#include "amiga.h"

#define BUTTON_ID 0
#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 30
#define SEND_MESSAGE_BUTTON_ID 1
#define SEND_MESSAGE_BUTTON_WIDTH 100
#define SEND_MESSAGE_BUTTON_HEIGHT 20
#define TEXT_INPUT_TEXT_EDITOR_ID 2
#define TEXT_INPUT_TEXT_EDITOR_WIDTH 300
#define TEXT_INPUT_TEXT_EDITOR_HEIGHT 100
#define CHAT_OUTPUT_TEXT_EDITOR_ID 3
#define CHAT_OUTPUT_TEXT_EDITOR_WIDTH 300
#define CHAT_OUTPUT_TEXT_EDITOR_HEIGHT 100

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct Library *WindowBase;
struct Library *LayoutBase;
struct Library *ButtonBase;
struct Library *TextFieldBase;
struct GfxBase *GfxBase;
struct Window *window;
Object *windowObject;
Object *mainLayout;
Object *chatLayout;
Object *chatInputLayout;
Object *sendMessageButton;
Object *textInputTextEditor;
Object *chatOutputTextEditor;
struct Screen *screen;
struct Gadget *gadget;

typedef enum {
	EXIT_APPLICATION, ALERT_BUTTON_PRESSED, SPEAK_BUTTON_PRESSED
} Action;

enum ScreenMode {
	PAL_NON_INTERLACED = 256,
	NTSC_NON_INTERLACED = 200,
	PAL_INTERLACED = 512,
	NTSC_INTERLACED = 400
};

struct Config {
	enum ScreenMode screenMode;
} config;

static void sendMessage();

LONG openGUILibraries() {
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 47);
	if (IntuitionBase == NULL)  {
		printf("Could not open intuition.library\n");
        return RETURN_ERROR;
	}

	WindowBase = OpenLibrary("window.class", 47);
	if (IntuitionBase == NULL)  {
		printf("Could not open window.class\n");
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

void configureApp() {
	config.screenMode = PAL_NON_INTERLACED;
}

LONG initVideo() {
	UWORD pens[] = {~0};

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
	
	windowObject = NewObject(WINDOW_GetClass(), NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_Activate, TRUE,
		WA_Title, "AmigaGPT",
		WA_InnerWidth, WIN_WIDTH,
		WA_InnerHeight, WIN_HEIGHT,
		WA_CloseGadget, TRUE,
		WINDOW_Layout, mainLayout,
		WINDOW_SharedPort, NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP,
		WA_CustomScreen, screen,
		TAG_DONE);

	if (windowObject == NULL) {
		printf("Could not create window object\n");
		return RETURN_ERROR;
	}

	window = (struct Window *)DoMethod(windowObject, WM_OPEN, NULL);

	if (window == NULL) {
		printf("Could not open window\n");
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

static void sendMessage() {
	ActivateGadget(sendMessageButton, window, NULL);
	SetGadgetAttrs(sendMessageButton, window, NULL, GA_DISABLED, TRUE, TAG_DONE);
	UBYTE *text = DoGadgetMethod(textInputTextEditor, window, NULL, GM_TEXTEDITOR_ExportText, NULL);
	DoGadgetMethod(chatOutputTextEditor, window, NULL, GM_TEXTEDITOR_InsertText, NULL, text, GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(chatOutputTextEditor, window, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n", GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(textInputTextEditor, window, NULL, GM_TEXTEDITOR_ClearText, NULL);
	printf("Sending message:\n%s\n", text);
	UBYTE *response = postMessageToOpenAI(text, "gpt-3.5-turbo", "user");
	SetGadgetAttrs(sendMessageButton, window, NULL, GA_DISABLED, FALSE, TAG_DONE);
	if (response != NULL) {
		DoGadgetMethod(chatOutputTextEditor, window, NULL, GM_TEXTEDITOR_InsertText, NULL, response, GV_TEXTEDITOR_InsertText_Bottom);
		DoGadgetMethod(chatOutputTextEditor, window, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n", GV_TEXTEDITOR_InsertText_Bottom);
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

    GetAttr(WINDOW_SigMask, windowObject, &winSignal);
	signalMask = winSignal;

	SetGadgetAttrs(textInputTextEditor, window, NULL, GFLG_SELECTED, TRUE, TAG_DONE);

    while (!done) {
        signals = Wait(signalMask);

		while ((result = DoMethod(windowObject, WM_HANDLEINPUT, &action)) != WMHI_LASTMSG) {
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
	if (windowObject) {
		DisposeObject(windowObject);
	}
    CloseScreen(screen);
}