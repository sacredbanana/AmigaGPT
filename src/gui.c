#include "gui.h"
#include <stdio.h>
#include <exec/lists.h>
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
#include <gadgets/listbrowser.h>
#include <devices/conunit.h>
#include <exec/execbase.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classusr.h>
#include <intuition/icclass.h>
#include <graphics/text.h>
#include <proto/button.h>
#include <proto/radiobutton.h>
#include <proto/window.h>
#include <proto/texteditor.h>
#include <proto/listbrowser.h>
#include <proto/string.h>
#include <proto/asl.h>
#include <proto/scroller.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <classes/window.h>
#include "speech.h"
#include "openai.h"
#include "version.h"
#include <stdbool.h>
#include "external/json-c/json.h"

// TODO: Fix edit menu

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
#define CONVERSATION_LIST_BROWSER_ID 7
#define CONVERSATION_LIST_BROWSER_WIDTH 100
#define CONVERSATION_LIST_BROWSER_HEIGHT 200
#define NEW_CHAT_BUTTON_ID 8
#define NEW_CHAT_BUTTON_WIDTH 100
#define NEW_CHAT_BUTTON_HEIGHT 20

#define MENU_ITEM_ABOUT_ID 1
#define MENU_ITEM_PREFERENCES_ID 2
#define MENU_ITEM_QUIT_ID 3
#define MENU_ITEM_SPEECH_ENABLED_ID 4
#define MENU_ITEM_CHAT_FONT_ID 5
#define MENU_ITEM_UI_FONT_ID 6
#define MENU_ITEM_SPEECH_SYSTEM_OLD_ID 7
#define MENU_ITEM_SPEECH_SYSTEM_NEW_ID 8
#define MENU_ITEM_CUT_ID 9
#define MENU_ITEM_COPY_ID 10
#define MENU_ITEM_PASTE_ID 11
#define MENU_ITEM_CLEAR_ID 12
#define MENU_ITEM_SELECT_ALL_ID 13
#define MENU_ITEM_MODEL_GPT_4_ID 14
#define MENU_ITEM_MODEL_GPT_4_0314_ID 15
#define MENU_ITEM_MODEL_GPT_4_32K_ID 16
#define MENU_ITEM_MODEL_GPT_4_32K_0314_ID 17
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_ID 18
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID 19
#define MENU_ITEM_MODEL_ID 20
#define MENU_ITEM_SPEECH_SYSTEM_ID 21

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
static struct Library *ListBrowserBase;
static struct Window *mainWindow;
static Object *mainWindowObject;
static Object *mainLayout;
static Object *chatLayout;
static Object *chatInputLayout;
static Object *chatOutputLayout;
static Object *conversationsLayout;
static Object *sendMessageButton;
static Object *textInputTextEditor;
static Object *chatOutputTextEditor;
static Object *chatOutputScroller;
static Object *statusBar;
static Object *conversationListBrowser;
static Object *newChatButton;
static struct Screen *screen;
static BOOL isPublicScreen;
static UWORD pens[] = {~0};
struct MinList *currentConversation;
struct List *conversationList;

static struct Config {
	BOOL speechEnabled;
	enum SpeechSystem speechSystem;
	enum Model model;
	UBYTE chatFontName[32];
	UWORD chatFontSize;
	UBYTE chatFontStyle;
	UBYTE chatFontFlags;
	UBYTE uiFontName[32];
	UWORD uiFontSize;
	UBYTE uiFontStyle;
	UBYTE uiFontFlags;
	UBYTE openAiApiKey[64];
	ULONG colors[32];
};
struct Config config = {
	.speechEnabled = TRUE,
	.speechSystem = SpeechSystemOld,
	.model = GPT_3_5_TURBO,
	.chatFontName = {0},
	.chatFontSize = 8,
	.chatFontStyle = FS_NORMAL,
	.chatFontFlags = FPF_DISKFONT | FPF_DESIGNED,
	.uiFontName = {0},
	.uiFontSize = 8,
	.uiFontStyle = FS_NORMAL,
	.uiFontFlags = FPF_DISKFONT | FPF_DESIGNED,
	.openAiApiKey = NULL,
	.colors = {
		5l<<16+0,
		0x00000000, 0x11111111, 0x55555555,
		0xAAAAAAAA, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x33333333,
		0xBBBBBBBB, 0xFFFFFFFF, 0x22222222,
		0x00000000, 0x00000000, 0x00000000,
		0
	}
};

