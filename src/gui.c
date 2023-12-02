#include <classes/requester.h>
#include <classes/window.h>
#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/execbase.h>
#include <exec/lists.h>
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
#include <libraries/amigaguide.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <proto/amigaguide.h>
#include <proto/asl.h>
#include <proto/button.h>
#include <proto/dos.h>
#include <proto/exec.h>
#ifdef __AMIGAOS3__
#include <proto/gadtools.h>
#endif
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/radiobutton.h>
#include <proto/requester.h>
#include <proto/scroller.h>
#include <proto/string.h>
#include <proto/texteditor.h>
#include <proto/utility.h>
#include <proto/window.h>
#include <stdio.h>
#include <utility/utility.h>
#include "config.h"
#include "external/json-c/json.h"
#include "gui.h"
#include "version.h"

#define HELP_KEY 0x5F
#define SCREEN_SELECT_RADIO_BUTTON_ID 0
#define SCREEN_SELECT_OK_BUTTON_ID 1
#define SEND_MESSAGE_BUTTON_ID 2
#define TEXT_INPUT_TEXT_EDITOR_ID 3
#define CHAT_OUTPUT_TEXT_EDITOR_ID 4
#define CHAT_OUTPUT_SCROLLER_ID 5
#define STATUS_BAR_ID 6
#define CONVERSATION_LIST_BROWSER_ID 7
#define NEW_CHAT_BUTTON_ID 8
#define DELETE_CHAT_BUTTON_ID 9

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
#define MENU_ITEM_MODEL_GPT_4_0613_ID 17
#define MENU_ITEM_MODEL_GPT_4_1106_PREVIEW_ID 18
#define MENU_ITEM_MODEL_GPT_4_32K_ID 19
#define MENU_ITEM_MODEL_GPT_4_32K_0314_ID 20
#define MENU_ITEM_MODEL_GPT_4_32K_0613_ID 21
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_ID 22
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID 23
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_0613_ID 24
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_1106_ID 25
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_16K_ID 26
#define MENU_ITEM_MODEL_GPT_3_5_TURBO_16K_0613_ID 27
#define MENU_ITEM_MODEL_ID 28
#define MENU_ITEM_SPEECH_SYSTEM_ID 29
#define MENU_ITEM_OPENAI_API_KEY_ID 30
#define MENU_ITEM_VIEW_DOCUMENTATION_ID 31
#define MENU_ITEM_VOICE_ID 32
#define MENU_ITEM_SPEECH_VOICE_AWB_ID 33
#define MENU_ITEM_SPEECH_VOICE_KAL_ID 34
#define MENU_ITEM_SPEECH_VOICE_KAL16_ID 35
#define MENU_ITEM_SPEECH_VOICE_RMS_ID 36
#define MENU_ITEM_SPEECH_VOICE_SLT_ID 37

