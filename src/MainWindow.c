#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/Busy_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/muimaster.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "gui.h"
#include "menu.h"
#include "datatypesclass.h"
#include "MainWindow.h"
#include <dos/dos.h>

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
Object *imageView;
Object *openImageButton;
Object *saveImageCopyButton;
WORD pens[NUMDRIPENS + 1];
BOOL isPublicScreen;
struct Conversation *currentConversation;
struct GeneratedImage *currentImage;
static char *Pages[]   = { "Chat Mode", "Image Generation Mode", NULL };

static struct Conversation* newConversation();
static STRPTR getMessageContentFromJson(struct json_object *json, BOOL stream);
static void formatText(STRPTR unformattedText);
static void addTextToConversation(struct Conversation *conversation, STRPTR text, STRPTR role);
static void displayConversation(struct Conversation *conversation);
static void freeConversation(struct Conversation *conversation);
static struct Conversation* copyConversation(struct Conversation *conversation);
static struct GeneratedImage* copyGeneratedImage(struct GeneratedImage *generatedImage);
static void sendChatMessage();
static LONG loadConversations();
static LONG saveConversations();
static LONG loadImages();
static LONG saveImages();
static STRPTR ISO8859_1ToUTF8(CONST_STRPTR iso8859_1String);
static STRPTR UTF8ToISO8859_1(CONST_STRPTR utf8String);
static BOOL copyFile(STRPTR source, STRPTR destination);

enum ButtonLabels {
	NEW_CHAT_BUTTON_LABEL,
	DELETE_CHAT_BUTTON_LABEL,
	SEND_MESSAGE_BUTTON_LABEL,
	NEW_IMAGE_BUTTON_LABEL,
	DELETE_IMAGE_BUTTON_LABEL,
	CREATE_IMAGE_BUTTON_LABEL
};

CONST_STRPTR BUTTON_LABEL_NAMES[] = {
	"+ New Chat",
	"- Delete Chat",
	"\nSend\n",
	"+ New Image",
	"- Delete Image",
	"\nCreate Image\n",
};

HOOKPROTONHNO(ConstructConversationLI_TextFunc, APTR, struct NList_ConstructMessage *ncm) {
	struct Conversation *entry = (struct Conversation *)ncm->entry;
    return (entry);
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
	struct GeneratedImage *entry = (struct GeneratedImage *)ncm->entry;
    return (entry);
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
	struct Conversation *conversation;
	DoMethod(conversationListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &conversation);
	currentConversation = conversation;
	displayConversation(currentConversation);
}
MakeHook(ConversationRowClickedHook, ConversationRowClickedFunc);

HOOKPROTONHNONP(ImageRowClickedFunc, void) {
	set(imageInputTextEditor, MUIA_Disabled, FALSE);

	struct GeneratedImage *image = NULL;
	DoMethod(imageListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, &image);
	if (image) {
		set(createImageButton, MUIA_Disabled, TRUE);
		set(openImageButton, MUIA_Disabled, FALSE);
		set(saveImageCopyButton, MUIA_Disabled, FALSE);
		set(imageInputTextEditor, MUIA_TextEditor_Contents, image->prompt);
		currentImage = image;
		set(imageView, MUIA_DataTypes_FileName, currentImage->filePath);
	}
}
MakeHook(ImageRowClickedHook, ImageRowClickedFunc);

HOOKPROTONHNONP(NewChatButtonClickedFunc, void) {
	currentConversation = NULL;
	DoMethod(chatOutputTextEditor, MUIM_TextEditor_ClearText);
	DoMethod(chatOutputTextEditor, MUIM_GoActive);
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
	} else {
		displayError("Please enter your OpenAI API key in the Open AI settings "
					 "in the menu.");
	}
}
MakeHook(SendMessageButtonClickedHook, SendMessageButtonClickedFunc);

HOOKPROTONHNONP(NewImageButtonClickedFunc, void) {
	currentImage = NULL;
	set(imageInputTextEditor, MUIA_Disabled, FALSE);
	set(createImageButton, MUIA_Disabled, FALSE);
	set(openImageButton, MUIA_Disabled, TRUE);
	set(saveImageCopyButton, MUIA_Disabled, TRUE);
	DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
	DoMethod(imageInputTextEditor, MUIM_GoActive);
	set(imageView, MUIA_DataTypes_FileName, "");
}
MakeHook(NewImageButtonClickedHook, NewImageButtonClickedFunc);

