#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
#include "amiga_compiler.h"
#endif
#include <json-c/json.h>
#include <intuition/icclass.h>
#include <libraries/codesets.h>
#ifdef __MORPHOS__
#include <libraries/ttengine.h>
#endif
#include <libraries/mui.h>
#include <mui/Aboutbox_mcc.h>
#include <mui/BetterString_mcc.h>
#include <mui/Busy_mcc.h>
#include <mui/Guigfx_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#ifdef __MORPHOS__
#include <mui/Scintilla_mcc.h>
#include "ChatOutputScintilla.h"
#include "CodeBlocksScintilla.h"
#include "CodeBlocksViewer.h"
#endif
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include "AboutAmigaGPTWindow.h"
#include "AmigaGPTTextEditor.h"
#include "APIKeyRequesterWindow.h"
#include "ARexx.h"
#include "ChatSystemRequesterWindow.h"
#include "CustomServerSettingsRequesterWindow.h"
#include "codefence.h"
#include "config.h"
#include "streamlog.h"
#include "ElevenLabsSettingsRequesterWindow.h"
#include "gui.h"
#include "menu.h"
#include "MainWindow.h"
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
#ifdef __MORPHOS__
struct Library *TTEngineBase;
#endif
Object *app = NULL;

static BOOL appQuitting = FALSE;

void mainWindowSignalQuit(void) { appQuitting = TRUE; }

BOOL mainWindowIsShuttingDown(void) { return appQuitting; }
ULONG redPen = 0, greenPen = 0, bluePen = 0, yellowPen = 0;
Object *imageWindowObject;
Object *imageWindowImageView;
Object *imageWindowImageViewGroup;
Object *codeBlocksWindowObject;
#ifdef __MORPHOS__
Object *codeBlocksListView;
Object *codeBlocksList;
Object *codeBlocksScintillaGroup;
Object *codeBlocksScintilla;
Object *codeBlocksCopyUtf8Button;
Object *codeBlocksCopySystemButton;
Object *codeBlocksSaveUtf8Button;
Object *codeBlocksSaveSystemButton;
#else
Object *codeBlocksOutputListView;
Object *codeBlocksOutputFloat;
#endif
BOOL isMUI5;
BOOL isMUI39;
BOOL isAROS;
struct codeset *systemCodeset;

#ifndef DAEMON
#define CODEBLOCKS_VIEW_BUFFER_LENGTH (1024 * 100)
#define CODEBLOCK_PLACEHOLDER_MAX 48
#ifndef __MORPHOS__
static STRPTR codeBlocksViewContents = NULL;
#endif

static BOOL append_cstr(STRPTR buf, ULONG cap, ULONG *off, const char *s);
static BOOL append_bytes(STRPTR buf, ULONG cap, ULONG *off, const void *p,
                         ULONG len);
static BOOL build_conversation_codeblocks_utf8(struct Conversation *conv,
                                              STRPTR buf, ULONG cap);
#endif

static BOOL checkMUICustomClassInstalled();
static void closeGUILibraries();

static CONST_STRPTR USED_CLASSES[] = {
    MUIC_Aboutbox,   MUIC_Busy,       MUIC_NList,
    MUIC_NListview,  MUIC_TextEditor, MUIC_BetterString,
    MUIC_NFloattext, MUIC_Guigfx,
#ifdef __MORPHOS__
    MUIC_Scintilla,
#endif
    NULL};

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

#ifdef __MORPHOS__
    if ((TTEngineBase = OpenLibrary("ttengine.library", TTENGINEMINVERSION)) ==
        NULL) {
        displayError(STRING_ERROR_TTENGINE_LIB_OPEN);
        return RETURN_ERROR;
    }
#endif

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
#ifdef __MORPHOS__
    if (TTEngineBase) {
        CloseLibrary(TTEngineBase);
        TTEngineBase = NULL;
    }
    if (ClipboardBase) {
        CloseLibrary(ClipboardBase);
        ClipboardBase = NULL;
    }
