#ifdef __MORPHOS__

#include <Scintilla/Scintilla.h>
#include <libraries/charsets.h>
#include <libraries/clipboard.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <dos/dos.h>
#include <proto/asl.h>
#include <proto/clipboard.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <SDI_hook.h>
#include "AmigaGPT_cat.h"
#include "CodeBlocksScintilla.h"
#include "CodeBlocksViewer.h"
#include "gui.h"
#include "MainWindow.h"

struct Library *ClipboardBase;

#define CODEBLOCK_LIST_LABEL_MAX 64
#define CODEBLOCK_DEFAULT_NAME_MAX 64

struct CodeBlockListItem {
    struct AICodeBlock *block;
    char label[CODEBLOCK_LIST_LABEL_MAX];
};

typedef struct {
    STRPTR data;
    ULONG length;
    BOOL mustFree;
} CodeBlockPayload;

static Object *codeBlocksListObject;
static Object *codeBlocksScintillaObject;
static Object *codeBlocksCopyUtf8ButtonObject;
static Object *codeBlocksCopySystemButtonObject;
static Object *codeBlocksSaveUtf8ButtonObject;
static Object *codeBlocksSaveSystemButtonObject;
/** Last block chosen in the list (survives focus loss on the NList). */
static struct AICodeBlock *codeBlocksCachedBlock;
/** Set by save button hooks; read in deferred save hook after PushMethod. */
static BOOL codeBlocksPendingSaveSystemCharset;
/** Block to save (snapshot at button press; list may lose active on defer). */
static struct AICodeBlock *codeBlocksPendingSaveBlock;
/** Intuition window for ASL parent (main window, not the code-blocks subwindow). */
static struct Window *codeBlocksAslParentWindow;

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

static const char *codeBlocksLanguageExtension(const char *language) {
    static const struct {
        const char *lang;
        const char *ext;
    } map[] = {
        {"python", ".py"},     {"javascript", ".js"}, {"js", ".js"},
        {"typescript", ".ts"}, {"ts", ".ts"},         {"c", ".c"},
        {"cpp", ".cpp"},       {"c++", ".cpp"},       {"csharp", ".cs"},
        {"cs", ".cs"},         {"java", ".java"},     {"go", ".go"},
        {"rust", ".rs"},       {"ruby", ".rb"},       {"php", ".php"},
        {"shell", ".sh"},      {"bash", ".sh"},       {"sh", ".sh"},
        {"html", ".html"},     {"css", ".css"},       {"json", ".json"},
        {"xml", ".xml"},       {"yaml", ".yaml"},     {"yml", ".yaml"},
        {"markdown", ".md"},   {"md", ".md"},         {"sql", ".sql"},
        {"kotlin", ".kt"},     {"swift", ".swift"},   {"lua", ".lua"},
        {"perl", ".pl"},       {"r", ".r"},           {"text", ".txt"},
    };
    ULONG i;

    if (language == NULL || language[0] == '\0') {
        return ".txt";
    }
    for (i = 0; i < sizeof(map) / sizeof(map[0]); i++) {
        if (strcasecmp(language, map[i].lang) == 0) {
            return map[i].ext;
        }
    }
    return ".txt";
}

void codeBlocksViewerDefaultFileName(struct AICodeBlock *block, STRPTR buf,
                                     ULONG buflen) {
    const char *ext = ".txt";

    if (buf == NULL || buflen == 0) {
        return;
    }
    buf[0] = '\0';
    if (block == NULL) {
        return;
    }
    if (block->language != NULL && block->language[0] != '\0') {
        ext = codeBlocksLanguageExtension((const char *)block->language);
    }
    snprintf(buf, buflen, "block-%lu%s", (unsigned long)block->index, ext);
}

