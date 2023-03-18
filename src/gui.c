#include "gui.h"
#include "support/gcc8_c_support.h"
#include "config.h"
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include "speech.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Window *window;
struct Screen *screen;
struct Gadget *gadget;

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
	GTYP_BOOLGADGET, &buttonBorder, NULL, &buttonText, 0, NULL, BUTTON_GADGET_NUM, NULL,
};

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
	SA_Depth, 2,
	SA_Title, (ULONG)"OpenAI Amiga",
	TAG_DONE);

	if (screen == NULL)
        return RETURN_ERROR;

	SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x2);
	SetRGB4(&(screen->ViewPort), 1, 0x0, 0x0, 0x0);
	SetRGB4(&(screen->ViewPort), 3, 0x0, 0xFF, 0x2);

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
	WA_Title, (ULONG)"OpenAI Amiga",
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

    Move(window->RPort, 30, 20);
	UBYTE article[] = "This is a test string!";
	Text(window->RPort, article, sizeof(article) - 1);
}

// The main loop of the GUI
LONG startGUIRunLoop() {
    ULONG signalMask, winSignal, signals;
	BOOL done = FALSE;
	STRPTR englishString = "Never gonna give you up, never gonna let you down, never going to run around and desert you";
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
					speakText(englishString);
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
    CloseWindow(window);
    CloseScreen(screen);
}