#ifdef __AMIGAOS4__
#define IntuitionBase Library
#define GfxBase Library
struct IntuitionIFace *IIntuition;
struct AslIFace *IAsl;
struct GraphicsIFace *IGraphics;
struct AmigaGuideIFace *IAmigaGuide;
struct LayoutIFace *ILayout;
struct ButtonIFace *IButton;
struct RadioButtonIFace *IRadioButton;
struct TextEditorIFace *ITextEditor;
struct ScrollerIFace *IScroller;
struct StringIFace *IString;
struct ListBrowserIFace *IListBrowser;
struct RequesterIFace *IRequester;
struct WindowIFace *IWindow;
extern struct UtilityIFace *IUtility;
struct Library * TextEditorBase;
#else
struct Library *TextFieldBase;
#endif

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *AmigaGuideBase;
struct Library *AslBase;
struct Library *WindowBase;
struct Library *LayoutBase;
struct Library *ButtonBase;
struct Library *RadioButtonBase;
struct Library *ScrollerBase;
struct Library *StringBase;
struct Library *ListBrowserBase;
struct Library *RequesterBase;
struct Library *GadToolsBase;
struct Window *mainWindow;
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
static LONG sendMessageButtonPen;
static LONG newChatButtonPen;
static LONG deleteChatButtonPen;
struct MinList *currentConversation;
struct List *conversationList;
static struct TextFont *uiTextFont = NULL;
static struct TextAttr screenFont = {
	.ta_Name = "",
	.ta_YSize = 8,
	.ta_Style = FS_NORMAL,
	.ta_Flags = FPF_DISKFONT | FPF_DESIGNED
};
static struct TextAttr chatTextAttr = {0};
static struct TextAttr uiTextAttr = {0};
static struct Menu *menu;
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
	#ifdef __AMIGAOS3__
	{NM_ITEM, "Accent", 0, 0, 0, MENU_ITEM_SPEECH_ACCENT_ID},
	{NM_ITEM, "Speech system", 0, 0, 0, MENU_ITEM_SPEECH_SYSTEM_ID},
	{NM_SUB, "Workbench 1.x v34", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_SYSTEM_34_ID},
	{NM_SUB, "Workbench 2.0 v37", 0, CHECKIT, 0, MENU_ITEM_SPEECH_SYSTEM_37_ID},
	#else
	{NM_ITEM, "Voice", 0, 0, 0, MENU_ITEM_VOICE_ID},
	{NM_SUB, "kal (fast)", 0, CHECKIT, 0, MENU_ITEM_SPEECH_VOICE_KAL_ID},
	{NM_SUB, "kal16 (fast)", 0, CHECKIT, 0, MENU_ITEM_SPEECH_VOICE_KAL16_ID},
	{NM_SUB, "awb (slow)", 0, CHECKIT|CHECKED, 0, MENU_ITEM_SPEECH_VOICE_AWB_ID},
	{NM_SUB, "rms (slow)", 0, CHECKIT, 0, MENU_ITEM_SPEECH_VOICE_RMS_ID},
	{NM_SUB, "slt (slow)", 0, CHECKIT, 0, MENU_ITEM_SPEECH_VOICE_SLT_ID},
	#endif
	{NM_TITLE, "OpenAI", 0, 0, 0, 0},
	{NM_ITEM, "API key", 0, 0, 0, MENU_ITEM_OPENAI_API_KEY_ID},
	{NM_ITEM, "Model", 0, 0, 0, MENU_ITEM_MODEL_ID},
	{NM_SUB, "gpt-4", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_ID},
	{NM_SUB, "gpt-4-0314", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_0314_ID},
	{NM_SUB, "gpt-4-0613", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_0613_ID},
	{NM_SUB, "gpt-4-1106-preview", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_1106_PREVIEW_ID},
	{NM_SUB, "gpt-4-32k", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_32K_ID},
	{NM_SUB, "gpt-4-32k-0314", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_32K_0314_ID},
	{NM_SUB, "gpt-4-32k-0613", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_4_32K_0613_ID},
	{NM_SUB, "gpt-3.5-turbo", 0, CHECKIT|CHECKED, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_ID},
	{NM_SUB, "gpt-3.5-turbo-0301", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_0301_ID},
	{NM_SUB, "gpt-3.5-turbo-0613", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_0613_ID},
	{NM_SUB, "gpt-3.5-turbo-1106", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_1106_ID},
	{NM_SUB, "gpt-3.5-turbo-16k", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_16K_ID},
	{NM_SUB, "gpt-3.5-turbo-16k-0613", 0, CHECKIT, 0, MENU_ITEM_MODEL_GPT_3_5_TURBO_16K_0613_ID},
	{NM_TITLE, "Help", 0, 0, 0, 0},
	{NM_ITEM, "View Documentation", 0, 0, 0, MENU_ITEM_VIEW_DOCUMENTATION_ID},
	{NM_END, NULL, 0, 0, 0, 0}
};
struct Hook idcmpHook;
struct MsgPort *appPort;
ULONG activeTextEditorGadgetID;

static STRPTR getMessageContentFromJson(struct json_object *json, BOOL stream);
static void formatText(STRPTR unformattedText);
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
static STRPTR ISO8859_1ToUTF8(CONST_STRPTR iso8859_1String);
static STRPTR UTF8ToISO8859_1(CONST_STRPTR utf8String);
static BOOL copyFile(STRPTR source, STRPTR destination);
static void openDocumentation();
static void updateMenu();
#ifdef __AMIGAOS4__
static uint32 processIDCMP(struct Hook *hook, struct Window *window, struct IntuiMessage *message);
#else
static void __SAVE_DS__ __ASM__ processIDCMP(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message));
#endif

