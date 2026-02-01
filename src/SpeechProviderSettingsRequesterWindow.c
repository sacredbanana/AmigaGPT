#include "SpeechProviderSettingsRequesterWindow.h"

#ifdef DAEMON
Object *speechProviderSettingsRequesterWindowObject = NULL;

LONG createSpeechProviderSettingsRequesterWindow(void) { return RETURN_OK; }

void openSpeechProviderSettingsRequesterWindow(void) {}

#else

#include <dos/dos.h>
#include <exec/exec.h>
#include <json-c/json.h>
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <utility/tagitem.h>
#include <mui/BetterString_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include <string.h>
#include "AmigaGPTConfig.h"
#include "MainWindow.h"
#include "gui.h"
#include "openai.h"
#include "speech.h"

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d)                                                    \
    (((ULONG)(a) << 24) | ((ULONG)(b) << 16) | ((ULONG)(c) << 8) | (ULONG)(d))
#endif

/* Some older MUI SDKs may not define this attribute.
 * If it's missing, treat it as TAG_IGNORE so the taglist still compiles. */
#ifndef MUIA_Window_AutoAdjust
#define MUIA_Window_AutoAdjust TAG_IGNORE
#endif

Object *speechProviderSettingsRequesterWindowObject = NULL;

static Object *speechProfileList = NULL;
static Object *speechProfileNameString = NULL;
static Object *speechSystemCycle = NULL;

/* OpenAI fields */
static Object *openAiApiKeyString = NULL;
static Object *openAiTtsModelCycle = NULL;
static Object *openAiTtsVoiceCycle = NULL;
static Object *openAiVoiceInstructionsEditor = NULL;

/* ElevenLabs fields */
static Object *elevenLabsAPIKeyString = NULL;
static Object *elevenLabsModelList = NULL;
static Object *elevenLabsVoiceSearchString = NULL;
static Object *elevenLabsVoiceList = NULL;
static Object *elevenLabsSearchButton = NULL;
static Object *elevenLabsFetchModelsButton = NULL;
static Object *elevenLabsCurrentModelText = NULL;
static Object *elevenLabsCurrentVoiceText = NULL;

/* Workbench fields */
static Object *workbenchAccentString = NULL;
static Object *workbenchAccentBrowseButton = NULL;
static Object *workbenchWarningText = NULL;
static Object *workbenchRateString = NULL;
static Object *workbenchPitchString = NULL;
static Object *workbenchModeCycle = NULL;
static Object *workbenchSexCycle = NULL;

/* Flite fields */
static Object *fliteVoiceCycle = NULL;
static Object *fliteWarningText = NULL;

static Object *saveProfileButton = NULL;
static Object *deleteProfileButton = NULL;

static Object *rightScrollgroup = NULL;
static Object *rightVirtgroup = NULL;
static Object *customCommonGroup = NULL;
static Object *workbenchGroup = NULL;
static Object *fliteGroup = NULL;
static Object *openAiGroup = NULL;
static Object *elevenLabsGroup = NULL;
static Object *okButton = NULL;
static Object *cancelButton = NULL;
static Object *testButton = NULL;

static struct json_object *speechProfilesJson = NULL;
static struct json_object *elevenLabsModelsJson = NULL;
static struct json_object *elevenLabsVoicesJson = NULL;
static BOOL suppressProfileSelected = FALSE;
static BOOL speechSettingsDirty = FALSE;
static LONG lastSelectedProfile = MUIV_NList_Active_Off;

static STRPTR speechSystemOptions[8] = {NULL};

#ifdef __AMIGAOS3__
static STRPTR narratorModeOptions[] = {(STRPTR) "Natural", (STRPTR) "Robotic",
                                       NULL};
static STRPTR narratorSexOptions[] = {(STRPTR) "Male", (STRPTR) "Female", NULL};
#endif

#define ELEVENLABS_HOST "api.elevenlabs.io"
#define ELEVENLABS_PORT 443

static void loadProfilesFromConfig(void);
static void saveProfilesToConfig(void);
static void populateProfileList(void);
static void loadProfileIntoUI(LONG activeIndex);
static struct json_object *createProfileFromUI(CONST_STRPTR name);
static void applyBuiltinProfileSelection(SpeechSystem sys);
static void reinitSpeechForActiveProfile(void);
static SpeechSystem getSpeechSystemFromCycle(void);
static void setFieldsEnabledForSystem(SpeechSystem sys, BOOL isCustomOrNew);
static void updateGroupVisibilityForSystem(SpeechSystem sys,
                                           BOOL isCustomOrNew);

static void beginRightPanelUpdate(void) {
    if (speechProviderSettingsRequesterWindowObject != NULL)
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Sleep, TRUE);
    if (rightVirtgroup != NULL)
        DoMethod(rightVirtgroup, MUIM_Group_InitChange);
}

static void endRightPanelUpdate(void) {
    if (rightVirtgroup != NULL)
        DoMethod(rightVirtgroup, MUIM_Group_ExitChange);
    if (speechProviderSettingsRequesterWindowObject != NULL)
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Sleep, FALSE);
}

static LONG getBuiltinSpeechProviderCount(void) {
    LONG count = 0;
#ifdef __AMIGAOS3__
    /* Workbench v34/v37 + OpenAI + ElevenLabs */
    count = 4;
#else
#ifdef __AMIGAOS4__
    /* Flite + OpenAI + ElevenLabs */
    count = 3;
#else
    /* MorphOS: OpenAI + ElevenLabs */
    count = 2;
#endif
#endif
    return count;
}

static BOOL isBuiltinListIndex(LONG idx) {
    /* Index 0 = New Profile, 1..builtinCount = built-ins */
    return (idx >= 1 && idx <= getBuiltinSpeechProviderCount());
}

static SpeechSystem builtinSystemForListIndex(LONG idx) {
#ifdef __AMIGAOS3__
    /* 1=v34, 2=v37, 3=OpenAI, 4=ElevenLabs */
    if (idx == 1)
        return SPEECH_SYSTEM_34;
    if (idx == 2)
        return SPEECH_SYSTEM_37;
    if (idx == 3)
        return SPEECH_SYSTEM_OPENAI;
    return SPEECH_SYSTEM_ELEVENLABS;
#else
#ifdef __AMIGAOS4__
    /* 1=Flite, 2=OpenAI, 3=ElevenLabs */
    if (idx == 1)
        return SPEECH_SYSTEM_FLITE;
    if (idx == 2)
        return SPEECH_SYSTEM_OPENAI;
    return SPEECH_SYSTEM_ELEVENLABS;
#else
    /* 1=OpenAI, 2=ElevenLabs */
    if (idx == 1)
        return SPEECH_SYSTEM_OPENAI;
    return SPEECH_SYSTEM_ELEVENLABS;
#endif
#endif
}

static CONST_STRPTR builtinNameForSystem(SpeechSystem sys) {
    /* Stored in config as a plain string; use SPEECH_SYSTEM_NAMES entries. */
    if (sys >= 0 && SPEECH_SYSTEM_NAMES[sys] != NULL)
        return SPEECH_SYSTEM_NAMES[sys];
    return SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI];
}

static BOOL fileExists(CONST_STRPTR path) {
    if (path == NULL || strlen(path) == 0)
        return FALSE;
    BPTR f = Open(path, MODE_OLDFILE);
    if (f != 0) {
        Close(f);
        return TRUE;
    }
    return FALSE;
}

static void updateRequirementWarningForSystem(SpeechSystem sys,
                                              Object *warningTextObject) {
    if (warningTextObject == NULL)
        return;

    static UBYTE warningBuf[512];
    warningBuf[0] = '\0';

#ifdef __AMIGAOS3__
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        struct Library *TranslatorBase = OpenLibrary("translator.library", 43);
        if (TranslatorBase != NULL) {
            CloseLibrary(TranslatorBase);
            TranslatorBase = NULL;
        } else {
            UBYTE line[256];
            snprintf(line, sizeof(line), STRING_SPEECH_WARNING_MISSING_FORMAT,
                     "translator.library v43");
            strncat(warningBuf, line,
                    sizeof(warningBuf) - strlen(warningBuf) - 1);
        }

        CONST_STRPTR narratorPath =
            (sys == SPEECH_SYSTEM_34)
                ? "AMIGAGPT:devs/speech/34/narrator.device"
                : "AMIGAGPT:devs/speech/37/narrator.device";
        if (!fileExists(narratorPath)) {
            if (strlen(warningBuf) > 0)
                strncat(warningBuf, "\n",
                        sizeof(warningBuf) - strlen(warningBuf) - 1);
            UBYTE line[256];
            snprintf(line, sizeof(line), STRING_SPEECH_WARNING_MISSING_FORMAT,
                     narratorPath);
            strncat(warningBuf, line,
                    sizeof(warningBuf) - strlen(warningBuf) - 1);
        }
    }
#endif

