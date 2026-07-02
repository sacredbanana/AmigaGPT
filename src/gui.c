#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
#include "amiga_compiler.h"
#endif
#include <json-c/json.h>
#include <intuition/icclass.h>
#include <proto/dos.h>
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
#include "ChatFindScintilla.h"
#include "ChatOutputScintilla.h"
#include "CodeBlocksScintilla.h"
#include "CodeBlocksViewer.h"
#endif
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include "AboutAmigaGPTWindow.h"
#include "AmigaGPTConfig.h"
#include "AmigaGPTTextEditor.h"
#include "APIKeyRequesterWindow.h"
#include "ARexx.h"
#include "CustomServerSettingsRequesterWindow.h"
#include "codefence.h"
#include "streamlog.h"
#if defined(__MORPHOS__) && !defined(DAEMON)
#include "morphos_relaunch.h"
#include "openai.h"
#endif
#include "SpeechProviderSettingsRequesterWindow.h"
#include "gui.h"
#include "menu.h"
#include "MainWindow.h"
#include "ProxySettingsRequesterWindow.h"
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

BOOL mainWindowIsShuttingDown(void) {
#ifndef DAEMON
    return appQuitting || mainWindowMorphosPrepareShutdownActive();
#else
    return appQuitting;
#endif
}
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
#ifdef __MORPHOS__
    streamLogLifecycle("initVideo begin");
#endif
    streamLogBootPhase("initVideo start");

    if (openGUILibraries() == RETURN_ERROR) {
        streamLogBootPhase("initVideo fail openGUILibraries");
        return RETURN_ERROR;
    }

    /* Initialize the MUI config class now that MUIMasterBase is open */
    if (initConfigMUI() == RETURN_ERROR) {
        displayError("Failed to initialize config MUI class");
    }

