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
#include "datatypesclass.h"
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
Object *imageInputTextEditor;
Object *createImageButton;
Object *newImageButton;
Object *deleteImageButton;
Object *imageListObject;
Object *imageViewContainer;
Object *imageView;
Object *openSmallImageButton;
Object *openMediumImageButton;
Object *openLargeImageButton;
Object *openOriginalImageButton;
WORD pens[NUMDRIPENS + 1];
LONG textEditorColorMap[] = {5,10,6,3,6,6,4,0,1,6,6,6,6,6,6,6};
LONG sendMessageButtonPen;
LONG newChatButtonPen;
LONG deleteButtonPen;
BOOL isPublicScreen;
struct Conversation *currentConversation;
struct GeneratedImage *currentImage;
static char *Pages[]   = { "Chat Mode", "Image Generation Mode", NULL };

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

HOOKPROTONHNO(ConstructConversationLI_TextFunc, APTR, struct NList_ConstructMessage *ncm) {
	struct Conversation *oldEntry = (struct Conversation *)ncm->entry;
	struct Conversation *newEntry = copyConversation(oldEntry);
    return (newEntry);
}
MakeHook(ConstructConversationLI_TextHook, ConstructConversationLI_TextFunc);

HOOKPROTONHNO(DestructConversationLI_TextFunc, void, struct NList_DestructMessage *ndm) {
	if (ndm->entry)
		freeConversation((struct Conversation *)ndm->entry);
}
MakeHook(DestructConversationLI_TextHook, DestructConversationLI_TextFunc);

HOOKPROTONHNO(DisplayConversationLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
  struct Conversation *entry = (struct Conversation *) ndm->entry;
  ndm->strings[0] = (STRPTR)entry->name;
}
MakeHook(DisplayConversationLI_TextHook, DisplayConversationLI_TextFunc);

HOOKPROTONHNO(ConstructImageLI_TextFunc, APTR, struct NList_ConstructMessage *ncm) {
	struct GeneratedImage *oldEntry = (struct GeneratedImage *)ncm->entry;
	struct GeneratedImage *newEntry = copyGeneratedImage(oldEntry);
    return (newEntry);
}
MakeHook(ConstructImageLI_TextHook, ConstructImageLI_TextFunc);

HOOKPROTONHNO(DestructImageLI_TextFunc, void, struct NList_DestructMessage *ndm) {
	if (ndm->entry) {
		struct GeneratedImage *entry = (struct GeneratedImage *)ndm->entry;
		FreeVec(entry->name);
		FreeVec(entry->filePath);
		FreeVec(entry->prompt);
		FreeVec(entry);
	}
}
MakeHook(DestructImageLI_TextHook, DestructImageLI_TextFunc);

HOOKPROTONHNO(DisplayImageLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
  struct GeneratedImage *entry = (struct GeneratedImage *) ndm->entry;
  ndm->strings[0] = (STRPTR)entry->name;
}
MakeHook(DisplayImageLI_TextHook, DisplayImageLI_TextFunc);

HOOKPROTONHNONP(ConversationRowClickedFunc, void) {
	if (currentImage != NULL)
		freeConversation(currentConversation);

	struct Conversation *conversation;
	DoMethod(conversationListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &conversation);
	currentConversation = copyConversation(conversation);
	displayConversation(currentConversation);
}
MakeHook(ConversationRowClickedHook, ConversationRowClickedFunc);

HOOKPROTONHNONP(ImageRowClickedFunc, void) {
	if (currentImage != NULL) {
		FreeVec(currentImage->name);
		FreeVec(currentImage->filePath);
		FreeVec(currentImage->prompt);
		FreeVec(currentImage);
	}

	struct GeneratedImage *image;
	DoMethod(imageListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &image);
	currentImage = copyGeneratedImage(image);
	// STRPTR imagePath = AllocVec(strlen(currentImage->filePath) + 3, MEMF_ANY);
	// snprintf(imagePath, strlen(currentImage->filePath) + 3, "5:%s", currentImage->filePath);
	// DoMethod(imageViewContainer, MUIM_Group_InitChange);
	// DoMethod(imageViewContainer, OM_REMMEMBER, imageView);
	// MUI_DisposeObject(imageView);
	// imageView = ImageObject,
	// 				MUIA_Image_CopySpec, TRUE,
	// 				MUIA_Image_Spec, imagePath,
	// 				MUIA_Image_FreeHoriz, TRUE,
	// 				MUIA_Image_FreeVert, TRUE,
	// 				// MUIA_Dtpic_Name, "PROGDIR:images/cactus.png",
	// 				MUIA_Width, 200,
	// 				MUIA_Height, 200,
	// 			End;
	// DoMethod(imageViewContainer, OM_ADDMEMBER, imageView);
	// DoMethod(imageViewContainer, MUIM_Group_ExitChange);
	// FreeVec(imagePath);
	// set(imageView, MUIA_Dtpic_Name, currentImage->filePath);
}
MakeHook(ImageRowClickedHook, ImageRowClickedFunc);

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

