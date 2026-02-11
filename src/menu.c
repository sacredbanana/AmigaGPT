#include <dos/dostags.h>
#include <libraries/amigaguide.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/amigaguide.h>
#include <proto/exec.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include "AmigaGPTConfig.h"
#include "APIKeyRequesterWindow.h"
#include "AboutAmigaGPTWindow.h"
#include "ARexx.h"
#include "ChatSystemRequesterWindow.h"
#include "CustomServerSettingsRequesterWindow.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "version.h"

Object *menuStrip = NULL;

static void populateSpeechMenu();
static void populateChatMenu();
static void populateArexxMenu();
static BPTR BuildNPPath(const char *const *extraDirs, BOOL addParent);
static void FreeNPPath(BPTR listHead);

/* The Shell's path nodes are trivial: next + lock (both BPTRs).   */
/* RKRM names it PathComponent / Guru Book calls it PathList entry. */
struct PathNodeCompat {
    BPTR pc_Next; /* BPTR to next node (or ZERO) */
    BPTR pc_Lock; /* BPTR lock on a directory    */
};

/* Iterate a BPTR path list */
#define PN_NEXT(n) ((struct PathNodeCompat *)BADDR((n)->pc_Next))

HOOKPROTONHNONP(AboutAmigaGPTMenuItemClickedFunc, void) {
    if (aboutAmigaGPTWindowObject) {
        set(aboutAmigaGPTWindowObject, MUIA_Window_Open, TRUE);
    } else {
        struct EasyStruct aboutRequester = {
            sizeof(struct EasyStruct), 0, STRING_ABOUT,
#ifdef __AMIGAOS3__
            "AmigaGPT for m68k AmigaOS 3\n\n"
#else
            "AmigaGPT for PPC AmigaOS 4\n\n"
#endif
            "Version " APP_VERSION "\n"
            "Build date: " __DATE__ "\n"
            "Build number: " BUILD_NUMBER "\n\n"
            "Developed by Cameron Armstrong (@sacredbanana on GitHub,\n"
            "YouTube and Twitter, @Nightfox on EAB)\n\n"
            "This app will always remain free but if you would like to\n"
            "support me you can do so at https://paypal.me/sacredbanana",
            STRING_OK};
        ULONG flags = IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS;
        EasyRequest(mainWindow, &aboutRequester, &flags, NULL);
    }
}
MakeHook(AboutAmigaGPTMenuItemClickedHook, AboutAmigaGPTMenuItemClickedFunc);

HOOKPROTONHNO(SpeechSystemMenuItemClickedFunc, void,
              SpeechSystem *speechSystem) {
    closeSpeech();
    configSetSpeechSystem(*speechSystem);
    if (initSpeech(*speechSystem) == RETURN_ERROR) {
        switch (*speechSystem) {
        case SPEECH_SYSTEM_34:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_34);
            break;
        case SPEECH_SYSTEM_37:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_37);
            break;
        case SPEECH_SYSTEM_FLITE:
            displayError(STRING_ERROR_SPEECH_INIT_FLITE);
            break;
        case SPEECH_SYSTEM_OPENAI:
            displayError(STRING_ERROR_SPEECH_INIT_OPENAI);
            break;
        default:
            displayError(STRING_ERROR_SPEECH_UNKNOWN_SYSTEM);
            break;
        }
    }
}
MakeHook(SpeechSystemMenuItemClickedHook, SpeechSystemMenuItemClickedFunc);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
HOOKPROTONHNONP(SpeechAccentMenuItemClickedFunc, void) {
    struct FileRequester *fileRequester;
    if (fileRequester = (struct FileRequester *)MUI_AllocAslRequestTags(
            ASL_FileRequest, ASLFR_Window, mainWindow, ASLFR_PopToFront, TRUE,
            ASLFR_Activate, TRUE, ASLFR_DrawersOnly, FALSE, ASLFR_InitialDrawer,
            "LOCALE:accents", ASLFR_DoPatterns, TRUE, ASLFR_InitialPattern,
            "#?.accent", TAG_DONE)) {
        if (MUI_AslRequestTags(fileRequester, TAG_DONE)) {
            configSetSpeechAccent(fileRequester->fr_File);
        }
        FreeAslRequest(fileRequester);
    }
}
MakeHook(SpeechAccentMenuItemClickedHook, SpeechAccentMenuItemClickedFunc);
#endif

HOOKPROTONHNONP(OpenARexxShellFunc, void) {
#ifdef __MORPHOS__
    SystemTags("TS && TCO", TAG_DONE);
#else
    SystemTags("PATH ADD SYS:Rexxc && TS && TCO", TAG_DONE);
#endif
}
MakeHook(OpenARexxShellFuncHook, OpenARexxShellFunc);

HOOKPROTONHNONP(CloseARexxShellFunc, void) {
#ifdef __MORPHOS__
    SystemTags("TE && TCC", TAG_DONE);
#else
    SystemTags("PATH ADD SYS:Rexxc >NIL: && TE && TCC", TAG_DONE);
#endif
}
MakeHook(CloseARexxShellFuncHook, CloseARexxShellFunc);

