#include <devices/printer.h>
#include <devices/prtbase.h>
#include <devices/trackdisk.h>
#include <json-c/json.h>
#include <mui/BetterString_mcc.h>
#include <mui/Busy_mcc.h>
#include <mui/Guigfx_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "AmigaGPTTextEditor.h"
#include "config.h"
#include "gui.h"
#include "menu.h"
#include "MainWindow.h"
#include <dos/dos.h>

/* Max nesting depth for B/I/U combined. Adjust as needed. */
#define MAX_STYLE_STACK 32

#define CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH 1024 * 100

typedef enum { STYLE_BOLD, STYLE_ITALIC, STYLE_UNDERLINE } StyleType;

/* A small stack to track active styles in the order they were opened. */
typedef struct {
    StyleType stack[MAX_STYLE_STACK];
    int top;
} StyleStack;

struct Window *mainWindow;
Object *mainWindowObject = NULL;
Object *newChatButton;
Object *deleteChatButton;
Object *sendMessageButton;
Object *chatInputTextEditor;
Object *chatOutputListView;
Object *chatOutputTextEditor;
Object *statusBar;
Object *conversationListObject;
Object *loadingBar;
Object *imageInputTextEditor;
Object *createImageButton;
Object *newImageButton;
Object *deleteImageButton;
Object *imageListObject;
Object *imageView;
Object *imageViewGroup;
Object *openImageButton;
Object *saveImageCopyButton;
STRPTR chatOutputTextEditorContents = NULL;
WORD pens[NUMDRIPENS + 1];
struct Conversation *currentConversation = NULL;
struct GeneratedImage *currentImage = NULL;
static STRPTR pages[3] = {NULL};

struct Conversation *newConversation();
static struct Conversation *copyConversation(struct Conversation *conversation);
static struct GeneratedImage *
copyGeneratedImage(struct GeneratedImage *generatedImage);
static void sendChatMessage();
static LONG loadConversations();
static LONG saveConversations();
static LONG loadImages();
static LONG saveImages();
static void openImage(struct GeneratedImage *image, WORD scaledWidth,
                      WORD scaledHeight);
static void initStyleStack(StyleStack *s);
static BOOL pushStyle(StyleStack *s, StyleType style);
static BOOL popStyle(StyleStack *s, StyleType style);
static BOOL isTopStyle(const StyleStack *s, StyleType style);
static void outputStyleOn(STRPTR out, size_t outSize, StyleType style);
static void outputStyleOff(STRPTR out, size_t outSize);
static UBYTE parseMarker(CONST_STRPTR input, size_t pos, size_t len,
                         StyleType *foundStyle);
static PICTURE *generateThumbnail(struct GeneratedImage *image);
static void addMainWindowActions();

HOOKPROTONHNO(ConstructConversationLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    struct Conversation *entry = (struct Conversation *)ncm->entry;
    return (entry);
}
MakeHook(ConstructConversationLI_TextHook, ConstructConversationLI_TextFunc);

HOOKPROTONHNO(DestructConversationLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    if (ndm->entry)
        freeConversation((struct Conversation *)ndm->entry);
}
MakeHook(DestructConversationLI_TextHook, DestructConversationLI_TextFunc);

HOOKPROTONHNO(DisplayConversationLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    struct Conversation *entry = (struct Conversation *)ndm->entry;
    ndm->strings[0] = (STRPTR)entry->name;
}
MakeHook(DisplayConversationLI_TextHook, DisplayConversationLI_TextFunc);

HOOKPROTONHNO(ConstructImageLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    struct GeneratedImage *entry = (struct GeneratedImage *)ncm->entry;
    return (entry);
}
MakeHook(ConstructImageLI_TextHook, ConstructImageLI_TextFunc);

HOOKPROTONHNO(DestructImageLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
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
    struct GeneratedImage *entry = (struct GeneratedImage *)ndm->entry;
    ndm->strings[0] = (STRPTR)entry->name;
}
MakeHook(DisplayImageLI_TextHook, DisplayImageLI_TextFunc);

BOOL thing = FALSE;

HOOKPROTONHNONP(ConversationRowClickedFunc, void) {
    thing = !thing;
    set(chatInputTextEditor, MUIA_TextEditor_FixedFont, TRUE);
    struct Conversation *conversation;
    DoMethod(conversationListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &conversation);
    if (conversation) {
        currentConversation = conversation;
        displayConversation(currentConversation);
    }
}
MakeHook(ConversationRowClickedHook, ConversationRowClickedFunc);

HOOKPROTONHNONP(ImageRowClickedFunc, void) {
    set(imageInputTextEditor, MUIA_Disabled, FALSE);

    struct GeneratedImage *image = NULL;
    DoMethod(imageListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active,
             &image);
    if (image) {
        set(createImageButton, MUIA_Disabled, TRUE);
        set(openImageButton, MUIA_Disabled, FALSE);
        set(saveImageCopyButton, MUIA_Disabled, FALSE);
        if (isAROS) {
            set(imageInputTextEditor, MUIA_String_Contents, image->prompt);
        } else {
            set(imageInputTextEditor, MUIA_TextEditor_Contents, image->prompt);
        }
        currentImage = image;
        DoMethod(imageViewGroup, MUIM_Group_InitChange);
        DoMethod(imageViewGroup, OM_REMMEMBER, imageView);
        MUI_DisposeObject(imageView);
        imageView = GuigfxObject, MUIA_Guigfx_FileName, image->filePath,
        MUIA_Guigfx_Quality, MUIV_Guigfx_Quality_Low, MUIA_Guigfx_ScaleMode,
        NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE, MUIA_Guigfx_Transparency,
        NITRF_MASK, End;
        DoMethod(imageViewGroup, OM_ADDMEMBER, imageView);
        DoMethod(imageViewGroup, MUIM_Group_MoveMember, imageView, 0);
        DoMethod(imageViewGroup, MUIM_Group_ExitChange);
    }
}
MakeHook(ImageRowClickedHook, ImageRowClickedFunc);

HOOKPROTONHNONP(NewChatButtonClickedFunc, void) {
    currentConversation = NULL;
    DoMethod(chatOutputTextEditor, MUIM_NList_Clear);
    DoMethod(chatInputTextEditor, MUIM_GoActive);
}
MakeHook(NewChatButtonClickedHook, NewChatButtonClickedFunc);

HOOKPROTONHNONP(DeleteChatButtonClickedFunc, void) {
    DoMethod(conversationListObject, MUIM_NList_Remove,
             MUIV_NList_Remove_Active);
    currentConversation = NULL;
    DoMethod(chatOutputTextEditor, MUIM_NList_Clear);
    saveConversations();
}
MakeHook(DeleteChatButtonClickedHook, DeleteChatButtonClickedFunc);

HOOKPROTONHNONP(SendMessageButtonClickedFunc, void) {
    if (config.openAiApiKey != NULL && strlen(config.openAiApiKey) > 0) {
        sendChatMessage();
    } else {
        displayError(STRING_ERROR_NO_API_KEY);
    }
}
MakeHook(SendMessageButtonClickedHook, SendMessageButtonClickedFunc);