static struct TextAttr screenFont = {
	.ta_Name = "",
	.ta_YSize = 8,
	.ta_Style = FS_NORMAL,
	.ta_Flags = FPF_DISKFONT | FPF_DESIGNED
};

struct TextFont *loadedFont = NULL;

static struct NewMenu amigaGPTMenu[] = {
	{NM_TITLE, "Project", 0, 0, 0, 0},
	{NM_ITEM, "About", 0, 0, 0, MENU_ITEM_ABOUT_ID},
	{NM_ITEM, "Preferences", "P", 0, 0, MENU_ITEM_PREFERENCES_ID},
	{NM_ITEM, NM_BARLABEL, 0, 0, 0, 0},
	{NM_ITEM, "Quit", "Q", 0, 0, MENU_ITEM_QUIT_ID},
	{NM_TITLE, "Edit", 0, 0, 0, 0},
	{NM_ITEM, "Cut", "X", 0, 0, MENU_ITEM_CUT_ID},
	{NM_ITEM, "Copy", "C", 0, 0, MENU_ITEM_COPY_ID},
	{NM_ITEM, "Paste", "V", 0, 0, MENU_ITEM_PASTE_ID},
	{NM_ITEM, "Clear", "L", 0, 0, MENU_ITEM_CLEAR_ID},
	{NM_ITEM, "Select all", "A", 0, 0, MENU_ITEM_SELECT_ALL_ID},
	{NM_TITLE, "View", 0, 0, 0, 0},
	{NM_ITEM, "Chat Font", 0, 0, 0, MENU_ITEM_CHAT_FONT_ID},
	{NM_ITEM, "UI Font", 0, 0, 0, MENU_ITEM_UI_FONT_ID},
	{NM_TITLE, "Speech", 0, 0, 0, 0},
	{NM_ITEM, "Enabled", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_ENABLED_ID},
	{NM_ITEM, "Speech system", 0, 0, 0, MENU_ITEM_SPEECH_SYSTEM_ID},
	{NM_SUB, "Old", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_SYSTEM_OLD_ID},
	{NM_SUB, "New", 0, CHECKIT, 0, MENU_ITEM_SPEECH_SYSTEM_NEW_ID},
	{NM_TITLE, "OpenAI", 0, 0, 0, 0},
	{NM_ITEM, "Model", 0, 0, 0, MENU_ITEM_MODEL_ID},
	{NM_SUB, "gpt-4", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_ID},
	{NM_SUB, "gpt-4-0314", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_0314_ID},
	{NM_SUB, "gpt-4-32k", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_32K_ID},
	{NM_SUB, "gpt-4-32k-0314", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_32K_0314_ID},
	{NM_SUB, "gpt-3.5-turbo", 0, CHECKIT|CHECKED, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_ID},
	{NM_SUB, "gpt-3.5-turbo-0301", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID},
	{NM_END, NULL, 0, 0, 0, 0}
};

static void sendMessage();
static void closeGUILibraries();
static LONG selectScreen();
static void clearModelMenuItems(struct Menu *menu);
static struct MinList* newConversation();
static void addTextToConversation(struct MinList *conversation, STRPTR text, STRPTR role);
static void addConversationToConversationList(struct List *conversationList, struct MinList *conversation, STRPTR title);
static struct MinList* getConversationFromConversationList(struct List *conversationList, ULONG index);
static void displayConversation(struct MinList *conversation);
static void freeConversation(struct MinList *conversation);
static void freeConversationList();
static LONG writeConfig();
static LONG readConfig();