HOOKPROTONHNONP(ARexxImportScriptMenuItemClickedFunc, void) {
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq != NULL) {
        if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                           STRING_MENU_AREXX_IMPORT_SCRIPT, ASLFR_InitialDrawer,
                           "REXX:", ASLFR_InitialPattern, "#?.rexx",
                           TAG_DONE)) {
            STRPTR filePath = fileReq->fr_Drawer;
            STRPTR fileName = fileReq->fr_File;
            UWORD fullPathLength = strlen(filePath) + strlen(fileName) + 2;
            STRPTR fullPath = AllocVec(fullPathLength, MEMF_CLEAR);
            strncpy(fullPath, filePath, strlen(filePath));
            AddPart(fullPath, fileName, fullPathLength);
            UWORD destinationPathLength =
                strlen("AMIGAGPT:rexx/") + strlen(fileName) + 1;
            STRPTR destinationPath =
                AllocVec(destinationPathLength, MEMF_CLEAR);
            strncpy(destinationPath, "AMIGAGPT:rexx/",
                    strlen("AMIGAGPT:rexx/"));
            AddPart(destinationPath, fileName, destinationPathLength);
            copyFile(fullPath, destinationPath);
            FreeVec(fullPath);
            FreeVec(destinationPath);
        }
        FreeAslRequest(fileReq);
    }
    populateArexxMenu();
}
MakeHook(ARexxImportScriptMenuItemClickedHook,
         ARexxImportScriptMenuItemClickedFunc);

HOOKPROTONHNP(ARexxRunScriptMenuItemClickedFunc, void, APTR obj) {
    STRPTR script;
    get(obj, MUIA_Menuitem_Title, &script);
    STRPTR scriptPath = AllocVec(1024, MEMF_CLEAR);
    NameFromLock(GetProgramDir(), scriptPath, 1024);
    AddPart(scriptPath, "rexx/", 1024);
    AddPart(scriptPath, script, 1024);
#ifdef __MORPHOS__
    const char *extra[] = {"SYS:Utilities", "MOSSYS:C", "SYS:S", "MOSSYS:S",
                           NULL};
#else
    const char *extra[] = {"SYS:Utilities",
                           "SYS:S",
                           "SYS:Rexxc",
                           "SYS:System",
                           "SYS:Prefs",
                           "SYS:Tools",
                           "SYS:Tools/Commodities",
                           "SYS:WBStartup",
                           NULL};
#endif

    BPTR npPath = BuildNPPath(extra, TRUE); // BuildNPPath should stop at NULL
                                            // and DUPLICATE any parent nodes.

    UBYTE command[1024];
#ifdef __AMIGAOS3__
    snprintf(command, sizeof(command), "Run SYS:Rexxc/RX %s", scriptPath);
#else
    snprintf(command, sizeof(command), "Run RX %s", scriptPath);
#endif

    LONG rc = SystemTags((CONST_STRPTR)command, NP_Path, (ULONG)npPath,
                         NP_StackSize, 32768, TAG_DONE);

    // Only free on failure. On success DOS owns the list.
    if (rc == -1 && npPath) {
        FreeNPPath(npPath);
    }
    FreeVec(scriptPath);
}
MakeHook(ARexxRunScriptMenuItemClickedHook, ARexxRunScriptMenuItemClickedFunc);

/**
 * Get the active text editor based on which page is selected in the mode
 * register group
 * @return The active text editor object
 */
static Object *getActiveTextEditor() {
    LONG activePage = 0;
    get(modeRegisterGroup, MUIA_Group_ActivePage, &activePage);
    return (activePage == 0) ? chatInputTextEditor : imageInputTextEditor;
}

HOOKPROTONHNONP(EditCutFunc, void) {
    Object *activeEditor = getActiveTextEditor();
    DoMethod(activeEditor, MUIM_TextEditor_ARexxCmd, "Cut");
}
MakeHook(EditCutHook, EditCutFunc);

HOOKPROTONHNONP(EditCopyFunc, void) {
    Object *activeEditor = getActiveTextEditor();
    DoMethod(activeEditor, MUIM_TextEditor_ARexxCmd, "Copy");
}
MakeHook(EditCopyHook, EditCopyFunc);

HOOKPROTONHNONP(EditPasteFunc, void) {
    Object *activeEditor = getActiveTextEditor();
    DoMethod(activeEditor, MUIM_TextEditor_ARexxCmd, "Paste");
}
MakeHook(EditPasteHook, EditPasteFunc);

