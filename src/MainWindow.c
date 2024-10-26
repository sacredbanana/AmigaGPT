#include <libraries/mui.h>
#include <mui/Busy_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/muimaster.h>
#include <SDI_hook.h>
#include <string.h>
#include "config.h"
#include "gui.h"
#include "menu.h"
#include "MainWindow.h"

struct Window *mainWindow;
Object *mainWindowObject;
Object *newChatButton;
Object *deleteChatButton;
Object *sendMessageButton;
Object *chatInputTextEditor;
Object *chatOutputTextEditor;
Object *chatOutputScroller;
Object *statusBar;
Object *conversationListObject;
Object *loadingBar;
WORD pens[NUMDRIPENS + 1];
LONG textEditorColorMap[] = {5,10,6,3,6,6,4,0,1,6,6,6,6,6,6,6};
LONG sendMessageButtonPen;
LONG newChatButtonPen;
LONG deleteButtonPen;
BOOL isPublicScreen;
struct Conversation *currentConversation;

enum ButtonLabels {
	NEW_CHAT_BUTTON_LABEL,
	DELETE_CHAT_BUTTON_LABEL,
	SEND_MESSAGE_BUTTON_LABEL
};

CONST_STRPTR BUTTON_LABEL_NAMES[] = {
	"+ New Chat",
	"- Delete Chat",
	"\nSend\n"
};

HOOKPROTONHNO(ConstructLI_TextFunc, APTR, struct NList_ConstructMessage *ncm) {
	struct Conversation *oldEntry = (struct Conversation *)ncm->entry;
	struct Conversation *newEntry = copyConversation(oldEntry);
    return (newEntry);
}
MakeHook(ConstructLI_TextHook, ConstructLI_TextFunc);

HOOKPROTONHNO(DestructLI_TextFunc, void, struct NList_DestructMessage *ndm) {
	if (ndm->entry)
		freeConversation((struct Conversation *)ndm->entry);
}
MakeHook(DestructLI_TextHook, DestructLI_TextFunc);

HOOKPROTONHNO(DisplayLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
  struct Conversation *entry = (struct Conversation *) ndm->entry;
  ndm->strings[0] = (STRPTR)entry->name;
}
MakeHook(DisplayLI_TextHook, DisplayLI_TextFunc);

HOOKPROTONHNONP(ConversationRowClickedFunc, void) {
	if (currentConversation != NULL)
		freeConversation(currentConversation);

	struct Conversation *conversation;
	DoMethod(conversationListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &conversation);
	currentConversation = copyConversation(conversation);
	displayConversation(currentConversation);
}
MakeHook(ConversationRowClickedHook, ConversationRowClickedFunc);

HOOKPROTONHNONP(NewChatButtonClickedFunc, void) {
	currentConversation = NULL;
	DoMethod(chatOutputTextEditor, MUIM_TextEditor_ClearText);
}
MakeHook(NewChatButtonClickedHook, NewChatButtonClickedFunc);

HOOKPROTONHNONP(DeleteChatButtonClickedFunc, void) {
	DoMethod(conversationListObject, MUIM_NList_Remove, MUIV_NList_Remove_Active);
	currentConversation = NULL;
	DoMethod(chatOutputTextEditor, MUIM_TextEditor_ClearText);
	saveConversations();
}
MakeHook(DeleteChatButtonClickedHook, DeleteChatButtonClickedFunc);

HOOKPROTONHNONP(SendMessageButtonClickedFunc, void) {
	if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
		sendChatMessage();
	}
	else {
		displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
	}
}
MakeHook(SendMessageButtonClickedHook, SendMessageButtonClickedFunc);

