#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
#include "amiga_compiler.h"
#endif
#include <json-c/json.h>
#include <intuition/icclass.h>
#include <libraries/codesets.h>
#include <libraries/mui.h>
#include <mui/Aboutbox_mcc.h>
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
#include "AboutAmigaGPTWindow.h"
#include "AmigaGPTTextEditor.h"
#include "APIKeyRequesterWindow.h"
#include "ARexx.h"
#include "ChatSystemRequesterWindow.h"
#include "CustomServerSettingsRequesterWindow.h"
#include "config.h"
#include "ElevenLabsSettingsRequesterWindow.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "VoiceInstructionsRequesterWindow.h"
#include "version.h"

#ifdef __AMIGAOS4__
struct MUIMasterIFace *IMUIMaster;
struct CodesetsIFace *ICodesets;
#endif

#ifdef __MORPHOS__
static ULONG muiDispatcherGate(void) {
    ULONG (*dispatcher)(struct IClass *, Object *, Msg);

    struct IClass *cl = (struct IClass *)REG_A0;
    Object *obj = (Object *)REG_A2;
    Msg msg = (Msg)REG_A1;

    dispatcher = (ULONG (*)(struct IClass *, Object *, Msg))cl->cl_UserData;

    return dispatcher(cl, obj, msg);
}

struct EmulLibEntry muiDispatcherEntry = {TRAP_LIB, 0,
                                          (void (*)(void))muiDispatcherGate};
#endif

struct Library *MUIMasterBase;
struct Library *CodesetsBase;
Object *app = NULL;
ULONG redPen = 0, greenPen = 0, bluePen = 0, yellowPen = 0;
Object *imageWindowObject;
Object *imageWindowImageView;
Object *imageWindowImageViewGroup;
BOOL isMUI5;
BOOL isMUI39;
BOOL isAROS;
struct codeset *systemCodeset;

static BOOL checkMUICustomClassInstalled();
static void closeGUILibraries();

static CONST_STRPTR USED_CLASSES[] = {
    MUIC_Aboutbox,   MUIC_Busy,       MUIC_NList,
    MUIC_NListview,  MUIC_TextEditor, MUIC_BetterString,
    MUIC_NFloattext, MUIC_Guigfx,     NULL};

/**
 * Open the libraries needed for the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG openGUILibraries() {
    if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL) {
        displayError(STRING_ERROR_MUI_LIB_OPEN);
        return RETURN_ERROR;
    }
#ifdef __AMIGAOS4__
    if ((IMUIMaster = (struct MUIIFace *)GetInterface(MUIMasterBase, "main", 1,
                                                      NULL)) == NULL) {
        displayError(STRING_ERROR_MUI_INTERFACE_OPEN);
        return RETURN_ERROR;
    }
#endif

    if ((CodesetsBase = OpenLibrary("codesets.library", 6)) == NULL) {
        displayError(STRING_ERROR_CODESETS_LIB_OPEN);
        return RETURN_ERROR;
    }
#ifdef __AMIGAOS4__
    if ((ICodesets = (struct CodesetsIFace *)GetInterface(CodesetsBase, "main",
                                                          1, NULL)) == NULL) {
        displayError(STRING_ERROR_CODESETS_INTERFACE_OPEN);
        return RETURN_ERROR;
    }
#endif

    if (!(systemCodeset = CodesetsFindA(NULL, NULL))) {
        displayError(STRING_ERROR_CODESETS_SYSTEM);
        return RETURN_ERROR;
    }

    isMUI5 = MUIMasterBase->lib_Version >= 21;
    isMUI39 = MUIMasterBase->lib_Version == 20;
    return RETURN_OK;
}

/**
 * Close the libraries used by the GUI
 **/