#ifdef __AMIGAOS4__
    if (sys == SPEECH_SYSTEM_FLITE) {
        /* Minimal device check */
        struct MsgPort *port =
            (struct MsgPort *)AllocSysObject(ASOT_PORT, NULL);
        struct IOStdReq *req = NULL;
        if (port != NULL) {
            req = (struct IOStdReq *)AllocSysObjectTags(
                ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct IOStdReq),
                ASOIOR_ReplyPort, port, TAG_END);
        }
        if (req == NULL) {
            UBYTE line[256];
            snprintf(line, sizeof(line), STRING_SPEECH_WARNING_MISSING_FORMAT,
                     "flite.device");
            strncat(warningBuf, line,
                    sizeof(warningBuf) - strlen(warningBuf) - 1);
        } else {
            if (OpenDevice("flite.device", 0, (struct IORequest *)req, 0) !=
                0) {
                UBYTE line[256];
                snprintf(line, sizeof(line),
                         STRING_SPEECH_WARNING_MISSING_FORMAT, "flite.device");
                strncat(warningBuf, line,
                        sizeof(warningBuf) - strlen(warningBuf) - 1);
            } else {
                CloseDevice((struct IORequest *)req);
            }
        }
        if (req != NULL)
            FreeSysObject(ASOT_IOREQUEST, req);
        if (port != NULL)
            FreeSysObject(ASOT_PORT, port);
    }
#endif

    set(warningTextObject, MUIA_Text_Contents, warningBuf);
}

static struct json_object *getElevenLabsModels(CONST_STRPTR apiKey) {
    return makeHttpsGetRequest(
        ELEVENLABS_HOST, ELEVENLABS_PORT, "/v1/models", apiKey, "xi-api-key",
        FALSE, configGetProxyEnabled(), configGetProxyHost(),
        configGetProxyPort(), configGetProxyUsesSSL(),
        configGetProxyRequiresAuth(), configGetProxyUsername(),
        configGetProxyPassword());
}

static struct json_object *searchElevenLabsVoices(CONST_STRPTR apiKey,
                                                  CONST_STRPTR query) {
    UBYTE endpoint[512];
    if (query != NULL && strlen(query) > 0) {
        snprintf(endpoint, sizeof(endpoint), "/v2/voices?search=%s", query);
    } else {
        snprintf(endpoint, sizeof(endpoint), "/v2/voices");
    }
    return makeHttpsGetRequest(
        ELEVENLABS_HOST, ELEVENLABS_PORT, endpoint, apiKey, "xi-api-key", FALSE,
        configGetProxyEnabled(), configGetProxyHost(), configGetProxyPort(),
        configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
        configGetProxyUsername(), configGetProxyPassword());
}

/* NList hooks for simple string lists (do not free entries) */
HOOKPROTONHNO(ConstructStringLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    return entry;
}
MakeHook(ConstructStringLI_TextHook, ConstructStringLI_TextFunc);

HOOKPROTONHNO(DestructStringLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    /* Don't free - strings owned elsewhere */
}
MakeHook(DestructStringLI_TextHook, DestructStringLI_TextFunc);

HOOKPROTONHNO(DisplayStringLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayStringLI_TextHook, DisplayStringLI_TextFunc);

static void populateElevenLabsModelList(void) {
    if (elevenLabsModelList == NULL)
        return;
    DoMethod(elevenLabsModelList, MUIM_NList_Clear);
    if (elevenLabsModelsJson == NULL)
        return;

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(elevenLabsModelsJson, "models",
                                   &modelsArray)) {
        if (json_object_is_type(elevenLabsModelsJson, json_type_array)) {
            modelsArray = elevenLabsModelsJson;
        } else {
            return;
        }
    }

    LONG modelCount = json_object_array_length(modelsArray);
    LONG selectedIndex = MUIV_NList_Active_Off;
    STRPTR currentModelId = configGetElevenLabsModel();

    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *nameObj = NULL;
        struct json_object *modelIdObj = NULL;
        if (modelObj == NULL)
            continue;
        if (json_object_object_get_ex(modelObj, "name", &nameObj)) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            DoMethod(elevenLabsModelList, MUIM_NList_InsertSingle, name,
                     MUIV_NList_Insert_Bottom);
            if (currentModelId != NULL &&
                json_object_object_get_ex(modelObj, "model_id", &modelIdObj)) {
                CONST_STRPTR mid = json_object_get_string(modelIdObj);
                if (mid != NULL && strcmp(currentModelId, mid) == 0) {
                    selectedIndex = i;
                }
            }
        }
    }

    if (selectedIndex != MUIV_NList_Active_Off) {
        set(elevenLabsModelList, MUIA_NList_Active, selectedIndex);
    }
}

static void populateElevenLabsVoiceList(void) {
    if (elevenLabsVoiceList == NULL)
        return;
    DoMethod(elevenLabsVoiceList, MUIM_NList_Clear);
    if (elevenLabsVoicesJson == NULL)
        return;

    struct json_object *voicesArray = NULL;
    if (!json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                   &voicesArray)) {
        if (json_object_is_type(elevenLabsVoicesJson, json_type_array)) {
            voicesArray = elevenLabsVoicesJson;
        } else {
            return;
        }
    }

    LONG voiceCount = json_object_array_length(voicesArray);
    LONG selectedIndex = MUIV_NList_Active_Off;
    STRPTR currentVoiceID = configGetElevenLabsVoiceID();

    for (LONG i = 0; i < voiceCount; i++) {
        struct json_object *voiceObj =
            json_object_array_get_idx(voicesArray, i);
        struct json_object *nameObj = NULL;
        struct json_object *voiceIdObj = NULL;
        if (voiceObj == NULL)
            continue;
        if (json_object_object_get_ex(voiceObj, "name", &nameObj)) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            DoMethod(elevenLabsVoiceList, MUIM_NList_InsertSingle, name,
                     MUIV_NList_Insert_Bottom);
            if (currentVoiceID != NULL &&
                json_object_object_get_ex(voiceObj, "voice_id", &voiceIdObj)) {
                CONST_STRPTR vid = json_object_get_string(voiceIdObj);
                if (vid != NULL && strcmp(currentVoiceID, vid) == 0) {
                    selectedIndex = i;
                }
            }
        }
    }

    if (selectedIndex != MUIV_NList_Active_Off) {
        set(elevenLabsVoiceList, MUIA_NList_Active, selectedIndex);
    }
}

HOOKPROTONHNONP(ElevenLabsFetchModelsButtonClickedFunc, void) {
    STRPTR apiKey = NULL;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);

    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }

    updateStatusBar(STRING_FETCHING_ELEVENLABS_MODELS, yellowPen);
    set(elevenLabsFetchModelsButton, MUIA_Disabled, TRUE);

    if (elevenLabsModelsJson != NULL) {
        json_object_put(elevenLabsModelsJson);
        elevenLabsModelsJson = NULL;
    }

    elevenLabsModelsJson = getElevenLabsModels(apiKey);
    if (elevenLabsModelsJson != NULL) {
        populateElevenLabsModelList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_FETCHING_ELEVENLABS_MODELS);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(elevenLabsFetchModelsButton, MUIA_Disabled, FALSE);
}
MakeHook(ElevenLabsFetchModelsButtonClickedHook,
         ElevenLabsFetchModelsButtonClickedFunc);

HOOKPROTONHNONP(ElevenLabsSearchVoicesButtonClickedFunc, void) {
    STRPTR apiKey = NULL;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);
    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }

    STRPTR query = NULL;
    get(elevenLabsVoiceSearchString, MUIA_String_Contents, &query);

    updateStatusBar(STRING_SEARCHING_ELEVENLABS_VOICES, yellowPen);
    set(elevenLabsSearchButton, MUIA_Disabled, TRUE);

    if (elevenLabsVoicesJson != NULL) {
        json_object_put(elevenLabsVoicesJson);
        elevenLabsVoicesJson = NULL;
    }

    elevenLabsVoicesJson = searchElevenLabsVoices(apiKey, query);
    if (elevenLabsVoicesJson != NULL) {
        populateElevenLabsVoiceList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_SEARCHING_ELEVENLABS_VOICES);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(elevenLabsSearchButton, MUIA_Disabled, FALSE);
}
MakeHook(ElevenLabsSearchVoicesButtonClickedHook,
         ElevenLabsSearchVoicesButtonClickedFunc);

static void loadProfilesFromConfig(void) {
    if (speechProfilesJson != NULL) {
        json_object_put(speechProfilesJson);
        speechProfilesJson = NULL;
    }

    STRPTR profilesStr = configGetSpeechProfiles();
    if (profilesStr != NULL && strlen(profilesStr) > 0) {
        speechProfilesJson = json_tokener_parse(profilesStr);
        if (speechProfilesJson == NULL ||
            !json_object_is_type(speechProfilesJson, json_type_array)) {
            if (speechProfilesJson != NULL)
                json_object_put(speechProfilesJson);
            speechProfilesJson = json_object_new_array();
        }
    } else {
        speechProfilesJson = json_object_new_array();
    }
}

static void saveProfilesToConfig(void) {
    if (speechProfilesJson == NULL)
        return;
    CONST_STRPTR jsonStr = json_object_to_json_string(speechProfilesJson);
    configSetSpeechProfiles(jsonStr);
}

static void populateSpeechSystemOptions(void) {
    int n = 0;
#ifdef __AMIGAOS3__
    speechSystemOptions[n++] = (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_34];
    speechSystemOptions[n++] = (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_37];
#endif
#ifdef __AMIGAOS4__
    speechSystemOptions[n++] = (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_FLITE];
#endif
    speechSystemOptions[n++] =
        (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI];
    speechSystemOptions[n++] =
        (STRPTR)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_ELEVENLABS];
    speechSystemOptions[n] = NULL;
}

