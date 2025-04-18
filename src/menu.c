#include <classes/arexx.h>
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

static void populateArexxMenu();

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

HOOKPROTONHNONP(ARexxShellMenuItemClickedFunc, void) {
    CONST_STRPTR arexxPath = PROGDIR "ARexxShell.arexx";
    BPTR file = Open(arexxPath, MODE_OLDFILE);
    if (file == NULL) {
        displayError(STRING_ERROR_AREXX_SHELL_OPEN);
    }
}
MakeHook(ARexxShellMenuItemClickedHook, ARexxShellMenuItemClickedFunc);

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
    DoMethod(arexxObject, AM_EXECUTE, scriptPath, NULL, NULL, NULL, NULL, NULL);
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
    MUIA_Menuitem_Title, STRING_MENU_ENABLED, MUIA_UserData,
    MENU_ITEM_CONNECTION_PROXY_ENABLED, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_SETTINGS, MUIA_UserData,
    MENU_ITEM_CONNECTION_PROXY_SETTINGS, MUIA_Menuitem_CopyStrings, FALSE, End,
    End,

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_SPEECH,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_ENABLED, MUIA_UserData,
    MENU_ITEM_SPEECH_ENABLED, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_SPEECH_SYSTEM, MUIA_UserData,
    MENU_ITEM_SPEECH_SYSTEM, MUIA_Menuitem_CopyStrings, FALSE,
#ifdef __AMIGAOS3__
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SPEECH_SYSTEM_34, MUIA_UserData, MENU_ITEM_SPEECH_SYSTEM_34,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~(1 << 0),
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_SPEECH_SYSTEM_37, MUIA_UserData,
    MENU_ITEM_SPEECH_SYSTEM_37, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 1), MUIA_Menuitem_CopyStrings, FALSE, End,
#endif
#ifdef __AMIGAOS4__
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SPEECH_SYSTEM_FLITE, MUIA_UserData,
    MENU_ITEM_SPEECH_SYSTEM_FLITE, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 0), MUIA_Menuitem_CopyStrings, FALSE, End,
#endif
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SPEECH_SYSTEM_OPENAI, MUIA_UserData,
    MENU_ITEM_SPEECH_SYSTEM_OPENAI, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 3), MUIA_Menuitem_CopyStrings, FALSE, End,
    End,
#ifdef __AMIGAOS3__
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SPEECH_ACCENT, MUIA_UserData, MENU_ITEM_SPEECH_ACCENT,
    MUIA_Menuitem_CopyStrings, FALSE, End,
#endif
#ifdef __AMIGAOS4__
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_FLITE_VOICE, MUIA_UserData, MENU_ITEM_SPEECH_FLITE_VOICE,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_FLITE_VOICE_KAL, MUIA_UserData,
    MENU_ITEM_SPEECH_FLITE_VOICE_KAL, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_FLITE_VOICE_KAL16, MUIA_UserData,
    MENU_ITEM_SPEECH_FLITE_VOICE_KAL16, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_FLITE_VOICE_AWB, MUIA_UserData,
    MENU_ITEM_SPEECH_FLITE_VOICE_AWB, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_FLITE_VOICE_RMS, MUIA_UserData,
    MENU_ITEM_SPEECH_FLITE_VOICE_RMS, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_FLITE_VOICE_SLT, MUIA_UserData,
    MENU_ITEM_SPEECH_FLITE_VOICE_SLT, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_CopyStrings, FALSE, End, End,