/**
 * Open the libraries needed for the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
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

	if ((ListBrowserBase = OpenLibrary("gadgets/listbrowser.gadget", 47)) == NULL) {
		printf("Could not open listbrowser.gadget\n");
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

/**
 * Close the libraries used by the GUI
**/
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

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initVideo() {
	if (openGUILibraries() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	readConfig();

	if (screen == NULL && selectScreen() == RETURN_ERROR)
		return RETURN_ERROR;

	conversationList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	NewList(conversationList);
	currentConversation = NULL;

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

	if ((newChatButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, NEW_CHAT_BUTTON_ID,
		GA_WIDTH, NEW_CHAT_BUTTON_WIDTH,
		GA_HEIGHT, NEW_CHAT_BUTTON_HEIGHT,
		BUTTON_TextPen, 3,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)"+ New Chat",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create new chat button\n");
			return RETURN_ERROR;
	}

	if ((conversationListBrowser = NewObject(LISTBROWSER_GetClass(), NULL,
		GA_ID, CONVERSATION_LIST_BROWSER_ID,
		GA_RelVerify, TRUE,
		GA_Width, CONVERSATION_LIST_BROWSER_WIDTH,
		GA_Height, CONVERSATION_LIST_BROWSER_HEIGHT,
		LISTBROWSER_WrapText, TRUE,
		LISTBROWSER_AutoFit, TRUE,
		LISTBROWSER_Labels, conversationList,
		TAG_DONE)) == NULL) {
			printf("Could not create conversation list browser\n");
			return RETURN_ERROR;
	}

	if ((conversationsLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, newChatButton,
		CHILD_WeightedHeight, 10,
		LAYOUT_AddChild, conversationListBrowser,
		CHILD_WeightedWidth, 90,
		TAG_DONE)) == NULL) {
			printf("Could not create conversations layout\n");
			return RETURN_ERROR;
	}

	struct TagItem chatOutputTextEditorMap[] = {
		{GA_TEXTEDITOR_Prop_First, SCROLLER_Top},
		{GA_TEXTEDITOR_Prop_Entries, SCROLLER_Total},
		{GA_TEXTEDITOR_Prop_Visible, SCROLLER_Visible},
		{TAG_DONE}
	};

	if ((chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Width, CHAT_OUTPUT_TEXT_EDITOR_WIDTH,
		GA_Height, CHAT_OUTPUT_TEXT_EDITOR_HEIGHT,
		GA_ReadOnly, TRUE,
		GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_MIME,
		GA_TEXTEDITOR_ExportHook, GV_TEXTEDITOR_ExportHook_Plain,
		ICA_MAP, &chatOutputTextEditorMap,
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

	struct TextAttr chatOutputTextEditorTextAttr = {
		.ta_Name = config.chatFontName,
		.ta_YSize = config.chatFontSize,
		.ta_Style = config.chatFontStyle,
		.ta_Flags = config.chatFontFlags
	};

	if ((textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Text, (ULONG)"",
		GA_Width, TEXT_INPUT_TEXT_EDITOR_WIDTH,
		GA_Height, TEXT_INPUT_TEXT_EDITOR_HEIGHT,
		GA_TextAttr, (ULONG)&chatOutputTextEditorTextAttr,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	struct TagItem chatOutputTextScrollerMap[] = {
		{GA_TEXTEDITOR_Prop_First, SCROLLER_Top},
		{TAG_DONE}
	};

	if ((chatOutputScroller = NewObject(SCROLLER_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_SCROLLER_ID,
		GA_RelVerify, TRUE,
		GA_Width, CHAT_OUTPUT_SCROLLER_WIDTH,
		GA_Height, CHAT_OUTPUT_SCROLLER_HEIGHT,
		SCROLLER_Top, 0,
		SCROLLER_Visible, 100,
		SCROLLER_Total, 100,
		ICA_MAP, &chatOutputTextScrollerMap,
		ICA_TARGET, chatOutputTextEditor,
		TAG_DONE)) == NULL) {
			printf("Could not create scroller\n");
			return RETURN_ERROR;
	}

	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, ICA_TARGET, chatOutputScroller, TAG_DONE);
	
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
		LAYOUT_AddChild, conversationsLayout,
		CHILD_WeightedWidth, 30,
		LAYOUT_AddChild, chatLayout,
		CHILD_WeightedWidth, 70,
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

/**
 * Display a requester for the screen the application should open on
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG selectScreen() {
	Object *screenSelectRadioButton, *selectScreenOkButton, *screenSelectLayout, *screenSelectWindowObject;
	struct Window *screenSelectWindow;
	struct ScreenModeRequester *screenModeRequester;
	screen = LockPubScreen("Workbench");

	if (strlen(config.uiFontName) > 0) {
		screenFont.ta_Name = config.uiFontName;
		screenFont.ta_YSize = config.uiFontSize;
		screenFont.ta_Style = config.uiFontStyle;
		screenFont.ta_Flags = config.uiFontFlags;
	}

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
					SA_Font, &screenFont,
					SA_Colors32, config.colors,
					TAG_DONE)) == NULL) {
						printf("Could not open screen\n");
						return RETURN_ERROR;
				}
			}
		}
	} else {
		// Open in Workbench
		isPublicScreen = TRUE;
	}

	DoMethod(screenSelectWindowObject, WM_CLOSE);

	return RETURN_OK;
}

/**
 * Sends a message to the OpenAI API and displays the response and speaks it if speech is anabled
**/ 
static void sendMessage() {
	BOOL isNewConversation = FALSE;
	if (currentConversation == NULL) {
		isNewConversation = TRUE;
		currentConversation = newConversation();
	}
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, TRUE, TAG_DONE);
	SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Sending", TAG_DONE);

	STRPTR text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	addTextToConversation(currentConversation, text, "user");
	displayConversation(currentConversation);
	DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);

	STRPTR response = postMessageToOpenAI(currentConversation, config.model, config.openAiApiKey);
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, FALSE, TAG_DONE);
	if (response != NULL) {
		addTextToConversation(currentConversation, response, "assistant");
		displayConversation(currentConversation);
		SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
		SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TEXTEDITOR_Pen, 0, TAG_DONE);
		if (config.speechEnabled)
			speakText(response);
		FreeVec(response);
		if (isNewConversation) {
			SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Generating conversation title", TAG_DONE);
			addTextToConversation(currentConversation, "generate a short title for this conversation and don't enclose the title in quotes or prefix the response with anything", "user");
			response = postMessageToOpenAI(currentConversation, config.model, config.openAiApiKey);
			if (response != NULL) {
				addConversationToConversationList(conversationList, currentConversation, response);
				SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
				struct MinNode *titleRequestNode = RemTail(currentConversation);
				FreeVec(titleRequestNode);
				FreeVec(response);
			}
		}
	} else {
		SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "No response from OpenAI", TAG_DONE);
		printf("No response from OpenAI\n");
	}
	FreeVec(text);
}

