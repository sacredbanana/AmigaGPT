#ifdef __AMIGAOS3__ || __AMIGAOS4__
#include "amiga_compiler.h"
#endif
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/Aboutbox_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/datatypes.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include "AboutAmigaGPTWindow.h"
#include "APIKeyRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "StartupOptionsWindow.h"
#include "version.h"

#define HELP_KEY 0x5F

#define MODE_SELECTION_TAB_CHAT_ID 0
#define MODE_SELECTION_TAB_IMAGE_GENERATION_ID 1

#ifdef __AMIGAOS4__
#define IntuitionBase Library
#define GfxBase Library
struct IntuitionIFace *IIntuition;
struct AslIFace *IAsl;
struct GraphicsIFace *IGraphics;
struct AmigaGuideIFace *IAmigaGuide;
extern struct UtilityIFace *IUtility;
struct DataTypesIFace *IDataTypes;
struct MUIMasterIFace *IMUIMaster;
struct GadToolsIFace *IGadTools;
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *AmigaGuideBase;
struct Library *AslBase;
struct Library *GadToolsBase;
struct Library *DataTypesBase;
struct Library *MUIMasterBase;
Object *app;
struct Window *imageWindow;
struct Screen *screen;
static Object *imageWindowObject;
static Object *mainGroup;
static Object *modeClickTab;
static Object *dataTypeObject;
static LONG selectedMode;
static struct GeneratedImage *currentImage;
struct List *modeSelectionTabList;
struct List *imageList;
static struct TextFont *uiTextFont = NULL;
struct TextAttr screenFont = {
	.ta_Name = "",
	.ta_YSize = 8,
	.ta_Style = FS_NORMAL,
	.ta_Flags = FPF_DISKFONT | FPF_DESIGNED
};
static struct TextAttr chatTextAttr = {0};
static struct TextAttr uiTextAttr = {0};

/**
 * Struct representing a generated image
 * @see enum ImageSize
 * @see enum ImageModel
 **/ 
static struct GeneratedImage {
	STRPTR name;
	STRPTR filePath;
	STRPTR prompt;
	enum ImageModel imageModel;
	WORD width;
	WORD height;
};
struct MsgPort *appPort;
ULONG activeTextEditorGadgetID;
static struct Node *chatTabNode;
static struct Node *imageGenerationTabNode;

static CONST_STRPTR USED_CLASSES[] = {
	MUIC_Aboutbox,
	MUIC_NList,
	MUIC_NListview,
	MUIC_TextEditor,
	NULL
	};