static BOOL codeBlocksPayloadFromBlock(struct AICodeBlock *block,
                                       BOOL systemCharset,
                                       CodeBlockPayload *payload) {
    ULONG rawLen;

    if (payload == NULL) {
        return FALSE;
    }
    payload->data = NULL;
    payload->length = 0;
    payload->mustFree = FALSE;

    if (block == NULL || block->raw_code == NULL) {
        return FALSE;
    }
    rawLen = block->code_length;
    if (rawLen == 0) {
        rawLen = (ULONG)strlen((const char *)block->raw_code);
    }
    if (rawLen == 0) {
        return FALSE;
    }

    if (!systemCharset) {
        payload->data = block->raw_code;
        payload->length = rawLen;
        return TRUE;
    }

    if (systemCodeset == NULL) {
        return FALSE;
    }

    payload->data = CodesetsUTF8ToStr(
        CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)block->raw_code,
        CSA_MapForeignChars, TRUE, TAG_DONE);
    if (payload->data == NULL) {
        return FALSE;
    }
    payload->length = (ULONG)strlen(payload->data);
    payload->mustFree = TRUE;
    return payload->length > 0;
}

static void codeBlocksPayloadFree(CodeBlockPayload *payload) {
    if (payload != NULL && payload->mustFree && payload->data != NULL) {
        CodesetsFreeA(payload->data, NULL);
        payload->data = NULL;
        payload->length = 0;
        payload->mustFree = FALSE;
    }
}

static struct Window *codeBlocksGetAslParentWindow(void) {
    if (codeBlocksAslParentWindow != NULL) {
        return codeBlocksAslParentWindow;
    }
    if (mainWindowObject != NULL) {
        struct Window *win = NULL;

        get(mainWindowObject, MUIA_Window, &win);
        return win;
    }
    return NULL;
}

static BOOL codeBlocksWritePayloadToPath(const CodeBlockPayload *payload,
                                         STRPTR path) {
    BPTR file;
    LONG written;

    if (payload == NULL || path == NULL || payload->data == NULL ||
        payload->length == 0) {
        return FALSE;
    }

    file = Open(path, MODE_NEWFILE);
    if (file == (BPTR)0) {
        return FALSE;
    }
    written = Write(file, (CONST_STRPTR)payload->data, payload->length);
    Close(file);
    return written == (LONG)payload->length;
}

static BOOL codeBlocksCopyPayload(const CodeBlockPayload *payload,
                                  BOOL utf8Charset) {
    LONG err;
    ULONG charsetTag;

    if (payload == NULL || payload->data == NULL || payload->length == 0) {
        return FALSE;
    }
    if (ClipboardBase == NULL) {
        return FALSE;
    }

    if (utf8Charset) {
        charsetTag = MIBENUM_UTF_8;
    } else {
        if (systemCodeset == NULL) {
            return FALSE;
        }
        charsetTag = (ULONG)systemCodeset->MIBenum;
    }

    /* clipboard.library: 0 = success (IFFERR_* on failure). */
    {
        struct TagItem tags[] = {
            {CLP_Charset, charsetTag},
            {CLP_Size, payload->length},
            {TAG_DONE, 0},
        };

        err = WriteClipTextA(payload->data, tags);
    }
    return err == 0;
}

static BOOL codeBlocksViewerCopyActiveBlock(BOOL systemCharset) {
    struct AICodeBlock *block = codeBlocksViewerGetSelectedBlock();
    CodeBlockPayload payload;
    BOOL ok;

    if (!codeBlocksPayloadFromBlock(block, systemCharset, &payload)) {
        return FALSE;
    }
    ok = codeBlocksCopyPayload(&payload, !systemCharset);
    codeBlocksPayloadFree(&payload);
    return ok;
}

BOOL codeBlocksViewerCopyActiveBlockUtf8(void) {
    return codeBlocksViewerCopyActiveBlock(FALSE);
}

BOOL codeBlocksViewerCopyActiveBlockSystem(void) {
    return codeBlocksViewerCopyActiveBlock(TRUE);
}

