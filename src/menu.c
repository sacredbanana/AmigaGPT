#include <classes/arexx.h>
#include <dos/dostags.h>
#include <libraries/amigaguide.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <mui/TextEditor_mcc.h>
#include <proto/amigaguide.h>
#include <proto/exec.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include "APIKeyRequesterWindow.h"
#include "AboutAmigaGPTWindow.h"
#include "ARexx.h"
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "VoiceInstructionsRequesterWindow.h"
#include "version.h"

Object *menuStrip;

static void populateSpeechMenu();
static void populateOpenAIMenu();
static void populateArexxMenu();

HOOKPROTONHNONP(AboutAmigaGPTMenuItemClickedFunc, void) {
    printf("speech: %lu\n", config.speechEnabled);
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
              enum SpeechSystem *speechSystem) {
    closeSpeech();
    config.speechSystem = *speechSystem;
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
    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
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
            if (config.speechAccent != NULL) {
                FreeVec(config.speechAccent);
            }
            config.speechAccent = AllocVec(strlen(fileRequester->fr_File) + 1,
                                           MEMF_ANY | MEMF_CLEAR);
            strncpy(config.speechAccent, fileRequester->fr_File,
                    strlen(fileRequester->fr_File));
            if (writeConfig() == RETURN_ERROR) {
                displayError(STRING_ERROR_CONFIG_FILE_WRITE);
            }
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
                strlen(PROGDIR "rexx/") + strlen(fileName) + 1;
            STRPTR destinationPath =
                AllocVec(destinationPathLength, MEMF_CLEAR);
            strncpy(destinationPath, PROGDIR "rexx/", strlen(PROGDIR "rexx/"));
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
#ifndef __MORPHOS__
    DoMethod(arexxObject, AM_EXECUTE, scriptPath, NULL, NULL, NULL, NULL, NULL);
#else
    UBYTE command[1024] = {0};
    snprintf(command, sizeof(command), "RX %s", scriptPath);
    SystemTagList(scriptPath, TAG_DONE);
#endif
    FreeVec(scriptPath);
}
MakeHook(ARexxRunScriptMenuItemClickedHook, ARexxRunScriptMenuItemClickedFunc);