#ifdef __AMIGAOS4__
static uint32 processIDCMP(struct Hook *hook, struct Window *window, struct IntuiMessage *message) {
#else
static void __SAVE_DS__ __ASM__ processIDCMP(__REG__ (a0, struct Hook *hook), __REG__ (a2, struct Window *window), __REG__ (a1, struct IntuiMessage *message)) {
#endif
	switch (message->Class) {
		case IDCMP_IDCMPUPDATE:
		{
			struct TagItem *tagList = message->IAddress;
			struct TagItem *gadgetID = FindTagItem(GA_ID, tagList);
			switch (gadgetID->ti_Data) {
				case TEXT_INPUT_TEXT_EDITOR_ID:
					activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
					break;					
				case CHAT_OUTPUT_SCROLLER_ID:
					gadgetID->ti_Tag = TAG_IGNORE;
					ULONG top;
					GetAttr(SCROLLER_Top, chatOutputScroller, &top);
					SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL,
					GA_TEXTEDITOR_Prop_First, top,
					TAG_DONE);
					break;
				case CHAT_OUTPUT_TEXT_EDITOR_ID:
					activeTextEditorGadgetID = CHAT_OUTPUT_TEXT_EDITOR_ID;
					gadgetID->ti_Tag = TAG_IGNORE;
					ULONG entries;
					GetAttr(GA_TEXTEDITOR_Prop_Entries, chatOutputTextEditor, &entries);
					ULONG first;
					GetAttr(GA_TEXTEDITOR_Prop_First, chatOutputTextEditor, &first);
					ULONG deltaFactor;
					GetAttr(GA_TEXTEDITOR_Prop_DeltaFactor, chatOutputTextEditor, &deltaFactor);
					ULONG visible;
					GetAttr(GA_TEXTEDITOR_Prop_Visible, chatOutputTextEditor, &visible);
					SetGadgetAttrs(chatOutputScroller, mainWindow, NULL,
					SCROLLER_Total, entries,
					SCROLLER_Top, first,
					SCROLLER_ArrowDelta, deltaFactor,
					SCROLLER_Visible, visible,
					TAG_DONE);
					RefreshGadgets(chatOutputScroller, mainWindow, NULL);
					break;
				}
			break;
		}
		default:
			printf("Unknown message class: %lx\n", message->Class);
			break;
	}
	#ifdef __AMIGAOS3__
	return;
	#else
	return WHOOKRSLT_IGNORE;
	#endif
}