HOOKPROTONHNONP(DeleteImageButtonClickedFunc, void) {
	DoMethod(imageListObject, MUIM_NList_Remove, MUIV_NList_Remove_Active);
	DoMethod(imageListObject, MUIM_NList_Select, MUIV_NList_Select_All, MUIV_NList_Select_Off, NULL);
	set(openImageButton, MUIA_Disabled, TRUE);
	set(saveImageCopyButton, MUIA_Disabled, TRUE);
	DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
	DoMethod(imageInputTextEditor, MUIM_GoActive);
	set(imageView, MUIA_DataTypes_FileName, "");
	saveImages();
}
MakeHook(DeleteImageButtonClickedHook, DeleteImageButtonClickedFunc);

HOOKPROTONHNONP(CreateImageButtonClickedFunc, void) {
	if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
		set(openImageButton, MUIA_Disabled, TRUE);
		set(saveImageCopyButton, MUIA_Disabled, TRUE);
		set(createImageButton, MUIA_Disabled, TRUE);
		set(newImageButton, MUIA_Disabled, TRUE);
		set(deleteImageButton, MUIA_Disabled, TRUE);
		set(imageInputTextEditor, MUIA_Disabled, TRUE);
		STRPTR text = DoMethod(imageInputTextEditor, MUIM_TextEditor_ExportText);
		// Remove trailing newline characters
		while (text[strlen(text) - 1] == '\n') {
			text[strlen(text) - 1] = '\0';
		}
		STRPTR textUTF_8 = ISO8859_1ToUTF8(text);

		const enum ImageSize imageSize = config.imageModel == DALL_E_2 ? config.imageSizeDallE2 : config.imageSizeDallE3;
		struct json_object *response = postImageCreationRequestToOpenAI(textUTF_8, config.imageModel, imageSize, config.openAiApiKey, config.proxyEnabled, config.proxyHost, config.proxyPort, config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername, config.proxyPassword);
		if (response == NULL) {
			displayError("Error connecting. Please try again.");
			set(createImageButton, MUIA_Disabled, FALSE);
			set(newImageButton, MUIA_Disabled, FALSE);
			set(deleteImageButton, MUIA_Disabled, FALSE);
			set(imageInputTextEditor, MUIA_Disabled, FALSE);
			updateStatusBar("Error", redPen);
			return;
		}
		struct json_object *error;

		if (json_object_object_get_ex(response, "error", &error)) {
			struct json_object *message = json_object_object_get(error, "message");
			STRPTR messageString = json_object_get_string(message);
			displayError(messageString);
			set(createImageButton, MUIA_Disabled, FALSE);
			set(newImageButton, MUIA_Disabled, FALSE);
			set(deleteImageButton, MUIA_Disabled, FALSE);
			set(imageInputTextEditor, MUIA_Disabled, FALSE);
			updateStatusBar("Error", 6);
			json_object_put(response);
			return;
		}

		struct array_list *data = json_object_get_array(json_object_object_get(response, "data"));
		struct json_object *dataObject = (struct json_object *)data->array[0];

		STRPTR url = json_object_get_string(json_object_object_get(dataObject, "url"));

		CreateDir(PROGDIR"images");

		// Generate unique ID for the image
		UBYTE fullPath[30] = "";
		UBYTE id[11] = "";
		CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
		srand(time(NULL));
		for (int i = 0; i < 9; i++) {
			id[i] = idChars[rand() % strlen(idChars)];
		}
		snprintf(fullPath, sizeof(fullPath), PROGDIR"images/%s.png", id);

		downloadFile(url, fullPath, config.proxyEnabled, config.proxyHost, config.proxyPort, config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername, config.proxyPassword);

		json_object_put(response);

		WORD imageWidth, imageHeight;
		switch (imageSize) {
			case IMAGE_SIZE_256x256:
				imageWidth = 256;
				imageHeight = 256;
				break;
			case IMAGE_SIZE_512x512:
				imageWidth = 512;
				imageHeight = 512;
				break;
			case IMAGE_SIZE_1024x1024:
				imageWidth = 1024;
				imageHeight = 1024;
				break;
			case IMAGE_SIZE_1792x1024:
				imageWidth = 1792;
				imageHeight = 1024;
				break;
			case IMAGE_SIZE_1024x1792:
				imageWidth = 1024;
				imageHeight = 1792;
				break;
			default:
				printf("Invalid image size\n");
				imageWidth = 256;
				imageHeight = 256;
				break;
		}

		updateStatusBar("Generating image name...", 7);
		struct Conversation *imageNameConversation = newConversation();
		addTextToConversation(imageNameConversation, text, "user");
		addTextToConversation(imageNameConversation, "generate a short title for this image and don't enclose the title in quotes or prefix the response with anything", "user");
		struct json_object **responses = postChatMessageToOpenAI(imageNameConversation, config.chatModel, config.openAiApiKey, FALSE, config.proxyEnabled, config.proxyHost, config.proxyPort, config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername, config.proxyPassword);
		
		struct GeneratedImage *generatedImage = AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
		if (responses == NULL) {
			displayError("Failed to generate image name. Using ID instead.");
			updateStatusBar("Error", 6);
		} else if (responses[0] != NULL) {
			STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
			formatText(responseString);
			generatedImage->name = AllocVec(strlen(responseString) + 1, MEMF_ANY | MEMF_CLEAR);
			strncpy(generatedImage->name, responseString, strlen(responseString));
			updateStatusBar("Ready", 5);
			json_object_put(responses[0]);
			FreeVec(responses);
		} else {
			generatedImage->name = AllocVec(11, MEMF_ANY | MEMF_CLEAR);
			strncpy(generatedImage->name, id, 10);
			updateStatusBar("Ready", 5);
			if (responses != NULL) {
				FreeVec(responses);
			}
			displayError("Failed to generate image name. Using ID instead.");
		}
		freeConversation(imageNameConversation);

		generatedImage->filePath = AllocVec(strlen(fullPath) + 1, MEMF_ANY | MEMF_CLEAR);
		strncpy(generatedImage->filePath, fullPath, strlen(fullPath));
		generatedImage->prompt = AllocVec(strlen(text) + 1, MEMF_ANY | MEMF_CLEAR);
		strncpy(generatedImage->prompt, text, strlen(text));
		generatedImage->imageModel = config.imageModel;
		generatedImage->width = imageWidth;
		generatedImage->height = imageHeight;
		DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage, MUIV_NList_Insert_Top);
		DoMethod(imageListObject, MUIM_NList_SetActive, 0, NULL);
		currentImage = generatedImage;
		ImageRowClickedFunc();

		set(createImageButton, MUIA_Disabled, FALSE);
		set(newImageButton, MUIA_Disabled, FALSE);
		set(deleteImageButton, MUIA_Disabled, FALSE);

		FreeVec(text);
		FreeVec(textUTF_8);

		saveImages();
	} else {
		displayError("Please enter your OpenAI API key in the Open AI settings in the menu.");
	}
}
MakeHook(CreateImageButtonClickedHook, CreateImageButtonClickedFunc);

