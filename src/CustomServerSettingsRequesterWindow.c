#include <exec/exec.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include "AmigaGPTConfig.h"
#include "CustomServerSettingsRequesterWindow.h"
#include "gui.h"
#include "openai.h"

Object *customServerTemplateCycle;
Object *customServerHostString;
Object *customServerPortString;
Object *customServerUsesSSLCycle;
Object *customServerApiKeyString;
Object *customServerChatModelString;
Object *customServerModelList;
Object *customServerFetchModelsButton;
Object *customServerApiEndpointCycle;
Object *customServerApiEndpointUrlString;
Object *customServerFullUrlPreviewString;
Object *customServerSettingsRequesterWindowObject;

static struct json_object *customServerModelsJson = NULL;

/* Template definitions */
enum {
    TEMPLATE_NONE = 0,
    TEMPLATE_GOOGLE_GEMINI,
    TEMPLATE_LM_STUDIO
};

/* Forward declarations */
static void populateModelList(void);

/* NList hooks for models */
HOOKPROTONHNO(ConstructCustomModelLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    STRPTR entry = (STRPTR)ncm->entry;
    return entry;
}
MakeHook(ConstructCustomModelLI_TextHook, ConstructCustomModelLI_TextFunc);

HOOKPROTONHNO(DestructCustomModelLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    /* Don't free - strings owned by JSON object */
}
MakeHook(DestructCustomModelLI_TextHook, DestructCustomModelLI_TextFunc);

HOOKPROTONHNO(DisplayCustomModelLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    STRPTR entry = (STRPTR)ndm->entry;
    ndm->strings[0] = entry;
}
MakeHook(DisplayCustomModelLI_TextHook, DisplayCustomModelLI_TextFunc);

/* Hook for when a model is selected from the list */
HOOKPROTONHNONP(CustomModelSelectedFunc, void) {
    LONG active = MUIV_NList_Active_Off;
    get(customServerModelList, MUIA_NList_Active, &active);

    if (active != MUIV_NList_Active_Off && customServerModelsJson != NULL &&
        json_object_is_type(customServerModelsJson, json_type_array)) {
        struct json_object *modelIdObj =
            json_object_array_get_idx(customServerModelsJson, active);
        if (modelIdObj != NULL) {
            CONST_STRPTR modelId = json_object_get_string(modelIdObj);
            if (modelId != NULL) {
                set(customServerChatModelString, MUIA_String_Contents, modelId);
            }
        }
    }
}
MakeHook(CustomModelSelectedHook, CustomModelSelectedFunc);

/* Hook for fetching models from the server */
HOOKPROTONHNONP(FetchCustomModelsFunc, void) {
    STRPTR host;
    LONG port;
    LONG usesSSL;
    STRPTR apiKey;
    STRPTR endpointUrl;

    get(customServerHostString, MUIA_String_Contents, &host);
    get(customServerPortString, MUIA_String_Integer, &port);
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    get(customServerApiKeyString, MUIA_String_Contents, &apiKey);
    get(customServerApiEndpointUrlString, MUIA_String_Contents, &endpointUrl);

    if (host == NULL || strlen(host) == 0) {
        displayError(STRING_ERROR_NO_HOST);
        return;
    }

    updateStatusBar(STRING_FETCHING_MODELS, yellowPen);
    set(customServerFetchModelsButton, MUIA_Disabled, TRUE);

    /* Free previous models JSON if any */
    if (customServerModelsJson != NULL) {
        json_object_put(customServerModelsJson);
        customServerModelsJson = NULL;
    }

    /* Fetch models from the custom server */
    customServerModelsJson = getChatModels(
        host, port, usesSSL == 1, apiKey, configGetProxyEnabled(),
        configGetProxyHost(), configGetProxyPort(), configGetProxyUsesSSL(),
        configGetProxyRequiresAuth(), configGetProxyUsername(),
        configGetProxyPassword(), endpointUrl);

    if (customServerModelsJson != NULL) {
        populateModelList();
        updateStatusBar(STRING_READY, greenPen);
    } else {
        displayError(STRING_ERROR_FETCHING_MODELS);
        updateStatusBar(STRING_READY, greenPen);
    }

    set(customServerFetchModelsButton, MUIA_Disabled, FALSE);
}
MakeHook(FetchCustomModelsHook, FetchCustomModelsFunc);

