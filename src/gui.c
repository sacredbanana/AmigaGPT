#include <classes/requester.h>
#include <classes/window.h>
#include <dos/dos.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <gadgets/button.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <gadgets/radiobutton.h>
#include <gadgets/scroller.h>
#include <gadgets/string.h>
#include <gadgets/texteditor.h>
#include <graphics/text.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <proto/asl.h>
#include <proto/button.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/radiobutton.h>
#include <proto/requester.h>
#include <proto/scroller.h>
#include <proto/string.h>
#include <proto/texteditor.h>
#include <proto/window.h>
#include <stdbool.h>
#include <stdio.h>
#include "config.h"
#include "external/json-c/json.h"
#include "gui.h"
#include "version.h"
#include "customtexteditor.h"

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
#define NEW_CHAT_BUTTON_WIDTH 50
#define NEW_CHAT_BUTTON_HEIGHT 20
#define DELETE_CHAT_BUTTON_ID 9
#define DELETE_CHAT_BUTTON_WIDTH 50
#define DELETE_CHAT_BUTTON_HEIGHT 20

#define MENU_ITEM_ABOUT_ID 1
#define MENU_ITEM_SPEECH_ACCENT_ID 2
#define MENU_ITEM_QUIT_ID 3
#define MENU_ITEM_SPEECH_ENABLED_ID 4
#define MENU_ITEM_CHAT_FONT_ID 5
#define MENU_ITEM_UI_FONT_ID 6
#define MENU_ITEM_SPEECH_SYSTEM_34_ID 7
#define MENU_ITEM_SPEECH_SYSTEM_37_ID 8
#define MENU_ITEM_CUT_ID 10
#define MENU_ITEM_COPY_ID 11
#define MENU_ITEM_PASTE_ID 12
#define MENU_ITEM_CLEAR_ID 13
#define MENU_ITEM_SELECT_ALL_ID 14
#define MENU_ITEM_MODEL_GPT_4_ID 15
#define MENU_ITEM_MODEL_GPT_4_0314_ID 16
#define MENU_ITEM_MODEL_GPT_4_32K_ID 17
#define MENU_ITEM_MODEL_GPT_4_32K_0314_ID 18
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_ID 19
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID 20
#define MENU_ITEM_MODEL_ID 21
#define MENU_ITEM_SPEECH_SYSTEM_ID 22
#define MENU_ITEM_OPENAI_API_KEY_ID 23

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *AslBase;
static struct Library *WindowBase;
static struct Library *LayoutBase;
static struct Library *ButtonBase;
static struct Library *RadioButtonBase;
struct Library *TextFieldBase;
static struct Library *ScrollerBase;
static struct Library *StringBase;
static struct Library *ListBrowserBase;
static struct Library *RequesterBase;
static struct Window *mainWindow;
static Object *mainWindowObject;
static Object *mainLayout;
static Object *chatLayout;
static Object *chatInputLayout;
static Object *chatOutputLayout;
static Object *conversationsLayout;
static Object *chatButtonsLayout;
static Object *sendMessageButton;
static Object *textInputTextEditor;
static Object *chatOutputTextEditor;
static Object *chatOutputScroller;
static Object *statusBar;
static Object *conversationListBrowser;
static Object *newChatButton;
static Object *deleteChatButton;
static struct Screen *screen;
static BOOL isPublicScreen;
static UWORD pens[] = {~0};
struct MinList *currentConversation;
struct List *conversationList;
static struct TextFont *uiTextFont = NULL;
static struct TextAttr screenFont = {
	.ta_Name = "",
	.ta_YSize = 8,
	.ta_Style = FS_NORMAL,
	.ta_Flags = FPF_DISKFONT | FPF_DESIGNED
};