static STRPTR getMessageContentFromJson(struct json_object *json, BOOL stream);
static void formatText(STRPTR unformattedText);
static void closeGUILibraries();
static struct Conversation* newConversation();
static void addTextToConversation(struct Conversation *conversation, STRPTR text, STRPTR role);
static void addImageToImageList(struct GeneratedImage *image);
static void freeImageList();
static void freeModeSelectionTabList();
static void saveImageCopy(struct GeneratedImage *image);
static void removeImageFromImageList(struct GeneratedImage *image);
static void openChatFontRequester();
static void openUIFontRequester();
static void openSpeechAccentRequester();
static void openChatSystemRequester();
static void openImage(struct GeneratedImage *generatedImage, WORD width, WORD height);
static LONG loadConversations();
static LONG loadImages();
static LONG saveImages();
static STRPTR ISO8859_1ToUTF8(CONST_STRPTR iso8859_1String);
static STRPTR UTF8ToISO8859_1(CONST_STRPTR utf8String);
static BOOL copyFile(STRPTR source, STRPTR destination);
static void createImage();

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
	if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 40)) == NULL) {
		printf("Could not open intuition.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((IntuitionBase = OpenLibrary("intuition.library", 50)) == NULL) {
		printf("Could not open intuition.library\n");
		return RETURN_ERROR;
	}
	if ((IIntuition = (struct IntuitionIFace *)GetInterface(IntuitionBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for intuition.library\n");
		return RETURN_ERROR;
	}
	#endif

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
	if ((GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 40)) == NULL) {
		printf( "Could not open graphics.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((GfxBase = OpenLibrary("graphics.library", 50)) == NULL) {
		printf( "Could not open graphics.library\n");
		return RETURN_ERROR;
	}
	if ((IGraphics = (struct GraphicsIFace *)GetInterface(GfxBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for graphics.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((AslBase = OpenLibrary("asl.library", 45)) == NULL) {
		printf( "Could not open asl.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((AslBase = OpenLibrary("asl.library", 50)) == NULL) {
		printf( "Could not open asl.library\n");
		return RETURN_ERROR;
	}
	if ((IAsl = (struct AslIFace *)GetInterface(AslBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for asl.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((AmigaGuideBase = OpenLibrary("amigaguide.library", 44)) == NULL) {
		printf( "Could not open amigaguide.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((AmigaGuideBase = OpenLibrary("amigaguide.library", 50)) == NULL) {
		printf( "Could not open amigaguide.library\n");
		return RETURN_ERROR;
	}
	if ((IAmigaGuide = (struct AmigaGuideIFace *)GetInterface(AmigaGuideBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for amigaguide.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((GadToolsBase = OpenLibrary("gadtools.library", 40)) == NULL) {
		printf( "Could not open gadtools.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((GadToolsBase = OpenLibrary("gadtools.library", 50)) == NULL) {
		printf( "Could not open gadtools.library\n");
		return RETURN_ERROR;
	}
	if ((IGadTools = (struct GadToolsIFace *)GetInterface(GadToolsBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for gadtools.library\n");
		return RETURN_ERROR;
	}
	#endif

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	if ((DataTypesBase = OpenLibrary("datatypes.library", 44)) == NULL) {
		printf( "Could not open datatypes.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((DataTypesBase = OpenLibrary("datatypes.library", 50)) == NULL) {
		printf( "Could not open datatypes.library\n");
		return RETURN_ERROR;
	}
	if ((IDataTypes = (struct DataTypesIFace *)GetInterface(DataTypesBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for datatypes.library\n");
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
	DropInterface((struct Interface *)IIntuition);
	DropInterface((struct Interface *)IGraphics);
	DropInterface((struct Interface *)IAsl);
	DropInterface((struct Interface *)IAmigaGuide);
	DropInterface((struct Interface *)IDataTypes);
	DropInterface((struct Interface *)IMUIMaster);
	DropInterface((struct Interface *)IGadTools);
	#endif

	CloseLibrary(GadToolsBase);
	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
	CloseLibrary(AslBase);
	CloseLibrary(AmigaGuideBase);
	CloseLibrary(DataTypesBase);
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

	// set(aboutAmigaGPTWindowObject, MUIA_Window_Screen, screen);

	// modeSelectionTabList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	// NewList(modeSelectionTabList);
	// chatTabNode = AllocClickTabNode(TAG_DONE);
	// SetClickTabNodeAttrs(chatTabNode,
	//  TNA_Number, MODE_SELECTION_TAB_CHAT_ID,
	//  TNA_Text, "Chat",
	//  TNA_TextPen, isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0x00000000, 0x00000000, OBP_Precision, PRECISION_GUI, TAG_DONE) : 1,
	//  #ifdef __AMIGAOS4__
	//  TNA_HintInfo, "Have a text conversation with ChatGPT",
	//  #endif
	//   TAG_DONE);
	// imageGenerationTabNode = AllocClickTabNode(TAG_DONE);
	// SetClickTabNodeAttrs(imageGenerationTabNode,
	//  TNA_Number, MODE_SELECTION_TAB_IMAGE_GENERATION_ID,
	//  TNA_Text, "Image Generation",
	//  TNA_TextPen, isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0x00000000, 0x00000000, OBP_Precision, PRECISION_GUI, TAG_DONE) : 1,
	//  #ifdef __AMIGAOS4__
	//  TNA_HintInfo, "Generate an image from a text prompt",
	//  #endif
	//   TAG_DONE);
	// AddTail(modeSelectionTabList, chatTabNode);	
	// AddTail(modeSelectionTabList, imageGenerationTabNode);

	currentConversation = NULL;

	// imageList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	// NewList(imageList);
	// currentImage = NULL;
	// loadImages();

	uiTextAttr.ta_Name = config.uiFontName ? config.uiFontName : "";
	uiTextAttr.ta_YSize = config.uiFontSize;
	uiTextAttr.ta_Style = config.uiFontStyle;
	uiTextAttr.ta_Flags = config.uiFontFlags;
	chatTextAttr.ta_Name = config.chatFontName ? config.chatFontName : "";
	chatTextAttr.ta_YSize = config.chatFontSize;
	chatTextAttr.ta_Style = config.chatFontStyle;
	chatTextAttr.ta_Flags = config.chatFontFlags;

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
		End)) {
		displayError("Could not create app!\n");
		return RETURN_ERROR;
	}

	if (createAboutAmigaGPTWindow() == RETURN_OK)
		DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);

	set(startupOptionsWindowObject, MUIA_Window_Open, TRUE);	

	DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);

	addMenuActions();
	addStartupOptionsWindowActions();

	loadConversations();

	// if ((createImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, CREATE_IMAGE_BUTTON_ID,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Create Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create create image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageListBrowser = NewObject(LISTBROWSER_GetClass(), NULL,
	// 	GA_ID, IMAGE_LIST_BROWSER_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_TextAttr, &uiTextAttr,
	// 	LISTBROWSER_WrapText, TRUE,
	// 	LISTBROWSER_AutoFit, TRUE,
	// 	LISTBROWSER_ShowSelected, TRUE,
	// 	LISTBROWSER_Labels, imageList,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image list browser\n");
	// 		return RETURN_ERROR;
	// }

	// pen = 5;
	// newChatButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, config.colors[pen*3+1], config.colors[pen*3+2], config.colors[pen*3+3], OBP_Precision, PRECISION_GUI, TAG_DONE) : pen;

	// if ((newImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, NEW_IMAGE_BUTTON_ID,
	// 	BUTTON_TextPen, newChatButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"+ New Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create new image button\n");
	// 		return RETURN_ERROR;
	// }

	// pen = 6;
	// deleteButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, config.colors[pen*3+1], config.colors[pen*3+2], config.colors[pen*3+3], OBP_Precision, PRECISION_GUI, TAG_DONE) : pen;

	// if ((deleteImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, DELETE_IMAGE_BUTTON_ID,
	// 	BUTTON_TextPen, deleteButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"- Delete Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create delete image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openSmallImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_SMALL_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Small Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create open small image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openMediumImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_MEDIUM_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Medium Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create open medium image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openLargeImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_LARGE_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Large Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create open large image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((openOriginalImageButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, OPEN_ORIGINAL_IMAGE_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Open Original Image",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not open original image button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((saveCopyButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, SAVE_COPY_BUTTON_ID,
	// 	GA_Disabled, TRUE,
	// 	BUTTON_TextPen, sendMessageButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"Save Copy",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not save copy button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageHistoryButtonsLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, newImageButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, deleteImageButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, imageListBrowser,
	// 	CHILD_WeightedWidth, 90,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image history buttons layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((newChatButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, NEW_CHAT_BUTTON_ID,
	// 	BUTTON_TextPen, newChatButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"+ New Chat",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create new chat button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((deleteChatButton = NewObject(BUTTON_GetClass(), NULL,
	// 	GA_ID, DELETE_CHAT_BUTTON_ID,
	// 	BUTTON_TextPen, deleteButtonPen,
	// 	#ifdef __AMIGAOS3__
	// 	BUTTON_BackgroundPen, 0,
	// 	#endif
	// 	GA_TextAttr, &uiTextAttr,
	// 	BUTTON_Justification, BCJ_CENTER,
	// 	GA_Text, (ULONG)"- Delete Chat",
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create delete chat button\n");
	// 		return RETURN_ERROR;
	// }

	// if ((conversationListBrowser = NewObject(LISTBROWSER_GetClass(), NULL,
	// 	GA_ID, CONVERSATION_LIST_BROWSER_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_TextAttr, &uiTextAttr,
	// 	LISTBROWSER_WrapText, TRUE,
	// 	LISTBROWSER_AutoFit, TRUE,
	// 	LISTBROWSER_ShowSelected, TRUE,
	// 	LISTBROWSER_Labels, conversationList,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create conversation list browser\n");
	// 		return RETURN_ERROR;
	// }

	// if ((conversationsLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, newChatButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, deleteChatButton,
	// 	CHILD_WeightedHeight, 5,
	// 	LAYOUT_AddChild, conversationListBrowser,
	// 	CHILD_WeightedWidth, 90,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create conversations layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((statusBar = NewObject(STRING_GetClass(), NULL,
	// 	GA_ID, STATUS_BAR_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_ReadOnly, TRUE,
	// 	GA_TextAttr, &uiTextAttr,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create text editor\n");
	// 		return RETURN_ERROR;
	// }

	// if ((textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
	// 	GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_TextAttr, &chatTextAttr,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create text editor\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
	// 	GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
	// 	GA_RelVerify, TRUE,
	// 	GA_ReadOnly, TRUE,
	// 	GA_TextAttr, &chatTextAttr,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_MIME,
	// 	GA_TEXTEDITOR_ExportHook, GV_TEXTEDITOR_ExportHook_Plain,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create text editor\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatOutputScroller = NewObject(SCROLLER_GetClass(), NULL,
	// 	GA_ID, CHAT_OUTPUT_SCROLLER_ID,
	// 	GA_RelVerify, TRUE,
	// 	ICA_TARGET, ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create scroller\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatInputLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_HorizAlignment, LALIGN_CENTER,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, textInputTextEditor,
	// 	CHILD_WeightedWidth, 80,
	// 	CHILD_NoDispose, TRUE,
	// 	LAYOUT_AddChild, sendMessageButton,
	// 	CHILD_WeightedWidth, 20,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat input layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatOutputLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_HorizAlignment, LALIGN_CENTER,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, chatOutputTextEditor,
	// 	CHILD_WeightedWidth, 100,
	// 	LAYOUT_AddChild, chatOutputScroller,
	// 	CHILD_WeightedWidth, 10,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat output layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatTextBoxesLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, chatOutputLayout,
	// 	CHILD_WeightedHeight, 70,
	// 	LAYOUT_AddChild, chatInputLayout,
	// 	CHILD_WeightedHeight, 20,
	// 	LAYOUT_AddChild, statusBar,
	// 	CHILD_WeightedHeight, 10,
	// 	CHILD_NoDispose, TRUE,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((chatModeLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, conversationsLayout,
	// 	CHILD_WeightedWidth, 30,
	// 	LAYOUT_AddChild, chatTextBoxesLayout,
	// 	CHILD_WeightedWidth, 70,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create main layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageGenerationInputLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, openSmallImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, openMediumImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, openLargeImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, openOriginalImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, saveCopyButton,
	// 	CHILD_WeightedHeight, 10,
	// 	LAYOUT_AddChild, textInputTextEditor,
	// 	CHILD_WeightedHeight, 40,
	// 	CHILD_NoDispose, TRUE,
	// 	LAYOUT_AddChild, createImageButton,
	// 	CHILD_WeightedHeight, 10,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image generation input layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageGenerationTextBoxesLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, imageGenerationInputLayout,
	// 	CHILD_WeightedHeight, 90,
	// 	LAYOUT_AddChild, statusBar,
	// 	CHILD_WeightedHeight, 10,
	// 	CHILD_NoDispose, TRUE,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create chat layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageGenerationModeLayout = NewObject(LAYOUT_GetClass(), NULL,
	// 	LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
	// 	LAYOUT_SpaceInner, TRUE,
	// 	LAYOUT_SpaceOuter, TRUE,
	// 	LAYOUT_AddChild, imageHistoryButtonsLayout,
	// 	CHILD_WeightedWidth, 30,
	// 	LAYOUT_AddChild, imageGenerationTextBoxesLayout,
	// 	CHILD_WeightedWidth, 70,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image generation layout\n");
	// 		return RETURN_ERROR;
	// }

	// if ((modeClickTabOld = NewObject(CLICKTAB_GetClass(), NULL,
	// 	GA_ID, CLICKTAB_MODE_SELECTION_ID,
	// 	GA_RelVerify, TRUE,
	// 	CLICKTAB_Labels, modeSelectionTabList,
	// 	CLICKTAB_AutoFit, TRUE,
	// 	CLICKTAB_PageGroup, NewObject(PAGE_GetClass(), NULL,
    //         PAGE_Add,       chatModeLayout,
    //         PAGE_Add,       imageGenerationModeLayout,
    //     TAG_DONE),
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create mode click tab\n");
	// 		return RETURN_ERROR;
	// }

	// Object *layoutToOpen;
	// if (isAmigaOS3X) {
	// 	switch (selectedMode) {
	// 		case MODE_SELECTION_TAB_CHAT_ID:
	// 			layoutToOpen = chatModeLayout;
	// 			break;
	// 		case MODE_SELECTION_TAB_IMAGE_GENERATION_ID:
	// 			layoutToOpen = imageGenerationModeLayout;
	// 			break;
	// 		default:
	// 			layoutToOpen = chatModeLayout;
	// 			break;
	// 	}
	// } else {
	// 	layoutToOpen = modeClickTabOld;
	// }

	appPort = CreateMsgPort();

	// if (!isPublicScreen) {
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_ColorMap, &textEditorColorMap, TAG_DONE);
	// 	if (!isAmigaOS3X || selectedMode == MODE_SELECTION_TAB_CHAT_ID)
	// 		SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_ColorMap, &textEditorColorMap, TAG_DONE);
	// }


	// For some reason it won't let you paste text into the empty text editor unless you do this
	// DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "", GV_TEXTEDITOR_InsertText_Bottom);

	updateStatusBar("Ready", 5);

	return RETURN_OK;
}

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 * 
**/ 
void updateStatusBar(CONST_STRPTR message, const ULONG pen) {
	set(statusBar, MUIA_String_Contents, message);
	set(statusBar, MUIA_TextColor, "rff0000");
	// SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_Pens, isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, config.colors[3*pen+1], config.colors[3*pen+2], config.colors[3*pen+3], OBP_Precision, PRECISION_GUI, TAG_DONE) : pen, TAG_DONE);
	// SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, message, TAG_DONE);
}

/**
 * Format a string with escape sequences into a string with the actual characters
 * @param unformattedText the text to format
**/
static void formatText(STRPTR unformattedText) {
	LONG newStringIndex = 0;
	const LONG oldStringLength = strlen(unformattedText);
	if (oldStringLength == 0) return;
	for (LONG oldStringIndex = 0; oldStringIndex < oldStringLength; oldStringIndex++) {
		if (unformattedText[oldStringIndex] == '\\') {
			if (unformattedText[oldStringIndex + 1] == 'n') {
				unformattedText[newStringIndex++] = '\n';
			} else if (unformattedText[oldStringIndex + 1] == 'r') {
				unformattedText[newStringIndex++] = '\r';
			} else if (unformattedText[oldStringIndex + 1] == 't') {
				unformattedText[newStringIndex++] = '\t';
			}
			oldStringIndex++;
		} else {
			unformattedText[newStringIndex++] = unformattedText[oldStringIndex];
		}
	}
	unformattedText[newStringIndex++] = '\0';
}

/**
 * Get the message content from the JSON response from OpenAI
 * @param json the JSON response from OpenAI
 * @param stream whether the response is a stream or not
 * @return a pointer to a new string containing the message content -- Free it with FreeVec() when you are done using it
 * If found role in the json instead of content then return an empty string
 * @todo Handle errors
**/
static STRPTR getMessageContentFromJson(struct json_object *json, BOOL stream) {
	if (json == NULL) return NULL;
	STRPTR json_str = json_object_to_json_string(json);
	struct json_object *choices = json_object_object_get(json, "choices");
	struct json_object *choice = json_object_array_get_idx(choices, 0);
	struct json_object *message = json_object_object_get(choice, stream ? "delta" : "message");
	if (stream) {
		struct json_object *role = json_object_object_get(message, "role");
		if (role != NULL)
			return "\0";
	}
	struct json_object *content = json_object_object_get(message, "content");
	return json_object_get_string(content);
}

/**
 * Sends a chat message to the OpenAI API and displays the response and speaks it if speech is enabled
**/
void sendChatMessage() {
	BOOL isNewConversation = FALSE;
	STRPTR finishReason = NULL;
	struct json_object **responses;
	if (currentConversation == NULL) {
		isNewConversation = TRUE;
		currentConversation = newConversation();
	}
	set(sendMessageButton, MUIA_Disabled, TRUE);
	set(newChatButton, MUIA_Disabled, TRUE);
	set(deleteChatButton, MUIA_Disabled, TRUE);

	updateStatusBar("Sending message...", 7);
	STRPTR receivedMessage = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
	STRPTR text = DoMethod(chatInputTextEditor, MUIM_TextEditor_ExportText);

	// Remove trailing newline characters
	while (text[strlen(text) - 1] == '\n') {
		text[strlen(text) - 1] = '\0';
	}
	STRPTR textUTF_8 = ISO8859_1ToUTF8(text);
	addTextToConversation(currentConversation, textUTF_8, "user");
	displayConversation(currentConversation);
	DoMethod(chatInputTextEditor, MUIM_TextEditor_ClearText);
	DoMethod(chatInputTextEditor, MUIM_GoActive);

	BOOL dataStreamFinished = FALSE;
	ULONG speechIndex = 0;
	UWORD wordNumber = 0;

	DoMethod(chatOutputTextEditor, MUIM_TextEditor_InsertText, "\n", MUIV_TextEditor_InsertText_Bottom);
	do {
		if (config.chatSystem != NULL && (config.chatSystem) > 0)
			addTextToConversation(currentConversation, config.chatSystem, "system");
		responses = postChatMessageToOpenAI(currentConversation, config.chatModel, config.openAiApiKey, TRUE);
		if (responses == NULL) {
			displayError("Could not connect to OpenAI");
			set(sendMessageButton, MUIA_Disabled, FALSE);
			set(newChatButton, MUIA_Disabled, FALSE);
			set(deleteChatButton, MUIA_Disabled, FALSE);
			FreeVec(receivedMessage);
			return;
		}
		if (config.chatSystem != NULL && strlen(config.chatSystem) > 0) {
			struct MinNode *chatSystemNode = RemTail(currentConversation->messages);
			FreeVec(chatSystemNode);
		}
		UWORD responseIndex = 0;
		struct json_object *response;
		while (response = responses[responseIndex++]) {
			struct json_object *error;
			if (json_object_object_get_ex(response, "error", &error)) {
				struct json_object *message = json_object_object_get(error, "message");
				STRPTR messageString = json_object_get_string(message);
				displayError(messageString);
				set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
				struct MinNode *lastMessage = RemTail(currentConversation->messages);
				FreeVec(lastMessage);
				if (currentConversation == currentConversation->messages->mlh_TailPred) {
					freeConversation(currentConversation);
					currentConversation = NULL;
					DoMethod(chatOutputTextEditor, MUIM_TextEditor_ClearText);
				} else {
					displayConversation(currentConversation);
				}
				json_object_put(response);

				set(sendMessageButton, MUIA_Disabled, FALSE);
				set(newChatButton, MUIA_Disabled, FALSE);
				set(deleteChatButton, MUIA_Disabled, FALSE);

				FreeVec(receivedMessage);
				return;
			}
			STRPTR contentString = getMessageContentFromJson(response, TRUE);
			if (contentString != NULL) {
				// Text for printing
				formatText(contentString);
				STRPTR formattedMessageISO8859_1 = UTF8ToISO8859_1(contentString);
				strncat(receivedMessage, contentString, READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
				DoMethod(chatOutputTextEditor, MUIM_TextEditor_InsertText, formattedMessageISO8859_1, MUIV_TextEditor_InsertText_Bottom);
				FreeVec(formattedMessageISO8859_1);
				// Text for speaking
				STRPTR unformattedMessageISO8859_1 = UTF8ToISO8859_1(receivedMessage);
				if (++wordNumber % 50 == 0) {
					if (config.speechEnabled) {
						if (config.speechSystem != SPEECH_SYSTEM_OPENAI) {
							speakText(unformattedMessageISO8859_1 + speechIndex);
						}
						speechIndex = strlen(unformattedMessageISO8859_1);
						FreeVec(unformattedMessageISO8859_1);
					}
				}
				finishReason = json_object_get_string(json_object_object_get(response, "finish_reason"));
				if (finishReason != NULL) {
					dataStreamFinished = TRUE;
				}
				json_object_put(response);
			} else {
				dataStreamFinished = TRUE;
			}
		}
	} while (!dataStreamFinished);

	if (responses != NULL) {
		addTextToConversation(currentConversation, receivedMessage, "assistant");
		if (config.speechEnabled) {
			STRPTR receivedMessageISO8859_1 = UTF8ToISO8859_1(receivedMessage);
			if (config.speechSystem == SPEECH_SYSTEM_OPENAI) {
				speakText(receivedMessage);
			} else {
				speakText(receivedMessageISO8859_1 + speechIndex);
			}
			FreeVec(receivedMessageISO8859_1);
		}
		FreeVec(responses);
		FreeVec(receivedMessage);
		if (isNewConversation) {
			updateStatusBar("Generating conversation title...", 7);
			addTextToConversation(currentConversation, "generate a short title for this conversation and don't enclose the title in quotes or prefix the response with anything", "user");
			responses = postChatMessageToOpenAI(currentConversation, config.chatModel, config.openAiApiKey, FALSE);
			if (responses == NULL) {
				displayError("Could not connect to OpenAI");
				set(sendMessageButton, MUIA_Disabled, FALSE);
				set(newChatButton, MUIA_Disabled, FALSE);
				set(deleteChatButton, MUIA_Disabled, FALSE);
				return;
			}
			if (responses[0] != NULL) {
				STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
				formatText(responseString);
				if (currentConversation->name == NULL) {
					currentConversation->name = AllocVec(strlen(responseString) + 1, MEMF_CLEAR);
					strncpy(currentConversation->name, responseString, strlen(responseString));
				}
				DoMethod(conversationListObject, MUIM_NList_InsertSingle, currentConversation, MUIV_NList_Insert_Top);
			}
			struct MinNode *titleRequestNode = RemTail(currentConversation->messages);
			FreeVec(titleRequestNode);
			json_object_put(responses[0]);
			FreeVec(responses);
		}
	}

	updateStatusBar("Ready", 5);
	saveConversations();
	
	set(sendMessageButton, MUIA_Disabled, FALSE);
	set(newChatButton, MUIA_Disabled, FALSE);
	set(deleteChatButton, MUIA_Disabled, FALSE);

	FreeVec(text);
	FreeVec(textUTF_8);
}

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
**/
static struct Conversation* newConversation() {
	struct Conversation *conversation = AllocVec(sizeof(struct Conversation), MEMF_CLEAR);
	struct MinList *messages = AllocVec(sizeof(struct MinList), MEMF_CLEAR);
	
	// NewMinList(conversation); // This is what makes us require exec.library 45. Replace with the following:
	if (messages) {
		messages->mlh_Tail = 0;
		messages->mlh_Head = (struct MinNode *)&messages->mlh_Tail;
		messages->mlh_TailPred = (struct MinNode *)&messages->mlh_Head;
	}

	conversation->messages = messages;
	conversation->name = NULL;
	
	return conversation;
}

/**
 * Add a block of text to the conversation list
 * @param conversation The conversation to add the text to
 * @param text The text to add to the conversation
 * @param role The role of the text (user or assistant)
**/
static void addTextToConversation(struct Conversation *conversation, STRPTR text, STRPTR role) {
	struct ConversationNode *conversationNode = AllocVec(sizeof(struct ConversationNode), MEMF_CLEAR);
	if (conversationNode == NULL) {
		printf("Failed to allocate memory for conversation node\n");
		return;
	}
	strncpy(conversationNode->role, role, sizeof(conversationNode->role) - 1);
	conversationNode->role[sizeof(conversationNode->role) - 1] = '\0';
	conversationNode->content = AllocVec(strlen(text) + 1, MEMF_CLEAR);
	strncpy(conversationNode->content, text, strlen(text));
	AddTail(conversation->messages, (struct Node *)conversationNode);
}

/**
 * Add an image to the image list
 * @param image The image to add to the imagelist
**/
static void addImageToImageList(struct GeneratedImage *image) {
	// struct Node *node;
	// if ((node = AllocListBrowserNode(1,
	// 	LBNCA_CopyText, TRUE,
	// 	LBNCA_Text, image->name,
	// 	LBNA_UserData, (ULONG)image,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create image list browser node\n");
	// 		return RETURN_ERROR;
	// }

	// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
	// AddHead(imageList, node);
	// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, imageList, TAG_DONE);
}

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
**/
void displayConversation(struct Conversation *conversation) {
	struct ConversationNode *conversationNode;
	STRPTR conversationString = AllocVec(WRITE_BUFFER_LENGTH, MEMF_CLEAR);

	for (conversationNode = (struct ConversationNode *)conversation->messages->mlh_Head;
		 conversationNode->node.mln_Succ != NULL;
		 conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			if ((strlen(conversationString) + strlen(conversationNode->content) + 5) > WRITE_BUFFER_LENGTH) {
				displayError("The conversation has exceeded the maximum length.\n\nPlease start a new conversation.");
				set(sendMessageButton, MUIA_Disabled, TRUE);
				return;
			}
			if (strcmp(conversationNode->role, "user") == 0) {
				STRPTR content = conversationNode->content;
				strncat(conversationString, "\33r\33b", WRITE_BUFFER_LENGTH - strlen(conversationString) - 5);
				while (*content != '\0') {
					strncat(conversationString, content, 1);
					if (*content++ == '\n') {
						strncat(conversationString, "\33b", WRITE_BUFFER_LENGTH - strlen(conversationString) - 3);
					}
				}
			} else if (strcmp(conversationNode->role, "assistant") == 0) {
				strncat(conversationString, "\33l", WRITE_BUFFER_LENGTH - strlen(conversationString) - 3);
				strncat(conversationString, conversationNode->content, WRITE_BUFFER_LENGTH - strlen(conversationString));
			}
			strncat(conversationString, "\n\33c\33[s:18]\n", WRITE_BUFFER_LENGTH - strlen(conversationString) - 12);
	}

	STRPTR conversationStringISO8859_1 = UTF8ToISO8859_1(conversationString);
	set(chatOutputTextEditor, MUIA_TextEditor_Contents, conversationStringISO8859_1);
	FreeVec(conversationString);
	FreeVec(conversationStringISO8859_1);
}

/**
 * Free the conversation
 * @param conversation The conversation to free
**/
void freeConversation(struct Conversation *conversation) {
	struct ConversationNode *conversationNode;
	while ((conversationNode = (struct ConversationNode *)RemHead(conversation->messages)) != NULL) {
		FreeVec(conversationNode->content);
		FreeVec(conversationNode);
	}
	if (conversation->name != NULL)
	 	FreeVec(conversation->name);
	FreeVec(conversation);
}

/**
 * Copy a conversation
 * @param conversation The conversation to copy
 * @return A pointer to the copied conversation
**/
struct Conversation* copyConversation(struct Conversation *conversation) {
	struct Conversation *copy = newConversation();
	struct ConversationNode *conversationNode;
	for (conversationNode = (struct ConversationNode *)conversation->messages->mlh_Head;
		 conversationNode->node.mln_Succ != NULL;
		 conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			addTextToConversation(copy, conversationNode->content, conversationNode->role);
	}
	if (conversation->name != NULL) {
		copy->name = AllocVec(strlen(conversation->name) + 1, MEMF_CLEAR);
		strncpy(copy->name, conversation->name, strlen(conversation->name));
	}
	return copy;
}

/**
 * Free the image list
**/
static void freeImageList() {
	// struct Node *imageListNode;
	// while ((imageListNode = RemHead(imageList)) != NULL) {
	// 	struct GeneratedImage *generatedImage;
	// 	GetListBrowserNodeAttrs(imageListNode, LBNA_UserData, (ULONG *)&generatedImage, TAG_END);
	// 	FreeVec(generatedImage->filePath);
	// 	FreeVec(generatedImage->name);
	// 	FreeVec(generatedImage->prompt);
	// 	FreeVec(generatedImage);
	// }
	// FreeVec(imageList);
}

/**
 * Save a copy of the image
 * @param image The image to save a copy of
**/ 
static void saveImageCopy(struct GeneratedImage *image) {
	// if (image == NULL) return;
	// STRPTR filePath = image->filePath;
	// struct FileRequester *fileReq = AllocAslRequestTags(ASL_FileRequest, TAG_END);
	// if (fileReq != NULL) {
	// 	if (AslRequestTags(fileReq,
	// 	ASLFR_Window, mainWindow,
	// 	ASLFR_TitleText, "Save Image Copy",
	// 	ASLFR_InitialFile, "image.png",
	// 	ASLFR_InitialDrawer, "SYS:",
	// 	ASLFR_DoSaveMode, TRUE,
	// 	TAG_DONE)) {
	// 		STRPTR savePath = fileReq->fr_Drawer;
	// 		STRPTR saveName = fileReq->fr_File;
	// 		BOOL isRootDirectory = savePath[strlen(savePath) - 1] == ':';
	// 		STRPTR fullPath = AllocVec(strlen(savePath) + strlen(saveName) + 2, MEMF_CLEAR);
	// 		snprintf(fullPath, strlen(savePath) + strlen(saveName) + 2, "%s%s%s", savePath, isRootDirectory ? "" : "/", saveName);
	// 		copyFile(filePath, fullPath);
	// 		FreeVec(fullPath);
	// 	}
	// 	FreeAslRequest(fileReq);
	// }
}

/**
 * Remove an image from the image list
 * @param image The image to remove from the image list
**/
static void removeImageFromImageList(struct GeneratedImage *image) {
	// if (image == NULL) return;
	// struct Node *node = imageList->lh_Head;
	// while (node != NULL) {
	// 	struct GeneratedImage *listBrowserImage;
	// 	GetListBrowserNodeAttrs(node, LBNA_UserData, (struct GeneratedImage *)&listBrowserImage, TAG_END);
	// 	if (listBrowserImage == image) {
	// 		#ifdef __AMIGAOS3__
	// 		DeleteFile(image->filePath);
	// 		#else
	// 		Delete(image->filePath);
	// 		#endif
	// 		// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Selected, -1, TAG_DONE);
	// 		// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
	// 		Remove(node);
	// 		FreeListBrowserNode(node);
	// 		// SetGadgetAttrs(imageListBrowser, mainWindow, NULL, LISTBROWSER_Labels, imageList, TAG_DONE);
	// 		FreeVec(image->filePath);
	// 		FreeVec(image->name);
	// 		FreeVec(image->prompt);
	// 		FreeVec(image);
	// 		return;
	// 	}
	// 	node = node->ln_Succ;
	// }
}

/**
 * Start the main run loop of the GUI
**/
void startGUIRunLoop() {
	DoMethod(app, MUIM_Application_Run);
/* ULONG signals;
	BOOL running = TRUE;

	while (running) {
		ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

		switch(id) {
			case MUIV_Application_ReturnID_Quit:
			{
				BOOL forceQuit;
				get(app, MUIA_Application_ForceQuit, &forceQuit);
				if(forceQuit || (MUI_RequestA(app, mainWindowObject, 0, "Quit?", "_Yes|_No", "\33cAre you sure you want to quit AmigaGPT?", 0)) == 1)
						running = FALSE;
				break;
			}
			default:
				break;
		}
		if(running && signals) Wait(signals);
	}
		// BOOL isTextInputTextEditorActive = GetAttr(GFLG_SELECTED, textInputTextEditor, NULL);
		// BOOL isChatOutputTextEditorActive = GetAttr(GFLG_SELECTED, chatOutputTextEditor, NULL);

		while ((result = DoMethod(mainWindowObjectOld, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (result & WMHI_CLASSMASK) {
				case WMHI_CLOSEWINDOW:
					done = TRUE;
					break;
				case WMHI_GADGETDOWN:
					switch (result & WMHI_GADGETMASK) {
						case TEXT_INPUT_TEXT_EDITOR_ID:
							if (isChatOutputTextEditorActive)
								ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
								activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							break;
					}
					break;
				case WMHI_GADGETUP:
					switch (result & WMHI_GADGETMASK) {
						case SEND_MESSAGE_BUTTON_ID:
						case TEXT_INPUT_TEXT_EDITOR_ID:
							if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
								sendChatMessage();
								saveConversations();
							}
							else
								displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
							break;
						case NEW_CHAT_BUTTON_ID:
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
							activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							break;
						case DELETE_CHAT_BUTTON_ID:
							removeConversationFromConversationList(currentConversation);
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
							activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							saveConversations();
							break;
						case CONVERSATION_LIST_BROWSER_ID:
							{
								// Switch to the conversation the user clicked on in the list
								struct ListBrowserNode *node;
								GetAttr(LISTBROWSER_SelectedNode, conversationListBrowser, &node);
								struct MinList *conversation;
								GetListBrowserNodeAttrs(node, LBNA_UserData,&conversation, TAG_END);
								currentConversation = conversation;
								displayConversation(currentConversation);
								break;
							}
						case NEW_IMAGE_BUTTON_ID:
							currentImage = NULL;
							SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, FALSE, TAG_DONE);
							DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
							SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
							ActivateLayoutGadget(imageGenerationModeLayout, mainWindow, NULL, textInputTextEditor);
							break;
						case IMAGE_LIST_BROWSER_ID:
							{
								// Switch to the image the user clicked on in the list
								struct ListBrowserNode *node;
								GetAttr(LISTBROWSER_SelectedNode, imageListBrowser, &node);
								struct GeneratedImage *image;
								GetListBrowserNodeAttrs(node, LBNA_UserData,&image, TAG_END);
								currentImage = image;
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, currentImage->prompt, TAG_DONE);
								SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
								SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
								break;
							}
						case CLICKTAB_MODE_SELECTION_ID:
						{
							struct Node *node;
							GetAttr(CLICKTAB_CurrentNode, modeClickTabOld, &node);
							if (node == chatTabNode) {
								ActivateLayoutGadget(chatModeLayout, mainWindow, NULL, textInputTextEditor);
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, FALSE, TAG_DONE);
								DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							} else if (node == imageGenerationTabNode) {
								ActivateLayoutGadget(imageGenerationModeLayout, mainWindow, NULL, textInputTextEditor);
								if (currentImage == NULL) {
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, FALSE, TAG_DONE);
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
								} else {
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, currentImage->prompt, TAG_DONE);
								}
							}
							break;
						}
						case CREATE_IMAGE_BUTTON_ID:
							if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
								createImage();
								saveImages();
							} else {
								displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
							}
							break;
						case DELETE_IMAGE_BUTTON_ID:
							removeImageFromImageList(currentImage);
							currentImage = NULL;
							saveImages();
							break;
						case OPEN_SMALL_IMAGE_BUTTON_ID:
						{
							if (currentImage->width == currentImage->height)
								openImage(currentImage, 256, 256);
							else if (currentImage->width > currentImage->height) {
								LONG height = (currentImage->height * 256) / currentImage->width;
								openImage(currentImage, 256, height);
							} else {
								LONG width = (currentImage->width * 256) / currentImage->height;
								openImage(currentImage, width, 256);
							}
							break;
						}
						case OPEN_MEDIUM_IMAGE_BUTTON_ID:
						{
							if (currentImage->width == currentImage->height)
								openImage(currentImage, 512, 512);
							else if (currentImage->width > currentImage->height) {
								LONG height = (currentImage->height * 512) / currentImage->width;
								openImage(currentImage, 512, height);
							} else {
								LONG width = (currentImage->width * 512) / currentImage->height;
								openImage(currentImage, width, 512);
							}
							break;
						}
						case OPEN_LARGE_IMAGE_BUTTON_ID:
						{
							if (currentImage->width == currentImage->height)
								openImage(currentImage, 1024, 1024);
							else if (currentImage->width > currentImage->height) {
								LONG height = (currentImage->height * 1024) / currentImage->width;
								openImage(currentImage, 1024, height);
							} else {
								LONG width = (currentImage->width * 1024) / currentImage->height;
								openImage(currentImage, width, 1024);
							}
							break;
						}
						case OPEN_ORIGINAL_IMAGE_BUTTON_ID:
							openImage(currentImage, currentImage->width, currentImage->height);
							break;
						case SAVE_COPY_BUTTON_ID:
							saveImageCopy(currentImage);
							break;
					}
					break;
				case WMHI_MENUPICK:
				{
					struct MenuItem *menuItem = ItemAddress(menu, code);
					ULONG itemIndex = code != -1 ? (ULONG)GTMENUITEM_USERDATA(menuItem) : NULL_ID;
					switch (itemIndex) {
						case MENU_ITEM_PROJECT_ABOUT_ID:
							openAboutWindow();
							break;
						case MENU_ITEM_EDIT_CUT_ID:
						{
							switch (activeTextEditorGadgetID) {
								case TEXT_INPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");
									break;
								case CHAT_OUTPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
									break;
							}
						}
						case MENU_ITEM_COPY_ID:
						{
							switch (activeTextEditorGadgetID) {
								case TEXT_INPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
									break;
								case CHAT_OUTPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
									break;
							}
							break;
						}
						case MENU_ITEM_PASTE_ID:
						{
							DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "PASTE");
							break;
						}
						case MENU_ITEM_SELECT_ALL_ID:
						{
							switch (activeTextEditorGadgetID) {
								case TEXT_INPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);
									break;
								case CHAT_OUTPUT_TEXT_EDITOR_ID:
									DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);
									break;
							}
							break;
						}
						case MENU_ITEM_CLEAR_ID:
						{
							DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CLEAR");
							break;
						}
						case MENU_ITEM_PROJECT_QUIT_ID:
							done = TRUE;
							break;
						case MENU_ITEM_CHAT_FONT_ID:
							openChatFontRequester();
							break;
						case MENU_ITEM_UI_FONT_ID:
							openUIFontRequester();
							break;
						case MENU_ITEM_SPEECH_ENABLED_ID:
							config.speechEnabled = !config.speechEnabled;
							if (config.speechEnabled) {
								closeSpeech();
								if (initSpeech(config.speechSystem) == RETURN_ERROR) {
									displayError("Could not initialise selected speech system.");
								}
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#ifdef __AMIGAOS3__
						case MENU_ITEM_SPEECH_ACCENT_ID:
							openSpeechAccentRequester();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_34_ID:
							closeSpeech();
							config.speechSystem = SPEECH_SYSTEM_34;
							if (initSpeech(SPEECH_SYSTEM_34) == RETURN_ERROR) {
								displayError("Could not initialise speech system v34. Please make sure the translator.library and narrator.device v34 are installed into the program directory.");								
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_37_ID:
							closeSpeech();
							config.speechSystem = SPEECH_SYSTEM_37;
							if (initSpeech(SPEECH_SYSTEM_37) == RETURN_ERROR) {
								displayError("Could not initialise speech system v37. Please make sure the translator.library and narrator.device v37 are installed into the program directory.");
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#else
						case MENU_ITEM_SPEECH_SYSTEM_FLITE_ID:
							closeSpeech();
							config.speechSystem = SPEECH_SYSTEM_FLITE;
							if (initSpeech(SPEECH_SYSTEM_FLITE) == RETURN_ERROR) {
								displayError("Could not initialise speech system Flite. Please make sure the flite.device is installed into the program directory.");								
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#endif
						case MENU_ITEM_SPEECH_SYSTEM_OPENAI_ID:
							closeSpeech();
							initSpeech(SPEECH_SYSTEM_OPENAI);
							config.speechSystem = SPEECH_SYSTEM_OPENAI;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#ifdef __AMIGAOS4__
						case MENU_ITEM_SPEECH_FLITE_VOICE_AWB_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_AWB;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_KAL_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_KAL;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_KAL16_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_KAL16;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_RMS_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_RMS;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_FLITE_VOICE_SLT_ID:
							config.speechFliteVoice = SPEECH_FLITE_VOICE_SLT;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#endif
						case MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_ID:
							config.openAITTSModel = OPENAI_TTS_MODEL_TTS_1;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD_ID:
							config.openAITTSModel = OPENAI_TTS_MODEL_TTS_1_HD;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_ALLOY;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_ECHO;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_FABLE;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_ONYX;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_NOVA;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER_ID:
							config.openAITTSVoice = OPENAI_TTS_VOICE_SHIMMER;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_OPENAI_API_KEY_ID:
							openApiKeyRequester();
							break;
						case MENU_ITEM_OPENAI_CHAT_SYSTEM_ID:
							openChatSystemRequester();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_ID:
							config.chatModel = GPT_4o;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_05_13_ID:
							config.chatModel = GPT_4o_2024_05_13;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI_ID:
							config.chatModel = GPT_4o_MINI;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI_2024_07_18_ID:
							config.chatModel = GPT_4o_MINI_2024_07_18;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_ID:
							config.chatModel = GPT_4_TURBO;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_PREVIEW_ID:
							config.chatModel = GPT_4_TURBO_PREVIEW;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0125_PREVIEW_ID:
							config.chatModel = GPT_4_0125_PREVIEW;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_1106_PREVIEW_ID:
							config.chatModel = GPT_4_1106_PREVIEW;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_ID:
							config.chatModel = GPT_4;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0613_ID:
							config.chatModel = GPT_4_0613;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_ID:
							config.chatModel = GPT_3_5_TURBO;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_0125_ID:
							config.chatModel = GPT_3_5_TURBO_0125;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_1106_ID:
							config.chatModel = GPT_3_5_TURBO_1106;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_2_ID:
							config.imageModel = DALL_E_2;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_3_ID:
							config.imageModel = DALL_E_3;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_256X256_ID:
							config.imageSizeDallE2 = IMAGE_SIZE_256x256;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_512X512_ID:
							config.imageSizeDallE2 = IMAGE_SIZE_512x512;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_1024X1024_ID:
							config.imageSizeDallE2 = IMAGE_SIZE_1024x1024;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1024_ID:
							config.imageSizeDallE3 = IMAGE_SIZE_1024x1024;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1792X1024_ID:
							config.imageSizeDallE3 = IMAGE_SIZE_1792x1024;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1792_ID:
							config.imageSizeDallE3 = IMAGE_SIZE_1024x1792;
							writeConfig();
							refreshOpenAIMenuItems();
							break;
						case MENU_ITEM_VIEW_DOCUMENTATION_ID:
							openDocumentation();
							break;
						default:
							break;
					}
					break;
				}
				case WMHI_RAWKEY:
					switch (code) {
						case HELP_KEY:
							openDocumentation();
							break;
					}
					break;
				default:
					break;
			}
		}
	}
	

	saveConversations();
	saveImages();

	return RETURN_OK;
	*/
}

/**
 * Display an error message
 * @param message the message to display
**/
void displayError(STRPTR message) {
	const LONG ERROR_CODE = IoErr();
	if (ERROR_CODE == 0) {
		if (!app || MUI_Request(app, mainWindowObject, MUIV_Requester_Image_Error, "Error", "*OK", "\33c%s", message) != 0) {
			fprintf(stderr, "%s\n", message);
		} 
	} else {
		if (app) {
			const UBYTE ERROR_BUFFER_LENGTH = 255;
			STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
			if (errorMessage) {
				Fault(ERROR_CODE, message, errorMessage, ERROR_BUFFER_LENGTH);
				MUI_Request(app, mainWindowObject, MUIV_Requester_Image_Error, "Error", "*OK", "\33c%s", errorMessage);
				updateStatusBar("Error", 6);
				FreeVec(errorMessage);
			}
		} else {
			PrintFault(ERROR_CODE, message);
		}
	}
}

/**
 * Opens a requester for the user to select the font for the chat window
**/
static void openChatFontRequester() {
	// struct FontRequester *fontRequester;
	// if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
	// 	struct TextAttr *chatFont;
	// 	if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
	// 		if (config.chatFontName != NULL) {
	// 			FreeVec(config.chatFontName);
	// 		}
	// 		chatFont = &fontRequester->fo_Attr;
	// 		config.chatFontName = AllocVec(strlen(chatFont->ta_Name) + 1, MEMF_ANY | MEMF_CLEAR);
	// 		strncpy(config.chatFontName, chatFont->ta_Name, strlen(chatFont->ta_Name));
	// 		config.chatFontSize = chatFont->ta_YSize;
	// 		config.chatFontStyle = chatFont->ta_Style;
	// 		config.chatFontFlags = chatFont->ta_Flags;
	// 		writeConfig();
	// 		// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
	// 		// SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
	// 	}
	// 	FreeAslRequest(fontRequester);
	// }
}

/**
 * Opens a requester for the user to select the font for the UI
**/
static void openUIFontRequester() {
	// struct FontRequester *fontRequester;
	// // GetAttr(WINDOW_Window, mainWindowObjectOld, &mainWindow);
	// if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
	// 	struct TextAttr *uiFont;
	// 	if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
	// 		if (config.uiFontName != NULL) {
	// 			FreeVec(config.uiFontName);
	// 		}
	// 		uiFont = &fontRequester->fo_Attr;
	// 		config.uiFontName = AllocVec(strlen(uiFont->ta_Name) + 1, MEMF_ANY | MEMF_CLEAR);
	// 		strncpy(config.uiFontName, uiFont->ta_Name, strlen(uiFont->ta_Name));
	// 		config.uiFontSize = uiFont->ta_YSize;
	// 		config.uiFontStyle = uiFont->ta_Style;
	// 		config.uiFontFlags = uiFont->ta_Flags;
	// 		writeConfig();
	// 		if (!isPublicScreen) {
	// 			if (uiTextFont)
	// 				CloseFont(uiTextFont);

	// 			// DoMethod(mainWindowObjectOld, WM_CLOSE, NULL);
	// 			uiTextFont = OpenFont(uiFont);
	// 			SetFont(&(screen->RastPort), uiTextFont);
	// 			SetFont(mainWindow->RPort, uiTextFont);
	// 			RemakeDisplay();
	// 			RethinkDisplay();
	// 			// mainWindow = DoMethod(mainWindowObjectOld, WM_OPEN, NULL);
	// 		}
	// 		// SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
	// 		// SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
	// 		// SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
	// 		// SetGadgetAttrs(statusBar, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
	// 		updateStatusBar("Ready", 5);
	// 		// SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);

	// 	}
	// 	FreeAslRequest(fontRequester);
	// }
}


/**
 * Opens a requester for the user to select accent for the speech
**/
static void openSpeechAccentRequester() {
	// #ifdef __AMIGAOS3__
	// struct FileRequester *fileRequester = (struct FileRequester *)AllocAslRequestTags(
	// 								ASL_FileRequest,
	// 								ASLFR_Window, mainWindow,
	// 								ASLFR_PopToFront, TRUE,
	// 								ASLFR_Activate, TRUE,
	// 								ASLFR_DrawersOnly, FALSE,
	// 								ASLFR_InitialDrawer, "LOCALE:accents",
	// 								ASLFR_DoPatterns, TRUE,
	// 								ASLFR_InitialPattern, "#?.accent",
	// 								TAG_DONE);

	// if (fileRequester) {
	// 	if (AslRequestTags(fileRequester, TAG_DONE)) {
	// 		if (config.speechAccent != NULL) {
	// 			FreeVec(config.speechAccent);
	// 		}
	// 		config.speechAccent = AllocVec(strlen(fileRequester->fr_File) + 1, MEMF_ANY | MEMF_CLEAR);
	// 		strncpy(config.speechAccent, fileRequester->fr_File, strlen(fileRequester->fr_File));
	// 		writeConfig();
	// 	}
	// 	FreeAslRequest(fileRequester);
	// }
	// #endif
}

/**
 * Opens a requester for the user to enter the chat system
**/
static void openChatSystemRequester() {
	// STRPTR buffer = AllocVec(CHAT_SYSTEM_LENGTH + 1, MEMF_ANY | MEMF_CLEAR);
	// if (buffer == NULL) {
	// 	displayError("Failed to allocate memory for chat system buffer");
	// 	return;
	// }
	// if (config.chatSystem != NULL) {
	// 	strncpy(buffer, config.chatSystem, CHAT_SYSTEM_LENGTH);
	// }
	// Object *chatSystemRequester = NewObject(REQUESTER_GetClass(), NULL,
	// 	REQ_Type, REQTYPE_STRING,
	// 	REQ_TitleText, "Enter how you would like AmigaGPT to respond",
	// 	REQ_BodyText, "If you would like AmigaGPT to respond in a\n"
	// 				  "certain way, or add some personality, type\n"
	// 				  "it in here. For example, you could type\n" 
	// 				  "\"Speak like a pirate\" or \"Speak like a\n"
	// 				  "robot\". If you would like AmigaGPT to respond\n"
	// 				  "normally, leave this blank. This setting will\n"
	// 				  "be applied to new conversations only.",
	// 	REQ_GadgetText, "OK|Cancel",
	// 	REQ_Image, REQIMAGE_INFO,
	// 	REQS_AllowEmpty, TRUE,
	// 	REQS_Buffer, buffer,
	// 	REQS_MaxChars, CHAT_SYSTEM_LENGTH,
	// 	REQ_ForceFocus, TRUE,
	// 	TAG_DONE);

	// if (chatSystemRequester) {
	// 	ULONG result = OpenRequester(chatSystemRequester, mainWindow);
	// 	if (result == 1) {
	// 		if (config.chatSystem != NULL) {
	// 			FreeVec(config.chatSystem);
	// 		}
	// 		config.chatSystem = AllocVec(strlen(buffer) + 1, MEMF_ANY | MEMF_CLEAR);
	// 		strncpy(config.chatSystem, buffer, strlen(buffer));
	// 		writeConfig();
	// 	}
	// 	DisposeObject(chatSystemRequester);
	// }
	// FreeVec(buffer);
}

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG saveConversations() {
	BPTR file = Open(PROGDIR"chat-history.json", MODE_NEWFILE);
	if (file == 0) {
		displayError("Failed to create message history file. Conversation history will not be saved.");
		return RETURN_ERROR;
	}

	struct json_object *conversationsJsonArray = json_object_new_array();

	LONG totalConversationCount;
	get(conversationListObject, MUIA_NList_Entries, &totalConversationCount);

	for (LONG i = 0; i < totalConversationCount; i++) {
		struct json_object *conversationJsonObject = json_object_new_object();
		struct Conversation *conversation;
		DoMethod(conversationListObject, MUIM_NList_GetEntry, i, &conversation);
		json_object_object_add(conversationJsonObject, "name", json_object_new_string(conversation->name));
		struct json_object *messagesJsonArray = json_object_new_array();
		struct ConversationNode *conversationNode;
		for (conversationNode = (struct ConversationNode *)conversation->messages->mlh_Head;
			conversationNode->node.mln_Succ != NULL;
			conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			if (!strcmp(conversationNode->role, "system")) continue;
			struct json_object *messageJsonObject = json_object_new_object();
			json_object_object_add(messageJsonObject, "role", json_object_new_string(conversationNode->role));
			json_object_object_add(messageJsonObject, "content", json_object_new_string(conversationNode->content));
			json_object_array_add(messagesJsonArray, messageJsonObject);
		}
		json_object_object_add(conversationJsonObject, "messages", messagesJsonArray);
		json_object_array_add(conversationsJsonArray, conversationJsonObject);
	}

	STRPTR conversationsJsonString = (STRPTR)json_object_to_json_string_ext(conversationsJsonArray, JSON_C_TO_STRING_PRETTY);

	if (Write(file, conversationsJsonString, strlen(conversationsJsonString)) != (LONG)strlen(conversationsJsonString)) {
		displayError("Failed to write to message history file. Conversation history will not be saved.");
		Close(file);
		json_object_put(conversationsJsonArray);
		return RETURN_ERROR;
	}

	Close(file);
	json_object_put(conversationsJsonArray);
	return RETURN_OK;
}

/**
 * Saves the images to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG saveImages() {
	// BPTR file = Open(PROGDIR"image-history.json", MODE_NEWFILE);
	// if (file == 0) {
	// 	displayError("Failed to create image history file. Image history will not be saved.");
	// 	return RETURN_ERROR;
	// }

	// struct json_object *imagesJsonArray = json_object_new_array();
	// struct json_object *imageJsonObject;
	// struct Node *imageListNode = imageList->lh_Head;
	// while (imageListNode->ln_Succ) {
	// 	imageJsonObject = json_object_new_object();
	// 	struct GeneratedImage *generatedImage;
	// 	GetListBrowserNodeAttrs(imageListNode, LBNA_UserData, &generatedImage, TAG_END);
	// 	json_object_object_add(imageJsonObject, "name", json_object_new_string(generatedImage->name));
	// 	json_object_object_add(imageJsonObject, "filePath", json_object_new_string(generatedImage->filePath));
	// 	json_object_object_add(imageJsonObject, "prompt", json_object_new_string(generatedImage->prompt));
	// 	json_object_object_add(imageJsonObject, "imageModel", json_object_new_int(generatedImage->imageModel));
	// 	json_object_object_add(imageJsonObject, "width", json_object_new_int(generatedImage->width));
	// 	json_object_object_add(imageJsonObject, "height", json_object_new_int(generatedImage->height));
	// 	json_object_array_add(imagesJsonArray, imageJsonObject);
	// 	imageListNode = imageListNode->ln_Succ;
	// }

	// STRPTR imagesJsonString = (STRPTR)json_object_to_json_string_ext(imagesJsonArray, JSON_C_TO_STRING_PRETTY);

	// if (Write(file, imagesJsonString, strlen(imagesJsonString)) != (LONG)strlen(imagesJsonString)) {
	// 	displayError("Failed to write to image history file. Image history will not be saved.");
	// 	Close(file);
	// 	json_object_put(imagesJsonArray);
	// 	return RETURN_ERROR;
	// }

	// Close(file);
	// json_object_put(imagesJsonArray);
	return RETURN_OK;
}

static void createImage() {
	struct json_object *response;

	// SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);

	// STRPTR text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);

	// // Remove trailing newline characters
	// while (text[strlen(text) - 1] == '\n') {
	// 	text[strlen(text) - 1] = '\0';
	// }
	// STRPTR textUTF_8 = ISO8859_1ToUTF8(text);

	// const enum ImageSize imageSize = config.imageModel == DALL_E_2 ? config.imageSizeDallE2 : config.imageSizeDallE3;
	// response = postImageCreationRequestToOpenAI(textUTF_8, config.imageModel, imageSize, config.openAiApiKey);
	// if (response == NULL) {
	// 	displayError("Error connecting. Please try again.");
	// 	SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	updateStatusBar("Error", 6);
	// 	return;
	// }
	// struct json_object *error;

	// if (json_object_object_get_ex(response, "error", &error)) {
	// 	struct json_object *message = json_object_object_get(error, "message");
	// 	STRPTR messageString = json_object_get_string(message);
	// 	displayError(messageString);
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, text, TAG_DONE);
	// 	json_object_put(response);
	// 	SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// 	updateStatusBar("Error", 6);
	// 	json_object_put(response);
	// 	return;
	// }

	// struct array_list *data = json_object_get_array(json_object_object_get(response, "data"));
	// struct json_object *dataObject = (struct json_object *)data->array[0];

	// STRPTR url = json_object_get_string(json_object_object_get(dataObject, "url"));

	// CreateDir(PROGDIR"images");

	// // Generate unique ID for the image
	// UBYTE fullPath[30] = "";
	// UBYTE id[11] = "";
	// CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
	// srand(time(NULL));
	// for (int i = 0; i < 9; i++) {
	// 	id[i] = idChars[rand() % strlen(idChars)];
	// }
	// snprintf(fullPath, sizeof(fullPath), PROGDIR"images/%s.png", id);

	// downloadFile(url, fullPath);

	// json_object_put(response);

	// WORD imageWidth, imageHeight;
	// switch (imageSize) {
	// 	case IMAGE_SIZE_256x256:
	// 		imageWidth = 256;
	// 		imageHeight = 256;
	// 		break;
	// 	case IMAGE_SIZE_512x512:
	// 		imageWidth = 512;
	// 		imageHeight = 512;
	// 		break;
	// 	case IMAGE_SIZE_1024x1024:
	// 		imageWidth = 1024;
	// 		imageHeight = 1024;
	// 		break;
	// 	case IMAGE_SIZE_1792x1024:
	// 		imageWidth = 1792;
	// 		imageHeight = 1024;
	// 		break;
	// 	case IMAGE_SIZE_1024x1792:
	// 		imageWidth = 1024;
	// 		imageHeight = 1792;
	// 		break;
	// 	default:
	// 		printf("Invalid image size\n");
	// 		imageWidth = 256;
	// 		imageHeight = 256;
	// 		break;
	// }

	// updateStatusBar("Generating image name...", 7);
	// struct MinList *imageNameConversation = newConversation();
	// addTextToConversation(imageNameConversation, text, "user");
	// addTextToConversation(imageNameConversation, "generate a short title for this image and don't enclose the title in quotes or prefix the response with anything", "user");
	// struct json_object **responses = postChatMessageToOpenAI(imageNameConversation, config.chatModel, config.openAiApiKey, FALSE);
	
	// struct GeneratedImage *generatedImage = AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
	// if (responses == NULL) {
	// 	displayError("Failed to generate image name. Using ID instead.");
	// 	updateStatusBar("Error", 6);
	// } else if (responses[0] != NULL) {
	// 	STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
	// 	formatText(responseString);
	// 	generatedImage->name = AllocVec(strlen(responseString) + 1, MEMF_ANY | MEMF_CLEAR);
	// 	strncpy(generatedImage->name, responseString, strlen(responseString));
	// 	updateStatusBar("Ready", 5);
	// 	json_object_put(responses[0]);
	// 	FreeVec(responses);
	// } else {
	// 	generatedImage->name = AllocVec(11, MEMF_ANY | MEMF_CLEAR);
	// 	strncpy(generatedImage->name, id, 10);
	// 	updateStatusBar("Ready", 5);
	// 	if (responses != NULL) {
	// 		FreeVec(responses);
	// 	}
	// 	displayError("Failed to generate image name. Using ID instead.");
	// }
	// freeConversation(imageNameConversation);

	// generatedImage->filePath = AllocVec(strlen(fullPath) + 1, MEMF_ANY | MEMF_CLEAR);
	// strncpy(generatedImage->filePath, fullPath, strlen(fullPath));
	// generatedImage->prompt = AllocVec(strlen(text) + 1, MEMF_ANY | MEMF_CLEAR);
	// strncpy(generatedImage->prompt, text, strlen(text));
	// generatedImage->imageModel = config.imageModel;
	// generatedImage->width = imageWidth;
	// generatedImage->height = imageHeight;
	// addImageToImageList(generatedImage);
	// currentImage = generatedImage;

	// SetGadgetAttrs(createImageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);
	// SetGadgetAttrs(openSmallImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(openMediumImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(openLargeImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(openOriginalImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(saveCopyButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(newImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(deleteImageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	// SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_ReadOnly, TRUE, TAG_DONE);

	// FreeVec(text);
	// FreeVec(textUTF_8);
	return;
}

/**
 * Opens and displays the image with sfaling
 * @param image the image to open
 * @param width the width of the image
 * @param height the height of the image
**/ 
static void openImage(struct GeneratedImage *image, WORD scaledWidth, WORD scaledHeight) {
	if (image == NULL) return;
	WORD lowestWidth = (screen->Width - 16) < scaledWidth ? (screen->Width - 16) : scaledWidth;
	WORD lowestHeight = screen->Height < scaledHeight ? screen->Height : scaledHeight;

	// struct Object *textLabel = NewObject(STRING_GetClass(), NULL,
	// 	GA_ReadOnly, TRUE,
	// 	STRINGA_Justification, GACT_STRINGCENTER,
	// 	TAG_DONE);
	
	// struct Object *imageWindowLayout = NewObject(LAYOUT_GetClass(), NULL,
	// LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
	// 	LAYOUT_AddChild, textLabel,
	// 	CHILD_MinWidth, lowestWidth,
	// 	CHILD_MinHeight, lowestHeight,
	// 	TAG_DONE);

	// if ((imageWindowObject = NewObject(WINDOW_GetClass(), NULL,
	// 	WINDOW_Position, WPOS_CENTERSCREEN,
	// 	WA_Activate, TRUE,
	// 	WA_Title, image->name,
	// 	WA_Width, lowestWidth,
	// 	WA_Height, lowestHeight,
	// 	WA_CloseGadget, TRUE,
	// 	WA_DragBar, TRUE,
	// 	WA_SizeGadget, TRUE,
	// 	WA_DepthGadget, FALSE,
	// 	WINDOW_Layout, imageWindowLayout,
	// 	WINDOW_Position, WPOS_CENTERSCREEN,
	// 	WINDOW_IDCMPHook, &idcmpHookCreateImageWindow,
	// 	WINDOW_InterpretIDCMPHook, TRUE,
	// 	WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE | IDCMP_REFRESHWINDOW,
	// 	WA_IDCMP,			IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_IDCMPUPDATE |
	// 						IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MOUSEBUTTONS |
	// 						IDCMP_MOUSEMOVE | IDCMP_VANILLAKEY |
	// 						IDCMP_RAWKEY,
	// 	WA_CustomScreen, screen,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create imageWindowObject\n");
	// 		return RETURN_ERROR;
	// }

	// if ((imageWindow = (struct Window *)DoMethod(imageWindowObject, WM_OPEN, NULL)) == NULL) {
	// 	printf("Could not open imageWindow\n");
	// 	return RETURN_ERROR;
	// }

	// SetGadgetAttrs(textLabel, imageWindow, NULL, STRINGA_TextVal, "Loading image...", TAG_DONE);

	// updateStatusBar("Loading image...", 7);
	// SetWindowPointer(imageWindow,
	// 							WA_BusyPointer,	TRUE,
	// 						TAG_DONE);

	// if ((dataTypeObject = NewDTObject(image->filePath,
	// 	DTA_SourceType, DTST_FILE,
	// 	DTA_GroupID, GID_PICTURE,
	// 	PDTA_Remap, TRUE,
	// 	GA_Text, "Loading image...",
	// 	GA_Left, imageWindow->BorderLeft,
	// 	GA_Top, imageWindow->BorderTop,
	// 	GA_RelWidth, imageWindow->Width - imageWindow->BorderLeft - imageWindow->BorderRight,
	// 	GA_RelHeight, imageWindow->Height - imageWindow->BorderTop - imageWindow->BorderBottom,
	// 	ICA_TARGET,	ICTARGET_IDCMP,
	// 	TAG_DONE)) == NULL) {
	// 		printf("Could not create dataTypeObject\n");
	// 		return RETURN_ERROR;	
	// }

	// updateStatusBar("Scaling image...", 8);
	// DoMethod(dataTypeObject, PDTM_SCALE, lowestWidth, lowestHeight, 0);

	// AddDTObject(imageWindow, NULL, dataTypeObject, -1);
	// RefreshDTObjects(dataTypeObject, imageWindow, NULL, NULL);

	// BOOL done = FALSE;
	// ULONG signalMask, winSignal, signals, result;
	// WORD code;

	// GetAttr(WINDOW_SigMask, imageWindowObject, &winSignal);
	// signalMask = winSignal;
	// while (!done) {
	// 	signals = Wait(signalMask);
	// 	while ((result = DoMethod(imageWindowObject, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
	// 		switch (result & WMHI_CLASSMASK) {
	// 			case WMHI_RAWKEY:
	// 			case WMHI_CLOSEWINDOW:
	// 			case WMHI_MOUSEBUTTONS:
	// 				done = TRUE;
	// 				break;
	// 		}
	// 	}
	// }

	// DoMethod(imageWindowObject, WM_CLOSE);
	// DisposeObject(imageWindowObject);
	// DisposeDTObject(dataTypeObject);

	// updateStatusBar("Ready", 5);
}

/**
 * Converts a UTF-8 string to ISO-8859-1 which is what the Amiga uses by default
 * Taken from https://stackoverflow.com/questions/23689733/convert-string-from-utf-8-to-iso-8859-1
 * @param utf8String the UTF-8 string to convert to ISO-8859-1
 * @return pointer to the converted string. Free it with FreeVec() when you're done with it
**/
static STRPTR UTF8ToISO8859_1(CONST_STRPTR utf8String) {
	if (utf8String == NULL)
		return NULL;

	STRPTR convertedString = AllocVec(strlen(utf8String) + 1, MEMF_ANY);
	UBYTE* convertedStringPointer = convertedString;

	UBYTE codepoint = NULL;
	while (*utf8String != 0) {
		UBYTE ch = (UBYTE)*utf8String;
		if (ch <= 0x7f)
			codepoint = ch;
		else if (ch <= 0xbf)
			codepoint = (codepoint << 6) | (ch & 0x3f);
		else if (ch <= 0xdf)
			codepoint = ch & 0x1f;
		else if (ch <= 0xef)
			codepoint = ch & 0x0f;
		else
			codepoint = ch & 0x07;
		++utf8String;
		if ((*utf8String & 0xc0) != 0x80) {
			 *convertedStringPointer = codepoint;
			convertedStringPointer++;
		}
	}
	*convertedStringPointer = '\0';

	return convertedString;
}

/**
 * Converts a UTF-8 string to ISO-8859-1 which is what the Amiga uses by default
 * Adapted from https://stackoverflow.com/questions/23689733/convert-string-from-utf-8-to-iso-8859-1
 * @param iso8859_1String the ISO-8859-1 string to convert to UTF-8
 * @return pointer to the converted string. Free it with FreeVec() when you're done with it
**/
static STRPTR ISO8859_1ToUTF8(CONST_STRPTR iso8859_1String) {
	if (iso8859_1String == NULL)
		return NULL;

	// Assume max UTF-8 size (4 bytes per character), plus null terminator
	STRPTR convertedString = AllocVec((4 * strlen(iso8859_1String) + 1) * sizeof(char), MEMF_ANY);
	UBYTE* convertedStringPointer = convertedString;

	while (*iso8859_1String != 0) {
		UBYTE ch = (UBYTE)*iso8859_1String;

		if(ch < 128) { 
			*convertedStringPointer++ = ch;
		} 
		else {
			*convertedStringPointer++ = 0xc0 | (ch >> 6);    // first byte
			*convertedStringPointer++ = 0x80 | (ch & 0x3f);  // second byte
		}
		
		++iso8859_1String;
	}
	
	*convertedStringPointer = '\0';

	return convertedString;
}

/**
 * Load the conversations from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG loadConversations() {
	BPTR file = Open(PROGDIR"chat-history.json", MODE_OLDFILE);
	if (file == 0) {
		return RETURN_OK;
	}

	#ifdef __AMIGAOS3__
	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	#else
	#ifdef __AMIGAOS4__
	int64_t fileSize = GetFileSize(file);
	#else
	struct FileInfoBlock fib;
	ExamineFH64(file, &fib, NULL);
	int64_t fileSize = fib.fib_Size;
	#endif
	#endif
	STRPTR conversationsJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, conversationsJsonString, fileSize) != fileSize) {
		displayError("Failed to read from message history file. Conversation history will not be loaded");
		Close(file);
		FreeVec(conversationsJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *conversationsJsonArray = json_tokener_parse(conversationsJsonString);
	if (conversationsJsonArray == NULL) {
		if (Rename(PROGDIR"chat-history.json", PROGDIR"chat-history.json.bak")) {
			displayError("Failed to parse chat history. Malformed JSON. The chat-history.json file is probably corrupted. Conversation history will not be loaded. A backup of the chat-history.json file has been created as chat-history.json.bak");
		} else if (copyFile(PROGDIR"chat-history.json", "RAM:chat-history.json")) {
			displayError("Failed to parse chat history. Malformed JSON. The chat-history.json file is probably corrupted. Conversation history will not be loaded. There was an error writing a backup of the chat history to disk but a copy has been saved to RAM:chat-history.json.bak");
			#ifdef __AMIGAOS3__
			if (!DeleteFile(PROGDIR"chat-history.json")) {
			#else
			if (!Delete(PROGDIR"chat-history.json")) {
			#endif
				displayError("Failed to delete chat-history.json. Please delete this file manually.");
			}
		}

		FreeVec(conversationsJsonString);
		return RETURN_ERROR;
	}

	for (UWORD i = 0; i < json_object_array_length(conversationsJsonArray); i++) {
		struct json_object *conversationJsonObject = json_object_array_get_idx(conversationsJsonArray, i);
		struct json_object *conversationNameJsonObject;
		if (!json_object_object_get_ex(conversationJsonObject, "name", &conversationNameJsonObject)) {
			displayError("Failed to parse chat history. \"name\" is missing from the conversation. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
			FreeVec(conversationsJsonString);
			json_object_put(conversationsJsonArray);
			return RETURN_ERROR;
		}

		STRPTR conversationName = json_object_get_string(conversationNameJsonObject);

		struct json_object *messagesJsonArray;
		if (!json_object_object_get_ex(conversationJsonObject, "messages", &messagesJsonArray)) {
			displayError("Failed to parse chat history. \"messages\" is missing from the conversation. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
			FreeVec(conversationsJsonString);
			json_object_put(conversationsJsonArray);
			return RETURN_ERROR;
		}

		struct Conversation *conversation = newConversation();
		conversation->name = AllocVec(strlen(conversationName) + 1, MEMF_ANY | MEMF_CLEAR);
		strncpy(conversation->name, conversationName, strlen(conversationName));

		set(conversationListObject, MUIA_NList_Quiet, TRUE);

		for (UWORD j = 0; j < json_object_array_length(messagesJsonArray); j++) {
			struct json_object *messageJsonObject = json_object_array_get_idx(messagesJsonArray, j);
			struct json_object *roleJsonObject;
			if (!json_object_object_get_ex(messageJsonObject, "role", &roleJsonObject)) {
				displayError("Failed to parse chat history. \"role\" is missing from the conversation message. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
				FreeVec(conversationsJsonString);
				json_object_put(conversationsJsonArray);
				return RETURN_ERROR;
			}
			STRPTR role = json_object_get_string(roleJsonObject);

			struct json_object *contentJsonObject = json_object_array_get_idx(messagesJsonArray, j);
			if (!json_object_object_get_ex(messageJsonObject, "content", &contentJsonObject)) {
				displayError("Failed to parse chat history. \"content\" is missing from the conversation. The chat-history.json file is probably corrupted. Conversation history will not be loaded.");
				FreeVec(conversationsJsonString);
				json_object_put(conversationsJsonArray);
				return RETURN_ERROR;
			}
			STRPTR content = json_object_get_string(contentJsonObject);
			addTextToConversation(conversation, content, role);
		}
		DoMethod(conversationListObject, MUIM_NList_InsertSingle, conversation, MUIV_NList_Insert_Top);
		freeConversation(conversation);
	}
	set(conversationListObject, MUIA_NList_Quiet, FALSE);
	json_object_put(conversationsJsonArray);
	return RETURN_OK;
}

/**
 * Load the images from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG loadImages() {
	BPTR file = Open(PROGDIR"image-history.json", MODE_OLDFILE);
	if (file == 0) {
		return RETURN_OK;
	}

	#ifdef __AMIGAOS3__
	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	#else
	#ifdef __AMIGAOS4__
	int64_t fileSize = GetFileSize(file);
	#else
	struct FileInfoBlock fib;
	ExamineFH64(file, &fib, NULL);
	int64_t fileSize = fib.fib_Size;
	#endif
	#endif
	STRPTR imagesJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, imagesJsonString, fileSize) != fileSize) {
		displayError("Failed to read from image history file. Image generation history will not be loaded");
		Close(file);
		FreeVec(imagesJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *imagesJsonArray = json_tokener_parse(imagesJsonString);
	if (imagesJsonArray == NULL) {
		if (Rename(PROGDIR"image-history.json", PROGDIR"image-history.json.bak")) {
			displayError("Failed to parse image history. Malformed JSON. The image-history.json file is probably corrupted. Image history will not be loaded. A backup of the image-history.json file has been created as image-history.json.bak");
		} else if (copyFile(PROGDIR"image-history.json", "RAM:image-history.json")) {
			displayError("Failed to parse image history. Malformed JSON. The image-history.json file is probably corrupted. Image history will not be loaded. There was an error writing a backup of the image history to disk but a copy has been saved to RAM:image-history.json.bak");
			#ifdef __AMIGAOS3__
			if (!DeleteFile(PROGDIR"image-history.json")) {
			#else
			if (!Delete(PROGDIR"image-history.json")) {
			#endif
				displayError("Failed to delete image-history.json. Please delete this file manually.");
			}
		}

		FreeVec(imagesJsonString);
		return RETURN_ERROR;
	}

	for (UWORD i = 0; i < json_object_array_length(imagesJsonArray); i++) {
		struct json_object *imageJsonObject = json_object_array_get_idx(imagesJsonArray, i);
		struct json_object *imageNameJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "name", &imageNameJsonObject)) {
			displayError("Failed to parse image history. \"name\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		STRPTR imageName = json_object_get_string(imageNameJsonObject);

		struct json_object *imageFilePathJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "filePath", &imageFilePathJsonObject)) {
			displayError("Failed to parse image history. \"filePath\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		STRPTR imageFilePath = json_object_get_string(imageFilePathJsonObject);

		struct json_object *imagePromptJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "prompt", &imagePromptJsonObject)) {
			displayError("Failed to parse image history. \"prompt\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		STRPTR imagePrompt = json_object_get_string(imagePromptJsonObject);

		struct json_object *imageModelJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "imageModel", &imageModelJsonObject)) {
			displayError("Failed to parse image history. \"imageModel\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		enum ImageModel imageModel = json_object_get_int(imageModelJsonObject);

		struct json_object *imageWidthJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "width", &imageWidthJsonObject)) {
			displayError("Failed to parse image history. \"width\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		UWORD imageWidth = (WORD)json_object_get_int(imageWidthJsonObject);

		struct json_object *imageHeightJsonObject;
		if (!json_object_object_get_ex(imageJsonObject, "height", &imageHeightJsonObject)) {
			displayError("Failed to parse image history. \"height\" is missing from the image. The image-history.json file is probably corrupted. Image history will not be loaded.");
			FreeVec(imagesJsonString);
			json_object_put(imagesJsonArray);
			return RETURN_ERROR;
		}

		UWORD imageHeight = (WORD)json_object_get_int(imageHeightJsonObject);

		struct GeneratedImage *generatedImage = AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
		generatedImage->name = AllocVec(strlen(imageName) + 1, MEMF_ANY);
		strcpy(generatedImage->name, imageName);
		generatedImage->filePath = AllocVec(strlen(imageFilePath) + 1, MEMF_ANY);
		strcpy(generatedImage->filePath, imageFilePath);
		generatedImage->prompt = AllocVec(strlen(imagePrompt) + 1, MEMF_ANY);
		strcpy(generatedImage->prompt, imagePrompt);
		generatedImage->imageModel = imageModel;
		generatedImage->width = imageWidth;
		generatedImage->height = imageHeight;
		addImageToImageList(generatedImage);
	}

	json_object_put(imagesJsonArray);
	return RETURN_OK;
}

/**
 * Copies a file from one location to another
 * @param source The source file to copy
 * @param destination The destination to copy the file to
 * @return TRUE if the file was copied successfully, FALSE otherwise
**/
static BOOL copyFile(STRPTR source, STRPTR destination) {
	const UBYTE ERROR_MESSAGE_BUFFER_SIZE = 255;
	const UWORD FILE_BUFFER_SIZE = 4096;
	BPTR srcFile, dstFile;
	LONG bytesRead, bytesWritten;
	APTR buffer = AllocVec(FILE_BUFFER_SIZE, MEMF_ANY);
	STRPTR errorMessage = AllocVec(ERROR_MESSAGE_BUFFER_SIZE, MEMF_ANY);

	if (!(srcFile = Open(source, MODE_OLDFILE))) {
		snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error opening %s for copy", source);
		displayError(errorMessage);
		FreeVec(buffer);
		FreeVec(errorMessage);
		return FALSE;
	}

	if (!(dstFile = Open(destination, MODE_NEWFILE))) {
		snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error creating %s for copy", destination);
		displayError(errorMessage);
		FreeVec(buffer);
		FreeVec(errorMessage);
		Close(srcFile);
		return FALSE;
	}

	updateStatusBar("Copying file...", 7);

	do {
		bytesRead = Read(srcFile, buffer, FILE_BUFFER_SIZE);

		if (bytesRead > 0) {
			bytesWritten = Write(dstFile, buffer, bytesRead);

			if (bytesWritten != bytesRead) {
				updateStatusBar("Ready", 5);
				snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
				displayError(errorMessage);
				FreeVec(buffer);
				FreeVec(errorMessage);
				Close(srcFile);
				Close(dstFile);
				return FALSE;
			}
		}
		else if (bytesRead < 0) {
			updateStatusBar("Ready", 5);
			snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
			displayError(errorMessage);
			FreeVec(buffer);
			FreeVec(errorMessage);
			Close(srcFile);
			Close(dstFile);
			return FALSE;
		}
	} while (bytesRead > 0);

	updateStatusBar("Ready", 5);
	FreeVec(buffer);
	FreeVec(errorMessage);
	Close(srcFile);
	Close(dstFile);
	return TRUE;
}

/**
 * Shutdown the GUI
**/
void shutdownGUI() {
	saveImages();
	DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
	MUI_DisposeObject(app);
	// freeImageList();
	if (isPublicScreen) {
		ReleasePen(screen->ViewPort.ColorMap, sendMessageButtonPen);
		ReleasePen(screen->ViewPort.ColorMap, newChatButtonPen);
		ReleasePen(screen->ViewPort.ColorMap, deleteButtonPen);
		UnlockPubScreen(NULL, screen);
	} else {
		CloseScreen(screen);
	}
	if (uiTextFont)
		CloseFont(uiTextFont);
	if (appPort)
		DeleteMsgPort(appPort);

	closeGUILibraries();
}