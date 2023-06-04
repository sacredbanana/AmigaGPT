#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <gadgets/texteditor.h>
#include <proto/texteditor.h>
#include <utility/hooks.h>
#include <clib/alib_protos.h>
#include "gui.h"

extern struct IntuitionBase *IntuitionBase;
extern struct Library *TextFieldBase;

ULONG dispatchCustomTextEditorClass(Class *customTextEditorClass, Object *customTextEditorObject, Msg message);

Class *initCustomTextEditorClass() {
	AddClass(TEXTEDITOR_GetClass());
	Class *customTextEditorClass = MakeClass(
		NULL,
		"texteditor.gadget",
		TEXTEDITOR_GetClass(),
		0,
		0);
	if (customTextEditorClass == NULL) {
		printf("Failed to create CustomTextEditor class\n");
		return NULL;
	}
	customTextEditorClass->cl_Dispatcher.h_Entry = HookEntry;
	customTextEditorClass->cl_Dispatcher.h_SubEntry = (HOOKFUNC)dispatchCustomTextEditorClass;
	return customTextEditorClass;
}

BOOL freeCustomTextEditorClass(Class *customTextEditorClass) {
	if (customTextEditorClass == NULL) {
		return FALSE;
	}
	FreeClass(customTextEditorClass);
	return TRUE;
}

ULONG dispatchCustomTextEditorClass(Class *customTextEditorClass, Object *customTextEditorObject, Msg message) {
	struct IntuiMessage *intuiMessage = (struct IntuiMessage *)message;
	APTR returnValue;
	switch (message->MethodID) {
		case OM_NEW:
			printf("OM_NEW\n");
			if (returnValue = DoSuperMethodA(customTextEditorClass, customTextEditorObject, message)) {
				return returnValue;
			}
			break;
		// case GM_RENDER:
		//     printf("GM_RENDER\n");
		//     // displayError("GM_RENDER");
		//     returnValue = DoSuperMethodA(customTextEditorClass, customTextEditorObject, message);
		//     break;
		default:
			returnValue = DoSuperMethodA(customTextEditorClass, customTextEditorObject, message);
			break;
	}
	return returnValue;
}