#endif
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_VOICE, MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "alloy", MUIA_UserData,
    MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 0), MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "ash",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_ASH, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 1), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "ballad",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_BALLAD, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 2), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "coral",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_CORAL, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 3), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "echo",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 4), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "fable",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 5), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "onyx",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 6), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "nova",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 7), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "sage",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_SAGE, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 8), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "shimmer",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 9), MUIA_Menuitem_CopyStrings, FALSE,
    End, End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_SPEECH_OPENAI_MODEL, MUIA_UserData,
    MENU_ITEM_SPEECH_OPENAI_MODEL, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "tts-1",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 0), MUIA_Menuitem_CopyStrings, FALSE,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "tts-1-hd",
    MUIA_UserData, MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~(1 << 1),
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4o-mini-tts", MUIA_UserData,
    MENU_ITEM_SPEECH_OPENAI_MODEL_GPT_4o_MINI_TTS, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 2), MUIA_Menuitem_CopyStrings, FALSE, End,
    End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
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
    MENU_ITEM_OPENAI_CHAT_MODEL, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "chatgpt-4o-latest",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_CHATGPT_4o_LATEST,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-3.5-turbo", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "gpt-3.5-turbo-0125", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_0125, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "gpt-3.5-turbo-1106", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_1106, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "gpt-4-turbo",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4-turbo-2024-04-09", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_2024_04_09, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "gpt-4-turbo-preview", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_PREVIEW, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "gpt-4",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "gpt-4-0613",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0613,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4.1", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_1, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "gpt-4.1-2024-04-14", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_1_2024_04_14, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "gpt-4.5-preview",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_5_PREVIEW,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4.5-preview-2025-02-27", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_5_PREVIEW_2025_02_27,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4o", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "gpt-4o-2024-11-20",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_11_20,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4o-2024-08-06", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_08_06, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "gpt-4o-2024-05-13",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_05_13,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "gpt-4o-mini", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "gpt-4o-mini-2024-07-18", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI_2024_07_18, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o1", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_o1, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o1-preview",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_PREVIEW,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "o1-preview-2024-09-12", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_o1_PREVIEW_2024_09_12, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o1-mini",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_MINI, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "o1-mini-2024-09-12", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_o1_MINI_2024_09_12, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o1-pro",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_PRO, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o1-pro-2025-03-19",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_PRO_2025_03_19,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "o3", MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o3,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~0,
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "o3-2025-04-16", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_o3_2025_04_16, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o3-mini",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o3_MINI, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "o3-mini-2025-01-31", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_o3_MINI_2025_01_31, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "o4-mini",
    MUIA_UserData, MENU_ITEM_OPENAI_CHAT_MODEL_o4_MINI, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    "o4-mini-2025-04-16", MUIA_UserData,
    MENU_ITEM_OPENAI_CHAT_MODEL_o4_MINI_2025_04_16, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~0, MUIA_Menuitem_CopyStrings, FALSE, End, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_IMAGE_MODEL, MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_MODEL,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "dall-e-2", MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_2, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 0), MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "dall-e-3",
    MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_3, MUIA_Menuitem_Checkit,
    TRUE, MUIA_Menuitem_Exclude, ~(1 << 1), MUIA_Menuitem_CopyStrings, FALSE,
    End, End, MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title,
    STRING_MENU_OPENAI_IMAGE_SIZE_DALL_E_2, MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2, MUIA_Menuitem_CopyStrings, FALSE,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "256x256",
    MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_256X256,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~(1 << 0),
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "512x512", MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_512X512, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 1), MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "1024x1024",
    MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_1024X1024,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~(1 << 2),
    MUIA_Menuitem_CopyStrings, FALSE, End, End, MUIA_Family_Child,
    MenuitemObject, MUIA_Menuitem_Title, STRING_MENU_OPENAI_IMAGE_SIZE_DALL_E_3,
    MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3,
    MUIA_Menuitem_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "1024x1024", MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1024, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 0), MUIA_Menuitem_CopyStrings, FALSE, End,
    MUIA_Family_Child, MenuitemObject, MUIA_Menuitem_Title, "1792x1024",
    MUIA_UserData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1792X1024,
    MUIA_Menuitem_Checkit, TRUE, MUIA_Menuitem_Exclude, ~(1 << 1),
    MUIA_Menuitem_CopyStrings, FALSE, End, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, "1024x1792", MUIA_UserData,
    MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1792, MUIA_Menuitem_Checkit, TRUE,
    MUIA_Menuitem_Exclude, ~(1 << 2), MUIA_Menuitem_CopyStrings, FALSE, End,
    End, End,

    MUIA_Family_Child, MenuObject, MUIA_Menu_Title, STRING_MENU_AREXX,
    MUIA_Menu_CopyStrings, FALSE, MUIA_Family_Child, MenuitemObject,
    MUIA_Menuitem_Title, STRING_MENU_AREXX_AREXX_SHELL, MUIA_UserData,
    MENU_ITEM_AREXX_AREXX_SHELL, MUIA_Menuitem_CopyStrings, FALSE, End,
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
    End,

    End;
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

    Object speechSystem34MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_34);
    if (isAROS) {
        set(speechSystem34MenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        set(speechSystem34MenuItem, MUIA_Menuitem_Checked,
            config.speechSystem == SPEECH_SYSTEM_34);
        DoMethod(speechSystem34MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Application, 3, MUIM_CallHook,
                 &SpeechSystemMenuItemClickedHook, SPEECH_SYSTEM_34);
        DoMethod(speechSystem34MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }

    Object speechSystem37MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_37);
    if (isAROS) {
        set(speechSystem37MenuItem, MUIA_Menuitem_Enabled, FALSE);
    } else {
        set(speechSystem37MenuItem, MUIA_Menuitem_Checked,
            config.speechSystem == SPEECH_SYSTEM_37);
        DoMethod(speechSystem37MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
                 TRUE, MUIV_Notify_Application, 3, MUIM_CallHook,
                 &SpeechSystemMenuItemClickedHook, SPEECH_SYSTEM_37);
        DoMethod(speechSystem37MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
                 MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
                 MUIA_Menuitem_Checked, TRUE);
    }

    Object speechSystemFliteMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_FLITE);
    set(speechSystemFliteMenuItem, MUIA_Menuitem_Checked,
        config.speechSystem == SPEECH_SYSTEM_FLITE);
    DoMethod(speechSystemFliteMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_CallHook,
             &SpeechSystemMenuItemClickedHook, SPEECH_SYSTEM_FLITE);
    DoMethod(speechSystemFliteMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechSystemOpenAIMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_OPENAI);
    set(speechSystemOpenAIMenuItem, MUIA_Menuitem_Checked,
        config.speechSystem == SPEECH_SYSTEM_OPENAI);
    DoMethod(speechSystemOpenAIMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_CallHook,
             &SpeechSystemMenuItemClickedHook, SPEECH_SYSTEM_OPENAI);
    DoMethod(speechSystemOpenAIMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

#ifdef __AMIGAOS4__
    Object speechSystemFliteVoiceAWBMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_AWB);
    set(speechSystemFliteVoiceAWBMenuItem, MUIA_Menuitem_Checked,
        config.speechFliteVoice == SPEECH_FLITE_VOICE_AWB);
    DoMethod(speechSystemFliteVoiceAWBMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, SPEECH_FLITE_VOICE_AWB, &config.speechFliteVoice);
    DoMethod(speechSystemFliteVoiceAWBMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechSystemFliteVoiceKALMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_KAL);
    set(speechSystemFliteVoiceKALMenuItem, MUIA_Menuitem_Checked,
        config.speechFliteVoice == SPEECH_FLITE_VOICE_KAL);
    DoMethod(speechSystemFliteVoiceKALMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, SPEECH_FLITE_VOICE_KAL, &config.speechFliteVoice);
    DoMethod(speechSystemFliteVoiceKALMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechSystemFliteVoiceKAL16MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_KAL16);
    set(speechSystemFliteVoiceKAL16MenuItem, MUIA_Menuitem_Checked,
        config.speechFliteVoice == SPEECH_FLITE_VOICE_KAL16);
    DoMethod(speechSystemFliteVoiceKAL16MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, SPEECH_FLITE_VOICE_KAL16,
             &config.speechFliteVoice);
    DoMethod(speechSystemFliteVoiceKAL16MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechSystemFliteVoiceRMSMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_RMS);
    set(speechSystemFliteVoiceRMSMenuItem, MUIA_Menuitem_Checked,
        config.speechFliteVoice == SPEECH_FLITE_VOICE_RMS);
    DoMethod(speechSystemFliteVoiceRMSMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, SPEECH_FLITE_VOICE_RMS, &config.speechFliteVoice);
    DoMethod(speechSystemFliteVoiceRMSMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechSystemFliteVoiceSLTMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_SLT);
    set(speechSystemFliteVoiceSLTMenuItem, MUIA_Menuitem_Checked,
        config.speechFliteVoice == SPEECH_FLITE_VOICE_SLT);
    DoMethod(speechSystemFliteVoiceSLTMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, SPEECH_FLITE_VOICE_SLT, &config.speechFliteVoice);
    DoMethod(speechSystemFliteVoiceSLTMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);
