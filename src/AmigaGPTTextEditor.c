#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <string.h>
#include "gui.h"
#include "AmigaGPTTextEditor.h"

struct MUI_CustomClass *amigaGPTTextEditorClass;

struct AmigaGPTTextEditorData {
    struct MUI_EventHandlerNode eh;
    struct Hook *submitHook;
    BOOL eventHandlerAdded;
};

/**
 * @brief Add the event handler to the object's window if it is not already
 * added
 */
static void addEventHandler(struct IClass *cl, Object *obj) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);
    Object *window = _win(obj);

    if (data->eventHandlerAdded || window == NULL)
        return;

    DoMethod(window, MUIM_Window_AddEventHandler, &data->eh);
    data->eventHandlerAdded = TRUE;
}

/**
 * @brief Remove the event handler from the object's window if it is currently
 * added. The handler must never outlive the object, otherwise the window walks
 * a dangling node when it iterates its event handler queue.
 */
static void remEventHandler(struct IClass *cl, Object *obj) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);
    Object *window = _win(obj);

    if (!data->eventHandlerAdded)
        return;

    if (window != NULL)
        DoMethod(window, MUIM_Window_RemEventHandler, &data->eh);
    data->eventHandlerAdded = FALSE;
}

SAVEDS ULONG mGet(struct IClass *cl, Object *obj, struct opGet *msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);
    ULONG *store = ((struct opGet *)msg)->opg_Storage;

    switch (((struct opGet *)msg)->opg_AttrID) {
    case MUIA_AmigaGPTTextEditor_SubmitHook:
        *store = (ULONG)data->submitHook;
        return TRUE;
    }

    return DoSuperMethodA(cl, obj, msg);
}

SAVEDS ULONG mSet(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);
    struct TagItem *tags, *tag;

    for (tags = ((struct opSet *)msg)->ops_AttrList;
         (tag = NextTagItem(&tags));) {
        IPTR ti_Data = tag->ti_Data;
        switch (tag->ti_Tag) {
        case MUIA_AmigaGPTTextEditor_SubmitHook:
            data->submitHook = (struct Hook *)ti_Data;
            break;
        }
    }

    return DoSuperMethodA(cl, obj, msg);
}

SAVEDS ULONG mSetup(struct IClass *cl, Object *obj, Msg msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, msg)))
        return FALSE;

    data->eh.ehn_Class = cl;
    data->eh.ehn_Object = obj;
    data->eh.ehn_Events = IDCMP_RAWKEY;
    data->eh.ehn_Flags = MUI_EHF_GUIMODE;
    data->eh.ehn_Priority = 100;
    data->eventHandlerAdded = FALSE;

    return TRUE;
}

SAVEDS ULONG mCleanup(struct IClass *cl, Object *obj, Msg msg) {
    remEventHandler(cl, obj);

    return DoSuperMethodA(cl, obj, msg);
}

SAVEDS ULONG mHandleEvent(struct IClass *cl, Object *obj,
                          struct MUIP_HandleEvent *msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);
    ULONG rc = 0;

    if (msg->imsg) {
        switch (msg->imsg->Class) {
        case IDCMP_RAWKEY: {
            if (msg->imsg->Code == 0x44) {
                if (msg->imsg->Qualifier == 32768) {
                    STRPTR text = NULL;
                    BOOL freeText = FALSE;
                    if (isAROS) {
                        get(obj, MUIA_String_Contents, &text);
                    } else {
                        text =
                            (STRPTR)DoMethod(obj, MUIM_TextEditor_ExportText);
                        freeText = text != NULL;
                    }
                    BOOL hasText = text != NULL && strlen(text) > 0;
                    if (freeText)
                        FreeVec(text);
                    if (hasText) {
                        rc = MUI_EventHandlerRC_Eat;
                        if (data->submitHook != NULL) {
                            DoMethod(obj, MUIM_CallHook, data->submitHook, NULL,
                                     NULL);
                        }
                        return rc;
                    }
                } else if (msg->imsg->Qualifier & IEQUALIFIER_RSHIFT) {
                    msg->imsg->Qualifier = 32768;
                }
            }
        } break;
        }
    }

    rc = DoSuperMethodA(cl, obj, msg);

    return rc;
}

SAVEDS ULONG mNew(struct IClass *cl, Object *obj, Msg *msg) {
    if (!(obj = (Object *)DoSuperMethodA(cl, obj, msg)))
        return 0;

    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);

    data->submitHook = NULL;
    data->eventHandlerAdded = FALSE;

    struct TagItem *tags, *tag;
    for (tags = ((struct opSet *)msg)->ops_AttrList;
         (tag = NextTagItem(&tags));) {
        IPTR ti_Data = tag->ti_Data;
        switch (tag->ti_Tag) {
        case MUIA_AmigaGPTTextEditor_SubmitHook:
            data->submitHook = (struct Hook *)ti_Data;
            break;
        }
    }

    return obj;
}

SAVEDS ULONG mGoActive(struct IClass *cl, Object *obj,
                       struct MUIP_GoActive *msg) {
    addEventHandler(cl, obj);

    return DoSuperMethodA(cl, obj, msg);
}

SAVEDS ULONG mGoInactive(struct IClass *cl, Object *obj,
                         struct MUIP_GoInactive *msg) {
    remEventHandler(cl, obj);

    return DoSuperMethodA(cl, obj, msg);
}

DISPATCHER(MyDispatcher) {
    switch (msg->MethodID) {
    case OM_NEW:
        return (mNew(cl, obj, (APTR)msg));
    case OM_GET:
        return (mGet(cl, obj, (APTR)msg));
    case OM_SET:
        return (mSet(cl, obj, (APTR)msg));
    case MUIM_HandleEvent:
        return (mHandleEvent(cl, obj, (APTR)msg));
    case MUIM_Setup:
        return (mSetup(cl, obj, (APTR)msg));
    case MUIM_GoActive:
        return (mGoActive(cl, obj, (APTR)msg));
    case MUIM_GoInactive:
        return (mGoInactive(cl, obj, (APTR)msg));
    case MUIM_Cleanup:
        return (mCleanup(cl, obj, (APTR)msg));
    }

    return DoSuperMethodA(cl, obj, msg);
}

/**
 * @brief Create the AmigaGPTTextEditor class
 * @return RETURN_OK if successful, RETURN_ERROR otherwise
 */
LONG createAmigaGPTTextEditor() {
    if (amigaGPTTextEditorClass != NULL)
        return RETURN_OK;

    if (!(amigaGPTTextEditorClass = MUI_CreateCustomClass(
              NULL, isAROS ? MUIC_BetterString : MUIC_TextEditor, NULL,
              sizeof(struct AmigaGPTTextEditorData), ENTRY(MyDispatcher))))
        return RETURN_ERROR;

    /* Do not set mcc_Class->cl_ID here. This is a private class, and MUI 3.8
     * treats a class with an ID as a public one while disposing its objects,
     * which crashes when the app shuts down. */
    return RETURN_OK;
}

/**
 * @brief Delete the AmigaGPTTextEditor class
 */
void deleteAmigaGPTTextEditor() {
    if (amigaGPTTextEditorClass == NULL)
        return;

    MUI_DeleteCustomClass(amigaGPTTextEditorClass);
    amigaGPTTextEditorClass = NULL;
}