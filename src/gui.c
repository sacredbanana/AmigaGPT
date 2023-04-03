#include "gui.h"
#include "support/gcc8_c_support.h"
#include "config.h"
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <proto/button.h>
#include "speech.h"
#include "openai.h"
#include "amiga.h"
#include "console.h"

#define BUTTON_ID 0
#define BUTTON_WIDTH 100
#define BUTTON_HEIGHT 30
#define SEND_MESSAGE_BUTTON_ID 1
#define SEND_MESSAGE_BUTTON_WIDTH 100
#define SEND_MESSAGE_BUTTON_HEIGHT 20

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Window *window;
struct Window *window2;
struct Screen *screen;
struct Gadget *gadget;
struct IOStdReq *ConsoleReadIORequest;
struct MsgPort *ConsoleReadPort;
struct IOStdReq *ConsoleWriteIORequest;
struct MsgPort *ConsoleWritePort;

UWORD buttonBorderData[] = {
	0, 0, BUTTON_WIDTH + 1, 0, BUTTON_WIDTH + 1, BUTTON_HEIGHT + 1,
	0, BUTTON_HEIGHT + 1, 0, 0,
};

struct Border buttonBorder = {
	-1, -1, 1, 0, JAM1, 5, buttonBorderData, NULL,
};

struct IntuiText buttonText = {
	3, 3, JAM1, 8, 10, NULL, "Press me!\0", NULL
};

struct Gadget buttonGadget = {
	NULL, 20, 50, BUTTON_WIDTH, BUTTON_HEIGHT,
	GFLG_GADGHCOMP, GACT_RELVERIFY | GACT_IMMEDIATE,
	GTYP_BOOLGADGET, &buttonBorder, NULL, &buttonText, 0, NULL, BUTTON_ID, NULL,
};

struct Gadget *sendMessageButtonGadget;

struct IntuiText sendMessageButtonText = {
	1, 1, JAM1, 8, 10, NULL, "Send", NULL
};

struct Border sendButtonBorder = {
	-1, -1, 1, 1, JAM1, 5, buttonBorderData, NULL,
};

struct DrawInfo drawInfo;

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
Action handleIDCMP(struct Window*);

LONG openGUILibraries() {
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
	if (IntuitionBase == NULL) 
        return RETURN_ERROR;
	
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
	if (GfxBase == NULL)
        return RETURN_ERROR;

	return RETURN_OK;
}