BOOL codeBlocksViewerSaveBlock(struct AICodeBlock *block, BOOL systemCharset) {
    CodeBlockPayload payload;
    BOOL ok = FALSE;
    BOOL cancelled = FALSE;
    UBYTE defaultName[CODEBLOCK_DEFAULT_NAME_MAX];
    struct FileRequester *fileReq;
    struct Window *aslParent;

    if (block == NULL) {
        return FALSE;
    }

    codeBlocksViewerDefaultFileName(block, defaultName, sizeof(defaultName));
    aslParent = codeBlocksGetAslParentWindow();
    if (aslParent == NULL) {
        return FALSE;
    }

    /*
     * ASL on mainWindow (Intuition), not the code-blocks MUI subwindow.
     * Payload is built only after the user confirms a path.
     */
    fileReq = (struct FileRequester *)MUI_AllocAslRequestTags(
        ASL_FileRequest, ASLFR_Window, aslParent, ASLFR_PopToFront, TRUE,
        ASLFR_Activate, TRUE, ASLFR_TitleText, STRING_SAVE_CODEBLOCK,
        ASLFR_InitialFile, defaultName, ASLFR_InitialDrawer, "SYS:",
        ASLFR_DoSaveMode, TRUE, TAG_DONE);
    if (fileReq == NULL) {
        return FALSE;
    }

    if (MUI_AslRequestTags(fileReq, TAG_DONE)) {
        STRPTR savePath = fileReq->fr_Drawer;
        STRPTR saveName = fileReq->fr_File;
        UWORD fullPathLength = (UWORD)(strlen(savePath) + strlen(saveName) + 2);
        STRPTR fullPath = AllocVec(fullPathLength, MEMF_CLEAR);

        if (fullPath != NULL) {
            snprintf(fullPath, fullPathLength, "%s", savePath);
            AddPart(fullPath, saveName, fullPathLength);
            if (codeBlocksPayloadFromBlock(block, systemCharset, &payload)) {
                ok = codeBlocksWritePayloadToPath(&payload, fullPath);
                codeBlocksPayloadFree(&payload);
            }
            FreeVec(fullPath);
        }
    } else {
        cancelled = TRUE;
        ok = TRUE;
    }
    FreeAslRequest(fileReq);
    return cancelled ? TRUE : ok;
}

BOOL codeBlocksViewerSaveActiveBlock(BOOL systemCharset) {
    return codeBlocksViewerSaveBlock(codeBlocksViewerGetSelectedBlock(),
                                     systemCharset);
}

HOOKPROTONHNONP(CodeBlockCopyUtf8ButtonFunc, void) {
    if (!codeBlocksViewerCopyActiveBlockUtf8()) {
        displayError(STRING_ERROR_CLIPBOARD_COPY);
    }
}
MakeHook(CodeBlockCopyUtf8ButtonHook, CodeBlockCopyUtf8ButtonFunc);

HOOKPROTONHNONP(CodeBlockCopySystemButtonFunc, void) {
    if (!codeBlocksViewerCopyActiveBlockSystem()) {
        displayError(STRING_ERROR_CLIPBOARD_COPY);
    }
}
MakeHook(CodeBlockCopySystemButtonHook, CodeBlockCopySystemButtonFunc);

HOOKPROTONHNONP(CodeBlockSaveDeferredFunc, void) {
    if (codeBlocksPendingSaveBlock == NULL) {
        displayError(STRING_ERROR_NO_ACTIVE_CODEBLOCK);
        return;
    }
    if (!codeBlocksViewerSaveBlock(codeBlocksPendingSaveBlock,
                                   codeBlocksPendingSaveSystemCharset)) {
        displayError(STRING_ERROR_CODEBLOCK_SAVE);
    }
}
MakeHook(CodeBlockSaveDeferredHook, CodeBlockSaveDeferredFunc);

HOOKPROTONHNONP(CodeBlockSaveUtf8ButtonFunc, void) {
    codeBlocksViewerSyncSelectionFromList();
    codeBlocksPendingSaveBlock = codeBlocksViewerGetSelectedBlock();
    codeBlocksPendingSaveSystemCharset = FALSE;
    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &CodeBlockSaveDeferredHook);
}
MakeHook(CodeBlockSaveUtf8ButtonHook, CodeBlockSaveUtf8ButtonFunc);

