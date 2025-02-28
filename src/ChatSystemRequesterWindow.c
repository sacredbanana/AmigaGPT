#include <exec/exec.h>
#include <libraries/mui.h>
#include <SDI_hook.h>
#include "ChatSystemRequesterWindow.h"
#include "config.h"
#include "gui.h"

Object *chatSystemRequesterString;
Object *chatSystemRequesterWindowObject;

HOOKPROTONHNONP(ChatSystemRequesterOkButtonClickedFunc, void) {
    STRPTR chatSystem;
    get(chatSystemRequesterString, MUIA_String_Contents, &chatSystem);
    if (config.chatSystem != NULL) {
        FreeVec(config.chatSystem);
        config.chatSystem = NULL;
    }
    config.chatSystem = AllocVec(strlen(chatSystem) + 1, MEMF_CLEAR);
    strncpy(config.chatSystem, chatSystem, strlen(chatSystem));
    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
}
MakeHook(ChatSystemRequesterOkButtonClickedHook,
         ChatSystemRequesterOkButtonClickedFunc);

/**
 * Create the about window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createChatSystemRequesterWindow() {
    Object *chatSystemRequesterOkButton, *chatSystemRequesterCancelButton;
    if ((chatSystemRequesterWindowObject = WindowObject,
            MUIA_Window_Title, STRING_CHAT_SYSTEM_WINDOW_TITLE,
            MUIA_Window_Width, 800,
            MUIA_Window_Height, 100,
            WindowContents, VGroup,
                Child, TextObject,
                    MUIA_Text_PreParse, "\33c",
                    MUIA_Text_Contents,  STRING_CHAT_SYSTEM_WINDOW_BODY,
                End,
                Child, chatSystemRequesterString = StringObject,
                    MUIA_String_MaxLen, CHAT_SYSTEM_LENGTH - 1,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, config.chatSystem,
                End,
                Child, HGroup,
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