HOOKPROTONHNONP(EditClearFunc, void) {
    Object *activeEditor = getActiveTextEditor();
    if (isAROS) {
        set(activeEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(activeEditor, MUIM_TextEditor_ClearText);
    }
}
MakeHook(EditClearHook, EditClearFunc);

HOOKPROTONHNONP(EditSelectAllFunc, void) {
    Object *activeEditor = getActiveTextEditor();
    DoMethod(activeEditor, MUIM_TextEditor_ARexxCmd, "SelectAll");
}
MakeHook(EditSelectAllHook, EditSelectAllFunc);

HOOKPROTONHNONP(OpenDocumentationMenuItemClickedFunc, void) {
    CONST_STRPTR guidePath = "AMIGAGPT:AmigaGPT.guide";
    BPTR file = Open(guidePath, MODE_OLDFILE);
    if (file == NULL) {
        displayError(STRING_ERROR_DOCUMENTATION_OPEN);
        return;
    }
    Close(file);

    struct Screen *screen;
    get(mainWindowObject, MUIA_Window_Screen, &screen);

    struct NewAmigaGuide guide = {
        .nag_Name = guidePath,
        .nag_Screen = screen,
        .nag_PubScreen = NULL,
        .nag_BaseName = STRING_APP_NAME,
        .nag_Extens = NULL,
    };
    AMIGAGUIDECONTEXT handle;
    if (handle = OpenAmigaGuide(&guide, NULL)) {
        CloseAmigaGuide(handle);
    } else {
        displayError(STRING_ERROR_DOCUMENTATION_OPEN);
    }
}
MakeHook(OpenDocumentationMenuItemClickedHook,
         OpenDocumentationMenuItemClickedFunc);

HOOKPROTONHNONP(RecreateMainWindowFunc, void) {
    set(mainWindowObject, MUIA_Window_Open, FALSE);
    DoMethod(mainWindowObject, MUIM_KillNotify, MUIA_Window_Screen);
    DoMethod(mainWindowObject, MUIM_KillNotify, MUIA_Window_CloseRequest);
    DoMethod(app, OM_REMMEMBER, mainWindowObject);
    if (createMainWindow() == RETURN_ERROR) {
        displayError(STRING_ERROR_MAIN_WINDOW);
        return;
    }
}
MakeHook(RecreateMainWindowHook, RecreateMainWindowFunc);

HOOKPROTONHNONP(FixedWidthFontsMenuItemClickedFunc, void) {
    configSetFixedWidthFonts(!configGetFixedWidthFonts());
    if (FALSE) { /* Auto-save handles this now */
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &RecreateMainWindowHook);
}
MakeHook(FixedWidthFontsMenuItemClickedHook,
         FixedWidthFontsMenuItemClickedFunc);

HOOKPROTONHNONP(TextAlignmentChangedFunc, void) { displayConversation(NULL); }
MakeHook(TextAlignmentChangedHook, TextAlignmentChangedFunc);

HOOKPROTONHNONP(ClearMuiSettingsFunc, void) {
#ifdef __AMIGAOS4__
    Delete("ENVARC:mui/AmigaGPT.prefs");
    Delete("ENV:mui/AmigaGPT.prefs");
#else
    DeleteFile("ENVARC:mui/AmigaGPT.prefs");
    DeleteFile("ENV:mui/AmigaGPT.prefs");
#endif
    DoMethod(app, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
}
MakeHook(ClearMuiSettingsHook, ClearMuiSettingsFunc);

void createMenu() {
    menuStrip = MenustripObject,

    /* Project Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_PROJECT,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_PRINT, MUIA_UserData,
    MENU_ITEM_PROJECT_PRINT, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Menuitem_Shortcut, "P", End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, NM_BARLABEL, MUIA_UserData, MENU_ITEM_NULL, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_ABOUT_AMIGAGPT, MUIA_UserData, MENU_ITEM_PROJECT_ABOUT_AMIGAGPT,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_ABOUT_MUI, MUIA_UserData,
    MENU_ITEM_PROJECT_ABOUT_MUI, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL,
    MUIA_UserData, MENU_ITEM_NULL, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_QUIT, MUIA_UserData,
    MENU_ITEM_PROJECT_QUIT, MUIA_Menuitem_Shortcut, "Q",
    MUIA_Menuitem_CopyStrings, FALSE, End, End,

    /* Edit Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_EDIT,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_CUT, MUIA_UserData, MENU_ITEM_EDIT_CUT,
    MUIA_Menuitem_Shortcut, "X", MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_COPY,
    MUIA_UserData, MENU_ITEM_EDIT_COPY, MUIA_Menuitem_Shortcut, "C",
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_PASTE, MUIA_UserData, MENU_ITEM_EDIT_PASTE,
    MUIA_Menuitem_Shortcut, "V", MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_CLEAR,
    MUIA_UserData, MENU_ITEM_EDIT_CLEAR, MUIA_Menuitem_Shortcut, "L",
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_SELECT_ALL, MUIA_UserData,
    MENU_ITEM_EDIT_SELECT_ALL, MUIA_Menuitem_Shortcut, "A",
    MUIA_Menuitem_CopyStrings, FALSE, End, End,

    /* View Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_VIEW,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_FIXED_WIDTH_FONTS, MUIA_UserData,
    MENU_ITEM_VIEW_FIXED_WIDTH_FONTS, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Toggle, TRUE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_USER_TEXT_ALIGNMENT, MUIA_UserData,
    MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_TEXT_ALIGNMENT_LEFT, MUIA_UserData,
    MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT_LEFT, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
    configGetUserTextAlignment() == ALIGN_LEFT, MUIA_Menuitem_Exclude,
    ~(1 << 0), MUIA_Menuitem_Toggle, TRUE, End, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_TEXT_ALIGNMENT_CENTER,
    MUIA_UserData, MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT_CENTER,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Checked, configGetUserTextAlignment() == ALIGN_CENTER,
    MUIA_Menuitem_Exclude, ~(1 << 1), MUIA_Menuitem_Toggle, TRUE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_TEXT_ALIGNMENT_RIGHT, MUIA_UserData,
    MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT_RIGHT, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
    configGetUserTextAlignment() == ALIGN_RIGHT, MUIA_Menuitem_Exclude,
    ~(1 << 2), MUIA_Menuitem_Toggle, TRUE, End, End, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_ASSISTANT_TEXT_ALIGNMENT,
    MUIA_UserData, MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_TEXT_ALIGNMENT_LEFT, MUIA_UserData,
    MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT_LEFT, MUIA_Menuitem_CopyStrings,
    FALSE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
    configGetAssistantTextAlignment() == ALIGN_LEFT, MUIA_Menuitem_Exclude,
    ~(1 << 0), MUIA_Menuitem_Toggle, TRUE, End, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_TEXT_ALIGNMENT_CENTER,
    MUIA_UserData, MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT_CENTER,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Checked, configGetAssistantTextAlignment() == ALIGN_CENTER,
    MUIA_Menuitem_Exclude, ~(1 << 1), MUIA_Menuitem_Toggle, TRUE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_TEXT_ALIGNMENT_RIGHT, MUIA_UserData,
    MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT_RIGHT, MUIA_Menuitem_CopyStrings,
    FALSE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
    configGetAssistantTextAlignment() == ALIGN_RIGHT, MUIA_Menuitem_Exclude,
    ~(1 << 2), MUIA_Menuitem_Toggle, TRUE, End, End, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL, MUIA_UserData,
    MENU_ITEM_NULL, End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_MUI_SETTINGS, MUIA_UserData, MENU_ITEM_VIEW_MUI_SETTINGS,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_CLEAR_MUI_SETTINGS, MUIA_UserData,
    MENU_ITEM_VIEW_CLEAR_MUI_SETTINGS, MUIA_Menuitem_CopyStrings, FALSE, End,
    End,

    /* Connection Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_CONNECTION,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_PROXY, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_ENABLED,
    MUIA_UserData, MENU_ITEM_CONNECTION_PROXY_ENABLED, MUIA_Menuitem_Toggle,
    TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SETTINGS, MUIA_UserData, MENU_ITEM_CONNECTION_PROXY_SETTINGS,
    MUIA_Menuitem_CopyStrings, FALSE, End, End, End,

    /* Chat Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_CHAT,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_CHAT_PROVIDER_SETTINGS, MUIA_UserData,
    MENU_ITEM_CHAT_PROVIDER_SETTINGS, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL,
    MUIA_UserData, MENU_ITEM_NULL, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_WEB_SEARCH, MUIA_UserData,
    MENU_ITEM_CHAT_WEB_SEARCH_ENABLED, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, STRING_SHELL_TOOL,
    MUIA_UserData, MENU_ITEM_CHAT_SHELL_TOOL_ENABLED, MUIA_Menuitem_CopyStrings,
    FALSE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL,
    MUIA_UserData, MENU_ITEM_NULL, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_OPENAI_CHAT_SYSTEM, MUIA_UserData,
    MENU_ITEM_CHAT_SYSTEM, MUIA_Menuitem_CopyStrings, FALSE, End, End,

    /* Image Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_IMAGE,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_IMAGE_PROVIDER_SETTINGS, MUIA_UserData,
    MENU_ITEM_IMAGE_PROVIDER_SETTINGS, MUIA_Menuitem_CopyStrings, FALSE, End,
    End,

    /* Speech Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_SPEECH,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_ENABLED, MUIA_UserData,
    MENU_ITEM_SPEECH_ENABLED, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle,
    TRUE, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL,
    MUIA_UserData, MENU_ITEM_NULL, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_SPEECH_PROVIDER_SETTINGS, MUIA_UserData,
    MENU_ITEM_SPEECH_PROVIDER_SETTINGS, MUIA_Menuitem_CopyStrings, FALSE, End,
    End,

    /* ARexx Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_AREXX,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_AREXX_AREXX_SHELL, MUIA_UserData,
    MENU_ITEM_AREXX_AREXX_SHELL, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle, TRUE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, NM_BARLABEL,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_AREXX_IMPORT_SCRIPT, MUIA_UserData,
    MENU_ITEM_AREXX_IMPORT_SCRIPT, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_AREXX_RUN_SCRIPT, MUIA_UserData, MENU_ITEM_AREXX_RUN_SCRIPT,
    MUIA_Menuitem_CopyStrings, FALSE, End, End,

    /* Help Menu */
        MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_HELP,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_OPEN_DOCUMENTATION, MUIA_UserData,
    MENU_ITEM_HELP_OPEN_DOCUMENTATION, MUIA_Menuitem_CopyStrings, FALSE, End,
    End, End;

    /* Speech menu is no longer populated dynamically. */
    populateChatMenu();
    populateArexxMenu();
}

HOOKPROTONHNONP(OpenChatProviderSettingsMenuFunc, void) {
    openChatProviderSettingsRequesterWindow();
}
MakeHook(OpenChatProviderSettingsMenuHook, OpenChatProviderSettingsMenuFunc);

HOOKPROTONHNONP(OpenImageProviderSettingsMenuFunc, void) {
    openImageProviderSettingsRequesterWindow();
}
MakeHook(OpenImageProviderSettingsMenuHook, OpenImageProviderSettingsMenuFunc);

void addMenuActions() {
    Object printMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_PRINT);
    DoMethod(printMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
             MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
             APP_ID_PRINT);

    Object aboutAmigaGPTMenuItem =
        DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_ABOUT_AMIGAGPT);
    DoMethod(aboutAmigaGPTMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &AboutAmigaGPTMenuItemClickedHook);

    Object aboutMUIMenuItem =
        DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_ABOUT_MUI);
    DoMethod(aboutMUIMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_Application_AboutMUI, mainWindowObject);

    Object quitMenuItem =
        DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_QUIT);
    DoMethod(quitMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger, MUIV_EveryTime,
             MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);

    Object cutMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_CUT);
    if (isAROS) {
        set(cutMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(cutMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
                 &EditCutHook);
    }

    Object copyMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_COPY);
    if (isAROS) {
        set(copyMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(copyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
                 &EditCopyHook);
    }

    Object pasteMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_PASTE);
    if (isAROS) {
        set(pasteMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(pasteMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
                 &EditPasteHook);
    }

    Object clearMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_CLEAR);
    if (isAROS) {
        set(clearMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(clearMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
                 &EditClearHook);
    }

    Object selectAllMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_SELECT_ALL);
    if (isAROS) {
        set(selectAllMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(selectAllMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
                 &EditSelectAllHook);
    }

    Object muiSettingsMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                  MENU_ITEM_VIEW_MUI_SETTINGS);
    DoMethod(muiSettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 1,
             MUIM_Application_OpenConfigWindow);

    Object clearMuiSettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_VIEW_CLEAR_MUI_SETTINGS);
    DoMethod(clearMuiSettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &ClearMuiSettingsHook);

    Object fixedWidthFontsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_VIEW_FIXED_WIDTH_FONTS);
    set(fixedWidthFontsMenuItem, MUIA_Menuitem_Checked,
        configGetFixedWidthFonts());
    DoMethod(fixedWidthFontsMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &FixedWidthFontsMenuItemClickedHook);

    Object userTextAlignmentLeftMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT_LEFT);
    set(userTextAlignmentLeftMenuItem, MUIA_Menuitem_Checked,
        configGetUserTextAlignment() == ALIGN_LEFT);
    DoMethod(userTextAlignmentLeftMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_UserTextAlignment, ALIGN_LEFT);
    DoMethod(userTextAlignmentLeftMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &TextAlignmentChangedHook, MUIV_TriggerValue);

    Object userTextAlignmentCenterMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT_CENTER);
    set(userTextAlignmentCenterMenuItem, MUIA_Menuitem_Checked,
        configGetUserTextAlignment() == ALIGN_CENTER);
    DoMethod(userTextAlignmentCenterMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_UserTextAlignment, ALIGN_CENTER);
    DoMethod(userTextAlignmentCenterMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_CallHook, &TextAlignmentChangedHook, MUIV_TriggerValue);

    Object userTextAlignmentRightMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_VIEW_USER_TEXT_ALIGNMENT_RIGHT);
    set(userTextAlignmentRightMenuItem, MUIA_Menuitem_Checked,
        configGetUserTextAlignment() == ALIGN_RIGHT);
    DoMethod(userTextAlignmentRightMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_UserTextAlignment, ALIGN_RIGHT);
    DoMethod(userTextAlignmentRightMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &TextAlignmentChangedHook);

    Object assistantTextAlignmentLeftMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT_LEFT);
    set(assistantTextAlignmentLeftMenuItem, MUIA_Menuitem_Checked,
        configGetAssistantTextAlignment() == ALIGN_LEFT);
    DoMethod(assistantTextAlignmentLeftMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_AssistantTextAlignment, ALIGN_LEFT);
    DoMethod(assistantTextAlignmentLeftMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_CallHook, &TextAlignmentChangedHook);

    Object assistantTextAlignmentCenterMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT_CENTER);
    set(assistantTextAlignmentCenterMenuItem, MUIA_Menuitem_Checked,
        configGetAssistantTextAlignment() == ALIGN_CENTER);
    DoMethod(assistantTextAlignmentCenterMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_AssistantTextAlignment, ALIGN_CENTER);
    DoMethod(assistantTextAlignmentCenterMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_CallHook, &TextAlignmentChangedHook);

    Object assistantTextAlignmentRightMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_VIEW_ASSISTANT_TEXT_ALIGNMENT_RIGHT);
    set(assistantTextAlignmentRightMenuItem, MUIA_Menuitem_Checked,
        configGetAssistantTextAlignment() == ALIGN_RIGHT);
    DoMethod(assistantTextAlignmentRightMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_AssistantTextAlignment, ALIGN_RIGHT);
    DoMethod(assistantTextAlignmentRightMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_CallHook, &TextAlignmentChangedHook);

    Object proxyEnabledMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY_ENABLED);
    set(proxyEnabledMenuItem, MUIA_Menuitem_Checked, configGetProxyEnabled());
    DoMethod(proxyEnabledMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_ProxyEnabled, MUIV_TriggerValue);

    Object proxySettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY_SETTINGS);
    DoMethod(proxySettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, proxySettingsRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);

    Object speechEnabledMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_ENABLED);
    set(speechEnabledMenuItem, MUIA_Menuitem_Checked, configGetSpeechEnabled());
    DoMethod(speechEnabledMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_SpeechEnabled, MUIV_TriggerValue);

    Object speechProviderSettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_PROVIDER_SETTINGS);
    DoMethod(speechProviderSettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_Application_ReturnID, APP_ID_SPEECH_PROVIDER_SETTINGS);

    Object openAIAPIKeyMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                   MENU_ITEM_AI_OPENAI_API_KEY);
    DoMethod(openAIAPIKeyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, apiKeyRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);
    DoMethod(openAIAPIKeyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, apiKeyRequesterString, 3, MUIM_Set,
             MUIA_String_Contents, configGetOpenAiApiKey());
    DoMethod(openAIAPIKeyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, apiKeyRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_ActiveObject, apiKeyRequesterString);

    /* Chat menu actions */
    Object chatSystemMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_CHAT_SYSTEM);
    DoMethod(chatSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, chatSystemRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);
    DoMethod(chatSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, chatSystemRequesterString, 3, MUIM_Set,
             MUIA_String_Contents, configGetChatSystem());
    DoMethod(chatSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, chatSystemRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_ActiveObject, chatSystemRequesterString);

    Object chatWebSearchEnabledMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CHAT_WEB_SEARCH_ENABLED);
    set(chatWebSearchEnabledMenuItem, MUIA_Menuitem_Checked,
        configGetWebSearchEnabled());
    DoMethod(chatWebSearchEnabledMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_WebSearchEnabled, MUIV_TriggerValue);

    Object chatShellToolEnabledMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CHAT_SHELL_TOOL_ENABLED);
    set(chatShellToolEnabledMenuItem, MUIA_Menuitem_Checked,
        configGetShellToolEnabled());
    DoMethod(chatShellToolEnabledMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, configObj, 3, MUIM_Set,
             MUIA_AmigaGPTConfig_ShellToolEnabled, MUIV_TriggerValue);

    Object chatProviderSettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CHAT_PROVIDER_SETTINGS);
    DoMethod(chatProviderSettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_Application_ReturnID, APP_ID_CHAT_PROVIDER_SETTINGS);

    /* Image menu actions */
    Object imageProviderSettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_IMAGE_PROVIDER_SETTINGS);
    DoMethod(imageProviderSettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2,
             MUIM_Application_ReturnID, APP_ID_IMAGE_PROVIDER_SETTINGS);

    Object arexShellMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                MENU_ITEM_AREXX_AREXX_SHELL);
    DoMethod(arexShellMenuItem, MUIM_Notify, MUIA_Menuitem_Checked, TRUE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &OpenARexxShellFuncHook);
    DoMethod(arexShellMenuItem, MUIM_Notify, MUIA_Menuitem_Checked, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &CloseARexxShellFuncHook);

    Object arexImportScriptMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_AREXX_IMPORT_SCRIPT);
    DoMethod(arexImportScriptMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook,
             &ARexxImportScriptMenuItemClickedHook, MUIV_TriggerValue);

    Object openDocumentationMenuItem =
        DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_HELP_OPEN_DOCUMENTATION);
    DoMethod(openDocumentationMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook,
             &OpenDocumentationMenuItemClickedHook, MUIV_TriggerValue);
}