static struct NewMenu amigaGPTMenu[] = {
	{NM_TITLE, "Project", 0, 0, 0, 0},
	{NM_ITEM, "About", 0, 0, 0, MENU_ITEM_ABOUT_ID},
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
	{NM_ITEM, "Accent", 0, 0, 0, MENU_ITEM_SPEECH_ACCENT_ID},
	{NM_ITEM, "Speech system", 0, 0, 0, MENU_ITEM_SPEECH_SYSTEM_ID},
	{NM_SUB, "Workbench 1.x v34", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_SYSTEM_34_ID},
	{NM_SUB, "Workbench 2.0 v37", 0, CHECKIT, 0, MENU_ITEM_SPEECH_SYSTEM_37_ID},
	{NM_TITLE, "OpenAI", 0, 0, 0, 0},
	{NM_ITEM, "API key", 0, 0, 0, MENU_ITEM_OPENAI_API_KEY_ID},
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
static void refreshModelMenuItems();
static void refreshSpeechMenuItems();
static struct MinList* newConversation();
static void addTextToConversation(struct MinList *conversation, STRPTR text, STRPTR role);
static void addConversationToConversationList(struct List *conversationList, struct MinList *conversation, STRPTR title);
static struct MinList* getConversationFromConversationList(struct List *conversationList, ULONG index);
static void displayConversation(struct MinList *conversation);
static void freeConversation(struct MinList *conversation);
static void freeConversationList();
static void removeConversationFromConversationList(struct List *conversationList, struct MinList *conversation);
static void openChatFontRequester();
static void openUIFontRequester();
static void openAboutWindow();
static void openSpeechAccentRequester();
static void openApiKeyRequester();
static LONG loadConversations();
static LONG saveConversations();
static BOOL copyFile(STRPTR source, STRPTR destination);

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

