#ifdef __AMIGAOS3__ || __AMIGAOS4__
#include "amiga_compiler.h"
#endif
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <json-c/json.h>
#include <intuition/icclass.h>
#include <libraries/mui.h>
#include <mui/Aboutbox_mcc.h>
#include <mui/Busy_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include "AboutAmigaGPTWindow.h"
#include "APIKeyRequesterWindow.h"
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "StartupOptionsWindow.h"
#include "version.h"
#include "datatypesclass.h"

#ifdef __AMIGAOS4__
struct MUIMasterIFace *IMUIMaster;
struct DataTypesIFace *IDataTypes;
#endif

#ifdef __MORPHOS__
static ULONG muiDispatcherGate(void)
{
	ULONG (*dispatcher)(struct IClass *, Object *, Msg);

	struct IClass *cl  = (struct IClass *)REG_A0;
	Object        *obj = (Object *)       REG_A2;
	Msg           msg  = (Msg)            REG_A1;

	dispatcher = (ULONG(*)(struct IClass *, Object *, Msg))cl->cl_UserData;

	return dispatcher(cl, obj, msg);
}

struct EmulLibEntry muiDispatcherEntry =
{
	TRAP_LIB, 0, (void (*)(void)) muiDispatcherGate
};
#endif

struct Library *MUIMasterBase;
struct Library *DataTypesBase;
Object *app;
ULONG redPen, greenPen, bluePen, yellowPen;
struct Screen *screen;
Object *imageWindowObject;
struct Window *imageWindow;
Object *openImageWindowImageView;
Object *dataTypeObject;

static CONST_STRPTR USED_CLASSES[] = {
	MUIC_Aboutbox,
	MUIC_Busy,
	MUIC_NList,
	MUIC_NListview,
	MUIC_TextEditor,
	NULL
	};

static void closeGUILibraries();

// #ifdef __AMIGAOS4__
// static uint32 processIDCMPCreateImageWindow(struct Hook *hook, struct Window *window, struct IntuiMessage *message) {
// #else
// static void __SAVE_DS__ __ASM__ processIDCMPCreateImageWindow(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message)) {
// #endif
// 	switch (message->Class) {
// 		case IDCMP_IDCMPUPDATE:
// 		{
// 			UWORD MsgCode = message->Code;
// 			struct TagItem *MsgTags = message->IAddress;
// 			struct TagItem *List = MsgTags;
// 			struct TagItem *This;

// 			while((This = NextTagItem(&List)) != NULL)
// 			{
// 				switch(This->ti_Tag)
// 				{
// 					case DTA_Busy:
// 						if(This->ti_Data)
// 						{
// 							SetWindowPointer(imageWindow,
// 								WA_BusyPointer,	TRUE,
// 							TAG_DONE);
// 							updateStatusBar("Processing image...", 7);
// 						}
// 						else
// 						{
// 							SetWindowPointerA(imageWindow,NULL);
// 							updateStatusBar("Ready", 5);
// 						}

// 						break;

// 					case DTA_Sync:
// 						SetAttrs(dataTypeObject,
// 						 GA_RelWidth, imageWindow->Width - imageWindow->BorderLeft - imageWindow->BorderRight,
// 						GA_RelHeight, imageWindow->Height - imageWindow->BorderTop - imageWindow->BorderBottom,
// 						 TAG_DONE);
// 						RefreshDTObjects(dataTypeObject,imageWindow,NULL,NULL);
// 						break;
// 				}
// 			}
// 			break;
// 		}
// 		case IDCMP_REFRESHWINDOW:
// 			BeginRefresh(imageWindow);
// 			EndRefresh(imageWindow,TRUE);
// 			break;
// 		default:
// 			printf("Unknown message class: %lx\n", message->Class);
// 			break;
// 	}
// 	#ifdef __AMIGAOS3__
// 	return;
// 	#else
// 	return WHOOKRSLT_IGNORE;
// 	#endif
// }