#endif
}

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG initVideo() {
    streamLogBootPhase("initVideo start");

    if (openGUILibraries() == RETURN_ERROR) {
        streamLogBootPhase("initVideo fail openGUILibraries");
        return RETURN_ERROR;
    }

#ifdef DAEMON
    if (!(app = ApplicationObject, MUIA_Application_Base, "AMIGAGPTD",
          MUIA_Application_Title, "AmigaGPT Daemon", MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "2023-2025 Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Description, "AmigaGPT Daemon",
          MUIA_Application_Version, APP_VER_STRING_AMIGAGPTD,
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
          MUIA_Application_Version, APP_VER_STRING_AMIGAGPT,
          MUIA_Application_UsedClasses, USED_CLASSES, MUIA_Application_HelpFile,
          "AMIGAGPT:AmigaGPT.guide", MUIA_Application_SingleTask, TRUE,
          MUIA_Application_Commands, arexxList, MUIA_Application_UseRexx, TRUE,
          SubWindow, apiKeyRequesterWindowObject, SubWindow,
          chatSystemRequesterWindowObject, SubWindow,
          customServerSettingsRequesterWindowObject, SubWindow,
          proxySettingsRequesterWindowObject, SubWindow,
          voiceInstructionsRequesterWindowObject, SubWindow,
          elevenLabsSettingsRequesterWindowObject, SubWindow,
#ifndef __MORPHOS__
          codeBlocksWindowObject = WindowObject, MUIA_Window_Title,
          STRING_MENU_VIEW_CODEBLOCKS, MUIA_Window_ID, OBJECT_ID_CODEBLOCKS_WINDOW,
          MUIA_Window_Width, 480, MUIA_Window_Height, 360,
          MUIA_Window_CloseGadget, TRUE, MUIA_Window_SizeGadget, TRUE,
          MUIA_Window_DepthGadget, TRUE, MUIA_Window_DragBar, TRUE,
          MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
          MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
          MUIA_Window_SizeRight, TRUE, MUIA_Window_UseBottomBorderScroller, FALSE,
          MUIA_Window_UseRightBorderScroller, FALSE,
          MUIA_Window_UseLeftBorderScroller, FALSE, WindowContents, VGroup,
            Child, codeBlocksOutputListView = NListviewObject,
              MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_On,
              MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_On,
              MUIA_NListview_NList, codeBlocksOutputFloat = NFloattextObject,
                MUIA_Font, MUIV_NList_Font_Fixed, MUIA_Frame, MUIV_Frame_Text,
                MUIA_ContextMenu, NULL, MUIA_NFloattext_Text, "",
              End,
            End,
          End, End,
#else
          codeBlocksWindowObject = WindowObject, MUIA_Window_Title,
          STRING_MENU_VIEW_CODEBLOCKS, MUIA_Window_ID, OBJECT_ID_CODEBLOCKS_WINDOW,
          MUIA_Window_Width, 560, MUIA_Window_Height, 360,
          MUIA_Window_CloseGadget, TRUE, MUIA_Window_SizeGadget, TRUE,
          MUIA_Window_DepthGadget, TRUE, MUIA_Window_DragBar, TRUE,
          MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
          MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
          MUIA_Window_SizeRight, TRUE, MUIA_Window_UseBottomBorderScroller, FALSE,
          MUIA_Window_UseRightBorderScroller, FALSE,
          MUIA_Window_UseLeftBorderScroller, FALSE, WindowContents, VGroup,
            Child, HGroup, MUIA_VertWeight, 100,
              Child, codeBlocksListView = NListviewObject,
                MUIA_HorizWeight, 0, MUIA_FixWidth, 128,
                MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_Off,
                MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_On,
                MUIA_NListview_NList, codeBlocksList = NListObject,
                  MUIA_NList_ConstructHook2, &ConstructCodeBlockListHook,
                  MUIA_NList_DestructHook2, &DestructCodeBlockListHook,
                  MUIA_NList_DisplayHook2, &DisplayCodeBlockListHook,
                  MUIA_NList_Format, "BAR MINW=72 MAXW=120",
                  MUIA_NList_AutoVisible, TRUE, MUIA_NList_Title, FALSE,
                End,
              End,
              Child, codeBlocksScintillaGroup = ScrollgroupObject,
                MUIA_HorizWeight, 100, MUIA_Scrollgroup_AutoBars, TRUE,
                MUIA_Scrollgroup_Contents, codeBlocksScintilla = ScintillaObject,
                  MUIA_Frame, MUIV_Frame_Text, End, End,
            End,
            Child, HGroup,
              Child, codeBlocksCopyUtf8Button = MUI_MakeObject(
                  MUIO_Button, STRING_MENU_COPY_CODEBLOCK, End,
              Child, codeBlocksCopySystemButton = MUI_MakeObject(
                  MUIO_Button, STRING_MENU_COPY_CODEBLOCK_SYSTEM, End,
              Child, codeBlocksSaveUtf8Button = MUI_MakeObject(
                  MUIO_Button, STRING_MENU_SAVE_CODEBLOCK_UTF8, End,
              Child, codeBlocksSaveSystemButton = MUI_MakeObject(
                  MUIO_Button, STRING_MENU_SAVE_CODEBLOCK_SYSTEM, End,
            End, End, End,
#endif
          SubWindow,
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

    /* CloseRequest only: never notify on MUIA_Window_Open FALSE — that also fires
     * during MUI_DisposeObject() and NList_Clear in the hook freezes on restart. */
    DoMethod(codeBlocksWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &CodeBlocksWindowClosedHook);
    DoMethod(codeBlocksWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);

#ifdef __MORPHOS__
    if ((ClipboardBase = OpenLibrary("clipboard.library", 51)) == NULL) {
        displayError(STRING_ERROR_CLIPBOARD_LIB_OPEN);
        return RETURN_ERROR;
    }
    codeBlocksViewerSetObjects(codeBlocksList, codeBlocksScintilla);
    codeBlocksViewerSetActionButtons(
        codeBlocksCopyUtf8Button, codeBlocksCopySystemButton,
        codeBlocksSaveUtf8Button, codeBlocksSaveSystemButton);
    codeBlocksViewerAttachListHooks();
    codeBlocksViewerAttachActionButtons();
    codeBlocksScintillaInitViewer(codeBlocksScintilla);
#else
    if (codeBlocksViewContents == NULL) {
        codeBlocksViewContents =
            AllocVec(CODEBLOCKS_VIEW_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
        if (codeBlocksViewContents == NULL) {
            displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
            return RETURN_ERROR;
        }
    }
#endif

    if (createAboutAmigaGPTWindow() == RETURN_OK)
        DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);

    streamLogBootPhase("createMainWindow call");
    if (createMainWindow() == RETURN_ERROR) {
        streamLogBootPhase("createMainWindow fail");
        return RETURN_ERROR;
    }
    streamLogBootPhase("initVideo ok");

#endif

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

    streamLogBootPhase("NewInput loop start");

    while (running) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, &signals);

        switch (id) {
        case MUIV_Application_ReturnID_Quit: {
            mainWindowSignalQuit();
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
        if (signals & SIGBREAKF_CTRL_C) {
            mainWindowSignalQuit();
            running = FALSE;
        }
    }
}

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 *
 **/
void strbufAppend(STRPTR buf, ULONG bufSize, CONST_STRPTR piece) {
    ULONG used, avail, n;

    if (buf == NULL || bufSize < 2 || piece == NULL || piece[0] == '\0') {
        return;
    }
    used = (ULONG)strlen(buf);
    if (used >= bufSize - 1) {
        return;
    }
    avail = bufSize - 1 - used;
    n = (ULONG)strlen(piece);
    if (n > avail) {
        n = avail;
    }
    if (n > 0) {
        CopyMem(piece, buf + used, n);
        buf[used + n] = '\0';
    }
}

void updateStatusBar(CONST_STRPTR message, const ULONG pen) {
#ifndef DAEMON
    ULONG cap = strlen(message) + 32;
    STRPTR formattedMessage = AllocVec(cap, MEMF_ANY);

    if (formattedMessage == NULL) {
        return;
    }
    snprintf((char *)formattedMessage, cap, "\33P[%lu]  %s\t", pen, message);
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
    ULONG titleCap = (ULONG)strlen(appName) + (ULONG)strlen(STRING_ERROR) + 2;
    UBYTE *errorTitle = AllocVec(titleCap, MEMF_ANY | MEMF_CLEAR);
    ULONG okCap = (ULONG)strlen(STRING_OK) + 2;
    STRPTR okString = AllocVec(okCap, MEMF_ANY | MEMF_CLEAR);
    const UBYTE ERROR_BUFFER_LENGTH = 255;
    STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);

    if (errorTitle == NULL || okString == NULL || errorMessage == NULL) {
        FreeVec(errorTitle);
        FreeVec(okString);
        FreeVec(errorMessage);
        return;
    }
    snprintf((char *)errorTitle, titleCap, "%s %s", appName, STRING_ERROR);
    snprintf(okString, okCap, "*%s", STRING_OK);
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
    FreeVec(errorTitle);
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
    conversation->name_list_display = NULL;
    conversation->system = NULL;

    return conversation;
}

void conversationRefreshNameListDisplay(struct Conversation *conversation) {
    if (conversation == NULL) {
        return;
    }
    if (conversation->name_list_display != NULL) {
        CodesetsFreeA(conversation->name_list_display, NULL);
        conversation->name_list_display = NULL;
    }
    if (conversation->name == NULL || conversation->name[0] == '\0') {
        return;
    }
    conversation->name_list_display = CodesetsUTF8ToStr(
        CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)conversation->name,
        CSA_MapForeignChars, TRUE, TAG_DONE);
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

CONST_STRPTR jsonGetApiErrorMessage(struct json_object *error) {
    struct json_object *message;

    if (error == NULL || json_object_is_type(error, json_type_null)) {
        return NULL;
    }
    if (json_object_is_type(error, json_type_string)) {
        return json_object_get_string(error);
    }
    if (!json_object_is_type(error, json_type_object)) {
        return NULL;
    }
    if (json_object_object_get_ex(error, "message", &message)) {
        return json_object_get_string(message);
    }
    return NULL;
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
    if (json == NULL || !json_object_is_type(json, json_type_object)) {
        return stream ? (UTF8 *)"" : NULL;
    }
    if (stream) {
        struct json_object *type;
        CONST_STRPTR typeStr;

        if (!json_object_object_get_ex(json, "type", &type)) {
            return (UTF8 *)"";
        }
        typeStr = json_object_get_string(type);
        if (typeStr == NULL) {
            return (UTF8 *)"";
        }
        if (strcmp(typeStr, "response.output_text.delta") == 0) {
            struct json_object *delta;

            if (!json_object_object_get_ex(json, "delta", &delta)) {
                return (UTF8 *)"";
            }
            if (json_object_is_type(delta, json_type_string)) {
                return (UTF8 *)json_object_get_string(delta);
            }
            if (json_object_is_type(delta, json_type_object) &&
                json_object_object_get_ex(delta, "text", &delta)) {
                return (UTF8 *)json_object_get_string(delta);
            }
            return (UTF8 *)"";
        }
        return (UTF8 *)"";
    } else {
        struct json_object *text = NULL;

        if (apiEndpoint == API_ENDPOINT_RESPONSES) {
            struct json_object *outputArray;
            struct json_object *output = NULL;

            if (!json_object_object_get_ex(json, "output", &outputArray) ||
                !json_object_is_type(outputArray, json_type_array)) {
                return (UTF8 *)"";
            }

            {
                int arrayLength = json_object_array_length(outputArray);
                int i;

                for (i = 0; i < arrayLength; i++) {
                    struct json_object *currentOutput =
                        json_object_array_get_idx(outputArray, i);
                    struct json_object *typeObj;

                    if (currentOutput == NULL ||
                        !json_object_is_type(currentOutput, json_type_object)) {
                        continue;
                    }
                    if (!json_object_object_get_ex(currentOutput, "type",
                                                 &typeObj)) {
                        continue;
                    }
                    {
                        const char *typeStr = json_object_get_string(typeObj);

                        if (typeStr != NULL && strcmp(typeStr, "message") == 0) {
                            output = currentOutput;
                            break;
                        }
                    }
                }
            }

            if (output == NULL) {
                return (UTF8 *)"";
            }

            {
                struct json_object *contentArray;
                struct json_object *content;

                if (!json_object_object_get_ex(output, "content",
                                             &contentArray) ||
                    !json_object_is_type(contentArray, json_type_array)) {
                    return (UTF8 *)"";
                }
                content = json_object_array_get_idx(contentArray, 0);
                if (content == NULL ||
                    !json_object_is_type(content, json_type_object) ||
                    !json_object_object_get_ex(content, "text", &text)) {
                    return (UTF8 *)"";
                }
            }
        } else {
            struct json_object *choices;
            struct json_object *choice;
            struct json_object *message;

            if (!json_object_object_get_ex(json, "choices", &choices) ||
                !json_object_is_type(choices, json_type_array)) {
                return NULL;
            }
            choice = json_object_array_get_idx(choices, 0);
            if (choice == NULL || !json_object_is_type(choice, json_type_object) ||
                !json_object_object_get_ex(choice, "message", &message) ||
                !json_object_is_type(message, json_type_object) ||
                !json_object_object_get_ex(message, "content", &text)) {
                return NULL;
            }
        }

        if (text == NULL) {
            return NULL;
        }
        if (retainJSONFormat) {
            UTF8 *textStr = json_object_to_json_string_ext(
                text, JSON_C_TO_STRING_NOSLASHESCAPE);

            if (textStr == NULL) {
                return NULL;
            }
            textStr++;
            textStr[strlen(textStr) - 1] = '\0';
            return textStr;
        }
        return (UTF8 *)json_object_get_string(text);
    }
}

static struct MinList *newEmptyMinList(void) {
    struct MinList *list = AllocVec(sizeof(struct MinList), MEMF_CLEAR);
    if (list) {
        list->mlh_Tail = 0;
        list->mlh_Head = (struct MinNode *)&list->mlh_Tail;
        list->mlh_TailPred = (struct MinNode *)&list->mlh_Head;
    }
    return list;
}

static void freeConversationNode(struct ConversationNode *conversationNode) {
    struct AICodeBlock *block;

    if (conversationNode->codeblocks != NULL) {
        while ((block = (struct AICodeBlock *)RemHead(
                    (struct List *)conversationNode->codeblocks)) != NULL) {
            if (block->language != NULL) {
                FreeVec(block->language);
            }
            if (block->raw_code != NULL) {
                FreeVec(block->raw_code);
            }
            FreeVec(block);
        }
        FreeVec(conversationNode->codeblocks);
    }
    if (conversationNode->display_text != NULL) {
        FreeVec(conversationNode->display_text);
    }
    if (conversationNode->raw_utf8 != NULL) {
        FreeVec(conversationNode->raw_utf8);
    }
}

UTF8 *conversationNodeGetRaw(const struct ConversationNode *node) {
    if (node == NULL) {
        return NULL;
    }
    return node->raw_utf8;
}

UTF8 *conversationNodeGetDisplay(const struct ConversationNode *node) {
    if (node == NULL) {
        return NULL;
    }
    if (node->display_text != NULL) {
        return node->display_text;
    }
    return node->raw_utf8;
}

/**
 * Add a block of text to the conversation list
 * @param conversation The conversation to add the text to
 * @param text The text to add to the conversation
 * @param role The role of the text (user or assistant)
 **/
void addTextToConversation(struct Conversation *conversation, UTF8 *text,
                           STRPTR role) {
    struct ConversationNode *conversationNode;
    ULONG textLength;

    if (text == NULL) {
        text = (UTF8 *)"";
    }
    textLength = strlen(text);

    conversationNode = AllocVec(sizeof(struct ConversationNode), MEMF_CLEAR);
    if (conversationNode == NULL) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        return;
    }
    snprintf(conversationNode->role, sizeof(conversationNode->role), "%s", role);

    conversationNode->raw_utf8 = AllocVec(textLength + 1, MEMF_CLEAR);
    if (conversationNode->raw_utf8 == NULL) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        FreeVec(conversationNode);
        return;
    }
    CopyMem(text, conversationNode->raw_utf8, textLength);
    conversationNode->raw_utf8[textLength] = '\0';
    conversationNode->raw_length = textLength;
    conversationNode->display_text = NULL;
    conversationNode->codeblocks = newEmptyMinList();
    if (conversationNode->codeblocks == NULL) {
        FreeVec(conversationNode->raw_utf8);
        FreeVec(conversationNode);
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        return;
    }

    AddTail((struct List *)conversation->messages,
            (struct Node *)conversationNode);

    conversationNodeParseCodeFences(conversation, conversationNode);
#if defined(__MORPHOS__) && !defined(DAEMON)
    refreshViewCodeBlocksMenuState();
#endif
}

/**
 * Free the conversation
 * @param conversation The conversation to free
 **/
void freeConversation(struct Conversation *conversation) {
    struct ConversationNode *conversationNode;
    while ((conversationNode = (struct ConversationNode *)RemHead(
                (struct List *)conversation->messages)) != NULL) {
        freeConversationNode(conversationNode);
        FreeVec(conversationNode);
    }
    if (conversation->name != NULL)
        FreeVec(conversation->name);
    if (conversation->name_list_display != NULL)
        CodesetsFreeA(conversation->name_list_display, NULL);
    if (conversation->system != NULL)
        CodesetsFreeA(conversation->system, NULL);
    FreeVec(conversation);
}

#ifndef DAEMON
static BOOL append_bytes(STRPTR buf, ULONG cap, ULONG *off, const void *p,
                         ULONG len) {
    if (len > cap || *off > cap - len) {
        return FALSE;
    }
    CopyMem(p, buf + *off, len);
    *off += len;
    buf[*off] = '\0';
    return TRUE;
}

static BOOL append_cstr(STRPTR buf, ULONG cap, ULONG *off, const char *s) {
    return append_bytes(buf, cap, off, s, (ULONG)strlen(s));
}

static BOOL build_conversation_codeblocks_utf8(struct Conversation *conv,
                                              STRPTR buf, ULONG cap) {
    struct MinNode *mn;
    ULONG off = 0;
    char header[CODEBLOCK_PLACEHOLDER_MAX];

    buf[0] = '\0';
    if (conv == NULL || conv->messages == NULL) {
        return FALSE;
    }
    for (mn = conv->messages->mlh_Head; mn->mln_Succ != NULL;
         mn = mn->mln_Succ) {
        struct ConversationNode *node = (struct ConversationNode *)mn;
        struct MinNode *bn;

        if (node->codeblocks == NULL) {
            continue;
        }
        for (bn = node->codeblocks->mlh_Head; bn->mln_Succ != NULL;
             bn = bn->mln_Succ) {
            struct AICodeBlock *block = (struct AICodeBlock *)bn;
            const char *lang =
                (block->language != NULL && block->language[0] != '\0')
                    ? (const char *)block->language
                    : "";
            int header_len;

            header_len =
                snprintf(header, sizeof(header),
                         (const char *)STRING_CHAT_CODEBLOCK_PLACEHOLDER,
                         (long)block->index);
            if (header_len <= 0) {
                return FALSE;
            }
            if (!append_bytes(buf, cap, &off, header, (ULONG)header_len)) {
                return FALSE;
            }

            if (!append_cstr(buf, cap, &off, "```") ||
                !append_cstr(buf, cap, &off, lang) ||
                !append_cstr(buf, cap, &off, "\n")) {
                return FALSE;
            }
            if (block->raw_code != NULL && block->code_length > 0) {
                if (!append_bytes(buf, cap, &off, block->raw_code,
                                  block->code_length)) {
                    return FALSE;
                }
            }
            if (!append_cstr(buf, cap, &off, "\n```\n\n")) {
                return FALSE;
            }
        }
    }
    return off > 0;
}

void openCodeBlocksViewerWindow(void) {
    struct Conversation *conv = getCurrentConversation();
    STRPTR utf8Buf = NULL;

#ifdef __MORPHOS__
    codeBlocksViewerScheduleOpenWindow();
    return;
#endif

    if (conv == NULL) {
        displayError(STRING_ERROR_NO_ACTIVE_CONVERSATION);
        return;
    }

#ifndef __MORPHOS__
    utf8Buf = AllocVec(CODEBLOCKS_VIEW_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
    if (utf8Buf == NULL) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        return;
    }
    if (!build_conversation_codeblocks_utf8(conv, utf8Buf,
                                            CODEBLOCKS_VIEW_BUFFER_LENGTH)) {
        FreeVec(utf8Buf);
        displayError(STRING_ERROR_NO_CODEBLOCKS_IN_CHAT);
        return;
    }

    {
        STRPTR formatted =
            CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                              (Tag)utf8Buf, CSA_MapForeignChars, TRUE, TAG_DONE);
        FreeVec(utf8Buf);
        if (formatted == NULL) {
            displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
            return;
        }
        snprintf(codeBlocksViewContents, CODEBLOCKS_VIEW_BUFFER_LENGTH, "%s",
                 formatted);
        CodesetsFreeA(formatted, NULL);
        set(codeBlocksOutputFloat, MUIA_NFloattext_Text, codeBlocksViewContents);
    }

    set(codeBlocksWindowObject, MUIA_Window_Open, TRUE);
#endif
}
#endif

/**
 * Shutdown the GUI
 **/
void shutdownGUI() {
    if (app) {
        DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);

#ifndef DAEMON
        mainWindowPrepareShutdown();
        chatOutputWheelShutdown();
#ifdef __MORPHOS__
        codeBlocksViewerPrepareShutdown();
        chatOutputScintillaDisposeNotifyClass();
#endif

        MUI_DisposeObject(app);
        app = NULL;
        chatOutputWheelDisposeClass();
        mainWindowInvalidateAfterShutdown();
#ifdef __MORPHOS__
        codeBlocksWindowObject = NULL;
        codeBlocksListView = NULL;
        codeBlocksList = NULL;
        codeBlocksScintillaGroup = NULL;
        codeBlocksScintilla = NULL;
        codeBlocksCopyUtf8Button = NULL;
        codeBlocksCopySystemButton = NULL;
        codeBlocksSaveUtf8Button = NULL;
        codeBlocksSaveSystemButton = NULL;
#endif
        imageWindowObject = NULL;
        imageWindowImageView = NULL;
        imageWindowImageViewGroup = NULL;
        menuStrip = NULL;
#endif
    }

#ifndef DAEMON
    if (chatOutputTextEditorContents) {
        FreeVec(chatOutputTextEditorContents);
        chatOutputTextEditorContents = NULL;
    }
#ifndef __MORPHOS__
    if (codeBlocksViewContents) {
        FreeVec(codeBlocksViewContents);
        codeBlocksViewContents = NULL;
    }
#endif
    if (isMUI5 || isMUI39) {
        deleteAmigaGPTTextEditor();
    }
#endif

    closeGUILibraries();
}