	if ((RequesterBase = OpenLibrary("requester.class", 47)) == NULL) {
		printf("Could not open requester.class\n");
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

	if (selectScreen() == RETURN_ERROR)
		return RETURN_ERROR;

	conversationList = AllocVec(sizeof(struct List), MEMF_CLEAR);
	NewList(conversationList);
	currentConversation = NULL;
	loadConversations();

	refreshModelMenuItems();
	refreshSpeechMenuItems();

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

	if ((deleteChatButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, DELETE_CHAT_BUTTON_ID,
		GA_WIDTH, DELETE_CHAT_BUTTON_WIDTH,
		GA_HEIGHT, DELETE_CHAT_BUTTON_HEIGHT,
		BUTTON_TextPen, 3,
		BUTTON_Justification, BCJ_CENTER,
		GA_TEXT, (ULONG)"- Delete Chat",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create delete chat button\n");
			return RETURN_ERROR;
	}

	if ((chatButtonsLayout = NewObject(LAYOUT_GetClass(), NULL,
		LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
		LAYOUT_SpaceInner, TRUE,
		LAYOUT_SpaceOuter, TRUE,
		LAYOUT_AddChild, newChatButton,
		LAYOUT_AddChild, deleteChatButton,
		TAG_DONE)) == NULL) {
			printf("Could not create conversations layout\n");
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
		LAYOUT_AddChild, chatButtonsLayout,
		CHILD_WeightedHeight, 10,
		LAYOUT_AddChild, conversationListBrowser,
		CHILD_WeightedWidth, 90,
		TAG_DONE)) == NULL) {
			printf("Could not create conversations layout\n");
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

	struct TextAttr chatTextAttr = {
		.ta_Name = config.chatFontName,
		.ta_YSize = config.chatFontSize,
		.ta_Style = config.chatFontStyle,
		.ta_Flags = config.chatFontFlags
	};

	if ((textInputTextEditor = NewObject(initCustomTextEditorClass(), NULL,
		GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Text, "",
		GA_Width, TEXT_INPUT_TEXT_EDITOR_WIDTH,
		GA_Height, TEXT_INPUT_TEXT_EDITOR_HEIGHT,
		GA_TextAttr, &chatTextAttr,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	struct TagItem chatOutputMap[] = {
		{SCROLLER_Top, GA_TEXTEDITOR_Prop_First},
		{SCROLLER_Total, GA_TEXTEDITOR_Prop_Entries},
		{SCROLLER_Visible, GA_TEXTEDITOR_Prop_Visible},
		{TAG_DONE}
	};

	if ((chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Width, CHAT_OUTPUT_TEXT_EDITOR_WIDTH,
		GA_Height, CHAT_OUTPUT_TEXT_EDITOR_HEIGHT,
		GA_ReadOnly, TRUE,
		GA_TextAttr, &chatTextAttr,
		GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_MIME,
		GA_TEXTEDITOR_ExportHook, GV_TEXTEDITOR_ExportHook_Plain,
		ICA_MAP, chatOutputMap,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	struct TagItem chatOutputScrollerMap[] = {
		{GA_TEXTEDITOR_Prop_First, SCROLLER_Top},
		{GA_TEXTEDITOR_Prop_Entries, SCROLLER_Total},
		{GA_TEXTEDITOR_Prop_Visible, SCROLLER_Visible},
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
		ICA_MAP, chatOutputScrollerMap,
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
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MENUPICK,
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

	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, "/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n/AMIGA \n", TAG_DONE);


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
						{
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
								ASLSM_NegativeText, NULL,
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
										done = TRUE;
									}
								}
							} else {
								// Open in Workbench
								isPublicScreen = TRUE;
								done = TRUE;
							}
							break;
						}
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
		SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, text, TAG_DONE);
		struct MinNode *lastMessage = RemTail(currentConversation);
		FreeVec(lastMessage);
		if (currentConversation == currentConversation->mlh_TailPred) {
			freeConversation(currentConversation);
			currentConversation = NULL;
			DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
		} else {
			displayConversation(currentConversation);
		}
	}
	FreeVec(text);
}

/**
 * Sets the checkbox for the model that is currently selected
**/ 
static void refreshModelMenuItems() {
	struct NewMenu *menu = amigaGPTMenu;
	while (menu->nm_UserData != MENU_ITEM_MODEL_ID) {
		menu++;
	}

	while ((++menu)->nm_Type == NM_SUB) {
		if (strcmp(menu->nm_Label, MODEL_NAMES[config.model]) == 0) {
			menu->nm_Flags |= CHECKED;
		} else {
			menu->nm_Flags &= ~CHECKED;
		}
	}

	SetAttrs(mainWindowObject, WINDOW_NewMenu, amigaGPTMenu, TAG_DONE);
}

/**
 * Sets the checkboxes for the speech options that are currently selected
**/ 
static void refreshSpeechMenuItems() {
	struct NewMenu *menu = amigaGPTMenu;
	while (menu->nm_UserData != MENU_ITEM_SPEECH_ACCENT_ID) {
		menu++;
	}

	if (config.speechEnabled) {
		menu++->nm_Flags |= CHECKED;
	} else {
		menu++->nm_Flags &= ~CHECKED;
	}

	while ((++menu)->nm_Type == NM_SUB) {
		if (strcmp(menu->nm_Label, SPEECH_SYSTEM_NAMES[config.speechSystem]) == 0) {
			menu->nm_Flags |= CHECKED;
		} else {
			menu->nm_Flags &= ~CHECKED;
		}
	}

	SetAttrs(mainWindowObject, WINDOW_NewMenu, amigaGPTMenu, TAG_DONE);
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
	struct MinList *conversation;
	GetListBrowserNodeAttrs(node, LBNA_UserData, &conversation, TAG_END);
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
				displayError("The conversation has exceeded the maximum length.\n\nPlease start a new conversation.");
				SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, TRUE, TAG_DONE);
				return;
			}
			if (strcmp(conversationNode->role, "user") == 0) {
				if (strlen(conversationString) == 0)
					snprintf(conversationString, WRITE_BUFFER_LENGTH - 3, "*%s*", conversationNode->content);
				else
					snprintf(conversationString, WRITE_BUFFER_LENGTH - 5, "%s\n\n*%s*", conversationString, conversationNode->content);
			} else {
				snprintf(conversationString, WRITE_BUFFER_LENGTH - 3, "%s\n\n%s\0", conversationString, conversationNode->content);
			}
    }

	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, conversationString, TAG_DONE);
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, FALSE, TAG_DONE);
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
 * Remove a conversation from the conversation list
 * @param conversationList The conversation list to remove the conversation from
 * @param conversation The conversation to remove from the conversation list