HOOKPROTONHNONP(OpenDocumentationMenuItemClickedFunc, void) {
    CONST_STRPTR guidePath = PROGDIR "AmigaGPT.guide";
    BPTR file = Open(guidePath, MODE_OLDFILE);
    if (file == NULL) {
        displayError(STRING_ERROR_DOCUMENTATION_OPEN);
        return;
    }
    Close(file);
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

void createMenu() {
    menuStrip = MenustripObject, MUIA_Family_Child, MenuObject, MUIA_Menu_Title,
    STRING_MENU_PROJECT, MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_ABOUT_AMIGAGPT,
    MUIA_UserData, MENU_ITEM_PROJECT_ABOUT_AMIGAGPT, MUIA_Menuitem_CopyStrings,
    FALSE, End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_ABOUT_MUI, MUIA_UserData, MENU_ITEM_PROJECT_ABOUT_MUI,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, NM_BARLABEL, MUIA_UserData, MENU_ITEM_NULL,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_QUIT, MUIA_UserData,
    MENU_ITEM_PROJECT_QUIT, MUIA_Menuitem_Shortcut, "Q",
    MUIA_Menuitem_CopyStrings, FALSE, End, End,

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

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_VIEW,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_MUI_SETTINGS, MUIA_UserData,
    MENU_ITEM_VIEW_MUI_SETTINGS, MUIA_Menuitem_CopyStrings, FALSE, End, End,

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_CONNECTION,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_PROXY, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_ENABLED,
    MUIA_UserData, MENU_ITEM_CONNECTION_PROXY_ENABLED, MUIA_Menuitem_Toggle,
    TRUE, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SETTINGS, MUIA_UserData, MENU_ITEM_CONNECTION_PROXY_SETTINGS,
    MUIA_Menuitem_CopyStrings, FALSE, End, End, End,

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_SPEECH,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_ENABLED, MUIA_UserData,
    MENU_ITEM_SPEECH_ENABLED, MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Toggle,
    TRUE, MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_SPEECH_SYSTEM,
    MUIA_UserData, MENU_ITEM_SPEECH_SYSTEM, MUIA_Menuitem_CopyStrings, FALSE,
    End,
#ifdef __AMIGAOS3__
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SPEECH_ACCENT, MUIA_UserData, MENU_ITEM_SPEECH_ACCENT,
    MUIA_Menuitem_CopyStrings, FALSE, End,
#endif
#ifdef __AMIGAOS4__
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_FLITE_VOICE, MUIA_UserData, MENU_ITEM_SPEECH_FLITE_VOICE,
    MUIA_Menuitem_CopyStrings, FALSE, End,
#endif
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_VOICE, MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_SPEECH_OPENAI_MODEL, MUIA_UserData,
    MENU_ITEM_SPEECH_OPENAI_MODEL, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_VOICE_INSTRUCTIONS, MUIA_UserData,
    MENU_ITEM_SPEECH_OPENAI_VOICE_INSTRUCTIONS, MUIA_Menuitem_CopyStrings,
    FALSE, End, End,

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_OPENAI,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_OPENAI_API_KEY, MUIA_UserData,
    MENU_ITEM_OPENAI_API_KEY, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_CHAT_SYSTEM, MUIA_UserData, MENU_ITEM_OPENAI_CHAT_SYSTEM,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_OPENAI_CHAT_MODEL, MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_IMAGE_MODEL, MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_MODEL,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_OPENAI_IMAGE_SIZE_DALL_E_2, MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_IMAGE_SIZE_DALL_E_3, MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_IMAGE_SIZE_GPT_IMAGE_1, MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_GPT_IMAGE_1, MUIA_Menuitem_CopyStrings, FALSE,
    End, End,

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

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_HELP,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_OPEN_DOCUMENTATION, MUIA_UserData,
    MENU_ITEM_HELP_OPEN_DOCUMENTATION, MUIA_Menuitem_CopyStrings, FALSE, End,
    End, End;

    populateSpeechMenu();
    populateOpenAIMenu();
    populateArexxMenu();
}

void addMenuActions() {
    Object aboutAmigaGPTMenuItem =
        DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_ABOUT_AMIGAGPT);
    DoMethod(aboutAmigaGPTMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook,
             &AboutAmigaGPTMenuItemClickedHook, MUIV_TriggerValue);

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
                 MUIV_EveryTime, chatInputTextEditor, 2,
                 MUIM_TextEditor_ARexxCmd, "Cut");
    }

    Object copyMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_COPY);
    if (isAROS) {
        set(copyMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(copyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, chatInputTextEditor, 2,
                 MUIM_TextEditor_ARexxCmd, "Copy");
    }

    Object pasteMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_PASTE);
    if (isAROS) {
        set(pasteMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(pasteMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, chatInputTextEditor, 2,
                 MUIM_TextEditor_ARexxCmd, "Paste");
    }

    Object clearMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_CLEAR);
    if (isAROS) {
        set(clearMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(clearMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, chatInputTextEditor, 3, MUIM_Set,
                 MUIA_String_Contents, "");
    }

    Object selectAllMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_SELECT_ALL);
    if (isAROS) {
        set(selectAllMenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        DoMethod(selectAllMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, chatInputTextEditor, 2,
                 MUIM_TextEditor_ARexxCmd, "SelectAll");
    }

    Object muiSettingsMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                  MENU_ITEM_VIEW_MUI_SETTINGS);
    DoMethod(muiSettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 1,
             MUIM_Application_OpenConfigWindow);

    Object proxyEnabledMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY_ENABLED);
    set(proxyEnabledMenuItem, MUIA_Menuitem_Checked, config.proxyEnabled);
    DoMethod(proxyEnabledMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, proxyEnabledMenuItem, 3, MUIM_WriteLong,
             MUIV_TriggerValue, &config.proxyEnabled);

    Object proxySettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY_SETTINGS);
    DoMethod(proxySettingsMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, proxySettingsRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);

    Object speechEnabledMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_ENABLED);
    set(speechEnabledMenuItem, MUIA_Menuitem_Checked, config.speechEnabled);
    DoMethod(speechEnabledMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             MUIV_EveryTime, speechEnabledMenuItem, 3, MUIM_WriteLong,
             MUIV_TriggerValue, &config.speechEnabled);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    Object speechAccentMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_ACCENT);
    DoMethod(speechAccentMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechAccentMenuItemClickedHook);
#endif

    Object openAIVoiceInstructionsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_INSTRUCTIONS);
    DoMethod(openAIVoiceInstructionsMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime,
             voiceInstructionsRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);
    DoMethod(openAIVoiceInstructionsMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime,
             voiceInstructionsRequesterString, 3, MUIM_Set,
             MUIA_String_Contents, config.openAIVoiceInstructions);
    DoMethod(openAIVoiceInstructionsMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime,
             voiceInstructionsRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_ActiveObject, voiceInstructionsRequesterString);

    Object openAIAPIKeyMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_API_KEY);
    DoMethod(openAIAPIKeyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, apiKeyRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);
    DoMethod(openAIAPIKeyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, apiKeyRequesterString, 3, MUIM_Set,
             MUIA_String_Contents, config.openAiApiKey);
    DoMethod(openAIAPIKeyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, apiKeyRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_ActiveObject, apiKeyRequesterString);

    Object openAIChatSystemMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_SYSTEM);
    DoMethod(openAIChatSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, chatSystemRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, TRUE);
    DoMethod(openAIChatSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, chatSystemRequesterString, 3, MUIM_Set,
             MUIA_String_Contents, config.chatSystem);
    DoMethod(openAIChatSystemMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, chatSystemRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_ActiveObject, chatSystemRequesterString);

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
               MUIA_Menuitem_Checked, config.speechSystem == i,
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
               MUIA_Menuitem_Checked, config.speechFliteVoice == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(fliteVoiceMenuItem, MUIM_Family_AddTail,
                 newFliteVoiceMenuItem);
        DoMethod(newFliteVoiceMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Self, 3, MUIM_WriteLong, i,
                 &config.speechFliteVoice);
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
               MUIA_Menuitem_Checked, config.openAITTSVoice == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(openAIVoiceMenuItem, MUIM_Family_AddTail,
                 newOpenAIVoiceMenuItem);
        DoMethod(newOpenAIVoiceMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Self, 3, MUIM_WriteLong, i,
                 &config.openAITTSVoice);
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
               MUIA_Menuitem_Checked, config.openAITTSModel == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(openAITTSModelMenuItem, MUIM_Family_AddTail,
                 newOpenAITTSModelMenuItem);
        DoMethod(newOpenAITTSModelMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Self, 3, MUIM_WriteLong, i,
                 &config.openAITTSModel);
        DoMethod(newOpenAITTSModelMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }
    DoMethod(menuStrip, MUIM_Menustrip_ExitChange);
}

/**
 * Populate the OpenAI menu with the models and image sizes
 */