HOOKPROTONHNONP(CreateImageButtonClickedFunc, void) {
	if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
		createImage();
		saveImages();
	} else {
		displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
	}
}
MakeHook(CreateImageButtonClickedHook, CreateImageButtonClickedFunc);

HOOKPROTONHNONP(OpenSmallImageButtonClickedFunc, void) {
	if (currentImage->width == currentImage->height)
		openImage(currentImage, 256, 256);
	else if (currentImage->width > currentImage->height) {
		LONG height = (currentImage->height * 256) / currentImage->width;
		openImage(currentImage, 256, height);
	} else {
		LONG width = (currentImage->width * 256) / currentImage->height;
		openImage(currentImage, width, 256);
	}
}
MakeHook(OpenSmallImageButtonClickedHook, OpenSmallImageButtonClickedFunc);

HOOKPROTONHNONP(OpenMediumImageButtonClickedFunc, void) {
	if (currentImage->width == currentImage->height)
		openImage(currentImage, 512, 512);
	else if (currentImage->width > currentImage->height) {
		LONG height = (currentImage->height * 512) / currentImage->width;
		openImage(currentImage, 512, height);
	} else {
		LONG width = (currentImage->width * 512) / currentImage->height;
		openImage(currentImage, width, 512);
	}
}
MakeHook(OpenMediumImageButtonClickedHook, OpenMediumImageButtonClickedFunc);

HOOKPROTONHNONP(OpenLargeImageButtonClickedFunc, void) {
	if (currentImage->width == currentImage->height)
		openImage(currentImage, 1124, 1024);
	else if (currentImage->width > currentImage->height) {
		LONG height = (currentImage->height * 1024) / currentImage->width;
		openImage(currentImage, 1024, height);
	} else {
		LONG width = (currentImage->width * 1024) / currentImage->height;
		openImage(currentImage, width, 1024);
	}
}
MakeHook(OpenLargeImageButtonClickedHook, OpenLargeImageButtonClickedFunc);

HOOKPROTONHNONP(OpenOriginalImageButtonClickedFunc, void) {
	openImage(currentImage, currentImage->width, currentImage->height);
}
MakeHook(OpenOriginalImageButtonClickedHook, OpenOriginalImageButtonClickedFunc);

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

