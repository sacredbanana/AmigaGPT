#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include "ProxySettingsRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *proxyHostString;
Object *proxyPortString;
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
        displayError("Invalid port number. Please enter a number between 0 and 65535.");
        return;
    }
    config.proxyPort = port;
	writeConfig();
    set(proxySettingsRequesterWindowObject, MUIA_Window_Open, FALSE);
}
MakeHook(ProxySettingsRequesterOkButtonClickedHook, ProxySettingsRequesterOkButtonClickedFunc);

/**
 * Create the proxy settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createProxySettingsRequesterWindow() {
    Object *proxySettingsRequesterOkButton, *proxySettingsRequesterCancelButton;
    if ((proxySettingsRequesterWindowObject = WindowObject,
			MUIA_Window_Title, "Proxy server settings",
			MUIA_Window_Width, 400,
			MUIA_Window_Height, 200,
            MUIA_Window_CloseGadget, FALSE,
			WindowContents, VGroup,
                Child, TextObject,
                    MUIA_Text_PreParse, "\33c",
                    MUIA_Text_Contents,  "Host",
                End,
				Child, proxyHostString = StringObject,
					MUIA_CycleChain, TRUE,
					MUIA_String_Contents, config.proxyHost,
				End,
                Child, TextObject,
                    MUIA_Text_PreParse, "\33c",
                    MUIA_Text_Contents,  "Port",
                End,
                Child, proxyPortString = StringObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Accept, "0123456789",
                    MUIA_String_MaxLen, 5,
                    MUIA_String_Integer, (LONG)config.proxyPort,
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
    return RETURN_OK;
}