#include <exec/exec.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <string.h>
#include "ElevenLabsSettingsRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"
#include "openai.h"

#define ELEVENLABS_HOST "api.elevenlabs.io"
#define ELEVENLABS_PORT 443

Object *elevenLabsSettingsRequesterWindowObject;
Object *elevenLabsAPIKeyString;
Object *elevenLabsModelList;
Object *elevenLabsVoiceSearchString;
Object *elevenLabsVoiceList;
Object *elevenLabsSearchButton;
Object *elevenLabsFetchModelsButton;
static Object *elevenLabsCurrentModelText;
static Object *elevenLabsCurrentVoiceText;

static struct json_object *elevenLabsModelsJson = NULL;
static struct json_object *elevenLabsVoicesJson = NULL;

/* Forward declarations */
static struct json_object *getElevenLabsModels(CONST_STRPTR apiKey);
static struct json_object *searchElevenLabsVoices(CONST_STRPTR apiKey,
                                                  CONST_STRPTR query);
static void populateModelList(void);
static void populateVoiceList(void);

/* NList hooks for models */
HOOKPROTONHNO(ConstructModelLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    return entry;
}
MakeHook(ConstructModelLI_TextHook, ConstructModelLI_TextFunc);

HOOKPROTONHNO(DestructModelLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    /* Don't free - strings owned by JSON object */
}
MakeHook(DestructModelLI_TextHook, DestructModelLI_TextFunc);

HOOKPROTONHNO(DisplayModelLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayModelLI_TextHook, DisplayModelLI_TextFunc);

/* NList hooks for voices */
HOOKPROTONHNO(ConstructVoiceLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    return entry;
}
MakeHook(ConstructVoiceLI_TextHook, ConstructVoiceLI_TextFunc);

HOOKPROTONHNO(DestructVoiceLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    /* Don't free - strings owned by JSON object */
}
MakeHook(DestructVoiceLI_TextHook, DestructVoiceLI_TextFunc);

HOOKPROTONHNO(DisplayVoiceLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayVoiceLI_TextHook, DisplayVoiceLI_TextFunc);

HOOKPROTONHNONP(ElevenLabsSettingsWindowOpenFunc, void) {
    /* Prefill the API key from the current config value */
    set(elevenLabsAPIKeyString, MUIA_String_Contents,
        config.elevenLabsAPIKey != NULL ? config.elevenLabsAPIKey : "");

    /* Update current model text */
    if (config.elevenLabsModelName != NULL) {
        set(elevenLabsCurrentModelText, MUIA_Text_Contents,
            config.elevenLabsModelName);
    } else {
        set(elevenLabsCurrentModelText, MUIA_Text_Contents,
            STRING_NONE_SELECTED);
    }

    /* Update current voice text */
    if (config.elevenLabsVoiceName != NULL) {
        set(elevenLabsCurrentVoiceText, MUIA_Text_Contents,
            config.elevenLabsVoiceName);
    } else {
        set(elevenLabsCurrentVoiceText, MUIA_Text_Contents,
            STRING_NONE_SELECTED);
    }
}
MakeHook(ElevenLabsSettingsWindowOpenHook, ElevenLabsSettingsWindowOpenFunc);

HOOKPROTONHNONP(ElevenLabsFetchModelsButtonClickedFunc, void) {
    STRPTR apiKey;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);

    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_ELEVENLABS_API_KEY_REQUIRED);
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
        populateModelList();
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
    STRPTR apiKey;
    STRPTR searchQuery;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);
    get(elevenLabsVoiceSearchString, MUIA_String_Contents, &searchQuery);

    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_ELEVENLABS_API_KEY_REQUIRED);
        return;
    }

    updateStatusBar(STRING_SEARCHING_ELEVENLABS_VOICES, yellowPen);
    set(elevenLabsSearchButton, MUIA_Disabled, TRUE);

    if (elevenLabsVoicesJson != NULL) {
        json_object_put(elevenLabsVoicesJson);
        elevenLabsVoicesJson = NULL;
    }

    elevenLabsVoicesJson = searchElevenLabsVoices(apiKey, searchQuery);

    if (elevenLabsVoicesJson != NULL) {
        populateVoiceList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_SEARCHING_ELEVENLABS_VOICES);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(elevenLabsSearchButton, MUIA_Disabled, FALSE);
}
MakeHook(ElevenLabsSearchVoicesButtonClickedHook,
         ElevenLabsSearchVoicesButtonClickedFunc);