static SpeechSystem getSpeechSystemFromCycle(void) {
    LONG sysIdx = 0;
    if (speechSystemCycle != NULL)
        get(speechSystemCycle, MUIA_Cycle_Active, &sysIdx);
    if (speechSystemOptions[sysIdx] != NULL) {
        for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
            if (strcmp(speechSystemOptions[sysIdx], SPEECH_SYSTEM_NAMES[s]) ==
                0) {
                return (SpeechSystem)s;
            }
        }
    }
    return SPEECH_SYSTEM_OPENAI;
}

HOOKPROTONHNONP(SpeechProviderSystemChangedFunc, void) {
    /* Changing the system toggles group visibility; keep window height stable.
     */
    LONG wasOpen = FALSE;
    LONG h = 0;
    if (speechProviderSettingsRequesterWindowObject != NULL) {
        get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            &wasOpen);
        if (wasOpen)
            get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height,
                &h);
    }

    SpeechSystem sys = getSpeechSystemFromCycle();
    updateGroupVisibilityForSystem(sys, TRUE);
    setFieldsEnabledForSystem(sys, TRUE);
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        updateRequirementWarningForSystem(sys, workbenchWarningText);
    } else if (sys == SPEECH_SYSTEM_FLITE) {
        updateRequirementWarningForSystem(sys, fliteWarningText);
    } else {
        updateRequirementWarningForSystem(sys, workbenchWarningText);
        updateRequirementWarningForSystem(sys, fliteWarningText);
    }

    if (speechProviderSettingsRequesterWindowObject != NULL && wasOpen &&
        h > 0) {
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height, h);
    }
}
MakeHook(SpeechProviderSystemChangedHook, SpeechProviderSystemChangedFunc);

static void populateProfileList(void) {
    if (speechProfileList == NULL)
        return;

    suppressProfileSelected = TRUE;
    DoMethod(speechProfileList, MUIM_NList_Clear);

    /* New Profile */
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)STRING_NEW_PROFILE, MUIV_NList_Insert_Bottom);

    /* Built-ins */
#ifdef __AMIGAOS3__
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_34],
             MUIV_NList_Insert_Bottom);
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_37],
             MUIV_NList_Insert_Bottom);
#endif
#ifdef __AMIGAOS4__
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_FLITE],
             MUIV_NList_Insert_Bottom);
#endif
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_OPENAI],
             MUIV_NList_Insert_Bottom);
    DoMethod(speechProfileList, MUIM_NList_InsertSingle,
             (ULONG)SPEECH_SYSTEM_NAMES[SPEECH_SYSTEM_ELEVENLABS],
             MUIV_NList_Insert_Bottom);

    /* Custom profiles */
    if (speechProfilesJson != NULL &&
        json_object_is_type(speechProfilesJson, json_type_array)) {
        int len = json_object_array_length(speechProfilesJson);
        for (int i = 0; i < len; i++) {
            struct json_object *p =
                json_object_array_get_idx(speechProfilesJson, i);
            if (p == NULL)
                continue;
            struct json_object *nameObj = json_object_object_get(p, "name");
            if (nameObj == NULL)
                continue;
            CONST_STRPTR name = json_object_get_string(nameObj);
            if (name != NULL && strlen(name) > 0) {
                DoMethod(speechProfileList, MUIM_NList_InsertSingle,
                         (ULONG)name, MUIV_NList_Insert_Bottom);
            }
        }
    }

    /* Select active */
    LONG select = 1; /* default */
    STRPTR activeName = configGetActiveSpeechProfileName();
    if (activeName != NULL && strlen(activeName) > 0) {
        /* Built-ins */
        for (LONG bi = 1; bi <= getBuiltinSpeechProviderCount(); bi++) {
            SpeechSystem sys = builtinSystemForListIndex(bi);
            CONST_STRPTR bn = builtinNameForSystem(sys);
            if (bn != NULL && strcmp(activeName, bn) == 0) {
                select = bi;
                break;
            }
        }
        /* Custom */
        if (speechProfilesJson != NULL) {
            int len = json_object_array_length(speechProfilesJson);
            for (int i = 0; i < len; i++) {
                struct json_object *p =
                    json_object_array_get_idx(speechProfilesJson, i);
                struct json_object *nameObj =
                    p ? json_object_object_get(p, "name") : NULL;
                CONST_STRPTR name =
                    nameObj ? json_object_get_string(nameObj) : NULL;
                if (name != NULL && strcmp(activeName, name) == 0) {
                    select = 1 + getBuiltinSpeechProviderCount() + i;
                    break;
                }
            }
        }
    }

    set(speechProfileList, MUIA_NList_Active, select);
    suppressProfileSelected = FALSE;
    loadProfileIntoUI(select);
}