/**
 * Clears all the checkboxes for all the models in the menu
 * @param menu The menu to clear the checkboxes for
**/ 
static void clearModelMenuItems(struct Menu *menu) {
	while (strcmp(menu->MenuName, "OpenAI") != 0) {
		menu = menu->NextMenu;
	}
	struct MenuItem *menuItem = menu->FirstItem;
	while (GTMENUITEM_USERDATA(menuItem) != MENU_ITEM_MODEL_ID) {
		menuItem = menuItem->NextItem;
	}
	menuItem = menuItem->SubItem;
	while (menuItem != NULL) {
		menuItem->Flags &= ~CHECKED;
		menuItem = menuItem->NextItem;
	}
}

/**
 * Clears all the checkboxes for all the speech systems in the menu
 * @param menu The menu to clear the checkboxes for
**/ 
static void clearSpeechSystemMenuItems(struct Menu *menu) {
	while (strcmp(menu->MenuName, "Speech") != 0) {
		menu = menu->NextMenu;
	}
	struct MenuItem *menuItem = menu->FirstItem;
	while (GTMENUITEM_USERDATA(menuItem) != MENU_ITEM_SPEECH_SYSTEM_ID) {
		menuItem = menuItem->NextItem;
	}
	menuItem = menuItem->SubItem;
	while (menuItem != NULL) {
		menuItem->Flags &= ~CHECKED;
		menuItem = menuItem->NextItem;
	}
}

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
**/
static struct MinList* newConversation() {
	struct MinList *conversation = AllocVec(sizeof(struct MinList), MEMF_CLEAR);
	NewMinList(conversation);
	return conversation;
}