**/ 
static void removeConversationFromConversationList(struct List *conversationList, struct MinList *conversation) {
	struct Node *node = conversationList->lh_Head->ln_Succ;
	while (node->ln_Succ != NULL) {
		struct ConversationNode *conversationNode;
		GetListBrowserNodeAttrs(node, LBNA_UserData, (struct ConversationNode *)&conversationNode, TAG_END);
		if (conversationNode == conversation) {
			SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);		
			Remove(node);
			SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, conversationList, TAG_DONE);
			freeConversation(conversation);
			FreeVec(node);
			return;
		}
		node = node->ln_Succ;
	}
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
				case WMHI_GADGETDOWN:
					switch (result & WMHI_GADGETMASK) {
						case TEXT_INPUT_TEXT_EDITOR_ID:
							if (isChatOutputTextEditorActive)
								ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
							break;
						case CHAT_OUTPUT_TEXT_EDITOR_ID:
							printf("Chat output text editor clicked\n");
							break;
					}
					break;
				case WMHI_GADGETUP:
					switch (result & WMHI_GADGETMASK) {
						case SEND_MESSAGE_BUTTON_ID:
						case TEXT_INPUT_TEXT_EDITOR_ID:
							if (strlen(config.openAiApiKey) > 0) {
								sendMessage();
								saveConversations();
							}
							else
								displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
							break;
						case NEW_CHAT_BUTTON_ID:
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
							break;
						case DELETE_CHAT_BUTTON_ID:
							removeConversationFromConversationList(conversationList, currentConversation);
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
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
							openAboutWindow();
							break;
						case MENU_ITEM_CUT_ID:
						{
							STRPTR result = NULL;
							// if (isTextInputTextEditorActive)
							// 	result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");
							// else if (isChatOutputTextEditorActive)
							// 	result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");

							result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CUT");

							if ((BOOL)result == FALSE)
								printf("Error cutting text\n");
							else if ((BOOL)result == TRUE)
								printf("Text cut\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_COPY_ID:
						{
							STRPTR result = NULL;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "COPY");

							if ((BOOL)result == FALSE)
								printf("Error copying text\n");
							else if ((BOOL)result == TRUE)
								printf("Text copied\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_PASTE_ID:
						{
							STRPTR result = NULL;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "PASTE");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "PASTE");

							if ((BOOL)result == FALSE)
								printf("Error pasting text\n");
							else if ((BOOL)result == TRUE)
								printf("Text pasted\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_SELECT_ALL_ID:
						{
							STRPTR result = NULL;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_MarkText, NULL, 0, 0, 65535, 65535);

							if ((BOOL)result == FALSE)
								printf("Error selecting all text\n");
							else if ((BOOL)result == TRUE)
								printf("All text selected\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_CLEAR_ID:
						{
							STRPTR result = NULL;
							if (isTextInputTextEditorActive)
								result = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CLEAR");
							else if (isChatOutputTextEditorActive)
								result = DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ARexxCmd, NULL, "CLEAR");

							if ((BOOL)result == FALSE)
								printf("Error clearing text\n");
							else if ((BOOL)result == TRUE)
								printf("Text cleared\n");
							printf("%s\n", result);
							FreeVec(result);
							break;
						}
						case MENU_ITEM_QUIT_ID:
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
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_ACCENT_ID:
							openSpeechAccentRequester();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_34_ID:
							closeSpeech();
							if (initSpeech(SPEECH_SYSTEM_34) == RETURN_OK) {
								config.speechSystem = SPEECH_SYSTEM_34;
							} else {
								config.speechSystem = SPEECH_SYSTEM_NONE;
								displayError("Could not initialise speech system v34. Please make sure the translator.library and narrator.device v34 are installed into the program directory.");
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_SYSTEM_37_ID:
							closeSpeech();
							if (initSpeech(SPEECH_SYSTEM_37) == RETURN_OK) {
								config.speechSystem = SPEECH_SYSTEM_37;
							} else {
								config.speechSystem = SPEECH_SYSTEM_NONE;
								displayError("Could not initialise speech system v37. Please make sure the translator.library and narrator.device v37 are installed into the program directory.");
							}
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_OPENAI_API_KEY_ID:
							openApiKeyRequester();
							break;
						case MENU_ITEM_MODEL_GPT_4_ID:
							config.model = GPT_4;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_4_0314_ID:
							config.model = GPT_4_0314;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_4_32K_ID:
							config.model = GPT_4_32K;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_4_32K_0314_ID:
							config.model = GPT_4_32K_0314;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_ID:
							config.model = GPT_3_5_TURBO;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID:
							config.model = GPT_3_5_TURBO_0301;
							writeConfig();
							refreshModelMenuItems();
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

	saveConversations();

	return RETURN_OK;
}

/**
 * Display an error message
 * @param message the message to display
**/ 
void displayError(STRPTR message) {
    DisplayBeep(screen);
    SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Error", TAG_DONE);
    
    STRPTR adjustedMsg = AllocVec(strlen(message) + 200, MEMF_ANY | MEMF_CLEAR);
    STRPTR dest = adjustedMsg;

    ULONG width = 300;
    struct RastPort rp = screen->RastPort;
    struct TextExtent te;
    
    while (*message != '\0') {
        // Find how much of the message fits within half the screen width
		ULONG fits = TextFit(&rp, message, strlen(message), &te, NULL, 1, width, rp.Font->tf_YSize);

        // Find last space in the range that fits
        STRPTR lastSpace = message + fits;
        while (lastSpace > message && *lastSpace != ' ') {
            lastSpace--;
        }

        // If we found a space, replace it with a newline and copy the line to the output
        if (*lastSpace == ' ' && lastSpace != message) {
            memcpy(dest, message, lastSpace - message);
            dest[lastSpace - message] = '\n';
            dest += lastSpace - message + 1;
            message = lastSpace + 1;
        } else {
            // If we didn't find a space, just copy the part that fits
            memcpy(dest, message, fits);
            dest += fits;
            message += fits;
            // If we have not reached the end of the message, add a newline
            if (*message != '\0') {
                *dest++ = '\n';
            }
        }
    }

	*dest = '\0';
    
    struct EasyStruct errorRequester = {
        sizeof(struct EasyStruct),
        0,
        "Error",
        adjustedMsg,
        "OK"
    };
    EasyRequest(mainWindow, &errorRequester, NULL, NULL);

	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, FALSE, TAG_DONE);

    FreeVec(adjustedMsg);
}

/**
 * Display an error message about a disk error
 * @param message the message to display
 * @param error the error code returned by IOErr()
**/ 
void displayDiskError(STRPTR message, LONG error) {
	const UBYTE ERROR_BUFFER_LENGTH = 255;
	const UBYTE FINAL_MESSAGE_LENGTH = strlen(message) + ERROR_BUFFER_LENGTH;
	STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
	STRPTR finalMessage = AllocVec(FINAL_MESSAGE_LENGTH, MEMF_ANY | MEMF_CLEAR);
	Fault(error, NULL, errorMessage, ERROR_BUFFER_LENGTH);
	snprintf(finalMessage, FINAL_MESSAGE_LENGTH - 3, "%s\n\n%s", message, error);
	displayError(finalMessage);
	FreeVec(errorMessage);
	FreeVec(finalMessage);
}

/**
 * Opens the About window
**/
static void openAboutWindow() {
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
}

/**
 * Opens a requester for the user to select the font for the chat window 
**/
static void openChatFontRequester() {
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
			SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
			SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TextAttr, chatFont, TAG_DONE);
		}
		FreeAslRequest(fontRequester);
	}
}