HOOKPROTONHNONP(ElevenLabsSettingsOkButtonClickedFunc, void) {
    /* Save API key */
    STRPTR apiKey;
    get(elevenLabsAPIKeyString, MUIA_String_Contents, &apiKey);
    if (config.elevenLabsAPIKey != NULL) {
        FreeVec(config.elevenLabsAPIKey);
        config.elevenLabsAPIKey = NULL;
    }
    if (apiKey != NULL && strlen(apiKey) > 0) {
        config.elevenLabsAPIKey = AllocVec(strlen(apiKey) + 1, MEMF_CLEAR);
        strncpy(config.elevenLabsAPIKey, apiKey, strlen(apiKey));
    }

    /* Save selected model */
    LONG modelActive = MUIV_NList_Active_Off;
    get(elevenLabsModelList, MUIA_NList_Active, &modelActive);
    if (modelActive != MUIV_NList_Active_Off && elevenLabsModelsJson != NULL) {
        /* Get the selected model name from the list */
        STRPTR selectedModelName = NULL;
        DoMethod(elevenLabsModelList, MUIM_NList_GetEntry, modelActive,
                 &selectedModelName);

        if (selectedModelName != NULL) {
            /* Save the model name */
            if (config.elevenLabsModelName != NULL) {
                FreeVec(config.elevenLabsModelName);
                config.elevenLabsModelName = NULL;
            }
            config.elevenLabsModelName =
                AllocVec(strlen(selectedModelName) + 1, MEMF_CLEAR);
            strncpy(config.elevenLabsModelName, selectedModelName,
                    strlen(selectedModelName));

            /* Find and save the model ID */
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
                    if (json_object_object_get_ex(modelObj, "name", &nameObj)) {
                        CONST_STRPTR name = json_object_get_string(nameObj);
                        if (name != NULL &&
                            strcmp(name, selectedModelName) == 0) {
                            struct json_object *modelIdObj = NULL;
                            if (json_object_object_get_ex(modelObj, "model_id",
                                                          &modelIdObj)) {
                                CONST_STRPTR modelId =
                                    json_object_get_string(modelIdObj);
                                if (config.elevenLabsModel != NULL) {
                                    FreeVec(config.elevenLabsModel);
                                    config.elevenLabsModel = NULL;
                                }
                                if (modelId != NULL) {
                                    config.elevenLabsModel = AllocVec(
                                        strlen(modelId) + 1, MEMF_CLEAR);
                                    strncpy(config.elevenLabsModel, modelId,
                                            strlen(modelId));
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    /* Save selected voice */
    LONG voiceActive = MUIV_NList_Active_Off;
    get(elevenLabsVoiceList, MUIA_NList_Active, &voiceActive);
    if (voiceActive != MUIV_NList_Active_Off && elevenLabsVoicesJson != NULL) {
        /* Get the selected voice name from the list */
        STRPTR selectedVoiceName = NULL;
        DoMethod(elevenLabsVoiceList, MUIM_NList_GetEntry, voiceActive,
                 &selectedVoiceName);

        if (selectedVoiceName != NULL) {
            /* Save the voice name */
            if (config.elevenLabsVoiceName != NULL) {
                FreeVec(config.elevenLabsVoiceName);
                config.elevenLabsVoiceName = NULL;
            }
            config.elevenLabsVoiceName =
                AllocVec(strlen(selectedVoiceName) + 1, MEMF_CLEAR);
            strncpy(config.elevenLabsVoiceName, selectedVoiceName,
                    strlen(selectedVoiceName));

            /* Find and save the voice ID */
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
                    if (json_object_object_get_ex(voiceObj, "name", &nameObj)) {
                        CONST_STRPTR name = json_object_get_string(nameObj);
                        if (name != NULL &&
                            strcmp(name, selectedVoiceName) == 0) {
                            struct json_object *voiceIdObj = NULL;
                            if (json_object_object_get_ex(voiceObj, "voice_id",
                                                          &voiceIdObj)) {
                                CONST_STRPTR voiceId =
                                    json_object_get_string(voiceIdObj);
                                if (config.elevenLabsVoiceID != NULL) {
                                    FreeVec(config.elevenLabsVoiceID);
                                    config.elevenLabsVoiceID = NULL;
                                }
                                if (voiceId != NULL) {
                                    config.elevenLabsVoiceID = AllocVec(
                                        strlen(voiceId) + 1, MEMF_CLEAR);
                                    strncpy(config.elevenLabsVoiceID, voiceId,
                                            strlen(voiceId));
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }

    set(elevenLabsSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(ElevenLabsSettingsOkButtonClickedHook,
         ElevenLabsSettingsOkButtonClickedFunc);

/**
 * Populate the model list from the fetched JSON
 **/
static void populateModelList(void) {
    DoMethod(elevenLabsModelList, MUIM_NList_Clear);

    if (elevenLabsModelsJson == NULL)
        return;

    struct json_object *modelsArray = NULL;
    if (!json_object_object_get_ex(elevenLabsModelsJson, "models",
                                   &modelsArray)) {
        /* Try if root is array */
        if (json_object_is_type(elevenLabsModelsJson, json_type_array)) {
            modelsArray = elevenLabsModelsJson;
        } else {
            return;
        }
    }

    LONG modelCount = json_object_array_length(modelsArray);
    LONG selectedIndex = MUIV_NList_Active_Off;

    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelObj =
            json_object_array_get_idx(modelsArray, i);
        struct json_object *nameObj = NULL;
        struct json_object *modelIdObj = NULL;

        if (json_object_object_get_ex(modelObj, "name", &nameObj)) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            DoMethod(elevenLabsModelList, MUIM_NList_InsertSingle, name,
                     MUIV_NList_Insert_Bottom);

            /* Check if this is the currently selected model */
            if (config.elevenLabsModel != NULL &&
                json_object_object_get_ex(modelObj, "model_id", &modelIdObj)) {
                CONST_STRPTR modelId = json_object_get_string(modelIdObj);
                if (modelId != NULL &&
                    strcmp(config.elevenLabsModel, modelId) == 0) {
                    selectedIndex = i;
                }
            }
        }
    }

    if (selectedIndex != MUIV_NList_Active_Off) {
        set(elevenLabsModelList, MUIA_NList_Active, selectedIndex);
    }
}

/**
 * Populate the voice list from the fetched JSON
 **/
static void populateVoiceList(void) {
    DoMethod(elevenLabsVoiceList, MUIM_NList_Clear);

    if (elevenLabsVoicesJson == NULL)
        return;

    struct json_object *voicesArray = NULL;
    if (!json_object_object_get_ex(elevenLabsVoicesJson, "voices",
                                   &voicesArray)) {
        /* Try if root is array */
        if (json_object_is_type(elevenLabsVoicesJson, json_type_array)) {
            voicesArray = elevenLabsVoicesJson;
        } else {
            return;
        }
    }

    LONG voiceCount = json_object_array_length(voicesArray);
    LONG selectedIndex = MUIV_NList_Active_Off;

    for (LONG i = 0; i < voiceCount; i++) {
        struct json_object *voiceObj =
            json_object_array_get_idx(voicesArray, i);
        struct json_object *nameObj = NULL;
        struct json_object *voiceIdObj = NULL;

        if (json_object_object_get_ex(voiceObj, "name", &nameObj)) {
            CONST_STRPTR name = json_object_get_string(nameObj);
            DoMethod(elevenLabsVoiceList, MUIM_NList_InsertSingle, name,
                     MUIV_NList_Insert_Bottom);

            /* Check if this is the currently selected voice */
            if (config.elevenLabsVoiceID != NULL &&
                json_object_object_get_ex(voiceObj, "voice_id", &voiceIdObj)) {
                CONST_STRPTR voiceId = json_object_get_string(voiceIdObj);
                if (voiceId != NULL &&
                    strcmp(config.elevenLabsVoiceID, voiceId) == 0) {
                    selectedIndex = i;
                }
            }
        }
    }

    if (selectedIndex != MUIV_NList_Active_Off) {
        set(elevenLabsVoiceList, MUIA_NList_Active, selectedIndex);
    }
}

/**
 * Get models from ElevenLabs API
 * @param apiKey the ElevenLabs API key
 * @return a pointer to a new json_object or NULL
 **/
static struct json_object *getElevenLabsModels(CONST_STRPTR apiKey) {
    return makeHttpsGetRequest(ELEVENLABS_HOST, ELEVENLABS_PORT, "/v1/models",
                               apiKey, "xi-api-key", FALSE, config.proxyEnabled,
                               config.proxyHost, config.proxyPort,
                               config.proxyUsesSSL, config.proxyRequiresAuth,
                               config.proxyUsername, config.proxyPassword);
}

/**
 * Search voices from ElevenLabs API
 * @param apiKey the ElevenLabs API key
 * @param query the search query (can be empty for all voices)
 * @return a pointer to a new json_object or NULL
 **/
static struct json_object *searchElevenLabsVoices(CONST_STRPTR apiKey,
                                                  CONST_STRPTR query) {
    UBYTE endpoint[512];
    if (query != NULL && strlen(query) > 0) {
        snprintf(endpoint, sizeof(endpoint), "/v2/voices?search=%s", query);
    } else {
        snprintf(endpoint, sizeof(endpoint), "/v2/voices");
    }
    return makeHttpsGetRequest(ELEVENLABS_HOST, ELEVENLABS_PORT, endpoint,
                               apiKey, "xi-api-key", FALSE, config.proxyEnabled,
                               config.proxyHost, config.proxyPort,
                               config.proxyUsesSSL, config.proxyRequiresAuth,
                               config.proxyUsername, config.proxyPassword);
}

/**
 * Create the ElevenLabs settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createElevenLabsSettingsRequesterWindow() {
    Object *elevenLabsSettingsOkButton, *elevenLabsSettingsCancelButton;

    if ((elevenLabsSettingsRequesterWindowObject = WindowObject,
         MUIA_Window_Title, STRING_MENU_ELEVENLABS_SETTINGS, MUIA_Window_Width,
         500, MUIA_Window_Height, 400, MUIA_Window_CloseGadget, FALSE,
         WindowContents, VGroup,
         /* API Key Section */
         Child, VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
         STRING_MENU_OPENAI_API_KEY, Child,
         elevenLabsAPIKeyString = StringObject, MUIA_Frame, MUIV_Frame_String,
         MUIA_CycleChain, TRUE, MUIA_String_Contents, config.elevenLabsAPIKey,
         End, End,
         /* Model Section */
         Child, VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
         STRING_MENU_ELEVENLABS_MODEL, Child, HGroup, Child,
         Label(STRING_CURRENT), Child, elevenLabsCurrentModelText = TextObject,
         MUIA_Text_Contents, STRING_NONE_SELECTED, End, End, Child, HGroup,
         Child,
         elevenLabsFetchModelsButton = MUI_MakeObject(
             MUIO_Button, STRING_FETCH_ELEVENLABS_MODELS, MUIA_CycleChain, TRUE,
             MUIA_InputMode, MUIV_InputMode_RelVerify, TAG_DONE),
         End, Child, NListviewObject, MUIA_NListview_NList,
         elevenLabsModelList = NListObject, MUIA_NList_ConstructHook2,
         &ConstructModelLI_TextHook, MUIA_NList_DestructHook2,
         &DestructModelLI_TextHook, MUIA_NList_DisplayHook2,
         &DisplayModelLI_TextHook, MUIA_NList_Format, "", MUIA_NList_Title,
         FALSE, MUIA_NList_MinLineHeight, 16, End, MUIA_CycleChain, TRUE,
         MUIA_VertWeight, 30, End, End,
         /* Voice Section */
         Child, VGroup, MUIA_Frame, MUIV_Frame_Group, MUIA_FrameTitle,
         STRING_MENU_ELEVENLABS_VOICE, Child, HGroup, Child,
         Label(STRING_CURRENT), Child, elevenLabsCurrentVoiceText = TextObject,
         MUIA_Text_Contents, STRING_NONE_SELECTED, End, End, Child, HGroup,
         Child, elevenLabsVoiceSearchString = StringObject, MUIA_Frame,
         MUIV_Frame_String, MUIA_CycleChain, TRUE, MUIA_String_AdvanceOnCR,
         TRUE, End, Child,
         elevenLabsSearchButton = MUI_MakeObject(
             MUIO_Button, STRING_SEARCH, MUIA_CycleChain, TRUE, MUIA_InputMode,
             MUIV_InputMode_RelVerify, MUIA_Weight, 30, TAG_DONE),
         End, Child, NListviewObject, MUIA_NListview_NList,
         elevenLabsVoiceList = NListObject, MUIA_NList_ConstructHook2,
         &ConstructVoiceLI_TextHook, MUIA_NList_DestructHook2,
         &DestructVoiceLI_TextHook, MUIA_NList_DisplayHook2,
         &DisplayVoiceLI_TextHook, MUIA_NList_Format, "", MUIA_NList_Title,
         FALSE, MUIA_NList_MinLineHeight, 16, End, MUIA_CycleChain, TRUE,
         MUIA_VertWeight, 50, End, End,
         /* Buttons */
         Child, HGroup, Child,
         elevenLabsSettingsOkButton =
             MUI_MakeObject(MUIO_Button, STRING_OK, MUIA_Background, MUII_FILL,
                            MUIA_CycleChain, TRUE, MUIA_InputMode,
                            MUIV_InputMode_RelVerify, TAG_DONE),
         Child,
         elevenLabsSettingsCancelButton =
             MUI_MakeObject(MUIO_Button, STRING_CANCEL, MUIA_Background,
                            MUII_FILL, MUIA_CycleChain, TRUE, MUIA_InputMode,
                            MUIV_InputMode_RelVerify, TAG_DONE),
         End, End, End) == NULL) {
        displayError(STRING_ERROR_ELEVENLABS_SETTINGS_WINDOW_CREATE);
        return RETURN_ERROR;
    }

    /* Set up notifications */
    DoMethod(elevenLabsFetchModelsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsFetchModelsButtonClickedHook);

    DoMethod(elevenLabsSearchButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsSearchVoicesButtonClickedHook);

    DoMethod(elevenLabsSettingsOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook,
             &ElevenLabsSettingsOkButtonClickedHook);

    DoMethod(elevenLabsSettingsCancelButton, MUIM_Notify, MUIA_Pressed, FALSE,
             elevenLabsSettingsRequesterWindowObject, 3, MUIM_Set,
             MUIA_Window_Open, FALSE);

    return RETURN_OK;
}