/**
 * Open the libraries needed for the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG openGUILibraries() {
	#ifdef __AMIGAOS3__
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

	#ifdef __AMIGAOS3__
	if ((WindowBase = OpenLibrary("window.class", 44)) == NULL) {
		printf("Could not open window.class\n");
		return RETURN_ERROR;
	}
	#else
	if ((WindowBase = OpenLibrary("window.class", 50)) == NULL) {
		printf("Could not open window.class\n");
		return RETURN_ERROR;
	}
	if ((IWindow = (struct WindowIFace *)GetInterface(WindowBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for window.class\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((LayoutBase = OpenLibrary("gadgets/layout.gadget", 44)) == NULL) {
		printf("Could not open layout.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((LayoutBase = OpenLibrary("gadgets/layout.gadget", 50)) == NULL) {
		printf("Could not open layout.gadget\n");
		return RETURN_ERROR;
	}
	if ((ILayout = (struct LayoutIFace *)GetInterface(LayoutBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for layout.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((ButtonBase = OpenLibrary("gadgets/button.gadget", 44)) == NULL) {
		printf("Could not open button.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((ButtonBase = OpenLibrary("gadgets/button.gadget", 50)) == NULL) {
		printf("Could not open button.gadget\n");
		return RETURN_ERROR;
	}
	if ((IButton = (struct ButtonIFace *)GetInterface(ButtonBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for button.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((RadioButtonBase = OpenLibrary("gadgets/radiobutton.gadget", 44)) == NULL) {
		printf("Could not open radiobutton.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((RadioButtonBase = OpenLibrary("gadgets/radiobutton.gadget", 50)) == NULL) {
		printf("Could not open radiobutton.gadget\n");
		return RETURN_ERROR;
	}
	if ((IRadioButton = (struct RadioButtonIFace *)GetInterface(RadioButtonBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for radiobutton.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((TextFieldBase = OpenLibrary("gadgets/texteditor.gadget", 15)) == NULL) {
		printf("Could not open texteditor.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((TextEditorBase = OpenLibrary("gadgets/texteditor.gadget", 50)) == NULL) {
		printf("Could not open texteditor.gadget\n");
		return RETURN_ERROR;
	}
	if ((ITextEditor = (struct TextEditorIFace *)GetInterface(TextEditorBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for texteditor.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((ScrollerBase = OpenLibrary("gadgets/scroller.gadget", 44)) == NULL) {
		printf("Could not open scroller.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((ScrollerBase = OpenLibrary("gadgets/scroller.gadget", 50)) == NULL) {
		printf("Could not open scroller.gadget\n");
		return RETURN_ERROR;
	}
	if ((IScroller = (struct ScrollerIFace *)GetInterface(ScrollerBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for scroller.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((StringBase = OpenLibrary("gadgets/string.gadget", 44)) == NULL) {
		printf("Could not open string.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((StringBase = OpenLibrary("gadgets/string.gadget", 50)) == NULL) {
		printf("Could not open string.gadget\n");
		return RETURN_ERROR;
	}
	if ((IString = (struct StringIFace *)GetInterface(StringBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for string.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((ListBrowserBase = OpenLibrary("gadgets/listbrowser.gadget", 44)) == NULL) {
		printf("Could not open listbrowser.gadget\n");
		return RETURN_ERROR;
	}
	#else
	if ((ListBrowserBase = OpenLibrary("gadgets/listbrowser.gadget", 50)) == NULL) {
		printf("Could not open listbrowser.gadget\n");
		return RETURN_ERROR;
	}
	if ((IListBrowser = (struct ListBrowserIFace *)GetInterface(ListBrowserBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for listbrowser.gadget\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((RequesterBase = OpenLibrary("requester.class", 42)) == NULL) {
		printf("Could not open requester.class\n");
		return RETURN_ERROR;
	}
	#else
	if ((RequesterBase = OpenLibrary("requester.class", 50)) == NULL) {
		printf("Could not open requester.class\n");
		return RETURN_ERROR;
	}
	if ((IRequester = (struct RequesterIFace *)GetInterface(RequesterBase, "main", 1, NULL)) == NULL) {
		printf("Could not get interface for requester.class\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
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

	#ifdef __AMIGAOS3__
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

	#ifdef __AMIGAOS3__
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

	#ifdef __AMIGAOS3__
	if ((GadToolsBase = OpenLibrary("gadtools.library", 40)) == NULL) {
		printf( "Could not open gadtools.library\n");
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
	DropInterface((struct Interface *)IWindow);
	DropInterface((struct Interface *)ILayout);
	DropInterface((struct Interface *)IButton);
	DropInterface((struct Interface *)ITextEditor);
	DropInterface((struct Interface *)IRadioButton);
	DropInterface((struct Interface *)IString);
	DropInterface((struct Interface *)IListBrowser);
	DropInterface((struct Interface *)IScroller);
	DropInterface((struct Interface *)IRequester);
	CloseLibrary(TextEditorBase);
	#else
	CloseLibrary(TextFieldBase);
	CloseLibrary(GadToolsBase);
	#endif

	CloseLibrary(IntuitionBase);
	CloseLibrary(GfxBase);
	CloseLibrary(AslBase);
	CloseLibrary(AmigaGuideBase);
	CloseLibrary(WindowBase);
	CloseLibrary(LayoutBase);
	CloseLibrary(ButtonBase);
	CloseLibrary(RadioButtonBase);
	CloseLibrary(StringBase);
	CloseLibrary(ListBrowserBase);
	CloseLibrary(ScrollerBase);
	CloseLibrary(RequesterBase);
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

	uiTextAttr.ta_Name = config.uiFontName;
	uiTextAttr.ta_YSize = config.uiFontSize;
	uiTextAttr.ta_Style = config.uiFontStyle;
	uiTextAttr.ta_Flags = config.uiFontFlags;
	chatTextAttr.ta_Name = config.chatFontName;
	chatTextAttr.ta_YSize = config.chatFontSize;
	chatTextAttr.ta_Style = config.chatFontStyle;
	chatTextAttr.ta_Flags = config.chatFontFlags;

	sendMessageButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0x00000000, 0xFFFFFFFF, OBP_Precision, PRECISION_GUI, TAG_DONE) : 8;

	if ((sendMessageButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, SEND_MESSAGE_BUTTON_ID,
		BUTTON_TextPen, sendMessageButtonPen,
		GA_TextAttr, &uiTextAttr,
		BUTTON_Justification, BCJ_CENTER,
		GA_Text, (ULONG)"Send",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create send message button\n");
			return RETURN_ERROR;
	}

	newChatButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0x00000000, 0xFFFFFFFF, 0x00000000, OBP_Precision, PRECISION_GUI, TAG_DONE) : 9;

	if ((newChatButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, NEW_CHAT_BUTTON_ID,
		BUTTON_TextPen, newChatButtonPen,
		GA_TextAttr, &uiTextAttr,
		BUTTON_Justification, BCJ_CENTER,
		GA_Text, (ULONG)"+ New Chat",
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create new chat button\n");
			return RETURN_ERROR;
	}

	deleteChatButtonPen = isPublicScreen ? ObtainBestPen(screen->ViewPort.ColorMap, 0xFFFFFFFF, 0x00000000, 0x00000000, OBP_Precision, PRECISION_GUI, TAG_DONE) : 4;

	if ((deleteChatButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, DELETE_CHAT_BUTTON_ID,
		BUTTON_TextPen, deleteChatButtonPen,
		GA_TextAttr, &uiTextAttr,
		BUTTON_Justification, BCJ_CENTER,
		GA_Text, (ULONG)"- Delete Chat",
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
		GA_TextAttr, &uiTextAttr,
		LISTBROWSER_WrapText, TRUE,
		LISTBROWSER_AutoFit, TRUE,
		LISTBROWSER_ShowSelected, TRUE,
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
		GA_ReadOnly, TRUE,
		GA_TextAttr, &uiTextAttr,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	if ((textInputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, TEXT_INPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_TextAttr, &chatTextAttr,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	if ((chatOutputTextEditor = NewObject(TEXTEDITOR_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_TEXT_EDITOR_ID,
		GA_RelVerify, TRUE,
		GA_ReadOnly, TRUE,
		GA_TextAttr, &chatTextAttr,
		ICA_TARGET, ICTARGET_IDCMP,
		GA_TEXTEDITOR_ImportHook, GV_TEXTEDITOR_ImportHook_MIME,
		GA_TEXTEDITOR_ExportHook, GV_TEXTEDITOR_ExportHook_Plain,
		TAG_DONE)) == NULL) {
			printf("Could not create text editor\n");
			return RETURN_ERROR;
	}

	if ((chatOutputScroller = NewObject(SCROLLER_GetClass(), NULL,
		GA_ID, CHAT_OUTPUT_SCROLLER_ID,
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create scroller\n");
			return RETURN_ERROR;
	}

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

	idcmpHook.h_Entry = (HOOKFUNC)processIDCMP;
	idcmpHook.h_Data = NULL;
	idcmpHook.h_SubEntry = NULL;

	appPort = CreateMsgPort();

	#ifdef __AMIGAOS4__
	refreshModelMenuItems();
	#endif

	if ((mainWindowObject = NewObject(WINDOW_GetClass(), NULL,
		WINDOW_Position, WPOS_CENTERSCREEN,
		WA_Activate, TRUE,
		WA_Title, "AmigaGPT",
		WA_Width, (WORD)(screen->Width * 0.8),
		WA_Height, (WORD)(screen->Height * 0.8),
		WA_CloseGadget, TRUE,
		WA_DragBar, isPublicScreen,
		WA_SizeGadget, isPublicScreen,
		WA_DepthGadget, isPublicScreen,
		WA_NewLookMenus, TRUE,
		WINDOW_AppPort, appPort,
		WINDOW_IconifyGadget, isPublicScreen,
		WINDOW_Layout, mainLayout,
		WINDOW_SharedPort, NULL,
		WINDOW_Position, isPublicScreen ? WPOS_CENTERSCREEN : WPOS_FULLSCREEN,
		WINDOW_NewMenu, amigaGPTMenu,
		WINDOW_IDCMPHook, &idcmpHook,
		WINDOW_InterpretIDCMPHook, TRUE,
		WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE,
		WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_MENUPICK | IDCMP_RAWKEY,
		WA_CustomScreen, screen,
		TAG_DONE)) == NULL) {
			printf("Could not create mainWindow object\n");
			return RETURN_ERROR;
	}

	if ((mainWindow = (struct Window *)DoMethod(mainWindowObject, WM_OPEN, NULL)) == NULL) {
		printf("Could not open mainWindow\n");
		return RETURN_ERROR;
	}

	// #ifdef __AMIGAOS3__
	refreshModelMenuItems();
	refreshSpeechMenuItems();
	// #endif

	// For some reason it won't let you paste text into the empty text editor unless you do this
	DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "", GV_TEXTEDITOR_InsertText_Bottom);

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
		GA_Width, 100,
		GA_Height, 30,
		BUTTON_Justification, BCJ_CENTER,
		GA_Text, (ULONG)radioButtonOptions,
		GA_RelVerify, TRUE,
		ICA_TARGET, ICTARGET_IDCMP,
		TAG_DONE)) == NULL) {
			printf("Could not create screenSelectRadioButton\n");
			return RETURN_ERROR;
	}

	if ((selectScreenOkButton = NewObject(BUTTON_GetClass(), NULL,
		GA_ID, SCREEN_SELECT_OK_BUTTON_ID,
		GA_Width, 100,
		GA_Height, 30,
		BUTTON_Justification, BCJ_CENTER,
		GA_Text, (ULONG)"OK",
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
		WA_Width, 200,
		WA_Height, 50,
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
								ASLSM_InitialDisplayDepth, 4,
								ASLSM_MinDepth, 4,
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
 * Sends a message to the OpenAI API and displays the response and speaks it if speech is enabled
**/
static void sendMessage() {
	BOOL isNewConversation = FALSE;
	STRPTR finishReason = NULL;
	struct json_object **responses;
	if (currentConversation == NULL) {
		isNewConversation = TRUE;
		currentConversation = newConversation();
	}
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
	SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Sending", TAG_DONE);
	STRPTR receivedMessage = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);

	STRPTR text = DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ExportText, NULL);
	// Remove trailing newline characters
	while (text[strlen(text) - 1] == '\n') {
		text[strlen(text) - 1] = '\0';
	}
	STRPTR textUTF_8 = ISO8859_1ToUTF8(text);
	addTextToConversation(currentConversation, textUTF_8, "user");
	displayConversation(currentConversation);
	DoGadgetMethod(textInputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
	ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);

	BOOL dataStreamFinished = FALSE;
	ULONG speechIndex = 0;
	UWORD wordNumber = 0;
	DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, "\n", GV_TEXTEDITOR_InsertText_Bottom);
	do {
		responses = postMessageToOpenAI(currentConversation, config.model, config.openAiApiKey, TRUE);
		UWORD responseIndex = 0;
		struct json_object *response;
		while (response = responses[responseIndex++]) {
			struct json_object *error;
			if (json_object_object_get_ex(response, "error", &error)) {
				struct json_object *message = json_object_object_get(error, "message");
				STRPTR messageString = json_object_get_string(message);
				displayError(messageString);
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
				json_object_put(response);
				FreeVec(receivedMessage);
				return;
			}
			STRPTR contentString = getMessageContentFromJson(response, TRUE);
			if (contentString != NULL) {
				formatText(contentString);
				STRPTR contentStringISO8859_1 = UTF8ToISO8859_1(contentString);
				strncat(receivedMessage, contentString, READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
				STRPTR receivedMessageISO8859_1 = UTF8ToISO8859_1(receivedMessage);
				DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_InsertText, NULL, contentStringISO8859_1, GV_TEXTEDITOR_InsertText_Bottom);
				if (++wordNumber % 50 == 0) {
					if (config.speechEnabled) {
						speakText(receivedMessageISO8859_1 + speechIndex);
						speechIndex = strlen(receivedMessageISO8859_1);
						FreeVec(receivedMessageISO8859_1);
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
			speakText(receivedMessageISO8859_1 + speechIndex);
			FreeVec(receivedMessageISO8859_1);
		}
		FreeVec(responses);
		FreeVec(receivedMessage);
		SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
		if (isNewConversation) {
			SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Generating conversation title", TAG_DONE);
			addTextToConversation(currentConversation, "generate a short title for this conversation and don't enclose the title in quotes or prefix the response with anything", "user");
			responses = postMessageToOpenAI(currentConversation, config.model, config.openAiApiKey, FALSE);
			if (responses[0] != NULL) {
				STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
				formatText(responseString);
				addConversationToConversationList(conversationList, currentConversation, responseString);
				SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
				struct MinNode *titleRequestNode = RemTail(currentConversation);
				FreeVec(titleRequestNode);
				json_object_put(responses[0]);
				FreeVec(responses);
			}
		}
	}
	FreeVec(text);
	FreeVec(textUTF_8);
}

/**
 * Updates the menu
 */
static void updateMenu() {
	#ifdef __AMIGAOS3__
	APTR *visualInfo;
	ULONG error = NULL;
	FreeMenus(menu);
	if (visualInfo = GetVisualInfo(screen, NULL)) {
		if (menu = CreateMenus(amigaGPTMenu, GTMN_SecondaryError, &error, TAG_DONE)) {
			if (LayoutMenus(menu, visualInfo, GTMN_NewLookMenus, TRUE, TAG_DONE)) {
				if (SetMenuStrip(mainWindow, menu)) {
					RefreshWindowFrame(mainWindow);
				} else {
					printf("Error setting menu strip\n");
				}
			} else {
				printf("Error laying out menu\n");
			}
		} else {
			printf("Error creating menu: %ld\n", error);
		}
		FreeVisualInfo(visualInfo);
	}
	#else
	SetAttrs(mainWindowObject, WINDOW_NewMenu, amigaGPTMenu, TAG_DONE);
	GetAttr(WINDOW_MenuStrip, mainWindowObject, &menu);
	#endif
}

/**
 * Sets the checkbox for the model that is currently selected
**/
static void refreshModelMenuItems() {
	APTR *visualInfo;
	ULONG error = NULL;
	struct NewMenu *newMenu = amigaGPTMenu;
	while (newMenu->nm_UserData != MENU_ITEM_MODEL_ID) {
		newMenu++;
	}

	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, MODEL_NAMES[config.model]) == 0) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}

	updateMenu();
}