/**
 * Add a block of text to the conversation list
 * @param conversation The conversation to add the text to
 * @param text The text to add to the conversation
 * @param role The role of the text (user or assistant)
**/ 
static void addTextToConversation(struct MinList *conversation, STRPTR text, STRPTR role) {
	struct ConversationNode *conversationNode = AllocVec(sizeof(struct ConversationNode), MEMF_CLEAR);
	if (conversationNode == NULL) {
		printf("Failed to allocate memory for conversation node\n");
		return;
	}
	strncpy(conversationNode->role, role, sizeof(conversationNode->role) - 1);
    conversationNode->role[sizeof(conversationNode->role) - 1] = '\0';
    strncpy(conversationNode->content, text, sizeof(conversationNode->content) - 1);
    conversationNode->content[sizeof(conversationNode->content) - 1] = '\0';
	AddTail(conversation, (struct Node *)conversationNode);
}

/**
 * Add a conversation to the conversation list
 * @param conversationList The conversation list to add the conversation to
 * @param conversation The conversation to add to the conversation list
 * @param title The title of the conversation
**/ 
static void addConversationToConversationList(struct List *conversationList, struct MinList *conversation, STRPTR title) {
	struct Node *node;
	if ((node = AllocListBrowserNode(1,
		LBNCA_CopyText, TRUE,
		LBNCA_Text, title,
		LBNA_UserData, (ULONG)conversation,
		TAG_DONE)) == NULL) {
			printf("Could not create conversation list browser node\n");
			return RETURN_ERROR;
	}

	SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);		
	AddHead(conversationList, node);
	SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, conversationList, TAG_DONE);
}

/**
 * Get a conversation from the conversation list
 * @param conversationList The conversation list to get the conversation from
 * @param index The index of the conversation to get
 * @return A pointer to the conversation
**/
static struct MinList* getConversationFromConversationList(struct List *conversationList, ULONG index) {
	struct Node *node = conversationList->lh_Head->ln_Succ;
	while (index > 0) {
		node = node->ln_Succ;
		index--;
	}
	STRPTR conversation;
	GetListBrowserNodeAttrs(node, LBNA_UserData,&conversation, TAG_END);
	return conversation;
}

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
**/
static void displayConversation(struct MinList *conversation) {
    struct ConversationNode *conversationNode;
    DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	STRPTR conversationString = AllocVec(WRITE_BUFFER_LENGTH, MEMF_CLEAR);

    for (conversationNode = (struct ConversationNode *)conversation->mlh_Head; 
         conversationNode->node.mln_Succ != NULL; 
         conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			if ((strlen(conversationString) + strlen(conversationNode->content) + 5) > WRITE_BUFFER_LENGTH) {
				// TODO: Handle this better
				return;
			}
			if (strcmp(conversationNode->role, "user") == 0) {
				if (strlen(conversationString) == 0)
					sprintf(conversationString, "*%s*", conversationNode->content);
				else
					sprintf(conversationString, "%s\n\n*%s*\0", conversationString, conversationNode->content);
			} else {
				sprintf(conversationString, "%s\n\n%s\0", conversationString, conversationNode->content);
			}
    }

	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, conversationString, TAG_DONE);
	FreeVec(conversationString);
}

/**
 * Free the conversation
 * @param conversation The conversation to free
**/ 
static void freeConversation(struct MinList *conversation) {
	struct ConversationNode *conversationNode;
	while ((conversationNode = (struct ConversationNode *)RemHead(conversation)) != NULL) {
		FreeVec(conversationNode);
	}
	FreeVec(conversation);
}

