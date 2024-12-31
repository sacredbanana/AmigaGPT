#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include <mui/TextEditor_mcc.h>
#include "APIKeyRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *apiKeyRequesterString;
Object *apiKeyRequesterWindowObject;

HOOKPROTONHNONP(APIKeyRequesterOkButtonClickedFunc, void) {
    STRPTR apiKey = DoMethod(apiKeyRequesterString, MUIM_TextEditor_ExportText);
    // Remove trailing newline characters
    for (ULONG i = 0; i < strlen(apiKey); i++) {
        if (apiKey[i] == '\n') {
            apiKey[i] = '\0';
        }
    }
    set(apiKeyRequesterString, MUIA_TextEditor_Contents, apiKey);
    if (config.openAiApiKey != NULL) {
        FreeVec(config.openAiApiKey);
        config.openAiApiKey = NULL;
    }
    config.openAiApiKey = AllocVec(strlen(apiKey) + 1, MEMF_CLEAR);
    strncpy(config.openAiApiKey, apiKey, strlen(apiKey));
    FreeVec(apiKey);
    writeConfig();
}
MakeHook(APIKeyRequesterOkButtonClickedHook,
         APIKeyRequesterOkButtonClickedFunc);

/**
 * Create the API key requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createAPIKeyRequesterWindow() {
    Object *apiKeyRequesterOkButton, *apiKeyRequesterCancelButton;
    if ((apiKeyRequesterWindowObject = WindowObject,
            MUIA_Window_Title, "Open AI API Key",
            MUIA_Window_Width, 800,
            MUIA_Window_Height, 200, 
            WindowContents, VGroup,
                Child, TextObject,
                    MUIA_Text_PreParse, "\33c",
                    MUIA_Text_Contents,  "Type or paste (Right-Amiga + V) your OpenAI API key here.\nThis key is required to use the OpenAI API.\nIf you do not have an API key, consult the AmigaGPT documentation for instructions on how to obtain one.",
                End,
                Child, apiKeyRequesterString = TextEditorObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_TextEditor_Contents, config.openAiApiKey,
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