#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include <string.h>
#include "AmigaGPTConfig.h"
#include "ProxySettingsRequesterWindow.h"
#include "gui.h"

Object *proxyHostString;
Object *proxyPortString;
Object *proxyUsesSSLCycle;
Object *proxyRequiresAuthCycle;
Object *proxyUsernameString;
Object *proxyPasswordString;
Object *proxySettingsRequesterWindowObject;

HOOKPROTONHNONP(ProxySettingsRequesterOkButtonClickedFunc, void) {
    STRPTR proxyHost;
    get(proxyHostString, MUIA_String_Contents, &proxyHost);
    configSetProxyHost(proxyHost);

    LONG port;
    get(proxyPortString, MUIA_String_Integer, &port);
    if (port < 0 || port > 65535) {
        displayError(STRING_ERROR_INVALID_PORT);
        return;
    }
    configSetProxyPort(port);

    LONG usesSSL;
    get(proxyUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    configSetProxyUsesSSL(usesSSL == 1);

    LONG requiresAuth;
    get(proxyRequiresAuthCycle, MUIA_Cycle_Active, &requiresAuth);
    configSetProxyRequiresAuth(requiresAuth == 1);

    STRPTR proxyUsername;
    get(proxyUsernameString, MUIA_String_Contents, &proxyUsername);
    configSetProxyUsername(proxyUsername);

    STRPTR proxyPassword;
    get(proxyPasswordString, MUIA_String_Contents, &proxyPassword);
    configSetProxyPassword(proxyPassword);

    set(proxySettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(ProxySettingsRequesterOkButtonClickedHook,
         ProxySettingsRequesterOkButtonClickedFunc);

/**
 * Create the proxy settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createProxySettingsRequesterWindow() {
    static STRPTR sslOptions[3] = {NULL};
    sslOptions[0] = STRING_ENCRYPTION_NONE;
    sslOptions[1] = STRING_ENCRYPTION_SSL;

    static STRPTR authOptions[3] = {NULL};
    authOptions[0] = STRING_AUTH_NONE;
    authOptions[1] = STRING_AUTH_USER_PASS;

    Object *proxySettingsRequesterOkButton, *proxySettingsRequesterCancelButton;
    if ((proxySettingsRequesterWindowObject = WindowObject,
			MUIA_Window_Title, STRING_PROXY_SETTINGS,
			MUIA_Window_Width, 400,
			MUIA_Window_Height, 200,
            MUIA_Window_CloseGadget, FALSE,
			WindowContents, VGroup,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle, STRING_PROXY_HOST,
                    Child, proxyHostString = StringObject,
                        MUIA_Frame, MUIV_Frame_String,
                        MUIA_CycleChain, TRUE,
                        MUIA_String_Contents, configGetProxyHost() != NULL ? configGetProxyHost() : "",
                    End,
                End,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle, STRING_PROXY_PORT,
                    Child, proxyPortString = StringObject,
                        MUIA_Frame, MUIV_Frame_String,
                        MUIA_CycleChain, TRUE,
                        MUIA_String_Accept, "0123456789",
                        MUIA_String_MaxLen, 5,
                        MUIA_String_Integer, (LONG)configGetProxyPort(),
                    End,
                End,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle, STRING_PROXY_ENCRYPTION,
                    Child, proxyUsesSSLCycle = CycleObject,
                        MUIA_CycleChain, TRUE,
                        MUIA_Cycle_Entries, sslOptions,
                        MUIA_Cycle_Active, configGetProxyUsesSSL() ? 1 : 0,
                    End,
                End,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle, STRING_PROXY_AUTH,
                    Child, proxyRequiresAuthCycle = CycleObject,
                        MUIA_CycleChain, TRUE,
                        MUIA_Cycle_Entries, authOptions,
                        MUIA_Cycle_Active, configGetProxyRequiresAuth() ? 1 : 0,
                    End,
                    Child, HGroup,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_USERNAME,
                            Child, proxyUsernameString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, configGetProxyUsername() != NULL ? configGetProxyUsername() : "",
                                MUIA_Disabled, !configGetProxyRequiresAuth(),
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle, STRING_PASSWORD,
                            Child, proxyPasswordString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Secret, TRUE,
                                MUIA_String_Contents, configGetProxyPassword() != NULL ? configGetProxyPassword() : "",
                                MUIA_Disabled, !configGetProxyRequiresAuth(),
                            End,
                        End,
                    End,
                End,
				Child, HGroup,
					Child, proxySettingsRequesterOkButton = MUI_MakeObject(MUIO_Button, STRING_OK,
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
					Child, proxySettingsRequesterCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
				End,
			End,
		End) == NULL) {
        displayError(STRING_ERROR_PROXY_SETTINGS);
        return RETURN_ERROR;
    }
    DoMethod(proxySettingsRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  MUIV_Notify_Window, 2, MUIM_CallHook, &ProxySettingsRequesterOkButtonClickedHook);
	DoMethod(proxySettingsRequesterCancelButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  MUIV_Notify_Window, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(proxyRequiresAuthCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, proxyUsernameString, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
    DoMethod(proxyRequiresAuthCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, proxyPasswordString, 3, MUIM_Set, MUIA_Disabled, MUIV_NotTriggerValue);
    return RETURN_OK;
}