/**
 * Populate the Speech menu with the installed speech engines
 **/
static void populateSpeechMenu() {
    DoMethod(menuStrip, MUIM_Menustrip_InitChange);

    Object *speechSystemMenuItem =
        (Object *)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM);

    // Remove any existing speech model items
    Object *oldSpeechSystemMenuItem;
    while (oldSpeechSystemMenuItem =
               (Object *)DoMethod(speechSystemMenuItem, MUIM_Family_GetChild)) {
        DoMethod(speechSystemMenuItem, MUIM_Family_Remove,
                 oldSpeechSystemMenuItem);
        DisposeObject(oldSpeechSystemMenuItem);
    }

    // Populate the speech system menu with the installed speech engines
    for (UBYTE i = 0; SPEECH_SYSTEM_NAMES[i] != NULL; i++) {
#ifndef __AMIGAOS3__
        if (i == SPEECH_SYSTEM_34 || i == SPEECH_SYSTEM_37)
            continue;
#endif
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
        if (i == SPEECH_SYSTEM_FLITE)
            continue;
#endif
        Object *newSpeechSystemMenuItem = MenuitemObject, MUIA_Menuitem_Title,
               SPEECH_SYSTEM_NAMES[i], MUIA_Menuitem_Checkit, TRUE,
               MUIA_Menuitem_Checked, configGetSpeechSystem() == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(speechSystemMenuItem, MUIM_Family_AddTail,
                 newSpeechSystemMenuItem);
        DoMethod(newSpeechSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Application, 3, MUIM_CallHook,
                 &SpeechSystemMenuItemClickedHook, i);
        DoMethod(newSpeechSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }

#ifdef __AMIGAOS4__
    Object *fliteVoiceMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE);
    Object *oldFliteVoiceMenuItem;
    while (oldFliteVoiceMenuItem =
               (Object *)DoMethod(fliteVoiceMenuItem, MUIM_Family_GetChild)) {
        DoMethod(fliteVoiceMenuItem, MUIM_Family_Remove, oldFliteVoiceMenuItem);
        DisposeObject(oldFliteVoiceMenuItem);
    }
    for (UBYTE i = 0; SPEECH_FLITE_VOICE_NAMES[i] != NULL; i++) {
        Object *newFliteVoiceMenuItem = MenuitemObject, MUIA_Menuitem_Title,
               SPEECH_FLITE_VOICE_NAMES[i], MUIA_Menuitem_Checkit, TRUE,
               MUIA_Menuitem_Checked, configGetSpeechFliteVoice() == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(fliteVoiceMenuItem, MUIM_Family_AddTail,
                 newFliteVoiceMenuItem);
        DoMethod(newFliteVoiceMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, configObj, 3, MUIM_Set,
                 MUIA_AmigaGPTConfig_SpeechFliteVoice, i);
        DoMethod(newFliteVoiceMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }
#endif

    Object *openAIVoiceMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE);
    Object *oldOpenAIVoiceMenuItem;
    while (oldOpenAIVoiceMenuItem =
               (Object *)DoMethod(openAIVoiceMenuItem, MUIM_Family_GetChild)) {
        DoMethod(openAIVoiceMenuItem, MUIM_Family_Remove,
                 oldOpenAIVoiceMenuItem);
        DisposeObject(oldOpenAIVoiceMenuItem);
    }
    for (UBYTE i = 0; OPENAI_TTS_VOICE_NAMES[i] != NULL; i++) {
        Object *newOpenAIVoiceMenuItem = MenuitemObject, MUIA_Menuitem_Title,
               OPENAI_TTS_VOICE_NAMES[i], MUIA_Menuitem_Checkit, TRUE,
               MUIA_Menuitem_Checked, configGetOpenAITTSVoice() == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(openAIVoiceMenuItem, MUIM_Family_AddTail,
                 newOpenAIVoiceMenuItem);
        DoMethod(newOpenAIVoiceMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, configObj, 3, MUIM_Set,
                 MUIA_AmigaGPTConfig_OpenAITTSVoice, i);
        DoMethod(newOpenAIVoiceMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }

    Object *openAITTSModelMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_MODEL);
    Object *oldOpenAITTSModelMenuItem;
    while (oldOpenAITTSModelMenuItem = (Object *)DoMethod(
               openAITTSModelMenuItem, MUIM_Family_GetChild)) {
        DoMethod(openAITTSModelMenuItem, MUIM_Family_Remove,
                 oldOpenAITTSModelMenuItem);
        DisposeObject(oldOpenAITTSModelMenuItem);
    }
    for (UBYTE i = 0; OPENAI_TTS_MODEL_NAMES[i] != NULL; i++) {
        Object *newOpenAITTSModelMenuItem = MenuitemObject, MUIA_Menuitem_Title,
               OPENAI_TTS_MODEL_NAMES[i], MUIA_Menuitem_Checkit, TRUE,
               MUIA_Menuitem_Checked, configGetOpenAITTSModel() == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(openAITTSModelMenuItem, MUIM_Family_AddTail,
                 newOpenAITTSModelMenuItem);
        DoMethod(newOpenAITTSModelMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, configObj, 3, MUIM_Set,
                 MUIA_AmigaGPTConfig_OpenAITTSModel, i);
        DoMethod(newOpenAITTSModelMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }
    DoMethod(menuStrip, MUIM_Menustrip_ExitChange);
}

