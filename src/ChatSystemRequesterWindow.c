#include <exec/exec.h>
#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <string.h>
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *chatSystemRequesterString;
Object *chatSystemRequesterWindowObject;

HOOKPROTONHNONP(ChatSystemRequesterOkButtonClickedFunc, void) {
    STRPTR chatSystem;
    if (isAROS) {
        get(chatSystemRequesterString, MUIA_String_Contents, &chatSystem);
    } else {
        chatSystem =
            DoMethod(chatSystemRequesterString, MUIM_TextEditor_ExportText);
    }
    if (config.chatSystem != NULL) {
        FreeVec(config.chatSystem);
        config.chatSystem = NULL;
    }
    config.chatSystem = AllocVec(strlen(chatSystem) + 1, MEMF_CLEAR);
    strncpy(config.chatSystem, chatSystem, strlen(chatSystem));
    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
    if (!isAROS) {
        FreeVec(chatSystem);
    }
}
MakeHook(ChatSystemRequesterOkButtonClickedHook,
         ChatSystemRequesterOkButtonClickedFunc);

/**
 * Create the chat system requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createChatSystemRequesterWindow() {
    static STRPTR chatSystemWindowBody;
    chatSystemWindowBody = STRING_CHAT_SYSTEM_WINDOW_BODY;
    Object *floattextObject, *chatSystemRequesterOkButton,
        *chatSystemRequesterCancelButton;
    if ((chatSystemRequesterWindowObject = WindowObject,
            MUIA_Window_Title, STRING_CHAT_SYSTEM_WINDOW_TITLE,
            MUIA_Window_Width, 400,
            MUIA_Window_Height, MUIV_Window_Height_Default,
            MUIA_Window_CloseGadget, FALSE,
            WindowContents, VGroup,
                Child, floattextObject = NFloattextObject,
                    MUIA_NList_AdjustHeight, TRUE,
                    MUIA_ContextMenu, NULL,
                    MUIA_NFloattext_Text, chatSystemWindowBody,
                    MUIA_NList_TypeSelect, MUIV_NList_TypeSelect_Line,
                End,
                Child,chatSystemRequesterString = isAROS ? BetterStringObject,
                    MUIA_String_MaxLen, CHAT_SYSTEM_LENGTH - 1,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.chatSystem,
                End : TextEditorObject,
                    MUIA_TextEditor_Contents, config.chatSystem,
                    MUIA_TextEditor_ReadOnly, FALSE,
                    MUIA_TextEditor_TabSize, 4,
                    MUIA_TextEditor_Rows, 6,
                End,
                Child, HGroup,
                    MUIA_Group_SameWidth, TRUE,
                    Child, chatSystemRequesterOkButton = MUI_MakeObject(MUIO_Button, STRING_OK,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                    Child, chatSystemRequesterCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
                        MUIA_Background, MUII_FILL,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    End,
                End,
            End,
        End) == NULL) {
        displayError(STRING_CHAT_SYSTEM_WINDOW_ERROR);
        return RETURN_ERROR;
    }
    DoMethod(chatSystemRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE,
              chatSystemRequesterString, 2, MUIM_CallHook, &ChatSystemRequesterOkButtonClickedHook);
    DoMethod(chatSystemRequesterOkButton, MUIM_Notify, MUIA_Pressed, FALSE, 
              chatSystemRequesterWindowObject, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(chatSystemRequesterCancelButton, MUIM_Notify, MUIA_Pressed, FALSE, 
              chatSystemRequesterWindowObject, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    return RETURN_OK;
}