HOOKPROTONHNONP(OpenImageButtonClickedFunc, void) {
	// Get screen dimensions
    WORD screenWidth = screen->Width;
    WORD screenHeight = screen->Height;

    // Calculate the maximum size for the image
    LONG maxImageWidth = (screenWidth * 90) / 100; // 90% of screen width
    LONG maxImageHeight = (screenHeight * 90) / 100; // 90% of screen height

    // Variables to hold the new dimensions
    LONG newWidth = 0;
    LONG newHeight = 0;

    if (currentImage != NULL) {
        if (currentImage->width > currentImage->height) {
            // Landscape: fit width and calculate height maintaining aspect ratio
            newWidth = maxImageWidth;
            newHeight = (currentImage->height * maxImageWidth) / currentImage->width;

            // Ensure height does not exceed maximum
            if (newHeight > maxImageHeight) {
                newHeight = maxImageHeight;
                newWidth = (currentImage->width * maxImageHeight) / currentImage->height;
            }
        } else {
            // Portrait or square: fit height and calculate width maintaining aspect ratio
            newHeight = maxImageHeight;
            newWidth = (currentImage->width * maxImageHeight) / currentImage->height;

            // Ensure width does not exceed maximum
            if (newWidth > maxImageWidth) {
                newWidth = maxImageWidth;
                newHeight = (currentImage->height * maxImageWidth) / currentImage->width;
            }
        }

        // Open the image with calculated dimensions
        openImage(currentImage, newWidth, newHeight);
    } else {
        displayError("No image is currently selected.");
    }
}
MakeHook(OpenImageButtonClickedHook, OpenImageButtonClickedFunc);