/**
 * Populate the Chat menu (web search and shell tool checkmarks)
 */
static void populateChatMenu() {
    DoMethod(menuStrip, MUIM_Menustrip_InitChange);

    // Set the checkmarks for web search and shell tool based on config
    Object *webSearchMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CHAT_WEB_SEARCH_ENABLED);
    if (webSearchMenuItem) {
        set(webSearchMenuItem, MUIA_Menuitem_Checked,
            configGetWebSearchEnabled());
    }

    Object *shellToolMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CHAT_SHELL_TOOL_ENABLED);
    if (shellToolMenuItem) {
        set(shellToolMenuItem, MUIA_Menuitem_Checked,
            configGetShellToolEnabled());
    }

    DoMethod(menuStrip, MUIM_Menustrip_ExitChange);
}

/**
 * Populate the AREXX menu with the installed scripts
 **/
static void populateArexxMenu() {
    DoMethod(menuStrip, MUIM_Menustrip_InitChange);

    Object arexxRunScriptMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_AREXX_RUN_SCRIPT);

    // Remove any existing script items
    Object *scriptMenuItem = NULL;
    while (scriptMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                             MENU_ITEM_AREXX_SCRIPT)) {
        DoMethod(arexxRunScriptMenuItem, MUIM_Family_Remove, scriptMenuItem);
        DisposeObject(scriptMenuItem);
    }

    // Scan the rexx directory for .rexx files
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    BPTR lock = Lock("AMIGAGPT:rexx", ACCESS_READ);
    if (lock != 0) {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        if (fib != NULL) {
            if (Examine(lock, fib)) {
                // Iterate through all files in the directory
                while (ExNext(lock, fib)) {
                    // Check if this is a file (not a directory) and has
                    // .rexx extension
                    STRPTR filename = fib->fib_FileName;
                    LONG len = strlen(filename);

                    if (len > 5 && !stricmp(&filename[len - 5], ".rexx")) {
                        Object *newScriptMenuItem = MenuitemObject,
                               MUIA_Menuitem_Title, filename,
                               MUIA_Menuitem_CopyStrings, TRUE, MUIA_UserData,
                               (APTR)MENU_ITEM_AREXX_SCRIPT, End;
                        DoMethod(arexxRunScriptMenuItem, MUIM_Family_AddTail,
                                 newScriptMenuItem);
                        DoMethod(newScriptMenuItem, MUIM_Notify,
                                 MUIA_Menuitem_Trigger, MUIV_EveryTime,
                                 MUIV_Notify_Self, 2, MUIM_CallHook,
                                 &ARexxRunScriptMenuItemClickedHook);
                    }
                }
            }
            FreeDosObject(DOS_FIB, fib);
            UnLock(lock);
        }
    }