static void closeGUILibraries() {
#ifdef __AMIGAOS4__
    DropInterface((struct Interface *)IMUIMaster);
    DropInterface((struct Interface *)ICodesets);
#endif
    CloseLibrary(MUIMasterBase);
    CloseLibrary(CodesetsBase);
}

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG initVideo() {
    if (openGUILibraries() == RETURN_ERROR) {
        return RETURN_ERROR;
    }

#ifdef DAEMON
    if (!(app = ApplicationObject, MUIA_Application_Base, "AMIGAGPTD",
          MUIA_Application_Title, "AmigaGPT Daemon", MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "2023-2025 Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Description, "AmigaGPT Daemon",
          MUIA_Application_Version,
          "$VER: AmigaGPTD " APP_VERSION " (" BUILD_DATE ")",
          MUIA_Application_SingleTask, TRUE, MUIA_Application_Commands,
          arexxList, MUIA_Application_UseRexx, TRUE, End)) {
        fprintf(stderr, "Failed to create MUI application. There may be "
                        "another instance of AmigaGPTD running.");
        return RETURN_ERROR;
    }
#else
    if (!checkMUICustomClassInstalled()) {
        displayError(STRING_ERROR_MUI_CUSTOM_CLASSES_INSTALLED);
        return RETURN_ERROR;
    }

    if (createAPIKeyRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createChatSystemRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createCustomServerSettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createProxySettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createVoiceInstructionsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createElevenLabsSettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (!(app = ApplicationObject, MUIA_Application_Base, "AMIGAGPT",
          MUIA_Application_Title, STRING_APP_NAME, MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "2023-2025 Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Description, STRING_APP_DESCRIPTION,
          MUIA_Application_Version,
          "$VER: AmigaGPT " APP_VERSION " (" BUILD_DATE ")",
          MUIA_Application_UsedClasses, USED_CLASSES, MUIA_Application_HelpFile,
          "AMIGAGPT:AmigaGPT.guide", MUIA_Application_SingleTask, TRUE,
          MUIA_Application_Commands, arexxList, MUIA_Application_UseRexx, TRUE,
          SubWindow, apiKeyRequesterWindowObject, SubWindow,
          chatSystemRequesterWindowObject, SubWindow,
          customServerSettingsRequesterWindowObject, SubWindow,
          proxySettingsRequesterWindowObject, SubWindow,
          voiceInstructionsRequesterWindowObject, SubWindow,
          elevenLabsSettingsRequesterWindowObject, SubWindow,
          imageWindowObject = WindowObject, MUIA_Window_Title, STRING_IMAGE,
          MUIA_Window_ID, OBJECT_ID_IMAGE_WINDOW, MUIA_Window_Width, 320,
          MUIA_Window_Height, 240, MUIA_Window_CloseGadget, TRUE,
          MUIA_Window_SizeGadget, TRUE, MUIA_Window_DepthGadget, TRUE,
          MUIA_Window_DragBar, TRUE, MUIA_Window_LeftEdge,
          MUIV_Window_LeftEdge_Centered, MUIA_Window_TopEdge,
          MUIV_Window_TopEdge_Centered, MUIA_Window_SizeRight, TRUE,
          MUIA_Window_UseBottomBorderScroller, FALSE,
          MUIA_Window_UseRightBorderScroller, FALSE,
          MUIA_Window_UseLeftBorderScroller, FALSE, WindowContents,
          imageWindowImageViewGroup = VGroup, Child,
          imageWindowImageView = RectangleObject, MUIA_Frame,
          MUIV_Frame_ImageButton, End, End, End, End)) {
        displayError(STRING_ERROR_APP_CREATE);
        return RETURN_ERROR;
    }

    DoMethod(imageWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    if (createAboutAmigaGPTWindow() == RETURN_OK)
        DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);

    if (createMainWindow() == RETURN_ERROR)
        return RETURN_ERROR;

#endif

    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);

    return RETURN_OK;
}

/**
 * Check if the MUI custom classes are available
 * @return TRUE if the classes are available, FALSE if not
 **/
static BOOL checkMUICustomClassInstalled() {
    BOOL hasAllClasses = TRUE;
    for (int i = 0; USED_CLASSES[i] != NULL; i++) {
        if (!isMUI5 && !strcmp(USED_CLASSES[i], MUIC_Aboutbox) != 0)
            continue;
        if (!isAROS && !strcmp(USED_CLASSES[i], MUIC_BetterString) != 0)
            continue;
        if (!MUI_GetClass(USED_CLASSES[i])) {
            displayError(STRING_ERROR_MUI_CUSTOM_CLASS_NOT_FOUND);
            putchar(':');
            displayError(USED_CLASSES[i]);
            hasAllClasses = FALSE;
        }
    }
    return hasAllClasses;
}