HOOKPROTONHNONP(SaveImageCopyButtonClickedFunc, void) {
	if (currentImage == NULL) return;
	STRPTR filePath = currentImage->filePath;
	struct FileRequester *fileReq = AllocAslRequestTags(ASL_FileRequest, TAG_END);
	if (fileReq != NULL) {
		if (AslRequestTags(fileReq,
		ASLFR_Window, mainWindow,
		ASLFR_TitleText, "Save Image Copy",
		ASLFR_InitialFile, "image.png",
		ASLFR_InitialDrawer, "SYS:",
		ASLFR_DoSaveMode, TRUE,
		TAG_DONE)) {
			STRPTR savePath = fileReq->fr_Drawer;
			STRPTR saveName = fileReq->fr_File;
			BOOL isRootDirectory = savePath[strlen(savePath) - 1] == ':';
			STRPTR fullPath = AllocVec(strlen(savePath) + strlen(saveName) + 2, MEMF_CLEAR);
			snprintf(fullPath, strlen(savePath) + strlen(saveName) + 2, "%s%s%s", savePath, isRootDirectory ? "" : "/", saveName);
			copyFile(filePath, fullPath);
			FreeVec(fullPath);
		}
		FreeAslRequest(fileReq);
	}
}
MakeHook(SaveImageCopyButtonClickedHook, SaveImageCopyButtonClickedFunc);

