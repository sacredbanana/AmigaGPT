#include <exec/exec.h>
#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <string.h>
#include "VoiceInstructionsRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *voiceInstructionsRequesterString;
Object *voiceInstructionsRequesterWindowObject;

HOOKPROTONHNONP(VoiceInstructionsRequesterOkButtonClickedFunc, void) {
    STRPTR voiceInstructions;
    if (isAROS) {
        get(voiceInstructionsRequesterString, MUIA_String_Contents,
            &voiceInstructions);
    } else {
        voiceInstructions = DoMethod(voiceInstructionsRequesterString,
                                     MUIM_TextEditor_ExportText);
    }
    if (config.openAIVoiceInstructions != NULL) {
        FreeVec(config.openAIVoiceInstructions);
        config.openAIVoiceInstructions = NULL;
    }
    config.openAIVoiceInstructions =
        AllocVec(strlen(voiceInstructions) + 1, MEMF_CLEAR);
    strncpy(config.openAIVoiceInstructions, voiceInstructions,
            strlen(voiceInstructions));
    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
    if (!isAROS) {
        FreeVec(voiceInstructions);
    }
}
MakeHook(VoiceInstructionsRequesterOkButtonClickedHook,
         VoiceInstructionsRequesterOkButtonClickedFunc);

/**
 * Create the voice instructions requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createVoiceInstructionsRequesterWindow() {
    static STRPTR voiceInstructionsWindowBody;
    voiceInstructionsWindowBody = STRING_VOICE_INSTRUCTIONS_WINDOW_BODY;
    Object *floattextObject, *voiceInstructionsRequesterOkButton,
        *voiceInstructionsRequesterCancelButton;
    if ((voiceInstructionsRequesterWindowObject = WindowObject,
            MUIA_Window_Title, STRING_MENU_OPENAI_VOICE_INSTRUCTIONS,
            MUIA_Window_Width, 400,
            MUIA_Window_Height, MUIV_Window_Height_Default,
            MUIA_Window_CloseGadget, FALSE,
            WindowContents, VGroup,
                Child, floattextObject = NFloattextObject,
                    MUIA_NList_AdjustHeight, TRUE,
                    MUIA_ContextMenu, NULL,
                    MUIA_NFloattext_Text, voiceInstructionsWindowBody,
                    MUIA_NList_TypeSelect, MUIV_NList_TypeSelect_Line,
                End,
                Child,voiceInstructionsRequesterString = isAROS ? BetterStringObject,
                    MUIA_String_MaxLen, CHAT_SYSTEM_LENGTH - 1,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.openAIVoiceInstructions,
                End : TextEditorObject,
                    MUIA_TextEditor_Contents, config.openAIVoiceInstructions,
                    MUIA_TextEditor_ReadOnly, FALSE,
                    MUIA_TextEditor_TabSize, 4,
                    MUIA_TextEditor_Rows, 6,
                End,
                Child, HGroup,
                    MUIA_Group_SameWidth, TRUE,
                    Child, voiceInstructionsRequesterOkButton = MUI_MakeObject(MUIO_Button, STRING_OK,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                    Child, voiceInstructionsRequesterCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                End,
            End,
        End) == NULL) {
        displayError(STRING_VOICE_INSTRUCTIONS_WINDOW_ERROR);
        return RETURN_ERROR;
    }
    DoMethod(voiceInstructionsRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
              voiceInstructionsRequesterString, 2, MUIM_CallHook, &VoiceInstructionsRequesterOkButtonClickedHook);
    DoMethod(voiceInstructionsRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE, 
              voiceInstructionsRequesterWindowObject, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(voiceInstructionsRequesterCancelButton, MUIM_Notify, MUIA_Pressed, FALSE, 
              voiceInstructionsRequesterWindowObject, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    return RETURN_OK;
}