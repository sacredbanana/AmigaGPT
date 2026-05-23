#ifdef __MORPHOS__

#include <Scintilla/Scintilla.h>
#include <libraries/charsets.h>
#include <libraries/clipboard.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <proto/clipboard.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include <string.h>
#include <SDI_hook.h>
#include "AmigaGPT_cat.h"
#include "CodeBlocksScintilla.h"
#include "CodeBlocksViewer.h"
#include "gui.h"

struct Library *ClipboardBase;

#define CODEBLOCK_LIST_LABEL_MAX 64

struct CodeBlockListItem {
    struct AICodeBlock *block;
    char label[CODEBLOCK_LIST_LABEL_MAX];
};

static Object *codeBlocksListObject;
static Object *codeBlocksScintillaObject;
static Object *codeBlocksCopyButtonObject;
/** Last block chosen in the list (survives focus loss on the NList). */
static struct AICodeBlock *codeBlocksCachedBlock;

static void codeBlocksListFormatLabel(struct CodeBlockListItem *item,
                                      struct AICodeBlock *block) {
    const char *lang;

    if (item == NULL || block == NULL) {
        return;
    }
    item->block = block;
    lang = (block->language != NULL && block->language[0] != '\0')
               ? (const char *)block->language
               : NULL;
    if (lang != NULL) {
        snprintf(item->label, sizeof(item->label), "%lu:%s",
                 (unsigned long)block->index, lang);
    } else {
        snprintf(item->label, sizeof(item->label), "%lu:-",
                 (unsigned long)block->index);
    }
}