/* Populate the model list from JSON */
static void populateModelList(void) {
    DoMethod(customServerModelList, MUIM_NList_Clear);

    if (customServerModelsJson == NULL)
        return;

    /* getChatModels returns a simple array of model ID strings */
    if (!json_object_is_type(customServerModelsJson, json_type_array)) {
        return;
    }

    LONG modelCount = json_object_array_length(customServerModelsJson);
    STRPTR currentModel = NULL;
    get(customServerChatModelString, MUIA_String_Contents, &currentModel);
    LONG selectedIndex = MUIV_NList_Active_Off;

    for (LONG i = 0; i < modelCount; i++) {
        struct json_object *modelIdObj =
            json_object_array_get_idx(customServerModelsJson, i);
        CONST_STRPTR modelId = json_object_get_string(modelIdObj);

        if (modelId != NULL) {
            DoMethod(customServerModelList, MUIM_NList_InsertSingle, modelId,
                     MUIV_NList_Insert_Bottom);

            /* Check if this is the currently selected model */
            if (currentModel != NULL && strcmp(modelId, currentModel) == 0) {
                selectedIndex = i;
            }
        }
    }

    /* Select the current model in the list if found */
    if (selectedIndex != MUIV_NList_Active_Off) {
        set(customServerModelList, MUIA_NList_Active, selectedIndex);
    }
}

HOOKPROTONHNONP(TemplateSelectedFunc, void) {
    LONG template;
    get(customServerTemplateCycle, MUIA_Cycle_Active, &template);

    switch (template) {
    case TEMPLATE_GOOGLE_GEMINI:
        set(customServerHostString, MUIA_String_Contents,
            "generativelanguage.googleapis.com");
        set(customServerPortString, MUIA_String_Integer, 443);
        set(customServerUsesSSLCycle, MUIA_Cycle_Active, 1); /* SSL enabled */
        set(customServerChatModelString, MUIA_String_Contents,
            "gemini-2.0-flash");
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            API_ENDPOINT_CHAT_COMPLETIONS);
        set(customServerApiEndpointUrlString, MUIA_String_Contents,
            "v1beta/openai");
        break;
    case TEMPLATE_LM_STUDIO:
        set(customServerHostString, MUIA_String_Contents, "localhost");
        set(customServerPortString, MUIA_String_Integer, 1234);
        set(customServerUsesSSLCycle, MUIA_Cycle_Active, 0); /* No SSL */
        set(customServerChatModelString, MUIA_String_Contents, "");
        set(customServerApiEndpointCycle, MUIA_Cycle_Active,
            API_ENDPOINT_RESPONSES);
        set(customServerApiEndpointUrlString, MUIA_String_Contents, "v1");
        break;
    default:
        /* TEMPLATE_NONE - do nothing */
        break;
    }

    /* Reset template cycle back to "None" after applying */
    if (template != TEMPLATE_NONE) {
        set(customServerTemplateCycle, MUIA_Cycle_Active, TEMPLATE_NONE);
    }
}
MakeHook(TemplateSelectedHook, TemplateSelectedFunc);