/**
 * Sets the checkboxes for the speech options that are currently selected
**/
static void refreshSpeechMenuItems() {
	APTR *visualInfo;
	ULONG error = NULL;
	struct NewMenu *newMenu = amigaGPTMenu;
	while (newMenu->nm_UserData != MENU_ITEM_SPEECH_ENABLED_ID) {
		newMenu++;
	}

	if (config.speechEnabled) {
		newMenu++->nm_Flags |= CHECKED;
	} else {
		newMenu++->nm_Flags &= ~CHECKED;
	}

	#ifdef __AMIGAOS3__
	newMenu++;
	while ((++newMenu)->nm_Type == NM_SUB) {
		if (strcmp(newMenu->nm_Label, SPEECH_SYSTEM_NAMES[config.speechSystem]) == 0) {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}
	#else
	while ((++newMenu)->nm_Type == NM_SUB) {
		CONST_STRPTR currentVoiceName = SPEECH_VOICE_NAMES[config.speechVoice];

		// Find the length of the first word in the label
		UBYTE labelLen = 0;
		while (newMenu->nm_Label[labelLen] != ' ') {
			labelLen++;
		}

		// Compare the first word of the label with currentVoiceName
		// We have to do this because the (slow) warnings were appended to the label
		if (strncmp(newMenu->nm_Label, currentVoiceName, labelLen) == 0 &&
			currentVoiceName[labelLen] == '\0') {
			newMenu->nm_Flags |= CHECKED;
		} else {
			newMenu->nm_Flags &= ~CHECKED;
		}
	}
	#endif

	updateMenu();
}

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
**/
static struct MinList* newConversation() {
	struct MinList *conversation = AllocVec(sizeof(struct MinList), MEMF_CLEAR);
	
	// NewMinList(conversation); // This is what makes us require exec.library 45. Replace with the following:
	if (conversation) {
		conversation->mlh_Tail = 0;
		conversation->mlh_Head = (struct MinNode *)&conversation->mlh_Tail;
		conversation->mlh_TailPred = (struct MinNode *)&conversation->mlh_Head;
	}
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
	conversationNode->content = AllocVec(strlen(text) + 1, MEMF_CLEAR);
	strncpy(conversationNode->content, text, strlen(text));
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
				SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, TRUE, TAG_DONE);
				return;
			}
			if (strcmp(conversationNode->role, "user") == 0) {
				STRPTR content = conversationNode->content;
				strncat(conversationString, "*", WRITE_BUFFER_LENGTH - strlen(conversationString) - 2);
				while (*content != '\0') {
					if (*content == '\n') {
						strncat(conversationString, "*", WRITE_BUFFER_LENGTH - strlen(conversationString) - 2);
						strncat(conversationString, content++, 1);
						strncat(conversationString, "*", WRITE_BUFFER_LENGTH - strlen(conversationString) - 2);
					} else {
						strncat(conversationString, content++, 1);
					}
				}
				strncat(conversationString, "*\n\n", WRITE_BUFFER_LENGTH - strlen(conversationString) - 4);
			} else {
				strncat(conversationString, conversationNode->content, WRITE_BUFFER_LENGTH - strlen(conversationString));
				strncat(conversationString, "\n\n", WRITE_BUFFER_LENGTH - strlen(conversationString) - 3);	
			}	
	}

	STRPTR conversationStringISO8859_1 = UTF8ToISO8859_1(conversationString);
	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_Contents, conversationStringISO8859_1, TAG_DONE);
	Delay(2);
	SetGadgetAttrs(chatOutputTextEditor, mainWindow, NULL, GA_TEXTEDITOR_CursorY, ~0, TAG_DONE);
	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);
	FreeVec(conversationString);
	FreeVec(conversationStringISO8859_1);
}