#endif

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    Object speechAccentMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_ACCENT);
    DoMethod(speechAccentMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechAccentMenuItemClickedHook);
#endif

    Object speechOpenAIVoiceAlloyMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY);
    set(speechOpenAIVoiceAlloyMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_ALLOY);
    DoMethod(speechOpenAIVoiceAlloyMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_ALLOY, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceAlloyMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceAshMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_ASH);
    set(speechOpenAIVoiceAshMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_ASH);
    DoMethod(speechOpenAIVoiceAshMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_ASH, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceAshMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceBalladMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_BALLAD);
    set(speechOpenAIVoiceBalladMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_BALLAD);
    DoMethod(speechOpenAIVoiceBalladMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, OPENAI_TTS_VOICE_BALLAD, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceBalladMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceCoralMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_CORAL);
    set(speechOpenAIVoiceCoralMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_CORAL);
    DoMethod(speechOpenAIVoiceCoralMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_CORAL, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceCoralMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceEchoMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO);
    set(speechOpenAIVoiceEchoMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_ECHO);
    DoMethod(speechOpenAIVoiceEchoMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_ECHO, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceEchoMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceFableMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE);
    set(speechOpenAIVoiceFableMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_FABLE);
    DoMethod(speechOpenAIVoiceFableMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_FABLE, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceFableMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceOnyxMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX);
    set(speechOpenAIVoiceOnyxMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_ONYX);
    DoMethod(speechOpenAIVoiceOnyxMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_ONYX, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceOnyxMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceNovaMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA);
    set(speechOpenAIVoiceNovaMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_NOVA);
    DoMethod(speechOpenAIVoiceNovaMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_NOVA, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceNovaMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceSageMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_SAGE);
    set(speechOpenAIVoiceSageMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_SAGE);
    DoMethod(speechOpenAIVoiceSageMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_VOICE_SAGE, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceSageMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIVoiceShimmerMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER);
    set(speechOpenAIVoiceShimmerMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSVoice == OPENAI_TTS_VOICE_SHIMMER);
    DoMethod(speechOpenAIVoiceShimmerMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, OPENAI_TTS_VOICE_SHIMMER, &config.openAITTSVoice);
    DoMethod(speechOpenAIVoiceShimmerMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIModelTTS1MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1);
    set(speechOpenAIModelTTS1MenuItem, MUIA_Menuitem_Checked,
        config.openAITTSModel == OPENAI_TTS_MODEL_TTS_1);
    DoMethod(speechOpenAIModelTTS1MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong,
             OPENAI_TTS_MODEL_TTS_1, &config.openAITTSModel);
    DoMethod(speechOpenAIModelTTS1MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIModelTTS1HDMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD);
    set(speechOpenAIModelTTS1HDMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSModel == OPENAI_TTS_MODEL_TTS_1_HD);
    DoMethod(speechOpenAIModelTTS1HDMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, OPENAI_TTS_MODEL_TTS_1_HD, &config.openAITTSModel);
    DoMethod(speechOpenAIModelTTS1HDMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object speechOpenAIModelGPT4oMiniTTSMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_SPEECH_OPENAI_MODEL_GPT_4o_MINI_TTS);
    set(speechOpenAIModelGPT4oMiniTTSMenuItem, MUIA_Menuitem_Checked,
        config.openAITTSModel == OPENAI_TTS_MODEL_GPT_4o_MINI_TTS);
    DoMethod(speechOpenAIModelGPT4oMiniTTSMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, OPENAI_TTS_MODEL_GPT_4o_MINI_TTS,
             &config.openAITTSModel);
    DoMethod(speechOpenAIModelGPT4oMiniTTSMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

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

    Object openAIChatModelGPT3_5TurboMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO);
    set(openAIChatModelGPT3_5TurboMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_3_5_TURBO);
    DoMethod(openAIChatModelGPT3_5TurboMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_3_5_TURBO, &config.chatModel);
    DoMethod(openAIChatModelGPT3_5TurboMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT3_5Turbo0125MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_0125);
    set(openAIChatModelGPT3_5Turbo0125MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_3_5_TURBO_0125);
    DoMethod(openAIChatModelGPT3_5Turbo0125MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_3_5_TURBO_0125, &config.chatModel);
    DoMethod(openAIChatModelGPT3_5Turbo0125MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT3_5Turbo1106MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_1106);
    set(openAIChatModelGPT3_5Turbo1106MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_3_5_TURBO_1106);
    DoMethod(openAIChatModelGPT3_5Turbo1106MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_3_5_TURBO_1106, &config.chatModel);
    DoMethod(openAIChatModelGPT3_5Turbo1106MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4TurboMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO);
    set(openAIChatModelGPT4TurboMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_TURBO);
    DoMethod(openAIChatModelGPT4TurboMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_TURBO, &config.chatModel);
    DoMethod(openAIChatModelGPT4TurboMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4Turbo2024_04_09MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_2024_04_09);
    set(openAIChatModelGPT4Turbo2024_04_09MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_TURBO_2024_04_09);
    DoMethod(openAIChatModelGPT4Turbo2024_04_09MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_TURBO_2024_04_09, &config.chatModel);
    DoMethod(openAIChatModelGPT4Turbo2024_04_09MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4TurboPreviewMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_PREVIEW);
    set(openAIChatModelGPT4TurboPreviewMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_TURBO_PREVIEW);
    DoMethod(openAIChatModelGPT4TurboPreviewMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_TURBO_PREVIEW, &config.chatModel);
    DoMethod(openAIChatModelGPT4TurboPreviewMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4);
    set(openAIChatModelGPT4MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4);
    DoMethod(openAIChatModelGPT4MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, GPT_4,
             &config.chatModel);
    DoMethod(openAIChatModelGPT4MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4_0613MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0613);
    set(openAIChatModelGPT4_0613MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_0613);
    DoMethod(openAIChatModelGPT4_0613MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_0613, &config.chatModel);
    DoMethod(openAIChatModelGPT4_0613MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4_0314MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0314);
    set(openAIChatModelGPT4_0314MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_0314);
    DoMethod(openAIChatModelGPT4_0314MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_0314, &config.chatModel);
    DoMethod(openAIChatModelGPT4_0314MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4_1MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_1);
    set(openAIChatModelGPT4_1MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_1);
    DoMethod(openAIChatModelGPT4_1MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, GPT_4_1,
             &config.chatModel);
    DoMethod(openAIChatModelGPT4_1MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4_1_2024_04_14MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_1_2024_04_14);
    set(openAIChatModelGPT4_1_2024_04_14MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_1_2024_04_14);
    DoMethod(openAIChatModelGPT4_1_2024_04_14MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_1_2024_04_14, &config.chatModel);
    DoMethod(openAIChatModelGPT4_1_2024_04_14MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4_5PreviewMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_5_PREVIEW);
    set(openAIChatModelGPT4_5PreviewMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_5_PREVIEW);
    DoMethod(openAIChatModelGPT4_5PreviewMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_5_PREVIEW, &config.chatModel);
    DoMethod(openAIChatModelGPT4_5PreviewMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4_5Preview2025_02_27MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData,
        MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_5_PREVIEW_2025_02_27);
    set(openAIChatModelGPT4_5Preview2025_02_27MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4_5_PREVIEW_2025_02_27);
    DoMethod(openAIChatModelGPT4_5Preview2025_02_27MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4_5_PREVIEW_2025_02_27, &config.chatModel);
    DoMethod(openAIChatModelGPT4_5Preview2025_02_27MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4oMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o);
    set(openAIChatModelGPT4oMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4o);
    DoMethod(openAIChatModelGPT4oMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, GPT_4o,
             &config.chatModel);
    DoMethod(openAIChatModelGPT4oMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4o2024_11_20MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_11_20);
    set(openAIChatModelGPT4o2024_11_20MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4o_2024_11_20);
    DoMethod(openAIChatModelGPT4o2024_11_20MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4o_2024_11_20, &config.chatModel);
    DoMethod(openAIChatModelGPT4o2024_11_20MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4o2024_08_06MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_08_06);
    set(openAIChatModelGPT4o2024_08_06MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4o_2024_08_06);
    DoMethod(openAIChatModelGPT4o2024_08_06MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4o_2024_08_06, &config.chatModel);
    DoMethod(openAIChatModelGPT4o2024_08_06MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4o2024_05_13MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_05_13);
    set(openAIChatModelGPT4o2024_05_13MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4o_2024_05_13);
    DoMethod(openAIChatModelGPT4o2024_05_13MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4o_2024_05_13, &config.chatModel);
    DoMethod(openAIChatModelGPT4o2024_05_13MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4oMiniMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI);
    set(openAIChatModelGPT4oMiniMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4o_MINI);
    DoMethod(openAIChatModelGPT4oMiniMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4o_MINI, &config.chatModel);
    DoMethod(openAIChatModelGPT4oMiniMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelGPT4oMini2024_07_18MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI_2024_07_18);
    set(openAIChatModelGPT4oMini2024_07_18MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == GPT_4o_MINI_2024_07_18);
    DoMethod(openAIChatModelGPT4oMini2024_07_18MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, GPT_4o_MINI_2024_07_18, &config.chatModel);
    DoMethod(openAIChatModelGPT4oMini2024_07_18MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o1);
    set(openAIChatModelo1MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1);
    DoMethod(openAIChatModelo1MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, o1,
             &config.chatModel);
    DoMethod(openAIChatModelo1MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1_2024_12_17MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_2024_12_17);
    set(openAIChatModelo1_2024_12_17MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_2024_12_17);
    DoMethod(openAIChatModelo1_2024_12_17MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o1_2024_12_17, &config.chatModel);
    DoMethod(openAIChatModelo1_2024_12_17MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1PreviewMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_PREVIEW);
    set(openAIChatModelo1PreviewMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_PREVIEW);
    DoMethod(openAIChatModelo1PreviewMenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o1_PREVIEW, &config.chatModel);
    DoMethod(openAIChatModelo1PreviewMenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1Preview2024_09_12MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_o1_PREVIEW_2024_09_12);
    set(openAIChatModelo1Preview2024_09_12MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_PREVIEW_2024_09_12);
    DoMethod(openAIChatModelo1Preview2024_09_12MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o1_PREVIEW_2024_09_12, &config.chatModel);
    DoMethod(openAIChatModelo1Preview2024_09_12MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1MiniMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_MINI);
    set(openAIChatModelo1MiniMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_MINI);
    DoMethod(openAIChatModelo1MiniMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, o1_MINI,
             &config.chatModel);
    DoMethod(openAIChatModelo1MiniMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1Mini2024_09_12MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_o1_MINI_2024_09_12);
    set(openAIChatModelo1Mini2024_09_12MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_MINI_2024_09_12);
    DoMethod(openAIChatModelo1Mini2024_09_12MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o1_MINI_2024_09_12, &config.chatModel);
    DoMethod(openAIChatModelo1Mini2024_09_12MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1ProMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o1_PRO);
    set(openAIChatModelo1ProMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_PRO);
    DoMethod(openAIChatModelo1ProMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, o1_PRO,
             &config.chatModel);
    DoMethod(openAIChatModelo1ProMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo1Pro2025_03_19MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_o1_PRO_2025_03_19);
    set(openAIChatModelo1Pro2025_03_19MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o1_PRO_2025_03_19);
    DoMethod(openAIChatModelo1Pro2025_03_19MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o1_PRO_2025_03_19, &config.chatModel);
    DoMethod(openAIChatModelo1Pro2025_03_19MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo3MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o3);
    set(openAIChatModelo3MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o3);
    DoMethod(openAIChatModelo3MenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, o3,
             &config.chatModel);
    DoMethod(openAIChatModelo3MenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo3_2025_04_16MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o3_2025_04_16);
    set(openAIChatModelo3_2025_04_16MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o3_2025_04_16);
    DoMethod(openAIChatModelo3_2025_04_16MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o3_2025_04_16, &config.chatModel);
    DoMethod(openAIChatModelo3_2025_04_16MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo3MiniMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o3_MINI);
    set(openAIChatModelo3MiniMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o3_MINI);
    DoMethod(openAIChatModelo3MiniMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, o3_MINI,
             &config.chatModel);
    DoMethod(openAIChatModelo3MiniMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo3Mini2025_01_31MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_o3_MINI_2025_01_31);
    set(openAIChatModelo3Mini2025_01_31MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o3_MINI_2025_01_31);
    DoMethod(openAIChatModelo3Mini2025_01_31MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o3_MINI_2025_01_31, &config.chatModel);
    DoMethod(openAIChatModelo3Mini2025_01_31MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo4MiniMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_CHAT_MODEL_o4_MINI);
    set(openAIChatModelo4MiniMenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o4_MINI);
    DoMethod(openAIChatModelo4MiniMenuItem, MUIM_Notify, MUIA_Menuitem_Checked,
             TRUE, MUIV_Notify_Application, 3, MUIM_WriteLong, o4_MINI,
             &config.chatModel);
    DoMethod(openAIChatModelo4MiniMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Self, 3, MUIM_Set,
             MUIA_Menuitem_Checked, TRUE);

    Object openAIChatModelo4Mini2025_04_16MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_CHAT_MODEL_o4_MINI_2025_04_16);
    set(openAIChatModelo4Mini2025_04_16MenuItem, MUIA_Menuitem_Checked,
        config.chatModel == o4_MINI_2025_04_16);
    DoMethod(openAIChatModelo4Mini2025_04_16MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, o4_MINI_2025_04_16, &config.chatModel);
    DoMethod(openAIChatModelo4Mini2025_04_16MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageModelDALL_E_2MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_2);
    set(openAIImageModelDALL_E_2MenuItem, MUIA_Menuitem_Checked,
        config.imageModel == DALL_E_2);
    DoMethod(openAIImageModelDALL_E_2MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, DALL_E_2, &config.imageModel);
    DoMethod(openAIImageModelDALL_E_2MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageModelDALL_E_3MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_3);
    set(openAIImageModelDALL_E_3MenuItem, MUIA_Menuitem_Checked,
        config.imageModel == DALL_E_3);
    DoMethod(openAIImageModelDALL_E_3MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, DALL_E_3, &config.imageModel);
    DoMethod(openAIImageModelDALL_E_3MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageSizeDALL_E_2_256X256MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_256X256);
    set(openAIImageSizeDALL_E_2_256X256MenuItem, MUIA_Menuitem_Checked,
        config.imageSizeDallE2 == IMAGE_SIZE_256x256);
    DoMethod(openAIImageSizeDALL_E_2_256X256MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, IMAGE_SIZE_256x256, &config.imageSizeDallE2);
    DoMethod(openAIImageSizeDALL_E_2_256X256MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageSizeDALL_E_2_512X512MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_512X512);
    set(openAIImageSizeDALL_E_2_512X512MenuItem, MUIA_Menuitem_Checked,
        config.imageSizeDallE2 == IMAGE_SIZE_512x512);
    DoMethod(openAIImageSizeDALL_E_2_512X512MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, IMAGE_SIZE_512x512, &config.imageSizeDallE2);
    DoMethod(openAIImageSizeDALL_E_2_512X512MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageSizeDALL_E_2_1024X1024MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_1024X1024);
    set(openAIImageSizeDALL_E_2_1024X1024MenuItem, MUIA_Menuitem_Checked,
        config.imageSizeDallE2 == IMAGE_SIZE_1024x1024);
    DoMethod(openAIImageSizeDALL_E_2_1024X1024MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, IMAGE_SIZE_1024x1024, &config.imageSizeDallE2);
    DoMethod(openAIImageSizeDALL_E_2_1024X1024MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageSizeDALL_E_3_1024X1024MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1024);
    set(openAIImageSizeDALL_E_3_1024X1024MenuItem, MUIA_Menuitem_Checked,
        config.imageSizeDallE3 == IMAGE_SIZE_1024x1024);
    DoMethod(openAIImageSizeDALL_E_3_1024X1024MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, IMAGE_SIZE_1024x1024, &config.imageSizeDallE3);
    DoMethod(openAIImageSizeDALL_E_3_1024X1024MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageSizeDALL_E_3_1792X1024MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1792X1024);
    set(openAIImageSizeDALL_E_3_1792X1024MenuItem, MUIA_Menuitem_Checked,
        config.imageSizeDallE3 == IMAGE_SIZE_1792x1024);
    DoMethod(openAIImageSizeDALL_E_3_1792X1024MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, IMAGE_SIZE_1792x1024, &config.imageSizeDallE3);
    DoMethod(openAIImageSizeDALL_E_3_1792X1024MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object openAIImageSizeDALL_E_3_1024X1792MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData,
                         MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1792);
    set(openAIImageSizeDALL_E_3_1024X1792MenuItem, MUIA_Menuitem_Checked,
        config.imageSizeDallE3 == IMAGE_SIZE_1024x1792);
    DoMethod(openAIImageSizeDALL_E_3_1024X1792MenuItem, MUIM_Notify,
             MUIA_Menuitem_Checked, TRUE, MUIV_Notify_Application, 3,
             MUIM_WriteLong, IMAGE_SIZE_1024x1792, &config.imageSizeDallE3);
    DoMethod(openAIImageSizeDALL_E_3_1024X1792MenuItem, MUIM_Notify,
             MUIA_Menuitem_Trigger, MUIV_EveryTime, MUIV_Notify_Self, 3,
             MUIM_Set, MUIA_Menuitem_Checked, TRUE);

    Object arexShellMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                MENU_ITEM_AREXX_AREXX_SHELL);
    DoMethod(arexShellMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook,
             &ARexxShellMenuItemClickedHook, MUIV_TriggerValue);

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
 * Populate the AREXX menu with the installed scripts
 **/
static void populateArexxMenu() {
    DoMethod(menuStrip, MUIM_Menustrip_InitChange);

    Object helpMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_HELP);
    Object openDocumentationMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_HELP_OPEN_DOCUMENTATION);
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
        DoMethod(menuStrip, MUIM_Menustrip_ExitChange);
    }
}