static void setFieldsEnabledForSystem(SpeechSystem sys, BOOL isCustomOrNew) {
    /* Enable/disable the “custom common” controls. Provider groups are
     * shown/hidden separately. */
    if (speechProfileNameString != NULL)
        set(speechProfileNameString, MUIA_Disabled, !isCustomOrNew);
    if (speechSystemCycle != NULL)
        set(speechSystemCycle, MUIA_Disabled, !isCustomOrNew);

    if (workbenchAccentString != NULL)
        set(workbenchAccentString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchAccentBrowseButton != NULL)
        set(workbenchAccentBrowseButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
#ifdef __AMIGAOS3__
    if (workbenchRateString != NULL)
        set(workbenchRateString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchPitchString != NULL)
        set(workbenchPitchString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchModeCycle != NULL)
        set(workbenchModeCycle, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (workbenchSexCycle != NULL)
        set(workbenchSexCycle, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
#endif

    if (fliteVoiceCycle != NULL)
        set(fliteVoiceCycle, MUIA_Disabled, !(sys == SPEECH_SYSTEM_FLITE));

    if (openAiApiKeyString != NULL)
        set(openAiApiKeyString, MUIA_Disabled, !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiTtsModelCycle != NULL)
        set(openAiTtsModelCycle, MUIA_Disabled, !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiTtsVoiceCycle != NULL)
        set(openAiTtsVoiceCycle, MUIA_Disabled, !(sys == SPEECH_SYSTEM_OPENAI));
    if (openAiVoiceInstructionsEditor != NULL)
        set(openAiVoiceInstructionsEditor, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_OPENAI));

    if (elevenLabsAPIKeyString != NULL)
        set(elevenLabsAPIKeyString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsFetchModelsButton != NULL)
        set(elevenLabsFetchModelsButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsModelList != NULL)
        set(elevenLabsModelList, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsVoiceSearchString != NULL)
        set(elevenLabsVoiceSearchString, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsSearchButton != NULL)
        set(elevenLabsSearchButton, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
    if (elevenLabsVoiceList != NULL)
        set(elevenLabsVoiceList, MUIA_Disabled,
            !(sys == SPEECH_SYSTEM_ELEVENLABS));
}

static void updateGroupVisibilityForSystem(SpeechSystem sys,
                                           BOOL isCustomOrNew) {
    if (customCommonGroup != NULL)
        set(customCommonGroup, MUIA_ShowMe, isCustomOrNew);

    if (workbenchGroup != NULL)
        set(workbenchGroup, MUIA_ShowMe,
            (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37));
    if (fliteGroup != NULL)
        set(fliteGroup, MUIA_ShowMe, (sys == SPEECH_SYSTEM_FLITE));
    if (openAiGroup != NULL)
        set(openAiGroup, MUIA_ShowMe, (sys == SPEECH_SYSTEM_OPENAI));
    if (elevenLabsGroup != NULL)
        set(elevenLabsGroup, MUIA_ShowMe, (sys == SPEECH_SYSTEM_ELEVENLABS));
}

static void loadProfileIntoUI(LONG activeIndex) {
    if (speechProfileNameString == NULL)
        return;

    if (activeIndex < 0)
        return;

    beginRightPanelUpdate();

    BOOL isCustom = !isBuiltinListIndex(activeIndex) && activeIndex > 0;
    SpeechSystem sys = SPEECH_SYSTEM_OPENAI;
    struct json_object *customProfile = NULL;
    BOOL isCustomOrNew = (isCustom || activeIndex == 0);

    if (isBuiltinListIndex(activeIndex)) {
        sys = builtinSystemForListIndex(activeIndex);
        set(speechProfileNameString, MUIA_String_Contents,
            builtinNameForSystem(sys));
    } else if (activeIndex == 0) {
        /* New profile */
        set(speechProfileNameString, MUIA_String_Contents, "");
        sys = SPEECH_SYSTEM_OPENAI;
    } else {
        /* Custom */
        int profileIndex =
            (int)(activeIndex - 1 - getBuiltinSpeechProviderCount());
        struct json_object *p =
            (speechProfilesJson != NULL)
                ? json_object_array_get_idx(speechProfilesJson, profileIndex)
                : NULL;
        customProfile = p;
        struct json_object *nameObj =
            p ? json_object_object_get(p, "name") : NULL;
        struct json_object *sysObj =
            p ? json_object_object_get(p, "speechSystem") : NULL;
        CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
        if (name != NULL)
            set(speechProfileNameString, MUIA_String_Contents, name);
        if (sysObj != NULL)
            sys = (SpeechSystem)json_object_get_int(sysObj);
    }

    /* System cycle selection (only meaningful for custom/new) */
    if (speechSystemCycle != NULL) {
        LONG active = 0;
        for (LONG i = 0; speechSystemOptions[i] != NULL; i++) {
            if (SPEECH_SYSTEM_NAMES[sys] != NULL &&
                strcmp(speechSystemOptions[i], SPEECH_SYSTEM_NAMES[sys]) == 0) {
                active = i;
                break;
            }
        }
        set(speechSystemCycle, MUIA_Cycle_Active, active);
    }

    updateGroupVisibilityForSystem(sys, isCustomOrNew);

    /* Accent */
    if (workbenchAccentString != NULL) {
        CONST_STRPTR accent = NULL;
        if (isBuiltinListIndex(activeIndex)) {
            if (sys == SPEECH_SYSTEM_34)
                accent = configGetSpeechAccent34();
            else if (sys == SPEECH_SYSTEM_37)
                accent = configGetSpeechAccent37();
        } else if (customProfile != NULL) {
            struct json_object *aObj =
                json_object_object_get(customProfile, "speechAccent");
            if (aObj != NULL)
                accent = json_object_get_string(aObj);
        }
        if (accent == NULL)
            accent = configGetSpeechAccent();
        set(workbenchAccentString, MUIA_String_Contents, accent ? accent : "");
    }

#ifdef __AMIGAOS3__
    /* narrator.device parameters (Workbench profiles) */
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        LONG rate = (sys == SPEECH_SYSTEM_34) ? (LONG)configGetNarratorRate34()
                                              : (LONG)configGetNarratorRate37();
        LONG pitch = (sys == SPEECH_SYSTEM_34)
                         ? (LONG)configGetNarratorPitch34()
                         : (LONG)configGetNarratorPitch37();
        LONG mode = (sys == SPEECH_SYSTEM_34) ? (LONG)configGetNarratorMode34()
                                              : (LONG)configGetNarratorMode37();
        LONG sex = (sys == SPEECH_SYSTEM_34) ? (LONG)configGetNarratorSex34()
                                             : (LONG)configGetNarratorSex37();

        if (customProfile != NULL) {
            struct json_object *o = NULL;
            o = json_object_object_get(customProfile, "narratorRate");
            if (o != NULL)
                rate = (LONG)json_object_get_int(o);
            o = json_object_object_get(customProfile, "narratorPitch");
            if (o != NULL)
                pitch = (LONG)json_object_get_int(o);
            o = json_object_object_get(customProfile, "narratorMode");
            if (o != NULL)
                mode = (LONG)json_object_get_int(o);
            o = json_object_object_get(customProfile, "narratorSex");
            if (o != NULL)
                sex = (LONG)json_object_get_int(o);
        }

        if (workbenchRateString != NULL)
            set(workbenchRateString, MUIA_String_Integer, rate);
        if (workbenchPitchString != NULL)
            set(workbenchPitchString, MUIA_String_Integer, pitch);
        if (workbenchModeCycle != NULL)
            set(workbenchModeCycle, MUIA_Cycle_Active, (mode != 0) ? 1 : 0);
        if (workbenchSexCycle != NULL)
            set(workbenchSexCycle, MUIA_Cycle_Active, (sex != 0) ? 1 : 0);
    }
#endif

    /* Flite voice */
    if (fliteVoiceCycle != NULL) {
        LONG fv = (LONG)configGetSpeechFliteVoice();
        if (customProfile != NULL) {
            struct json_object *fvObj =
                json_object_object_get(customProfile, "speechFliteVoice");
            if (fvObj != NULL)
                fv = (LONG)json_object_get_int(fvObj);
        }
        set(fliteVoiceCycle, MUIA_Cycle_Active, fv);
    }

    /* OpenAI fields */
    if (openAiApiKeyString != NULL) {
        STRPTR key = configGetOpenAiApiKey();
        if (customProfile != NULL) {
            struct json_object *kObj =
                json_object_object_get(customProfile, "openAiApiKey");
            if (kObj != NULL)
                key = (STRPTR)json_object_get_string(kObj);
        }
        set(openAiApiKeyString, MUIA_String_Contents, key ? key : "");
    }
    if (openAiTtsModelCycle != NULL) {
        LONG m = (LONG)configGetOpenAITTSModel();
        if (customProfile != NULL) {
            struct json_object *mObj =
                json_object_object_get(customProfile, "openAITTSModel");
            if (mObj != NULL)
                m = (LONG)json_object_get_int(mObj);
        }
        set(openAiTtsModelCycle, MUIA_Cycle_Active, m);
    }
    if (openAiTtsVoiceCycle != NULL) {
        LONG v = (LONG)configGetOpenAITTSVoice();
        if (customProfile != NULL) {
            struct json_object *vObj =
                json_object_object_get(customProfile, "openAITTSVoice");
            if (vObj != NULL)
                v = (LONG)json_object_get_int(vObj);
        }
        set(openAiTtsVoiceCycle, MUIA_Cycle_Active, v);
    }
    if (openAiVoiceInstructionsEditor != NULL) {
        STRPTR instr = configGetOpenAIVoiceInstructions();
        if (customProfile != NULL) {
            struct json_object *iObj = json_object_object_get(
                customProfile, "openAIVoiceInstructions");
            if (iObj != NULL)
                instr = (STRPTR)json_object_get_string(iObj);
        }
        set(openAiVoiceInstructionsEditor, MUIA_TextEditor_Contents,
            instr ? instr : "");
    }

    /* ElevenLabs fields */
    if (elevenLabsAPIKeyString != NULL) {
        STRPTR k = configGetElevenLabsAPIKey();
        if (customProfile != NULL) {
            struct json_object *ko =
                json_object_object_get(customProfile, "elevenLabsAPIKey");
            if (ko != NULL)
                k = (STRPTR)json_object_get_string(ko);
        }
        set(elevenLabsAPIKeyString, MUIA_String_Contents, k ? k : "");
    }
    if (elevenLabsCurrentModelText != NULL) {
        STRPTR mn = configGetElevenLabsModelName();
        if (customProfile != NULL) {
            struct json_object *mno =
                json_object_object_get(customProfile, "elevenLabsModelName");
            if (mno != NULL)
                mn = (STRPTR)json_object_get_string(mno);
        }
        set(elevenLabsCurrentModelText, MUIA_Text_Contents,
            mn != NULL ? mn : (STRPTR)STRING_NONE_SELECTED);
    }
    if (elevenLabsCurrentVoiceText != NULL) {
        STRPTR vn = configGetElevenLabsVoiceName();
        if (customProfile != NULL) {
            struct json_object *vno =
                json_object_object_get(customProfile, "elevenLabsVoiceName");
            if (vno != NULL)
                vn = (STRPTR)json_object_get_string(vno);
        }
        set(elevenLabsCurrentVoiceText, MUIA_Text_Contents,
            vn != NULL ? vn : (STRPTR)STRING_NONE_SELECTED);
    }

    setFieldsEnabledForSystem(sys, isCustomOrNew);
    /* Update per-provider dependency warnings (shown only in relevant groups)
     */
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        updateRequirementWarningForSystem(sys, workbenchWarningText);
    } else if (sys == SPEECH_SYSTEM_FLITE) {
        updateRequirementWarningForSystem(sys, fliteWarningText);
    } else {
        /* Clear both */
        updateRequirementWarningForSystem(sys, workbenchWarningText);
        updateRequirementWarningForSystem(sys, fliteWarningText);
    }

    /* Built-ins can’t be deleted; New profile can’t be deleted */
    if (deleteProfileButton != NULL)
        set(deleteProfileButton, MUIA_Disabled,
            (activeIndex <= getBuiltinSpeechProviderCount()));

    /* Save only makes sense for custom/new profiles. */
    if (saveProfileButton != NULL)
        set(saveProfileButton, MUIA_Disabled, !isCustomOrNew);

    endRightPanelUpdate();
}

HOOKPROTONHNONP(ProfileSelectedFunc, void) {
    if (suppressProfileSelected)
        return;
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);

    /* Profile selection toggles group visibility; keep window height stable. */
    LONG wasOpen = FALSE;
    LONG h = 0;
    if (speechProviderSettingsRequesterWindowObject != NULL) {
        get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            &wasOpen);
        if (wasOpen)
            get(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height,
                &h);
    }

    loadProfileIntoUI(active);

    if (speechProviderSettingsRequesterWindowObject != NULL && wasOpen &&
        h > 0) {
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Height, h);
    }

    lastSelectedProfile = active;
    speechSettingsDirty = FALSE;
}
MakeHook(SpeechProviderProfileSelectedHook, ProfileSelectedFunc);

static struct json_object *createProfileFromUI(CONST_STRPTR name) {
    struct json_object *p = json_object_new_object();
    json_object_object_add(p, "name", json_object_new_string(name ? name : ""));