HOOKPROTONHNONP(ConfigureForScreenFunc, void) {
	const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
	STRPTR buttonLabelText = AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", greenPen, BUTTON_LABEL_NAMES[NEW_CHAT_BUTTON_LABEL]);
	set(newChatButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", redPen, BUTTON_LABEL_NAMES[DELETE_CHAT_BUTTON_LABEL]);
	set(deleteChatButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", bluePen, BUTTON_LABEL_NAMES[SEND_MESSAGE_BUTTON_LABEL]);
	set(sendMessageButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", greenPen, BUTTON_LABEL_NAMES[NEW_IMAGE_BUTTON_LABEL]);
	set(newImageButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", redPen, BUTTON_LABEL_NAMES[DELETE_IMAGE_BUTTON_LABEL]);
	set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
	snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0", bluePen, BUTTON_LABEL_NAMES[CREATE_IMAGE_BUTTON_LABEL]);
	set(createImageButton, MUIA_Text_Contents, buttonLabelText);
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

	set(openImageButton, MUIA_Disabled, TRUE);
	set(saveImageCopyButton, MUIA_Disabled, TRUE);
	
	updateStatusBar("Ready", greenPen);
}
MakeHook(ConfigureForScreenHook, ConfigureForScreenFunc);

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG createMainWindow() {
	currentConversation = NULL;
	currentImage = NULL;

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
			WindowContents, VGroup,
				Child, RegisterGroup(Pages),
					Child, HGroup,
						Child, VGroup, MUIA_Weight, 30,
							// New chat button
							Child, newChatButton = MUI_MakeObject(MUIO_Button, "",
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							TAG_DONE),
							// Delete chat button
							Child, deleteChatButton = MUI_MakeObject(MUIO_Button, BUTTON_LABEL_NAMES[DELETE_CHAT_BUTTON_LABEL],
								MUIA_Background, MUII_FILL,
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							TAG_DONE),
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
						GroupFrame,
						Child, VGroup,
							// New image button
							Child, newImageButton = MUI_MakeObject(MUIO_Button, "New Image",
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							TAG_DONE),
							// Delete image button
							Child, deleteImageButton = MUI_MakeObject(MUIO_Button, "Delete Image",
								MUIA_CycleChain, TRUE,
								MUIA_InputMode, MUIV_InputMode_RelVerify,
							TAG_DONE),
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
						Child, VGroup, MUIA_Weight, 220,
							// Image view
							#ifdef __MORPHOS__
							Child, imageView = VGroup,
								Child, VSpace(0),
								Child, TextObject,
									TextFrame,
									MUIA_Text_Contents, "\33cImage preview not supported in MorphOS. Click \"Open Image\" to view the image.",
									MUIA_Background, MUII_TextBack,
								End,
								Child, VSpace(0),
							End,
							#else
							Child, imageView = isMUI5 ? DataTypesObject, NULL,TAG_END) : VGroup,
								Child, VSpace(0),
								Child, TextObject,
									TextFrame,
									MUIA_Text_Contents, "\33cMUI version 5 is required for image preview. Click \"Open Image\" to view the image.",
									MUIA_Background, MUII_TextBack,
								End,
								Child, VSpace(0),
							End,
							#endif
							Child, HGroup,
								GroupFrame,
								// Open image button
								Child, openImageButton = MUI_MakeObject(MUIO_Button, "Open Image",
									MUIA_CycleChain, TRUE,
									MUIA_InputMode, MUIV_InputMode_RelVerify,
								TAG_DONE),
								// Save image copy button
								Child, saveImageCopyButton = MUI_MakeObject(MUIO_Button, "Save Image Copy",
									MUIA_CycleChain, TRUE,
									MUIA_InputMode, MUIV_InputMode_RelVerify,
								TAG_DONE),
							End,
							Child, HGroup,
								// Image input text editor
								Child, imageInputTextEditor = TextEditorObject,
									MUIA_Weight, 80,
									MUIA_TextEditor_ReadOnly, FALSE,
									MUIA_TextEditor_TabSize, 4,
									MUIA_TextEditor_Rows, 3,
									MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
								End,
								Child, VGroup,
									MUIA_Weight, 20,
									// Create image button
									Child, createImageButton = MUI_MakeObject(MUIO_Button, "\nCreate Image\n",
										MUIA_Weight, 20,
										MUIA_CycleChain, TRUE,
										MUIA_InputMode, MUIV_InputMode_RelVerify,
									TAG_DONE),
								End,
							End,
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

	loadConversations();
	loadImages();

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
	DoMethod(openImageButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  openImageButton, 2, MUIM_CallHook, &OpenImageButtonClickedHook);
	DoMethod(saveImageCopyButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  saveImageCopyButton, 2, MUIM_CallHook, &SaveImageCopyButtonClickedHook);
	DoMethod(conversationListObject, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook, &ConversationRowClickedHook, MUIV_TriggerValue);
	DoMethod(newImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  newImageButton, 2, MUIM_CallHook, &NewImageButtonClickedHook);
	DoMethod(deleteImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  MUIV_Notify_Application, 3, MUIM_CallHook, &DeleteImageButtonClickedHook, MUIV_TriggerValue);
	DoMethod(imageListObject, MUIM_Notify, MUIA_NList_EntryClick, MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook, &ImageRowClickedHook, MUIV_TriggerValue);
	DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, 
	  MUIV_Notify_Application, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
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
 * @brief Sends a chat message to the OpenAI API and displays the response and speaks it if speech is enabled
 * @details This function sends a chat message to the OpenAI API and displays the response in the chat window. It also speaks the response if speech is enabled.
 * @param void
 * @return void
**/
static void sendChatMessage() {
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

	updateStatusBar("Sending message...", yellowPen);
	set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
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
		if (config.chatSystem != NULL && strlen(config.chatSystem) > 0 &&
			config.chatModel != o1_PREVIEW && config.chatModel != o1_PREVIEW_2024_09_12 &&
			config.chatModel != o1_MINI && config.chatModel != o1_MINI_2024_09_12) {
			addTextToConversation(currentConversation, config.chatSystem, "system");
		}
		responses = postChatMessageToOpenAI(currentConversation, config.chatModel, config.openAiApiKey, TRUE, config.proxyEnabled, config.proxyHost, config.proxyPort, config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername, config.proxyPassword);
		if (responses == NULL) {
			displayError("Could not connect to OpenAI");
			set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
			set(sendMessageButton, MUIA_Disabled, FALSE);
			set(newChatButton, MUIA_Disabled, FALSE);
			set(deleteChatButton, MUIA_Disabled, FALSE);
			FreeVec(receivedMessage);
			return;
		}
		if (config.chatSystem != NULL && strlen(config.chatSystem) > 0 &&
			config.chatModel != o1_PREVIEW && config.chatModel != o1_PREVIEW_2024_09_12 &&
			config.chatModel != o1_MINI && config.chatModel != o1_MINI_2024_09_12) {
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
				set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
				set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
				struct MinNode *lastMessage = RemTail(currentConversation->messages);
				FreeVec(lastMessage);
				if (currentConversation ==
					currentConversation->messages->mlh_TailPred) {
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

	set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);

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
			set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
			addTextToConversation(currentConversation, "generate a short title for this conversation and don't enclose the title in quotes or prefix the response with anything", "user");
			responses = postChatMessageToOpenAI(currentConversation, GPT_4o_MINI, config.openAiApiKey, FALSE, config.proxyEnabled, config.proxyHost, config.proxyPort, config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername, config.proxyPassword);
			struct MinNode *titleRequestNode = RemTail(currentConversation->messages);
			FreeVec(titleRequestNode);
			set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
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
			json_object_put(responses[0]);
			FreeVec(responses);
		}
	}

	updateStatusBar("Ready", greenPen);
	saveConversations();
	
	set(sendMessageButton, MUIA_Disabled, FALSE);
	set(newChatButton, MUIA_Disabled, FALSE);
	set(deleteChatButton, MUIA_Disabled, FALSE);

	FreeVec(text);
	FreeVec(textUTF_8);
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
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
**/
static void displayConversation(struct Conversation *conversation) {
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
static void freeConversation(struct Conversation *conversation) {
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
static struct Conversation* copyConversation(struct Conversation *conversation) {
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
 * Copy a generated image
 * @param generatedImage The generated image to copy
 * @return A pointer to the copied generated image
**/
static struct GeneratedImage* copyGeneratedImage(struct GeneratedImage *generatedImage) {
	struct GeneratedImage *newEntry = AllocVec(sizeof(struct GeneratedImage), MEMF_CLEAR);
	newEntry->name = AllocVec(strlen(generatedImage->name) + 1, MEMF_CLEAR);
	strncpy(newEntry->name, generatedImage->name, strlen(generatedImage->name));
	newEntry->filePath = AllocVec(strlen(generatedImage->filePath) + 1, MEMF_CLEAR);
	strncpy(newEntry->filePath, generatedImage->filePath, strlen(generatedImage->filePath));
	newEntry->prompt = AllocVec(strlen(generatedImage->prompt) + 1, MEMF_CLEAR);
	strncpy(newEntry->prompt, generatedImage->prompt, strlen(generatedImage->prompt));
	newEntry->imageModel = generatedImage->imageModel;
	newEntry->width = generatedImage->width;
	newEntry->height = generatedImage->height;
    return (newEntry);
}

/**
 * Display an error message
 * @param message the message to display
**/
void displayError(STRPTR message) {
	const LONG ERROR_CODE = IoErr();
	if (ERROR_CODE == 0) {
		if (!app || MUI_Request(app, mainWindowObject,
		#ifdef __MORPHOS__
		NULL,
		#else
		 MUIV_Requester_Image_Error, "Error",
		 #endif
		  "*OK", "\33c%s", message) != 0) {
			fprintf(stderr, "%s\n", message);
		} 
	} else {
		if (app) {
			const UBYTE ERROR_BUFFER_LENGTH = 255;
			STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
			if (errorMessage) {
				Fault(ERROR_CODE, message, errorMessage, ERROR_BUFFER_LENGTH);
				MUI_Request(app, mainWindowObject, 
				#ifdef __MORPHOS__
				NULL,
				#else
				 MUIV_Requester_Image_Error, "Error",
				 #endif
				"*OK", "\33c%s", errorMessage);
				updateStatusBar("Error", redPen);
				FreeVec(errorMessage);
			}
		} else {
			PrintFault(ERROR_CODE, message);
		}
	}
}

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG saveConversations() {
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
	BPTR file = Open(PROGDIR"image-history.json", MODE_NEWFILE);
	if (file == 0) {
		displayError("Failed to create image history file. Image history will not be saved.");
		return RETURN_ERROR;
	}

	LONG totalImageCount;
	get(imageListObject, MUIA_NList_Entries, &totalImageCount);

	struct json_object *imagesJsonArray = json_object_new_array();
	for (LONG i = 0; i < totalImageCount; i++) {
		struct json_object *imageJsonObject = json_object_new_object();
		struct GeneratedImage *generatedImage;
		
		DoMethod(imageListObject, MUIM_NList_GetEntry, i, &generatedImage);
		json_object_object_add(imageJsonObject, "name", json_object_new_string(generatedImage->name));
		json_object_object_add(imageJsonObject, "filePath", json_object_new_string(generatedImage->filePath));
		json_object_object_add(imageJsonObject, "prompt", json_object_new_string(generatedImage->prompt));
		json_object_object_add(imageJsonObject, "imageModel", json_object_new_int(generatedImage->imageModel));
		json_object_object_add(imageJsonObject, "width", json_object_new_int(generatedImage->width));
		json_object_object_add(imageJsonObject, "height", json_object_new_int(generatedImage->height));
		json_object_array_add(imagesJsonArray, imageJsonObject);
	}

	STRPTR imagesJsonString = (STRPTR)json_object_to_json_string_ext(imagesJsonArray, JSON_C_TO_STRING_PRETTY);

	if (Write(file, imagesJsonString, strlen(imagesJsonString)) != (LONG)strlen(imagesJsonString)) {
		displayError("Failed to write to image history file. Image history will not be saved.");
		Close(file);
		json_object_put(imagesJsonArray);
		return RETURN_ERROR;
	}

	Close(file);
	json_object_put(imagesJsonArray);
	return RETURN_OK;
}

/**
 * Opens and displays the image with scaling
 * @param image the image to open
 * @param width the width of the image
 * @param height the height of the image
**/ 
void openImage(struct GeneratedImage *image, WORD scaledWidth, WORD scaledHeight) {
	#ifdef __MORPHOS__
	copyFile(image->filePath, "T:tempImage.png");
	Execute("System:Utilities/MultiView T:tempImage.png", NULL, NULL);
	#else
	if (image == NULL) return;
	if (!isMUI5) {
		copyFile(image->filePath, "T:tempImage.png");
		#ifdef __AMIGAOS3__
		Execute("MultiView T:tempImage.png", NULL, NULL);
		#else
		SystemTags("MultiView T:tempImage.png", SYS_Input, NULL, SYS_Output, NULL, TAG_DONE);
		#endif
		return;
	}
	WORD lowestWidth = (screen->Width - 16) < scaledWidth ? (screen->Width - 16) : scaledWidth;
	WORD lowestHeight = screen->Height < scaledHeight ? screen->Height : scaledHeight;

	updateStatusBar("Loading image...", yellowPen);

	set(imageWindowObject, MUIA_Window_Title, image->name);
	set(imageWindowObject, MUIA_Window_Width, lowestWidth);
	set(imageWindowObject, MUIA_Window_Height, lowestHeight);
	set(imageWindowObject, MUIA_Window_Activate, TRUE);
	set(imageWindowObject, MUIA_Window_Screen, screen);
	set(imageWindowObject, MUIA_Window_Open, TRUE);
	set(openImageWindowImageView, MUIA_DataTypes_FileName, image->filePath);

	updateStatusBar("Ready", greenPen);
	#endif
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
			#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
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
			#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
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
		DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage, MUIV_NList_Insert_Top);
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

	updateStatusBar("Copying file...", yellowPen);

	do {
		bytesRead = Read(srcFile, buffer, FILE_BUFFER_SIZE);

		if (bytesRead > 0) {
			bytesWritten = Write(dstFile, buffer, bytesRead);

			if (bytesWritten != bytesRead) {
				updateStatusBar("Ready", greenPen);
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
			updateStatusBar("Ready", greenPen);
			snprintf(errorMessage, ERROR_MESSAGE_BUFFER_SIZE, "Error copying %s to %s", source, destination);
			displayError(errorMessage);
			FreeVec(buffer);
			FreeVec(errorMessage);
			Close(srcFile);
			Close(dstFile);
			return FALSE;
		}
	} while (bytesRead > 0);

	updateStatusBar("Ready", greenPen);
	FreeVec(buffer);
	FreeVec(errorMessage);
	Close(srcFile);
	Close(dstFile);
	return TRUE;
}

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 * 
**/ 
void updateStatusBar(CONST_STRPTR message, const ULONG pen) {
	STRPTR formattedMessage = AllocVec(strlen(message) + 20, MEMF_ANY);
	snprintf(formattedMessage, strlen(message) + 20, "\33P[%lu\33p[2]%s\0", pen, message);
	set(statusBar, MUIA_Text_Contents, formattedMessage);
	FreeVec(formattedMessage);
}