HOOKPROTONHNONP(CodeBlockSaveSystemButtonFunc, void) {
    codeBlocksViewerSyncSelectionFromList();
    codeBlocksPendingSaveBlock = codeBlocksViewerGetSelectedBlock();
    codeBlocksPendingSaveSystemCharset = TRUE;
    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &CodeBlockSaveDeferredHook);
}
MakeHook(CodeBlockSaveSystemButtonHook, CodeBlockSaveSystemButtonFunc);

void codeBlocksViewerSetAslParentWindow(struct Window *parent) {
    codeBlocksAslParentWindow = parent;
}

void codeBlocksViewerSetObjects(Object *list, Object *scintilla) {
    codeBlocksListObject = list;
    codeBlocksScintillaObject = scintilla;
}

void codeBlocksViewerSetActionButtons(Object *copyUtf8, Object *copySystem,
                                      Object *saveUtf8, Object *saveSystem) {
    codeBlocksCopyUtf8ButtonObject = copyUtf8;
    codeBlocksCopySystemButtonObject = copySystem;
    codeBlocksSaveUtf8ButtonObject = saveUtf8;
    codeBlocksSaveSystemButtonObject = saveSystem;
}

void codeBlocksViewerAttachListHooks(void) {
    if (codeBlocksListObject == NULL) {
        return;
    }
    DoMethod(codeBlocksListObject, MUIM_Notify, MUIA_NList_Active,
             MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook,
             &CodeBlockListActiveHook);
}

void codeBlocksViewerAttachActionButtons(void) {
    if (codeBlocksCopyUtf8ButtonObject != NULL) {
        DoMethod(codeBlocksCopyUtf8ButtonObject, MUIM_Notify, MUIA_Pressed,
                 FALSE, codeBlocksCopyUtf8ButtonObject, 2, MUIM_CallHook,
                 &CodeBlockCopyUtf8ButtonHook);
    }
    if (codeBlocksCopySystemButtonObject != NULL) {
        DoMethod(codeBlocksCopySystemButtonObject, MUIM_Notify, MUIA_Pressed,
                 FALSE, codeBlocksCopySystemButtonObject, 2, MUIM_CallHook,
                 &CodeBlockCopySystemButtonHook);
    }
    if (codeBlocksSaveUtf8ButtonObject != NULL) {
        DoMethod(codeBlocksSaveUtf8ButtonObject, MUIM_Notify, MUIA_Pressed,
                 FALSE, codeBlocksSaveUtf8ButtonObject, 2, MUIM_CallHook,
                 &CodeBlockSaveUtf8ButtonHook);
    }
    if (codeBlocksSaveSystemButtonObject != NULL) {
        DoMethod(codeBlocksSaveSystemButtonObject, MUIM_Notify, MUIA_Pressed,
                 FALSE, codeBlocksSaveSystemButtonObject, 2, MUIM_CallHook,
                 &CodeBlockSaveSystemButtonHook);
    }
}

void codeBlocksViewerClearList(void) {
    codeBlocksCachedBlock = NULL;
    codeBlocksPendingSaveBlock = NULL;
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

void codeBlocksViewerPrepareShutdown(void) {
    /*
     * Conversation AICodeBlocks may be freed when the main NList is destroyed
     * during MUI_DisposeObject(app). Drop cached pointers and empty Scintilla
     * before that so nothing dereferences conversation-owned raw_code.
     */
    codeBlocksCachedBlock = NULL;
    codeBlocksPendingSaveBlock = NULL;

    if (codeBlocksScintillaObject != NULL) {
        codeBlocksScintillaSetUtf8Text(codeBlocksScintillaObject, "");
    }
    if (codeBlocksListObject != NULL) {
        DoMethod(codeBlocksListObject, MUIM_NList_Clear);
    }
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

#endif /* __MORPHOS__ */