HOOKPROTONHNONP(NewImageButtonClickedFunc, void) {
    currentImage = NULL;
    set(imageInputTextEditor, MUIA_Disabled, FALSE);
    set(createImageButton, MUIA_Disabled, FALSE);
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    if (isAROS) {
        set(imageInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(imageInputTextEditor, MUIM_GoActive);
    DoMethod(imageViewGroup, MUIM_Group_InitChange);
    DoMethod(imageViewGroup, OM_REMMEMBER, imageView);
    MUI_DisposeObject(imageView);
    imageView = RectangleObject, MUIA_Frame, MUIV_Frame_ImageButton, End;
    DoMethod(imageViewGroup, OM_ADDMEMBER, imageView);
    DoMethod(imageViewGroup, MUIM_Group_MoveMember, imageView, 0);
    DoMethod(imageViewGroup, MUIM_Group_ExitChange);
}
MakeHook(NewImageButtonClickedHook, NewImageButtonClickedFunc);

HOOKPROTONHNONP(DeleteImageButtonClickedFunc, void) {
    DoMethod(imageListObject, MUIM_NList_Remove, MUIV_NList_Remove_Active);
    DoMethod(imageListObject, MUIM_NList_Select, MUIV_NList_Select_All,
             MUIV_NList_Select_Off, NULL);
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    if (isAROS) {
        set(imageInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(imageViewGroup, MUIM_Group_InitChange);
    DoMethod(imageViewGroup, OM_REMMEMBER, imageView);
    MUI_DisposeObject(imageView);
    imageView = RectangleObject, MUIA_Frame, MUIV_Frame_ImageButton, End;
    DoMethod(imageViewGroup, OM_ADDMEMBER, imageView);
    DoMethod(imageViewGroup, MUIM_Group_MoveMember, imageView, 0);
    DoMethod(imageViewGroup, MUIM_Group_ExitChange);
    saveImages();
}
MakeHook(DeleteImageButtonClickedHook, DeleteImageButtonClickedFunc);

HOOKPROTONHNONP(CreateImageButtonClickedFunc, void) {
    if (config.openAiApiKey == NULL || strlen(config.openAiApiKey) == 0) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    set(createImageButton, MUIA_Disabled, TRUE);
    set(newImageButton, MUIA_Disabled, TRUE);
    set(deleteImageButton, MUIA_Disabled, TRUE);
    set(imageInputTextEditor, MUIA_Disabled, TRUE);
    STRPTR text;
    if (isAROS) {
        get(imageInputTextEditor, MUIA_String_Contents, &text);
    } else {
        text = DoMethod(imageInputTextEditor, MUIM_TextEditor_ExportText);
    }
    // Remove trailing newline characters
    while (text[strlen(text) - 1] == '\n') {
        text[strlen(text) - 1] = '\0';
    }

    UTF8 *textUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                        CSA_Source, (Tag)text, TAG_DONE);

    ImageSize imageSize;
    switch (config.imageModel) {
    case DALL_E_2:
        imageSize = config.imageSizeDallE2;
        break;
    case DALL_E_3:
        imageSize = config.imageSizeDallE3;
        break;
    case GPT_IMAGE_1:
        imageSize = config.imageSizeGptImage1;
        break;
    default:
        imageSize = config.imageSizeDallE2;
        break;
    }
    struct json_object *response = postImageCreationRequestToOpenAI(
        textUTF8, config.imageModel, imageSize, config.openAiApiKey,
        config.proxyEnabled, config.proxyHost, config.proxyPort,
        config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername,
        config.proxyPassword);
    CodesetsFreeA(textUTF8, NULL);

    if (response == NULL) {
        displayError(STRING_ERROR_CONNECTION);
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        if (!isAROS) {
            FreeVec(text);
        }
        return;
    }
    struct json_object *error;

    if (json_object_object_get_ex(response, "error", &error) &&
        !json_object_is_type(error, json_type_null)) {
        struct json_object *message = json_object_object_get(error, "message");
        STRPTR messageString = json_object_get_string(message);
        if (messageString != NULL) {
            displayError(messageString);
        } else {
            struct json_object *type = json_object_object_get(error, "type");
            STRPTR typeString = json_object_get_string(type);
            if (typeString != NULL) {
                if (strcmp(typeString, "invalid_request_error") == 0) {
                    displayError(STRING_ERROR_INVALID_REQUEST);
                } else {
                    displayError(typeString);
                }
            }
        }
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        json_object_put(response);
        if (!isAROS) {
            FreeVec(text);
        }
        return;
    }

    struct array_list *data =
        json_object_get_array(json_object_object_get(response, "data"));
    struct json_object *dataObject = (struct json_object *)data->array[0];

    STRPTR b64 =
        json_object_get_string(json_object_object_get(dataObject, "b64_json"));

    LONG data_len;
    UBYTE *imageData = decodeBase64(b64, &data_len);

    CreateDir("AMIGAGPT:images");
    CreateDir("AMIGAGPT:images/thumbnails");

    // Generate unique ID for the image
    UBYTE fullPath[30] = "";
    UBYTE id[11] = "";
    CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand(time(NULL));
    for (UBYTE i = 0; i < 9; i++) {
        id[i] = idChars[rand() % strlen(idChars)];
    }
    snprintf(fullPath, sizeof(fullPath), "AMIGAGPT:images/%s.png", id);

    FILE *file = fopen(fullPath, "wb");
    fwrite(imageData, 1, data_len, file);
    fclose(file);
    FreeVec(imageData);

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
    case IMAGE_SIZE_1024x1536:
        imageWidth = 1024;
        imageHeight = 1536;
        break;
    case IMAGE_SIZE_1536x1024:
        imageWidth = 1536;
        imageHeight = 1024;
        break;
    case IMAGE_SIZE_AUTO:
        imageWidth = 1024;
        imageHeight = 1024;
        break;
    default:
        displayError(STRING_ERROR_INVALID_IMAGE_SIZE);
        imageWidth = 256;
        imageHeight = 256;
        break;
    }

    updateStatusBar(STRING_GENERATING_IMAGE_NAME, 7);
    struct Conversation *imageNameConversation = newConversation();
    addTextToConversation(imageNameConversation, text, "user");
    addTextToConversation(
        imageNameConversation,
        "generate a short title for this image and don't enclose the "
        "title "
        "in quotes or prefix the response with anything",
        "user");
    struct json_object **responses = postChatMessageToOpenAI(
        imageNameConversation, GPT_5_NANO, config.openAiApiKey, FALSE,
        config.proxyEnabled, config.proxyHost, config.proxyPort,
        config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername,
        config.proxyPassword);

    struct GeneratedImage *generatedImage =
        AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
    if (responses == NULL) {
        displayError(STRING_ERROR_GENERATING_IMAGE_NAME);
    } else if (responses[0] != NULL) {
        if (json_object_object_get_ex(responses[0], "error", &error) &&
            !json_object_is_type(error, json_type_null)) {
            set(createImageButton, MUIA_Disabled, FALSE);
            set(newImageButton, MUIA_Disabled, FALSE);
            set(deleteImageButton, MUIA_Disabled, FALSE);
            set(imageInputTextEditor, MUIA_Disabled, FALSE);
            json_object_put(responses[0]);
            FreeVec(responses);
            if (!isAROS) {
                FreeVec(text);
            }
            freeConversation(imageNameConversation);
            return;
        }
        STRPTR responseString = getMessageContentFromJson(responses[0], FALSE);
        generatedImage->name =
            AllocVec(strlen(responseString) + 1, MEMF_ANY | MEMF_CLEAR);
        strncpy(generatedImage->name, responseString, strlen(responseString));
        updateStatusBar(STRING_READY, 5);
        json_object_put(responses[0]);
        FreeVec(responses);
    } else {
        generatedImage->name = AllocVec(11, MEMF_ANY | MEMF_CLEAR);
        strncpy(generatedImage->name, id, 10);
        updateStatusBar(STRING_READY, 5);
        if (responses != NULL) {
            FreeVec(responses);
        }
        displayError("Failed to generate image name. Using ID instead.");
    }
    freeConversation(imageNameConversation);

    generatedImage->filePath =
        AllocVec(strlen(fullPath) + 1, MEMF_ANY | MEMF_CLEAR);
    strncpy(generatedImage->filePath, fullPath, strlen(fullPath));
    generatedImage->prompt = AllocVec(strlen(text) + 1, MEMF_ANY | MEMF_CLEAR);
    strncpy(generatedImage->prompt, text, strlen(text));
    generatedImage->imageModel = config.imageModel;
    generatedImage->width = imageWidth;
    generatedImage->height = imageHeight;
    DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage,
             MUIV_NList_Insert_Top);
    DoMethod(imageListObject, MUIM_NList_SetActive, 0, NULL);
    currentImage = generatedImage;
    ImageRowClickedFunc();

    set(createImageButton, MUIA_Disabled, FALSE);
    set(newImageButton, MUIA_Disabled, FALSE);
    set(deleteImageButton, MUIA_Disabled, FALSE);

    saveImages();

    if (!isAROS) {
        FreeVec(text);
    }
}
MakeHook(CreateImageButtonClickedHook, CreateImageButtonClickedFunc);

HOOKPROTONHNONP(OpenImageButtonClickedFunc, void) {
    // Get screen dimensions from main window
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    WORD screenWidth = currentScreen ? currentScreen->Width : 640;
    WORD screenHeight = currentScreen ? currentScreen->Height : 480;

    // Calculate the maximum size for the image
    LONG maxImageWidth = (screenWidth * 90) / 100;   // 90% of screen width
    LONG maxImageHeight = (screenHeight * 90) / 100; // 90% of screen height

    // Variables to hold the new dimensions
    LONG newWidth = 0;
    LONG newHeight = 0;

    if (currentImage != NULL) {
        if (currentImage->width > currentImage->height) {
            // Landscape: fit width and calculate height maintaining aspect
            // ratio
            newWidth = maxImageWidth;
            newHeight =
                (currentImage->height * maxImageWidth) / currentImage->width;

            // Ensure height does not exceed maximum
            if (newHeight > maxImageHeight) {
                newHeight = maxImageHeight;
                newWidth = (currentImage->width * maxImageHeight) /
                           currentImage->height;
            }
        } else {
            // Portrait or square: fit height and calculate width
            // maintaining aspect ratio
            newHeight = maxImageHeight;
            newWidth =
                (currentImage->width * maxImageHeight) / currentImage->height;

            // Ensure width does not exceed maximum
            if (newWidth > maxImageWidth) {
                newWidth = maxImageWidth;
                newHeight = (currentImage->height * maxImageWidth) /
                            currentImage->width;
            }
        }

        // Open the image with calculated dimensions
        openImage(currentImage, newWidth, newHeight);
    } else {
        displayError(STRING_ERROR_NO_IMAGE_SELECTED);
    }
}
MakeHook(OpenImageButtonClickedHook, OpenImageButtonClickedFunc);

HOOKPROTONHNONP(SaveImageCopyButtonClickedFunc, void) {
    if (currentImage == NULL)
        return;
    STRPTR filePath = currentImage->filePath;
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq != NULL) {
        if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                           STRING_SAVE_IMAGE_COPY, ASLFR_InitialFile,
                           "image.png", ASLFR_InitialDrawer,
                           "SYS:", ASLFR_DoSaveMode, TRUE, TAG_DONE)) {
            STRPTR savePath = fileReq->fr_Drawer;
            STRPTR saveName = fileReq->fr_File;
            UWORD fullPathLength = strlen(savePath) + strlen(saveName) + 2;
            STRPTR fullPath = AllocVec(fullPathLength, MEMF_CLEAR);
            strncpy(fullPath, savePath, strlen(savePath));
            AddPart(fullPath, saveName, fullPathLength);
            copyFile(filePath, fullPath);
            FreeVec(fullPath);
        }
        FreeAslRequest(fileReq);
    }
}
MakeHook(SaveImageCopyButtonClickedHook, SaveImageCopyButtonClickedFunc);

