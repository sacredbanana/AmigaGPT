#include "support/gcc8_c_support.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <graphics/view.h>
#include <proto/intuition.h>
#include <intuition/intuition.h>


struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

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
	SysBase = *((struct ExecBase**)4UL);
	void openLibraries();
	void closeLibraries();
	void configureApp();
	void initVideo(struct Screen**, struct Window**);
	struct Window *window;
	struct Screen *screen;
	struct View *view;

	openLibraries();
	configureApp();
	initVideo(&screen, &window);
	
	SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x2);

	Move(window->RPort, 30, 20);

	char article[] = "This is a test string!";

	Text(window->RPort, article, sizeof(article) - 1);

	window->RPort->cp_x = 20;
	window->RPort->cp_y += 20;

	Wait(1 << window->UserPort->mp_SigBit);

	CloseWindow(window);

	FreeMem(screen->Font, sizeof(struct TextAttr));
	CloseScreen(screen);
	OpenWorkBench();

	closeLibraries();

	return 0;
}

void openLibraries() {
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
	if (IntuitionBase == NULL)
		Exit(RETURN_ERROR);

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 0);
	if (GfxBase == NULL)
		Exit(RETURN_ERROR);
}

void initVideo(struct Screen **screen, struct Window **window) {
	struct TextAttr *font;
	font = AllocMem(sizeof(struct TextAttr), MEMF_CHIP);
	font->ta_Name = "topaz.font";
	font->ta_YSize = TOPAZ_SIXTY;
	font->ta_Style = FS_NORMAL;
	font->ta_Flags = FPF_ROMFONT;

	struct NewScreen NewScreen;
	NewScreen.LeftEdge = 0;
	NewScreen.TopEdge = 0;
	NewScreen.Width = 640;
	NewScreen.Height = config.screenMode;
	NewScreen.Depth = 4;
	NewScreen.DetailPen = 0;
	NewScreen.BlockPen = 1;
	NewScreen.ViewModes = HIRES;
	NewScreen.Type = CUSTOMSCREEN;
	NewScreen.Font = NULL;
	NewScreen.DefaultTitle = "openAI";
	NewScreen.Gadgets = NULL;
	NewScreen.CustomBitMap = NULL;

	if ((*screen = (struct Screen *)OpenScreen(&NewScreen)) == NULL)
		Exit(RETURN_ERROR);

	CloseWorkBench();

	struct NewWindow NewWindow;
	NewWindow.LeftEdge = 0;
	NewWindow.TopEdge = 0;
	NewWindow.Width = 640;
	NewWindow.Height = config.screenMode;
	NewWindow.DetailPen = 0;
	NewWindow.BlockPen = 1;
	NewWindow.Title = NULL;
	NewWindow.Flags = SMART_REFRESH | ACTIVATE | BORDERLESS | NOCAREREFRESH | BACKDROP;
	NewWindow.IDCMPFlags = NULL;
	NewWindow.Type = CUSTOMSCREEN;
	NewWindow.FirstGadget = NULL;
	NewWindow.CheckMark = NULL;
	NewWindow.Screen = *screen;
	NewWindow.BitMap = NULL;
	NewWindow.MinWidth = 30;
	NewWindow.MinHeight = 20;
	NewWindow.MaxWidth = 600;
	NewWindow.MaxHeight = 600;

	if ((*window = (struct Window *)OpenWindow(&NewWindow)) == NULL)
		Exit(RETURN_ERROR);
}

void closeLibraries() {
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
}

void configureApp() {
	config.screenMode = PAL_NON_INTERLACED;
}