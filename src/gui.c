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
#define MENU_ITEM_FONT_ID 5
#define MENU_ITEM_SPEECH_SYSTEM_OLD_ID 6
#define MENU_ITEM_SPEECH_SYSTEM_NEW_ID 7
#define MENU_ITEM_CUT_ID 8
#define MENU_ITEM_COPY_ID 9
#define MENU_ITEM_PASTE_ID 10
#define MENU_ITEM_CLEAR_ID 11
#define MENU_ITEM_SELECT_ALL_ID 12
#define MENU_ITEM_MODEL_GPT_4_ID 13
#define MENU_ITEM_MODEL_GPT_4_0314_ID 14
#define MENU_ITEM_MODEL_GPT_4_32K_ID 15
#define MENU_ITEM_MODEL_GPT_4_32K_0314_ID 16
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_ID 17
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID 18
#define MENU_ITEM_MODEL_ID 22
#define MENU_ITEM_SPEECH_SYSTEM_ID 23

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
static BOOL isSpeechEnabled;
static enum SpeechSystem speechSystem;
static enum Model model;
struct MinList *currentConversation;
struct List *conversationList;

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
	{NM_ITEM, "Font", 0, 0, 0, MENU_ITEM_FONT_ID},
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
static void addTextToConversation(struct MinList *conversation, UBYTE *text, UBYTE *role);
static void addConversationToConversationList(struct List *conversationList, struct MinList *conversation, UBYTE *title);
static void freeConversation(struct MinList *conversation);
static void freeConversationList();

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

LONG initVideo() {
	if (openGUILibraries() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

	if (selectScreen() == RETURN_ERROR) {
		return RETURN_ERROR;
	}

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
		GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_Plain,
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

	if ((textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_Text, (ULONG)"",
		GA_Width, TEXT_INPUT_TEXT_EDITOR_WIDTH,
		GA_Height, TEXT_INPUT_TEXT_EDITOR_HEIGHT,
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

static LONG selectScreen() {
	Object *screenSelectRadioButton, *selectScreenOkButton, *screenSelectLayout, *screenSelectWindowObject;
	struct Window *screenSelectWindow;
	struct ScreenModeRequester *screenModeRequester;
	screen = LockPubScreen("Workbench");

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
					TAG_DONE)) == NULL) {
						printf("Could not open screen\n");
						return RETURN_ERROR;
				}

				SetRGB4(&(screen->ViewPort), 0, 0x0, 0x1, 0x5);
				SetRGB4(&(screen->ViewPort), 1, 0xA, 0x0, 0x0);
				SetRGB4(&(screen->ViewPort), 2, 0x0, 0x0, 0x3);
				SetRGB4(&(screen->ViewPort), 3, 0xB, 0xF, 0x2);
				SetRGB4(&(screen->ViewPort), 4, 0xF, 0xF, 0x0);
			}
		}
	} else {
		// Open in Workbench
		isPublicScreen = TRUE;
	}

	DoMethod(screenSelectWindowObject, WM_CLOSE);

	return RETURN_OK;
}