#else
    APTR context =
        ObtainDirContextTags(EX_StringNameInput, "AMIGAGPT:rexx", EX_DataFields,
                             (EXF_NAME | EXF_LINK | EXF_TYPE), TAG_END);
    if (context) {
        struct ExamineData *dat;

        while ((dat = ExamineDir(context))) /* until no more data.*/
        {
            if (EXD_IS_FILE(dat)) /* a file */
            {
                if (strlen(dat->Name) > 5 &&
                    !stricmp(&dat->Name[strlen(dat->Name) - 5], ".rexx")) {
                    Object *newScriptMenuItem = MenuitemObject,
                           MUIA_Menuitem_Title, dat->Name,
                           MUIA_Menuitem_CopyStrings, TRUE, MUIA_UserData,
                           (APTR)MENU_ITEM_AREXX_SCRIPT, End;
                    DoMethod(arexxRunScriptMenuItem, MUIM_Family_AddTail,
                             newScriptMenuItem);
                    DoMethod(newScriptMenuItem, MUIM_Notify,
                             MUIA_Menuitem_Trigger, MUIV_EveryTime,
                             MUIV_Notify_Self, 2, MUIM_CallHook,
                             &ARexxRunScriptMenuItemClickedHook);
                }
            }
        }
    } else {
        displayError(STRING_ERROR);
    }
    ReleaseDirContext(context);
