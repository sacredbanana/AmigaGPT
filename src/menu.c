#include <libraries/amigaguide.h>
#include <libraries/asl.h>
#include <libraries/gadtools.h>
#include <libraries/mui.h>
#include <mui/TextEditor_mcc.h>
#include <proto/amigaguide.h>
#include <proto/exec.h>
#include <SDI_hook.h>
#include "APIKeyRequesterWindow.h"
#include "AboutAmigaGPTWindow.h"
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "menu.h"
#include "ProxySettingsRequesterWindow.h"
#include "VoiceInstructionsRequesterWindow.h"
#include "version.h"

Object *menuStrip;

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

static struct NewMenu amigaGPTMenu[] = {
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_PROJECT},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_PROJECT_ABOUT_AMIGAGPT},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_PROJECT_ABOUT_MUI},
    {NM_ITEM, NM_BARLABEL, 0, 0, 0, (APTR)MENU_ITEM_NULL},
    {NM_ITEM, NULL, "Q", 0, 0, (APTR)MENU_ITEM_PROJECT_QUIT},
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_EDIT},
    {NM_ITEM, NULL, "X", 0, 0, (APTR)MENU_ITEM_EDIT_CUT},
    {NM_ITEM, NULL, "C", 0, 0, (APTR)MENU_ITEM_EDIT_COPY},
    {NM_ITEM, NULL, "V", 0, 0, (APTR)MENU_ITEM_EDIT_PASTE},
    {NM_ITEM, NULL, "L", 0, 0, (APTR)MENU_ITEM_EDIT_CLEAR},
    {NM_ITEM, NULL, "A", 0, 0, (APTR)MENU_ITEM_EDIT_SELECT_ALL},
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_VIEW},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_VIEW_MUI_SETTINGS},
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_CONNECTION},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_CONNECTION_PROXY},
    {NM_SUB, NULL, 0, CHECKIT | CHECKED | MENUTOGGLE, 0,
     (APTR)MENU_ITEM_CONNECTION_PROXY_ENABLED},
    {NM_SUB, NULL, 0, 0, 0, (APTR)MENU_ITEM_CONNECTION_PROXY_SETTINGS},
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH},
    {NM_ITEM, NULL, 0, CHECKIT | CHECKED | MENUTOGGLE, 0,
     (APTR)MENU_ITEM_SPEECH_ENABLED},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH_SYSTEM},
#ifdef __AMIGAOS3__
    {NM_SUB, NULL, 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_SPEECH_SYSTEM_34},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_SPEECH_SYSTEM_37},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_SPEECH_SYSTEM_OPENAI},
#else
#ifdef __AMIGAOS4__
    {NM_SUB, NULL, 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_SPEECH_SYSTEM_FLITE},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_SPEECH_SYSTEM_OPENAI},
#else
    {NM_SUB, NULL, 0, CHECKIT | CHECKED, ~(1 << 2),
     (APTR)MENU_ITEM_SPEECH_SYSTEM_OPENAI},
#endif
#endif
#ifdef __AMIGAOS3__
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH_ACCENT},
#endif
#ifdef __AMIGAOS4__
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH_FLITE_VOICE},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_KAL},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_KAL16},
    {NM_SUB, NULL, 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_AWB},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 3),
     (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_RMS},
    {NM_SUB, NULL, 0, CHECKIT | MENUTOGGLE, ~(1 << 4),
     (APTR)MENU_ITEM_SPEECH_FLITE_VOICE_SLT},
