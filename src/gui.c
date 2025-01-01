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
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include "AboutAmigaGPTWindow.h"
#include "APIKeyRequesterWindow.h"
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "StartupOptionsWindow.h"
#include "version.h"
#include "datatypesclass.h"

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
Object *openImageWindowImageView;
Object *dataTypeObject;
BOOL isMUI5;
BOOL isAROS;
struct codeset *systemCodeset;
struct codeset *utf8Codeset;

static CONST_STRPTR USED_CLASSES[] = {
    MUIC_Aboutbox,   MUIC_Busy,         MUIC_NList,      MUIC_NListview,
    MUIC_TextEditor, MUIC_BetterString, MUIC_NFloattext, NULL};

static BOOL checkMUICustomClassInstalled();
static void closeGUILibraries();

/**
 * Open the libraries needed for the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG openGUILibraries() {
    if ((MUIMasterBase = OpenLibrary("muimaster.library", 19)) == NULL) {
        printf("Could not open muimaster.library\nPlease make sure you have "
               "MUI installed\n");
        return RETURN_ERROR;
    }
#ifdef __AMIGAOS4__
    if ((IMUIMaster = (struct MUIIFace *)GetInterface(MUIMasterBase, "main", 1,
                                                      NULL)) == NULL) {
        printf("Could not get interface for muimaster.library\n");
        return RETURN_ERROR;
    }
#endif

    if ((CodesetsBase = OpenLibrary("codesets.library", 6)) == NULL) {
        printf(
            "Could not open codesets.library\nPlease make sure you have "
            "codesets installed\nRefer to the documentation for more info.\n");
        return RETURN_ERROR;
    }
#ifdef __AMIGAOS4__
    if ((ICodesets = (struct CodesetsIFace *)GetInterface(CodesetsBase, "main",
                                                          1, NULL)) == NULL) {
        printf("Could not get interface for codesets.library\n");
        return RETURN_ERROR;
    }
#endif

    if (!(systemCodeset = CodesetsFindA(NULL, NULL))) {
        displayError("Could not find the system codeset");
        return RETURN_ERROR;
    }

    if (!(utf8Codeset = CodesetsFind("UTF-8", NULL, CSA_FallbackToDefault,
                                     FALSE, TAG_DONE))) {
        displayError("Could not find the UTF-8 codeset");
        return RETURN_ERROR;
    }

    struct Library *AROSBase;

    if (AROSBase = OpenLibrary("aros.library", 0)) {
        isAROS = TRUE;
        CloseLibrary(AROSBase);
    } else {
        isAROS = FALSE;
    }

    isMUI5 = MUIMasterBase->lib_Version >= 20;
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
        displayError(
            "Could not find all the MUI custom classes. Please make sure they "
            "are installed. Refer to the documentation for more information.");
        return RETURN_ERROR;
    }

    create_datatypes_class();

    if (createStartupOptionsWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createMainWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createAPIKeyRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createChatSystemRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (createProxySettingsRequesterWindow() == RETURN_ERROR)
        return RETURN_ERROR;

    if (isMUI5 && !isAROS &&
            (imageWindowObject = WindowObject, MUIA_Window_Title, "Image",
             MUIA_Window_Width, 320, MUIA_Window_Height, 240,
             MUIA_Window_CloseGadget, TRUE, MUIA_Window_LeftEdge,
             MUIV_Window_LeftEdge_Centered, MUIA_Window_TopEdge,
             MUIV_Window_TopEdge_Centered, MUIA_Window_SizeRight, TRUE,
             MUIA_Window_UseBottomBorderScroller, FALSE,
             MUIA_Window_UseRightBorderScroller, FALSE,
             MUIA_Window_UseLeftBorderScroller, FALSE, WindowContents, VGroup,
             Child, openImageWindowImageView = DataTypesObject, NULL, TAG_END),
        End, End) == NULL) {
            displayError("Could not create image window");
            return RETURN_ERROR;
        }

    if (!(app = ApplicationObject, MUIA_Application_Base, "AmigaGPT",
          MUIA_Application_Title, "AmigaGPT", MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "(C) 2023-2025 Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Description,
          "AmigaGPT is an app for chatting to ChatGPT or creating AI "
          "images "
          "with DALL-E",
          MUIA_Application_UsedClasses, USED_CLASSES, MUIA_Application_HelpFile,
          "PROGDIR:AmigaGPT.guide", SubWindow, startupOptionsWindowObject,
          SubWindow, mainWindowObject, SubWindow, apiKeyRequesterWindowObject,
          SubWindow, chatSystemRequesterWindowObject, SubWindow,
          proxySettingsRequesterWindowObject, End)) {
        displayError("Could not create app!\n");
        return RETURN_ERROR;
    }

    DoMethod(imageWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Self, 3, MUIM_Set, MUIA_Window_Open, FALSE);

    if (createAboutAmigaGPTWindow() == RETURN_OK)
        DoMethod(app, OM_ADDMEMBER, aboutAmigaGPTWindowObject);

    if (isMUI5 && !isAROS) {
        DoMethod(app, OM_ADDMEMBER, imageWindowObject);
    }

    set(startupOptionsWindowObject, MUIA_Window_Open, TRUE);

    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);

    addMenuActions();
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
            displayError("Could not find the MUI custom class:");
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

    delete_datatypes_class();

    closeGUILibraries();
}