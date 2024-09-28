#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include "APIKeyRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *apiKeyRequesterString;
Object *apiKeyRequesterWindowObject;

HOOKPROTONHNONP(APIKeyRequesterOkButtonClickedFunc, void) {
	STRPTR apiKey;
	get(apiKeyRequesterString, MUIA_String_Contents, &apiKey);
	if (config.openAiApiKey != NULL) {
		FreeVec(config.openAiApiKey);
		config.openAiApiKey = NULL;
	}
	config.openAiApiKey = AllocVec(strlen(apiKey) + 1, MEMF_CLEAR);
	strncpy(config.openAiApiKey, apiKey, strlen(apiKey));
	writeConfig();
}
MakeHook(APIKeyRequesterOkButtonClickedHook, APIKeyRequesterOkButtonClickedFunc);

LONG createAPIKeyRequesterWindow() {
    Object *apiKeyRequesterOkButton, *apiKeyRequesterCancelButton;
    if ((apiKeyRequesterWindowObject = WindowObject,
			MUIA_Window_Title, "Open AI API Key",
			MUIA_Window_Screen, screen,
			MUIA_Window_Width, 800,
			MUIA_Window_Height, 100,
			WindowContents, VGroup,
				Child, apiKeyRequesterString = StringObject,
					MUIA_String_MaxLen, OPENAI_API_KEY_LENGTH,
					MUIA_CycleChain, TRUE,
					MUIA_String_Contents, config.openAiApiKey,
				End,
				Child, HGroup,
					Child, apiKeyRequesterOkButton = MUI_MakeObject(MUIO_Button, "OK",
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
					Child, apiKeyRequesterCancelButton = MUI_MakeObject(MUIO_Button, "Cancel",
						MUIA_Background, MUII_FILL,
						MUIA_CycleChain, TRUE,
						MUIA_InputMode, MUIV_InputMode_RelVerify,
					End,
				End,
			End,
		End) == NULL) {
        displayError("Could not create API key requester window");
        return RETURN_ERROR;
    }
    DoMethod(apiKeyRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
			  apiKeyRequesterString, 2, MUIM_CallHook, &APIKeyRequesterOkButtonClickedHook);
	DoMethod(apiKeyRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  apiKeyRequesterWindowObject, 3, MUIM_Set, MUIA_Window_Open, FALSE);
	DoMethod(apiKeyRequesterCancelButton, MUIM_Notify, MUIA_Pressed, FALSE, 
			  apiKeyRequesterWindowObject, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    return RETURN_OK;
}