HOOKPROTONHNONP(ConfigureForScreenFunc, void) {
    // Get the current screen to allocate pens
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);

    if (currentScreen) {
        // Release old pens if they exist
        if (redPen)
            ReleasePen(currentScreen->ViewPort.ColorMap, redPen);
        if (greenPen)
            ReleasePen(currentScreen->ViewPort.ColorMap, greenPen);
        if (bluePen)
            ReleasePen(currentScreen->ViewPort.ColorMap, bluePen);
        if (yellowPen)
            ReleasePen(currentScreen->ViewPort.ColorMap, yellowPen);

        // Allocate new pens for current screen
        redPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF, 0,
                               0, OBP_Precision, PRECISION_GUI, TAG_DONE);
        greenPen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0xBBBBBBBB, 0,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        bluePen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0, 0xFFFFFFFF,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        yellowPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF,
                                  0xFFFFFFFF, 0, OBP_Precision, PRECISION_GUI,
                                  TAG_DONE);
    }

    const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
    STRPTR buttonLabelText = AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s\0",
             greenPen, STRING_NEW_CHAT);
    set(newChatButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s\0",
             redPen, STRING_DELETE_CHAT);
    set(deleteChatButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
             bluePen, STRING_SEND);
    set(sendMessageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s\0",
             greenPen, STRING_NEW_IMAGE);
    set(newImageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s\0",
             redPen, STRING_DELETE_IMAGE);
    set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
             bluePen, STRING_CREATE_IMAGE);
    set(createImageButton, MUIA_Text_Contents, buttonLabelText);
    FreeVec(buttonLabelText);
    SetAttrs(mainWindowObject, MUIA_Window_ActiveObject, chatInputTextEditor,
             TAG_DONE);

    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
    set(mainWindowObject, MUIA_Window_Open, TRUE);
    addMenuActions();

    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);

    updateStatusBar(STRING_READY, greenPen);
}
MakeHook(ConfigureForScreenHook, ConfigureForScreenFunc);

/**
 * Initialize the style stack.
 * @param s : the style stack
 */
static void initStyleStack(StyleStack *s) { s->top = -1; }

/**
 * Push a style onto the stack.
 * @param s : the style stack
 * @param style : the style to push
 * @return true if the style was pushed, false if the stack is full
 */
static BOOL pushStyle(StyleStack *s, StyleType style) {
    if (s->top >= (MAX_STYLE_STACK - 1)) {
        return FALSE;
    }
    s->stack[++s->top] = style;
    return TRUE;
}

/**
 * Pop the top style from the stack.
 * @param s : the style stack
 * @param style : the style to pop
 * @return true if the top style was 'style' and was popped, false otherwise
 */
static BOOL popStyle(StyleStack *s, StyleType style) {
    if (s->top < 0) {
        return FALSE;
    }
    if (s->stack[s->top] == style) {
        s->top--;
        return TRUE;
    }
    return FALSE;
}

/**
 * Check if the stack is currently 'style' on top.
 * @return true if stack is currently 'style' on top
 */
static BOOL isTopStyle(const StyleStack *s, StyleType style) {
    if (s->top < 0)
        return FALSE;
    return (s->stack[s->top] == style);
}

/**
 * Output MUI escape codes for turning on/off styles:
 *   \033b = bold on
 *   \033i = italic on
 *   \033u = underline on
 *   \033n = normal (off)
 *
 * @param out : output buffer
 * @param outSize : size of the output buffer
 * @param style : style to turn on
 */
static void outputStyleOn(STRPTR out, size_t outSize, StyleType style) {
    switch (style) {
    case STYLE_BOLD:
        strncat(out, "\033b", outSize - strlen(out) - 1);
        break;
    case STYLE_ITALIC:
        strncat(out, "\033i", outSize - strlen(out) - 1);
        break;
    case STYLE_UNDERLINE:
        strncat(out, "\033u", outSize - strlen(out) - 1);
        break;
    }
}

/*
 * Output MUI escape code to turn off all styles.
 * @param out : output buffer
 * @param outSize : size of the output buffer
 */
static void outputStyleOff(STRPTR out, size_t outSize) {
    /*
       MCC_NList's docs say \033n sets the soft style back to normal,
       thus turning off bold/italic/underline.
    */
    strncat(out, "\033n", outSize - strlen(out) - 1);
}

/*
 * Attempt to parse the next formatting marker:
 *  - "**" => toggles bold
 *  - "*"  => toggles italic
 *  - "__" => toggles underline
 *  - "_"  => toggles italic (some Markdown variants do this),
 *            but we'll skip that unless you truly want single underscore
 *            for italic. We'll assume only * for italic, __ for underline.
 *
 * @param input : the input string
 * @param pos : current position in the input string
 * @param len : length of the input string
 * @param foundStyle : output parameter for the style found
 * @return :
 *   0 if no recognised marker
 *   2 if recognized a 2-char marker (e.g., "**", "__")
 *   1 if recognized a single-char marker (e.g., "*")
 *
 * Also outputs which style it corresponds to in 'foundStyle'.
 */
static UBYTE parseMarker(CONST_STRPTR input, size_t pos, size_t len,
                         StyleType *foundStyle) {
    if (pos >= len)
        return 0;

    // Check for "__" (underline)
    if (pos + 1 < len && input[pos] == '_' && input[pos + 1] == '_') {
        *foundStyle = STYLE_UNDERLINE;
        return 2;
    }
    // Check for "**" (bold)
    if (pos + 1 < len && input[pos] == '*' && input[pos + 1] == '*') {
        *foundStyle = STYLE_BOLD;
        return 2;
    }
    // Check for single "*"
    if (input[pos] == '*') {
        *foundStyle = STYLE_ITALIC;
        return 1;
    }
    return 0;
}

/**
 * Convert a (mini-)Markdown string with possible nesting of
 *   **bold**, *italic*, __underline__
 * to MUI/NList escape codes with a proper stack approach.
 *
 * - Supports nesting, e.g. **bold *italic** inside**
 * - Supports escaping:
 *      \*, \_, \\, so they're inserted literally.
 *
 * @param input the input string
 * @return a newly allocated string. Caller must FreeVec() it.
 */