/**
 * Opens a requester for the user to select the font for the UI
**/
static void openUIFontRequester() {
	struct FontRequester *fontRequester;
	GetAttr(WINDOW_Window, mainWindowObject, &mainWindow);
	if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
		struct TextAttr *uiFont;
		if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
			uiFont = &fontRequester->fo_Attr;
			memset(config.uiFontName, 0, sizeof(config.uiFontName));
			strncpy(config.uiFontName, uiFont->ta_Name, sizeof(config.uiFontName) - 1);
			config.uiFontName[sizeof(config.uiFontName) - 1] = '\0';
			config.uiFontSize = uiFont->ta_YSize;
			config.uiFontStyle = uiFont->ta_Style;
			config.uiFontFlags = uiFont->ta_Flags;										
			writeConfig();
			if (!isPublicScreen) {
				if (uiTextFont)
					CloseFont(uiTextFont);

				DoMethod(mainWindowObject, WM_CLOSE, NULL);
				uiTextFont = OpenFont(uiFont);
				SetFont(&(screen->RastPort), uiTextFont);
				SetFont(mainWindow->RPort, uiTextFont);
				RemakeDisplay();
				RethinkDisplay();
				mainWindow = DoMethod(mainWindowObject, WM_OPEN, NULL);
			}
			SetGadgetAttrs(newChatButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			SetGadgetAttrs(statusBar, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
		}
		FreeAslRequest(fontRequester);
	}
}