#endif
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE},
    {NM_SUB, "alloy", 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY},
    {NM_SUB, "ash", 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ASH},
    {NM_SUB, "ballad", 0, CHECKIT | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_BALLAD},
    {NM_SUB, "coral", 0, CHECKIT | MENUTOGGLE, ~(1 << 3),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_CORAL},
    {NM_SUB, "echo", 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO},
    {NM_SUB, "fable", 0, CHECKIT | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE},
    {NM_SUB, "onyx", 0, CHECKIT | MENUTOGGLE, ~(1 << 3),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX},
    {NM_SUB, "nova", 0, CHECKIT | MENUTOGGLE, ~(1 << 4),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA},
    {NM_SUB, "sage", 0, CHECKIT | MENUTOGGLE, ~(1 << 5),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_SAGE},
    {NM_SUB, "shimmer", 0, CHECKIT | MENUTOGGLE, ~(1 << 5),
     (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL},
    {NM_SUB, "tts-1", 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1},
    {NM_SUB, "tts-1-hd", 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD},
    {NM_SUB, "gpt-4o-mini-tts", 0, CHECKIT | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_SPEECH_OPENAI_MODEL_GPT_4o_MINI_TTS},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_SPEECH_OPENAI_VOICE_INSTRUCTIONS},
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI_API_KEY},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI_CHAT_SYSTEM},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI_CHAT_MODEL},
    {NM_SUB, "gpt-4o", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o},
    {NM_SUB, "gpt-4o-2024-11-20", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_11_20},
    {NM_SUB, "gpt-4o-2024-08-06", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_08_06},
    {NM_SUB, "gpt-4o-2024-05-13", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_05_13},
    {NM_SUB, "chatgpt-4o-latest", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_CHATGPT_4o_LATEST},
    {NM_SUB, "gpt-4o-mini", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI},
    {NM_SUB, "gpt-4o-mini-2024-07-18", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI_2024_07_18},
    {NM_SUB, "o1", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1},
    {NM_SUB, "o1-2024-12-17", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_2024_12_17},
    {NM_SUB, "o1-preview", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_PREVIEW},
    {NM_SUB, "o1-preview-2024-09-12", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_PREVIEW_2024_09_12},
    {NM_SUB, "o1-mini", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_MINI},
    {NM_SUB, "o1-mini-2024-09-12", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_MINI_2024_09_12},
    {NM_SUB, "o1-pro", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_PRO},
    {NM_SUB, "o1-pro-2025-03-19", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o1_PRO_2025_03_19},
    {NM_SUB, "o3-mini", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o3_MINI},
    {NM_SUB, "o3-mini-2025-01-31", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_o3_MINI_2025_01_31},
    {NM_SUB, "gpt-4-turbo", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO},
    {NM_SUB, "gpt-4-turbo-2024-04-09", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_2024_04_09},
    {NM_SUB, "gpt-4-turbo-preview", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_PREVIEW},
    {NM_SUB, "gpt-4", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4},
    {NM_SUB, "gpt-4-0613", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0613},
    {NM_SUB, "gpt-4-0314", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0314},
    {NM_SUB, "gpt-4.5-preview", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_5_PREVIEW},
    {NM_SUB, "gpt-4.5-preview-2025-02-27", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_5_PREVIEW_2025_02_27},
    {NM_SUB, "gpt-3.5-turbo", 0, CHECKIT | CHECKED | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO},
    {NM_SUB, "gpt-3.5-turbo-0125", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_0125},
    {NM_SUB, "gpt-3.5-turbo-1106", 0, CHECKIT | MENUTOGGLE, ~0,
     (APTR)MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_1106},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI_IMAGE_MODEL},
    {NM_SUB, "dall-e-2", 0, CHECKIT | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_2},
    {NM_SUB, "dall-e-3", 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_3},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2},
    {NM_SUB, "256x256", 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_256X256},
    {NM_SUB, "512x512", 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_512X512},
    {NM_SUB, "1024x1024", 0, CHECKIT | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_1024X1024},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3},
    {NM_SUB, "1024x1024", 0, CHECKIT | CHECKED | MENUTOGGLE, ~(1 << 0),
     (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1024},
    {NM_SUB, "1792x1024", 0, CHECKIT | MENUTOGGLE, ~(1 << 1),
     (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1792X1024},
    {NM_SUB, "1024x1792", 0, CHECKIT | MENUTOGGLE, ~(1 << 2),
     (APTR)MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1792},
    {NM_TITLE, NULL, 0, 0, 0, (APTR)MENU_ITEM_HELP},
    {NM_ITEM, NULL, 0, 0, 0, (APTR)MENU_ITEM_HELP_OPEN_DOCUMENTATION},
    {NM_END, NULL, 0, 0, 0, 0}};

void createMenu() {
    menuStrip = MUI_MakeObject(MUIO_MenustripNM, amigaGPTMenu);
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

    Object openDocumentationMenuItem =
        DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_HELP_OPEN_DOCUMENTATION);
    DoMethod(openDocumentationMenuItem, MUIM_Notify, MUIA_Menuitem_Trigger,
             MUIV_EveryTime, MUIV_Notify_Application, 3, MUIM_CallHook,
             &OpenDocumentationMenuItemClickedHook, MUIV_TriggerValue);
}