/**
 * Free the conversation
 * @param conversation The conversation to free
**/
static void freeConversation(struct MinList *conversation) {
	struct ConversationNode *conversationNode;
	while ((conversationNode = (struct ConversationNode *)RemHead(conversation)) != NULL) {
		FreeVec(conversationNode->content);
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
	if (conversation == NULL) return;
	struct Node *node = conversationList->lh_Head;
	while (node != NULL) {
		struct MinList *listBrowserConversation;
		GetListBrowserNodeAttrs(node, LBNA_UserData, (struct MinList *)&listBrowserConversation, TAG_END);
		if (listBrowserConversation == conversation) {
			SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Selected, -1, TAG_DONE);
			SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, ~0, TAG_DONE);
			Remove(node);
			FreeListBrowserNode(node);
			SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, LISTBROWSER_Labels, conversationList, TAG_DONE);
			freeConversation(conversation);
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

	activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;

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
								activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
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
							activeTextEditorGadgetID = TEXT_INPUT_TEXT_EDITOR_ID;
							break;
						case DELETE_CHAT_BUTTON_ID:
							removeConversationFromConversationList(conversationList, currentConversation);
							currentConversation = NULL;
							DoGadgetMethod(chatOutputTextEditor, mainWindow, NULL, GM_TEXTEDITOR_ClearText, NULL);
							ActivateLayoutGadget(mainLayout, mainWindow, NULL, textInputTextEditor);
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
					}
					break;
				case WMHI_MENUPICK:
				{
					struct MenuItem *menuItem = ItemAddress(menu, code);
					ULONG itemIndex = GTMENUITEM_USERDATA(menuItem);
					switch (itemIndex) {
						case MENU_ITEM_ABOUT_ID:
							openAboutWindow();
							break;
						case MENU_ITEM_CUT_ID:
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
						#ifdef __AMIGAOS3__
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
						#else
						case MENU_ITEM_SPEECH_VOICE_AWB_ID:
							config.speechVoice = SPEECH_VOICE_AWB;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_VOICE_KAL_ID:
							config.speechVoice = SPEECH_VOICE_KAL;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_VOICE_KAL16_ID:
							config.speechVoice = SPEECH_VOICE_KAL16;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_VOICE_RMS_ID:
							config.speechVoice = SPEECH_VOICE_RMS;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						case MENU_ITEM_SPEECH_VOICE_SLT_ID:
							config.speechVoice = SPEECH_VOICE_SLT;
							writeConfig();
							refreshSpeechMenuItems();
							break;
						#endif
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
						case MENU_ITEM_MODEL_GPT_4_0613_ID:
							config.model = GPT_4_0613;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_4_1106_PREVIEW_ID:
							config.model = GPT_4_1106_PREVIEW;
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
						case MENU_ITEM_MODEL_GPT_4_32K_0613_ID:
							config.model = GPT_4_32K_0613;
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
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_0613_ID:
							config.model = GPT_3_5_TURBO_0613;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_1106_ID:
							config.model = GPT_3_5_TURBO_1106;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_16K_ID:
							config.model = GPT_3_5_TURBO_16K;
							writeConfig();
							refreshModelMenuItems();
							break;
						case MENU_ITEM_MODEL_GPT_3_5_TURBO_16K_0613_ID:
							config.model = GPT_3_5_TURBO_16K_0613;
							writeConfig();
							refreshModelMenuItems();
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

	return RETURN_OK;
}

/**
 * Opens the application's documentation guide
*/
void openDocumentation() {
	struct NewAmigaGuide guide = {
		.nag_Name = "PROGDIR:AmigaGPT.guide",
		.nag_Screen = screen,
		.nag_PubScreen = NULL,
		.nag_BaseName = "AmigaGPT",
		.nag_Extens = NULL,
	};
	AMIGAGUIDECONTEXT handle;
	if (handle = OpenAmigaGuide(&guide, NULL)) {
		CloseAmigaGuide(handle);
	} else {
		displayDiskError("Could not open documentation", IoErr());
	}
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

	SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_Disabled, FALSE, TAG_DONE);

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
		#ifdef __AMIGAOS3__
		"AmigaGPT for m68k AmigaOS 3\n\n"
		#else
		"AmigaGPT for PPC AmigaOS 4\n\n"
		#endif
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
			SetGadgetAttrs(deleteChatButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			SetGadgetAttrs(sendMessageButton, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			SetGadgetAttrs(statusBar, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);
			SetGadgetAttrs(statusBar, mainWindow, NULL, STRINGA_TextVal, "Ready", TAG_DONE);
			SetGadgetAttrs(conversationListBrowser, mainWindow, NULL, GA_TextAttr, uiFont, TAG_DONE);

		}
		FreeAslRequest(fontRequester);
	}
}


