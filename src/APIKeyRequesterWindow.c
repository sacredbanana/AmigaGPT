#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include <mui/TextEditor_mcc.h>
#include <string.h>
#include "APIKeyRequesterWindow.h"
#include "config.h"
#include "gui.h"
#include "MainWindow.h"

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
    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
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
            MUIA_Window_Title, STRING_API_KEY_REQUESTER_TITLE,
            MUIA_Window_Width, 800,
            MUIA_Window_Height, 200,
            MUIA_Window_CloseGadget, FALSE,
            WindowContents, VGroup,
                Child, TextObject,
                    MUIA_Text_PreParse, "\033c",
                    MUIA_Text_Contents,  STRING_API_KEY_REQUESTER_BODY_INSTRUCTION,
                End,
                Child, TextObject,
                    MUIA_Text_PreParse, "\033c",
                    MUIA_Text_Contents,  STRING_API_KEY_REQUESTER_BODY_REASON,
                End,
                Child, TextObject,
                    MUIA_Text_PreParse, "\033c",
                    MUIA_Text_Contents,  STRING_API_KEY_REQUESTER_BODY_HELP,
                End,
                Child, apiKeyRequesterString = TextEditorObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_TextEditor_Contents, config.openAiApiKey,
                End,
                Child, HGroup,
                    Child, apiKeyRequesterOkButton = MUI_MakeObject(MUIO_Button, STRING_OK,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                    Child, apiKeyRequesterCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                End,
            End,
        End) == NULL) {
        displayError(STRING_ERROR_API_KEY_REQUESTER);
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