#ifdef __AMIGAOS3__ || __AMIGAOS4__
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
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "StartupOptionsWindow.h"
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

    dispatcher = (ULONG(*)(struct IClass *, Object *, Msg))cl->cl_UserData;

    return dispatcher(cl, obj, msg);
}

struct EmulLibEntry muiDispatcherEntry = {TRAP_LIB, 0,
                                          (void (*)(void))muiDispatcherGate};
#endif

struct Library *MUIMasterBase;
struct Library *CodesetsBase;
Object *app = NULL;
ULONG redPen = NULL, greenPen = NULL, bluePen = NULL, yellowPen = NULL;
struct Screen *screen;
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

    if (!checkMUICustomClassInstalled()) {
        displayError(STRING_ERROR_MUI_CUSTOM_CLASSES_INSTALLED);
        return RETURN_ERROR;
    }

    if (createStartupOptionsWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createAPIKeyRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createChatSystemRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createProxySettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createVoiceInstructionsRequesterWindow() == RETURN_ERROR)
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
          "PROGDIR:AmigaGPT.guide", MUIA_Application_SingleTask, TRUE,
          MUIA_Application_Commands, arexxList, MUIA_Application_UseRexx, TRUE,
          SubWindow, startupOptionsWindowObject, SubWindow,
          apiKeyRequesterWindowObject, SubWindow,
          chatSystemRequesterWindowObject, SubWindow,
          proxySettingsRequesterWindowObject, SubWindow,
          voiceInstructionsRequesterWindowObject, SubWindow,
          imageWindowObject = WindowObject, MUIA_Window_Title, STRING_IMAGE,
          MUIA_Window_Width, 320, MUIA_Window_Height, 240,
          MUIA_Window_CloseGadget, TRUE, MUIA_Window_LeftEdge,
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

    set(startupOptionsWindowObject, MUIA_Window_Open, TRUE);

    if (createMainWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);

    addStartupOptionsWindowActions();

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
        default:
            break;
        }
        if (running && signals)
            Wait(signals);
    }
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
    if (app) {
        updateStatusBar(STRING_ERROR, redPen);
    }
    const UBYTE ERROR_BUFFER_LENGTH = 255;
    STRPTR errorMessage = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
    CONST_STRPTR okString =
        AllocVec(strlen(STRING_OK) + 2, MEMF_ANY | MEMF_CLEAR);
    snprintf(okString, strlen(STRING_OK) + 2, "*%s", STRING_OK);
    const LONG ERROR_CODE = IoErr();
    if (ERROR_CODE == 0) {
        if (!app ||
            MUI_Request(app, mainWindowObject,
#ifdef __MORPHOS__
                        NULL,
#else
                        MUIV_Requester_Image_Error,
#endif
                        STRING_ERROR, okString, "\33c%s", message) != 0) {
            struct EasyStruct errorES = {sizeof(struct EasyStruct), 0,
                                         STRING_ERROR, message, STRING_OK};
            EasyRequest(NULL, &errorES, NULL, NULL);
        }
    } else {
        STRPTR errorDescription = AllocVec(ERROR_BUFFER_LENGTH, MEMF_ANY);
        Fault(ERROR_CODE, NULL, errorDescription, ERROR_BUFFER_LENGTH);
        snprintf(errorMessage, ERROR_BUFFER_LENGTH, "%s: %s\n\n%s\0",
                 STRING_ERROR, errorDescription, message);
        if (app) {
            MUI_Request(app, mainWindowObject,
#ifdef __MORPHOS__
                        NULL,
#else
                        MUIV_Requester_Image_Error,
#endif
                        STRING_ERROR, okString, "\33c%s", errorMessage);
        } else {
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
 * Shutdown the GUI
 **/
void shutdownGUI() {
    if (app) {
        DoMethod(app, MUIM_Application_Save, MUIV_Application_Save_ENVARC);
        MUI_DisposeObject(app);
    }
    if (redPen) {
        ReleasePen(screen->ViewPort.ColorMap, redPen);
    }
    if (greenPen) {
        ReleasePen(screen->ViewPort.ColorMap, greenPen);
    }
    if (bluePen) {
        ReleasePen(screen->ViewPort.ColorMap, bluePen);
    }
    if (yellowPen) {
        ReleasePen(screen->ViewPort.ColorMap, yellowPen);
    }
    if (isPublicScreen) {
        UnlockPubScreen(NULL, screen);
    } else {
        CloseScreen(screen);
    }
    if (chatOutputTextEditorContents) {
        FreeVec(chatOutputTextEditorContents);
    }
    if (isMUI5 || isMUI39) {
        deleteAmigaGPTTextEditor();
    }

    closeGUILibraries();
}