void closeGUILibraries() {
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
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

	if (screen == NULL)
        return RETURN_ERROR;

	SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x5);
	SetRGB4(&(screen->ViewPort), 1, 0xA, 0x3, 0x0);
	SetRGB4(&(screen->ViewPort), 3, 0xB, 0xF, 0x2);
	SetRGB4(&(screen->ViewPort), 4, 0xF, 0xF, 0x0);

	window = OpenWindowTags(NULL,
	WA_Left, WIN_LEFT_EDGE,
	WA_Top, WIN_TOP_EDGE,
	WA_Width, WIN_WIDTH,
	WA_Height, WIN_HEIGHT,
	WA_MinWidth, WIN_MIN_WIDTH,
	WA_MinHeight, WIN_MIN_HEIGHT,
	WA_MaxWidth, ~0,
	WA_MaxHeight, ~0,
	WA_Gadgets, (ULONG)&buttonGadget,
	WA_Title, (ULONG)"AmigaGPT",
	WA_CloseGadget, TRUE,
	WA_SizeGadget, FALSE,
	WA_DepthGadget, FALSE,
	WA_DragBar, FALSE,
	WA_Activate, TRUE,
	WA_NoCareRefresh, TRUE,
	WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP,
	WA_CustomScreen, screen,
	TAG_DONE);

	if (window == NULL)
		return RETURN_ERROR;

	window2 = OpenWindowTags(NULL,
	WA_Left, 200,
	WA_Top, 100,
	WA_Width, 400,
	WA_Height, 100,
	WA_MinWidth, 100,
	WA_MinHeight, 50,
	WA_MaxWidth, ~0,
	WA_MaxHeight, ~0,
	WA_Title, (ULONG)"Conversation",
	WA_CloseGadget, FALSE,
	WA_SizeGadget, TRUE,
	WA_DepthGadget, FALSE,
	WA_DragBar, TRUE,
	WA_Activate, TRUE,
	WA_SimpleRefresh, TRUE,
	WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
	WA_CustomScreen, screen,
	TAG_DONE);

	if ((ConsoleReadPort= CreatePort("OA.ConsoleRead", 0)) == NULL) {
		return RETURN_ERROR;
	}

	if ((ConsoleReadIORequest = (struct IOStdReq *)CreateExtIO(ConsoleReadPort, sizeof(struct IOStdReq))) == NULL) {
		return RETURN_ERROR;
	}

	ConsoleReadIORequest->io_Data = (APTR)window;
	ConsoleReadIORequest->io_Length = sizeof(struct Window);

	if ((ConsoleWritePort= CreatePort("OA.ConsoleWrite", 0)) == NULL) {
		return RETURN_ERROR;
	}

	if ((ConsoleWriteIORequest = (struct IOStdReq *)CreateExtIO(ConsoleWritePort, sizeof(struct IOStdReq))) == NULL) {
		return RETURN_ERROR;
	}

	ConsoleWriteIORequest->io_Data = (APTR)window2;
	ConsoleWriteIORequest->io_Length = sizeof(struct Window);

	if (OpenDevice("console.device", CONU_SNIPMAP, (struct IORequest *)ConsoleWriteIORequest, CONFLAG_DEFAULT) != 0) {
		return RETURN_ERROR;
	}

	selectGraphicRendition(SGR_PRIMARY, 4, 0);

	if ((sendMessageButtonGadget = (struct Gadget*)NewObject(NULL, "frbuttonclass", 
		GA_Left, 310L,
		GA_Top, 50L,
		GA_Width, SEND_MESSAGE_BUTTON_WIDTH,
		GA_Height, SEND_MESSAGE_BUTTON_HEIGHT,
		GA_IntuiText, (ULONG)&sendMessageButtonText,
		GA_ID, SEND_MESSAGE_BUTTON_ID,
		GA_RelVerify, TRUE,
		TAG_DONE)) == NULL) {
			return RETURN_ERROR;
	}

	AddGList(window, (struct Gadget*)sendMessageButtonGadget, -1, -1, NULL);
	RefreshGList((struct Gadget*)sendMessageButtonGadget, window, NULL, -1);

	return RETURN_OK;
}

static void sendMessage() {
	UBYTE *response = postMessageToOpenAI("Tell me an interesting random fact", "gpt-3.5-turbo", "user");
	if (response != NULL) {
		consolePrintText(response);
		moveCursorNextLine(2);
		speakText(response);
		Delay(50);
		FreeVec(response);
	} else {
		UBYTE text66[] = "No response from OpenAI!\n";
		Write(Output(), (APTR)text66, strlen(text66));
	}
}

// The main loop of the GUI
LONG startGUIRunLoop() {
    ULONG signalMask, winSignal, signals;
	BOOL done = FALSE;
	UBYTE englishString[] = "Never gonna give you up, never gonna let you down, never going to run around and desert you";
    Action action;

    winSignal = 1L << window->UserPort->mp_SigBit;
	signalMask = winSignal;

    while (!done) {
        signals = Wait(signalMask);

		if (signals & winSignal) {
			switch (handleIDCMP(window)) {
				case EXIT_APPLICATION:
					done = TRUE;
					break;
				case ALERT_BUTTON_PRESSED:
					sendMessage();
					break;
				default:
					break;
			}
		}
    }

	return RETURN_OK;
}

// Handle the messages from the GUI
Action handleIDCMP(struct Window *window) {
	struct IntuiMessage *message = NULL;
	ULONG class;

	while (message = (struct IntuiMessage *)GetMsg(window->UserPort)) {
		class = message->Class;
		ReplyMsg((struct Message *)message);

		switch (class) {
			case IDCMP_CLOSEWINDOW:
				return EXIT_APPLICATION;
			case IDCMP_GADGETUP:
				return ALERT_BUTTON_PRESSED;
			default:
				break;
		}
	}

	return NULL;
}

void shutdownGUI() {
	if (!(CheckIO(ConsoleReadIORequest)))
		AbortIO(ConsoleReadIORequest);
	if (!(CheckIO(ConsoleWriteIORequest)))
		AbortIO(ConsoleWriteIORequest);

	WaitIO(ConsoleReadIORequest);
	WaitIO(ConsoleWriteIORequest);
	CloseDevice((struct IORequest *)ConsoleWriteIORequest);
    CloseWindow(window);
    CloseScreen(screen);
}