/**
 * Start the main run loop of the GUI
 **/
void startGUIRunLoop() {
    ULONG signals;
    BOOL running = TRUE;

    while (running) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

        switch (id) {
        case MUIV_Application_ReturnID_Quit: {
            running = FALSE;
            break;
        }
#ifndef DAEMON
        // Must be called here because calling via a hook causes a crash
        case APP_ID_PRINT:
            printConversation();
            break;
#endif
        default:
            break;
        }
        if (running && signals)
            signals = Wait(signals | SIGBREAKF_CTRL_C);
        if (signals & SIGBREAKF_CTRL_C)
            running = FALSE;
    }
}

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 *
 **/
void updateStatusBar(CONST_STRPTR message, const ULONG pen) {
#ifndef DAEMON
    STRPTR formattedMessage = AllocVec(strlen(message) + 32, MEMF_ANY);
    snprintf(formattedMessage, strlen(message) + 32, "\33P[%lu]  %s\t\0", pen,
             message);
    set(statusBar, MUIA_Text_Contents, formattedMessage);
    FreeVec(formattedMessage);
#else
    printf("AmigaGPTD Status: %s\n", message);
#endif
}

/**
 * Copies a file from one location to another
 * @param source The source file to copy
 * @param destination The destination to copy the file to
 * @return TRUE if the file was copied successfully, FALSE otherwise
 **/
BOOL copyFile(STRPTR source, STRPTR destination) {
    const UWORD FILE_BUFFER_SIZE = 4096;
    BPTR srcFile, dstFile;
    LONG bytesRead, bytesWritten;
    APTR buffer = AllocVec(FILE_BUFFER_SIZE, MEMF_ANY);

    if (!(srcFile = Open(source, MODE_OLDFILE))) {
        displayError(STRING_ERROR_FILE_COPY_OPEN);
        FreeVec(buffer);
        return FALSE;
    }

    if (!(dstFile = Open(destination, MODE_NEWFILE))) {
        displayError(STRING_ERROR_FILE_COPY_CREATE);
        FreeVec(buffer);
        Close(srcFile);
        return FALSE;
    }

    updateStatusBar(STRING_COPYING_FILE, yellowPen);

    do {
        bytesRead = Read(srcFile, buffer, FILE_BUFFER_SIZE);

        if (bytesRead > 0) {
            bytesWritten = Write(dstFile, buffer, bytesRead);

            if (bytesWritten != bytesRead) {
                updateStatusBar(STRING_READY, greenPen);
                displayError(STRING_ERROR_FILE_COPY);
                FreeVec(buffer);
                Close(srcFile);
                Close(dstFile);
                return FALSE;
            }
        } else if (bytesRead < 0) {
            updateStatusBar(STRING_READY, greenPen);
            displayError(STRING_ERROR_FILE_COPY);
            FreeVec(buffer);
            Close(srcFile);
            Close(dstFile);
            return FALSE;
        }
    } while (bytesRead > 0);

    updateStatusBar(STRING_READY, greenPen);
    FreeVec(buffer);
    Close(srcFile);
    Close(dstFile);
    return TRUE;
}

/**
 * Display an error message
 * @param message the message to display
 **/