    /* System */
    LONG sysIdx = 0;
    get(speechSystemCycle, MUIA_Cycle_Active, &sysIdx);
    SpeechSystem sys = SPEECH_SYSTEM_OPENAI;
    if (speechSystemOptions[sysIdx] != NULL) {
        for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
            if (strcmp(speechSystemOptions[sysIdx], SPEECH_SYSTEM_NAMES[s]) ==
                0) {
                sys = (SpeechSystem)s;
                break;
            }
        }
    }
    json_object_object_add(p, "speechSystem", json_object_new_int((int)sys));

    /* Accent */
    STRPTR accent = NULL;
    if (workbenchAccentString != NULL)
        get(workbenchAccentString, MUIA_String_Contents, &accent);
    if (accent != NULL && strlen(accent) > 0) {
        json_object_object_add(p, "speechAccent",
                               json_object_new_string(accent));
    }

#ifdef __AMIGAOS3__
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        LONG rate = 0, pitch = 0, mode = 0, sex = 0;
        if (workbenchRateString != NULL)
            get(workbenchRateString, MUIA_String_Integer, &rate);
        if (workbenchPitchString != NULL)
            get(workbenchPitchString, MUIA_String_Integer, &pitch);
        if (workbenchModeCycle != NULL)
            get(workbenchModeCycle, MUIA_Cycle_Active, &mode);
        if (workbenchSexCycle != NULL)
            get(workbenchSexCycle, MUIA_Cycle_Active, &sex);

        json_object_object_add(p, "narratorRate",
                               json_object_new_int((int)rate));
        json_object_object_add(p, "narratorPitch",
                               json_object_new_int((int)pitch));
        json_object_object_add(p, "narratorMode",
                               json_object_new_int((int)((mode != 0) ? 1 : 0)));
        json_object_object_add(p, "narratorSex",
                               json_object_new_int((int)((sex != 0) ? 1 : 0)));
    }