static BOOL codeBlocksConversationHasBlocksImpl(struct Conversation *conv) {
    struct MinNode *mn;

    if (conv == NULL || conv->messages == NULL) {
        return FALSE;
    }

    for (mn = conv->messages->mlh_Head; mn->mln_Succ != NULL;
         mn = mn->mln_Succ) {
        struct ConversationNode *node = (struct ConversationNode *)mn;
        struct MinNode *bn;

        if (node->codeblocks == NULL) {
            continue;
        }
        for (bn = node->codeblocks->mlh_Head; bn->mln_Succ != NULL;
             bn = bn->mln_Succ) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL codeBlocksConversationHasBlocks(struct Conversation *conv) {
    return codeBlocksConversationHasBlocksImpl(conv);
}

HOOKPROTONHNO(ConstructCodeBlockListFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    return ncm->entry;
}
MakeHook(ConstructCodeBlockListHook, ConstructCodeBlockListFunc);

HOOKPROTONHNO(DestructCodeBlockListFunc, void,
              struct NList_DestructMessage *ndm) {
    if (ndm->entry != NULL) {
        FreeVec(ndm->entry);
    }
}
MakeHook(DestructCodeBlockListHook, DestructCodeBlockListFunc);

HOOKPROTONHNO(DisplayCodeBlockListFunc, void,
              struct NList_DisplayMessage *ndm) {
    struct CodeBlockListItem *item = (struct CodeBlockListItem *)ndm->entry;

    if (item != NULL) {
        ndm->strings[0] = item->label;
    }
}
MakeHook(DisplayCodeBlockListHook, DisplayCodeBlockListFunc);

void codeBlocksViewerSyncSelectionFromList(void) {
    struct CodeBlockListItem *item = NULL;

    if (codeBlocksListObject == NULL) {
        return;
    }
    DoMethod(codeBlocksListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &item);
    if (item != NULL && item->block != NULL) {
        codeBlocksCachedBlock = item->block;
    }
    codeBlocksViewerShowActiveBlock();
}

HOOKPROTONHNONP(CodeBlockListActiveFunc, void) {
    codeBlocksViewerSyncSelectionFromList();
}
MakeHook(CodeBlockListActiveHook, CodeBlockListActiveFunc);

HOOKPROTONHNONP(CodeBlockCopyButtonFunc, void) {
    if (!codeBlocksViewerCopyActiveBlockUtf8()) {
        displayError(STRING_ERROR_CLIPBOARD_COPY);
    }
}
MakeHook(CodeBlockCopyButtonHook, CodeBlockCopyButtonFunc);

void codeBlocksViewerSetObjects(Object *list, Object *scintilla) {
    codeBlocksListObject = list;
    codeBlocksScintillaObject = scintilla;
}

void codeBlocksViewerSetCopyButton(Object *button) {
    codeBlocksCopyButtonObject = button;
}

void codeBlocksViewerAttachListHooks(void) {
    if (codeBlocksListObject == NULL) {
        return;
    }
    DoMethod(codeBlocksListObject, MUIM_Notify, MUIA_NList_Active,
             MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook,
             &CodeBlockListActiveHook);
}

void codeBlocksViewerAttachCopyButton(void) {
    if (codeBlocksCopyButtonObject == NULL) {
        return;
    }
    DoMethod(codeBlocksCopyButtonObject, MUIM_Notify, MUIA_Pressed, FALSE,
             codeBlocksCopyButtonObject, 2, MUIM_CallHook,
             &CodeBlockCopyButtonHook);
}

void codeBlocksViewerClearList(void) {
    codeBlocksCachedBlock = NULL;
    if (codeBlocksListObject != NULL) {
        DoMethod(codeBlocksListObject, MUIM_NList_Clear);
    }
}

HOOKPROTONHNONP(CodeBlocksWindowClosedFunc, void) {
    codeBlocksViewerClearList();
}
MakeHook(CodeBlocksWindowClosedHook, CodeBlocksWindowClosedFunc);

void codeBlocksViewerDismiss(void) {
    codeBlocksViewerClearList();
    if (codeBlocksWindowObject != NULL) {
        set(codeBlocksWindowObject, MUIA_Window_Open, FALSE);
    }
}

BOOL codeBlocksViewerPopulate(struct Conversation *conv) {
    struct MinNode *mn;
    ULONG count = 0;

    codeBlocksCachedBlock = NULL;

    if (codeBlocksListObject == NULL || conv == NULL ||
        conv->messages == NULL) {
        return FALSE;
    }

    DoMethod(codeBlocksListObject, MUIM_NList_Clear);

    for (mn = conv->messages->mlh_Head; mn->mln_Succ != NULL;
         mn = mn->mln_Succ) {
        struct ConversationNode *node = (struct ConversationNode *)mn;
        struct MinNode *bn;

        if (node->codeblocks == NULL) {
            continue;
        }
        for (bn = node->codeblocks->mlh_Head; bn->mln_Succ != NULL;
             bn = bn->mln_Succ) {
            struct AICodeBlock *block = (struct AICodeBlock *)bn;
            struct CodeBlockListItem *item;

            item = AllocVec(sizeof(*item), MEMF_CLEAR);
            if (item == NULL) {
                DoMethod(codeBlocksListObject, MUIM_NList_Clear);
                return FALSE;
            }
            codeBlocksListFormatLabel(item, block);
            DoMethod(codeBlocksListObject, MUIM_NList_InsertSingle, item,
                     MUIV_NList_Insert_Bottom);
            count++;
        }
    }

    return count > 0;
}

static struct AICodeBlock *codeBlocksViewerGetSelectedBlock(void) {
    if (codeBlocksCachedBlock != NULL) {
        return codeBlocksCachedBlock;
    }

    if (codeBlocksListObject != NULL) {
        struct CodeBlockListItem *item = NULL;

        DoMethod(codeBlocksListObject, MUIM_NList_GetEntry,
                 MUIV_NList_GetEntry_Active, &item);
        if (item != NULL && item->block != NULL) {
            return item->block;
        }
    }
    return NULL;
}

void codeBlocksViewerShowActiveBlock(void) {
    struct AICodeBlock *block = codeBlocksViewerGetSelectedBlock();
    const char *text = "";

    if (codeBlocksScintillaObject == NULL) {
        return;
    }
    if (block != NULL && block->raw_code != NULL) {
        text = (const char *)block->raw_code;
    }
    codeBlocksScintillaSetUtf8Text(codeBlocksScintillaObject, text);
}

BOOL codeBlocksViewerCopyActiveBlockUtf8(void) {
    struct AICodeBlock *block = codeBlocksViewerGetSelectedBlock();
    ULONG nbytes;
    LONG err;

    if (block == NULL || block->raw_code == NULL) {
        return FALSE;
    }
    nbytes = block->code_length;
    if (nbytes == 0) {
        nbytes = (ULONG)strlen((const char *)block->raw_code);
    }
    if (nbytes == 0) {
        return FALSE;
    }
    if (ClipboardBase == NULL) {
        return FALSE;
    }

    /* clipboard.library: 0 = success (IFFERR_* on failure). */
    {
        struct TagItem tags[] = {
            {CLP_Charset, MIBENUM_UTF_8},
            {CLP_Size, nbytes},
            {TAG_DONE, 0},
        };

        err = WriteClipTextA(block->raw_code, tags);
    }
    return err == 0;
}

#endif /* __MORPHOS__ */
