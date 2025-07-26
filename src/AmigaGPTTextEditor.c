#include <libraries/mui.h>
#include <mui/BetterString_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <string.h>
#include "gui.h"
#include "AmigaGPTTextEditor.h"

struct MUI_CustomClass *amigaGPTTextEditorClass;

struct AmigaGPTTextEditorData {
    struct MUI_EventHandlerNode *eh;
    struct Hook *submitHook;
    BOOL isActive;
};

SAVEDS ULONG mGet(struct IClass *cl, Object *obj, struct opGet *msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);
    ULONG *store = ((struct opGet *)msg)->opg_Storage;

    switch (((struct opGet *)msg)->opg_AttrID) {
    case MUIA_AmigaGPTTextEditor_SubmitHook:
        *store = (ULONG)&data->submitHook;
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
    struct MUI_EventHandlerNode *eh = (struct MUI_EventHandlerNode *)AllocVec(
        sizeof(struct MUI_EventHandlerNode), MEMF_PUBLIC);
    eh->ehn_Class = cl;
    eh->ehn_Object = obj;
    eh->ehn_Events = IDCMP_RAWKEY;
    eh->ehn_Flags = MUI_EHF_GUIMODE;
    eh->ehn_Priority = 100;
    data->eh = eh;
    data->isActive = FALSE;

    if (!(DoSuperMethodA(cl, obj, msg)))
        return FALSE;

    return TRUE;
}

SAVEDS ULONG mCleanup(struct IClass *cl, Object *obj, Msg msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);

    FreeVec(data->eh);

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
                    STRPTR text;
                    if (isAROS) {
                        get(obj, MUIA_String_Contents, &text);
                    } else {
                        text = DoMethod(obj, MUIM_TextEditor_ExportText);
                    }
                    if (strlen(text) > 0) {
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
    data->eh = NULL;

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
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);

    if (!data->isActive) {
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, data->eh);
        data->isActive = TRUE;
    }

    return DoSuperMethodA(cl, obj, msg);
}

SAVEDS ULONG mGoInactive(struct IClass *cl, Object *obj,
                         struct MUIP_GoInactive *msg) {
    struct AmigaGPTTextEditorData *data = INST_DATA(cl, obj);

    if (data->isActive && _win(obj) != NULL) {
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, data->eh);
    }
    data->isActive = FALSE;
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
    if (!(amigaGPTTextEditorClass = MUI_CreateCustomClass(
              NULL, isAROS ? MUIC_BetterString : MUIC_TextEditor, NULL,
              sizeof(struct AmigaGPTTextEditorData), ENTRY(MyDispatcher))))
        return RETURN_ERROR;

    amigaGPTTextEditorClass->mcc_Class->cl_ID = (ClassID) "AmigaGPTTextEditor";
    return RETURN_OK;
}

/**
 * @brief Delete the AmigaGPTTextEditor class
 */
void deleteAmigaGPTTextEditor() {
    MUI_DeleteCustomClass(amigaGPTTextEditorClass);
}