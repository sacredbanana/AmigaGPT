#include "support/gcc8_c_support.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>
#include <proto/translator.h>

#define WIN_LEFT_EDGE 0
#define WIN_TOP_EDGE 0
#define WIN_WIDTH 640
#define WIN_MIN_WIDTH 640
#define WIN_HEIGHT 500
#define WIN_MIN_HEIGHT 500

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *TranslatorBase;
struct Window *window;
struct Screen *screen;
LONG exitCode;

void openLibraries();
void closeLibraries();
void configureApp();
void initVideo();
void cleanExit(LONG);
BOOL handleIDCMP(struct Window*);

enum ScreenMode {
	PAL_NON_INTERLACED = 256,
	NTSC_NON_INTERLACED = 200,
	PAL_INTERLACED = 512,
	NTSC_INTERLACED = 400
};

struct Config {
	enum ScreenMode screenMode;
} config;

int main() {
	exitCode = 0;
	SysBase = *((struct ExecBase**)4UL);
	ULONG signalMask, winSignal, signals;
	BOOL done = FALSE;

	openLibraries();
	if (exitCode)
		goto exit;

	configureApp();

	initVideo();
	if (exitCode)
		goto exit;

	winSignal = 1L << window->UserPort->mp_SigBit;
	signalMask = winSignal;

	Move(window->RPort, 30, 20);

	UBYTE article[] = "This is a test string!";

	Text(window->RPort, article, sizeof(article) - 1);

	do {
		signals = Wait(signalMask);

		if (signals & winSignal)
			done = handleIDCMP(window);
	} while (!done);

	cleanExit(RETURN_OK);

exit:
	return exitCode;
}

void openLibraries() {
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 37);
	if (IntuitionBase == NULL) {
		cleanExit(RETURN_ERROR);
		return;
	}
		

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
	if (GfxBase == NULL) {
		cleanExit(RETURN_ERROR);
		return;
	}

	TranslatorBase = (struct Library *)OpenLibrary("translator.library", 0);
	if (TranslatorBase == NULL)
		cleanExit(RETURN_ERROR);
}

void initVideo() {
	UWORD pens[] = {~0};

	screen = OpenScreenTags(NULL,
	SA_Pens, (ULONG)pens,
	SA_DisplayID, HIRES_KEY,
	SA_Depth, 2,
	SA_Title, (ULONG)"OpenAI Amiga",
	TAG_DONE);

	if (screen == NULL) {
		cleanExit(RETURN_ERROR);
		return;
	}

	SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x2);

	window = OpenWindowTags(NULL,
	WA_Left, WIN_LEFT_EDGE,
	WA_Top, WIN_TOP_EDGE,
	WA_Width, WIN_WIDTH,
	WA_Height, WIN_HEIGHT,
	WA_MinWidth, WIN_MIN_WIDTH,
	WA_MinHeight, WIN_MIN_HEIGHT,
	WA_MaxWidth, ~0,
	WA_MaxHeight, ~0,
	WA_CloseGadget, TRUE,
	WA_SizeGadget, FALSE,
	WA_DepthGadget, FALSE,
	WA_DragBar, FALSE,
	WA_Activate, TRUE,
	WA_NoCareRefresh, TRUE,
	WA_IDCMP, IDCMP_CLOSEWINDOW,
	WA_CustomScreen, screen,
	WA_Title, "Main Window",
	WA_ScreenTitle, "Our screen - main window active",
	TAG_DONE);

	if (window == NULL)
		cleanExit(RETURN_ERROR);

	// struct NewWindow NewWindow;
	// NewWindow.LeftEdge = 0;
	// NewWindow.TopEdge = 0;
	// NewWindow.Width = 640;
	// NewWindow.Height = config.screenMode - 100;
	// NewWindow.DetailPen = 0;
	// NewWindow.BlockPen = 1;
	// NewWindow.Title = NULL;
	// NewWindow.Flags = SMART_REFRESH | ACTIVATE | BORDERLESS | NOCAREREFRESH | BACKDROP;
	// NewWindow.IDCMPFlags = NULL;
	// NewWindow.Type = CUSTOMSCREEN;
	// NewWindow.FirstGadget = NULL;
	// NewWindow.CheckMark = NULL;
	// NewWindow.Screen = screen;
	// NewWindow.BitMap = NULL;
	// NewWindow.MinWidth = 30;
	// NewWindow.MinHeight = 20;
	// NewWindow.MaxWidth = 600;
	// NewWindow.MaxHeight = 600;

	// if ((window = (struct Window *)OpenWindow(&NewWindow)) == NULL)
	// 	Exit(RETURN_ERROR);
}

void closeLibraries() {
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
	CloseLibrary(TranslatorBase);
}

void configureApp() {
	config.screenMode = PAL_NON_INTERLACED;
}

BOOL handleIDCMP(struct Window *window) {
	BOOL done = FALSE;
	struct IntuiMessage *message = NULL;
	ULONG class;

	while (message = (struct IntuiMessage *)GetMsg(window->UserPort)) {
		class = message->Class;
		ReplyMsg((struct Message *)message);

		switch (class) {
			case IDCMP_CLOSEWINDOW:
				done = TRUE;
				break;
			default:
				break;
		}
	}

	return done;
}

void cleanExit(LONG returnValue) {
	// There seems to be a bug with the Exit() call causing the program to guru. Use dirty goto's for now
	CloseWindow(window);
	CloseScreen(screen);
	closeLibraries();
	exitCode = returnValue;
	// Exit(returnValue);
}