/**
 * Open the libraries needed for the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG openGUILibraries() {
	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL) {
		printf("Could not open muimaster.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL) {
		printf("Could not open muimaster.library\n");
		return RETURN_ERROR;
	}
	if ((IMUIMaster = (struct MUIIFace *)GetInterface(MUIMasterBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for muimaster.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((DataTypesBase = OpenLibrary("datatypes.library", 44)) == NULL) {
		printf("Could not open datatypes.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((DataTypesBase = OpenLibrary("datatypes.library", 44)) == NULL) {
		printf("Could not open datatypes.library\n");
		return RETURN_ERROR;
	}
	if ((IDataTypes = (struct DataTypesIFace *)GetInterface(DataTypesBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for datatypes.library\n");
		return RETURN_ERROR;
	}
	#endif

	return RETURN_OK;
}

/**
 * Close the libraries used by the GUI
**/
static void closeGUILibraries() {
	#ifdef __AMIGAOS4__
	DropInterface((struct Interface *)IMUIMaster);
	#endif
	CloseLibrary(MUIMasterBase);
}

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initVideo() {
	if (openGUILibraries() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	if (createStartupOptionsWindow() == RETURN_ERROR)
		return RETURN_ERROR;

	if (createMainWindow() == RETURN_ERROR)
		return RETURN_ERROR;

	if (createAPIKeyRequesterWindow() == RETURN_ERROR)
		return RETURN_ERROR;

	if (createChatSystemRequesterWindow() == RETURN_ERROR)
		return RETURN_ERROR;

	if ((imageWindowObject = WindowObject,
			MUIA_Window_Title, "Image",
			MUIA_Window_Width, 320,
			MUIA_Window_Height, 240,
			MUIA_Window_CloseGadget, TRUE,
			MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
			MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
			MUIA_Window_SizeRight, TRUE,
			MUIA_Window_UseBottomBorderScroller, FALSE,
			MUIA_Window_UseRightBorderScroller, FALSE,
			MUIA_Window_UseLeftBorderScroller, FALSE,
			WindowContents, VGroup,
			Child, openImageWindowImageView = DataTypesObject, NULL,TAG_END),
			End,
		End) == NULL) {
        displayError("Could not create image window");
        return RETURN_ERROR;
    }

	if (!(app = ApplicationObject,
		MUIA_Application_Base, "AmigaGPT",
		MUIA_Application_Title, "AmigaGPT",
		MUIA_Application_Version, APP_VERSION,
		MUIA_Application_Copyright, "(C) 2023-2024 Cameron Armstrong (Nightfox/sacredbanana)",
		MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
		MUIA_Application_Description, "AmigaGPT is an app for chatting to ChatGPT or creating AI images with DALL-E",
		MUIA_Application_UsedClasses, USED_CLASSES,
		MUIA_Application_HelpFile, "PROGDIR:AmigaGPT.guide",
		SubWindow, startupOptionsWindowObject,
		SubWindow, mainWindowObject,
		SubWindow, apiKeyRequesterWindowObject,
		SubWindow, chatSystemRequesterWindowObject,
		SubWindow, imageWindowObject,
		End)) {
		displayError("Could not create app!\n");
		return RETURN_ERROR;
	}

	DoMethod(imageWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    get(imageWindowObject, MUIA_Window, &imageWindow);   

	if (createAboutAmigaGPTWindow() == RETURN_OK)
		DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);

	set(startupOptionsWindowObject, MUIA_Window_Open, TRUE);	

	DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);

	addMenuActions();
	addStartupOptionsWindowActions();

	return RETURN_OK;
}

/**
 * Start the main run loop of the GUI
**/
void startGUIRunLoop() {
	ULONG signals;
	BOOL running = TRUE;

	while (running) {
		ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

		switch(id) {
			case MUIV_Application_ReturnID_Quit:
			{
				running = FALSE;
				break;
			}
			default:
				break;
		}
		if(running && signals) Wait(signals);
	}
}

/**
 * Shutdown the GUI
**/
void shutdownGUI() {
	DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
	MUI_DisposeObject(app);
	ReleasePen(screen->ViewPort.ColorMap, redPen);
	ReleasePen(screen->ViewPort.ColorMap, greenPen);
	ReleasePen(screen->ViewPort.ColorMap, bluePen);
	ReleasePen(screen->ViewPort.ColorMap, yellowPen);
	if (isPublicScreen) {
		UnlockPubScreen(NULL, screen);
	} else {
		CloseScreen(screen);
	}

	delete_datatypes_class();

	closeGUILibraries();
}