static void populateOpenAIMenu() {
    DoMethod(menuStrip, MUIM_Menustrip_InitChange);

    Object *openAIChatModelMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL);

    // Remove any existing chat model items
    Object *chatModelMenuItem;
    while (chatModelMenuItem = (Object *)DoMethod(openAIChatModelMenuItem,
                                                  MUIM_Family_GetChild)) {
        DoMethod(openAIChatModelMenuItem, MUIM_Family_Remove,
                 chatModelMenuItem);
        DisposeObject(chatModelMenuItem);
    }

    // Populate the chat model menu with the models
    for (UBYTE i = 0; CHAT_MODEL_NAMES[i] != NULL; i++) {
        Object *newChatModelMenuItem = MenuitemObject, MUIA_Menuitem_Title,
               CHAT_MODEL_NAMES[i], MUIA_Menuitem_Checkit, TRUE,
               MUIA_Menuitem_Checked, config.chatModel == i,
               MUIA_Menuitem_Toggle, TRUE, MUIA_Menuitem_Exclude, ~0,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(openAIChatModelMenuItem, MUIM_Family_AddTail,
                 newChatModelMenuItem);
        DoMethod(newChatModelMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
        DoMethod(newChatModelMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_WriteLong, i,
                 &config.chatModel);
    }

    Object *openAIImageModelMenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_MODEL);

    // Remove any existing image model items
    Object *imageModelMenuItem;
    while (imageModelMenuItem = (Object *)DoMethod(openAIImageModelMenuItem,
                                                   MUIM_Family_GetChild)) {
        DoMethod(openAIImageModelMenuItem, MUIM_Family_Remove,
                 imageModelMenuItem);
        DisposeObject(imageModelMenuItem);
    }

    // Populate the image model menu with the models
    for (UBYTE i = 0; IMAGE_MODEL_NAMES[i] != NULL; i++) {
        Object *newImageModelMenuItem = MenuitemObject, MUIA_Menuitem_Title,
               IMAGE_MODEL_NAMES[i], MUIA_Menuitem_Checkit, TRUE,
               MUIA_Menuitem_Checked, config.imageModel == i,
               MUIA_Menuitem_Exclude, ~(1 << i), MUIA_Menuitem_Toggle, TRUE,
               MUIA_Menuitem_CopyStrings, FALSE, End;
        DoMethod(openAIImageModelMenuItem, MUIM_Family_AddTail,
                 newImageModelMenuItem);
        DoMethod(newImageModelMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Self, 3, MUIM_WriteLong, i,
                 &config.imageModel);
    }

    Object *openAIImageSizeDALL_E_2MenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2);

    // Remove any existing image size items
    Object *imageSizeDallE2MenuItem;
    while (imageSizeDallE2MenuItem = (Object *)DoMethod(
               openAIImageSizeDALL_E_2MenuItem, MUIM_Family_GetChild)) {
        DoMethod(openAIImageSizeDALL_E_2MenuItem, MUIM_Family_Remove,
                 imageSizeDallE2MenuItem);
        DisposeObject(imageSizeDallE2MenuItem);
    }

    // Populate the image size menu with the sizes
    for (UBYTE i = 0; IMAGE_SIZES_DALL_E_2[i] != IMAGE_SIZE_NULL; i++) {
        enum ImageSize imageSize = IMAGE_SIZES_DALL_E_2[i];
        Object *newImageSizeDallE2MenuItem = MenuitemObject,
               MUIA_Menuitem_Title, IMAGE_SIZE_NAMES[imageSize],
               MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
               config.imageSizeDallE2 == imageSize, MUIA_Menuitem_Exclude,
               ~(1 << i), MUIA_Menuitem_Toggle, TRUE, MUIA_Menuitem_CopyStrings,
               FALSE, End;
        DoMethod(openAIImageSizeDALL_E_2MenuItem, MUIM_Family_AddTail,
                 newImageSizeDallE2MenuItem);
        DoMethod(newImageSizeDallE2MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Self, 3, MUIM_WriteLong, imageSize,
                 &config.imageSizeDallE2);
    }

    Object *openAIImageSizeDALL_E_3MenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3);

    // Remove any existing image size items
    Object *imageSizeDallE3MenuItem;
    while (imageSizeDallE3MenuItem = (Object *)DoMethod(
               openAIImageSizeDALL_E_3MenuItem, MUIM_Family_GetChild)) {
        DoMethod(openAIImageSizeDALL_E_3MenuItem, MUIM_Family_Remove,
                 imageSizeDallE3MenuItem);
        DisposeObject(imageSizeDallE3MenuItem);
    }

    // Populate the image size menu with the sizes
    for (UBYTE i = 0; IMAGE_SIZES_DALL_E_3[i] != IMAGE_SIZE_NULL; i++) {
        enum ImageSize imageSize = IMAGE_SIZES_DALL_E_3[i];
        Object *newImageSizeDallE3MenuItem = MenuitemObject,
               MUIA_Menuitem_Title, IMAGE_SIZE_NAMES[imageSize],
               MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
               config.imageSizeDallE3 == imageSize, MUIA_Menuitem_Exclude,
               ~(1 << i), MUIA_Menuitem_Toggle, TRUE, MUIA_Menuitem_CopyStrings,
               FALSE, End;
        DoMethod(openAIImageSizeDALL_E_3MenuItem, MUIM_Family_AddTail,
                 newImageSizeDallE3MenuItem);
        DoMethod(newImageSizeDallE3MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Self, 3, MUIM_WriteLong, imageSize,
                 &config.imageSizeDallE3);
    }

    Object *openAIImageSizeGPT_IMAGE_1MenuItem = (Object *)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_SIZE_GPT_IMAGE_1);

    // Remove any existing image size items
    Object *imageSizeGptImage1MenuItem;
    while (imageSizeGptImage1MenuItem = (Object *)DoMethod(
               openAIImageSizeGPT_IMAGE_1MenuItem, MUIM_Family_GetChild)) {
        DoMethod(openAIImageSizeGPT_IMAGE_1MenuItem, MUIM_Family_Remove,
                 imageSizeGptImage1MenuItem);
        DisposeObject(imageSizeGptImage1MenuItem);
    }

    // Populate the image size menu with the sizes
    for (UBYTE i = 0; IMAGE_SIZES_GPT_IMAGE_1[i] != IMAGE_SIZE_NULL; i++) {
        enum ImageSize imageSize = IMAGE_SIZES_GPT_IMAGE_1[i];
        Object *newImageSizeGptImage1MenuItem = MenuitemObject,
               MUIA_Menuitem_Title, IMAGE_SIZE_NAMES[imageSize],
               MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Checked,
               config.imageSizeGptImage1 == imageSize, MUIA_Menuitem_Exclude,
               ~(1 << i), MUIA_Menuitem_Toggle, TRUE, MUIA_Menuitem_CopyStrings,
               FALSE, End;
        DoMethod(openAIImageSizeGPT_IMAGE_1MenuItem, MUIM_Family_AddTail,
                 newImageSizeGptImage1MenuItem);
        DoMethod(newImageSizeGptImage1MenuItem, MUIM_Notify,
                 MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Self, 3,
                 MUIM_WriteLong, imageSize, &config.imageSizeGptImage1);
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
    BPTR lock = Lock(PROGDIR "rexx", ACCESS_READ);
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
        ObtainDirContextTags(EX_StringNameInput, PROGDIR "rexx", EX_DataFields,
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