void displayError(STRPTR message) {
#ifndef DAEMON
    const UBYTE appName[] = "AmigaGPT";
    if (app) {
        updateStatusBar(STRING_ERROR, redPen);
    }
#else
    const UBYTE appName[] = "AmigaGPTD";
#endif
    UBYTE *errorTitle = AllocVec(strlen(appName) + strlen(STRING_ERROR) + 2,
                                 MEMF_ANY | MEMF_CLEAR);
    snprintf(errorTitle, strlen(appName) + strlen(STRING_ERROR) + 2, "%s %s",
             appName, STRING_ERROR);
    const UBYTE ERROR_BUFFER_LENGTH = 255;
    STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
    CONST_STRPTR okString =
        AllocVec(strlen(STRING_OK) + 2, MEMF_ANY | MEMF_CLEAR);
    snprintf(okString, strlen(STRING_OK) + 2, "*%s", STRING_OK);
    const LONG ERROR_CODE = IoErr();
    if (ERROR_CODE == 0) {
#ifndef DAEMON
        if (!app || MUI_Request(app, mainWindowObject,
#ifdef __MORPHOS__
                                NULL,
#else
                                MUIV_Requester_Image_Error,
#endif
                                errorTitle, okString, "\33c%s", message) != 0) {
#endif
            struct EasyStruct errorES = {sizeof(struct EasyStruct), 0,
                                         errorTitle, message, STRING_OK};
            EasyRequest(NULL, &errorES, NULL, NULL);
#ifndef DAEMON
        }
#endif
    } else {
        STRPTR errorDescription = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY);
        Fault(ERROR_CODE, NULL, errorDescription, ERROR_BUFFER_LENGTH);
        snprintf(errorMessage, ERROR_BUFFER_LENGTH, "%s: %s\n\n%s\0",
                 errorTitle, errorDescription, message);
        if (app) {
#ifndef DAEMON
            MUI_Request(app, mainWindowObject,
#ifdef __MORPHOS__
                        NULL,
#else
                        MUIV_Requester_Image_Error,
#endif
                        errorTitle, okString, "\33c%s", errorMessage);
        } else {
#endif
            struct EasyStruct errorES = {sizeof(struct EasyStruct), 0,
                                         STRING_ERROR, errorMessage, STRING_OK};
            EasyRequest(NULL, &errorES, NULL, NULL);
        }
        FreeVec(errorDescription);
    }
    FreeVec(errorMessage);
    FreeVec(okString);
}

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
 **/
struct Conversation *newConversation() {
    struct Conversation *conversation =
        AllocVec(sizeof(struct Conversation), MEMF_CLEAR);
    struct MinList *messages = AllocVec(sizeof(struct MinList), MEMF_CLEAR);

    // NewMinList(conversation); // This is what makes us require
    // exec.library 45. Replace with the following:
    if (messages) {
        messages->mlh_Tail = 0;
        messages->mlh_Head = (struct MinNode *)&messages->mlh_Tail;
        messages->mlh_TailPred = (struct MinNode *)&messages->mlh_Head;
    }

    conversation->messages = messages;
    conversation->name = NULL;
    conversation->system = NULL;

    return conversation;
}

/**
 * Sets the system of the conversation
 * @param conversation the conversation to set the system of
 * @param system the system to set
 **/
void setConversationSystem(struct Conversation *conversation,
                           CONST_STRPTR system) {
    if (conversation->system != NULL) {
        CodesetsFreeA(conversation->system, NULL);
    }
    if (system == NULL || strlen(system) == 0) {
        conversation->system = NULL;
        return;
    }
    UTF8 *systemUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,

                                          CSA_Source, (Tag)system, TAG_DONE);
    conversation->system = systemUTF8;
}

/**
 * Get the message content from the JSON response from OpenAI
 * @param json the JSON response from OpenAI
 * @param stream whether the response is a stream or not
 * @param retainJSONFormat whether to retain the JSON format of the message
 * string
 * @param apiEndpoint the API endpoint to use
 * @return a pointer to a new UTF8 string containing the message content --
 * If it found role in the JSON instead of content then return an empty string
 **/