void setMenuTitles() {
    Object projectMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT);
    set(projectMenuItem, MUIA_Menu_Title, STRING_MENU_PROJECT);

    Object aboutAmigaGPTMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_ABOUT_AMIGAGPT);
    set(aboutAmigaGPTMenuItem, MUIA_Menuitem_Title, STRING_MENU_ABOUT_AMIGAGPT);

    Object aboutMuiMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                               MENU_ITEM_PROJECT_ABOUT_MUI);
    set(aboutMuiMenuItem, MUIA_Menuitem_Title, STRING_MENU_ABOUT_MUI);

    Object quitMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_PROJECT_QUIT);
    set(quitMenuItem, MUIA_Menuitem_Title, STRING_MENU_QUIT);

    Object editMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT);
    set(editMenuItem, MUIA_Menu_Title, STRING_MENU_EDIT);

    Object cutMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_CUT);
    set(cutMenuItem, MUIA_Menuitem_Title, STRING_MENU_CUT);

    Object copyMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_COPY);
    set(copyMenuItem, MUIA_Menuitem_Title, STRING_MENU_COPY);

    Object pasteMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_PASTE);
    set(pasteMenuItem, MUIA_Menuitem_Title, STRING_MENU_PASTE);

    Object clearMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_CLEAR);
    set(clearMenuItem, MUIA_Menuitem_Title, STRING_MENU_CLEAR);

    Object selectAllMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_EDIT_SELECT_ALL);
    set(selectAllMenuItem, MUIA_Menuitem_Title, STRING_MENU_SELECT_ALL);

    Object viewMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_VIEW);
    set(viewMenuItem, MUIA_Menu_Title, STRING_MENU_VIEW);

    Object muiSettingsMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                  MENU_ITEM_VIEW_MUI_SETTINGS);
    set(muiSettingsMenuItem, MUIA_Menuitem_Title, STRING_MENU_MUI_SETTINGS);

    Object connectionMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION);
    set(connectionMenuItem, MUIA_Menuitem_Title, STRING_MENU_CONNECTION);

    Object proxyMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY);
    set(proxyMenuItem, MUIA_Menuitem_Title, STRING_MENU_PROXY);

    Object proxyEnabledMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY_ENABLED);
    set(proxyEnabledMenuItem, MUIA_Menuitem_Title, STRING_MENU_ENABLED);

    Object proxySettingsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_CONNECTION_PROXY_SETTINGS);
    set(proxySettingsMenuItem, MUIA_Menuitem_Title, STRING_MENU_SETTINGS);

    Object speechMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH);
    set(speechMenuItem, MUIA_Menu_Title, STRING_MENU_SPEECH);

    Object speechEnabledMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_ENABLED);
    set(speechEnabledMenuItem, MUIA_Menuitem_Title, STRING_MENU_ENABLED);

    Object speechSystemMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM);
    set(speechSystemMenuItem, MUIA_Menuitem_Title, STRING_MENU_SPEECH_SYSTEM);

