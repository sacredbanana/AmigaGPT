#include <libraries/mui.h>
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
WORD pens[NUMDRIPENS + 1];
LONG textEditorColorMap[] = {5,10,6,3,6,6,4,0,1,6,6,6,6,6,6,6};
LONG sendMessageButtonPen;
LONG newChatButtonPen;
LONG deleteButtonPen;
BOOL isPublicScreen;
struct Conversation *currentConversation;

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

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG createMainWindow() {
	createMenu();

	chatOutputScroller = ScrollbarObject, End;

	if ((mainWindowObject = WindowObject,
		MUIA_Window_Title, "AmigaGPT",
		MUIA_Window_ID, OBJECT_ID_MAIN_WINDOW,
		MUIA_Window_CloseGadget, TRUE,
		MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
		MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
		MUIA_Window_Menustrip, menuStrip,
		MUIA_Window_SizeRight, TRUE,
		MUIA_Window_UseBottomBorderScroller, FALSE,
		MUIA_Window_UseRightBorderScroller, FALSE,
		MUIA_Window_UseLeftBorderScroller, FALSE,
		WindowContents, HGroup,
				Child, VGroup,
					Child, VGroup,
						// New chat button
						Child, newChatButton = MUI_MakeObject(MUIO_Button, "+ New Chat",
							MUIA_Width, 500,
							MUIA_CycleChain, TRUE,
							MUIA_InputMode, MUIV_InputMode_RelVerify,
						End,
						// Delete chat button
						Child, deleteChatButton = MUI_MakeObject(MUIO_Button, "- Delete Chat",
							MUIA_Width, 500,
							MUIA_Background, MUII_FILL,
							MUIA_CycleChain, TRUE,
							MUIA_InputMode, MUIV_InputMode_RelVerify,
						End,
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
					// Status bar
					Child, statusBar = StringObject,
						MUIA_ObjectID, OBJECT_ID_STATUS_BAR,
						MUIA_String_MaxLen, 1024,
						MUIA_CycleChain, TRUE,
						MUIA_String_Accept, "",
						// MUIA_Background, MUII_FILL,
					End,
					Child, HGroup,
						// Chat input text editor
						Child, chatInputTextEditor = TextEditorObject,
							MUIA_ObjectID, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
							MUIA_TextEditor_ReadOnly, FALSE,
							MUIA_TextEditor_TabSize, 4,
							MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
						End,
						// Send message button
						Child, sendMessageButton = MUI_MakeObject(MUIO_Button, "Send",
						MUIA_ObjectID, OBJECT_ID_SEND_MESSAGE_BUTTON,
							MUIA_CycleChain, TRUE,
							MUIA_InputMode, MUIV_InputMode_RelVerify,
						End,
					End,
				End,
			End,
	End) == NULL) {
        displayError("Could not create main window");
        return RETURN_ERROR;
    }

    get(mainWindowObject, MUIA_Window, &mainWindow);          
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