#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include <string.h>
#include "CustomServerSettingsRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *customServerHostString;
Object *customServerPortString;
Object *customServerUsesSSLCycle;
Object *customServerApiKeyString;
Object *customServerChatModelString;
Object *customServerApiEndpointCycle;
Object *customServerApiEndpointUrlString;
Object *customServerFullUrlPreviewString;
Object *customServerSettingsRequesterWindowObject;

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

    STRPTR fullUrlPreviewString[512];
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
    if (config.customHost != NULL) {
        FreeVec(config.customHost);
        config.customHost = NULL;
    }
    config.customHost = AllocVec(strlen(customServerHost) + 1, MEMF_CLEAR);
    strncpy(config.customHost, customServerHost, strlen(customServerHost));

    LONG port;
    get(customServerPortString, MUIA_String_Integer, &port);
    if (port < 0 || port > 65535) {
        displayError(STRING_ERROR_INVALID_PORT);
        return;
    }
    config.customPort = port;

    LONG usesSSL;
    get(customServerUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    config.customUseSSL = usesSSL == 1;

    STRPTR customServerApiKey;
    get(customServerApiKeyString, MUIA_String_Contents, &customServerApiKey);
    if (config.customApiKey != NULL) {
        FreeVec(config.customApiKey);
        config.customApiKey = NULL;
    }
    config.customApiKey = AllocVec(strlen(customServerApiKey) + 1, MEMF_CLEAR);
    strncpy(config.customApiKey, customServerApiKey,
            strlen(customServerApiKey));

    STRPTR customServerChatModel;
    get(customServerChatModelString, MUIA_String_Contents,
        &customServerChatModel);
    if (config.customChatModel != NULL) {
        FreeVec(config.customChatModel);
        config.customChatModel = NULL;
    }
    config.customChatModel =
        AllocVec(strlen(customServerChatModel) + 1, MEMF_CLEAR);
    strncpy(config.customChatModel, customServerChatModel,
            strlen(customServerChatModel));

    LONG apiEndpoint;
    get(customServerApiEndpointCycle, MUIA_Cycle_Active, &apiEndpoint);
    config.customApiEndpoint = apiEndpoint;

    STRPTR customServerApiEndpointUrl;
    get(customServerApiEndpointUrlString, MUIA_String_Contents,
        &customServerApiEndpointUrl);

    if (config.customApiEndpointUrl != NULL) {
        FreeVec(config.customApiEndpointUrl);
        config.customApiEndpointUrl = NULL;
    }
    config.customApiEndpointUrl =
        AllocVec(strlen(customServerApiEndpointUrl) + 1, MEMF_CLEAR);
    strncpy(config.customApiEndpointUrl, customServerApiEndpointUrl,
            strlen(customServerApiEndpointUrl));

    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
    set(customServerSettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(CustomServerSettingsRequesterOkButtonClickedHook,
         CustomServerSettingsRequesterOkButtonClickedFunc);

/**
 * Create the custom server settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createCustomServerSettingsRequesterWindow() {
    static STRPTR sslOptions[3] = {NULL};
    sslOptions[0] = STRING_ENCRYPTION_NONE;
    sslOptions[1] = STRING_ENCRYPTION_SSL;

    static STRPTR apiEndpointOptions[2] = {NULL};
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
                MUIA_FrameTitle, STRING_PROXY_HOST,
                Child, customServerHostString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.customHost,
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
                    MUIA_String_Integer, (LONG)config.customPort,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_PROXY_ENCRYPTION,
                Child, customServerUsesSSLCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, sslOptions,
                    MUIA_Cycle_Active, config.customUseSSL ? 1 : 0,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_API_KEY,
                Child, customServerApiKeyString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.customApiKey,
                    MUIA_String_MaxLen, 256,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_CHAT_MODEL,
                Child, customServerChatModelString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.customChatModel != NULL ? config.customChatModel : "gemini-2.0-flash",
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_API_ENDPOINT,
                Child, customServerApiEndpointCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, apiEndpointOptions,
                    MUIA_Cycle_Active, config.customApiEndpoint,
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_MENU_OPENAI_API_ENDPOINT_URL,
                Child, customServerApiEndpointUrlString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.customApiEndpointUrl != NULL ? config.customApiEndpointUrl : "v1",
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
    DoMethod(customServerHostString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerPortString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerUsesSSLCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiKeyString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiEndpointCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    DoMethod(customServerApiEndpointUrlString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &SettingsChangedHook);
    return RETURN_OK;
}