#endif

    /* Flite voice */
    LONG fv = 0;
    if (fliteVoiceCycle != NULL)
        get(fliteVoiceCycle, MUIA_Cycle_Active, &fv);
    json_object_object_add(p, "speechFliteVoice", json_object_new_int((int)fv));

    /* OpenAI */
    STRPTR apiKey = NULL;
    if (openAiApiKeyString != NULL)
        get(openAiApiKeyString, MUIA_String_Contents, &apiKey);
    if (apiKey != NULL && strlen(apiKey) > 0)
        json_object_object_add(p, "openAiApiKey",
                               json_object_new_string(apiKey));

    LONG m = 0, v = 0;
    if (openAiTtsModelCycle != NULL)
        get(openAiTtsModelCycle, MUIA_Cycle_Active, &m);
    if (openAiTtsVoiceCycle != NULL)
        get(openAiTtsVoiceCycle, MUIA_Cycle_Active, &v);
    json_object_object_add(p, "openAITTSModel", json_object_new_int((int)m));
    json_object_object_add(p, "openAITTSVoice", json_object_new_int((int)v));

    STRPTR instr = NULL;
    if (openAiVoiceInstructionsEditor != NULL)
        get(openAiVoiceInstructionsEditor, MUIA_TextEditor_Contents, &instr);
    if (instr != NULL && strlen(instr) > 0)
        json_object_object_add(p, "openAIVoiceInstructions",
                               json_object_new_string(instr));

    /* ElevenLabs */
    STRPTR eApiKey = NULL;
    if (elevenLabsAPIKeyString != NULL)
        get(elevenLabsAPIKeyString, MUIA_String_Contents, &eApiKey);
    if (eApiKey != NULL && strlen(eApiKey) > 0) {
        json_object_object_add(p, "elevenLabsAPIKey",
                               json_object_new_string(eApiKey));
    }

    LONG modelActive = MUIV_NList_Active_Off;
    if (elevenLabsModelList != NULL)
        get(elevenLabsModelList, MUIA_NList_Active, &modelActive);
    if (modelActive != MUIV_NList_Active_Off && elevenLabsModelsJson != NULL) {
        STRPTR selectedModelName = NULL;
        DoMethod(elevenLabsModelList, MUIM_NList_GetEntry, modelActive,
                 &selectedModelName);
        if (selectedModelName != NULL) {
            json_object_object_add(p, "elevenLabsModelName",
                                   json_object_new_string(selectedModelName));

            struct json_object *modelsArray = NULL;
            if (json_object_object_get_ex(elevenLabsModelsJson, "models",
                                          &modelsArray) ||
                json_object_is_type(elevenLabsModelsJson, json_type_array)) {
                if (modelsArray == NULL)
                    modelsArray = elevenLabsModelsJson;
                LONG modelCount = json_object_array_length(modelsArray);
                for (LONG i = 0; i < modelCount; i++) {
                    struct json_object *modelObj =
                        json_object_array_get_idx(modelsArray, i);
                    struct json_object *nameObj = NULL;
                    if (modelObj == NULL)
                        continue;
                    if (json_object_object_get_ex(modelObj, "name", &nameObj)) {
                        CONST_STRPTR n = json_object_get_string(nameObj);
                        if (n != NULL && strcmp(n, selectedModelName) == 0) {
                            struct json_object *modelIdObj = NULL;
                            if (json_object_object_get_ex(modelObj, "model_id",
                                                          &modelIdObj)) {
                                CONST_STRPTR modelId =
                                    json_object_get_string(modelIdObj);
                                if (modelId != NULL) {
                                    json_object_object_add(
                                        p, "elevenLabsModel",
                                        json_object_new_string(modelId));
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    LONG voiceActive = MUIV_NList_Active_Off;
    if (elevenLabsVoiceList != NULL)
        get(elevenLabsVoiceList, MUIA_NList_Active, &voiceActive);
    if (voiceActive != MUIV_NList_Active_Off && elevenLabsVoicesJson != NULL) {
        STRPTR selectedVoiceName = NULL;
        DoMethod(elevenLabsVoiceList, MUIM_NList_GetEntry, voiceActive,
                 &selectedVoiceName);
        if (selectedVoiceName != NULL) {
            json_object_object_add(p, "elevenLabsVoiceName",
                                   json_object_new_string(selectedVoiceName));

            struct json_object *voicesArray = NULL;
            if (json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                          &voicesArray) ||
                json_object_is_type(elevenLabsVoicesJson, json_type_array)) {
                if (voicesArray == NULL)
                    voicesArray = elevenLabsVoicesJson;
                LONG voiceCount = json_object_array_length(voicesArray);
                for (LONG i = 0; i < voiceCount; i++) {
                    struct json_object *voiceObj =
                        json_object_array_get_idx(voicesArray, i);
                    struct json_object *nameObj = NULL;
                    if (voiceObj == NULL)
                        continue;
                    if (json_object_object_get_ex(voiceObj, "name", &nameObj)) {
                        CONST_STRPTR n = json_object_get_string(nameObj);
                        if (n != NULL && strcmp(n, selectedVoiceName) == 0) {
                            struct json_object *voiceIdObj = NULL;
                            if (json_object_object_get_ex(voiceObj, "voice_id",
                                                          &voiceIdObj)) {
                                CONST_STRPTR voiceId =
                                    json_object_get_string(voiceIdObj);
                                if (voiceId != NULL) {
                                    json_object_object_add(
                                        p, "elevenLabsVoiceID",
                                        json_object_new_string(voiceId));
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    return p;
}

static void applyBuiltinProfileSelection(SpeechSystem sys) {
    configSetActiveSpeechProfileName(builtinNameForSystem(sys));
    configSetSpeechSystem(sys);

    /* Save system-specific settings from UI into config */
    if (sys == SPEECH_SYSTEM_34 || sys == SPEECH_SYSTEM_37) {
        STRPTR accent = NULL;
        if (workbenchAccentString != NULL)
            get(workbenchAccentString, MUIA_String_Contents, &accent);
        if (accent != NULL && strlen(accent) > 0) {
            if (sys == SPEECH_SYSTEM_34)
                configSetSpeechAccent34(accent);
            else
                configSetSpeechAccent37(accent);
        }
#ifdef __AMIGAOS3__
        LONG rate = 0, pitch = 0, mode = 0, sex = 0;
        if (workbenchRateString != NULL)
            get(workbenchRateString, MUIA_String_Integer, &rate);
        if (workbenchPitchString != NULL)
            get(workbenchPitchString, MUIA_String_Integer, &pitch);
        if (workbenchModeCycle != NULL)
            get(workbenchModeCycle, MUIA_Cycle_Active, &mode);
        if (workbenchSexCycle != NULL)
            get(workbenchSexCycle, MUIA_Cycle_Active, &sex);

        if (sys == SPEECH_SYSTEM_34) {
            configSetNarratorRate34((ULONG)rate);
            configSetNarratorPitch34((ULONG)pitch);
            configSetNarratorMode34((ULONG)((mode != 0) ? 1 : 0));
            configSetNarratorSex34((ULONG)((sex != 0) ? 1 : 0));
        } else {
            configSetNarratorRate37((ULONG)rate);
            configSetNarratorPitch37((ULONG)pitch);
            configSetNarratorMode37((ULONG)((mode != 0) ? 1 : 0));
            configSetNarratorSex37((ULONG)((sex != 0) ? 1 : 0));
        }
#endif
    } else if (sys == SPEECH_SYSTEM_FLITE) {
        LONG fv = 0;
        if (fliteVoiceCycle != NULL)
            get(fliteVoiceCycle, MUIA_Cycle_Active, &fv);
        configSetSpeechFliteVoice((SpeechFliteVoice)fv);
    } else if (sys == SPEECH_SYSTEM_OPENAI) {
        STRPTR key = NULL;
        if (openAiApiKeyString != NULL)
            get(openAiApiKeyString, MUIA_String_Contents, &key);
        configSetOpenAiApiKey(key);

        LONG m = 0, v = 0;
        if (openAiTtsModelCycle != NULL)
            get(openAiTtsModelCycle, MUIA_Cycle_Active, &m);
        if (openAiTtsVoiceCycle != NULL)
            get(openAiTtsVoiceCycle, MUIA_Cycle_Active, &v);
        configSetOpenAITTSModel((OpenAITTSModel)m);
        configSetOpenAITTSVoice((OpenAITTSVoice)v);

        STRPTR instr = NULL;
        if (openAiVoiceInstructionsEditor != NULL)
            get(openAiVoiceInstructionsEditor, MUIA_TextEditor_Contents,
                &instr);
        configSetOpenAIVoiceInstructions(instr);
    } else if (sys == SPEECH_SYSTEM_ELEVENLABS) {
        STRPTR apiKey = NULL;
        if (elevenLabsAPIKeyString != NULL)
            get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);
        configSetElevenLabsAPIKey(apiKey);

        /* Save model selection */
        LONG modelActive = MUIV_NList_Active_Off;
        if (elevenLabsModelList != NULL)
            get(elevenLabsModelList, MUIA_NList_Active, &modelActive);
        if (modelActive != MUIV_NList_Active_Off &&
            elevenLabsModelsJson != NULL) {
            STRPTR selectedModelName = NULL;
            DoMethod(elevenLabsModelList, MUIM_NList_GetEntry, modelActive,
                     &selectedModelName);
            if (selectedModelName != NULL) {
                configSetElevenLabsModelName(selectedModelName);

                struct json_object *modelsArray = NULL;
                if (json_object_object_get_ex(elevenLabsModelsJson, "models",
                                              &modelsArray) ||
                    json_object_is_type(elevenLabsModelsJson,
                                        json_type_array)) {
                    if (modelsArray == NULL)
                        modelsArray = elevenLabsModelsJson;
                    LONG modelCount = json_object_array_length(modelsArray);
                    for (LONG i = 0; i < modelCount; i++) {
                        struct json_object *modelObj =
                            json_object_array_get_idx(modelsArray, i);
                        struct json_object *nameObj = NULL;
                        if (modelObj == NULL)
                            continue;
                        if (json_object_object_get_ex(modelObj, "name",
                                                      &nameObj)) {
                            CONST_STRPTR n = json_object_get_string(nameObj);
                            if (n != NULL &&
                                strcmp(n, selectedModelName) == 0) {
                                struct json_object *modelIdObj = NULL;
                                if (json_object_object_get_ex(
                                        modelObj, "model_id", &modelIdObj)) {
                                    CONST_STRPTR modelId =
                                        json_object_get_string(modelIdObj);
                                    if (modelId != NULL)
                                        configSetElevenLabsModel(modelId);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }

        /* Save voice selection */
        LONG voiceActive = MUIV_NList_Active_Off;
        if (elevenLabsVoiceList != NULL)
            get(elevenLabsVoiceList, MUIA_NList_Active, &voiceActive);
        if (voiceActive != MUIV_NList_Active_Off &&
            elevenLabsVoicesJson != NULL) {
            STRPTR selectedVoiceName = NULL;
            DoMethod(elevenLabsVoiceList, MUIM_NList_GetEntry, voiceActive,
                     &selectedVoiceName);
            if (selectedVoiceName != NULL) {
                configSetElevenLabsVoiceName(selectedVoiceName);

                struct json_object *voicesArray = NULL;
                if (json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                              &voicesArray) ||
                    json_object_is_type(elevenLabsVoicesJson,
                                        json_type_array)) {
                    if (voicesArray == NULL)
                        voicesArray = elevenLabsVoicesJson;
                    LONG voiceCount = json_object_array_length(voicesArray);
                    for (LONG i = 0; i < voiceCount; i++) {
                        struct json_object *voiceObj =
                            json_object_array_get_idx(voicesArray, i);
                        struct json_object *nameObj = NULL;
                        if (voiceObj == NULL)
                            continue;
                        if (json_object_object_get_ex(voiceObj, "name",
                                                      &nameObj)) {
                            CONST_STRPTR n = json_object_get_string(nameObj);
                            if (n != NULL &&
                                strcmp(n, selectedVoiceName) == 0) {
                                struct json_object *voiceIdObj = NULL;
                                if (json_object_object_get_ex(
                                        voiceObj, "voice_id", &voiceIdObj)) {
                                    CONST_STRPTR voiceId =
                                        json_object_get_string(voiceIdObj);
                                    if (voiceId != NULL)
                                        configSetElevenLabsVoiceID(voiceId);
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    /* Apply accent for Workbench profiles into legacy speechAccent for now */
    if (sys == SPEECH_SYSTEM_34) {
        STRPTR a = configGetSpeechAccent34();
        if (a != NULL)
            configSetSpeechAccent(a);
    } else if (sys == SPEECH_SYSTEM_37) {
        STRPTR a = configGetSpeechAccent37();
        if (a != NULL)
            configSetSpeechAccent(a);
    }
}

static void reinitSpeechForActiveProfile(void) {
    struct SpeechRequestSettings s;
    configGetSpeechRequestSettings(&s);
    closeSpeech();
    if (initSpeech(s.speechSystem) == RETURN_ERROR) {
        switch (s.speechSystem) {
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
    configFreeSpeechRequestSettings(&s);
}

static BOOL isNameReservedForBuiltin(CONST_STRPTR name) {
    if (name == NULL || strlen(name) == 0)
        return TRUE;
    for (int s = 0; SPEECH_SYSTEM_NAMES[s] != NULL; s++) {
        if (strcasecmp(name, SPEECH_SYSTEM_NAMES[s]) == 0)
            return TRUE;
    }
    return FALSE;
}

static int findCustomProfileIndexByName(CONST_STRPTR name) {
    if (name == NULL || speechProfilesJson == NULL)
        return -1;
    if (!json_object_is_type(speechProfilesJson, json_type_array))
        return -1;
    int len = json_object_array_length(speechProfilesJson);
    for (int i = 0; i < len; i++) {
        struct json_object *p =
            json_object_array_get_idx(speechProfilesJson, i);
        if (p == NULL)
            continue;
        struct json_object *nameObj = json_object_object_get(p, "name");
        CONST_STRPTR n = nameObj ? json_object_get_string(nameObj) : NULL;
        if (n != NULL && strcmp(n, name) == 0)
            return i;
    }
    return -1;
}

HOOKPROTONHNONP(SpeechProviderOkFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(speechProfileList, MUIA_NList_Active, &active);
    if (active == MUIV_NList_Active_Off)
        return;

    if (isBuiltinListIndex(active)) {
        SpeechSystem sys = builtinSystemForListIndex(active);
        applyBuiltinProfileSelection(sys);
        reinitSpeechForActiveProfile();
        speechSettingsDirty = FALSE;
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            FALSE);
        return;
    }

    /* Custom / New Profile: save/update profile JSON and activate it */
    STRPTR profileName = NULL;
    get(speechProfileNameString, MUIA_String_Contents, &profileName);
    if (profileName == NULL || strlen(profileName) == 0) {
        displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
        return;
    }
    if (isNameReservedForBuiltin(profileName)) {
        displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
        return;
    }

    if (speechProfilesJson == NULL)
        speechProfilesJson = json_object_new_array();

    struct json_object *newProfile = createProfileFromUI(profileName);

    if (active == 0) {
        /* New profile: update if name already exists, else add */
        int existingIdx = findCustomProfileIndexByName(profileName);
        if (existingIdx >= 0)
            json_object_array_put_idx(speechProfilesJson, existingIdx,
                                      newProfile);
        else
            json_object_array_add(speechProfilesJson, newProfile);
    } else {
        int profileIndex = (int)(active - 1 - getBuiltinSpeechProviderCount());
        if (profileIndex < 0) {
            json_object_put(newProfile);
            return;
        }
        json_object_array_put_idx(speechProfilesJson, profileIndex, newProfile);
    }

    saveProfilesToConfig();
    configSetActiveSpeechProfileName(profileName);
    configSetSpeechSystem(getSpeechSystemFromCycle());
    reinitSpeechForActiveProfile();
    speechSettingsDirty = FALSE;
    set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(SpeechProviderOkHook, SpeechProviderOkFunc);

HOOKPROTONHNONP(SpeechProviderCancelFunc, void) {
    speechSettingsDirty = FALSE;
    set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(SpeechProviderCancelHook, SpeechProviderCancelFunc);

HOOKPROTONHNONP(SpeechProviderTestFunc, void) {
    /* Build a temporary settings struct from the current UI state. */
    struct SpeechRequestSettings s;
    memset(&s, 0, sizeof(s));

    s.speechSystem = getSpeechSystemFromCycle();

    /* Accent (Workbench) */
    if (workbenchAccentString != NULL) {
        STRPTR a = NULL;
        get(workbenchAccentString, MUIA_String_Contents, &a);
        s.accentPath = a;
    }

#ifdef __AMIGAOS3__
    if (workbenchRateString != NULL) {
        LONG v = 0;
        get(workbenchRateString, MUIA_String_Integer, &v);
        s.narratorRate = (UWORD)v;
    }
    if (workbenchPitchString != NULL) {
        LONG v = 0;
        get(workbenchPitchString, MUIA_String_Integer, &v);
        s.narratorPitch = (UWORD)v;
    }
    if (workbenchModeCycle != NULL) {
        LONG v = 0;
        get(workbenchModeCycle, MUIA_Cycle_Active, &v);
        s.narratorMode = (UWORD)((v != 0) ? 1 : 0);
    }
    if (workbenchSexCycle != NULL) {
        LONG v = 0;
        get(workbenchSexCycle, MUIA_Cycle_Active, &v);
        s.narratorSex = (UWORD)((v != 0) ? 1 : 0);
    }
#endif

    /* Flite */
    if (fliteVoiceCycle != NULL) {
        LONG fv = 0;
        get(fliteVoiceCycle, MUIA_Cycle_Active, &fv);
        s.fliteVoice = (SpeechFliteVoice)fv;
    }

    /* OpenAI */
    if (openAiApiKeyString != NULL) {
        STRPTR k = NULL;
        get(openAiApiKeyString, MUIA_String_Contents, &k);
        s.openAiApiKey = k;
    }
    if (openAiTtsModelCycle != NULL) {
        LONG m = 0;
        get(openAiTtsModelCycle, MUIA_Cycle_Active, &m);
        s.openAiTtsModel = (OpenAITTSModel)m;
    }
    if (openAiTtsVoiceCycle != NULL) {
        LONG v = 0;
        get(openAiTtsVoiceCycle, MUIA_Cycle_Active, &v);
        s.openAiTtsVoice = (OpenAITTSVoice)v;
    }
    if (openAiVoiceInstructionsEditor != NULL) {
        STRPTR instr = NULL;
        get(openAiVoiceInstructionsEditor, MUIA_TextEditor_Contents, &instr);
        s.openAiVoiceInstructions = instr;
    }

    /* ElevenLabs */
    if (elevenLabsAPIKeyString != NULL) {
        STRPTR k = NULL;
        get(elevenLabsAPIKeyString, MUIA_String_Contents, &k);
        s.elevenLabsApiKey = k;
    }

    /* Prefer the already-selected model/voice IDs from config fields if they
     * exist in the UI state (these are just pointers, not ownership). */
    s.elevenLabsModel = configGetElevenLabsModel();
    s.elevenLabsVoiceID = configGetElevenLabsVoiceID();

    /* Test phrase */
    STRPTR test =
        (STRPTR) "Aurora borealis, at this time of year, at this time of day, "
                 "in "
                 "this part of the country, localized entirely within your "
                 "kitchen? - Yes.\n"
                 "May I see it? No.";

    AudioFormat fmt = AUDIO_FORMAT_PCM;
    speakTextWithSettings(test, NULL, &fmt, &s);
}
MakeHook(SpeechProviderTestHook, SpeechProviderTestFunc);

HOOKPROTONHNONP(SaveProfileFunc, void) {
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);

    if (active <= 0) {
        /* Create new custom profile */
        STRPTR profileName = NULL;
        get(speechProfileNameString, MUIA_String_Contents, &profileName);
        if (profileName == NULL || strlen(profileName) == 0) {
            displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
            return;
        }

        if (isNameReservedForBuiltin(profileName)) {
            displayError(STRING_ERROR_PROFILE_NAME_RESERVED);
            return;
        }

        if (speechProfilesJson == NULL)
            speechProfilesJson = json_object_new_array();

        /* Update if existing name, else add */
        struct json_object *p = createProfileFromUI(profileName);
        int existingIdx = findCustomProfileIndexByName(profileName);
        if (existingIdx >= 0)
            json_object_array_put_idx(speechProfilesJson, existingIdx, p);
        else
            json_object_array_add(speechProfilesJson, p);
        saveProfilesToConfig();
        populateProfileList();
        return;
    }

    if (isBuiltinListIndex(active)) {
        /* Save button is disabled for built-ins */
        return;
    }

    /* Update existing custom profile */
    int profileIndex = (int)(active - 1 - getBuiltinSpeechProviderCount());
    if (profileIndex < 0 || speechProfilesJson == NULL)
        return;

    STRPTR profileName = NULL;
    get(speechProfileNameString, MUIA_String_Contents, &profileName);
    if (profileName == NULL || strlen(profileName) == 0) {
        displayError(STRING_ERROR_PROFILE_NAME_REQUIRED);
        return;
    }

    struct json_object *newProfile = createProfileFromUI(profileName);
    json_object_array_put_idx(speechProfilesJson, profileIndex, newProfile);
    saveProfilesToConfig();
    populateProfileList();
}
MakeHook(SpeechProviderSaveProfileHook, SaveProfileFunc);

HOOKPROTONHNONP(DeleteProfileFunc, void) {
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);
    if (active <= getBuiltinSpeechProviderCount())
        return;
    int profileIndex = (int)(active - 1 - getBuiltinSpeechProviderCount());
    if (profileIndex < 0 || speechProfilesJson == NULL)
        return;

    /* If deleting the currently active profile, fall back to OpenAI. */
    STRPTR activeName = configGetActiveSpeechProfileName();
    struct json_object *p =
        json_object_array_get_idx(speechProfilesJson, profileIndex);
    struct json_object *nameObj = p ? json_object_object_get(p, "name") : NULL;
    CONST_STRPTR name = nameObj ? json_object_get_string(nameObj) : NULL;
    if (activeName != NULL && name != NULL && strcmp(activeName, name) == 0) {
        configSetActiveSpeechProfileName(
            builtinNameForSystem(SPEECH_SYSTEM_OPENAI));
        configSetSpeechSystem(SPEECH_SYSTEM_OPENAI);
    }

    json_object_array_del_idx(speechProfilesJson, profileIndex, 1);
    saveProfilesToConfig();
    populateProfileList();
}
MakeHook(SpeechProviderDeleteProfileHook, DeleteProfileFunc);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
HOOKPROTONHNONP(SelectAccentFunc, void) {
    LONG active = 0;
    get(speechProfileList, MUIA_NList_Active, &active);

    struct FileRequester *fileRequester;
    if ((fileRequester = (struct FileRequester *)MUI_AllocAslRequestTags(
             ASL_FileRequest, ASLFR_Window, mainWindow, ASLFR_PopToFront, TRUE,
             ASLFR_Activate, TRUE, ASLFR_DrawersOnly, FALSE,
             ASLFR_InitialDrawer, "LOCALE:accents", ASLFR_DoPatterns, TRUE,
             ASLFR_InitialPattern, "#?.accent", TAG_DONE)) != NULL) {
        if (MUI_AslRequestTags(fileRequester, TAG_DONE)) {
            set(workbenchAccentString, MUIA_String_Contents,
                fileRequester->fr_File);

            if (isBuiltinListIndex(active)) {
                SpeechSystem sys = builtinSystemForListIndex(active);
                if (sys == SPEECH_SYSTEM_34) {
                    configSetSpeechAccent34(fileRequester->fr_File);
                } else if (sys == SPEECH_SYSTEM_37) {
                    configSetSpeechAccent37(fileRequester->fr_File);
                }
            }
        }
        FreeAslRequest(fileRequester);
    }
}
MakeHook(SpeechProviderSelectAccentHook, SelectAccentFunc);
#endif

LONG createSpeechProviderSettingsRequesterWindow(void) {
    if (speechProviderSettingsRequesterWindowObject != NULL)
        return RETURN_OK;

    populateSpeechSystemOptions();
    loadProfilesFromConfig();

    speechProviderSettingsRequesterWindowObject = WindowObject,
    MUIA_Window_Title, STRING_SPEECH_PROVIDER_SETTINGS, MUIA_Window_ID,
    MAKE_ID('S', 'P', 'R', 'V'), MUIA_Window_CloseGadget, TRUE,
    /* Keep window size stable when switching profiles (ShowMe toggles). */
        MUIA_Window_AutoAdjust, FALSE, MUIA_Window_SizeGadget, TRUE,
    MUIA_Window_DepthGadget, TRUE, MUIA_Window_DragBar, TRUE,
    MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered, MUIA_Window_TopEdge,
    MUIV_Window_TopEdge_Centered,
    /* Start at a reasonable height; user can resize vertically. */
        MUIA_Window_Height, 340, WindowContents, HGroup,
    /* Left panel - Profiles */
        Child, VGroup, MUIA_VertWeight, 100, MUIA_Frame, MUIV_Frame_Group,
    MUIA_FrameTitle, STRING_PROFILES, MUIA_FixWidth, 170, Child,
    NListviewObject, MUIA_VertWeight, 100, MUIA_NListview_NList,
    speechProfileList = NListObject, MUIA_NList_Format, "", MUIA_NList_Title,
    FALSE, MUIA_NList_MinLineHeight, 16, End, MUIA_CycleChain, TRUE,
    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto, End, Child,
    saveProfileButton =
        MUI_MakeObject(MUIO_Button, STRING_SAVE_PROFILE, TAG_DONE),
    Child,
    deleteProfileButton =
        MUI_MakeObject(MUIO_Button, STRING_DELETE_PROFILE, TAG_DONE),
    End,
    /* Right panel - Scrollable settings + OK/Cancel */
        Child, VGroup, MUIA_VertWeight, 100, Child,
    rightScrollgroup = ScrollgroupObject, MUIA_VertWeight, 100,
    MUIA_Scrollgroup_FreeHoriz, TRUE, MUIA_Scrollgroup_Contents,
    rightVirtgroup = VirtgroupObject, Child, customCommonGroup = VGroup, MUIA_Frame,
    MUIV_Frame_Group, MUIA_FrameTitle, STRING_PROFILE_NAME, Child, ColGroup(2),
    Child, Label(STRING_PROFILE_NAME), Child,
    speechProfileNameString = StringObject, MUIA_Frame, MUIV_Frame_String,
    MUIA_CycleChain, TRUE, MUIA_String_MaxLen, 64, End, Child,
    Label(STRING_MENU_SPEECH_SYSTEM), Child, speechSystemCycle = CycleObject,
    MUIA_Cycle_Entries, speechSystemOptions, MUIA_CycleChain, TRUE, End, End,
    End,

    Child, workbenchGroup = VGroup, MUIA_Frame, MUIV_Frame_Group,
    MUIA_FrameTitle, "Workbench narrator.device", Child, ColGroup(2), Child,
    Label(STRING_MENU_SPEECH_ACCENT), Child, HGroup, Child,
    workbenchAccentString = StringObject, MUIA_Frame, MUIV_Frame_String,
    MUIA_CycleChain, TRUE, End, Child,
    workbenchAccentBrowseButton = MUI_MakeObject(MUIO_Button, "...", TAG_DONE),
    End,
#ifdef __AMIGAOS3__
    Child, Label("Rate (wpm)"), Child, workbenchRateString = StringObject,
    MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, TRUE, MUIA_String_Accept,
    "0123456789", MUIA_String_MaxLen, 5, MUIA_String_Integer, (LONG)150, End,
    Child, Label("Pitch (Hz)"), Child, workbenchPitchString = StringObject,
    MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, TRUE, MUIA_String_Accept,
    "0123456789", MUIA_String_MaxLen, 5, MUIA_String_Integer, (LONG)110, End,
    Child, Label("Mode"), Child, workbenchModeCycle = CycleObject,
    MUIA_CycleChain, TRUE, MUIA_Cycle_Entries, narratorModeOptions,
    MUIA_Cycle_Active, 0, End, Child, Label("Sex"), Child,
    workbenchSexCycle = CycleObject, MUIA_CycleChain, TRUE, MUIA_Cycle_Entries,
    narratorSexOptions, MUIA_Cycle_Active, 0, End,
#endif
    Child, Label(""), Child, workbenchWarningText = TextObject,
    MUIA_Text_Contents, "", End, End, Child, VSpace(0), End,

    Child, fliteGroup = VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
    STRING_MENU_FLITE_VOICE, Child, ColGroup(2), Child,
    Label(STRING_MENU_FLITE_VOICE), Child, fliteVoiceCycle = CycleObject,
    MUIA_Cycle_Entries, (ULONG)SPEECH_FLITE_VOICE_NAMES, MUIA_CycleChain, TRUE,
    End, End, Child, fliteWarningText = TextObject, MUIA_Text_Contents, "", End,
    End,

    Child, openAiGroup = VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
    STRING_PROVIDER_OPENAI, Child, ColGroup(2), Child,
    Label(STRING_MENU_OPENAI_API_KEY), Child, openAiApiKeyString = StringObject,
    MUIA_Frame, MUIV_Frame_String, MUIA_CycleChain, TRUE, MUIA_String_MaxLen,
    256, End, Child, Label(STRING_MENU_SPEECH_OPENAI_MODEL), Child,
    openAiTtsModelCycle = CycleObject, MUIA_Cycle_Entries,
    (ULONG)OPENAI_TTS_MODEL_NAMES, MUIA_CycleChain, TRUE, End, Child,
    Label(STRING_MENU_OPENAI_VOICE), Child, openAiTtsVoiceCycle = CycleObject,
    MUIA_Cycle_Entries, (ULONG)OPENAI_TTS_VOICE_NAMES, MUIA_CycleChain, TRUE,
    End, End, Child, VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
    STRING_MENU_OPENAI_VOICE_INSTRUCTIONS, Child,
    openAiVoiceInstructionsEditor = TextEditorObject, MUIA_TextEditor_FixedFont,
    TRUE, MUIA_CycleChain, TRUE, End, End, End,

    Child, elevenLabsGroup = VGroup, MUIA_Frame, MUIV_Frame_Group,
    MUIA_FrameTitle, STRING_MENU_ELEVENLABS_SETTINGS, Child, VGroup, MUIA_Frame,
    MUIV_Frame_Group, MUIA_FrameTitle, STRING_MENU_OPENAI_API_KEY, Child,
    elevenLabsAPIKeyString = StringObject, MUIA_Frame, MUIV_Frame_String,
    MUIA_CycleChain, TRUE, MUIA_String_MaxLen, 256, End, End, Child, VGroup,
    MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle, STRING_MENU_ELEVENLABS_MODEL,
    Child, HGroup, Child, Label(STRING_CURRENT), Child,
    elevenLabsCurrentModelText = TextObject, MUIA_Text_Contents,
    STRING_NONE_SELECTED, End, End, Child, HGroup, Child,
    elevenLabsFetchModelsButton =
        MUI_MakeObject(MUIO_Button, STRING_FETCH_ELEVENLABS_MODELS, TAG_DONE),
    End, Child, NListviewObject, MUIA_NListview_NList,
    elevenLabsModelList = NListObject, MUIA_NList_ConstructHook2,
    &ConstructStringLI_TextHook, MUIA_NList_DestructHook2,
    &DestructStringLI_TextHook, MUIA_NList_DisplayHook2,
    &DisplayStringLI_TextHook, MUIA_NList_Format, "", MUIA_NList_Title, FALSE,
    MUIA_NList_MinLineHeight, 16, End, MUIA_CycleChain, TRUE,
    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto, MUIA_FixHeight, 80,
    End, End, Child, VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
    STRING_MENU_ELEVENLABS_VOICE, Child, HGroup, Child, Label(STRING_CURRENT),
    Child, elevenLabsCurrentVoiceText = TextObject, MUIA_Text_Contents,
    STRING_NONE_SELECTED, End, End, Child, HGroup, Child,
    elevenLabsVoiceSearchString = StringObject, MUIA_Frame, MUIV_Frame_String,
    MUIA_CycleChain, TRUE, End, Child,
    elevenLabsSearchButton =
        MUI_MakeObject(MUIO_Button, STRING_SEARCH, TAG_DONE),
    End, Child, NListviewObject, MUIA_NListview_NList,
    elevenLabsVoiceList = NListObject, MUIA_NList_ConstructHook2,
    &ConstructStringLI_TextHook, MUIA_NList_DestructHook2,
    &DestructStringLI_TextHook, MUIA_NList_DisplayHook2,
    &DisplayStringLI_TextHook, MUIA_NList_Format, "", MUIA_NList_Title, FALSE,
    MUIA_NList_MinLineHeight, 16, End, MUIA_CycleChain, TRUE,
    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto, MUIA_FixHeight, 120,
    End, End, End, End, End, Child, HGroup, Child,
    okButton = MUI_MakeObject(MUIO_Button, STRING_OK, TAG_DONE), Child,
    cancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL, TAG_DONE), Child,
    testButton = MUI_MakeObject(MUIO_Button, (STRPTR) "Test", TAG_DONE), End,
    End, End, End;

    if (speechProviderSettingsRequesterWindowObject == NULL) {
        displayError(STRING_ERROR_SPEECH_PROVIDER_SETTINGS_WINDOW_CREATE);
        return RETURN_ERROR;
    }

    /* Notifications */
    /* Close gadget should behave like Cancel. */
    DoMethod(speechProviderSettingsRequesterWindowObject, MUIM_Notify,
             MUIA_Window_CloseRequest, TRUE, MUIV_Notify_Application, 2,
             MUIM_CallHook, &SpeechProviderCancelHook);

    DoMethod(speechProfileList, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderProfileSelectedHook);

    DoMethod(saveProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderSaveProfileHook);
    DoMethod(deleteProfileButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderDeleteProfileHook);

    DoMethod(speechSystemCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderSystemChangedHook);

    DoMethod(elevenLabsFetchModelsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsFetchModelsButtonClickedHook);
    DoMethod(elevenLabsSearchButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsSearchVoicesButtonClickedHook);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    DoMethod(workbenchAccentBrowseButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderSelectAccentHook);
#endif

    DoMethod(okButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &SpeechProviderOkHook);
    DoMethod(cancelButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderCancelHook);
    DoMethod(testButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &SpeechProviderTestHook);

    return RETURN_OK;
}

void openSpeechProviderSettingsRequesterWindow(void) {
    if (speechProviderSettingsRequesterWindowObject != NULL) {
        /* Reload profiles to pick up any changes */
        loadProfilesFromConfig();
        populateProfileList();
        set(speechProviderSettingsRequesterWindowObject, MUIA_Window_Open,
            TRUE);
    }
}

#endif /* !DAEMON */