#ifdef __AMIGAOS3__
    Object speechSystem34MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_34);
    set(speechSystem34MenuItem, MUIA_Menuitem_Title,
        STRING_MENU_SPEECH_SYSTEM_34);

    Object speechSystem37MenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_37);
    set(speechSystem37MenuItem, MUIA_Menuitem_Title,
        STRING_MENU_SPEECH_SYSTEM_37);

    Object speechAccentMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_ACCENT);
    set(speechAccentMenuItem, MUIA_Menuitem_Title, STRING_MENU_SPEECH_ACCENT);
#endif

#ifdef __AMIGAOS4__
    Object speechSystemFliteMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_FLITE);
    set(speechSystemFliteMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_SPEECH_SYSTEM_FLITE);

    Object fliteVoiceMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                 MENU_ITEM_SPEECH_FLITE_VOICE);
    set(fliteVoiceMenuItem, MUIA_Menuitem_Title, STRING_MENU_FLITE_VOICE);

    Object fliteVoiceKalMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_KAL);
    set(fliteVoiceKalMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_FLITE_VOICE_KAL);

    Object fliteVoiceKal16MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_KAL16);
    set(fliteVoiceKal16MenuItem, MUIA_Menuitem_Title,
        STRING_MENU_FLITE_VOICE_KAL16);

    Object fliteVoiceAwbMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_AWB);
    set(fliteVoiceAwbMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_FLITE_VOICE_AWB);

    Object fliteVoiceRmsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_RMS);
    set(fliteVoiceRmsMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_FLITE_VOICE_RMS);

    Object fliteVoiceSltMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_FLITE_VOICE_SLT);
    set(fliteVoiceSltMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_FLITE_VOICE_SLT);
#endif

    Object speechSystemOpenAIMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_SYSTEM_OPENAI);
    set(speechSystemOpenAIMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_SPEECH_SYSTEM_OPENAI);

    Object speechSystemOpenAIVoiceMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE);
    set(speechSystemOpenAIVoiceMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPENAI_VOICE);

    Object speechSystemOpenAIModelMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_MODEL);
    set(speechSystemOpenAIModelMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_SPEECH_OPENAI_MODEL);

    Object speechSystemOpenAIVoiceInstructionsMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_SPEECH_OPENAI_VOICE_INSTRUCTIONS);
    set(speechSystemOpenAIVoiceInstructionsMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPENAI_VOICE_INSTRUCTIONS);

    Object openAIMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI);
    set(openAIMenuItem, MUIA_Menu_Title, STRING_MENU_OPENAI);

    Object apiKeyMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_API_KEY);
    set(apiKeyMenuItem, MUIA_Menuitem_Title, STRING_MENU_OPENAI_API_KEY);

    Object chatSystemMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                 MENU_ITEM_OPENAI_CHAT_SYSTEM);
    set(chatSystemMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPENAI_CHAT_SYSTEM);
    Object chatModelMenuItem = (Object)DoMethod(menuStrip, MUIM_FindUData,
                                                MENU_ITEM_OPENAI_CHAT_MODEL);
    set(chatModelMenuItem, MUIA_Menuitem_Title, STRING_MENU_OPENAI_CHAT_MODEL);

    Object openAIImageModelMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_MODEL);
    set(openAIImageModelMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPENAI_IMAGE_MODEL);

    Object openAIImageSizeDALL_E_2MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2);
    set(openAIImageSizeDALL_E_2MenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPENAI_IMAGE_SIZE_DALL_E_2);

    Object openAIImageSizeDALL_E_3MenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3);
    set(openAIImageSizeDALL_E_3MenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPENAI_IMAGE_SIZE_DALL_E_3);

    Object helpMenuItem =
        (Object)DoMethod(menuStrip, MUIM_FindUData, MENU_ITEM_HELP);
    set(helpMenuItem, MUIA_Menu_Title, STRING_MENU_HELP);

    Object openDocumentationMenuItem = (Object)DoMethod(
        menuStrip, MUIM_FindUData, MENU_ITEM_HELP_OPEN_DOCUMENTATION);
    set(openDocumentationMenuItem, MUIA_Menuitem_Title,
        STRING_MENU_OPEN_DOCUMENTATION);
}