HOOKPROTONHNONP(SettingsChangedFunc, void) {
    STRPTR customServerHost;
    get(customServerHostString, MUIA_String_Contents, &customServerHost);

    LONG port;
    get(customServerPortString, MUIA_String_Integer, &port);

    LONG usesSSL;
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);

    STRPTR customServerApiKey;
    get(customServerApiKeyString, MUIA_String_Contents, &customServerApiKey);

    LONG apiEndpoint;
    get(customServerApiEndpointCycle, MUIA_Cycle_Active, &apiEndpoint);

    STRPTR customServerApiEndpointUrl;
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &customServerApiEndpointUrl);

    UBYTE fullUrlPreviewString[512];
    snprintf(fullUrlPreviewString, 512, "%s://%s:%d%s%s/%s\0",
             usesSSL == 1 ? "https" : "http", customServerHost, port,
             strlen(customServerApiEndpointUrl) > 0 ? "/" : "",
             customServerApiEndpointUrl, API_ENDPOINT_NAMES[apiEndpoint]);
    set(customServerFullUrlPreviewString, MUIA_Text_Contents,
        fullUrlPreviewString);
}
MakeHook(SettingsChangedHook, SettingsChangedFunc);

HOOKPROTONHNONP(CustomServerSettingsRequesterOkButtonClickedFunc, void) {
    STRPTR customServerHost;
    get(customServerHostString, MUIA_String_Contents, &customServerHost);
    configSetCustomHost(customServerHost);

    LONG port;
    get(customServerPortString, MUIA_String_Integer, &port);
    if (port < 0 || port > 65535) {
        displayError(STRING_ERROR_INVALID_PORT);
        return;
    }
    configSetCustomPort(port);

    LONG usesSSL;
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    configSetCustomUseSSL(usesSSL == 1);

    STRPTR customServerApiKey;
    get(customServerApiKeyString, MUIA_String_Contents, &customServerApiKey);
    configSetCustomApiKey(customServerApiKey);

    STRPTR customServerChatModel;
    get(customServerChatModelString, MUIA_String_Contents,
        &customServerChatModel);
    configSetCustomChatModel(customServerChatModel);

    LONG apiEndpoint;
    get(customServerApiEndpointCycle, MUIA_Cycle_Active, &apiEndpoint);
    configSetCustomApiEndpoint(apiEndpoint);

    STRPTR customServerApiEndpointUrl;
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &customServerApiEndpointUrl);
    configSetCustomApiEndpointUrl(customServerApiEndpointUrl);

    set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(CustomServerSettingsRequesterOkButtonClickedHook,
         CustomServerSettingsRequesterOkButtonClickedFunc);