static STRPTR convertMarkdownFormattingToMUI(CONST_STRPTR input) {
    if (!input)
        return NULL;

    // Heuristic for output expansion: each marker can expand
    // to ~4 chars ("\033b" or "\033n"), so let's be generous.
    size_t inLen = strlen(input);
    size_t outCap = inLen * 6 + 128;
    STRPTR out = AllocVec(outCap, MEMF_ANY | MEMF_CLEAR);
    if (!out)
        return NULL;

    StyleStack styleStack;
    initStyleStack(&styleStack);

    size_t i = 0;
    while (i < inLen) {
        // 1) Check for escapes: "\"
        if (input[i] == '\\') {
            // If we have a next char, output it literally and skip
            if (i + 1 < inLen) {
                // Next char is literal
                size_t outLen = strlen(out);
                if (outLen < outCap - 2) {
                    out[outLen] = input[i + 1];
                    out[outLen + 1] = '\0';
                }
                i += 2;
            } else {
                // Just a trailing backslash alone:
                // output it or ignore, your choice. We'll just ignore it.
                i++;
            }
            continue;
        }

        // 2) Try to parse a formatting marker
        StyleType styleFound;
        UBYTE markerLen = parseMarker(input, i, inLen, &styleFound);
        if (markerLen > 0) {
            // It's either a 1-char or 2-char marker
            if (isTopStyle(&styleStack, styleFound)) {
                // It's a closing marker for the top style
                popStyle(&styleStack, styleFound);
                // Output style off
                outputStyleOff(out, outCap);

                // Now re-enable any style(s) that remain on the stack
                // because \033n turns off everything.
                for (UWORD s = 0; s <= styleStack.top; s++) {
                    outputStyleOn(out, outCap, styleStack.stack[s]);
                }
            } else {
                // It's an opening marker for a new style
                if (pushStyle(&styleStack, styleFound)) {
                    outputStyleOn(out, outCap, styleFound);
                } else {
                    // We've run out of stack space; treat as literal
                    // We'll just insert the raw marker:
                    size_t availableSpace = outCap - strlen(out) - 1;
                    size_t copyLen = (markerLen < availableSpace)
                                         ? markerLen
                                         : availableSpace;

                    UBYTE tempBuf[256];
                    if (copyLen > sizeof(tempBuf) - 1) {
                        copyLen = sizeof(tempBuf) - 1;
                    }

                    memcpy(tempBuf, &input[i], copyLen);
                    tempBuf[copyLen] = '\0';

                    strncat(out, tempBuf, availableSpace);
                }
            }
            i += markerLen;
        } else {
            // 3) Not a marker, just a normal character
            size_t outLen = strlen(out);
            if (outLen < outCap - 2) {
                out[outLen] = input[i];
                out[outLen + 1] = '\0';
            }
            i++;
        }
    }

    // If any styles remain open, we can close them
    while (styleStack.top >= 0) {
        popStyle(&styleStack, styleStack.stack[styleStack.top]);
        outputStyleOff(out, outCap);
        // Re-enable anything that might remain on the stack
        for (int s = 0; s <= styleStack.top; s++) {
            outputStyleOn(out, outCap, styleStack.stack[s]);
        }
    }

    return out;
}

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow() {
    if ((isMUI5 || isMUI39) && createAmigaGPTTextEditor() == RETURN_ERROR) {
        displayError("Could not create custom class.");
    }

    if (mainWindowObject != NULL) {
        MUI_DisposeObject(mainWindowObject);
    }

    if (isMUI5 || isMUI39) {
        chatInputTextEditor = NewObject(
            MUIC_AmigaGPTTextEditor, NULL, TextFrame, MUIA_Background,
            MUII_BACKGROUND, MUIA_ObjectID, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
            MUIA_AmigaGPTTextEditor_SubmitHook, &SendMessageButtonClickedHook,
            isAROS ? TAG_DONE : TAG_SKIP, NULL, MUIA_TextEditor_FixedFont,
            config.fixedWidthFonts, MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4, MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);

        imageInputTextEditor = NewObject(
            MUIC_AmigaGPTTextEditor, NULL, MUIA_Weight, 80,
            MUIA_AmigaGPTTextEditor_SubmitHook, &CreateImageButtonClickedHook,
            isAROS ? TAG_DONE : TAG_SKIP, NULL, MUIA_TextEditor_FixedFont,
            config.fixedWidthFonts, MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4, MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);
    } else {
        chatInputTextEditor = MUI_NewObject(
            isAROS ? MUIC_BetterString : MUIC_TextEditor, TextFrame,
            MUIA_Background, MUII_BACKGROUND, MUIA_ObjectID,
            OBJECT_ID_CHAT_INPUT_TEXT_EDITOR, isAROS ? TAG_DONE : TAG_SKIP,
            NULL, MUIA_TextEditor_FixedFont, config.fixedWidthFonts,
            MUIA_TextEditor_ReadOnly, FALSE, MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3, MUIA_TextEditor_ExportHook,
            MUIV_TextEditor_ExportHook_EMail, TAG_DONE);

        imageInputTextEditor = MUI_NewObject(
            isAROS ? MUIC_BetterString : MUIC_TextEditor, TextFrame,
            MUIA_Background, MUIA_Weight, 80, isAROS ? TAG_DONE : TAG_SKIP,
            NULL, MUIA_TextEditor_FixedFont, config.fixedWidthFonts,
            MUIA_TextEditor_ReadOnly, FALSE, MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3, MUIA_TextEditor_ExportHook,
            MUIV_TextEditor_ExportHook_EMail, TAG_DONE);
    }

    createMenu();

    if (chatOutputTextEditorContents == NULL) {
        chatOutputTextEditorContents = AllocVec(
            CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, MEMF_ANY | MEMF_CLEAR);
    }

    pages[0] = STRING_CHAT_MODE;
    pages[1] = STRING_IMAGE_GENERATION_MODE;

    if ((mainWindowObject = WindowObject,
            MUIA_Window_Title, STRING_APP_NAME,
            MUIA_Window_ID, OBJECT_ID_MAIN_WINDOW,
            MUIA_Window_CloseGadget, TRUE,
            MUIA_Window_SizeGadget, TRUE,
            MUIA_Window_DepthGadget, TRUE,
            MUIA_Window_DragBar, TRUE,
            MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
            MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
            MUIA_Window_Width, MUIV_Window_Width_Screen(90),
            MUIA_Window_Height, MUIV_Window_Height_Screen(90),
            MUIA_Window_Menustrip, menuStrip,
            MUIA_Window_SizeRight, TRUE,
            MUIA_Window_UseBottomBorderScroller, FALSE,
            MUIA_Window_UseRightBorderScroller, FALSE,
            MUIA_Window_UseLeftBorderScroller, FALSE,
            WindowContents, VGroup,
                Child, RegisterGroup(pages),
                    Child, HGroup,
                        Child, VGroup, MUIA_Weight, 30,
                            // New chat button
                            Child, newChatButton = MUI_MakeObject(MUIO_Button, STRING_NEW_CHAT,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Delete chat button
                            Child, deleteChatButton = MUI_MakeObject(MUIO_Button, STRING_DELETE_CHAT,
                                MUIA_Background, MUII_FILL,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Conversation list
                            Child, NListviewObject,
                                MUIA_CycleChain, 1,
                                MUIA_NListview_NList, conversationListObject = NListObject,
                                    MUIA_ContextMenu, NULL,
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
                                Child, chatOutputListView = NListviewObject,
                                MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_None,
                                MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                                MUIA_NListview_NList, chatOutputTextEditor = NFloattextObject,
                                    MUIA_Font, config.fixedWidthFonts ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
                                    MUIA_Frame, MUIV_Frame_Text,
                                    MUIA_ContextMenu, NULL,
                                    MUIA_NFloattext_Text, chatOutputTextEditorContents,
                                    End,
                                End,
                            End,
                            Child, HGroup, MUIA_VertWeight, 20,
                                // Chat input text editor
                                Child, chatInputTextEditor,
                                // Send message button
                                Child, HGroup, MUIA_HorizWeight, 10,
                                    Child, sendMessageButton = MUI_MakeObject(MUIO_Button, STRING_SEND,
                                        MUIA_ObjectID, OBJECT_ID_SEND_MESSAGE_BUTTON,
                                        MUIA_CycleChain, TRUE,
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                    End,
                                End,
                            End,
                        End,
                    End,
                    Child, HGroup,
                        GroupFrame,
                        Child, VGroup,
                            // New image button
                            Child, newImageButton = MUI_MakeObject(MUIO_Button, STRING_NEW_IMAGE,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Delete image button
                            Child, deleteImageButton = MUI_MakeObject(MUIO_Button, STRING_DELETE_IMAGE,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Image list
                            Child, NListviewObject,
                                MUIA_CycleChain, 1,
                                MUIA_NListview_NList, imageListObject = NListObject,
                                    MUIA_ContextMenu, NULL,
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
                        Child, imageViewGroup = VGroup, MUIA_Weight, 220,
                            // Image view
                            Child, imageView = RectangleObject,
                                MUIA_Frame, MUIV_Frame_ImageButton,
                            End,
                            Child, HGroup,
                                // Open image button
                                Child, openImageButton = MUI_MakeObject(MUIO_Button, STRING_OPEN_IMAGE,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                // Save image copy button
                                Child, saveImageCopyButton = MUI_MakeObject(MUIO_Button, STRING_SAVE_IMAGE_COPY,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                            End,
                            Child, HGroup,
                                // Image input text editor
                                Child, imageInputTextEditor,
                                Child, VGroup,
                                    MUIA_Weight, 20,
                                    // Create image button
                                    Child, createImageButton = MUI_MakeObject(MUIO_Button, STRING_CREATE_IMAGE,
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
                Child, statusBar = TextObject,
                    MUIA_Background, MUII_SHADOW,
                    MUIA_Text_Contents, STRING_READY,
                End,
                // Loading bar
                Child, loadingBar = BusyObject, MUIA_VertWeight, 10,
                    MUIA_MaxHeight, 20,
                    MUIA_Busy_Speed, MUIV_Busy_Speed_Off,
                End,
            End,
        End) == NULL) {
        displayError(STRING_ERROR_MAIN_WINDOW);
        return RETURN_ERROR;
    }

    get(mainWindowObject, MUIA_Window, &mainWindow);

    DoMethod(app, OM_ADDMEMBER, mainWindowObject);

    DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_Screen, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &ConfigureForScreenHook);

    addMainWindowActions();
    
    // Open the main window immediately after creation
    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
    set(mainWindowObject, MUIA_Window_Open, TRUE);
    addMenuActions();
    
    // Allocate pens after window is opened
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    if (currentScreen) {
        redPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF, 0,
                               0, OBP_Precision, PRECISION_GUI, TAG_DONE);
        greenPen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0xBBBBBBBB, 0,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        bluePen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0, 0xFFFFFFFF,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        yellowPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF,
                                  0xFFFFFFFF, 0, OBP_Precision, PRECISION_GUI,
                                  TAG_DONE);

        // Now set up the button labels with colors
        const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
        STRPTR buttonLabelText = AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW_CHAT);
        set(newChatButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_CHAT);
        set(deleteChatButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
                 bluePen, STRING_SEND);
        set(sendMessageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW_IMAGE);
        set(newImageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_IMAGE);
        set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
                 bluePen, STRING_CREATE_IMAGE);
        set(createImageButton, MUIA_Text_Contents, buttonLabelText);
        FreeVec(buttonLabelText);

        // Force a layout refresh to ensure buttons display correctly
        DoMethod(mainWindowObject, MUIM_Group_InitChange);
        DoMethod(mainWindowObject, MUIM_Group_ExitChange);
    }
    
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    
    updateStatusBar(STRING_READY, greenPen);

    loadConversations();
    loadImages();

    return RETURN_OK;
}

/**
 * Add actions to the main window
 **/
static void addMainWindowActions() {
    DoMethod(newChatButton, MUIM_Notify, MUIA_Pressed, FALSE, newChatButton, 2,
             MUIM_CallHook, &NewChatButtonClickedHook);
    DoMethod(deleteChatButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 3, MUIM_CallHook,
             &DeleteChatButtonClickedHook, MUIV_TriggerValue);
    DoMethod(sendMessageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             sendMessageButton, 2, MUIM_CallHook,
             &SendMessageButtonClickedHook);
    DoMethod(createImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             createImageButton, 2, MUIM_CallHook,
             &CreateImageButtonClickedHook);
    DoMethod(openImageButton, MUIM_Notify, MUIA_Pressed, FALSE, openImageButton,
             2, MUIM_CallHook, &OpenImageButtonClickedHook);
    DoMethod(saveImageCopyButton, MUIM_Notify, MUIA_Pressed, FALSE,
             saveImageCopyButton, 2, MUIM_CallHook,
             &SaveImageCopyButtonClickedHook);
    DoMethod(conversationListObject, MUIM_Notify, MUIA_NList_EntryClick,
             MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook,
             &ConversationRowClickedHook, MUIV_TriggerValue);
    DoMethod(newImageButton, MUIM_Notify, MUIA_Pressed, FALSE, newImageButton,
             2, MUIM_CallHook, &NewImageButtonClickedHook);
    DoMethod(deleteImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 3, MUIM_CallHook,
             &DeleteImageButtonClickedHook, MUIV_TriggerValue);
    DoMethod(imageListObject, MUIM_Notify, MUIA_NList_EntryClick,
             MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook,
             &ImageRowClickedHook, MUIV_TriggerValue);
    DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);
}

/**
 * @brief Sends a chat message to the OpenAI API and displays the response
 *and speaks it if speech is enabled
 * @details This function sends a chat message to the OpenAI API and
 *displays the response in the chat window. It also speaks the response if
 *speech is enabled.
 **/
static void sendChatMessage() {
    BOOL isNewConversation = FALSE;
    struct json_object **responses;
    if (currentConversation == NULL) {
        isNewConversation = TRUE;
        currentConversation = newConversation();
    }
    set(sendMessageButton, MUIA_Disabled, TRUE);
    set(newChatButton, MUIA_Disabled, TRUE);
    set(deleteChatButton, MUIA_Disabled, TRUE);

    updateStatusBar(STRING_SENDING_MESSAGE, yellowPen);
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
    UTF8 *receivedMessage = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
    STRPTR text;
    if (isAROS) {
        get(chatInputTextEditor, MUIA_String_Contents, &text);
    } else {
        text = DoMethod(chatInputTextEditor, MUIM_TextEditor_ExportText);
    }
    UBYTE userStyleString[] = "\033r\033b\0333";
    UBYTE userAlignment;
    switch (config.userTextAlignment) {
    case ALIGN_LEFT:
        userAlignment = 'l';
        break;
    case ALIGN_CENTER:
        userAlignment = 'c';
        break;
    case ALIGN_RIGHT:
        userAlignment = 'r';
        break;
    }
    userStyleString[1] = userAlignment;
    strncat(chatOutputTextEditorContents, userStyleString,
            strlen(userStyleString));
    size_t currentLength = strlen(chatOutputTextEditorContents);
    for (ULONG i = 0; i < strlen(text); i++) {
        if (currentLength >= CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH - 10)
            break;

        chatOutputTextEditorContents[currentLength++] = text[i];
        chatOutputTextEditorContents[currentLength] = '\0';

        // If it's a newline, add the styling codes after it
        if (text[i] == '\n') {
            strncat(chatOutputTextEditorContents, userStyleString,
                    strlen(userStyleString));
            currentLength = strlen(chatOutputTextEditorContents);
        }
    }

    set(chatOutputTextEditor, MUIA_NFloattext_Text,
        chatOutputTextEditorContents);

    // Remove trailing newline characters
    while (text[strlen(text) - 1] == '\n') {
        text[strlen(text) - 1] = '\0';
    }

    UTF8 *textUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,

                                        CSA_Source, (Tag)text, TAG_DONE);

    addTextToConversation(currentConversation, textUTF8, "user");
    CodesetsFreeA(textUTF8, NULL);

    setConversationSystem(currentConversation, config.chatSystem);

    if (isAROS) {
        set(chatInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(chatInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(chatInputTextEditor, MUIM_GoActive);

    BOOL dataStreamFinished = FALSE;
    ULONG speechIndex = 0;
    UWORD wordNumber = 0;

    strncat(chatOutputTextEditorContents, "\n", 1);

    do {
        responses = postChatMessageToOpenAI(
            currentConversation, config.chatModel, config.openAiApiKey, TRUE,
            config.proxyEnabled, config.proxyHost, config.proxyPort,
            config.proxyUsesSSL, config.proxyRequiresAuth, config.proxyUsername,
            config.proxyPassword);
        if (responses == NULL) {
            displayError(STRING_ERROR_CONNECTING_OPENAI);
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
            set(sendMessageButton, MUIA_Disabled, FALSE);
            set(newChatButton, MUIA_Disabled, FALSE);
            set(deleteChatButton, MUIA_Disabled, FALSE);
            FreeVec(receivedMessage);
            if (!isAROS) {
                FreeVec(text);
            }
            return;
        }

        UWORD responseIndex = 0;

        struct json_object *response;
        while (response = responses[responseIndex++]) {
            struct json_object *error;
            if (json_object_object_get_ex(response, "error", &error) &&
                !json_object_is_type(error, json_type_null)) {
                struct json_object *message =
                    json_object_object_get(error, "message");
                UTF8 *messageString = json_object_get_string(message);
                displayError(messageString);
                set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
                if (isAROS) {
                    set(chatInputTextEditor, MUIA_String_Contents, text);
                } else {
                    set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
                }
                struct Node *lastMessage =
                    RemTail((struct List *)currentConversation->messages);
                FreeVec(lastMessage);
                if (currentConversation ==
                    currentConversation->messages->mlh_TailPred) {
                    currentConversation = NULL;
                    chatOutputTextEditorContents[0] = '\0';
                    set(chatOutputTextEditor, MUIA_NFloattext_Text,
                        chatOutputTextEditorContents);
                } else {
                    displayConversation(currentConversation);
                }
                json_object_put(response);

                set(sendMessageButton, MUIA_Disabled, FALSE);
                set(newChatButton, MUIA_Disabled, FALSE);
                set(deleteChatButton, MUIA_Disabled, FALSE);

                FreeVec(receivedMessage);
                if (!isAROS) {
                    FreeVec(text);
                }
                return;
            }

            UTF8 *contentString = getMessageContentFromJson(response, TRUE);
            if (contentString != NULL) {
                // Text for printing
                if (strlen(contentString) > 0) {
                    STRPTR formattedMessageSystemEncoded =
                        CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)contentString,
                                          CSA_MapForeignChars, TRUE, TAG_DONE);
                    strncat(receivedMessage, contentString,
                            READ_BUFFER_LENGTH - strlen(receivedMessage) -
                                strlen(contentString) - 1);
                    strncat(chatOutputTextEditorContents,
                            formattedMessageSystemEncoded,
                            CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH -
                                strlen(chatOutputTextEditorContents) -
                                strlen(formattedMessageSystemEncoded) - 1);
                    CodesetsFreeA(formattedMessageSystemEncoded, NULL);
                    if (++wordNumber % 50 == 0) {
                        STRPTR formattedContent =
                            convertMarkdownFormattingToMUI(
                                chatOutputTextEditorContents);
                        strncpy(chatOutputTextEditorContents, formattedContent,
                                CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH - 1);
                        FreeVec(formattedContent);
                        set(chatOutputTextEditor, MUIA_NFloattext_Text,
                            chatOutputTextEditorContents);
                        set(chatOutputListView, MUIA_NList_First,
                            MUIV_NList_First_Bottom);
                        if (config.speechEnabled) {
                            // Text for speaking
                            STRPTR unformattedMessageSystemEncoded =
                                CodesetsUTF8ToStr(
                                    CSA_DestCodeset, (Tag)systemCodeset,
                                    CSA_Source, (Tag)receivedMessage,
                                    CSA_MapForeignChars, TRUE, TAG_DONE);
                            if (config.speechSystem != SPEECH_SYSTEM_OPENAI) {
                                speakText(unformattedMessageSystemEncoded +
                                              speechIndex,
                                          NULL, NULL);
                            }
                            speechIndex =
                                strlen(unformattedMessageSystemEncoded);
                            CodesetsFreeA(unformattedMessageSystemEncoded,
                                          NULL);
                        }
                    }
                }
                STRPTR type = json_object_get_string(
                    json_object_object_get(response, "type"));
                if (strcmp(type, "response.completed") == 0) {
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
        addTextToConversation(currentConversation, receivedMessage,
                              "assistant");
        displayConversation(currentConversation);

        if (config.speechEnabled) {
            if (config.speechSystem == SPEECH_SYSTEM_OPENAI) {
                speakText(receivedMessage, NULL, AUDIO_FORMAT_PCM);
            } else {
                STRPTR receivedMessageSystemEncoded = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)receivedMessage, CSA_MapForeignChars, TRUE, TAG_DONE);
                speakText(receivedMessageSystemEncoded + speechIndex, NULL,
                          NULL);
                CodesetsFreeA(receivedMessageSystemEncoded, NULL);
            }
        }
        FreeVec(responses);
        FreeVec(receivedMessage);
        if (!isAROS) {
            FreeVec(text);
        }
        if (isNewConversation) {
            updateStatusBar(STRING_GENERATING_CONVERSATION_TITLE, 7);
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
            addTextToConversation(currentConversation,
                                  "generate a short title for this "
                                  "conversation and don't enclose the title in "
                                  "quotes or prefix the response with anything",
                                  "user");
            setConversationSystem(currentConversation, NULL);
            responses = postChatMessageToOpenAI(
                currentConversation, GPT_5_NANO, config.openAiApiKey, FALSE,
                config.proxyEnabled, config.proxyHost, config.proxyPort,
                config.proxyUsesSSL, config.proxyRequiresAuth,
                config.proxyUsername, config.proxyPassword);
            struct Node *titleRequestNode =
                RemTail((struct List *)currentConversation->messages);
            FreeVec(titleRequestNode);
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
            if (responses == NULL) {
                displayError(STRING_ERROR_CONNECTING_OPENAI);
                set(sendMessageButton, MUIA_Disabled, FALSE);
                set(newChatButton, MUIA_Disabled, FALSE);
                set(deleteChatButton, MUIA_Disabled, FALSE);
                return;
            }
            if (responses[0] != NULL) {
                UTF8 *responseString =
                    getMessageContentFromJson(responses[0], FALSE);
                if (currentConversation->name == NULL) {
                    currentConversation->name =
                        AllocVec(strlen(responseString) + 1, MEMF_CLEAR);
                    strncpy(currentConversation->name, responseString,
                            strlen(responseString));
                }
                DoMethod(conversationListObject, MUIM_NList_InsertSingle,
                         currentConversation, MUIV_NList_Insert_Top);
            }
            json_object_put(responses[0]);
            FreeVec(responses);
        }
    }

    updateStatusBar(STRING_READY, greenPen);
    saveConversations();

    set(sendMessageButton, MUIA_Disabled, FALSE);
    set(newChatButton, MUIA_Disabled, FALSE);
    set(deleteChatButton, MUIA_Disabled, FALSE);
}

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
 **/
void displayConversation(struct Conversation *conversation) {
    if (conversation == NULL) {
        conversation = currentConversation;
    }
    struct ConversationNode *conversationNode;
    chatOutputTextEditorContents[0] = '\0';
    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        if ((strlen(chatOutputTextEditorContents) +
             strlen(conversationNode->content) + 256) > WRITE_BUFFER_LENGTH) {
            displayError(STRING_ERROR_CONVERSATION_MAX_LENGTH_EXCEEDED);
            set(sendMessageButton, MUIA_Disabled, TRUE);
            return;
        }
        if (strcmp(conversationNode->role, "user") == 0) {
            UTF8 *content = conversationNode->content;
            UBYTE userAlignment;
            switch (config.userTextAlignment) {
            case ALIGN_LEFT:
                userAlignment = 'l';
                break;
            case ALIGN_CENTER:
                userAlignment = 'c';
                break;
            case ALIGN_RIGHT:
                userAlignment = 'r';
                break;
            }
            UBYTE userStyleString[] = "\033r\033b\0333";
            userStyleString[1] = userAlignment;
            strncat(chatOutputTextEditorContents, userStyleString,
                    strlen(userStyleString));
            for (ULONG i = 0; i < strlen(content); i++) {
                strncat(chatOutputTextEditorContents, content + i, 1);
                if (content[i] == '\n') {
                    strncat(chatOutputTextEditorContents, userStyleString,
                            strlen(userStyleString));
                }
            }
        } else if (strcmp(conversationNode->role, "assistant") == 0) {
            set(chatOutputListView, MUIA_NFloattext_Align,
                config.assistantTextAlignment);
            UBYTE assistantStyleString[] = "\n\n\0332";
            strncat(chatOutputTextEditorContents, assistantStyleString,
                    strlen(assistantStyleString));
            STRPTR formattedContent =
                convertMarkdownFormattingToMUI(conversationNode->content);
            strncat(chatOutputTextEditorContents, formattedContent,
                    strlen(formattedContent));
            FreeVec(formattedContent);
            const UBYTE messageSeparatorStyleString[] = "\n\n";
            strncat(chatOutputTextEditorContents, messageSeparatorStyleString,
                    strlen(messageSeparatorStyleString));
        }
    }

    STRPTR convertedConversationString = CodesetsUTF8ToStr(
        CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
        (Tag)chatOutputTextEditorContents, CSA_MapForeignChars, TRUE, TAG_DONE);

    snprintf(chatOutputTextEditorContents, WRITE_BUFFER_LENGTH, "%s\0",
             convertedConversationString);

    CodesetsFreeA(convertedConversationString, NULL);

    set(chatOutputTextEditor, MUIA_NFloattext_Text,
        chatOutputTextEditorContents);
    set(chatOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);
}

/**
 * Copy a conversation
 * @param conversation The conversation to copy
 * @return A pointer to the copied conversation
 **/
static struct Conversation *
copyConversation(struct Conversation *conversation) {
    struct Conversation *copy = newConversation();
    struct ConversationNode *conversationNode;
    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        addTextToConversation(copy, conversationNode->content,
                              conversationNode->role);
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
static struct GeneratedImage *
copyGeneratedImage(struct GeneratedImage *generatedImage) {
    struct GeneratedImage *newEntry =
        AllocVec(sizeof(struct GeneratedImage), MEMF_CLEAR);
    newEntry->name = AllocVec(strlen(generatedImage->name) + 1, MEMF_CLEAR);
    strncpy(newEntry->name, generatedImage->name, strlen(generatedImage->name));
    newEntry->filePath =
        AllocVec(strlen(generatedImage->filePath) + 1, MEMF_CLEAR);
    strncpy(newEntry->filePath, generatedImage->filePath,
            strlen(generatedImage->filePath));
    newEntry->prompt = AllocVec(strlen(generatedImage->prompt) + 1, MEMF_CLEAR);
    strncpy(newEntry->prompt, generatedImage->prompt,
            strlen(generatedImage->prompt));
    newEntry->imageModel = generatedImage->imageModel;
    newEntry->width = generatedImage->width;
    newEntry->height = generatedImage->height;
    return (newEntry);
}

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG saveConversations() {
    BPTR file = Open("AMIGAGPT:chat-history.json", MODE_NEWFILE);
    if (file == 0) {
        displayError(STRING_ERROR_CHAT_HISTORY_CREATE);
        return RETURN_ERROR;
    }

    struct json_object *conversationsJsonArray = json_object_new_array();

    LONG totalConversationCount;
    get(conversationListObject, MUIA_NList_Entries, &totalConversationCount);

    for (LONG i = 0; i < totalConversationCount; i++) {
        struct json_object *conversationJsonObject = json_object_new_object();
        struct Conversation *conversation;
        DoMethod(conversationListObject, MUIM_NList_GetEntry, i, &conversation);
        json_object_object_add(conversationJsonObject, "name",
                               json_object_new_string(conversation->name));
        struct json_object *messagesJsonArray = json_object_new_array();
        struct ConversationNode *conversationNode;
        for (conversationNode =
                 (struct ConversationNode *)conversation->messages->mlh_Head;
             conversationNode->node.mln_Succ != NULL;
             conversationNode =
                 (struct ConversationNode *)conversationNode->node.mln_Succ) {
            if (!strcmp(conversationNode->role, "system"))
                continue;
            struct json_object *messageJsonObject = json_object_new_object();
            json_object_object_add(
                messageJsonObject, "role",
                json_object_new_string(conversationNode->role));
            json_object_object_add(
                messageJsonObject, "content",
                json_object_new_string(conversationNode->content));
            json_object_array_add(messagesJsonArray, messageJsonObject);
        }
        json_object_object_add(conversationJsonObject, "messages",
                               messagesJsonArray);
        json_object_array_add(conversationsJsonArray, conversationJsonObject);
    }

    STRPTR conversationsJsonString = (STRPTR)json_object_to_json_string_ext(
        conversationsJsonArray, JSON_C_TO_STRING_PRETTY);

    if (Write(file, conversationsJsonString, strlen(conversationsJsonString)) !=
        (LONG)strlen(conversationsJsonString)) {
        displayError(STRING_ERROR_CHAT_HISTORY_SAVE);
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
    BPTR file = Open("AMIGAGPT:image-history.json", MODE_NEWFILE);
    if (file == 0) {
        displayError(STRING_ERROR_IMAGE_HISTORY_CREATE);
        return RETURN_ERROR;
    }

    LONG totalImageCount;
    get(imageListObject, MUIA_NList_Entries, &totalImageCount);

    struct json_object *imagesJsonArray = json_object_new_array();
    for (LONG i = 0; i < totalImageCount; i++) {
        struct json_object *imageJsonObject = json_object_new_object();
        struct GeneratedImage *generatedImage;

        DoMethod(imageListObject, MUIM_NList_GetEntry, i, &generatedImage);
        json_object_object_add(imageJsonObject, "name",
                               json_object_new_string(generatedImage->name));
        json_object_object_add(
            imageJsonObject, "filePath",
            json_object_new_string(generatedImage->filePath));
        json_object_object_add(imageJsonObject, "prompt",
                               json_object_new_string(generatedImage->prompt));
        json_object_object_add(imageJsonObject, "imageModel",
                               json_object_new_int(generatedImage->imageModel));
        json_object_object_add(imageJsonObject, "width",
                               json_object_new_int(generatedImage->width));
        json_object_object_add(imageJsonObject, "height",
                               json_object_new_int(generatedImage->height));
        json_object_array_add(imagesJsonArray, imageJsonObject);
    }

    STRPTR imagesJsonString = (STRPTR)json_object_to_json_string_ext(
        imagesJsonArray, JSON_C_TO_STRING_PRETTY);

    if (Write(file, imagesJsonString, strlen(imagesJsonString)) !=
        (LONG)strlen(imagesJsonString)) {
        displayError(STRING_ERROR_IMAGE_HISTORY_WRITE);
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
static void openImage(struct GeneratedImage *image, WORD scaledWidth,
                      WORD scaledHeight) {
    if (image == NULL)
        return;

    // Get screen dimensions from main window
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    WORD screenWidth = currentScreen ? currentScreen->Width : 640;
    WORD screenHeight = currentScreen ? currentScreen->Height : 480;

    WORD lowestWidth =
        (screenWidth - 16) < scaledWidth ? (screenWidth - 16) : scaledWidth;
    WORD lowestHeight =
        screenHeight < scaledHeight ? screenHeight : scaledHeight;

    updateStatusBar(STRING_LOADING_IMAGE, yellowPen);

    Object *imageWindowLoadingTextObject = VGroup, Child, VSpace(0), Child,
           TextObject, MUIA_Text_Contents, STRING_LOADING_IMAGE,
           MUIA_Text_PreParse, "\033c", End, Child, VSpace(0), End;
    DoMethod(imageWindowImageViewGroup, OM_ADDMEMBER,
             imageWindowLoadingTextObject);
    DoMethod(imageWindowImageViewGroup, OM_REMMEMBER, imageWindowImageView);
    MUI_DisposeObject(imageWindowImageView);

    set(imageWindowObject, MUIA_Window_Title, image->name);
    set(imageWindowObject, MUIA_Window_Width, lowestWidth);
    set(imageWindowObject, MUIA_Window_Height, lowestHeight);
    set(imageWindowObject, MUIA_Window_Activate, TRUE);
    set(imageWindowObject, MUIA_Window_Open, TRUE);

    DoMethod(imageWindowImageViewGroup, MUIM_Group_InitChange);
    imageWindowImageView = GuigfxObject, MUIA_Guigfx_FileName, image->filePath,
    MUIA_Guigfx_Quality, MUIV_Guigfx_Quality_Best, MUIA_Guigfx_ScaleMode,
    NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE | NISMF_KEEPASPECT_SCREEN,
    MUIA_Guigfx_Transparency, NITRF_MASK, End;
    DoMethod(imageWindowImageViewGroup, OM_ADDMEMBER, imageWindowImageView);
    DoMethod(imageWindowImageViewGroup, OM_REMMEMBER,
             imageWindowLoadingTextObject);
    DoMethod(imageWindowImageViewGroup, MUIM_Group_ExitChange);
    MUI_DisposeObject(imageWindowLoadingTextObject);

    updateStatusBar(STRING_READY, greenPen);
}

/**
 * Load the conversations from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG loadConversations() {
    BPTR file = Open("AMIGAGPT:chat-history.json", MODE_OLDFILE);
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
        displayError(STRING_ERROR_CHAT_HISTORY_READ);
        Close(file);
        FreeVec(conversationsJsonString);
        return RETURN_ERROR;
    }

    Close(file);

    struct json_object *conversationsJsonArray =
        json_tokener_parse(conversationsJsonString);
    if (conversationsJsonArray == NULL) {
        if (Rename("AMIGAGPT:chat-history.json",
                   "AMIGAGPT:chat-history.json.bak")) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_BACKUP);
        } else if (copyFile("AMIGAGPT:chat-history.json",
                            "RAM:chat-history.json")) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_BACKUP_RAM);
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
            if (!DeleteFile("AMIGAGPT:chat-history.json")) {
#else
            if (!Delete("AMIGAGPT:chat-history.json")) {
#endif
                displayError(STRING_ERROR_CHAT_HISTORY_DELETE);
            }
        }

        FreeVec(conversationsJsonString);
        return RETURN_ERROR;
    }

    for (UWORD i = 0; i < json_object_array_length(conversationsJsonArray);
         i++) {
        struct json_object *conversationJsonObject =
            json_object_array_get_idx(conversationsJsonArray, i);
        struct json_object *conversationNameJsonObject;
        if (!json_object_object_get_ex(conversationJsonObject, "name",
                                       &conversationNameJsonObject)) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
            FreeVec(conversationsJsonString);
            json_object_put(conversationsJsonArray);
            return RETURN_ERROR;
        }

        UTF8 *conversationName =
            json_object_get_string(conversationNameJsonObject);

        struct json_object *messagesJsonArray;
        if (!json_object_object_get_ex(conversationJsonObject, "messages",
                                       &messagesJsonArray)) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
            FreeVec(conversationsJsonString);
            json_object_put(conversationsJsonArray);
            return RETURN_ERROR;
        }

        struct Conversation *conversation = newConversation();
        conversation->name =
            AllocVec(strlen(conversationName) + 1, MEMF_ANY | MEMF_CLEAR);
        strncpy(conversation->name, conversationName, strlen(conversationName));

        set(conversationListObject, MUIA_NList_Quiet, TRUE);

        for (UWORD j = 0; j < json_object_array_length(messagesJsonArray);
             j++) {
            struct json_object *messageJsonObject =
                json_object_array_get_idx(messagesJsonArray, j);
            struct json_object *roleJsonObject;
            if (!json_object_object_get_ex(messageJsonObject, "role",
                                           &roleJsonObject)) {
                displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
                FreeVec(conversationsJsonString);
                json_object_put(conversationsJsonArray);
                return RETURN_ERROR;
            }
            STRPTR role = json_object_get_string(roleJsonObject);

            struct json_object *contentJsonObject =
                json_object_array_get_idx(messagesJsonArray, j);
            if (!json_object_object_get_ex(messageJsonObject, "content",
                                           &contentJsonObject)) {
                displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
                FreeVec(conversationsJsonString);
                json_object_put(conversationsJsonArray);
                return RETURN_ERROR;
            }
            UTF8 *content = json_object_get_string(contentJsonObject);
            addTextToConversation(conversation, content, role);
        }
        DoMethod(conversationListObject, MUIM_NList_InsertSingle, conversation,
                 MUIV_NList_Insert_Top);
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
    BPTR file = Open("AMIGAGPT:image-history.json", MODE_OLDFILE);
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
        displayError(STRING_ERROR_IMAGE_HISTORY_READ);
        Close(file);
        FreeVec(imagesJsonString);
        return RETURN_ERROR;
    }

    Close(file);

    struct json_object *imagesJsonArray = json_tokener_parse(imagesJsonString);
    if (imagesJsonArray == NULL) {
        if (Rename("AMIGAGPT:image-history.json",
                   "AMIGAGPT:image-history.json.bak")) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_BACKUP);
        } else if (copyFile("AMIGAGPT:image-history.json",
                            "RAM:image-history.json")) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_BACKUP_RAM);
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
            if (!DeleteFile("AMIGAGPT:image-history.json")) {
#else
            if (!Delete("AMIGAGPT:image-history.json")) {
#endif
                displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            }
        }

        FreeVec(imagesJsonString);
        return RETURN_ERROR;
    }

    for (UWORD i = 0; i < json_object_array_length(imagesJsonArray); i++) {
        struct json_object *imageJsonObject =
            json_object_array_get_idx(imagesJsonArray, i);
        struct json_object *imageNameJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "name",
                                       &imageNameJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        STRPTR imageName = json_object_get_string(imageNameJsonObject);

        struct json_object *imageFilePathJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "filePath",
                                       &imageFilePathJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        STRPTR imageFilePath = json_object_get_string(imageFilePathJsonObject);

        struct json_object *imagePromptJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "prompt",
                                       &imagePromptJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        STRPTR imagePrompt = json_object_get_string(imagePromptJsonObject);

        struct json_object *imageModelJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "imageModel",
                                       &imageModelJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        ImageModel imageModel = json_object_get_int(imageModelJsonObject);

        struct json_object *imageWidthJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "width",
                                       &imageWidthJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        UWORD imageWidth = (WORD)json_object_get_int(imageWidthJsonObject);

        struct json_object *imageHeightJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "height",
                                       &imageHeightJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        UWORD imageHeight = (WORD)json_object_get_int(imageHeightJsonObject);

        struct GeneratedImage *generatedImage =
            AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
        generatedImage->name = AllocVec(strlen(imageName) + 1, MEMF_ANY);
        strcpy(generatedImage->name, imageName);
        generatedImage->filePath =
            AllocVec(strlen(imageFilePath) + 1, MEMF_ANY);
        strcpy(generatedImage->filePath, imageFilePath);
        generatedImage->prompt = AllocVec(strlen(imagePrompt) + 1, MEMF_ANY);
        strcpy(generatedImage->prompt, imagePrompt);
        generatedImage->imageModel = imageModel;
        generatedImage->width = imageWidth;
        generatedImage->height = imageHeight;
        DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage,
                 MUIV_NList_Insert_Top);
    }

    json_object_put(imagesJsonArray);
    return RETURN_OK;
}

/**
 * Print the conversation text to the printer
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG printConversation() {
    BPTR printerFile;
    LONG result = RETURN_ERROR;

    printerFile = Open("PRT:", MODE_NEWFILE);
    if (printerFile) {
        STRPTR printerText =
            AllocVec(strlen(READ_BUFFER_LENGTH) + 1, MEMF_ANY | MEMF_CLEAR);
        struct ConversationNode *conversationNode;

        for (conversationNode = (struct ConversationNode *)
                                    currentConversation->messages->mlh_Head;
             conversationNode->node.mln_Succ != NULL;
             conversationNode =
                 (struct ConversationNode *)conversationNode->node.mln_Succ) {
            UTF8 *content = conversationNode->content;
            if (strcmp(conversationNode->role, "user") == 0) {
                Write(printerFile, "*******************\n", -1);
                Write(printerFile, "User:\n\n", -1);

                STRPTR convertedConversationString = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)content, CSA_MapForeignChars, TRUE, TAG_DONE);

                Write(printerFile, convertedConversationString, -1);

                CodesetsFreeA(convertedConversationString, NULL);
            } else if (strcmp(conversationNode->role, "assistant") == 0) {
                Write(printerFile, "\n\n", -1);
                Write(printerFile, "*******************\n", -1);
                Write(printerFile, "AmigaGPT:\n\n", -1);

                STRPTR convertedConversationString = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)content, CSA_MapForeignChars, TRUE, TAG_DONE);

                Write(printerFile, convertedConversationString, -1);

                CodesetsFreeA(convertedConversationString, NULL);

                Write(printerFile, "\n\n", -1);
            }
        }

        FreeVec(printerText);
        Close(printerFile);
        result = RETURN_OK;
    }

    return RETURN_OK;
}