/**
 * Free the conversation list
**/
static void freeConversationList() {
	struct Node *conversationListNode;
	while ((conversationListNode = RemHead(conversationList)) != NULL) {
		ULONG *conversation;
		GetListBrowserNodeAttrs(conversationListNode, LBNA_UserData, (ULONG *)&conversation, TAG_END);
		freeConversation(conversation);
		FreeVec(conversationListNode);
	}
	FreeVec(conversationList);
}

/**
 * The main run loop of the GUI
 * @return The return code of the application
 * @see RETURN_OK
 * @see RETURN_ERROR
**/ 
LONG startGUIRunLoop() {
    ULONG signalMask, winSignal, signals, result;
	BOOL done = FALSE;
    WORD code;

    GetAttr(WINDOW_SigMask, mainWindowObject, &winSignal);
	signalMask = winSignal;

	SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GFLG_SELECTED, TRUE, TAG_DONE);

    while (!done) {
        signals = Wait(signalMask);

		BOOL isTextInputTextEditorActive = GetAttr(GFLG_SELECTED, textInputTextEditor, NULL);
		BOOL isChatOutputTextEditorActive = GetAttr(GFLG_SELECTED, chatOutputTextEditor, NULL);

		while ((result = DoMethod(mainWindowObject, WM_HANDLEINPUT, &code)) != WMHI_LASTMSG) {
			switch (result & WMHI_CLASSMASK) {
				case WMHI_CLOSEWINDOW:
					done = TRUE;
					break;
				case WMHI_GADGETUP:
					switch (result & WMHI_GADGETMASK) {
						case SEND_MESSAGE_BUTTON_ID:
						case TEXT_INPUT_TEXT_EDITOR_ID:
							sendMessage();
							break;
						case NEW_CHAT_BUTTON_ID:
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
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
					}
					break;
				case WMHI_MENUPICK:
				{
					struct Menu *menuStrip;
					GetAttr(WINDOW_MenuStrip, mainWindowObject, &menuStrip);
					struct MenuItem *menuItem = ItemAddress(menuStrip, code);
					ULONG itemIndex = GTMENUITEM_USERDATA(menuItem);
					switch (itemIndex) {
						case MENU_ITEM_ABOUT_ID: 
						{
							#define APP_VERSION_STRING(x) #x
							#define APP_BUILD_DATE_STRING(x) #x
							struct EasyStruct aboutRequester = {
								sizeof(struct EasyStruct),
								0,
								"About",
								"AmigaGPT\n\n"
								"Version " APP_VERSION "\n"
								"Build date: " __DATE__ "\n"
								"Build number: " BUILD_NUMBER "\n\n"
								"Developed by Cameron Armstrong (@sacredbanana on GitHub,\n"
								"YouTube and Twitter, @Nightfox on EAB)\n\n"
								"This app will always remain free but if you would like to\n"
								"support me you can do so at https://paypal.me/sacredbanana",
								"OK"
							};
							EasyRequest(mainWindow, &aboutRequester, NULL, NULL);
							break;
						}
						case MENU_ITEM_PREFERENCES_ID:
							break;
						case MENU_ITEM_CUT_ID:
						{
							STRPTR result;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");

							if (result == FALSE)
								printf("Error cutting text\n");
							else if (result == TRUE)
								printf("Text cut\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_COPY_ID:
						{
							STRPTR result;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");

							if (result == FALSE)
								printf("Error copying text\n");
							else if (result == TRUE)
								printf("Text copied\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_PASTE_ID:
						{
							STRPTR result;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "PASTE");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "PASTE");

							if (result == FALSE)
								printf("Error pasting text\n");
							else if (result == TRUE)
								printf("Text pasted\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_SELECT_ALL_ID:
						{
							STRPTR result;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);

							if (result == FALSE)
								printf("Error selecting all text\n");
							else if (result == TRUE)
								printf("All text selected\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_CLEAR_ID:
						{
							STRPTR result;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CLEAR");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CLEAR");

							if (result == FALSE)
								printf("Error clearing text\n");
							else if (result == TRUE)
								printf("Text cleared\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_QUIT_ID:
							done = TRUE;
							break;
						case MENU_ITEM_CHAT_FONT_ID:
							{
								struct FontRequester *fontRequester;
								if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
									struct TextAttr *chatFont;
									if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
										chatFont = &fontRequester->fo_Attr;
										strncpy(config.chatFontName, chatFont->ta_Name, sizeof(config.chatFontName) - 1);
										config.chatFontName[sizeof(config.chatFontName) - 1] = '\0';
										config.chatFontSize = chatFont->ta_YSize;
										config.chatFontStyle = chatFont->ta_Style;
										config.chatFontFlags = chatFont->ta_Flags;
										writeConfig();
									}
									SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
									SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
									FreeAslRequest(fontRequester);
								}
								break;
							}
						case MENU_ITEM_UI_FONT_ID:
							{
								struct FontRequester *fontRequester;
								if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
									struct TextAttr *uiFont;
									if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
										uiFont = &fontRequester->fo_Attr;
										strncpy(config.uiFontName, uiFont->ta_Name, sizeof(config.uiFontName) - 1);
										config.uiFontName[sizeof(config.uiFontName) - 1] = '\0';
										config.uiFontSize = uiFont->ta_YSize;
										config.uiFontStyle = uiFont->ta_Style;
										config.uiFontFlags = uiFont->ta_Flags;
										writeConfig();
										FreeAslRequest(fontRequester);
										DoMethod(mainWindowObject, WM_CLOSE, NULL);
										WORD width = screen->Width;
										WORD height = screen->Height;
										LONG displayId = GetVPModeID(&screen->ViewPort);
										CloseScreen(screen);
										screen = OpenScreenTags(NULL,
											SA_Pens, (ULONG)pens,
											SA_DisplayID, displayId,
											SA_Depth, 3,
											SA_Overscan, OSCAN_TEXT,
											SA_AutoScroll, TRUE,
											SA_Width, width,
											SA_Height, height,
											SA_Font, uiFont,
											SA_Colors32, config.colors,
											TAG_DONE);

										SetAttrs(mainWindowObject, WA_CustomScreen, screen, TAG_DONE);
										mainWindow = DoMethod(mainWindowObject, WM_OPEN, NULL);
										GetAttr(WINDOW_SigMask, mainWindowObject, &winSignal);
										signalMask = winSignal;
									}
								}
								break;
							}
						case MENU_ITEM_SPEECH_ENABLED_ID:
							menuItem->Flags ^= CHECKED;
							config.speechEnabled = !config.speechEnabled;
							writeConfig();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_OLD_ID:
							clearSpeechSystemMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.speechSystem = SpeechSystemOld;
							writeConfig();
							closeSpeech();
							initSpeech(config.speechSystem);
							break;
						case MENU_ITEM_SPEECH_SYSTEM_NEW_ID:
							clearSpeechSystemMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.speechSystem = SpeechSystemNew;
							writeConfig();
							closeSpeech();
							initSpeech(config.speechSystem);
							break;
						case MENU_ITEM_MODEL_GPT_4_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.model = GPT_4;
							writeConfig();
							break;
						case MENU_ITEM_MODEL_GPT_4_0314_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.model = GPT_4_0314;
							writeConfig();
							break;
						case MENU_ITEM_MODEL_GPT_4_32K_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.model = GPT_4_32K;
							writeConfig();
							break;
						case MENU_ITEM_MODEL_GPT_4_32K_0314_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.model = GPT_4_32K_0314;
							writeConfig();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.model = GPT_3_5_TURBO;
							writeConfig();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							config.model = GPT_3_5_TURBO_0301;
							writeConfig();
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

/**
 * Write the config to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG writeConfig() {
    BPTR file = Open("PROGDIR:config.json", MODE_NEWFILE);
    if (file == 0) {
        printf("Failed to open the config file\n");
        return RETURN_ERROR;
    }

	struct json_object *configJsonObject = json_object_new_object();
	json_object_object_add(configJsonObject, "speechEnabled", json_object_new_boolean(config.speechEnabled));
	json_object_object_add(configJsonObject, "speechSystem", json_object_new_int(config.speechSystem));
	json_object_object_add(configJsonObject, "model", json_object_new_int(config.model));
	json_object_object_add(configJsonObject, "chatFontName", json_object_new_string(config.chatFontName));
	json_object_object_add(configJsonObject, "chatFontSize", json_object_new_int(config.chatFontSize));
	json_object_object_add(configJsonObject, "chatFontStyle", json_object_new_int(config.chatFontStyle));
	json_object_object_add(configJsonObject, "chatFontFlags", json_object_new_int(config.chatFontFlags));
	json_object_object_add(configJsonObject, "uiFontName", json_object_new_string(config.uiFontName));
	json_object_object_add(configJsonObject, "uiFontSize", json_object_new_int(config.uiFontSize));
	json_object_object_add(configJsonObject, "uiFontStyle", json_object_new_int(config.uiFontStyle));
	json_object_object_add(configJsonObject, "uiFontFlags", json_object_new_int(config.uiFontFlags));
	json_object_object_add(configJsonObject, "openAiApiKey", json_object_new_string(config.openAiApiKey));
	STRPTR configJsonString = (STRPTR)json_object_to_json_string(configJsonObject);

    if (Write(file, configJsonString, strlen(configJsonString)) != strlen(configJsonString)) {
        printf("Failed to write the data to the config file\n");
        Close(file);
		json_object_put(configJsonObject);
        return RETURN_ERROR;
    }

    Close(file);
	json_object_put(configJsonObject);
    return RETURN_OK;
}

/**
 * Read the config from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG readConfig() {
	BPTR file = Open("PROGDIR:config.json", MODE_OLDFILE);
	if (file == 0) {
		// No config exists. Create a new one from defaults
		writeConfig();
		return RETURN_OK;
	}

	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	STRPTR configJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, configJsonString, fileSize) != fileSize) {
		printf("Failed to read the config file\n");
		Close(file);
		FreeVec(configJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *configJsonObject = json_tokener_parse(configJsonString);
	if (configJsonObject == NULL) {
		printf("Failed to parse the config file\n");
		Close(file);
		FreeVec(configJsonString);
		return RETURN_ERROR;
	}

	config.speechEnabled = json_object_get_boolean(json_object_object_get(configJsonObject, "speechEnabled"));
	config.speechSystem = json_object_get_int(json_object_object_get(configJsonObject, "speechSystem"));
	config.model = json_object_get_int(json_object_object_get(configJsonObject, "model"));
	memset(config.chatFontName, 0, sizeof(config.chatFontName));
	strncpy(config.chatFontName, json_object_get_string(json_object_object_get(configJsonObject, "chatFontName")), sizeof(config.chatFontName) - 1);
	config.chatFontSize = json_object_get_int(json_object_object_get(configJsonObject, "chatFontSize"));
	config.chatFontStyle = json_object_get_int(json_object_object_get(configJsonObject, "chatFontStyle"));
	config.chatFontFlags = json_object_get_int(json_object_object_get(configJsonObject, "chatFontFlags"));
	memset(config.uiFontName, 0, sizeof(config.uiFontName));
	strncpy(config.uiFontName, json_object_get_string(json_object_object_get(configJsonObject, "uiFontName")), sizeof(config.uiFontName) - 1);
	config.uiFontSize = json_object_get_int(json_object_object_get(configJsonObject, "uiFontSize"));
	config.uiFontStyle = json_object_get_int(json_object_object_get(configJsonObject, "uiFontStyle"));
	config.uiFontFlags = json_object_get_int(json_object_object_get(configJsonObject, "uiFontFlags"));
	memset(config.openAiApiKey, 0, sizeof(config.openAiApiKey));
	strncpy(config.openAiApiKey, json_object_get_string(json_object_object_get(configJsonObject, "openAiApiKey")), sizeof(config.openAiApiKey) - 1);

	FreeVec(configJsonString);
	json_object_put(configJsonObject);
	return RETURN_OK;
}

/**
 * Shutdown the GUI
**/
void shutdownGUI() {
	freeConversationList();
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