UTF8 *getMessageContentFromJson(struct json_object *json, BOOL stream,
                                BOOL retainJSONFormat,
                                APIEndpoint apiEndpoint) {
    if (json == NULL)
        return NULL;
    if (stream) {
        struct json_object *type = json_object_object_get(json, "type");
        UTF8 *typeStr = json_object_get_string(type);
        if (strcmp(typeStr, "response.output_text.delta") == 0) {
            struct json_object *text = json_object_object_get(json, "delta");
            return json_object_get_string(text);
        } else {
            return "";
        }
    } else {
        struct json_object *text;
        if (apiEndpoint == API_ENDPOINT_RESPONSES) {
            struct json_object *outputArray =
                json_object_object_get(json, "output");
            struct json_object *output = NULL;

            int arrayLength = json_object_array_length(outputArray);
            for (int i = 0; i < arrayLength; i++) {
                struct json_object *currentOutput =
                    json_object_array_get_idx(outputArray, i);
                struct json_object *typeObj =
                    json_object_object_get(currentOutput, "type");
                if (typeObj != NULL) {
                    const char *typeStr = json_object_get_string(typeObj);
                    if (strcmp(typeStr, "message") == 0) {
                        output = currentOutput;
                        break;
                    }
                }
            }

            if (output == NULL) {
                return "";
            }

            struct json_object *contentArray =
                json_object_object_get(output, "content");
            struct json_object *content =
                json_object_array_get_idx(contentArray, 0);
            text = json_object_object_get(content, "text");
        } else {
            struct json_object *contentArray =
                json_object_object_get(json, "choices");
            struct json_object *content =
                json_object_array_get_idx(contentArray, 0);
            struct json_object *message =
                json_object_object_get(content, "message");
            text = json_object_object_get(message, "content");
        }

        UTF8 *textStr;
        if (retainJSONFormat) {
            textStr = json_object_to_json_string_ext(
                text, JSON_C_TO_STRING_NOSLASHESCAPE);
            // remove the enclosing quotes
            textStr++;
            textStr[strlen(textStr) - 1] = '\0';
        } else {
            textStr = json_object_get_string(text);
        }
        return textStr;
    }
}

/**
 * Add a block of text to the conversation list
 * @param conversation The conversation to add the text to
 * @param text The text to add to the conversation
 * @param role The role of the text (user or assistant)
 **/
void addTextToConversation(struct Conversation *conversation, UTF8 *text,
                           STRPTR role) {
    struct ConversationNode *conversationNode =
        AllocVec(sizeof(struct ConversationNode), MEMF_CLEAR);
    if (conversationNode == NULL) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        return;
    }
    strncpy(conversationNode->role, role, sizeof(conversationNode->role) - 1);
    conversationNode->role[sizeof(conversationNode->role) - 1] = '\0';
    conversationNode->content = AllocVec(strlen(text) + 1, MEMF_CLEAR);
    strncpy(conversationNode->content, text, strlen(text));
    AddTail((struct List *)conversation->messages,
            (struct Node *)conversationNode);
}

/**
 * Free the conversation
 * @param conversation The conversation to free
 **/
void freeConversation(struct Conversation *conversation) {
    struct ConversationNode *conversationNode;
    while ((conversationNode = (struct ConversationNode *)RemHead(
                (struct List *)conversation->messages)) != NULL) {
        FreeVec(conversationNode->content);
        FreeVec(conversationNode);
    }
    if (conversation->name != NULL)
        FreeVec(conversation->name);
    if (conversation->system != NULL)
        CodesetsFreeA(conversation->system, NULL);
    FreeVec(conversation);
}

/**
 * Shutdown the GUI
 **/
void shutdownGUI() {
    if (app) {
        DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);

#ifndef DAEMON
        // Release pens if they were allocated
        if (mainWindowObject && (redPen || greenPen || bluePen || yellowPen)) {
            struct Screen *currentScreen;
            get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
            if (currentScreen) {
                if (redPen)
                    ReleasePen(currentScreen->ViewPort.ColorMap, redPen);
                if (greenPen)
                    ReleasePen(currentScreen->ViewPort.ColorMap, greenPen);
                if (bluePen)
                    ReleasePen(currentScreen->ViewPort.ColorMap, bluePen);
                if (yellowPen)
                    ReleasePen(currentScreen->ViewPort.ColorMap, yellowPen);
            }
        }
#endif

        MUI_DisposeObject(app);
    }

#ifndef DAEMON
    if (chatOutputTextEditorContents) {
        FreeVec(chatOutputTextEditorContents);
    }
    if (isMUI5 || isMUI39) {
        deleteAmigaGPTTextEditor();
    }
#endif

    closeGUILibraries();
}