/**
 * Opens a requester for the user to select accent for the speech
**/
static void openSpeechAccentRequester() {
	#ifdef __AMIGAOS3__
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
	#endif
}

/**
 * Opens a requester for the user to enter their OpenAI API key
**/
static void openApiKeyRequester() {
	UBYTE buffer[64];
	strncpy(buffer, config.openAiApiKey, sizeof(buffer) - 1);
	Object *apiKeyRequester = NewObject(REQUESTER_GetClass(), NULL,
		REQ_Type, REQTYPE_STRING,
		REQ_TitleText, "Enter your OpenAI API key",
		REQ_BodyText, "Please type or paste (Right Amiga + V) your OpenAI API key here",
		REQ_GadgetText, "OK|CANCEL",
		REQ_Image, REQIMAGE_INFO,
		REQS_AllowEmpty, FALSE,
		REQS_Buffer, buffer,
		REQS_MaxChars, sizeof(buffer) - 1,
		REQS_Invisible, FALSE,
		REQ_ForceFocus, TRUE,
		TAG_DONE);

	if (apiKeyRequester) {
		ULONG result = OpenRequester(apiKeyRequester, mainWindow);
		if (result == 1) {
			strncpy(config.openAiApiKey, buffer, sizeof(config.openAiApiKey) - 1);
			writeConfig();
		}
		DisposeObject(apiKeyRequester);
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
LONG loadConversations() {
	BPTR file = Open("PROGDIR:chat-history.json", MODE_OLDFILE);
	if (file == 0) {
		return RETURN_OK;
	}

	#ifdef __AMIGAOS3__
	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	#else
	int64 fileSize = GetFileSize(file);
	#endif
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
			#ifdef __AMIGAOS3__
			if (!DeleteFile("PROGDIR:chat-history.json")) {
			#else
			if (!Delete("PROGDIR:chat-history.json")) {
			#endif
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
		ReleasePen(screen->ViewPort.ColorMap, sendMessageButtonPen);
		ReleasePen(screen->ViewPort.ColorMap, newChatButtonPen);
		ReleasePen(screen->ViewPort.ColorMap, deleteChatButtonPen);
		UnlockPubScreen(NULL, screen);
	} else {
		CloseScreen(screen);
	}
	if (uiTextFont)
		CloseFont(uiTextFont);
	if (appPort)
		DeleteMsgPort(appPort);
	#ifdef __AMIGAOS3__
	FreeMenus(menu);
	#endif

	closeGUILibraries();
}