struct MUI_CustomClass *mcc;
/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG createMainWindow() {
	createMenu();

	create_datatypes_class();

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
			WindowContents, VGroup,
				Child, RegisterGroup(Pages),
					Child, HGroup,
						Child, VGroup, MUIA_Weight, 30,
							// New chat button
							Child, newChatButton = MUI_MakeObject(MUIO_Button, "",
								// MUIA_Width, 500,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Delete chat button
							Child, deleteChatButton = MUI_MakeObject(MUIO_Button, BUTTON_LABEL_NAMES[DELETE_CHAT_BUTTON_LABEL],
								// MUIA_Width, 500,
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
									MUIA_NList_ConstructHook2, &ConstructConversationLI_TextHook,
									MUIA_NList_DestructHook2, &DestructConversationLI_TextHook,
									MUIA_NList_DisplayHook2, &DisplayConversationLI_TextHook,
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
							Child, HGroup, MUIA_VertWeight, 60,
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
								Child, chatOutputScroller = ScrollbarObject, End,
							End,
							// // Status bar
							// Child, statusBar = TextObject, MUIA_VertWeight, 10,
							// 	TextFrame,
							// 	MUIA_Text_Contents, "Ready",
							// 	MUIA_Background, MUII_SHADOWBACK,
							// End,
							// // Loading bar
							// Child, loadingBar = BusyObject, MUIA_VertWeight, 10,
							// 	MUIA_MaxHeight, 20,
							// 	MUIA_Busy_Speed, MUIV_Busy_Speed_Off,
							// End,
							Child, HGroup, MUIA_VertWeight, 20,
								// Chat input text editor
								Child, chatInputTextEditor = TextEditorObject,
									MUIA_ObjectID, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
									MUIA_TextEditor_ReadOnly, FALSE,
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
						End,
					End,
					Child, HGroup,
						Child, VGroup, MUIA_Weight, 30,
							// New image button
							Child, newImageButton = MUI_MakeObject(MUIO_Button, "New Image",
								// MUIA_ObjectID, OBJECT_ID_NEW_IMAGE_BUTTON,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Delete image button
							Child, deleteImageButton = MUI_MakeObject(MUIO_Button, "Delete Image",
								// MUIA_ObjectID, OBJECT_ID_DELETE_IMAGE_BUTTON,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Image list
							Child, NListviewObject,
								MUIA_CycleChain, 1,
								MUIA_NListview_NList, imageListObject = NListObject,
									MUIA_NList_DefaultObjectOnClick, TRUE,
									MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
									MUIA_NList_ConstructHook2, &ConstructImageLI_TextHook,
									MUIA_NList_DestructHook2, &DestructImageLI_TextHook,
									MUIA_NList_DisplayHook2, &DisplayImageLI_TextHook,
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
						Child, VGroup, MUIA_Weight, 70,
							// Image view container
							Child, imageViewContainer = VGroup,
								// Image view
								Child, imageView = DataTypesObject,
									TextFrame,
									MUIA_Background, MUII_BACKGROUND,
									TAG_END),
									
								// Child, imageView = ImageObject,
								// 	MUIA_Image_CopySpec, TRUE,
								// 	MUIA_Image_Spec, "5:PROGDIR:images/love.png",
								// 	MUIA_Image_FreeHoriz, TRUE,
								// 	MUIA_Image_FreeVert, TRUE,
								// 	// MUIA_Dtpic_Name, "PROGDIR:images/cactus.png",
								// 	MUIA_MinWidth, 200,
								// 	MUIA_MinHeight, 200,
							End,
							// Open small image button
							Child, openSmallImageButton = MUI_MakeObject(MUIO_Button, "Open Small",
								// MUIA_ObjectID, OBJECT_ID_OPEN_SMALL_IMAGE_BUTTON,
								MUIA_MaxWidth, 100,
								MUIA_Weight, 10,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Open medium image button
							Child, openMediumImageButton = MUI_MakeObject(MUIO_Button, "Open Medium",
								// MUIA_ObjectID, OBJECT_ID_OPEN_MEDIUM_IMAGE_BUTTON,
								MUIA_MaxWidth, 100,
								MUIA_Weight, 10,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Open large image button
							Child, openLargeImageButton = MUI_MakeObject(MUIO_Button, "Open Large",
								// MUIA_ObjectID, OBJECT_ID_OPEN_LARGE_IMAGE_BUTTON,
								MUIA_MaxWidth, 100,
								MUIA_Weight, 10,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Open original image button
							Child, openOriginalImageButton = MUI_MakeObject(MUIO_Button, "Open Original",
								// MUIA_ObjectID, OBJECT_ID_OPEN_ORIGINAL_IMAGE_BUTTON,
								MUIA_MaxWidth, 100,
								MUIA_Weight, 10,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							End,
							// Image input text editor
							Child, imageInputTextEditor = TextEditorObject,
								// MUIA_ObjectID, OBJECT_ID_IMAGE_INPUT_TEXT_EDITOR,
								MUIA_TextEditor_ReadOnly, FALSE,
								MUIA_TextEditor_TabSize, 4,
								MUIA_TextEditor_Rows, 3,
								MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
							End,
						End,
						// Create image button
						Child, createImageButton = MUI_MakeObject(MUIO_Button, "Create Image",
							// MUIA_ObjectID, OBJECT_ID_CREATE_IMAGE_BUTTON,
							MUIA_MaxWidth, 100,
							MUIA_Weight, 10,
							MUIA_CycleChain, TRUE,
							MUIA_InputMode, MUIV_InputMode_RelVerify,
						End,
					End,
				End,
				// Status bar
				Child, statusBar = TextObject, MUIA_VertWeight, 10,
					TextFrame,
					MUIA_Text_Contents, "Ready",
					MUIA_Background, MUII_SHADOWBACK,
				End,
				// Loading bar
				Child, loadingBar = BusyObject, MUIA_VertWeight, 10,
					MUIA_MaxHeight, 20,
					MUIA_Busy_Speed, MUIV_Busy_Speed_Off,
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
	DoMethod(createImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  createImageButton, 2, MUIM_CallHook, &CreateImageButtonClickedHook);
	DoMethod(openSmallImageButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  openSmallImageButton, 2, MUIM_CallHook, &OpenSmallImageButtonClickedHook);
	DoMethod(openMediumImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  openMediumImageButton, 2, MUIM_CallHook, &OpenMediumImageButtonClickedHook);
	DoMethod(openLargeImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  openLargeImageButton, 2, MUIM_CallHook, &OpenLargeImageButtonClickedHook);
	DoMethod(openOriginalImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  openOriginalImageButton, 2, MUIM_CallHook, &OpenOriginalImageButtonClickedHook);
	DoMethod(conversationListObject, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook, &ConversationRowClickedHook, MUIV_TriggerValue);
	DoMethod(imageListObject, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook, &ImageRowClickedHook, MUIV_TriggerValue);
	DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, 
	  MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
}