/**
 * Create the custom server settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createCustomServerSettingsRequesterWindow() {
    static STRPTR templateOptions[4] = {NULL};
    templateOptions[TEMPLATE_NONE] = STRING_MENU_TEMPLATE_NONE;
    templateOptions[TEMPLATE_GOOGLE_GEMINI] = STRING_MENU_TEMPLATE_GOOGLE_GEMINI;
    templateOptions[TEMPLATE_LM_STUDIO] = STRING_MENU_TEMPLATE_LM_STUDIO;

    static STRPTR sslOptions[3] = {NULL};
    sslOptions[0] = STRING_ENCRYPTION_NONE;
    sslOptions[1] = STRING_ENCRYPTION_SSL;

    static STRPTR apiEndpointOptions[3] = {NULL};
    apiEndpointOptions[0] = API_ENDPOINT_NAMES[API_ENDPOINT_RESPONSES];
    apiEndpointOptions[1] = API_ENDPOINT_NAMES[API_ENDPOINT_CHAT_COMPLETIONS];

    Object *customServerSettingsRequesterOkButton,
        *customServerSettingsRequesterCancelButton;
    if ((customServerSettingsRequesterWindowObject = WindowObject,
        MUIA_Window_Title, STRING_MENU_CUSTOM_SERVER_SETTINGS,
        MUIA_Window_Width, 400,
        MUIA_Window_Height, 200,
        MUIA_Window_CloseGadget, FALSE,
        WindowContents, VGroup,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_TEMPLATE,
                Child, customServerTemplateCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, templateOptions,
                    MUIA_Cycle_Active, TEMPLATE_NONE,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_PROXY_HOST,
                Child, customServerHostString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, configGetCustomHost(),
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_PROXY_PORT,
                Child, customServerPortString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Accept, "0123456789",
                    MUIA_String_MaxLen, 5,
                    MUIA_String_Integer, (LONG)configGetCustomPort(),
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_PROXY_ENCRYPTION,
                Child, customServerUsesSSLCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, sslOptions,
                    MUIA_Cycle_Active, configGetCustomUseSSL() ? 1 : 0,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_API_KEY,
                Child, customServerApiKeyString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, configGetCustomApiKey(),
                    MUIA_String_MaxLen, 256,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_CHAT_MODEL,
                Child, customServerChatModelString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, configGetCustomChatModel() != NULL ? configGetCustomChatModel() : "gemini-2.0-flash",
                End,
                Child, HGroup,
                    Child, customServerFetchModelsButton = MUI_MakeObject(
                        MUIO_Button, STRING_FETCH_MODELS, MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify, TAG_DONE),
                End,
                Child, NListviewObject,
                    MUIA_NListview_NList, customServerModelList = NListObject,
                        MUIA_NList_ConstructHook2, &ConstructCustomModelLI_TextHook,
                        MUIA_NList_DestructHook2, &DestructCustomModelLI_TextHook,
                        MUIA_NList_DisplayHook2, &DisplayCustomModelLI_TextHook,
                        MUIA_NList_Format, "",
                        MUIA_NList_Title, FALSE,
                        MUIA_NList_MinLineHeight, 16,
                    End,
                    MUIA_CycleChain, TRUE,
                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                    MUIA_FixHeight, 80,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_API_ENDPOINT,
                Child, customServerApiEndpointCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, apiEndpointOptions,
                    MUIA_Cycle_Active, configGetCustomApiEndpoint(),
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_API_ENDPOINT_URL,
                Child, customServerApiEndpointUrlString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, configGetCustomApiEndpointUrl() != NULL ? configGetCustomApiEndpointUrl() : "v1",
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_FULL_URL_PREVIEW,
                Child, customServerFullUrlPreviewString = TextObject,
                    MUIA_Frame, MUIV_Frame_String,
                End,
            End,
            Child, HGroup,
                Child, customServerSettingsRequesterOkButton = MUI_MakeObject(MUIO_Button, STRING_OK,
                    MUIA_Background, MUII_FILL,
                    MUIA_CycleChain, TRUE,
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                End,
                Child, customServerSettingsRequesterCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
                    MUIA_Background, MUII_FILL,
                    MUIA_CycleChain, TRUE,
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                End,
            End,
        End,
    End) == NULL) {
        displayError(STRING_ERROR_CUSTOM_SERVER_SETTINGS);
        return RETURN_ERROR;
    }
    DoMethod(customServerSettingsRequesterOkButton, MUIM_Notify, MUIA_Pressed,
             FALSE, MUIV_Notify_Window, 2, MUIM_CallHook,
             &CustomServerSettingsRequesterOkButtonClickedHook);
    DoMethod(customServerSettingsRequesterCancelButton, MUIM_Notify,
             MUIA_Pressed, FALSE, MUIV_Notify_Window, 3, MUIM_Set,
             MUIA_Window_Open, FALSE);
    DoMethod(customServerSettingsRequesterWindowObject, MUIM_Notify, MUIA_Window_Open, MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerTemplateCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &TemplateSelectedHook);
    DoMethod(customServerHostString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerPortString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerUsesSSLCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiKeyString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiEndpointCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiEndpointUrlString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerFetchModelsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 2, MUIM_CallHook, &FetchCustomModelsHook);
    DoMethod(customServerModelList, MUIM_Notify, MUIA_NList_Active,
             MUIV_EveryTime, MUIV_Notify_Application, 2, MUIM_CallHook,
             &CustomModelSelectedHook);
    return RETURN_OK;
}