/**
 * Opens a requester for the user to select accent for the speech
**/
static void openSpeechAccentRequester() {
	struct FileRequester *fileRequester = (struct FileRequester *)AllocAslRequestTags(
									ASL_FileRequest,
									ASLFR_Window, mainWindow,
									ASLFR_PopToFront, TRUE,
									ASLFR_Activate, TRUE,
									ASLFR_DrawersOnly, FALSE,
									ASLFR_InitialDrawer, "LOCALE:accents",
									ASLFR_DoPatterns, TRUE,
									ASLFR_InitialPattern, "#?.accent",
									TAG_DONE);
								
	if (fileRequester) {
		if (AslRequestTags(fileRequester, TAG_DONE)) {
			strncpy(config.speechAccent, fileRequester->fr_File, sizeof(config.speechAccent) - 1);
			config.speechAccent[sizeof(config.speechAccent) - 1] = '\0';
			writeConfig();
		}
		FreeAslRequest(fileRequester);
	}
}

/**
 * Opens a requester for the user to enter their OpenAI API key
**/
static void openApiKeyRequester() {
	UBYTE buffer[64];
	strncpy(buffer, config.openAiApiKey, sizeof(buffer) - 1);
	Object *openApiKeyRequester = NewObject(REQUESTER_GetClass(), NULL,
		REQ_Type, REQTYPE_STRING,
		REQ_TitleText, "Enter your OpenAI API key",
		REQ_BodyText, "Please type or paste your OpenAI API key here",
		REQ_GadgetText, "OK|CANCEL",
		REQS_AllowEmpty, FALSE,
		REQS_Buffer, buffer,
		REQS_MaxChars, sizeof(buffer) - 1,
		REQS_Invisible, FALSE,
		REQ_ForceFocus, TRUE,
		TAG_DONE);

	if (openApiKeyRequester) {
		ULONG result = OpenRequester(openApiKeyRequester, mainWindow);
		if (result == 1) {
			strncpy(config.openAiApiKey, buffer, sizeof(config.openAiApiKey) - 1);
			writeConfig();
		}
		DisposeObject(openApiKeyRequester);
	}
}

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG saveConversations() {
    BPTR file = Open("PROGDIR:chat-history.json", MODE_NEWFILE);
    if (file == 0) {
        displayDiskError("Failed to create message history file. Conversation history will not be saved.", IoErr());
        return RETURN_ERROR;
    }

	struct json_object *conversationsJsonArray = json_object_new_array();
	struct json_object *conversationJsonObject;
	struct Node *conversationListNode = conversationList->lh_Head;
	while (conversationListNode->ln_Succ) {
		conversationJsonObject = json_object_new_object();
		struct MinList *conversation;
		GetListBrowserNodeAttrs(conversationListNode, LBNA_UserData, &conversation, TAG_END);
		STRPTR conversationTitle;
		GetListBrowserNodeAttrs(conversationListNode, LBNCA_Text, &conversationTitle, TAG_END);
		json_object_object_add(conversationJsonObject, "name", json_object_new_string(conversationTitle));
		struct json_object *messagesJsonArray = json_object_new_array();
		struct ConversationNode *conversationNode;
		for (conversationNode = (struct ConversationNode *)conversation->mlh_Head; 
         conversationNode->node.mln_Succ != NULL; 
         conversationNode = (struct ConversationNode *)conversationNode->node.mln_Succ) {
			struct json_object *messageJsonObject = json_object_new_object();
			json_object_object_add(messageJsonObject, "role", json_object_new_string(conversationNode->role));
			json_object_object_add(messageJsonObject, "content", json_object_new_string(conversationNode->content));
			json_object_array_add(messagesJsonArray, messageJsonObject);
    	}
		json_object_object_add(conversationJsonObject, "messages", messagesJsonArray);
		json_object_array_add(conversationsJsonArray, conversationJsonObject);
		conversationListNode = conversationListNode->ln_Succ;
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
 * Load the conversations from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG loadConversations() {
	BPTR file = Open("PROGDIR:chat-history.json", MODE_OLDFILE);
	if (file == 0) {
		return RETURN_OK;
	}

	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	STRPTR conversationsJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, conversationsJsonString, fileSize) != fileSize) {
		displayDiskError("Failed to read from message history file. Conversation history will not be loaded", IoErr());
		Close(file);
		FreeVec(conversationsJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *conversationsJsonArray = json_tokener_parse(conversationsJsonString);
	if (conversationsJsonArray == NULL) {
		if (Rename("PROGDIR:chat-history.json", "PROGDIR:chat-history.json.bak")) {
			displayDiskError("Failed to parse chat history. Malformed JSON. The chat-history.json file is probably corrupted. Conversation history will not be loaded. A backup of the chat-history.json file has been created as chat-history.json.bak", IoErr());
		} else if (copyFile("PROGDIR:chat-history.json", "RAM:chat-history.json")) {
			displayError("Failed to parse chat history. Malformed JSON. The chat-history.json file is probably corrupted. Conversation history will not be loaded. There was an error writing a backup of the chat history to disk but a copy has been saved to RAM:chat-history.json.bak");
			if (!DeleteFile("PROGDIR:chat-history.json")) {
				displayDiskError("Failed to delete chat-history.json. Please delete this file manually.", IoErr());
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

		struct MinList *conversation = newConversation();
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
		addConversationToConversationList(conversationList, conversation, conversationName);

	}

	json_object_put(conversationsJsonArray);
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
		displayDiskError(errorMessage, IoErr());
		FreeVec(buffer);
		FreeVec(errorMessage);
        return FALSE;
    }

    if (!(dstFile = Open(destination, MODE_NEWFILE))) {
		snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error creating %s for copy", destination);
		displayDiskError(errorMessage, IoErr());
		FreeVec(buffer);
		FreeVec(errorMessage);
        Close(srcFile);
        return FALSE;
    }

    do {
        bytesRead = Read(srcFile, buffer, sizeof(buffer));

        if (bytesRead > 0) {
            bytesWritten = Write(dstFile, buffer, bytesRead);

            if (bytesWritten != bytesRead) {
				snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
				displayDiskError(errorMessage, IoErr());
				FreeVec(buffer);
				FreeVec(errorMessage);
                Close(srcFile);
                Close(dstFile);
                return FALSE;
            }
        }
        else if (bytesRead < 0) {
			snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
			displayDiskError(errorMessage, IoErr());
			FreeVec(buffer);
			FreeVec(errorMessage);
			Close(srcFile);
			Close(dstFile);
            return FALSE;
        }
    } while (bytesRead > 0);

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
	freeConversationList();
	if (mainWindowObject) {
		DisposeObject(mainWindowObject);
	}
	if (isPublicScreen) {
		UnlockPubScreen(NULL, screen);
	} else {
		CloseScreen(screen);
	}
	if (uiTextFont)
		CloseFont(uiTextFont);

	closeGUILibraries();
}