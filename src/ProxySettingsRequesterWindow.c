#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include "ProxySettingsRequesterWindow.h"
#include "config.h"
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
    if (config.proxyHost != NULL) {
        FreeVec(config.proxyHost);
        config.proxyHost = NULL;
    }
    config.proxyHost = AllocVec(strlen(proxyHost) + 1, MEMF_CLEAR);
    strncpy(config.proxyHost, proxyHost, strlen(proxyHost));

    LONG port;
    get(proxyPortString, MUIA_String_Integer, &port);
    if (port < 0 || port > 65535) {
        displayError(
            "Invalid port number. Please enter a number between 0 and 65535.");
        return;
    }
    config.proxyPort = port;

    LONG usesSSL;
    get(proxyUsesSSLCycle, MUIA_Cycle_Active, &usesSSL);
    config.proxyUsesSSL = usesSSL == 1;

    LONG requiresAuth;
    get(proxyRequiresAuthCycle, MUIA_Cycle_Active, &requiresAuth);
    config.proxyRequiresAuth = requiresAuth == 1;

    STRPTR proxyUsername;
    get(proxyUsernameString, MUIA_String_Contents, &proxyUsername);
    if (config.proxyUsername != NULL) {
        FreeVec(config.proxyUsername);
        config.proxyUsername = NULL;
    }
    config.proxyUsername = AllocVec(strlen(proxyUsername) + 1, MEMF_CLEAR);
    strncpy(config.proxyUsername, proxyUsername, strlen(proxyUsername));

    STRPTR proxyPassword;
    get(proxyPasswordString, MUIA_String_Contents, &proxyPassword);
    if (config.proxyPassword != NULL) {
        FreeVec(config.proxyPassword);
        config.proxyPassword = NULL;
    }
    config.proxyPassword = AllocVec(strlen(proxyPassword) + 1, MEMF_CLEAR);
    strncpy(config.proxyPassword, proxyPassword, strlen(proxyPassword));

    writeConfig();
    set(proxySettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(ProxySettingsRequesterOkButtonClickedHook,
         ProxySettingsRequesterOkButtonClickedFunc);

/**
 * Create the proxy settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createProxySettingsRequesterWindow() {
    static CONST_STRPTR SSL_OPTIONS[] = {"None (fastest)", "SSL (secure)",
                                         NULL};

    static CONST_STRPTR AUTH_OPTIONS[] = {"No authentication",
                                          "Username and password", NULL};

    Object *proxySettingsRequesterOkButton, *proxySettingsRequesterCancelButton;
    if ((proxySettingsRequesterWindowObject = WindowObject,
			MUIA_Window_Title, "Proxy server settings",
			MUIA_Window_Width, 400,
			MUIA_Window_Height, 200,
            MUIA_Window_CloseGadget, FALSE,
			WindowContents, VGroup,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle,  "Host",
                    Child, proxyHostString = StringObject,
                        MUIA_Frame, MUIV_Frame_String,
                        MUIA_CycleChain, TRUE,
                        MUIA_String_Contents, config.proxyHost,
                    End,
                End,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle,  "Port",
                    Child, proxyPortString = StringObject,
                        MUIA_Frame, MUIV_Frame_String,
                        MUIA_CycleChain, TRUE,
                        MUIA_String_Accept, "0123456789",
                        MUIA_String_MaxLen, 5,
                        MUIA_String_Integer, (LONG)config.proxyPort,
                    End,
                End,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle,  "Encryption",
                    Child, proxyUsesSSLCycle = CycleObject,
                        MUIA_CycleChain, TRUE,
                        MUIA_Cycle_Entries, SSL_OPTIONS,
                        MUIA_Cycle_Active, config.proxyUsesSSL ? 1 : 0,
                    End,
                End,
                Child, VGroup,
                    MUIA_Frame, MUIV_Frame_Group,
                    MUIA_FrameTitle,  "Authentication",
                    Child, proxyRequiresAuthCycle = CycleObject,
                        MUIA_CycleChain, TRUE,
                        MUIA_Cycle_Entries, AUTH_OPTIONS,
                        MUIA_Cycle_Active, config.proxyRequiresAuth ? 1 : 0,
                    End,
                    Child, HGroup,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle,  "Username",
                            Child, proxyUsernameString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Contents, config.proxyUsername,
                                MUIA_Disabled, !config.proxyRequiresAuth,
                            End,
                        End,
                        Child, VGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_FrameTitle,  "Password",
                            Child, proxyPasswordString = StringObject,
                                MUIA_Frame, MUIV_Frame_String,
                                MUIA_CycleChain, TRUE,
                                MUIA_String_Secret, TRUE,
                                MUIA_String_Contents, config.proxyPassword,
                                MUIA_Disabled, !config.proxyRequiresAuth,
                            End,
                        End,
                    End,
                End,
				Child, HGroup,
					Child, proxySettingsRequesterOkButton = MUI_MakeObject(MUIO_Button, "OK",
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
					Child, proxySettingsRequesterCancelButton = MUI_MakeObject(MUIO_Button, "Cancel",
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
				End,
			End,
		End) == NULL) {
        displayError("Could not create proxy settings requester window");
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