#ifdef DAEMON
    if (!(app = ApplicationObject, MUIA_Application_Base, "AMIGAGPTD",
          MUIA_Application_Title, "AmigaGPT Daemon", MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "2023-2026 Cameron Armstrong (Nightfox/sacredbanana)",
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

    if (createCustomServerSettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createProxySettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createSpeechProviderSettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    {
      ULONG appCreateAttempts = 0;
#ifdef __MORPHOS__
      Delay(30);
      streamLogLifecycle("initVideo app create cooldown done");
#endif
retryCreateApp:
      if (!(app = ApplicationObject, MUIA_Application_Base, "AMIGAGPT",
          MUIA_Application_Title, STRING_APP_NAME, MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "2023-2026 Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Description, STRING_APP_DESCRIPTION,
          MUIA_Application_Version, APP_VER_STRING_AMIGAGPT,
          MUIA_Application_UsedClasses, USED_CLASSES, MUIA_Application_HelpFile,
          "AMIGAGPT:AmigaGPT.guide", MUIA_Application_SingleTask, TRUE,
          MUIA_Application_Commands, arexxList, MUIA_Application_UseRexx, TRUE,
          SubWindow, apiKeyRequesterWindowObject, SubWindow,
          customServerSettingsRequesterWindowObject, SubWindow,
          proxySettingsRequesterWindowObject, SubWindow,
          speechProviderSettingsRequesterWindowObject, SubWindow,
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
        UBYTE phase[96];
        snprintf((STRPTR)phase, sizeof(phase),
                 "app create fail attempt=%lu", (unsigned long)(appCreateAttempts + 1));
        streamLogLifecycle((CONST_STRPTR)phase);
        if (++appCreateAttempts < 12) {
            Delay(appCreateAttempts > 4 ? 40 : 25);
            streamLogLifecycle("initVideo app create retry");
            goto retryCreateApp;
        }
        streamLogStartupPhase("app create failed");
#ifdef __MORPHOS__
        morphosStartupShowAlert(
            "AmigaGPT could not start (MUI application busy).\n\n"
            "If you just quit, wait 5 seconds and try once.\n\n"
            "Diagnostics: T:amigagpt_startup.last and "
            "T:amigagpt_shutdown.last");
#else
        displayError(STRING_ERROR_APP_CREATE);
#endif
        return RETURN_ERROR;
      }
      {
          UBYTE phase[96];
          snprintf((STRPTR)phase, sizeof(phase), "app create ok attempts=%lu",
                   (unsigned long)(appCreateAttempts + 1));
          streamLogLifecycle((CONST_STRPTR)phase);
      }
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
    streamLogLifecycle("initVideo subwindow notifies done");

#ifdef __MORPHOS__
    streamLogLifecycle("initVideo clipboard open begin");
    if ((ClipboardBase = OpenLibrary("clipboard.library", 51)) == NULL) {
        displayError(STRING_ERROR_CLIPBOARD_LIB_OPEN);
        return RETURN_ERROR;
    }
    streamLogLifecycle("initVideo clipboard open done");
    codeBlocksViewerSetObjects(codeBlocksList, codeBlocksScintilla);
    codeBlocksViewerSetActionButtons(
        codeBlocksCopyUtf8Button, codeBlocksCopySystemButton,
        codeBlocksSaveUtf8Button, codeBlocksSaveSystemButton);
    streamLogLifecycle("initVideo codeblocks hooks begin");
    codeBlocksViewerAttachListHooks();
    codeBlocksViewerAttachActionButtons();
    codeBlocksViewerPrimeScintillaAtStartup();
    streamLogLifecycle("initVideo codeblocks hooks done");
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

    streamLogLifecycle("initVideo about window begin");
    if (createAboutAmigaGPTWindow() == RETURN_OK)
        DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);
    streamLogLifecycle("initVideo about window done");

    streamLogBootPhase("createMainWindow call");
    streamLogLifecycle("mainWindow create begin");
    if (createMainWindow() == RETURN_ERROR) {
        streamLogBootPhase("createMainWindow fail");
        streamLogLifecycle("mainWindow create fail");
        streamLogStartupPhase("createMainWindow fail");
        if (app != NULL) {
            streamLogLifecycle("MUI_DisposeObject(app) begin after mainWindow fail");
            MUI_DisposeObject(app);
            streamLogLifecycle("MUI_DisposeObject(app) done after mainWindow fail");
            app = NULL;
        }
        return RETURN_ERROR;
    }
    streamLogLifecycle("mainWindow create ok");
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
#if defined(__MORPHOS__) && !defined(DAEMON)
    morphosRunStartupDeferred();
#endif

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
#ifdef __MORPHOS__
        case APP_ID_CHAT_FIND_SHOW:
            chatFindScintillaShow(chatOutputTextEditor);
            break;
        case APP_ID_CHAT_FIND_NEXT:
            chatFindScintillaNext(chatOutputTextEditor);
            break;
        case APP_ID_CHAT_FIND_PREV:
            chatFindScintillaPrev(chatOutputTextEditor);
            break;
        case APP_ID_CHAT_FIND_CLOSE:
            chatFindScintillaHide();
            break;
        case APP_ID_CHAT_FIND_STRING_FOCUS:
            chatFindScintillaOnStringFocus();
            break;
        case APP_ID_CHAT_FIND_UPDATE:
            chatFindScintillaUpdateCounter(chatOutputTextEditor);
            break;
        case APP_ID_CHAT_USER_PREV:
            chatUserNavPrev(chatOutputTextEditor);
            break;
        case APP_ID_CHAT_USER_NEXT:
            chatUserNavNext(chatOutputTextEditor);
            break;
#endif
        case APP_ID_CHAT_PROVIDER_SETTINGS:
            openChatProviderSettingsRequesterWindow();
            break;
        case APP_ID_IMAGE_PROVIDER_SETTINGS:
            openImageProviderSettingsRequesterWindow();
            break;
        case APP_ID_SPEECH_PROVIDER_SETTINGS:
            openSpeechProviderSettingsRequesterWindow();
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
    conversation->lastResponseId = NULL;

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
static UTF8 *messageScratch = NULL;
static ULONG messageScratchCap = 0;

static void ensureMessageScratch(ULONG need) {
    if (messageScratch != NULL && need <= messageScratchCap)
        return;
    ULONG newCap = messageScratchCap ? messageScratchCap : 1024;
    while (newCap < need) {
        newCap *= 2;
        if (newCap < 1024)
            newCap = 1024;
    }
    UTF8 *newBuf = AllocVec(newCap, MEMF_ANY | MEMF_CLEAR);
    if (newBuf == NULL)
        return;
    if (messageScratch != NULL) {
        strncpy((char *)newBuf, (char *)messageScratch, newCap - 1);
        FreeVec(messageScratch);
    }
    messageScratch = newBuf;
    messageScratchCap = newCap;
}

static void resetMessageScratch(void) {
    ensureMessageScratch(8);
    if (messageScratch != NULL)
        messageScratch[0] = '\0';
}

static void appendNewlineToMessageScratch(void) {
    if (messageScratch == NULL)
        return;
    ULONG curLen = (ULONG)strlen((char *)messageScratch);
    ensureMessageScratch(curLen + 4);
    if (messageScratch == NULL)
        return;
    if (curLen > 0 && messageScratch[curLen - 1] != '\n') {
        strncat((char *)messageScratch, "\n",
                messageScratchCap - strlen((char *)messageScratch) - 1);
    }
}

static void appendJsonStringToMessageScratch(struct json_object *obj,
                                             BOOL retainJSONFormat) {
    if (obj == NULL)
        return;
    if (messageScratch == NULL)
        resetMessageScratch();
    if (messageScratch == NULL)
        return;

    UTF8 *piece = NULL;
    size_t pieceLen = 0;

    if (retainJSONFormat) {
        /* When the source is a JSON string, preserve the textual content
         * without wrapping quotes. Using json_object_get_string() here avoids
         * leaving a stray trailing quote in ARexx responses while still
         * keeping any JSON-style escaping already present in the string data.
         */
        if (json_object_is_type(obj, json_type_string)) {
            piece = (UTF8 *)json_object_get_string(obj);
            if (piece != NULL)
                pieceLen = strlen((char *)piece);
        } else {
            UTF8 *raw = (UTF8 *)json_object_to_json_string_ext(
                obj, JSON_C_TO_STRING_NOSLASHESCAPE);
            if (raw != NULL) {
                piece = raw;
                pieceLen = strlen((char *)raw);
            }
        }
    } else {
        piece = (UTF8 *)json_object_get_string(obj);
        if (piece != NULL)
            pieceLen = strlen((char *)piece);
    }

    if (piece == NULL || pieceLen == 0)
        return;

    ULONG curLen = (ULONG)strlen((UTF8 *)messageScratch);
    ensureMessageScratch(curLen + (ULONG)pieceLen + 4);
    if (messageScratch == NULL)
        return;

    strncat((UTF8 *)messageScratch, piece,
            messageScratchCap - strlen((char *)messageScratch) - 1);
}

UTF8 *getMessageContentFromJson(struct json_object *json, BOOL stream,
                                BOOL retainJSONFormat,
                                APIChatEndpoint apiEndpoint) {
    if (json == NULL)
        return NULL;
    if (stream) {
        /* Streaming can be either:
         * - OpenAI Responses streaming events (type=response.output_text.delta)
         * - OpenAI-compatible chat.completions streaming chunks
         *   (object=chat.completion.chunk, choices[].delta.content)
         * - Gemini native streamGenerateContent SSE chunks
         *   (candidates[].content.parts[].text)
         */

        /* Responses API streaming */
        struct json_object *type = json_object_object_get(json, "type");
        if (type != NULL) {
            UTF8 *typeStr = json_object_get_string(type);
            if (typeStr != NULL &&
                strcmp(typeStr, "response.output_text.delta") == 0) {
                struct json_object *text =
                    json_object_object_get(json, "delta");
                return text != NULL ? (UTF8 *)json_object_get_string(text)
                                    : (UTF8 *)"";
            }
            /* Any other typed event (e.g. response.completed) contributes no
             * text */
            return (UTF8 *)"";
        }

        /* chat.completions streaming chunk */
        struct json_object *choices = json_object_object_get(json, "choices");
        if (choices != NULL && json_object_is_type(choices, json_type_array) &&
            json_object_array_length(choices) > 0) {
            struct json_object *choice0 = json_object_array_get_idx(choices, 0);
            if (choice0 != NULL) {
                struct json_object *delta =
                    json_object_object_get(choice0, "delta");
                if (delta != NULL) {
                    struct json_object *content =
                        json_object_object_get(delta, "content");
                    if (content != NULL) {
                        UTF8 *s = json_object_get_string(content);
                        return s != NULL ? s : (UTF8 *)"";
                    }
                }

                /* Legacy /v1/completions streaming chunk:
                 * { "choices": [{"text": "..."}], ... } */
                struct json_object *textObj =
                    json_object_object_get(choice0, "text");
                if (textObj != NULL) {
                    UTF8 *s = json_object_get_string(textObj);
                    return s != NULL ? s : (UTF8 *)"";
                }
            }
        }

        /* Gemini native streamGenerateContent chunk */
        struct json_object *candidates =
            json_object_object_get(json, "candidates");
        if (candidates != NULL &&
            json_object_is_type(candidates, json_type_array) &&
            json_object_array_length(candidates) > 0) {
            resetMessageScratch();
            int candLen = json_object_array_length(candidates);
            for (int c = 0; c < candLen; c++) {
                struct json_object *cand =
                    json_object_array_get_idx(candidates, c);
                if (cand == NULL)
                    continue;
                struct json_object *content =
                    json_object_object_get(cand, "content");
                if (content == NULL ||
                    !json_object_is_type(content, json_type_object))
                    continue;
                struct json_object *parts =
                    json_object_object_get(content, "parts");
                if (parts == NULL ||
                    !json_object_is_type(parts, json_type_array))
                    continue;
                int pLen = json_object_array_length(parts);
                for (int p = 0; p < pLen; p++) {
                    struct json_object *part =
                        json_object_array_get_idx(parts, p);
                    if (part == NULL ||
                        !json_object_is_type(part, json_type_object))
                        continue;
                    struct json_object *t =
                        json_object_object_get(part, "text");
                    if (t != NULL) {
                        appendJsonStringToMessageScratch(t, retainJSONFormat);
                    }
                }
                if (c < candLen - 1)
                    appendNewlineToMessageScratch();
            }
            if (messageScratch != NULL && strlen((char *)messageScratch) > 0) {
                return messageScratch;
            }
            return (UTF8 *)"";
        }

        return (UTF8 *)"";
    } else {
        resetMessageScratch();
        if (apiEndpoint == API_CHAT_ENDPOINT_RESPONSES) {
            struct json_object *outputArray =
                json_object_object_get(json, "output");
            struct json_object *output = NULL;

            if (outputArray == NULL ||
                !json_object_is_type(outputArray, json_type_array)) {
                return (UTF8 *)"";
            }
            int arrayLength = json_object_array_length(outputArray);
            for (int i = 0; i < arrayLength; i++) {
                struct json_object *currentOutput =
                    json_object_array_get_idx(outputArray, i);
                struct json_object *typeObj =
                    json_object_object_get(currentOutput, "type");
                if (typeObj != NULL) {
                    UTF8 *typeStr = json_object_get_string(typeObj);
                    if (strcmp(typeStr, "message") == 0) {
                        output = currentOutput;
                        break;
                    }
                }
            }

            if (output == NULL) {
                return (UTF8 *)"";
            }

            struct json_object *contentArray =
                json_object_object_get(output, "content");
            if (contentArray != NULL &&
                json_object_is_type(contentArray, json_type_array)) {
                /* Concatenate all text blocks in the message content */
                int cLen = json_object_array_length(contentArray);
                for (int i = 0; i < cLen; i++) {
                    struct json_object *content =
                        json_object_array_get_idx(contentArray, i);
                    if (content == NULL ||
                        !json_object_is_type(content, json_type_object))
                        continue;
                    struct json_object *t =
                        json_object_object_get(content, "text");
                    if (t != NULL) {
                        appendJsonStringToMessageScratch(t, retainJSONFormat);
                    }
                }
                if (messageScratch != NULL &&
                    strlen((char *)messageScratch) > 0) {
                    return messageScratch;
                }
            }
            return (UTF8 *)"";
        } else if (apiEndpoint == API_CHAT_ENDPOINT_MESSAGES) {
            /* Anthropic/Claude Messages API response format:
             * { "content": [{"type": "text", "text": "..."}], ... } */
            struct json_object *contentArray =
                json_object_object_get(json, "content");
            if (contentArray == NULL ||
                !json_object_is_type(contentArray, json_type_array)) {
                return (UTF8 *)"";
            }

            /* Concatenate all text blocks in the content array */
            int arrayLength = json_object_array_length(contentArray);
            for (int i = 0; i < arrayLength; i++) {
                struct json_object *block =
                    json_object_array_get_idx(contentArray, i);
                struct json_object *typeObj =
                    json_object_object_get(block, "type");
                if (typeObj != NULL) {
                    UTF8 *typeStr = json_object_get_string(typeObj);
                    if (strcmp(typeStr, "text") == 0) {
                        struct json_object *t =
                            json_object_object_get(block, "text");
                        if (t != NULL) {
                            appendJsonStringToMessageScratch(t,
                                                             retainJSONFormat);
                        }
                    }
                }
            }

            if (messageScratch != NULL && strlen((char *)messageScratch) > 0) {
                return messageScratch;
            }
            return (UTF8 *)"";
        } else {
            /* Gemini native generateContent format:
             * { "candidates": [{ "content": { "parts": [{"text":"..."}] } }] }
             */
            struct json_object *candidates =
                json_object_object_get(json, "candidates");
            if (candidates != NULL &&
                json_object_is_type(candidates, json_type_array) &&
                json_object_array_length(candidates) > 0) {
                int candLen = json_object_array_length(candidates);
                for (int c = 0; c < candLen; c++) {
                    struct json_object *cand =
                        json_object_array_get_idx(candidates, c);
                    if (cand == NULL)
                        continue;
                    struct json_object *content =
                        json_object_object_get(cand, "content");
                    if (content == NULL ||
                        !json_object_is_type(content, json_type_object))
                        continue;
                    struct json_object *parts =
                        json_object_object_get(content, "parts");
                    if (parts == NULL ||
                        !json_object_is_type(parts, json_type_array))
                        continue;
                    int pLen = json_object_array_length(parts);
                    for (int p = 0; p < pLen; p++) {
                        struct json_object *part =
                            json_object_array_get_idx(parts, p);
                        if (part == NULL ||
                            !json_object_is_type(part, json_type_object))
                            continue;
                        struct json_object *t =
                            json_object_object_get(part, "text");
                        if (t != NULL) {
                            appendJsonStringToMessageScratch(t,
                                                             retainJSONFormat);
                        }
                    }
                    if (c < candLen - 1)
                        appendNewlineToMessageScratch();
                }
                if (messageScratch != NULL &&
                    strlen((char *)messageScratch) > 0) {
                    return messageScratch;
                }
            }

            /* OpenAI chat/completions format:
             * { "choices": [{"message": {"content": "..."}}] } */
            struct json_object *contentArray =
                json_object_object_get(json, "choices");
            if (contentArray == NULL ||
                !json_object_is_type(contentArray, json_type_array)) {
                return (UTF8 *)"";
            }
            int cLen = json_object_array_length(contentArray);
            for (int i = 0; i < cLen; i++) {
                struct json_object *choice =
                    json_object_array_get_idx(contentArray, i);
                if (choice == NULL)
                    continue;
                struct json_object *message =
                    json_object_object_get(choice, "message");
                if (message == NULL)
                    continue;
                struct json_object *content =
                    json_object_object_get(message, "content");
                if (content == NULL)
                    continue;
                if (json_object_is_type(content, json_type_string)) {
                    appendJsonStringToMessageScratch(content, retainJSONFormat);
                    if (i < cLen - 1) {
                        appendNewlineToMessageScratch();
                    }
                } else if (json_object_is_type(content, json_type_array)) {
                    /* Some providers return content as an array of parts */
                    int pLen = json_object_array_length(content);
                    for (int p = 0; p < pLen; p++) {
                        struct json_object *part =
                            json_object_array_get_idx(content, p);
                        if (part == NULL ||
                            !json_object_is_type(part, json_type_object))
                            continue;
                        struct json_object *typeObj =
                            json_object_object_get(part, "type");
                        if (typeObj != NULL) {
                            UTF8 *typeStr = json_object_get_string(typeObj);
                            if (typeStr != NULL &&
                                strcmp((char *)typeStr, "text") == 0) {
                                struct json_object *t =
                                    json_object_object_get(part, "text");
                                appendJsonStringToMessageScratch(
                                    t, retainJSONFormat);
                            }
                        }
                    }
                    if (i < cLen - 1)
                        appendNewlineToMessageScratch();
                }
            }
            if (messageScratch != NULL && strlen((char *)messageScratch) > 0) {
                return messageScratch;
            }
            return (UTF8 *)"";
        }
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
    FreeVec(conversation->messages);
    if (conversation->name != NULL)
        FreeVec(conversation->name);
    if (conversation->name_list_display != NULL)
        CodesetsFreeA(conversation->name_list_display, NULL);
    if (conversation->system != NULL)
        CodesetsFreeA(conversation->system, NULL);
    if (conversation->lastResponseId != NULL)
        FreeVec(conversation->lastResponseId);
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

STRPTR utf8ToLatin1(UTF8 *src) {
    if (src == NULL)
        return NULL;
    ULONG srcLen = strlen(src);
    STRPTR dest = AllocVec(srcLen + 1, MEMF_ANY | MEMF_CLEAR);
    if (dest == NULL)
        return NULL;
    ULONG si = 0, di = 0;
    while (si < srcLen) {
        UBYTE c = (UBYTE)src[si];
        if (c < 0x80) {
            dest[di++] = c;
            si++;
        } else if ((c & 0xE0) == 0xC0 && si + 1 < srcLen &&
                   ((UBYTE)src[si + 1] & 0xC0) == 0x80) {
            UWORD codepoint = ((c & 0x1F) << 6) | (src[si + 1] & 0x3F);
            dest[di++] = (codepoint <= 0xFF) ? (UBYTE)codepoint : '?';
            si += 2;
        } else if ((c & 0xF0) == 0xE0 && si + 2 < srcLen) {
            dest[di++] = '?';
            si += 3;
        } else if ((c & 0xF8) == 0xF0 && si + 3 < srcLen) {
            dest[di++] = '?';
            si += 4;
        } else {
            dest[di++] = c;
            si++;
        }
    }
    dest[di] = '\0';
    return dest;
}
/**
 * Shutdown the GUI
 **/
void shutdownGUI() {
    if (app) {
#ifndef DAEMON
#if defined(__MORPHOS__)
        streamLogShutdownPhase("shutdown gui begin");
        morphosRelaunchShutdownBegin();
#endif
        /*
         * Close windows, detach Scintilla notifies, clear documents before ENVARC
         * save — saving while large styled chat docs are active can freeze MUI/OS.
         */
        mainWindowPrepareShutdown();
        streamLogShutdownPhase("main window prepare done");
        streamLogLifecycle("chatOutputWheelShutdown begin");
        chatOutputWheelShutdown();
        streamLogLifecycle("chatOutputWheelShutdown done");
#ifdef __MORPHOS__
        codeBlocksViewerPrepareShutdown();
#endif

#ifdef __MORPHOS__
        /*
         * Never MUIM_Application_Save on quit — can hard-freeze MorphOS when chat
         * Scintilla holds a large/styled document (MUI prefs walk the object tree).
         * Window prefs are restored from ENVARC on next start if saved earlier in-session.
         */
        streamLogLifecycle("shutdown ENVARC save skipped on quit");
        streamLogShutdownPhase("envarc save skipped on quit");
#else
        DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
        streamLogLifecycle("shutdown ENVARC save done");
#endif

        if (openAIChatStreamInProgress()) {
            streamLogLifecycle("shutdown dispose stream in progress");
        }
        streamLogLifecycle("MUI_DisposeObject(app) begin");
        streamLogShutdownPhase("dispose app begin");
        MUI_DisposeObject(app);
        streamLogLifecycle("MUI_DisposeObject(app) done");
        streamLogShutdownPhase("dispose app done");
        app = NULL;
#if defined(__MORPHOS__) && !defined(DAEMON)
        /*
         * Let Intuition/MUI finish teardown before the next WB launch (freeze with no
         * "startup begin" if the next process starts during DisposeObject teardown).
         */
        Delay(MORPHOS_RELAUNCH_POST_DISPOSE_DELAY);
        streamLogLifecycle("shutdown post-dispose cooldown done");
        streamLogShutdownPhase("post-dispose cooldown done");
        morphosRelaunchShutdownEnd();
        streamLogShutdownPhase("shutdown gui end");
#endif
        chatOutputWheelDisposeClass();
#ifdef __MORPHOS__
        chatOutputScintillaDisposeNotifyClass();
#endif
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