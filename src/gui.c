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
#include "APIKeyRequesterWindow.h"
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "StartupOptionsWindow.h"
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
BOOL isAROS;
struct codeset *systemCodeset;

static BOOL checkMUICustomClassInstalled();
static void closeGUILibraries();

static CONST_STRPTR USED_CLASSES[] = {
    MUIC_Aboutbox,   MUIC_Busy,       MUIC_NList,
    MUIC_NListview,  MUIC_TextEditor, MUIC_BetterString,
    MUIC_NFloattext, MUIC_Guigfx,     NULL};

HOOKPROTONHNO(SendMessageFunc, APTR, ULONG *arg) {
    printf("SendMessage\nModel: %s\nSystem: %s\nAPI Key: %s\nPrompt: %s\n",
           arg[0], arg[1], arg[2], arg[3]);
    STRPTR modelString = (STRPTR)arg[0];
    STRPTR system = (STRPTR)arg[1];
    STRPTR apiKey = (STRPTR)arg[2];
    STRPTR prompt = (STRPTR)arg[3];

    if (apiKey == NULL) {
        apiKey = config.openAiApiKey;
    }
    enum ChatModel model;
    if (modelString == NULL) {
        model = config.chatModel;
    } else {
        for (UBYTE i = 0; CHAT_MODEL_NAMES[i] != NULL; i++) {
            if (CHAT_MODEL_NAMES[i + 1] == NULL) {
                return (RETURN_ERROR);
            }
            if (strcmp(modelString, CHAT_MODEL_NAMES[i]) == 0) {
                model = i;
                break;
            }
        }
    }
    struct Conversation *conversation = newConversation();
    UTF8 *promptUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                          CSA_Source, (Tag)prompt, TAG_DONE);
    addTextToConversation(conversation, promptUTF8, "user");
    CodesetsFreeA(promptUTF8, NULL);

    if (system != NULL) {
        UTF8 *systemUTF8 =
            CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                               CSA_Source, (Tag)system, TAG_DONE);
        addTextToConversation(conversation, systemUTF8, "system");
        CodesetsFreeA(systemUTF8, NULL);
    }

    struct json_object **responses =
        postChatMessageToOpenAI(conversation, model, apiKey, FALSE, FALSE, NULL,
                                0, FALSE, FALSE, NULL, NULL);

    if (responses == NULL) {
        printf(STRING_ERROR_CONNECTING_OPENAI);
        set(app, MUIA_Application_RexxString, STRING_ERROR_CONNECTING_OPENAI);
        freeConversation(conversation);
        return (RETURN_ERROR);
    }
    struct json_object *response = responses[0];
    UTF8 *contentString = getMessageContentFromJson(response, FALSE);

    if (contentString != NULL) {
        if (strlen(contentString) > 0) {
            STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                (Tag)contentString, CSA_MapForeignChars, TRUE, TAG_DONE);
            set(app, MUIA_Application_RexxString,
                formattedMessageSystemEncoded);
            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
        }
        json_object_put(response);
        FreeVec(responses);
        freeConversation(conversation);
        return (RETURN_OK);
    }
    FreeVec(responses);
    freeConversation(conversation);
    return (RETURN_ERROR);
}
MakeHook(SendMessageHook, SendMessageFunc);

static struct MUI_Command arexxList[] = {
    {"SENDMESSAGE",
     "M=MODEL/K,S=SYSTEM/K,K=APIKEY/K,P=PROMPT/F",
     4,
     &SendMessageHook,
     {0, 0, 0, 0, 0}},
    {NULL, NULL, 0, NULL, {0, 0, 0, 0, 0}}};

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
        displayError(STRING_ERROR_MUI_CUSTOM_CLASSES_INSTALLED);
        return RETURN_ERROR;
    }

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

    if (!(app = ApplicationObject, MUIA_Application_Base, "AMIGAGPT",
          MUIA_Application_Title, STRING_APP_NAME, MUIA_Application_Version,
          APP_VERSION, MUIA_Application_Copyright,
          "© 2023-2025 Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Author, "Cameron Armstrong (Nightfox/sacredbanana)",
          MUIA_Application_Description, STRING_APP_DESCRIPTION,
          MUIA_Application_Version,
          "$VER: AmigaGPT " APP_VERSION " (" BUILD_DATE ")",
          MUIA_Application_UsedClasses, USED_CLASSES, MUIA_Application_HelpFile,
          "PROGDIR:AmigaGPT.guide", MUIA_Application_SingleTask, TRUE,
          MUIA_Application_Commands, arexxList, MUIA_Application_UseRexx, TRUE,
          SubWindow, startupOptionsWindowObject, SubWindow, mainWindowObject,
          SubWindow, apiKeyRequesterWindowObject, SubWindow,
          chatSystemRequesterWindowObject, SubWindow,
          proxySettingsRequesterWindowObject, SubWindow,
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
        case ID_RESCAN: {
            printf("Rescan\n");
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

    closeGUILibraries();
}