HOOKPROTONHNONP(ConfigureForScreenFunc, void) {
	const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
	STRPTR buttonLabelText = AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", greenPen, BUTTON_LABEL_NAMES[NEW_CHAT_BUTTON_LABEL]);
	set(newChatButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", redPen, BUTTON_LABEL_NAMES[DELETE_CHAT_BUTTON_LABEL]);
	set(deleteChatButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", bluePen, BUTTON_LABEL_NAMES[SEND_MESSAGE_BUTTON_LABEL]);
	set(sendMessageButton, MUIA_Text_Contents, buttonLabelText);
	FreeVec(buttonLabelText);
	SetAttrs(mainWindowObject,
		MUIA_Window_ID, isPublicScreen ? OBJECT_ID_MAIN_WINDOW : NULL,
		MUIA_Window_DepthGadget, isPublicScreen,
        MUIA_Window_SizeGadget, isPublicScreen,
        MUIA_Window_DragBar, isPublicScreen,
        MUIA_Window_Width, MUIV_Window_Width_Screen(isPublicScreen ? 90 : 100),
        MUIA_Window_Height, MUIV_Window_Height_Screen(isPublicScreen ? 90 : 100),
        MUIA_Window_ActiveObject, chatInputTextEditor,
		MUIA_Window_Open, TRUE,
        TAG_DONE);
	
	updateStatusBar("Ready", greenPen);
}
MakeHook(ConfigureForScreenHook, ConfigureForScreenFunc);

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG createMainWindow() {
	createMenu();

	if ((mainWindowObject = WindowObject,
		MUIA_Window_Title, "AmigaGPT",
		MUIA_Window_CloseGadget, TRUE,
		MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
		MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
		MUIA_Window_Menustrip, menuStrip,
		MUIA_Window_SizeRight, TRUE,
		MUIA_Window_UseBottomBorderScroller, FALSE,
		MUIA_Window_UseRightBorderScroller, FALSE,
		MUIA_Window_UseLeftBorderScroller, FALSE,
		WindowContents, HGroup,
				Child, VGroup, MUIA_Weight, 30,
					// New chat button
					Child, newChatButton = MUI_MakeObject(MUIO_Button, "",
						MUIA_Width, 500,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
					// Delete chat button
					Child, deleteChatButton = MUI_MakeObject(MUIO_Button, BUTTON_LABEL_NAMES[DELETE_CHAT_BUTTON_LABEL],
						MUIA_Width, 500,
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
					// Conversation list
					Child, NListviewObject,
						MUIA_CycleChain, 1,
						MUIA_NListview_NList, conversationListObject = NListObject,
							MUIA_NList_DefaultObjectOnClick, TRUE,
							MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
							MUIA_NList_ConstructHook2, &ConstructLI_TextHook,
							MUIA_NList_DestructHook2, &DestructLI_TextHook,
							MUIA_NList_DisplayHook2, &DisplayLI_TextHook,
							MUIA_NList_Format, "BAR MINW=100 MAXW=200",
							MUIA_NList_AutoVisible, TRUE,
							MUIA_NList_TitleSeparator, FALSE,
							MUIA_NList_Title, FALSE,
							MUIA_NList_MinColSortable, 0,
							MUIA_NList_Imports, MUIV_NList_Imports_All,
							MUIA_NList_Exports, MUIV_NList_Exports_All,
						End,
					End,
				End,
				Child, VGroup,
					// Chat output text display
					Child, HGroup,
						Child, chatOutputTextEditor = TextEditorObject,
							MUIA_TextEditor_Contents, "",
							MUIA_Text_Copy, TRUE,
							MUIA_Text_Marking, TRUE,
							MUIA_Text_SetMin, FALSE,
							MUIA_Text_SetMax, FALSE,
							MUIA_Text_SetVMax, FALSE,
							MUIA_Text_Shorten, MUIV_Text_Shorten_Nothing,
							MUIA_TextEditor_ReadOnly, TRUE,
							MUIA_TextEditor_ImportHook,  MUIV_TextEditor_ImportHook_EMail,
							MUIA_TextEditor_Slider, chatOutputScroller,
						End,
						Child, chatOutputScroller = ScrollbarObject,
						End,
					End,
					Child, HGroup,
						// Chat input text editor
						Child, chatInputTextEditor = TextEditorObject, MUIA_HorizWeight, 90,
							MUIA_ObjectID, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
							MUIA_TextEditor_ReadOnly, FALSE,
							MUIA_HorizWeight, 90,
							MUIA_TextEditor_TabSize, 4,
							MUIA_TextEditor_Rows, 3,
							MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
						End,
						// Send message button
						Child, HGroup, MUIA_HorizWeight, 10,
							Child, sendMessageButton = MUI_MakeObject(MUIO_Button, BUTTON_LABEL_NAMES[SEND_MESSAGE_BUTTON_LABEL],
								MUIA_ObjectID, OBJECT_ID_SEND_MESSAGE_BUTTON,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
								MUIA_Text_Contents, BUTTON_LABEL_NAMES[SEND_MESSAGE_BUTTON_LABEL],
							End,
						End,
					End,
					// Status bar
					Child, statusBar = TextObject,
						TextFrame,
						MUIA_Text_Contents, "Ready",
						MUIA_Background, MUII_SHADOWBACK,
					End,
					// Loading bar
					Child, loadingBar = BusyObject,
						MUIA_MaxHeight, 10,
						MUIA_Busy_Speed, MUIV_Busy_Speed_Off,
					End,
				End,
			End,
	End) == NULL) {
        displayError("Could not create main window");
        return RETURN_ERROR;
    }

    get(mainWindowObject, MUIA_Window, &mainWindow);   

	DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_Screen, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &ConfigureForScreenHook);

	addMainWindowActions();
	UnlockPubScreen(NULL, screen);       
    return RETURN_OK;
}

/**
 * Add actions to the main window
**/ 
void addMainWindowActions() {
	DoMethod(newChatButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  newChatButton, 2, MUIM_CallHook, &NewChatButtonClickedHook);
	DoMethod(deleteChatButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  MUIV_Notify_Application, 3, MUIM_CallHook, &DeleteChatButtonClickedHook, MUIV_TriggerValue);
	DoMethod(sendMessageButton, MUIM_Notify, MUIA_Pressed, FALSE,
              sendMessageButton, 2, MUIM_CallHook, &SendMessageButtonClickedHook);
	DoMethod(conversationListObject, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook, &ConversationRowClickedHook, MUIV_TriggerValue);
	DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, 
	  MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
}