// Sends a message to the OpenAI API and displays the response and speaks it if speech is anabled
static void sendMessage() {
	BOOL isNewConversation = FALSE;
	if (currentConversation == NULL) {
		isNewConversation = TRUE;
		currentConversation = newConversation();
	}
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, TRUE, TAG_DONE);
	SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Sending", TAG_DONE);

	UBYTE *text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	addTextToConversation(currentConversation, text, "user");
	DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, text, GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n===============================\n\n", GV_TEXTEDITOR_InsertText_Bottom);
	DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);

	UBYTE *response = postMessageToOpenAI(currentConversation, model);
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_DISABLED, FALSE, TAG_DONE);
	if (response != NULL) {
		addTextToConversation(currentConversation, response, "assistant");
		SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
		SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TEXTEDITOR_Pen, 0, TAG_DONE);
		DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, response, GV_TEXTEDITOR_InsertText_Bottom);
		DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n\n===============================\n\n", GV_TEXTEDITOR_InsertText_Bottom);
		if (isSpeechEnabled)
			speakText(response);
		FreeVec(response);
		if (isNewConversation) {
			struct Node *conversationListNode = conversationList->lh_Head->ln_Succ;
			SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Generating conversation title", TAG_DONE);
			addTextToConversation(currentConversation, "generate a short title for this conversation and don't enclose the title in quotes or prefix the response with anything", "user");
			response = postMessageToOpenAI(currentConversation, model);
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

// Clear all the checkboxes for all the models in rhe menu
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

// Clear all the checkboxes for all the speech systems in rhe menu
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

// Create a new conversation
static struct MinList* newConversation() {
	struct MinList *conversation = AllocVec(sizeof(struct MinList), MEMF_CLEAR);
	NewMinList(conversation);
	return conversation;
}

// Add a block of text to the conversation list
static void addTextToConversation(struct MinList *conversation, UBYTE *text, UBYTE *role) {
	struct ConversationNode *conversationNode = AllocVec(sizeof(struct ConversationNode), MEMF_CLEAR);
	if (conversationNode == NULL) {
		printf("Failed to allocate memory for conversation node\n");
		return;
	}
	strcpy(conversationNode->role, role);
	strcpy(conversationNode->content, text);
	AddTail(conversation, (struct Node *)conversationNode);
}

// Add a conversation to the conversation list
static void addConversationToConversationList(struct List *conversationList, struct MinList *conversation, UBYTE *title) {
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

// Free the conversation
static void freeConversation(struct MinList *conversation) {
	struct ConversationNode *conversationNode;
	while ((conversationNode = (struct ConversationNode *)RemHead(conversation)) != NULL) {
		FreeVec(conversationNode);
	}
	FreeVec(conversation);
}

// Free the conversation list
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

// The main loop of the GUI
LONG startGUIRunLoop() {
	struct FontRequester *fontRequester;
	struct TextAttr *currentFont;
    ULONG signalMask, winSignal, signals, result;
	BOOL done = FALSE;
    WORD code;

	isSpeechEnabled = TRUE;
	speechSystem = SpeechSystemOld;
	model = GPT_3_5_TURBO;

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
						case MENU_ITEM_FONT_ID:
							if (fontRequester = (struct FontRequester *)AllocAslRequestTags(ASL_FontRequest, TAG_DONE)) {
								if (AslRequestTags(fontRequester, ASLFO_Window, (ULONG)mainWindow, TAG_DONE)) {
									currentFont = &fontRequester->fo_Attr;
								}
								FreeAslRequest(fontRequester);
								SetGadgetAttrs(textInputTextEditor, mainWindow, NULL, GA_TextAttr, currentFont, TAG_DONE);
								SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TextAttr, currentFont, TAG_DONE);
							}
							break;
						case MENU_ITEM_SPEECH_ENABLED_ID:
							menuItem->Flags ^= CHECKED;
							isSpeechEnabled = !isSpeechEnabled;
							break;
						case MENU_ITEM_SPEECH_SYSTEM_OLD_ID:
							clearSpeechSystemMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							speechSystem = SpeechSystemOld;
							closeSpeech();
							initSpeech(speechSystem);
							break;
						case MENU_ITEM_SPEECH_SYSTEM_NEW_ID:
							clearSpeechSystemMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							speechSystem = SpeechSystemNew;
							closeSpeech();
							initSpeech(speechSystem);
							break;
						case MENU_ITEM_MODEL_GPT_4_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							model = GPT_4;
							break;
						case MENU_ITEM_MODEL_GPT_4_0314_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							model = GPT_4_0314;
							break;
						case MENU_ITEM_MODEL_GPT_4_32K_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							model = GPT_4_32K;
							break;
						case MENU_ITEM_MODEL_GPT_4_32K_0314_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							model = GPT_4_32K_0314;
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							model = GPT_3_5_TURBO;
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID:
							clearModelMenuItems(menuStrip);
							menuItem = ItemAddress(menuStrip, code);
							menuItem->Flags |= CHECKED;
							model = GPT_3_5_TURBO_0301;
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