#endif
    DoMethod(menuStrip, MUIM_Menustrip_ExitChange);
}

/**
 * @brief Build a NP_Path list.
 * @param extraDirs[]: array of assigns/paths like "SYS:Utilities",
 *MOSSYS:Utilities"
 * @param addParent: if TRUE, duplicate the callerÃÂ¢ÃÂÃÂs existing CLI path first
 * @returns BPTR to first node, or ZERO on failure (nothing allocated).
 **/
static BPTR BuildNPPath(const char *const *extraDirs, BOOL addParent) {
    struct PathNodeCompat *head = NULL, *tail = NULL;

// Optionally copy the callerÃÂ¢ÃÂÃÂs current shell path (duplicates the locks).
#ifndef __AMIGAOS4__
    if (addParent) {
        struct CommandLineInterface *cli =
            Cli(); // NULL if weÃÂ¢ÃÂÃÂre not in a shell
        if (cli && cli->cli_CommandDir) {
            for (struct PathNodeCompat *src =
                     (struct PathNodeCompat *)BADDR(cli->cli_CommandDir);
                 src; src = PN_NEXT(src)) {
                BPTR dup = DupLock(src->pc_Lock);
                if (!dup)
                    goto fail;

                struct PathNodeCompat *node = (struct PathNodeCompat *)AllocVec(
                    sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);
                if (!node) {
                    UnLock(dup);
                    goto fail;
                }

                node->pc_Lock = dup;
                if (tail)
                    tail->pc_Next = MKBADDR(node);
                else
                    head = node;
                tail = node;
            }
        }
    }
#endif

    // Append our extra directories
    const size_t extraCount = sizeof(extraDirs) / sizeof(extraDirs[0]);
    for (size_t i = 0; i < extraCount; ++i) {
        if (!extraDirs[i] || !extraDirs[i][0])
            continue;
        BPTR lock = Lock((STRPTR)extraDirs[i], ACCESS_READ);
        if (!lock)
            continue;

        struct PathNodeCompat *node = (struct PathNodeCompat *)AllocVec(
            sizeof(*node), MEMF_PUBLIC | MEMF_CLEAR);
        if (!node) {
            UnLock(lock);
            goto fail;
        }

        node->pc_Lock = lock;
        if (tail)
            tail->pc_Next = MKBADDR(node);
        else
            head = node;
        tail = node;
    }

    /* Note: current directory and C: are implicit; donÃÂ¢ÃÂÃÂt add them yourself. */
    /* C: is always searched last, current dir always first.                 */ /*  */

    return MKBADDR(head);

fail:
    if (head) {
        struct PathNodeCompat *n = head;
        while (n) {
            struct PathNodeCompat *next = PN_NEXT(n);
            if (n->pc_Lock)
                UnLock(n->pc_Lock);
            FreeVec(n);
            n = next;
        }
    }
    return NULL;
}

/**
 * @brief Free a NP_Path list we built if SystemTagList() fails (rc == -1).
 * @param listHead The head of the NP_Path list to free.
 **/
static void FreeNPPath(BPTR listHead) {
    if (!listHead)
        return;
    struct PathNodeCompat *n = (struct PathNodeCompat *)BADDR(listHead);
    while (n) {
        struct PathNodeCompat *next = PN_NEXT(n);
        if (n->pc_Lock)
            UnLock(n->pc_Lock);
        FreeVec(n);
        n = next;
    }
}