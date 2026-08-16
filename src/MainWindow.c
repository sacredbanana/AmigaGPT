#include <devices/inputevent.h>
#include <devices/printer.h>
#include <devices/prtbase.h>
#include <devices/trackdisk.h>
#include <libraries/mui.h>
#include <json-c/json.h>
#include <mui/BetterString_mcc.h>
#include <mui/Busy_mcc.h>
#include <mui/Guigfx_mcc.h>
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "AmigaGPTConfig.h"
#include "AmigaGPTTextEditor.h"
#include "streamlog.h"
#include "gui.h"
#include "menu.h"
#include "MainWindow.h"
#include "openai.h"
#include "speech.h"
#ifdef __MORPHOS__
#include <mui/Scintilla_mcc.h>
#include "ChatFindScintilla.h"
#include "ChatOutputScintilla.h"
#include "CodeBlocksViewer.h"
#endif
#include "chatmd_markers.h"
#include "utf8stream.h"

#ifndef MAIN_WINDOW_SAVED_MIN_WIDTH
#define MAIN_WINDOW_SAVED_MIN_WIDTH 320
#define MAIN_WINDOW_SAVED_MIN_HEIGHT 200
#endif

static STRPTR dupStringAlloc(CONST_STRPTR src) {
    STRPTR copy;
    ULONG n;

    if (src == NULL) {
        return NULL;
    }
    n = (ULONG)strlen(src) + 1;
    copy = AllocVec(n, MEMF_ANY | MEMF_CLEAR);
    if (copy != NULL) {
        strncpy(copy, src, n - 1);
        copy[n - 1] = '\0';
    }
    return copy;
}
#include <dos/dos.h>

/* Max nesting depth for B/I/U combined. Adjust as needed. */
#define MAX_STYLE_STACK 32

#define CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH (1024 * 512)
#define CHAT_OUTPUT_WIDGET_SAFE_LIMIT (60 * 1024)
#define CONVERSATION_TITLE_FALLBACK_MAX 96
/** UI refresh during stream: at most ~5 updates per second (R3). */
#define STREAM_UI_MIN_REFRESH_MS 200

static struct timeval streamUiLastRefresh;
static BOOL streamUiLastRefreshValid;

#ifdef __MORPHOS__
/**
 * TRUE while `sendChatMessage` is streaming assistant deltas into
 * `chatOutputTextEditorContents`. Chat Scintilla then refreshes **raw UTF-8** only
 * (no `chatOutputScintillaBuildMidiMarkdownDisplay` / link spans) — Handlungsanweisung
 * §5 / R3: kein Markdown-Parse pro Chunk. Cleared in `finishChatStream` and on early exits.
 */
static BOOL morphosChatStreamRawScintillaRefresh = FALSE;
#endif

typedef enum { STYLE_BOLD, STYLE_ITALIC, STYLE_UNDERLINE } StyleType;

/** Frees the pointer array only; each json_object must already be json_object_put. */
static void freeOpenAIResponseArray(struct json_object **responses) {
    if (responses != NULL) {
        FreeVec(responses);
    }
}

/** Release from index onward (error path); caller may have put [0..fromIndex-1]. */
static void discardOpenAIResponseArray(struct json_object **responses,
                                       UWORD fromIndex) {
    UWORD i;

    if (responses == NULL) {
        return;
    }
    for (i = fromIndex; i < RESPONSE_ARRAY_BUFFER_LENGTH && responses[i] != NULL;
         i++) {
        json_object_put(responses[i]);
    }
    FreeVec(responses);
}

/* A small stack to track active styles in the order they were opened. */
typedef struct {
    StyleType stack[MAX_STYLE_STACK];
    int top;
} StyleStack;

struct Window *mainWindow;
Object *mainWindowObject = NULL;
Object *newChatButton;
Object *deleteChatButton;
Object *sendMessageButton;
Object *stopSpeakingButton;
Object *chatInputTextEditor;
#ifndef __MORPHOS__
Object *chatOutputListView;
#endif
#ifdef __MORPHOS__
Object *chatOutputScroller;
#endif
Object *chatOutputTextEditor;
Object *statusBar;
Object *conversationListObject;
Object *loadingBar;
Object *loadingBarGroup;
Object *imageInputTextEditor;
Object *createImageButton;
Object *newImageButton;
Object *deleteImageButton;
Object *imageListObject;
Object *imageView;
Object *imageViewGroup;
Object *openImageButton;
Object *saveImageCopyButton;
Object *modeRegisterGroup;
STRPTR chatOutputTextEditorContents = NULL;
static ULONG chatOutputTextEditorContentsCapacity =
    CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH;
WORD pens[NUMDRIPENS + 1];
struct Conversation *currentConversation = NULL;
struct GeneratedImage *currentImage = NULL;
static STRPTR pages[3] = {NULL};

struct Conversation *newConversation();
static struct Conversation *copyConversation(struct Conversation *conversation);
static struct GeneratedImage *
copyGeneratedImage(struct GeneratedImage *generatedImage);
static void sendChatMessage();

typedef enum {
    CHAT_STREAM_OK,
    CHAT_STREAM_PARTIAL,
    CHAT_STREAM_FAILED
} ChatStreamOutcome;

static ChatStreamOutcome chatStreamClassifyOutcome(UTF8 *receivedMessage);
static void finishChatStream(ChatStreamOutcome outcome, UTF8 *receivedMessage,
                             ULONG speechUtf8Index, BOOL isNewConversation);
static LONG loadConversations();
static LONG saveConversations();
#define LAST_CONVERSATION_DIR "ENVARC:AmigaGPT"
#define LAST_CONVERSATION_PATH "ENVARC:AmigaGPT/last-conversation"
#define LAST_CONVERSATION_LEGACY "AMIGAGPT:last-conversation.txt"
#define LAST_CONVERSATION_NAME_MAX 512

static void ensureLastConversationEnvarcDir(void) {
    CreateDir(LAST_CONVERSATION_DIR);
}

static void saveLastSelectedConversationName(struct Conversation *conversation) {
    BPTR file;

    if (conversation == NULL || conversation->name == NULL ||
        conversation->name[0] == '\0') {
        return;
    }
    ensureLastConversationEnvarcDir();
    file = Open(LAST_CONVERSATION_PATH, MODE_NEWFILE);
    if (file == 0) {
        return;
    }
    Write(file, conversation->name, (LONG)strlen(conversation->name));
    Close(file);
}

static BPTR openLastConversationFile(void) {
    BPTR file = Open(LAST_CONVERSATION_PATH, MODE_OLDFILE);

    if (file == 0) {
        file = Open(LAST_CONVERSATION_LEGACY, MODE_OLDFILE);
    }
    return file;
}

static BOOL restoreLastSelectedConversation(void) {
    BPTR file;
    STRPTR nameBuf;
    LONG fileSize;
    LONG total;
    LONG i;
    BOOL restored = FALSE;

    if (conversationListObject == NULL) {
        return FALSE;
    }
    file = openLastConversationFile();
    if (file == 0) {
        return FALSE;
    }
#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    fileSize = Seek(file, 0, OFFSET_BEGINNING);
#else
#ifdef __AMIGAOS4__
    fileSize = (LONG)GetFileSize(file);
#else
    struct FileInfoBlock fib;

    ExamineFH64(file, &fib, NULL);
    fileSize = (LONG)fib.fib_Size;
#endif
#endif
    if (fileSize <= 0 || fileSize >= LAST_CONVERSATION_NAME_MAX) {
        Close(file);
        return FALSE;
    }
    nameBuf = AllocVec((ULONG)fileSize + 1, MEMF_CLEAR);
    if (nameBuf == NULL) {
        Close(file);
        return FALSE;
    }
    if (Read(file, nameBuf, fileSize) != fileSize) {
        FreeVec(nameBuf);
        Close(file);
        return FALSE;
    }
    Close(file);
    nameBuf[fileSize] = '\0';

    get(conversationListObject, MUIA_NList_Entries, &total);
    for (i = 0; i < total; i++) {
        struct Conversation *conversation = NULL;

        DoMethod(conversationListObject, MUIM_NList_GetEntry, i, &conversation);
        if (conversation != NULL && conversation->name != NULL &&
            strcmp(conversation->name, nameBuf) == 0) {
            DoMethod(conversationListObject, MUIM_NList_SetActive, i, NULL);
            streamLogLifecycle("restore last conversation ok");
            saveLastSelectedConversationName(conversation);
            restored = TRUE;
            break;
        }
    }
    if (!restored) {
        streamLogLifecycle("restore last conversation miss");
    }
    FreeVec(nameBuf);
    return restored;
}

#ifdef __MORPHOS__
void mainWindowCaptureGeometryForConfig(void) {
    LONG left = 0;
    LONG top = 0;
    LONG width = 0;
    LONG height = 0;
    struct Screen *scr = NULL;

    if (mainWindowObject == NULL) {
        return;
    }
    get(mainWindowObject, MUIA_Window_LeftEdge, &left);
    get(mainWindowObject, MUIA_Window_TopEdge, &top);
    get(mainWindowObject, MUIA_Window_Width, &width);
    get(mainWindowObject, MUIA_Window_Height, &height);
    get(mainWindowObject, MUIA_Window_Screen, &scr);
    if (width < MAIN_WINDOW_SAVED_MIN_WIDTH ||
        height < MAIN_WINDOW_SAVED_MIN_HEIGHT) {
        return;
    }
    if (scr != NULL) {
        LONG sw = scr->Width;
        LONG sh = scr->Height;

        if (width > sw) {
            width = sw - 8;
        }
        if (height > sh) {
            height = sh - 8;
        }
        if (left < 0 || top < 0 || left > sw || top > sh) {
            left = 0;
            top = 0;
        }
        if (left + width > sw) {
            left = sw - width;
        }
        if (top + height > sh) {
            top = sh - height;
        }
        if (left < 0) {
            left = 0;
        }
        if (top < 0) {
            top = 0;
        }
    }
    configSetMainWindowLeft(left);
    configSetMainWindowTop(top);
    configSetMainWindowWidth((ULONG)width);
    configSetMainWindowHeight((ULONG)height);
}

static BOOL mainWindowSavedGeometryValid(void) {
    return mainWindowObject != NULL &&
           configGetMainWindowWidth() >= MAIN_WINDOW_SAVED_MIN_WIDTH &&
           configGetMainWindowHeight() >= MAIN_WINDOW_SAVED_MIN_HEIGHT;
}

static void mainWindowApplySavedGeometry(void) {
    LONG left;
    LONG top;
    LONG width;
    LONG height;
    struct Screen *scr = NULL;

    if (!mainWindowSavedGeometryValid()) {
        return;
    }
    left = configGetMainWindowLeft();
    top = configGetMainWindowTop();
    width = (LONG)configGetMainWindowWidth();
    height = (LONG)configGetMainWindowHeight();
    get(mainWindowObject, MUIA_Window_Screen, &scr);
    if (scr != NULL) {
        LONG sw = scr->Width;
        LONG sh = scr->Height;

        if (width > sw) {
            width = sw - 8;
        }
        if (height > sh) {
            height = sh - 8;
        }
        if (left < 0 || top < 0 || left >= sw || top >= sh) {
            left = (sw - width) / 2;
            top = (sh - height) / 2;
        }
        if (left + width > sw) {
            left = sw - width;
        }
        if (top + height > sh) {
            top = sh - height;
        }
        if (left < 0) {
            left = 0;
        }
        if (top < 0) {
            top = 0;
        }
    }
    set(mainWindowObject, MUIA_Window_Width, width);
    set(mainWindowObject, MUIA_Window_Height, height);
    set(mainWindowObject, MUIA_Window_LeftEdge, left);
    set(mainWindowObject, MUIA_Window_TopEdge, top);
}
#endif /* __MORPHOS__ */

static LONG loadImages();
static LONG saveImages();
static void openImage(struct GeneratedImage *image, WORD scaledWidth,
                      WORD scaledHeight);
static void initStyleStack(StyleStack *s);
static BOOL pushStyle(StyleStack *s, StyleType style);
static BOOL popStyle(StyleStack *s, StyleType style);
static BOOL isTopStyle(const StyleStack *s, StyleType style);
static void outputStyleOn(STRPTR out, size_t outSize, StyleType style);
static void outputStyleOff(STRPTR out, size_t outSize);
static UBYTE parseMarker(CONST_STRPTR input, size_t pos, size_t len,
                         StyleType *foundStyle, const StyleStack *stack);
static BOOL ensureChatOutputBufferCapacity(ULONG required);
static void addMainWindowActions();
#ifndef __MORPHOS__
static Object *newChatOutputFloattextObject(void);
static void installChatOutputWheelHandler(void);
#endif
#ifdef __MORPHOS__
static void clearChatOutputDisplay(void);
#endif

/**
 * Shows the loading bar and starts the busy meter animating
 **/
void showLoadingBar() {
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
    set(loadingBarGroup, MUIA_Group_ActivePage, 1);
}

/**
 * Stops the busy meter and hides the loading bar, leaving it blank
 **/
void hideLoadingBar() {
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
    set(loadingBarGroup, MUIA_Group_ActivePage, 0);
}

/**
 * Title for a new conversation: model text if non-empty, otherwise a truncated
 * first user line, otherwise the localized "New chat" string.
 **/
static STRPTR allocNewConversationTitle(struct Conversation *conv,
                                        const UTF8 *apiText) {
    if (apiText != NULL && strlen((const char *)apiText) > 0) {
        ULONG n = (ULONG)strlen((const char *)apiText) + 1;
        STRPTR out = AllocVec(n, MEMF_ANY | MEMF_CLEAR);
        if (out != NULL) {
            strncpy(out, (const char *)apiText, n - 1);
            out[n - 1] = '\0';
        }
        return out;
    }
    if (conv != NULL && conv->messages != NULL) {
        struct MinNode *node = conv->messages->mlh_Head;
        while (node->mln_Succ != NULL) {
            struct ConversationNode *m = (struct ConversationNode *)node;
            if (strcmp((const char *)m->role, "user") == 0) {
                UTF8 *userRaw = conversationNodeGetRaw(m);
                if (userRaw != NULL && strlen((const char *)userRaw) > 0) {
                    STRPTR out = AllocVec(CONVERSATION_TITLE_FALLBACK_MAX + 1,
                                          MEMF_ANY | MEMF_CLEAR);
                    if (out == NULL)
                        return NULL;
                    ULONG pos = 0;
                    const UTF8 *s = userRaw;
                    for (; *s != '\0' && pos < CONVERSATION_TITLE_FALLBACK_MAX;
                         s++) {
                        UBYTE c = (UBYTE)*s;
                        if (c == '\n' || c == '\r')
                            out[pos++] = ' ';
                        else
                            out[pos++] = (char)c;
                    }
                    while (pos > 0 && out[pos - 1] == ' ')
                        pos--;
                    out[pos] = '\0';
                    if (pos > 0)
                        return out;
                    FreeVec(out);
                }
            }
            node = node->mln_Succ;
        }
    }
    {
        const char *d = STRING_NEW_CHAT;
        ULONG n = (ULONG)strlen(d) + 1;
        STRPTR out = AllocVec(n, MEMF_ANY | MEMF_CLEAR);
        if (out != NULL) {
            strncpy(out, d, n - 1);
            out[n - 1] = '\0';
        }
        return out;
    }
}

HOOKPROTONHNO(ConstructConversationLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    struct Conversation *entry = (struct Conversation *)ncm->entry;
    return (entry);
}
MakeHook(ConstructConversationLI_TextHook, ConstructConversationLI_TextFunc);

HOOKPROTONHNO(DestructConversationLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    if (ndm->entry)
        freeConversation((struct Conversation *)ndm->entry);
}
MakeHook(DestructConversationLI_TextHook, DestructConversationLI_TextFunc);

HOOKPROTONHNO(DisplayConversationLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    static char nameBuf[512];
    struct Conversation *entry = (struct Conversation *)ndm->entry;
    if (entry == NULL) {
        ndm->strings[0] = (STRPTR) "";
    } else if (entry->name_list_display != NULL) {
        ndm->strings[0] = entry->name_list_display;
    } else if (entry->name == NULL) {
        ndm->strings[0] = (STRPTR) "";
    } else {
        STRPTR converted = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)entry->name,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        if (converted != NULL) {
            strncpy(nameBuf, converted, sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
            CodesetsFreeA(converted, NULL);
        } else {
            strncpy(nameBuf, entry->name, sizeof(nameBuf) - 1);
            nameBuf[sizeof(nameBuf) - 1] = '\0';
        }
        ndm->strings[0] = nameBuf;
    }
}
MakeHook(DisplayConversationLI_TextHook, DisplayConversationLI_TextFunc);

HOOKPROTONHNO(ConstructImageLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    struct GeneratedImage *entry = (struct GeneratedImage *)ncm->entry;
    return (entry);
}
MakeHook(ConstructImageLI_TextHook, ConstructImageLI_TextFunc);

HOOKPROTONHNO(DestructImageLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    if (ndm->entry) {
        struct GeneratedImage *entry = (struct GeneratedImage *)ndm->entry;
        FreeVec(entry->name);
        FreeVec(entry->filePath);
        FreeVec(entry->prompt);
        FreeVec(entry);
    }
}
MakeHook(DestructImageLI_TextHook, DestructImageLI_TextFunc);

HOOKPROTONHNO(DisplayImageLI_TextFunc, void, struct NList_DisplayMessage *ndm) {
    static char imgNameBuf[512];
    struct GeneratedImage *entry = (struct GeneratedImage *)ndm->entry;
    if (entry == NULL || entry->name == NULL) {
        ndm->strings[0] = (STRPTR) "";
    } else {
        STRPTR converted = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)entry->name,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        if (converted != NULL) {
            strncpy(imgNameBuf, converted, sizeof(imgNameBuf) - 1);
            imgNameBuf[sizeof(imgNameBuf) - 1] = '\0';
            CodesetsFreeA(converted, NULL);
        } else {
            strncpy(imgNameBuf, entry->name, sizeof(imgNameBuf) - 1);
            imgNameBuf[sizeof(imgNameBuf) - 1] = '\0';
        }
        ndm->strings[0] = imgNameBuf;
    }
}
MakeHook(DisplayImageLI_TextHook, DisplayImageLI_TextFunc);

#ifdef __MORPHOS__
static struct Conversation *conversationRowPending;
static BOOL morphosStartupDeferredDone;
static BOOL morphosConversationSelectEnabled;
static BOOL chatOutputRefreshPending;
static BOOL chatOutputRefreshPreserveViewport;
static BOOL chatOutputRefreshFromList;
static BOOL chatOutputMorphosListRefreshActive;

HOOKPROTONHNONP(ChatOutputRefreshDeferredFunc, void) {
    BOOL preserve = chatOutputRefreshPreserveViewport;

    chatOutputRefreshPending = FALSE;
    chatOutputMorphosListRefreshActive = chatOutputRefreshFromList;
    chatOutputRefreshFromList = FALSE;
    if (mainWindowIsShuttingDown()) {
        chatOutputMorphosListRefreshActive = FALSE;
        return;
    }
    streamLogLifecycle("displayConversation scintilla refresh begin");
    chatOutputUpdateFromBuffer(preserve);
    chatOutputMorphosListRefreshActive = FALSE;
    streamLogLifecycle("displayConversation scintilla refresh done");
}
MakeHook(ChatOutputRefreshDeferredHook, ChatOutputRefreshDeferredFunc);

void morphosScheduleChatOutputRefresh(BOOL preserveViewport) {
    if (mainWindowIsShuttingDown() || app == NULL) {
        return;
    }
    chatOutputRefreshPreserveViewport = preserveViewport;
    if (!chatOutputRefreshPending) {
        chatOutputRefreshFromList = FALSE;
        chatOutputRefreshPending = TRUE;
        DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
                 &ChatOutputRefreshDeferredHook);
    }
}

void morphosScheduleChatOutputRefreshFromList(void) {
    if (mainWindowIsShuttingDown() || app == NULL) {
        return;
    }
    chatOutputRefreshPreserveViewport = FALSE;
    chatOutputRefreshFromList = TRUE;
    if (!chatOutputRefreshPending) {
        chatOutputRefreshPending = TRUE;
        DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
                 &ChatOutputRefreshDeferredHook);
    }
}

static void morphosApplyPendingConversationSelection(void) {
    struct Conversation *conversation = conversationRowPending;

    conversationRowPending = NULL;
    if (mainWindowIsShuttingDown() || conversation == NULL) {
        return;
    }
    currentConversation = conversation;
    saveLastSelectedConversationName(currentConversation);
    streamLogLifecycle("conversation select deferred begin");
    codeBlocksViewerDismiss();
    displayConversation(currentConversation);
    refreshViewCodeBlocksMenuState();
    streamLogLifecycle("conversation select deferred done");
    if (chatInputTextEditor != NULL) {
        DoMethod(chatInputTextEditor, MUIM_GoActive);
    }
}

static void morphosFlushPendingPushMethods(void) {
    if (app == NULL) {
        return;
    }
    if (mainWindowIsShuttingDown()) {
        conversationRowPending = NULL;
        chatOutputRefreshPending = FALSE;
        chatOutputRefreshFromList = FALSE;
        chatOutputScintillaCancelDeferredStyles();
        chatOutputScintillaCancelPendingCodeblockOpen();
        streamLogLifecycle("morphos flush pushmethods skipped shutdown");
        return;
    }
    ULONG muiSig = 0;
    ULONG pass;

    streamLogLifecycle("morphos flush pushmethods begin");
    for (pass = 0; pass < 12; pass++) {
        BOOL hadPending = chatOutputRefreshPending ||
                          (conversationRowPending != NULL) ||
                          chatOutputScintillaHasDeferredWorkPending();

        (void)DoMethod(app, MUIM_Application_CheckRefresh);
        (void)DoMethod(app, MUIM_Application_NewInput, &muiSig);
        if (!hadPending) {
            break;
        }
    }
    streamLogLifecycle("morphos flush pushmethods done");
}

static void morphosEnableConversationSelect(void);

void morphosRunStartupDeferred(void) {
    if (morphosStartupDeferredDone || mainWindowObject == NULL ||
        chatOutputTextEditor == NULL) {
        return;
    }
    morphosStartupDeferredDone = TRUE;
    streamLogLifecycle("morphos startup deferred begin");
    chatOutputScintillaFinishViewerInit(chatOutputTextEditor);
    streamLogBootPhase("chat scintilla init");
    streamLogLifecycle("morphos startup deferred finish done");
    chatOutputScintillaInstallMouseUpGuard();
    streamLogLifecycle("morphos startup deferred mouse guard done");
    mainWindowApplySavedGeometry();
    morphosEnableConversationSelect();
}

HOOKPROTONHNONP(ConversationRowClickedDeferredFunc, void) {
    if (!morphosConversationSelectEnabled || mainWindowIsShuttingDown()) {
        return;
    }
    morphosApplyPendingConversationSelection();
}
MakeHook(ConversationRowClickedDeferredHook, ConversationRowClickedDeferredFunc);

static void morphosEnableConversationSelect(void) {
    struct Conversation *active = NULL;
    struct Conversation *pick = NULL;

    morphosConversationSelectEnabled = TRUE;
    streamLogLifecycle("morphos conversation select enabled");
    if (conversationListObject != NULL) {
        DoMethod(conversationListObject, MUIM_NList_GetEntry,
                 MUIV_NList_GetEntry_Active, &active);
        if (active == NULL) {
            DoMethod(conversationListObject, MUIM_NList_GetEntry, 0, &active);
            if (active != NULL) {
                DoMethod(conversationListObject, MUIM_NList_SetActive, 0,
                         NULL);
                streamLogLifecycle("morphos conversation auto-select first");
            }
        } else {
            streamLogLifecycle("morphos conversation auto-select active");
        }
    }
    if (conversationRowPending != NULL) {
        pick = conversationRowPending;
    } else if (active != NULL) {
        pick = active;
    }
    if (pick != NULL && app != NULL) {
        conversationRowPending = pick;
        DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
                 &ConversationRowClickedDeferredHook);
    } else {
        streamLogLifecycle("morphos conversation auto-select none");
    }
}

HOOKPROTONHNONP(ConversationRowClickedFunc, void) {
    struct Conversation *conversation;

    DoMethod(conversationListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &conversation);
    if (conversation == NULL || app == NULL) {
        return;
    }
    conversationRowPending = conversation;
    if (!morphosConversationSelectEnabled) {
        streamLogLifecycle("conversation select queued until startup ready");
        return;
    }
    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &ConversationRowClickedDeferredHook);
}

#else /* !__MORPHOS__ */
HOOKPROTONHNONP(ConversationRowClickedFunc, void) {
    set(chatInputTextEditor, MUIA_TextEditor_FixedFont, TRUE);
    struct Conversation *conversation;
    DoMethod(conversationListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &conversation);
    if (conversation) {
        currentConversation = conversation;
        saveLastSelectedConversationName(currentConversation);
        displayConversation(currentConversation);
        DoMethod(chatInputTextEditor, MUIM_GoActive);
    }
}
#endif
MakeHook(ConversationRowClickedHook, ConversationRowClickedFunc);

HOOKPROTONHNONP(ImageRowClickedFunc, void) {
    set(imageInputTextEditor, MUIA_Disabled, FALSE);

    struct GeneratedImage *image = NULL;
    DoMethod(imageListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active,
             &image);
    if (image) {
        set(createImageButton, MUIA_Disabled, TRUE);
        set(openImageButton, MUIA_Disabled, FALSE);
        set(saveImageCopyButton, MUIA_Disabled, FALSE);
        if (isAROS) {
            set(imageInputTextEditor, MUIA_String_Contents, image->prompt);
        } else {
            set(imageInputTextEditor, MUIA_TextEditor_Contents, image->prompt);
        }
        currentImage = image;
        DoMethod(imageViewGroup, MUIM_Group_InitChange);
        DoMethod(imageViewGroup, OM_REMMEMBER, imageView);
        MUI_DisposeObject(imageView);
        // clang-format off
        imageView = GuigfxObject,
            MUIA_Guigfx_FileName, image->filePath,
            MUIA_Guigfx_Quality, MUIV_Guigfx_Quality_Low,
            MUIA_Guigfx_ScaleMode, NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE,
            MUIA_Guigfx_Transparency, NITRF_MASK,
        End;
        // clang-format on
        DoMethod(imageViewGroup, OM_ADDMEMBER, imageView);
        DoMethod(imageViewGroup, MUIM_Group_MoveMember, imageView, 0);
        DoMethod(imageViewGroup, MUIM_Group_ExitChange);
    }
}
MakeHook(ImageRowClickedHook, ImageRowClickedFunc);

HOOKPROTONHNONP(NewChatButtonClickedFunc, void) {
    currentConversation = NULL;
#ifdef __MORPHOS__
    clearChatOutputDisplay();
#else
    DoMethod(chatOutputTextEditor, MUIM_NList_Clear);
#endif
#ifdef __MORPHOS__
    codeBlocksViewerDismiss();
    refreshViewCodeBlocksMenuState();
#endif
    DoMethod(chatInputTextEditor, MUIM_GoActive);
}
MakeHook(NewChatButtonClickedHook, NewChatButtonClickedFunc);

HOOKPROTONHNONP(DeleteChatButtonClickedFunc, void) {
    DoMethod(conversationListObject, MUIM_NList_Remove,
             MUIV_NList_Remove_Active);
    currentConversation = NULL;
#ifdef __MORPHOS__
    clearChatOutputDisplay();
#else
    DoMethod(chatOutputTextEditor, MUIM_NList_Clear);
#endif
#ifdef __MORPHOS__
    codeBlocksViewerDismiss();
    refreshViewCodeBlocksMenuState();
#endif
    saveConversations();
}
MakeHook(DeleteChatButtonClickedHook, DeleteChatButtonClickedFunc);

HOOKPROTONHNONP(SendMessageButtonClickedFunc, void) {
    struct ChatRequestSettings chatSettings;
    configGetActiveChatRequestSettings(&chatSettings);
    if (chatSettings.authorizationType != AUTHORIZATION_TYPE_NONE &&
        (chatSettings.apiKey == NULL || strlen(chatSettings.apiKey) == 0)) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }
    sendChatMessage();
}
MakeHook(SendMessageButtonClickedHook, SendMessageButtonClickedFunc);

HOOKPROTONHNONP(StopSpeakingButtonClickedFunc, void) {
#ifdef __AMIGAOS3__
    if (NarratorIO != NULL &&
        ((struct IORequest *)NarratorIO)->io_Device != NULL) {
        if (!CheckIO((struct IORequest *)NarratorIO)) {
            AbortIO((struct IORequest *)NarratorIO);
            WaitIO((struct IORequest *)NarratorIO);
        }
    }
#elif defined(__AMIGAOS4__)
    if (fliteRequest != NULL &&
        ((struct IORequest *)fliteRequest)->io_Device != NULL &&
        !CheckIO((struct IORequest *)fliteRequest)) {
        AbortIO((struct IORequest *)fliteRequest);
        WaitIO((struct IORequest *)fliteRequest);
    }
#endif
    if (ahiRequest != NULL && !CheckIO((struct IORequest *)ahiRequest)) {
        AbortIO((struct IORequest *)ahiRequest);
        WaitIO((struct IORequest *)ahiRequest);
    }
}
MakeHook(StopSpeakingButtonClickedHook, StopSpeakingButtonClickedFunc);

HOOKPROTONHNONP(NewImageButtonClickedFunc, void) {
    currentImage = NULL;
    set(imageInputTextEditor, MUIA_Disabled, FALSE);
    set(createImageButton, MUIA_Disabled, FALSE);
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    if (isAROS) {
        set(imageInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(imageInputTextEditor, MUIM_GoActive);
    DoMethod(imageViewGroup, MUIM_Group_InitChange);
    DoMethod(imageViewGroup, OM_REMMEMBER, imageView);
    MUI_DisposeObject(imageView);
    // clang-format off
    imageView = RectangleObject, MUIA_Frame, MUIV_Frame_ImageButton, End;
    // clang-format on
    DoMethod(imageViewGroup, OM_ADDMEMBER, imageView);
    DoMethod(imageViewGroup, MUIM_Group_MoveMember, imageView, 0);
    DoMethod(imageViewGroup, MUIM_Group_ExitChange);
}
MakeHook(NewImageButtonClickedHook, NewImageButtonClickedFunc);

HOOKPROTONHNONP(DeleteImageButtonClickedFunc, void) {
    struct GeneratedImage *entry;
    DoMethod(imageListObject, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active,
             &entry);
    if (entry != NULL && entry->filePath != NULL) {
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
        DeleteFile(entry->filePath);
#else
        Delete(entry->filePath);
#endif
    }
    DoMethod(imageListObject, MUIM_NList_Remove, MUIV_NList_Remove_Active);
    DoMethod(imageListObject, MUIM_NList_Select, MUIV_NList_Select_All,
             MUIV_NList_Select_Off, NULL);
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    if (isAROS) {
        set(imageInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(imageViewGroup, MUIM_Group_InitChange);
    DoMethod(imageViewGroup, OM_REMMEMBER, imageView);
    MUI_DisposeObject(imageView);
    // clang-format off
    imageView = RectangleObject, MUIA_Frame, MUIV_Frame_ImageButton, End;
    // clang-format on
    DoMethod(imageViewGroup, OM_ADDMEMBER, imageView);
    DoMethod(imageViewGroup, MUIM_Group_MoveMember, imageView, 0);
    DoMethod(imageViewGroup, MUIM_Group_ExitChange);
    saveImages();
}
MakeHook(DeleteImageButtonClickedHook, DeleteImageButtonClickedFunc);

static BOOL isStringInList(CONST_STRPTR str, CONST_STRPTR *list) {
    if (str == NULL || list == NULL)
        return FALSE;
    for (UBYTE i = 0; list[i] != NULL; i++) {
        if (strcmp(str, list[i]) == 0)
            return TRUE;
    }
    return FALSE;
}

HOOKPROTONHNONP(CreateImageButtonClickedFunc, void) {
    struct ImageRequestSettings imageSettings;
    configGetActiveImageRequestSettings(&imageSettings);
    CONST_STRPTR apiKey = imageSettings.apiKey;
    if (imageSettings.authorizationType != AUTHORIZATION_TYPE_NONE &&
        (apiKey == NULL || strlen(apiKey) == 0)) {
        displayError(STRING_ERROR_NO_API_KEY);
        return;
    }
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    set(createImageButton, MUIA_Disabled, TRUE);
    set(newImageButton, MUIA_Disabled, TRUE);
    set(deleteImageButton, MUIA_Disabled, TRUE);
    set(imageInputTextEditor, MUIA_Disabled, TRUE);
    STRPTR text;
    if (isAROS) {
        get(imageInputTextEditor, MUIA_String_Contents, &text);
    } else {
        text = DoMethod(imageInputTextEditor, MUIM_TextEditor_ExportText);
    }
    // Remove trailing newline characters
    while (text[strlen(text) - 1] == '\n') {
        text[strlen(text) - 1] = '\0';
    }

    /* Keep legacy image model enum for history + size selection. */
    ImageModel imageModel = configGetImageModel();
    ImageSize imageSize;
    switch (imageModel) {
    case DALL_E_2:
        imageSize = configGetImageSizeDallE2();
        break;
    case DALL_E_3:
        imageSize = configGetImageSizeDallE3();
        break;
    case GPT_IMAGE_1:
    case GPT_IMAGE_1_MINI:
    case GPT_IMAGE_1_5:
        imageSize = configGetImageSizeGptImage1();
        break;
    default:
        imageSize = configGetImageSizeGptImage1();
        break;
    }
    struct json_object *response = postImageCreationRequestToOpenAI(
        text, imageSettings.host, imageSettings.port, imageSettings.useSSL,
        imageSettings.apiEndpointUrl, imageSettings.authorizationType,
        imageSettings.customHeaders, imageSettings.model, imageSize, apiKey,
        configGetProxyEnabled(), configGetProxyHost(), configGetProxyPort(),
        configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
        configGetProxyUsername(), configGetProxyPassword(),
        configGetImageFormat(), imageSettings.imageApiEndpoint);

    if (response == NULL) {
        displayError(STRING_ERROR_CONNECTION);
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        if (!isAROS) {
            FreeVec(text);
        }
        return;
    }
    struct json_object *error;

    if (json_object_object_get_ex(response, "error", &error) &&
        !json_object_is_type(error, json_type_null)) {
        struct json_object *message = json_object_object_get(error, "message");
        UTF8 *messageString = json_object_get_string(message);
        STRPTR messageStringSystemEncoded = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)messageString,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        if (messageStringSystemEncoded != NULL) {
            displayError(messageStringSystemEncoded);
            CodesetsFreeA(messageStringSystemEncoded, NULL);
        } else {
            struct json_object *type = json_object_object_get(error, "type");
            UTF8 *typeString = json_object_get_string(type);
            if (typeString != NULL) {
                if (strcmp(typeString, "invalid_request_error") == 0) {
                    displayError(STRING_ERROR_INVALID_REQUEST);
                } else {
                    displayError(typeString);
                }
            }
        }
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        json_object_put(response);
        if (!isAROS) {
            FreeVec(text);
        }
        return;
    }

    struct json_object *dataArrayObj = NULL;
    struct array_list *data = NULL;
    struct json_object *dataObject = NULL;
    STRPTR b64 = NULL;

    if (json_object_object_get_ex(response, "data", &dataArrayObj) &&
        json_object_is_type(dataArrayObj, json_type_array)) {
        data = json_object_get_array(dataArrayObj);
    }
    if (data != NULL && data->length > 0) {
        dataObject = (struct json_object *)data->array[0];
    }
    if (dataObject != NULL && json_object_is_type(dataObject, json_type_object)) {
        struct json_object *b64Obj = NULL;
        if (json_object_object_get_ex(dataObject, "b64_json", &b64Obj)) {
            b64 = (STRPTR)json_object_get_string(b64Obj);
        }
    }
    if (b64 == NULL) {
        displayError(STRING_ERROR_CONNECTION);
        json_object_put(response);
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        if (!isAROS) { FreeVec(text); }
        return;
    }

    STRPTR imageData;
    ULONG b64Len = strlen(b64);
    CodesetsDecodeB64(CSA_B64SourceString, (Tag)b64, CSA_B64SourceLen,
                      (Tag)b64Len, CSA_B64DestPtr, (Tag)&imageData, TAG_DONE);
    if (imageData == NULL) {
        displayError(STRING_ERROR_INVALID_BASE64);
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        json_object_put(response);
        return;
    }

    LONG data_len = (b64Len * 3) / 4;
    while (b64Len > 0 && b64[--b64Len] == '=')
        data_len--;

    CreateDir("AMIGAGPT:images");
    /* Try to match file extension to actual bytes; fallback to user preference.
     */
    STRPTR imageFormat;
    if (data_len >= 8 && imageData[0] == 0x89 && imageData[1] == 0x50 &&
        imageData[2] == 0x4E && imageData[3] == 0x47 && imageData[4] == 0x0D &&
        imageData[5] == 0x0A && imageData[6] == 0x1A && imageData[7] == 0x0A) {
        imageFormat = "png";
    } else if (data_len >= 3 && imageData[0] == 0xFF && imageData[1] == 0xD8 &&
               imageData[2] == 0xFF) {
        imageFormat = "jpg";
    } else {
        ImageFormat cfgFmt = configGetImageFormat();
        if (cfgFmt != IMAGE_FORMAT_NULL && IMAGE_FORMAT_NAMES[cfgFmt] != NULL) {
            imageFormat = IMAGE_FORMAT_NAMES[cfgFmt];
        } else {
            imageFormat = "png";
        }
    }

    // Generate unique ID for the image
    UBYTE fullPath[35] = "";
    UBYTE id[11] = "";
    CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
    srand(time(NULL));
    for (UBYTE i = 0; i < 9; i++) {
        id[i] = idChars[rand() % strlen(idChars)];
    }
    snprintf(fullPath, sizeof(fullPath), "AMIGAGPT:images/%s.%s", id,
             imageFormat);

    if (imageData == NULL || data_len <= 0) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        json_object_put(response);
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        if (!isAROS) { FreeVec(text); }
        return;
    }

    FILE *file = fopen(fullPath, "wb");
    if (file != NULL) {
        fwrite(imageData, 1, data_len, file);
        fclose(file);
    }
    CodesetsFreeA(imageData, NULL);

    json_object_put(response);

    WORD imageWidth, imageHeight;
    switch (imageSize) {
    case IMAGE_SIZE_NULL:
        /* "None" means don't send size; image may still be returned. */
        imageWidth = 1024;
        imageHeight = 1024;
        break;
    case IMAGE_SIZE_256x256:
        imageWidth = 256;
        imageHeight = 256;
        break;
    case IMAGE_SIZE_512x512:
        imageWidth = 512;
        imageHeight = 512;
        break;
    case IMAGE_SIZE_1024x1024:
        imageWidth = 1024;
        imageHeight = 1024;
        break;
    case IMAGE_SIZE_1792x1024:
        imageWidth = 1792;
        imageHeight = 1024;
        break;
    case IMAGE_SIZE_1024x1792:
        imageWidth = 1024;
        imageHeight = 1792;
        break;
    case IMAGE_SIZE_1024x1536:
        imageWidth = 1024;
        imageHeight = 1536;
        break;
    case IMAGE_SIZE_1536x1024:
        imageWidth = 1536;
        imageHeight = 1024;
        break;
    case IMAGE_SIZE_AUTO:
        imageWidth = 1024;
        imageHeight = 1024;
        break;
    default:
        displayError(STRING_ERROR_INVALID_IMAGE_SIZE);
        imageWidth = 256;
        imageHeight = 256;
        break;
    }

    updateStatusBar(STRING_GENERATING_IMAGE_NAME, 7);
    struct ChatRequestSettings nameSettings;
    configGetActiveChatRequestSettings(&nameSettings);
    nameSettings.webSearchEnabled = FALSE;
    /* Image title generation must use a chat-capable model. If the user has
     * (accidentally) selected an image model as their chat model, fall back to
     * a sane chat default for the active provider to avoid bad requests. */
    CONST_STRPTR titleModel = nameSettings.model;
    if (titleModel != NULL && strlen(titleModel) > 0) {
        BOOL isImageModel = FALSE;
        if (nameSettings.host != NULL &&
            strcmp(nameSettings.host, "api.openai.com") == 0) {
            isImageModel = isStringInList(titleModel, OPENAI_IMAGE_MODELS);
        } else if (nameSettings.host != NULL &&
                   strcmp(nameSettings.host,
                          "generativelanguage.googleapis.com") == 0) {
            isImageModel = isStringInList(titleModel, GEMINI_IMAGE_MODELS);
        } else if (nameSettings.host != NULL &&
                   strcmp(nameSettings.host, "api.x.ai") == 0) {
            isImageModel = isStringInList(titleModel, GROK_IMAGE_MODELS);
        }
        if (isImageModel) {
            if (nameSettings.host != NULL &&
                strcmp(nameSettings.host, "api.openai.com") == 0) {
                titleModel = "gpt-5-chat-latest";
            } else if (nameSettings.host != NULL &&
                       strcmp(nameSettings.host,
                              "generativelanguage.googleapis.com") == 0) {
                titleModel = "gemini-2.5-flash";
            } else if (nameSettings.host != NULL &&
                       strcmp(nameSettings.host, "api.x.ai") == 0) {
                titleModel = "grok-4";
            }
        }
    }
    UTF8 *textUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                        CSA_Source, (Tag)text, TAG_DONE);
    struct Conversation *imageNameConversation = newConversation();
    addTextToConversation(imageNameConversation, textUTF8, "user");
    CodesetsFreeA(textUTF8, NULL);
    addTextToConversation(
        imageNameConversation,
        "generate a short title for this image and don't enclose the "
        "title "
        "in quotes or prefix the response with anything",
        "user");
    struct json_object **responses = postChatMessageToOpenAI(
        imageNameConversation, nameSettings.host, nameSettings.port,
        nameSettings.useSSL, titleModel, nameSettings.apiKey, FALSE,
        nameSettings.useProxy, nameSettings.proxyHost, nameSettings.proxyPort,
        nameSettings.proxyUsesSSL, nameSettings.proxyRequiresAuth,
        nameSettings.proxyUsername, nameSettings.proxyPassword, FALSE, FALSE,
        nameSettings.apiEndpoint, nameSettings.apiEndpointUrl,
        nameSettings.authorizationType, nameSettings.customHeaders);

    struct GeneratedImage *generatedImage =
        AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
    if (responses == NULL) {
        displayError(STRING_ERROR_GENERATING_IMAGE_NAME);
    } else if (responses[0] != NULL) {
        if (json_object_object_get_ex(responses[0], "error", &error) &&
            !json_object_is_type(error, json_type_null)) {
            set(createImageButton, MUIA_Disabled, FALSE);
            set(newImageButton, MUIA_Disabled, FALSE);
            set(deleteImageButton, MUIA_Disabled, FALSE);
            set(imageInputTextEditor, MUIA_Disabled, FALSE);
            json_object_put(responses[0]);
            FreeVec(responses);
            if (!isAROS) {
                FreeVec(text);
            }
            freeConversation(imageNameConversation);
            return;
        }
        /* Some providers (e.g. Claude with web search) may return text in
         * multiple response objects. Concatenate all text parts. */
        ULONG combinedLen = 1;
        UWORD ri = 0;
        struct json_object *r = NULL;
        STRPTR combined = NULL;
        while ((r = responses[ri++]) != NULL) {
            UTF8 *part = getMessageContentFromJson(r, FALSE, FALSE,
                                                   nameSettings.apiEndpoint);
            if (part != NULL)
                combinedLen += strlen(part) + 1;
        }
        combined = AllocVec(combinedLen, MEMF_ANY | MEMF_CLEAR);
        if (combined == NULL) {
            combined = (STRPTR) "";
        } else {
            ri = 0;
            while ((r = responses[ri++]) != NULL) {
                UTF8 *part = getMessageContentFromJson(
                    r, FALSE, FALSE, nameSettings.apiEndpoint);
                if (part != NULL && strlen(part) > 0) {
                    strncat(combined, part, combinedLen - strlen(combined) - 1);
                }
            }
        }
        generatedImage->name =
            AllocVec(strlen(combined) + 1, MEMF_ANY | MEMF_CLEAR);
        if (generatedImage->name != NULL) {
            strncpy(generatedImage->name, combined, strlen(combined));
        }
        updateStatusBar(STRING_READY, 5);
        ri = 0;
        while ((r = responses[ri++]) != NULL) {
            json_object_put(r);
        }
        FreeVec(responses);
        if (combined != NULL && combined != (STRPTR) "") {
            FreeVec(combined);
        }
    } else {
        generatedImage->name = dupStringAlloc(id);
        updateStatusBar(STRING_READY, 5);
        if (responses != NULL) {
            FreeVec(responses);
        }
        displayError("Failed to generate image name. Using ID instead.");
    }
    freeConversation(imageNameConversation);

    generatedImage->filePath = dupStringAlloc(fullPath);
    generatedImage->prompt = dupStringAlloc(text);
    if (generatedImage->name == NULL || generatedImage->filePath == NULL ||
        generatedImage->prompt == NULL) {
        if (generatedImage->name != NULL) {
            FreeVec(generatedImage->name);
        }
        if (generatedImage->filePath != NULL) {
            FreeVec(generatedImage->filePath);
        }
        if (generatedImage->prompt != NULL) {
            FreeVec(generatedImage->prompt);
        }
        FreeVec(generatedImage);
        set(createImageButton, MUIA_Disabled, FALSE);
        set(newImageButton, MUIA_Disabled, FALSE);
        set(deleteImageButton, MUIA_Disabled, FALSE);
        set(imageInputTextEditor, MUIA_Disabled, FALSE);
        if (!isAROS) {
            FreeVec(text);
        }
        displayError(STRING_ERROR_GENERATING_IMAGE_NAME);
        return;
    }
    generatedImage->imageModel = configGetImageModel();
    generatedImage->width = imageWidth;
    generatedImage->height = imageHeight;
    DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage,
             MUIV_NList_Insert_Top);
    DoMethod(imageListObject, MUIM_NList_SetActive, 0, NULL);
    currentImage = generatedImage;
    ImageRowClickedFunc();

    set(createImageButton, MUIA_Disabled, FALSE);
    set(newImageButton, MUIA_Disabled, FALSE);
    set(deleteImageButton, MUIA_Disabled, FALSE);

    saveImages();

    if (!isAROS) {
        FreeVec(text);
    }
}
MakeHook(CreateImageButtonClickedHook, CreateImageButtonClickedFunc);

HOOKPROTONHNONP(OpenImageButtonClickedFunc, void) {
    // Get screen dimensions from main window
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    WORD screenWidth = currentScreen ? currentScreen->Width : 640;
    WORD screenHeight = currentScreen ? currentScreen->Height : 480;

    // Calculate the maximum size for the image
    LONG maxImageWidth = (screenWidth * 90) / 100;   // 90% of screen width
    LONG maxImageHeight = (screenHeight * 90) / 100; // 90% of screen height

    // Variables to hold the new dimensions
    LONG newWidth = 0;
    LONG newHeight = 0;

    if (currentImage != NULL) {
        if (currentImage->width > currentImage->height) {
            // Landscape: fit width and calculate height maintaining aspect
            // ratio
            newWidth = maxImageWidth;
            newHeight =
                (currentImage->height * maxImageWidth) / currentImage->width;

            // Ensure height does not exceed maximum
            if (newHeight > maxImageHeight) {
                newHeight = maxImageHeight;
                newWidth = (currentImage->width * maxImageHeight) /
                           currentImage->height;
            }
        } else {
            // Portrait or square: fit height and calculate width
            // maintaining aspect ratio
            newHeight = maxImageHeight;
            newWidth =
                (currentImage->width * maxImageHeight) / currentImage->height;

            // Ensure width does not exceed maximum
            if (newWidth > maxImageWidth) {
                newWidth = maxImageWidth;
                newHeight = (currentImage->height * maxImageWidth) /
                            currentImage->width;
            }
        }

        // Open the image with calculated dimensions
        openImage(currentImage, newWidth, newHeight);
    } else {
        displayError(STRING_ERROR_NO_IMAGE_SELECTED);
    }
}
MakeHook(OpenImageButtonClickedHook, OpenImageButtonClickedFunc);

HOOKPROTONHNONP(SaveImageCopyButtonClickedFunc, void) {
    if (currentImage == NULL)
        return;
    STRPTR filePath = currentImage->filePath;
    STRPTR fileExtension = strrchr(filePath, '.');
    if (fileExtension == NULL) {
        fileExtension = ".png";
    }
    UBYTE fileName[11] = "";
    snprintf(fileName, sizeof(fileName), "image%s\0", fileExtension);
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq != NULL) {
        if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                           STRING_SAVE_IMAGE_COPY, ASLFR_InitialFile, fileName,
                           ASLFR_InitialDrawer, "SYS:", ASLFR_DoSaveMode, TRUE,
                           TAG_DONE)) {
            STRPTR savePath = fileReq->fr_Drawer;
            STRPTR saveName = fileReq->fr_File;
            UWORD fullPathLength = strlen(savePath) + strlen(saveName) + 2;
            STRPTR fullPath = AllocVec(fullPathLength, MEMF_CLEAR);
            if (fullPath != NULL) {
                snprintf(fullPath, fullPathLength, "%s", savePath);
                AddPart(fullPath, saveName, fullPathLength);
                copyFile(filePath, fullPath);
                FreeVec(fullPath);
            }
        }
        FreeAslRequest(fileReq);
    }
}
MakeHook(SaveImageCopyButtonClickedHook, SaveImageCopyButtonClickedFunc);

HOOKPROTONHNONP(ConfigureForScreenFunc, void) {
    struct Screen *currentScreen;

    if (mainWindowObject == NULL || app == NULL) {
        return;
    }

    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);

    if (currentScreen) {
        // Release old pens if they exist
        if (redPen)
            ReleasePen(currentScreen->ViewPort.ColorMap, redPen);
        if (greenPen)
            ReleasePen(currentScreen->ViewPort.ColorMap, greenPen);
        if (bluePen)
            ReleasePen(currentScreen->ViewPort.ColorMap, bluePen);
        if (yellowPen)
            ReleasePen(currentScreen->ViewPort.ColorMap, yellowPen);

        // Allocate new pens for current screen
        redPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF, 0,
                               0, OBP_Precision, PRECISION_GUI, TAG_DONE);
        greenPen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0xBBBBBBBB, 0,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        bluePen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0, 0xFFFFFFFF,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        yellowPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF,
                                  0xFFFFFFFF, 0, OBP_Precision, PRECISION_GUI,
                                  TAG_DONE);
    }

    const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
    STRPTR buttonLabelText = AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s\0",
             greenPen, STRING_NEW_CHAT);
    set(newChatButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s\0",
             redPen, STRING_DELETE_CHAT);
    set(deleteChatButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
             bluePen, STRING_SEND);
    set(sendMessageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s\0",
             greenPen, STRING_NEW_IMAGE);
    set(newImageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
             redPen, STRING_STOP_SPEAKING);
    set(stopSpeakingButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s\0",
             redPen, STRING_DELETE_IMAGE);
    set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
             bluePen, STRING_CREATE_IMAGE);
    set(createImageButton, MUIA_Text_Contents, buttonLabelText);
    FreeVec(buttonLabelText);
    SetAttrs(mainWindowObject, MUIA_Window_ActiveObject, chatInputTextEditor,
             TAG_DONE);

    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);

    updateStatusBar(STRING_READY, greenPen);
}
MakeHook(ConfigureForScreenHook, ConfigureForScreenFunc);

/**
 * Initialize the style stack.
 * @param s : the style stack
 */
static void initStyleStack(StyleStack *s) { s->top = -1; }

/**
 * Push a style onto the stack.
 * @param s : the style stack
 * @param style : the style to push
 * @return true if the style was pushed, false if the stack is full
 */
static BOOL pushStyle(StyleStack *s, StyleType style) {
    if (s->top >= (MAX_STYLE_STACK - 1)) {
        return FALSE;
    }
    s->stack[++s->top] = style;
    return TRUE;
}

/**
 * Pop the top style from the stack.
 * @param s : the style stack
 * @param style : the style to pop
 * @return true if the top style was 'style' and was popped, false otherwise
 */
static BOOL popStyle(StyleStack *s, StyleType style) {
    if (s->top < 0) {
        return FALSE;
    }
    if (s->stack[s->top] == style) {
        s->top--;
        return TRUE;
    }
    return FALSE;
}

/**
 * Check if the stack is currently 'style' on top.
 * @return true if stack is currently 'style' on top
 */
static BOOL isTopStyle(const StyleStack *s, StyleType style) {
    if (s->top < 0)
        return FALSE;
    return (s->stack[s->top] == style);
}

static BOOL stackHasBold(const StyleStack *s) {
    int i;

    for (i = 0; i <= s->top; i++) {
        if (s->stack[i] == STYLE_BOLD) {
            return TRUE;
        }
    }
    return FALSE;
}

static void popBoldRun(StyleStack *s) {
    while (s->top >= 0 && s->stack[s->top] != STYLE_BOLD) {
        s->top--;
    }
    if (s->top >= 0) {
        s->top--;
    }
}

/**
 * Output MUI escape codes for turning on/off styles:
 *   \033b = bold on
 *   \033i = italic on
 *   \033u = underline on
 *   \033n = normal (off)
 *
 * @param out : output buffer
 * @param outSize : size of the output buffer
 * @param style : style to turn on
 */
static void outputStyleOn(STRPTR out, size_t outSize, StyleType style) {
    switch (style) {
    case STYLE_BOLD:
        strbufAppend(out, (ULONG)outSize, "\033b");
        break;
    case STYLE_ITALIC:
        strbufAppend(out, (ULONG)outSize, "\033i");
        break;
    case STYLE_UNDERLINE:
        strbufAppend(out, (ULONG)outSize, "\033u");
        break;
    }
}

/*
 * Output MUI escape code to turn off all styles.
 * @param out : output buffer
 * @param outSize : size of the output buffer
 */
static void outputStyleOff(STRPTR out, size_t outSize) {
    /*
       MCC_NList's docs say \033n sets the soft style back to normal,
       thus turning off bold/italic/underline.
    */
    strbufAppend(out, (ULONG)outSize, "\033n");
}

/*
 * Attempt to parse the next formatting marker:
 *  - "**" => toggles bold
 *  - "*"  => toggles italic
 *  - "__" => toggles underline
 *  - "_"  => toggles italic (some Markdown variants do this),
 *            but we'll skip that unless you truly want single underscore
 *            for italic. We'll assume only * for italic, __ for underline.
 *
 * @param input : the input string
 * @param pos : current position in the input string
 * @param len : length of the input string
 * @param foundStyle : output parameter for the style found
 * @return :
 *   0 if no recognised marker
 *   2 if recognized a 2-char marker (e.g., "**", "__")
 *   1 if recognized a single-char marker (e.g., "*")
 *
 * Also outputs which style it corresponds to in 'foundStyle'.
 */
static UBYTE parseMarker(CONST_STRPTR input, size_t pos, size_t len,
                         StyleType *foundStyle, const StyleStack *stack) {
    if (pos >= len) {
        return 0;
    }

    // Check for "__" (underline)
    if (pos + 1 < len && input[pos] == '_' && input[pos + 1] == '_') {
        *foundStyle = STYLE_UNDERLINE;
        return 2;
    }
    if (pos + 1 < len && input[pos] == '*' && input[pos + 1] == '*') {
        if (stackHasBold(stack)) {
            if (!chatMdBoldDoubleStarCanClose((const char *)input, (ULONG)pos,
                                              (ULONG)len)) {
                return 0;
            }
        } else if (!chatMdBoldDoubleStarCanOpen((const char *)input, (ULONG)pos,
                                                (ULONG)len)) {
            return 0;
        }
        *foundStyle = STYLE_BOLD;
        return 2;
    }
    if (input[pos] == '*') {
        BOOL closing = isTopStyle(stack, STYLE_ITALIC);

        if (closing ? chatMdItalicStarCanClose((const char *)input, (ULONG)pos,
                                               (ULONG)len)
                    : chatMdItalicStarCanOpen((const char *)input, (ULONG)pos,
                                              (ULONG)len)) {
            *foundStyle = STYLE_ITALIC;
            return 1;
        }
        return 0;
    }
    return 0;
}

/**
 * Convert a (mini-)Markdown string with possible nesting of
 *   **bold**, *italic*, __underline__
 * to MUI/NList escape codes with a proper stack approach.
 *
 * - Supports nesting, e.g. **bold *italic** inside**
 * - Supports escaping:
 *      \*, \_, \\, so they're inserted literally.
 *
 * @param input the input string
 * @return a newly allocated string. Caller must FreeVec() it.
 */
static STRPTR convertMarkdownFormattingToMUI(CONST_STRPTR input) {
    if (!input)
        return NULL;

    // Heuristic for output expansion: each marker can expand
    // to ~4 chars ("\033b" or "\033n"), so let's be generous.
    size_t inLen = strlen(input);
    size_t outCap = inLen * 6 + 128;
    STRPTR out = AllocVec(outCap, MEMF_ANY | MEMF_CLEAR);
    if (!out)
        return NULL;

    StyleStack styleStack;
    initStyleStack(&styleStack);

    size_t i = 0;
    while (i < inLen) {
        // 1) Check for escapes: "\"
        if (input[i] == '\\') {
            // If we have a next char, output it literally and skip
            if (i + 1 < inLen) {
                // Next char is literal
                size_t outLen = strlen(out);
                if (outLen < outCap - 2) {
                    out[outLen] = input[i + 1];
                    out[outLen + 1] = '\0';
                }
                i += 2;
            } else {
                // Just a trailing backslash alone:
                // output it or ignore, your choice. We'll just ignore it.
                i++;
            }
            continue;
        }

        // 2) Try to parse a formatting marker
        StyleType styleFound;
        UBYTE markerLen = parseMarker(input, i, inLen, &styleFound, &styleStack);
        if (markerLen > 0) {
            if (styleFound == STYLE_BOLD) {
                if (stackHasBold(&styleStack)) {
                    popBoldRun(&styleStack);
                    outputStyleOff(out, outCap);
                    for (UWORD s = 0; s <= styleStack.top; s++) {
                        outputStyleOn(out, outCap, styleStack.stack[s]);
                    }
                } else if (pushStyle(&styleStack, STYLE_BOLD)) {
                    outputStyleOn(out, outCap, STYLE_BOLD);
                } else {
                    size_t availableSpace = outCap - strlen(out) - 1;
                    size_t copyLen = (markerLen < availableSpace)
                                         ? markerLen
                                         : availableSpace;

                    UBYTE tempBuf[256];
                    if (copyLen > sizeof(tempBuf) - 1) {
                        copyLen = sizeof(tempBuf) - 1;
                    }

                    memcpy(tempBuf, &input[i], copyLen);
                    tempBuf[copyLen] = '\0';

                    strbufAppend(out, (ULONG)outCap, (STRPTR)tempBuf);
                }
            } else if (isTopStyle(&styleStack, styleFound)) {
                popStyle(&styleStack, styleFound);
                outputStyleOff(out, outCap);

                for (UWORD s = 0; s <= styleStack.top; s++) {
                    outputStyleOn(out, outCap, styleStack.stack[s]);
                }
            } else {
                if (pushStyle(&styleStack, styleFound)) {
                    outputStyleOn(out, outCap, styleFound);
                } else {
                    // We've run out of stack space; treat as literal
                    // We'll just insert the raw marker:
                    size_t availableSpace = outCap - strlen(out) - 1;
                    size_t copyLen = (markerLen < availableSpace)
                                         ? markerLen
                                         : availableSpace;

                    UBYTE tempBuf[256];
                    if (copyLen > sizeof(tempBuf) - 1) {
                        copyLen = sizeof(tempBuf) - 1;
                    }

                    memcpy(tempBuf, &input[i], copyLen);
                    tempBuf[copyLen] = '\0';

                    strbufAppend(out, (ULONG)outCap, (STRPTR)tempBuf);
                }
            }
            i += markerLen;
        } else {
            // 3) Not a marker, just a normal character
            size_t outLen = strlen(out);
            if (outLen < outCap - 2) {
                out[outLen] = input[i];
                out[outLen + 1] = '\0';
            }
            i++;
        }
    }

    // If any styles remain open, we can close them
    while (styleStack.top >= 0) {
        popStyle(&styleStack, styleStack.stack[styleStack.top]);
        outputStyleOff(out, outCap);
        // Re-enable anything that might remain on the stack
        for (int s = 0; s <= styleStack.top; s++) {
            outputStyleOn(out, outCap, styleStack.stack[s]);
        }
    }

    return out;
}

static BOOL ensureChatOutputBufferCapacity(ULONG required) {
    if (chatOutputTextEditorContents == NULL)
        return FALSE;
    if (required <= chatOutputTextEditorContentsCapacity)
        return TRUE;

    ULONG newCapacity = chatOutputTextEditorContentsCapacity;
    while (newCapacity < required) {
        if (newCapacity > 8 * 1024 * 1024) {
            break;
        }
        newCapacity *= 2;
    }
    if (newCapacity < required) {
        return FALSE;
    }

    STRPTR bigger = AllocVec(newCapacity, MEMF_ANY | MEMF_CLEAR);
    if (bigger == NULL) {
        return FALSE;
    }
    strncpy(bigger, chatOutputTextEditorContents,
            chatOutputTextEditorContentsCapacity - 1);
    set(chatOutputTextEditor, MUIA_NFloattext_Text, "");
    FreeVec(chatOutputTextEditorContents);
    chatOutputTextEditorContents = bigger;
    chatOutputTextEditorContentsCapacity = newCapacity;
    return TRUE;
}

/**
 * Prepare assistant text for NFloattext. Returns a newly AllocVec'd string,
 * or NULL if input is NULL or allocation fails. Caller must FreeVec() when
 * non-NULL.
 */
static STRPTR formatAssistantTextForDisplay(CONST_STRPTR input) {
    STRPTR out;
    size_t len;

    if (input == NULL) {
        return NULL;
    }
    if (!configGetMarkdownFormatting()) {
        len = strlen(input);
        out = AllocVec(len + 1, MEMF_CLEAR);
        if (out == NULL) {
            return NULL;
        }
        if (len > 0) {
            CopyMem(input, out, len);
        }
        out[len] = '\0';
        return out;
    }
    return convertMarkdownFormattingToMUI(input);
}

#ifdef __MORPHOS__

static void chatOutputFillRoleStyles(struct Conversation *conversation,
                                     UBYTE *styles, ULONG bufLen) {
    struct ConversationNode *conversationNode;
    UTF8 *content;
    ULONG contentLen;
    ULONG pos = 0;
    UBYTE styleByte;

    if (styles == NULL || conversation == NULL || bufLen == 0) {
        return;
    }

    memset(styles, 0, bufLen);

    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        content = conversationNodeGetDisplay(conversationNode);
        if (content == NULL) {
            content = (UTF8 *)"";
        }
        contentLen = (ULONG)strlen((const char *)content);

        if (pos > 0) {
            if (pos + 2 > bufLen) {
                return;
            }
            pos += 2;
        }

        styleByte =
            (strcmp(conversationNode->role, "user") == 0) ? (UBYTE)1 : (UBYTE)0;

        if (pos + contentLen > bufLen) {
            if (pos < bufLen) {
                memset(styles + pos, styleByte, bufLen - pos);
            }
            return;
        }
        memset(styles + pos, styleByte, contentLen);
        pos += contentLen;
    }
}

static BOOL mainWindowShuttingDown = FALSE;

BOOL mainWindowMorphosPrepareShutdownActive(void) { return mainWindowShuttingDown; }

void chatOutputUpdateFromBuffer(BOOL preserveViewport) {
    ULONG textLen;
    UBYTE *roleStyles = NULL;

    if (mainWindowShuttingDown || mainWindowObject == NULL ||
        chatOutputTextEditor == NULL || chatOutputTextEditorContents == NULL) {
        return;
    }

    textLen = (ULONG)strlen(chatOutputTextEditorContents);
    if (textLen == 0) {
        streamLogLifecycle("chatOutputUpdateFromBuffer empty");
        chatOutputScintillaSetUtf8TextWithRoleStyles(chatOutputTextEditor, "", NULL, 0,
                                                     preserveViewport);
#ifdef __MORPHOS__
        chatUserNavClear();
        chatFindScintillaUpdateCounter(chatOutputTextEditor);
#endif
        return;
    }

    if (currentConversation != NULL) {
        roleStyles = (UBYTE *)AllocVec(textLen, MEMF_ANY);
        if (roleStyles != NULL) {
            char *displayText = NULL;
            UBYTE *displayStyles = NULL;

            streamLogLifecycle("chatOutputUpdateFromBuffer roleStyles begin");
            chatOutputFillRoleStyles(currentConversation, roleStyles, textLen);
            streamLogLifecycle("chatOutputUpdateFromBuffer roleStyles done");

#ifdef __MORPHOS__
            {
                BOOL morphosUseRawRefresh =
                    !configGetMarkdownFormatting() ||
                    morphosChatStreamRawScintillaRefresh;

                if (morphosUseRawRefresh) {
                    if (morphosChatStreamRawScintillaRefresh) {
                        streamLogLifecycle(
                            "chatOutput refresh stream raw path");
                        chatOutputScintillaMorphosSkipViewport = TRUE;
                        if (chatOutputScintillaAppendStreamDelta(
                                chatOutputTextEditor, chatOutputTextEditorContents,
                                textLen, roleStyles, textLen)) {
                            streamLogLifecycle(
                                "chatOutputUpdateFromBuffer scintilla append done");
#ifdef __MORPHOS__
                            chatUserNavRebuild(roleStyles, textLen);
                            chatFindScintillaUpdateCounter(chatOutputTextEditor);
#endif
                            FreeVec(roleStyles);
                            return;
                        }
                        streamLogLifecycle(
                            "chat stream append fallback settext");
                    } else {
                        streamLogLifecycle("chatOutput refresh raw path");
                        chatOutputScintillaMorphosSkipViewport =
                            chatOutputMorphosListRefreshActive;
                        chatOutputScintillaAugmentStyleBytesHotspots(
                            chatOutputTextEditorContents, roleStyles, textLen);
                    }
                    streamLogLifecycle(
                        "chatOutputUpdateFromBuffer scintilla raw begin");
                    chatOutputScintillaForgetMarkdownLinkSpans();
                    chatOutputScintillaSetUtf8TextWithRoleStyles(
                        chatOutputTextEditor, chatOutputTextEditorContents,
                        roleStyles, textLen, preserveViewport);
                    streamLogLifecycle(
                        "chatOutputUpdateFromBuffer scintilla raw done");
#ifdef __MORPHOS__
                    chatUserNavRebuild(roleStyles, textLen);
                    chatFindScintillaUpdateCounter(chatOutputTextEditor);
#endif
                    FreeVec(roleStyles);
                    return;
                }
                if (chatOutputMorphosListRefreshActive) {
                    streamLogLifecycle("chatOutput refresh list markdown path");
                } else {
                    streamLogLifecycle("chatOutput refresh markdown path");
                }
            }
#endif

            displayText = (char *)AllocVec((textLen * 2) + 1, MEMF_ANY);
            displayStyles = (UBYTE *)AllocVec(textLen * 2, MEMF_ANY);

            if (displayText != NULL && displayStyles != NULL) {
                ULONG displayCap = textLen * 2;

                streamLogLifecycle("chatOutputUpdateFromBuffer markdown begin");
                textLen = chatOutputScintillaBuildMidiMarkdownDisplay(
                    chatOutputTextEditorContents, roleStyles, textLen,
                    displayText, displayStyles);
                streamLogLifecycle("chatOutputUpdateFromBuffer markdown done");
                streamLogLifecycle("chatOutputUpdateFromBuffer pipeTables begin");
                textLen = chatOutputScintillaFormatPipeTables(
                    displayText, displayStyles, textLen, displayCap);
                streamLogLifecycle("chatOutputUpdateFromBuffer pipeTables done");
                streamLogLifecycle("chatOutputUpdateFromBuffer scintilla set begin");
                chatOutputScintillaSetUtf8TextWithRoleStyles(
                    chatOutputTextEditor, displayText, displayStyles, textLen,
                    preserveViewport);
                streamLogLifecycle("chatOutputUpdateFromBuffer scintilla set done");
#ifdef __MORPHOS__
                chatUserNavRebuild(displayStyles, textLen);
                chatFindScintillaUpdateCounter(chatOutputTextEditor);
#endif
                FreeVec(displayText);
                FreeVec(displayStyles);
            } else {
                if (displayText != NULL) {
                    FreeVec(displayText);
                }
                if (displayStyles != NULL) {
                    FreeVec(displayStyles);
                }
                streamLogLifecycle("chatOutputUpdateFromBuffer scintilla fallback begin");
                chatOutputScintillaForgetMarkdownLinkSpans();
                chatOutputScintillaAugmentStyleBytesHotspots(
                    chatOutputTextEditorContents, roleStyles, textLen);
                chatOutputScintillaSetUtf8TextWithRoleStyles(
                    chatOutputTextEditor, chatOutputTextEditorContents,
                    roleStyles, textLen, preserveViewport);
                streamLogLifecycle("chatOutputUpdateFromBuffer scintilla fallback done");
#ifdef __MORPHOS__
                chatUserNavRebuild(roleStyles, textLen);
                chatFindScintillaUpdateCounter(chatOutputTextEditor);
#endif
            }

            FreeVec(roleStyles);
            return;
        }
        streamLogLifecycle("chatOutputUpdateFromBuffer roleStyles alloc fail");
    }

    streamLogLifecycle("chatOutputUpdateFromBuffer scintilla plain begin");
    chatOutputScintillaSetUtf8TextWithRoleStyles(
        chatOutputTextEditor, chatOutputTextEditorContents, roleStyles, textLen,
        preserveViewport);
    streamLogLifecycle("chatOutputUpdateFromBuffer scintilla plain done");

    if (roleStyles != NULL) {
        FreeVec(roleStyles);
    }
}

static void clearChatOutputDisplay(void) {
    if (chatOutputTextEditorContents != NULL) {
        chatOutputTextEditorContents[0] = '\0';
    }
    chatOutputUpdateFromBuffer(FALSE);
}

#else /* !__MORPHOS__ */

/* Phase 7c: mouse wheel on chat NFloattext (HandleInput + window EH on same class) */
static struct MUI_CustomClass *chatOutputFloattextClass;
static struct MUI_EventHandlerNode *chatOutputWheelEH;
static BOOL chatOutputWheelEHInstalled;

static LONG chatOutputWheelScrollCommand(struct MUIP_HandleInput *msg) {
    if (msg == NULL) {
        return 0;
    }
    if (msg->muikey == MUIKEY_UP) {
        return MUIV_NList_First_Up;
    }
    if (msg->muikey == MUIKEY_DOWN) {
        return MUIV_NList_First_Down;
    }
    if (msg->muikey == MUIKEY_PAGEUP) {
        return MUIV_NList_First_PageUp;
    }
    if (msg->muikey == MUIKEY_PAGEDOWN) {
        return MUIV_NList_First_PageDown;
    }
    if (msg->imsg != NULL) {
        BOOL shift =
            (msg->imsg->Qualifier &
             (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
        UWORD code = msg->imsg->Code;

        if (msg->imsg->Class == IDCMP_RAWKEY ||
            msg->imsg->Class == IECLASS_RAWKEY ||
            msg->imsg->Class == IECLASS_NEWMOUSE) {
            switch (code) {
            case NM_WHEEL_UP:
                return shift ? MUIV_NList_First_PageUp : MUIV_NList_First_Up;
            case NM_WHEEL_DOWN:
                return shift ? MUIV_NList_First_PageDown
                             : MUIV_NList_First_Down;
            default:
                break;
            }
        }
    }
    return 0;
}

SAVEDS static ULONG ChatOutputFloattext_HandleInput(struct IClass *cl, Object *obj,
                                                   struct MUIP_HandleInput *msg) {
    LONG cmd = chatOutputWheelScrollCommand(msg);

    if (cmd != 0) {
        set(obj, MUIA_NList_First, cmd);
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, msg);
}

SAVEDS static ULONG ChatOutputFloattext_HandleEvent(struct IClass *cl, Object *obj,
                                                    struct MUIP_HandleEvent *msg) {
    struct MUIP_HandleInput inputMsg;
    LONG cmd;

    inputMsg.MethodID = MUIM_HandleInput;
    inputMsg.imsg = msg->imsg;
    inputMsg.muikey = msg->muikey;
    cmd = chatOutputWheelScrollCommand(&inputMsg);
    if (cmd != 0) {
        set(obj, MUIA_NList_First, cmd);
        return MUI_EventHandlerRC_Eat;
    }
    return DoSuperMethodA(cl, obj, msg);
}

DISPATCHER(ChatOutputFloattextDispatcher) {
    switch (msg->MethodID) {
    case MUIM_HandleInput:
        return ChatOutputFloattext_HandleInput(cl, obj, (APTR)msg);
    case MUIM_HandleEvent:
        return ChatOutputFloattext_HandleEvent(cl, obj, (APTR)msg);
    }
    return DoSuperMethodA(cl, obj, msg);
}

static LONG createChatOutputFloattextClass(void) {
    if (chatOutputFloattextClass != NULL) {
        return RETURN_OK;
    }
    chatOutputFloattextClass = MUI_CreateCustomClass(
        NULL, MUIC_NFloattext, NULL, 0, ENTRY(ChatOutputFloattextDispatcher));
    if (chatOutputFloattextClass == NULL) {
        return RETURN_ERROR;
    }
    chatOutputFloattextClass->mcc_Class->cl_ID =
        (ClassID) "AmigaGPTChatOutputFloattext";
    return RETURN_OK;
}

static void deleteChatOutputFloattextClass(void) {
    if (chatOutputFloattextClass != NULL) {
        MUI_DeleteCustomClass(chatOutputFloattextClass);
        chatOutputFloattextClass = NULL;
    }
}

static void installChatOutputWheelHandler(void) {
    if (chatOutputWheelEHInstalled || chatOutputTextEditor == NULL ||
        mainWindowObject == NULL || chatOutputFloattextClass == NULL) {
        return;
    }
    if (chatOutputWheelEH == NULL) {
        chatOutputWheelEH = (struct MUI_EventHandlerNode *)AllocVec(
            sizeof(struct MUI_EventHandlerNode), MEMF_PUBLIC | MEMF_CLEAR);
        if (chatOutputWheelEH == NULL) {
            return;
        }
        chatOutputWheelEH->ehn_Class = chatOutputFloattextClass->mcc_Class;
        chatOutputWheelEH->ehn_Object = chatOutputTextEditor;
        chatOutputWheelEH->ehn_Events = IDCMP_RAWKEY;
        chatOutputWheelEH->ehn_Flags = MUI_EHF_GUIMODE;
        chatOutputWheelEH->ehn_Priority = 100;
    }
    DoMethod(mainWindowObject, MUIM_Window_AddEventHandler, chatOutputWheelEH);
    chatOutputWheelEHInstalled = TRUE;
}

void chatOutputWheelShutdown(void) {
    streamLogLifecycle("chatOutputWheelShutdown begin");
    if (chatOutputWheelEHInstalled && mainWindowObject != NULL &&
        chatOutputWheelEH != NULL) {
        DoMethod(mainWindowObject, MUIM_Window_RemEventHandler, chatOutputWheelEH);
    }
    chatOutputWheelEHInstalled = FALSE;
    if (chatOutputWheelEH != NULL) {
        FreeVec(chatOutputWheelEH);
        chatOutputWheelEH = NULL;
    }
    /* Custom class must outlive chatOutputTextEditor until MUI_DisposeObject(app). */
    streamLogLifecycle("chatOutputWheelShutdown done");
}

void chatOutputWheelDisposeClass(void) {
    deleteChatOutputFloattextClass();
}

static Object *newChatOutputFloattextObject(void) {
    if (createChatOutputFloattextClass() == RETURN_OK) {
        return (Object *)NewObject(chatOutputFloattextClass->mcc_Class, NULL,
            MUIA_Font,
            configGetFixedWidthFonts() ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
            MUIA_Frame, MUIV_Frame_Text, MUIA_ContextMenu, NULL,
            MUIA_NFloattext_Text, chatOutputTextEditorContents, TAG_DONE);
    }
    return (Object *)MUI_NewObject(
        MUIC_NFloattext, MUIA_Font,
        configGetFixedWidthFonts() ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
        MUIA_Frame, MUIV_Frame_Text, MUIA_ContextMenu, NULL,
        MUIA_NFloattext_Text, chatOutputTextEditorContents, TAG_DONE);
}

#endif /* !__MORPHOS__ */

#ifdef __MORPHOS__
void chatOutputWheelShutdown(void) {}
void chatOutputWheelDisposeClass(void) {}

void applyFixedWidthFontsSetting(void) {
    applyChatFontSetting();
}

void applyChatFontSetting(void) {
    if (chatOutputTextEditor == NULL || mainWindowIsShuttingDown()) {
        return;
    }
    streamLogLifecycle("chat font apply begin");
    chatOutputScintillaCancelDeferredStyles();
    chatOutputScintillaRefreshFont(chatOutputTextEditor);
    streamLogLifecycle("chat font apply done");
}
#endif

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow() {
    streamLogBootPhase("createMainWindow start");
    streamLogLifecycle("createMainWindow start");

    if ((isMUI5 || isMUI39) && createAmigaGPTTextEditor() == RETURN_ERROR) {
        displayError("Could not create custom class.");
    }
#ifndef __MORPHOS__
    (void)createChatOutputFloattextClass();
#else
    if (!chatFindScintillaInit()) {
        streamLogLifecycle("createMainWindow chat find init failed");
    }
#endif

    if (mainWindowObject != NULL) {
#ifdef __MORPHOS__
        chatOutputScintillaDetachNotify();
#endif
        MUI_DisposeObject(mainWindowObject);
    }

    // clang-format off
    if (isMUI5 || isMUI39) {
        chatInputTextEditor = NewObject(
            MUIC_AmigaGPTTextEditor, NULL,
            TextFrame,
            MUIA_Background, MUII_BACKGROUND,
            MUIA_ObjectID, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
            MUIA_AmigaGPTTextEditor_SubmitHook, &SendMessageButtonClickedHook,
            isAROS ? TAG_DONE : TAG_SKIP, NULL,
#ifndef __MORPHOS__
            MUIA_TextEditor_FixedFont, configGetFixedWidthFonts(),
#endif
            MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);

        imageInputTextEditor = NewObject(
            MUIC_AmigaGPTTextEditor, NULL,
            MUIA_Weight, 80,
            MUIA_AmigaGPTTextEditor_SubmitHook, &CreateImageButtonClickedHook,
            isAROS ? TAG_DONE : TAG_SKIP, NULL,
#ifndef __MORPHOS__
            MUIA_TextEditor_FixedFont, configGetFixedWidthFonts(),
#endif
            MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);
    } else {
        chatInputTextEditor = MUI_NewObject(
            isAROS ? MUIC_BetterString : MUIC_TextEditor,
            TextFrame,
            MUIA_Background, MUII_BACKGROUND,
            MUIA_ObjectID, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
            isAROS ? TAG_DONE : TAG_SKIP, NULL,
#ifndef __MORPHOS__
            MUIA_TextEditor_FixedFont, configGetFixedWidthFonts(),
#endif
            MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);

        imageInputTextEditor = MUI_NewObject(
            isAROS ? MUIC_BetterString : MUIC_TextEditor,
            TextFrame,
            MUIA_Background, MUII_BACKGROUND,
            MUIA_Weight, 80,
            isAROS ? TAG_DONE : TAG_SKIP, NULL,
#ifndef __MORPHOS__
            MUIA_TextEditor_FixedFont, configGetFixedWidthFonts(),
#endif
            MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);
    }
    // clang-format on

    createMenu();

    if (chatOutputTextEditorContents == NULL) {
        chatOutputTextEditorContents = AllocVec(
            CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, MEMF_ANY | MEMF_CLEAR);
        chatOutputTextEditorContentsCapacity =
            CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH;
    }

    pages[0] = STRING_CHAT_MODE;
    pages[1] = STRING_IMAGE_GENERATION_MODE;

    streamLogLifecycle("createMainWindow WindowObject begin");
    if ((mainWindowObject = WindowObject,
            MUIA_Window_Title, STRING_APP_NAME,
            MUIA_Window_ID, OBJECT_ID_MAIN_WINDOW,
            MUIA_Window_CloseGadget, TRUE,
            MUIA_Window_SizeGadget, TRUE,
            MUIA_Window_DepthGadget, TRUE,
            MUIA_Window_DragBar, TRUE,
            MUIA_Window_LeftEdge, MUIV_Window_LeftEdge_Centered,
            MUIA_Window_TopEdge, MUIV_Window_TopEdge_Centered,
            MUIA_Window_Width, MUIV_Window_Width_Screen(90),
            MUIA_Window_Height, MUIV_Window_Height_Screen(90),
            MUIA_Window_Menustrip, menuStrip,
            MUIA_Window_SizeRight, TRUE,
            MUIA_Window_UseBottomBorderScroller, FALSE,
            MUIA_Window_UseRightBorderScroller, FALSE,
            MUIA_Window_UseLeftBorderScroller, FALSE,
            WindowContents, VGroup,
                Child, modeRegisterGroup = RegisterGroup(pages),
                    Child, HGroup,
                        Child, VGroup, MUIA_Weight, 30,
                            // New chat button
                            Child, newChatButton = MUI_MakeObject(MUIO_Button, STRING_NEW_CHAT,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Delete chat button
                            Child, deleteChatButton = MUI_MakeObject(MUIO_Button, STRING_DELETE_CHAT,
                                MUIA_Background, MUII_FILL,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Conversation list
                            Child, NListviewObject,
                                MUIA_CycleChain, 1,
                                MUIA_NListview_NList, conversationListObject = NListObject,
                                    MUIA_ContextMenu, NULL,
                                    MUIA_NList_DefaultObjectOnClick, TRUE,
                                    MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
                                    MUIA_NList_ConstructHook2, &ConstructConversationLI_TextHook,
                                    MUIA_NList_DestructHook2, &DestructConversationLI_TextHook,
                                    MUIA_NList_DisplayHook2, &DisplayConversationLI_TextHook,
                                    MUIA_NList_Format, "BAR MINW=100 MAXW=200",
                                    MUIA_NList_AutoVisible, TRUE,
                                    MUIA_NList_TitleSeparator, FALSE,
                                    MUIA_NList_Title, FALSE,
                                    MUIA_NList_MinColSortable, 0,
                                    MUIA_NList_Imports, MUIV_NList_Imports_All,
                                    MUIA_NList_Exports, MUIV_NList_Exports_All,
                                End,
                            End,
                        End,
                        Child, VGroup,
                            // Chat output text display
                            Child, HGroup, MUIA_VertWeight, 60,
#ifdef __MORPHOS__
                                Child, VGroup,
                                    Child, chatFindScintillaGetBar(),
                                    Child, chatOutputScroller = ScrollgroupObject,
                                    MUIA_Scrollgroup_AutoBars, TRUE,
                                    MUIA_Scrollgroup_Contents,
                                        chatOutputTextEditor = ScintillaObject,
                                        MUIA_Frame, MUIV_Frame_Text,
                                    End,
                                End,
                                End,
#else
                                Child, chatOutputListView = NListviewObject,
                                MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_None,
                                MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                                MUIA_NListview_NList,
                                    chatOutputTextEditor =
                                        newChatOutputFloattextObject(),
                                End,
#endif
                            End,
                            Child, HGroup, MUIA_VertWeight, 20,
                                // Chat input text editor
                                Child, chatInputTextEditor,
                                // Send message button
                                Child, VGroup, MUIA_HorizWeight, 10,
                                    Child, sendMessageButton = MUI_MakeObject(MUIO_Button, STRING_SEND,
                                        MUIA_ObjectID, OBJECT_ID_SEND_MESSAGE_BUTTON,
                                        MUIA_CycleChain, TRUE,
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                    End,
                                    Child, stopSpeakingButton = MUI_MakeObject(MUIO_Button, STRING_STOP_SPEAKING,
                                        MUIA_CycleChain, TRUE,
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                    End,
                                End,
                            End,
                        End,
                    End,
                    Child, HGroup,
                        GroupFrame,
                        Child, VGroup,
                            // New image button
                            Child, newImageButton = MUI_MakeObject(MUIO_Button, STRING_NEW_IMAGE,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Delete image button
                            Child, deleteImageButton = MUI_MakeObject(MUIO_Button, STRING_DELETE_IMAGE,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            // Image list
                            Child, NListviewObject,
                                MUIA_CycleChain, 1,
                                MUIA_NListview_NList, imageListObject = NListObject,
                                    MUIA_ContextMenu, NULL,
                                    MUIA_NList_DefaultObjectOnClick, TRUE,
                                    MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
                                    MUIA_NList_ConstructHook2, &ConstructImageLI_TextHook,
                                    MUIA_NList_DestructHook2, &DestructImageLI_TextHook,
                                    MUIA_NList_DisplayHook2, &DisplayImageLI_TextHook,
                                    MUIA_NList_Format, "BAR MINW=100 MAXW=200",
                                    MUIA_NList_AutoVisible, TRUE,
                                    MUIA_NList_TitleSeparator, FALSE,
                                    MUIA_NList_Title, FALSE,
                                    MUIA_NList_MinColSortable, 0,
                                    MUIA_NList_Imports, MUIV_NList_Imports_All,
                                    MUIA_NList_Exports, MUIV_NList_Exports_All,
                                End,
                            End,
                        End,
                        Child, imageViewGroup = VGroup, MUIA_Weight, 220,
                            // Image view
                            Child, imageView = RectangleObject,
                                MUIA_Frame, MUIV_Frame_ImageButton,
                            End,
                            Child, HGroup,
                                // Open image button
                                Child, openImageButton = MUI_MakeObject(MUIO_Button, STRING_OPEN_IMAGE,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                // Save image copy button
                                Child, saveImageCopyButton = MUI_MakeObject(MUIO_Button, STRING_SAVE_IMAGE_COPY,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                            End,
                            Child, HGroup,
                                // Image input text editor
                                Child, imageInputTextEditor,
                                Child, VGroup,
                                    MUIA_Weight, 20,
                                    // Create image button
                                    Child, createImageButton = MUI_MakeObject(MUIO_Button, STRING_CREATE_IMAGE,
                                        MUIA_Weight, 20,
                                        MUIA_CycleChain, TRUE,
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                    TAG_DONE),
                                End,
                            End,
                        End,
                    End,
                End,
                // Status bar
                Child, statusBar = TextObject,
                    MUIA_Background, MUII_SHADOW,
                    MUIA_Text_Contents, STRING_READY,
                End,
                // Loading bar: blank page while idle, busy meter page while a
                // request is in flight, so it no longer stays frozen on its
                // last position once a request finishes.
                Child, loadingBarGroup = PageGroup, MUIA_VertWeight, 10,
                    MUIA_MaxHeight, 20,
                    MUIA_Group_ActivePage, 0,
                    Child, HVSpace,
                    Child, loadingBar = BusyObject,
                        MUIA_Busy_Speed, MUIV_Busy_Speed_Off,
                    End,
                End,
            End,
        End) == NULL) {
        streamLogLifecycle("createMainWindow WindowObject fail");
        streamLogStartupPhase("createMainWindow fail");
        // clang-format on
        displayError(STRING_ERROR_MAIN_WINDOW);
        return RETURN_ERROR;
    }
    streamLogLifecycle("createMainWindow WindowObject ok");

    get(mainWindowObject, MUIA_Window, &mainWindow);
#ifdef __MORPHOS__
    streamLogLifecycle("createMainWindow codeBlocksViewerSetAslParentWindow begin");
    codeBlocksViewerSetAslParentWindow(mainWindow);
    streamLogLifecycle("createMainWindow codeBlocksViewerSetAslParentWindow done");
    streamLogLifecycle("createMainWindow chat scintilla prime begin");
    chatOutputScintillaPrimeViewer(chatOutputTextEditor);
    streamLogLifecycle("createMainWindow chat scintilla prime done");
#endif

    streamLogLifecycle("createMainWindow OM_ADDMEMBER begin");
    DoMethod(app, OM_ADDMEMBER, mainWindowObject);
    streamLogLifecycle("createMainWindow OM_ADDMEMBER done");

    addMainWindowActions();

#ifdef __MORPHOS__
    chatFindScintillaSetContext(mainWindowObject, chatOutputTextEditor);
    chatFindScintillaWireToApp(app);
#endif

    /* Once before open; second Load after loadConversations broke NList on restart. */
    streamLogLifecycle("createMainWindow Application_Load begin");
    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
    streamLogLifecycle("createMainWindow Application_Load done");
#ifdef __MORPHOS__
    /* ENVARC must not reopen code viewer before Scintilla is fully ready. */
    if (codeBlocksWindowObject != NULL) {
        streamLogLifecycle("createMainWindow codeBlocksWindow close begin");
        set(codeBlocksWindowObject, MUIA_Window_Open, FALSE);
        streamLogLifecycle("createMainWindow codeBlocksWindow close done");
    }
    streamLogLifecycle("createMainWindow chat scintilla finish deferred");
#endif
    set(mainWindowObject, MUIA_Window_Open, TRUE);
#ifdef __MORPHOS__
    DoMethod(mainWindowObject, MUIM_Window_ToFront, NULL);
#endif
    streamLogBootPhase("main window open");
    streamLogLifecycle("createMainWindow window open done");
    addMenuActions();
    
    // Allocate pens after window is opened
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    if (currentScreen) {
        redPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF, 0,
                               0, OBP_Precision, PRECISION_GUI, TAG_DONE);
        greenPen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0xBBBBBBBB, 0,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        bluePen =
            ObtainBestPen(currentScreen->ViewPort.ColorMap, 0, 0, 0xFFFFFFFF,
                          OBP_Precision, PRECISION_GUI, TAG_DONE);
        yellowPen = ObtainBestPen(currentScreen->ViewPort.ColorMap, 0xFFFFFFFF,
                                  0xFFFFFFFF, 0, OBP_Precision, PRECISION_GUI,
                                  TAG_DONE);

        // Now set up the button labels with colors
        const UBYTE BUTTON_LABEL_BUFFER_SIZE = 64;
        STRPTR buttonLabelText = AllocVec(BUTTON_LABEL_BUFFER_SIZE, MEMF_ANY);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW_CHAT);
        set(newChatButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_CHAT);
        set(deleteChatButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
                 bluePen, STRING_SEND);
        set(sendMessageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
                 redPen, STRING_STOP_SPEAKING);
        set(stopSpeakingButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW_IMAGE);
        set(newImageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_IMAGE);
        set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]%s\0",
                 bluePen, STRING_CREATE_IMAGE);
        set(createImageButton, MUIA_Text_Contents, buttonLabelText);
        FreeVec(buttonLabelText);

        // Force a layout refresh to ensure buttons display correctly
        DoMethod(mainWindowObject, MUIM_Group_InitChange);
        DoMethod(mainWindowObject, MUIM_Group_ExitChange);
    }
    
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    
    updateStatusBar(STRING_READY, greenPen);

    streamLogBootPhase("loadConversations call");
    streamLogLifecycle("createMainWindow loadConversations begin");
    loadConversations();
    streamLogBootPhase("loadConversations done");
    streamLogLifecycle("createMainWindow loadConversations done");
    restoreLastSelectedConversation();
    loadImages();
    streamLogBootPhase("createMainWindow ok");
    streamLogLifecycle("createMainWindow ok");

#ifndef __MORPHOS__
    installChatOutputWheelHandler();
#endif

    return RETURN_OK;
}

static void mainWindowReleasePens(void) {
    struct Screen *currentScreen = NULL;

    if (mainWindowObject == NULL) {
        redPen = greenPen = bluePen = yellowPen = 0;
        return;
    }

    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    if (currentScreen != NULL) {
        if (redPen) {
            ReleasePen(currentScreen->ViewPort.ColorMap, redPen);
        }
        if (greenPen) {
            ReleasePen(currentScreen->ViewPort.ColorMap, greenPen);
        }
        if (bluePen) {
            ReleasePen(currentScreen->ViewPort.ColorMap, bluePen);
        }
        if (yellowPen) {
            ReleasePen(currentScreen->ViewPort.ColorMap, yellowPen);
        }
    }
    redPen = greenPen = bluePen = yellowPen = 0;
}

static void mainWindowEmptyNList(Object *list) {
    if (list == NULL) {
        return;
    }
    set(list, MUIA_NList_Quiet, TRUE);
    DoMethod(list, MUIM_NList_Clear);
    set(list, MUIA_NList_Quiet, FALSE);
}

void mainWindowPrepareShutdown(void) {
    streamLogLifecycle("mainWindowPrepareShutdown begin");
    streamLogShutdownPhase("prepare shutdown begin");
    mainWindowShuttingDown = TRUE;
    mainWindowSignalQuit();
    openAIChatStreamRequestCancel();
    saveLastSelectedConversationName(currentConversation);
    currentConversation = NULL;
    currentImage = NULL;

#ifdef __MORPHOS__
    morphosConversationSelectEnabled = FALSE;
    conversationRowPending = NULL;
    codeBlocksViewerBeginShutdown();
    chatOutputScintillaCancelPendingCodeblockOpen();
    chatOutputScintillaCancelDeferredStyles();
    chatOutputRefreshPending = FALSE;
    chatOutputRefreshFromList = FALSE;
    morphosFlushPendingPushMethods();
    /* Close code viewer + drop chat notifies while main window is still open. */
    codeBlocksViewerCloseWindow();
    chatFindScintillaHide();
    chatUserNavClear();
    chatOutputScintillaQuiesceForShutdown(chatOutputTextEditor);
    chatOutputScintillaDetachNotify();
    /* No SCI_CLEARALL/SETTEXT on quit — can freeze MorphOS after styled large docs. */
    chatOutputScintillaForgetMarkdownLinkSpans();
#endif

    /* Pens must be released while the window/screen is still valid. */
    mainWindowReleasePens();

    mainWindowEmptyNList(conversationListObject);
    mainWindowEmptyNList(imageListObject);

    if (mainWindowObject != NULL) {
        set(mainWindowObject, MUIA_Window_Open, FALSE);
        DoMethod(mainWindowObject, MUIM_KillNotify, MUIA_Window_Screen);
        DoMethod(mainWindowObject, MUIM_KillNotify, MUIA_Window_CloseRequest);
    }
    streamLogLifecycle("mainWindowPrepareShutdown done");
    streamLogShutdownPhase("prepare shutdown done");
}

void mainWindowInvalidateAfterShutdown(void) {
    mainWindowShuttingDown = FALSE;
#ifdef __MORPHOS__
    morphosStartupDeferredDone = FALSE;
    morphosConversationSelectEnabled = FALSE;
    conversationRowPending = NULL;
    chatOutputRefreshPending = FALSE;
#endif
    mainWindow = NULL;
    mainWindowObject = NULL;
    newChatButton = NULL;
    deleteChatButton = NULL;
    sendMessageButton = NULL;
    chatInputTextEditor = NULL;
#ifndef __MORPHOS__
    chatOutputListView = NULL;
#endif
#ifdef __MORPHOS__
    chatOutputScroller = NULL;
#endif
    chatOutputTextEditor = NULL;
    statusBar = NULL;
    conversationListObject = NULL;
    loadingBar = NULL;
    imageInputTextEditor = NULL;
    createImageButton = NULL;
    newImageButton = NULL;
    deleteImageButton = NULL;
    imageListObject = NULL;
    imageView = NULL;
    imageViewGroup = NULL;
    openImageButton = NULL;
    saveImageCopyButton = NULL;
    modeRegisterGroup = NULL;
    currentConversation = NULL;
    currentImage = NULL;
}

/**
 * Add actions to the main window
 **/
static void addMainWindowActions() {
    DoMethod(newChatButton, MUIM_Notify, MUIA_Pressed, FALSE, newChatButton, 2,
             MUIM_CallHook, &NewChatButtonClickedHook);
    DoMethod(deleteChatButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 3, MUIM_CallHook,
             &DeleteChatButtonClickedHook, MUIV_TriggerValue);
    DoMethod(sendMessageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             sendMessageButton, 2, MUIM_CallHook,
             &SendMessageButtonClickedHook);
    DoMethod(stopSpeakingButton, MUIM_Notify, MUIA_Pressed, FALSE,
             stopSpeakingButton, 2, MUIM_CallHook,
             &StopSpeakingButtonClickedHook);
    DoMethod(createImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             createImageButton, 2, MUIM_CallHook,
             &CreateImageButtonClickedHook);
    DoMethod(openImageButton, MUIM_Notify, MUIA_Pressed, FALSE, openImageButton,
             2, MUIM_CallHook, &OpenImageButtonClickedHook);
    DoMethod(saveImageCopyButton, MUIM_Notify, MUIA_Pressed, FALSE,
             saveImageCopyButton, 2, MUIM_CallHook,
             &SaveImageCopyButtonClickedHook);
    DoMethod(conversationListObject, MUIM_Notify, MUIA_NList_EntryClick,
             MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook,
             &ConversationRowClickedHook, MUIV_TriggerValue);
    DoMethod(newImageButton, MUIM_Notify, MUIA_Pressed, FALSE, newImageButton,
             2, MUIM_CallHook, &NewImageButtonClickedHook);
    DoMethod(deleteImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 3, MUIM_CallHook,
             &DeleteImageButtonClickedHook, MUIV_TriggerValue);
    DoMethod(imageListObject, MUIM_Notify, MUIA_NList_EntryClick,
             MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook,
             &ImageRowClickedHook, MUIV_TriggerValue);
    DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);
}

static void streamUiResetRefreshClock(void) {
    streamUiLastRefreshValid = FALSE;
}

static BOOL streamUiShouldRefresh(UWORD chunkCount) {
    struct timeval now;

    if (chunkCount == 1) {
        gettimeofday(&streamUiLastRefresh, NULL);
        streamUiLastRefreshValid = TRUE;
        return TRUE;
    }
    if (!streamUiLastRefreshValid) {
        gettimeofday(&streamUiLastRefresh, NULL);
        streamUiLastRefreshValid = TRUE;
        return TRUE;
    }
    gettimeofday(&now, NULL);
    {
        long elapsedMs =
            (long)(now.tv_sec - streamUiLastRefresh.tv_sec) * 1000L +
            (long)(now.tv_usec - streamUiLastRefresh.tv_usec) / 1000L;

        if (elapsedMs >= STREAM_UI_MIN_REFRESH_MS) {
            streamUiLastRefresh = now;
            return TRUE;
        }
    }
    return FALSE;
}

static void streamUiFlushChatDisplay(void) {
#ifdef __MORPHOS__
    chatOutputUpdateFromBuffer(FALSE);
#else
    set(chatOutputTextEditor, MUIA_NFloattext_Text,
        chatOutputTextEditorContents);
    set(chatOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);
#endif
}

/** Speak only UTF-8 not yet spoken (system TTS); avoids full-buffer convert. */
static void speakStreamUtf8Tail(STRPTR receivedMessage, ULONG *utf8Spoken) {
    ULONG totalLen;
    STRPTR tailSystem;

    if (receivedMessage == NULL || utf8Spoken == NULL) {
        return;
    }
    totalLen = strlen(receivedMessage);
    if (*utf8Spoken >= totalLen) {
        return;
    }
    tailSystem = CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                                   (Tag)(receivedMessage + *utf8Spoken),
                                   CSA_MapForeignChars, TRUE, TAG_DONE);
    if (tailSystem != NULL && tailSystem[0] != '\0') {
        speakText(tailSystem, NULL, NULL);
        CodesetsFreeA(tailSystem, NULL);
    }
    *utf8Spoken = totalLen;
}

/**
 * Append one validated UTF-8 piece to the live chat view and receivedMessage.
 */
static void appendAssistantStreamText(STRPTR piece, STRPTR receivedMessage,
                                    UWORD *wordNumber, ULONG *speechUtf8Index) {
    size_t pieceLen;

    if (piece == NULL || piece[0] == '\0') {
        return;
    }

    pieceLen = strlen(piece);
    strbufAppend(receivedMessage, READ_BUFFER_LENGTH, piece);

#ifdef __MORPHOS__
    strbufAppend(chatOutputTextEditorContents,
                 CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, piece);
#else
    {
        STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)piece,
            CSA_MapForeignChars, TRUE, TAG_DONE);

        if (formattedMessageSystemEncoded != NULL) {
            strbufAppend(chatOutputTextEditorContents,
                         CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                         formattedMessageSystemEncoded);
            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
        }
    }
#endif

    ++(*wordNumber);
    if (*wordNumber == 1) {
        streamLogLifecycle("chat stream first chunk");
    }
    /*
     * OS3/OS4: markdown only after stream (displayConversation); avoids full-buffer
     * convertMarkdownFormattingToMUI on every chunk.
     * MorphOS: live refresh stays raw UTF-8 while morphosChatStreamRawScintillaRefresh
     * (Handlungsanweisung §5 / R3 — no Midi-Markdown or link-parse per chunk).
     */
    if (streamUiShouldRefresh(*wordNumber)) {
        streamUiFlushChatDisplay();
        if (configGetSpeechEnabled() &&
            configGetSpeechSystem() != SPEECH_SYSTEM_OPENAI &&
            configGetSpeechSystem() != SPEECH_SYSTEM_XAI) {
            speakStreamUtf8Tail(receivedMessage, speechUtf8Index);
        }
    }
}

static void appendValidatedUtf8Chunk(struct UTF8StreamBuffer *stream,
                                     const STRPTR chunk,
                                     STRPTR receivedMessage, UWORD *wordNumber,
                                     ULONG *speechUtf8Index) {
    UBYTE piece[2048];
    ULONG taken;

    if (chunk == NULL || chunk[0] == '\0') {
        return;
    }

    if (stream == NULL) {
        appendAssistantStreamText((STRPTR)chunk, receivedMessage, wordNumber,
                                  speechUtf8Index);
        return;
    }

    if (!utf8stream_append(stream, (const UBYTE *)chunk, strlen(chunk))) {
        streamLogUtf8("append failed (buffer grow)");
        appendAssistantStreamText((STRPTR)chunk, receivedMessage, wordNumber,
                                  speechUtf8Index);
        return;
    }

    while ((taken = utf8stream_take_complete(stream, piece, sizeof(piece) - 1)) >
           0) {
        piece[taken] = '\0';
        appendAssistantStreamText((STRPTR)piece, receivedMessage, wordNumber,
                                  speechUtf8Index);
    }
}

static void flushUtf8StreamToMessage(struct UTF8StreamBuffer *stream,
                                     STRPTR receivedMessage, UWORD *wordNumber,
                                     ULONG *speechUtf8Index) {
    UBYTE piece[2048];
    ULONG taken;

    if (stream == NULL) {
        return;
    }

    taken = utf8stream_flush(stream, piece, sizeof(piece) - 1);
    if (taken > 0) {
        piece[taken] = '\0';
        appendAssistantStreamText((STRPTR)piece, receivedMessage, wordNumber,
                                  speechUtf8Index);
    }
}

/**
 * @brief Sends a chat message to the OpenAI API and displays the response
 *and speaks it if speech is enabled
 * @details This function sends a chat message to the OpenAI API and
 *displays the response in the chat window. It also speaks the response if
 *speech is enabled.
 **/
/* msgctxt "STRING_CHAT_RESPONSE_PARTIAL (275//)" */
/* msgid "Response incomplete (connection ended early)." */
/* msgctxt "STRING_CHAT_STREAM_TRUNCATED (276//)" */
/* msgid "Response truncated (stream buffer limit reached)." */

static ChatStreamOutcome chatStreamClassifyOutcome(UTF8 *receivedMessage) {    struct ChatRequestSettings chatSettings;

    if (receivedMessage == NULL || receivedMessage[0] == '\0') {
        return CHAT_STREAM_FAILED;
    }
    configGetActiveChatRequestSettings(&chatSettings);
    if (!chatSettings.stream ||
        chatSettings.apiEndpoint != API_CHAT_ENDPOINT_RESPONSES) {
        return CHAT_STREAM_OK;
    }
    switch (openAIChatStreamTransportOutcome()) {
    case CHAT_TRANSPORT_OK:
        return CHAT_STREAM_OK;
    case CHAT_TRANSPORT_PARTIAL:
        return CHAT_STREAM_PARTIAL;
    case CHAT_TRANSPORT_FAILED:
        return CHAT_STREAM_FAILED;
    default:
        /* Continuation read: outcome set at end of each batch */
        if (openAIChatStreamCompletedOk()) {
            return CHAT_STREAM_OK;
        }
        return CHAT_STREAM_PARTIAL;
    }
}

static CONST_STRPTR chatStreamOutcomeName(ChatStreamOutcome outcome) {
    switch (outcome) {
    case CHAT_STREAM_OK:
        return "OK";
    case CHAT_STREAM_PARTIAL:
        return "PARTIAL";
    case CHAT_STREAM_FAILED:
    default:
        return "FAILED";
    }
}

static void finishChatStream(ChatStreamOutcome outcome, UTF8 *receivedMessage,
                             ULONG speechUtf8Index, BOOL isNewConversation) {
    struct ChatRequestSettings chatSettings;

#ifdef __MORPHOS__
    morphosChatStreamRawScintillaRefresh = FALSE;
#endif
    if (mainWindowIsShuttingDown()) {
        return;
    }
    if (streamLogIsEnabled()) {
        ULONG msgLen =
            receivedMessage != NULL ? (ULONG)strlen(receivedMessage) : 0;
        streamLogChatEnd(chatStreamOutcomeName(outcome), msgLen,
                         openAIChatStreamCompletedOk(),
                         openAIChatStreamTruncated(),
                         openAIChatStreamLastSseSnippet());
    }

    configGetActiveChatRequestSettings(&chatSettings);


    /* Handle shell tool calls - loop to handle multiple sequential commands */
    while (chatSettings.stream && chatSettings.shellToolEnabled &&
           hasPendingToolCall()) {
        UTF8 *command = getPendingToolCommand();
        STRPTR callId = getPendingToolCallId();
        UTF8 *responseId = getPendingResponseId();

        /* Ask user for confirmation before executing the command */
        UBYTE confirmMsg[4096];
        snprintf(confirmMsg, sizeof(confirmMsg),
                 STRING_SHELL_TOOL_CONFIRMATION_BODY, command);
        LONG result = MUI_Request(app, mainWindowObject,
#ifdef __MORPHOS__
                                  NULL,
#else
                                  MUIV_Requester_Image_Warning,
#endif
                                  STRING_SHELL_TOOL_CONFIRMATION_TITLE,
                                  STRING_SHELL_TOOL_CONFIRMATION_BUTTONS,
                                  confirmMsg, TAG_DONE);

        if (result != 1) {
            /* User denied - clear pending tool call and break out of loop */
            clearPendingToolCall();
            strncat(chatOutputTextEditorContents,
                    STRING_SHELL_TOOL_DENIED_BANNER,
                    chatOutputTextEditorContentsCapacity -
                        strlen(chatOutputTextEditorContents) - 1);
            strncat(receivedMessage, STRING_SHELL_TOOL_DENIED_BANNER,
                    READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
            streamUiFlushChatDisplay();
            break; /* Stop processing more tool calls */
        }

        /* User allowed - proceed with execution */

        /* Display that we're executing a command */
        UBYTE statusMsg[256];
        snprintf(statusMsg, sizeof(statusMsg), STRING_EXECUTING_COMMAND);
        updateStatusBar(statusMsg, yellowPen);

        /* Show the command in the chat output and save to history */
        UBYTE cmdDisplay[512];
        snprintf(cmdDisplay, sizeof(cmdDisplay),
                 STRING_SHELL_TOOL_EXECUTING_BANNER_FORMAT, command);
        strncat(chatOutputTextEditorContents, cmdDisplay,
                chatOutputTextEditorContentsCapacity -
                    strlen(chatOutputTextEditorContents) - 1);
        strncat(receivedMessage, cmdDisplay,
                READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
        streamUiFlushChatDisplay();

        /* Execute the shell command */
        LONG exitCode = 0;
        STRPTR output = executeShellCommand(command, &exitCode);

        /* Display the output and save to history */
        UBYTE outputDisplay[4096];
        snprintf(outputDisplay, sizeof(outputDisplay),
                 STRING_SHELL_TOOL_OUTPUT_DISPLAY_FORMAT, exitCode,
                 output != NULL ? output : (STRPTR)STRING_SHELL_TOOL_NO_OUTPUT);
        strncat(chatOutputTextEditorContents, outputDisplay,
                chatOutputTextEditorContentsCapacity -
                    strlen(chatOutputTextEditorContents) - 1);
        strncat(receivedMessage, outputDisplay,
                READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
        streamUiFlushChatDisplay();

        /* Build output string with exit code */
        UBYTE toolOutput[8192];
        snprintf(toolOutput, sizeof(toolOutput),
                 STRING_SHELL_TOOL_TOOL_OUTPUT_FORMAT, exitCode,
                 output != NULL ? output : (STRPTR)STRING_SHELL_TOOL_NO_OUTPUT);

        /* Send the tool result back to the API - this may set a new pending
         * tool call if OpenAI wants to run another command */
        struct json_object *toolResponse = postToolResultToOpenAI(
            responseId, callId, toolOutput, chatSettings.model,
            chatSettings.host, chatSettings.port, chatSettings.useSSL,
            chatSettings.apiKey, configGetProxyEnabled(),
            chatSettings.proxyHost, chatSettings.proxyPort,
            chatSettings.proxyUsesSSL, chatSettings.proxyRequiresAuth,
            chatSettings.proxyUsername, chatSettings.proxyPassword,
            chatSettings.shellToolEnabled, chatSettings.apiEndpointUrl,
            chatSettings.authorizationType, chatSettings.customHeaders);

        if (output != NULL) {
            FreeVec(output);
        }

        /* Get the text content from the new response */
        if (toolResponse != NULL) {
            /* Check for errors */
            struct json_object *error;
            if (json_object_object_get_ex(toolResponse, "error", &error) &&
                !json_object_is_type(error, json_type_null)) {
                clearPendingToolCall();
                struct json_object *message =
                    json_object_object_get(error, "message");
                if (message != NULL) {
                    displayError(json_object_get_string(message));
                }
                json_object_put(toolResponse);
                break; /* Stop on error */
            }

            /* Each tool result creates a new response id; keep it in sync so
             * stateful requests (e.g. auto-generated conversation title) chain
             * from the latest response, not the pre-tool id. */
            conversationSyncLastResponseIdFromPayload(currentConversation,
                                                      toolResponse);

            /* Check if there's another tool call - if so, loop will continue.
             * postToolResultToOpenAI will have set pendingToolCall if so */
            if (hasPendingToolCall()) {
                json_object_put(toolResponse);
                continue;
            }

            /* No more tool calls - get the final response text */
            UTF8 *toolContentString = getMessageContentFromJson(
                toolResponse, FALSE, FALSE, chatSettings.apiEndpoint);
            if (toolContentString != NULL && strlen(toolContentString) > 0) {
                /* Append to the received message */
                strncat(receivedMessage, toolContentString,
                        READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);

                /* Display in chat output */
                STRPTR formattedToolResponse =
                    CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset,
                                      CSA_Source, (Tag)toolContentString,
                                      CSA_MapForeignChars, TRUE, TAG_DONE);
                if (formattedToolResponse != NULL) {
                    strncat(chatOutputTextEditorContents, formattedToolResponse,
                            chatOutputTextEditorContentsCapacity -
                                strlen(chatOutputTextEditorContents) - 1);
                    CodesetsFreeA(formattedToolResponse, NULL);
                } else {
                    STRPTR latin1 = utf8ToLatin1(toolContentString);
                    if (latin1 != NULL) {
                        strncat(chatOutputTextEditorContents, latin1,
                                chatOutputTextEditorContentsCapacity -
                                    strlen(chatOutputTextEditorContents) - 1);
                        FreeVec(latin1);
                    }
                }

#ifndef __MORPHOS__
                STRPTR formattedContent = convertMarkdownFormattingToMUI(
                    chatOutputTextEditorContents);
                if (formattedContent != NULL) {
                    strncpy(chatOutputTextEditorContents, formattedContent,
                            chatOutputTextEditorContentsCapacity - 1);
                    FreeVec(formattedContent);
                }
                set(chatOutputTextEditor, MUIA_NFloattext_Text,
                    chatOutputTextEditorContents);
                set(chatOutputListView, MUIA_NList_First,
                    MUIV_NList_First_Bottom);
#else
                streamUiFlushChatDisplay();
#endif
            }
            json_object_put(toolResponse);
        }
    } /* end of while (tool calls) */

    hideLoadingBar();

    if (outcome == CHAT_STREAM_OK || outcome == CHAT_STREAM_PARTIAL) {
#ifndef __MORPHOS__
        set(chatOutputTextEditor, MUIA_NFloattext_Text,
            chatOutputTextEditorContents);
#endif
        addTextToConversation(currentConversation, receivedMessage,
                              "assistant");
        displayConversation(currentConversation);
#ifdef __MORPHOS__
        refreshViewCodeBlocksMenuState();
#endif

        if (configGetSpeechEnabled()) {
            SpeechSystem speechSys = configGetSpeechSystem();
            if (speechSys == SPEECH_SYSTEM_OPENAI ||
                speechSys == SPEECH_SYSTEM_XAI) {
                speakText(receivedMessage, NULL, AUDIO_FORMAT_PCM);
            } else {
                speakStreamUtf8Tail(receivedMessage, &speechUtf8Index);
            }
        }

        if (isNewConversation) {
            struct json_object **responses;

            updateStatusBar(STRING_GENERATING_CONVERSATION_TITLE, 7);
            showLoadingBar();
            addTextToConversation(currentConversation,
                                  "generate a short title for this "
                                  "conversation and don't enclose the title in "
                                  "quotes or prefix the response with anything",
                                  "user");
            setConversationSystem(currentConversation, NULL);
            /* Do not send web search / server tools on the title request: xAI
             * injects search tools when web search is on, and the model may
             * return only tool calls with no assistant message text ? leaving
             * the title empty and the row blank in the list. */
            responses = postChatMessageToOpenAI(
                currentConversation, chatSettings.host, chatSettings.port,
                chatSettings.useSSL, chatSettings.model, chatSettings.apiKey,
                FALSE, chatSettings.useProxy, chatSettings.proxyHost,
                chatSettings.proxyPort, chatSettings.proxyUsesSSL,
                chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
                chatSettings.proxyPassword, FALSE, FALSE,
                chatSettings.apiEndpoint, chatSettings.apiEndpointUrl,
                chatSettings.authorizationType, chatSettings.customHeaders);
            struct Node *titleRequestNode =
                RemTail((struct List *)currentConversation->messages);
            FreeVec(titleRequestNode);
            hideLoadingBar();
            if (responses == NULL) {
                displayError(STRING_ERROR_CONNECTING_OPENAI);
            } else if (responses[0] != NULL) {
                ULONG combinedLen = 1;
                UWORD ri = 0;
                struct json_object *r = NULL;
                STRPTR combined = NULL;

                while ((r = responses[ri++]) != NULL) {
                    UTF8 *part = getMessageContentFromJson(
                        r, FALSE, FALSE, chatSettings.apiEndpoint);
                    if (part != NULL)
                        combinedLen += strlen(part) + 1;
                }
                combined = AllocVec(combinedLen, MEMF_ANY | MEMF_CLEAR);
                if (combined == NULL) {
                    combined = (STRPTR) "";
                } else {
                    ri = 0;
                    while ((r = responses[ri++]) != NULL) {
                        UTF8 *part = getMessageContentFromJson(
                            r, FALSE, FALSE, chatSettings.apiEndpoint);
                        if (part != NULL && strlen(part) > 0) {
                            strncat(combined, part,
                                    combinedLen - strlen(combined) - 1);
                        }
                    }
                }
                if (currentConversation->name == NULL) {
                    currentConversation->name = allocNewConversationTitle(
                        currentConversation, (const UTF8 *)combined);
                    if (currentConversation->name != NULL) {
                        conversationRefreshNameListDisplay(currentConversation);
                    }
                }
                if (combined != NULL && combined != (STRPTR) "") {
                    FreeVec(combined);
                }
                DoMethod(conversationListObject, MUIM_NList_InsertSingle,
                         currentConversation, MUIV_NList_Insert_Top);
                DoMethod(conversationListObject, MUIM_NList_SetActive,
                         MUIV_NList_Active_Top, NULL);
                ri = 0;
                r = NULL;
                while ((r = responses[ri++]) != NULL) {
                    json_object_put(r);
                }
            }
            FreeVec(responses);
        }
    }

    if (outcome == CHAT_STREAM_PARTIAL) {
        if (openAIChatStreamTruncated()) {
            updateStatusBar(STRING_CHAT_STREAM_TRUNCATED, yellowPen);
        } else {
            updateStatusBar(STRING_CHAT_RESPONSE_PARTIAL, yellowPen);
        }
    } else {
        updateStatusBar(STRING_READY, greenPen);
    }

    if (!mainWindowIsShuttingDown()) {
        saveConversations();
    }

    set(sendMessageButton, MUIA_Disabled, FALSE);
    set(newChatButton, MUIA_Disabled, FALSE);
    set(deleteChatButton, MUIA_Disabled, FALSE);
}

static void sendChatMessage() {
    BOOL isNewConversation = FALSE;
    struct json_object **responses;

    streamLogLifecycle("sendChatMessage begin");
    if (currentConversation == NULL) {
        isNewConversation = TRUE;
        currentConversation = newConversation();
#ifdef __MORPHOS__
        clearChatOutputDisplay();
#else
        chatOutputTextEditorContents[0] = '\0';
        set(chatOutputTextEditor, MUIA_NFloattext_Text,
            chatOutputTextEditorContents);
#endif
        set(conversationListObject, MUIA_NList_Active, MUIV_NList_Active_Off);
    }
    set(sendMessageButton, MUIA_Disabled, TRUE);
    set(newChatButton, MUIA_Disabled, TRUE);
    set(deleteChatButton, MUIA_Disabled, TRUE);

    updateStatusBar(STRING_SENDING_MESSAGE, yellowPen);
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
    UTF8 *receivedMessage = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
    STRPTR text;
    if (isAROS) {
        get(chatInputTextEditor, MUIA_String_Contents, &text);
    } else {
        text = DoMethod(chatInputTextEditor, MUIM_TextEditor_ExportText);
    }

    // Remove trailing newline characters
    while (text != NULL && text[0] != '\0' && text[strlen(text) - 1] == '\n') {
        text[strlen(text) - 1] = '\0';
    }

    UTF8 *textUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                        CSA_Source, (Tag)text, TAG_DONE);

#ifndef __MORPHOS__
    {
        UBYTE userStyleString[] = "\033r\033b\0333";
        UBYTE userAlignment;
        switch (configGetUserTextAlignment()) {
        case ALIGN_LEFT:
            userAlignment = 'l';
            break;
        case ALIGN_CENTER:
            userAlignment = 'c';
            break;
        case ALIGN_RIGHT:
            userAlignment = 'r';
            break;
        }
        userStyleString[1] = userAlignment;
        strbufAppend(chatOutputTextEditorContents,
                     CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, userStyleString);
        for (ULONG i = 0; i < strlen(text); i++) {
            UBYTE oneChar[2] = {0, 0};

            if (strlen(chatOutputTextEditorContents) >=
                CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH - 10) {
                break;
            }
            oneChar[0] = text[i];
            strbufAppend(chatOutputTextEditorContents,
                         CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                         (STRPTR)oneChar);
            if (text[i] == '\n') {
                strbufAppend(chatOutputTextEditorContents,
                             CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                             userStyleString);
            }
        }

        set(chatOutputTextEditor, MUIA_NFloattext_Text,
            chatOutputTextEditorContents);
        set(chatOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);
    }
#endif

    addTextToConversation(currentConversation, textUTF8, "user");
#ifdef __MORPHOS__
    /* Rebuild buffer from nodes; do not append (buffer still held full history). */
    displayConversation(currentConversation);
#endif
    CodesetsFreeA(textUTF8, NULL);

    struct ChatRequestSettings chatSettings;

    configGetActiveChatRequestSettings(&chatSettings);
    setConversationSystem(currentConversation, chatSettings.chatSystem);

    if (isAROS) {
        set(chatInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(chatInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(chatInputTextEditor, MUIM_GoActive);

    BOOL dataStreamFinished = FALSE;
    ULONG speechUtf8Index = 0;
    UWORD wordNumber = 0;
    struct UTF8StreamBuffer *utf8Stream = utf8stream_create(4096);

    strbufAppend(chatOutputTextEditorContents,
                 CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, "\n");
    streamUiResetRefreshClock();
#ifdef __MORPHOS__
    morphosChatStreamRawScintillaRefresh = TRUE;
#endif

    do {
        streamLogLifecycle("chat send postChat begin");
        responses = postChatMessageToOpenAI(
            currentConversation, chatSettings.host, chatSettings.port,
            chatSettings.useSSL, chatSettings.model, chatSettings.apiKey,
            chatSettings.stream, chatSettings.useProxy, chatSettings.proxyHost,
            chatSettings.proxyPort, chatSettings.proxyUsesSSL,
            chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
            chatSettings.proxyPassword, chatSettings.webSearchEnabled,
            chatSettings.shellToolEnabled, chatSettings.apiEndpoint,
            chatSettings.apiEndpointUrl, chatSettings.authorizationType,
            chatSettings.customHeaders);
        if (responses == NULL) {
            streamLogLifecycle("chat send postChat returned null");
            streamLogApiError("connect", "postChatMessageToOpenAI returned NULL");
            displayError(STRING_ERROR_CONNECTING_OPENAI);
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
            set(sendMessageButton, MUIA_Disabled, FALSE);
            set(newChatButton, MUIA_Disabled, FALSE);
            set(deleteChatButton, MUIA_Disabled, FALSE);
            utf8stream_free(utf8Stream);
            FreeVec(receivedMessage);
            if (!isAROS) {
                FreeVec(text);
            }
#ifdef __MORPHOS__
            morphosChatStreamRawScintillaRefresh = FALSE;
#endif
            return;
        }

        streamLogLifecycle("chat send postChat returned batch");

        UWORD responseIndex = 0;

        struct json_object *response;
        if (mainWindowIsShuttingDown()) {
            freeOpenAIResponseArray(responses);
            responses = NULL;
            dataStreamFinished = TRUE;
            break;
        }

        while (response = responses[responseIndex++]) {
            struct json_object *error;
            if (json_object_object_get_ex(response, "error", &error) &&
                !json_object_is_type(error, json_type_null)) {
                CONST_STRPTR messageString = jsonGetApiErrorMessage(error);
                streamLogApiError("api_json_error", messageString);
                STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)(messageString != NULL ? messageString : ""),
                    CSA_MapForeignChars, TRUE, TAG_DONE);
                displayError(formattedMessageSystemEncoded);
                CodesetsFreeA(formattedMessageSystemEncoded, NULL);
                set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
                if (isAROS) {
                    set(chatInputTextEditor, MUIA_String_Contents, text);
                } else {
                    set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
                }
                struct Node *lastMessage =
                    RemTail((struct List *)currentConversation->messages);
                FreeVec(lastMessage);
                if (currentConversation ==
                    currentConversation->messages->mlh_TailPred) {
                    currentConversation = NULL;
#ifdef __MORPHOS__
                    clearChatOutputDisplay();
#else
                    chatOutputTextEditorContents[0] = '\0';
                    set(chatOutputTextEditor, MUIA_NFloattext_Text,
                        chatOutputTextEditorContents);
#endif
                } else {
                    displayConversation(currentConversation);
#ifdef __MORPHOS__
                    refreshViewCodeBlocksMenuState();
#endif
                }
                json_object_put(response);
                discardOpenAIResponseArray(responses, responseIndex);
                responses = NULL;

                set(sendMessageButton, MUIA_Disabled, FALSE);
                set(newChatButton, MUIA_Disabled, FALSE);
                set(deleteChatButton, MUIA_Disabled, FALSE);

                utf8stream_free(utf8Stream);
                FreeVec(receivedMessage);
                if (!isAROS) {
                    FreeVec(text);
                }
#ifdef __MORPHOS__
                morphosChatStreamRawScintillaRefresh = FALSE;
#endif
                return;
            }

            if (!json_object_is_type(response, json_type_object)) {
                json_object_put(response);
                continue;
            }

            UTF8 *contentString = getMessageContentFromJson(
                response, chatSettings.stream, FALSE, chatSettings.apiEndpoint);
            if (!chatSettings.stream) {
                appendValidatedUtf8Chunk(utf8Stream, contentString,
                                         receivedMessage, &wordNumber,
                                         &speechUtf8Index);
                json_object_put(response);
                dataStreamFinished = TRUE;
                continue;
            } else {
                if (contentString != NULL) {
                    if (strlen(contentString) > 0) {
                        appendValidatedUtf8Chunk(utf8Stream, contentString,
                                                 receivedMessage, &wordNumber,
                                                 &speechUtf8Index);
                    }
                    struct json_object *typeObj = NULL;
                    STRPTR type = NULL;

                    if (json_object_object_get_ex(response, "type", &typeObj)) {
                        type = json_object_get_string(typeObj);
                    }
                    if (type != NULL &&
                        strcmp(type, "response.completed") == 0) {
                        dataStreamFinished = TRUE;
                    } else if (json_object_object_get(response, "choices") !=
                               NULL) {
                        struct json_object *choices =
                            json_object_object_get(response, "choices");
                        if (choices != NULL &&
                            json_object_is_type(choices, json_type_array) &&
                            json_object_array_length(choices) > 0) {
                            struct json_object *choice0 =
                                json_object_array_get_idx(choices, 0);
                            if (choice0 != NULL) {
                                struct json_object *finishReason =
                                    json_object_object_get(choice0,
                                                           "finish_reason");
                                if (finishReason != NULL &&
                                    !json_object_is_type(finishReason,
                                                         json_type_null)) {
                                    UTF8 *fr =
                                        json_object_get_string(finishReason);
                                    if (fr != NULL && strlen(fr) > 0) {
                                        dataStreamFinished = TRUE;
                                    }
                                }
                            }
                        }
                    } else {
                        struct json_object *candidates =
                            json_object_object_get(response, "candidates");
                        if (candidates != NULL &&
                            json_object_is_type(candidates, json_type_array) &&
                            json_object_array_length(candidates) > 0) {
                            struct json_object *cand0 =
                                json_object_array_get_idx(candidates, 0);
                            if (cand0 != NULL &&
                                json_object_is_type(cand0, json_type_object)) {
                                struct json_object *finishReason =
                                    json_object_object_get(cand0,
                                                           "finishReason");
                                if (finishReason == NULL) {
                                    finishReason = json_object_object_get(
                                        cand0, "finish_reason");
                                }
                                if (finishReason != NULL &&
                                    !json_object_is_type(finishReason,
                                                         json_type_null)) {
                                    UTF8 *fr =
                                        json_object_get_string(finishReason);
                                    if (fr != NULL && strlen(fr) > 0) {
                                        dataStreamFinished = TRUE;
                                    }
                                }
                            }
                        }
                    }
                    json_object_put(response);
                } else {
                    dataStreamFinished = TRUE;
                }
            }
        }
        freeOpenAIResponseArray(responses);
        responses = NULL;

#ifndef DAEMON
        if (app != NULL) {
            ULONG muiSig = 0;
            DoMethod(app, MUIM_Application_NewInput, &muiSig);
        }
#endif
        if (chatSettings.stream &&
            chatSettings.apiEndpoint == API_CHAT_ENDPOINT_RESPONSES &&
            !openAIChatStreamInProgress()) {
            dataStreamFinished = TRUE;
        }
    } while (!dataStreamFinished && !mainWindowIsShuttingDown());

    flushUtf8StreamToMessage(utf8Stream, receivedMessage, &wordNumber,
                             &speechUtf8Index);
    utf8stream_free(utf8Stream);

    if (!mainWindowIsShuttingDown()) {
        streamUiFlushChatDisplay();
        finishChatStream(chatStreamClassifyOutcome(receivedMessage), receivedMessage,
                         speechUtf8Index, isNewConversation);
    }

    streamLogLifecycle("sendChatMessage end");
    FreeVec(receivedMessage);
    if (!isAROS) {
        FreeVec(text);
    }
}

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
 **/
void displayConversation(struct Conversation *conversation) {
    struct ConversationNode *conversationNode;
    UTF8 *content;

    if (mainWindowIsShuttingDown()) {
        return;
    }
#ifdef __MORPHOS__
    streamLogLifecycle("displayConversation build buffer begin");
#endif
    if (conversation == NULL) {
        conversation = currentConversation;
    }
    if (conversation == NULL || chatOutputTextEditorContents == NULL) {
        return;
    }

    chatOutputTextEditorContents[0] = '\0';

    ULONG estimatedRequired = 1024;
    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        UTF8 *rawEstimate = conversationNodeGetRaw(conversationNode);
        estimatedRequired +=
            strlen((const char *)(rawEstimate != NULL ? rawEstimate : (UTF8 *)"")) *
                3 +
            512;
    }
    if (!ensureChatOutputBufferCapacity(estimatedRequired)) {
        displayError(STRING_ERROR_CONVERSATION_MAX_LENGTH_EXCEEDED);
        set(sendMessageButton, MUIA_Disabled, TRUE);
        return;
    }

    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        content = conversationNodeGetDisplay(conversationNode);
        if (content == NULL) {
            content = (UTF8 *)"";
        }
        UTF8 *rawNode = conversationNodeGetRaw(conversationNode);
        if ((strlen(chatOutputTextEditorContents) +
             strlen((const char *)(rawNode != NULL ? rawNode : (UTF8 *)"")) +
             256) >
            chatOutputTextEditorContentsCapacity) {
            ULONG need = strlen(chatOutputTextEditorContents) +
                         strlen((const char *)(rawNode != NULL ? rawNode : (UTF8 *)"")) *
                             3 +
                         1024;
            if (!ensureChatOutputBufferCapacity(need)) {
                displayError(STRING_ERROR_CONVERSATION_MAX_LENGTH_EXCEEDED);
                set(sendMessageButton, MUIA_Disabled, TRUE);
                return;
            }
        }
        if ((strlen(chatOutputTextEditorContents) + strlen(content) + 256) >
            WRITE_BUFFER_LENGTH) {
            displayError(STRING_ERROR_CONVERSATION_MAX_LENGTH_EXCEEDED);
            break;
        }
#ifdef __MORPHOS__
        if (chatOutputTextEditorContents[0] != '\0') {
            strbufAppend(chatOutputTextEditorContents,
                         CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, "\n\n");
        }
        strbufAppend(chatOutputTextEditorContents,
                     CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, content);
#else
        if (strcmp(conversationNode->role, "user") == 0) {
            UTF8 *userRaw = conversationNodeGetRaw(conversationNode);
            STRPTR userContent =
                CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset,
                                  CSA_Source, (Tag)userRaw,
                                  CSA_MapForeignChars, TRUE, TAG_DONE);
            STRPTR latin1Fallback = NULL;
            BOOL freeWithCodesets = (userContent != NULL);
            if (userContent == NULL) {
                latin1Fallback = utf8ToLatin1(userRaw);
                userContent = latin1Fallback ? latin1Fallback
                                             : (STRPTR)userRaw;
            }
            content = (UTF8 *)userContent;
            UBYTE userAlignment;
            switch (configGetUserTextAlignment()) {
            case ALIGN_LEFT:
                userAlignment = 'l';
                break;
            case ALIGN_CENTER:
                userAlignment = 'c';
                break;
            case ALIGN_RIGHT:
                userAlignment = 'r';
                break;
            }
            UBYTE userStyleString[] = "\033r\033b\0333";
            userStyleString[1] = userAlignment;
            strbufAppend(chatOutputTextEditorContents,
                         CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                         userStyleString);
            for (ULONG i = 0; i < strlen(content); i++) {
                UBYTE oneChar[2] = {0, 0};

                oneChar[0] = content[i];
                strbufAppend(chatOutputTextEditorContents,
                             CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                             (STRPTR)oneChar);
                if (content[i] == '\n') {
                    strbufAppend(chatOutputTextEditorContents,
                                 CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                                 userStyleString);
                }
            }
            if (freeWithCodesets)
                CodesetsFreeA(userContent, NULL);
            else if (latin1Fallback != NULL)
                FreeVec(latin1Fallback);
        } else if (strcmp(conversationNode->role, "assistant") == 0) {
            set(chatOutputListView, MUIA_NFloattext_Align,
                configGetAssistantTextAlignment());
            UBYTE assistantStyleString[] = "\n\n\0332";
            STRPTR formattedContent = formatAssistantTextForDisplay(content);

            strbufAppend(chatOutputTextEditorContents,
                         CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                         assistantStyleString);
            if (formattedContent != NULL) {
                strbufAppend(chatOutputTextEditorContents,
                             CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH,
                             formattedContent);
                FreeVec(formattedContent);
            }
            strbufAppend(chatOutputTextEditorContents,
                         CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, "\n\n");
        }
#endif
    }

#ifdef __MORPHOS__
    streamLogLifecycle("displayConversation build buffer done");
    morphosScheduleChatOutputRefreshFromList();
#else
    {
        ULONG totalLen = (ULONG)strlen(chatOutputTextEditorContents);
        if (totalLen > CHAT_OUTPUT_WIDGET_SAFE_LIMIT) {
            STRPTR truncated = chatOutputTextEditorContents +
                               (totalLen - CHAT_OUTPUT_WIDGET_SAFE_LIMIT);
            memmove(chatOutputTextEditorContents, truncated,
                    CHAT_OUTPUT_WIDGET_SAFE_LIMIT + 1);
        }

        set(chatOutputTextEditor, MUIA_NFloattext_Text,
            chatOutputTextEditorContents);
        set(chatOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);
    }
#endif
}

/**
 * Copy a conversation
 * @param conversation The conversation to copy
 * @return A pointer to the copied conversation
 **/
static struct Conversation *
copyConversation(struct Conversation *conversation) {
    struct Conversation *copy = newConversation();
    struct ConversationNode *conversationNode;
    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
         conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        addTextToConversation(copy, conversationNodeGetRaw(conversationNode),
                              conversationNode->role);
    }
    if (conversation->name != NULL) {
        copy->name = dupStringAlloc(conversation->name);
        if (copy->name != NULL) {
            conversationRefreshNameListDisplay(copy);
        }
    }
    if (conversation->lastResponseId != NULL &&
        strlen(conversation->lastResponseId) > 0) {
        copy->lastResponseId = AllocVec(
            strlen(conversation->lastResponseId) + 1, MEMF_ANY | MEMF_CLEAR);
        if (copy->lastResponseId != NULL) {
            strncpy(copy->lastResponseId, conversation->lastResponseId,
                    strlen(conversation->lastResponseId));
        }
    }
    return copy;
}

/**
 * Copy a generated image
 * @param generatedImage The generated image to copy
 * @return A pointer to the copied generated image
 **/
static struct GeneratedImage *
copyGeneratedImage(struct GeneratedImage *generatedImage) {
    struct GeneratedImage *newEntry =
        AllocVec(sizeof(struct GeneratedImage), MEMF_CLEAR);
    newEntry->name = dupStringAlloc(generatedImage->name);
    newEntry->filePath = dupStringAlloc(generatedImage->filePath);
    newEntry->prompt = dupStringAlloc(generatedImage->prompt);
    newEntry->imageModel = generatedImage->imageModel;
    newEntry->width = generatedImage->width;
    newEntry->height = generatedImage->height;
    return (newEntry);
}

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG saveConversations() {
    BPTR file = Open("AMIGAGPT:chat-history.json", MODE_NEWFILE);
    if (file == 0) {
        displayError(STRING_ERROR_CHAT_HISTORY_CREATE);
        return RETURN_ERROR;
    }

    struct json_object *conversationsJsonArray = json_object_new_array();

    LONG totalConversationCount;
    get(conversationListObject, MUIA_NList_Entries, &totalConversationCount);

    for (LONG i = 0; i < totalConversationCount; i++) {
        struct json_object *conversationJsonObject = json_object_new_object();
        struct Conversation *conversation;
        DoMethod(conversationListObject, MUIM_NList_GetEntry, i, &conversation);
        json_object_object_add(conversationJsonObject, "name",
                               json_object_new_string(conversation->name));
        if (conversation->lastResponseId != NULL &&
            strlen(conversation->lastResponseId) > 0) {
            json_object_object_add(
                conversationJsonObject, "lastResponseId",
                json_object_new_string(conversation->lastResponseId));
        }
        struct json_object *messagesJsonArray = json_object_new_array();
        struct ConversationNode *conversationNode;
        for (conversationNode =
                 (struct ConversationNode *)conversation->messages->mlh_Head;
             conversationNode->node.mln_Succ != NULL;
             conversationNode =
                 (struct ConversationNode *)conversationNode->node.mln_Succ) {
            if (!strcmp(conversationNode->role, "system"))
                continue;
            struct json_object *messageJsonObject = json_object_new_object();
            json_object_object_add(
                messageJsonObject, "role",
                json_object_new_string(conversationNode->role));
            json_object_object_add(
                messageJsonObject, "content",
                json_object_new_string(
                    conversationNodeGetRaw(conversationNode)));
            json_object_array_add(messagesJsonArray, messageJsonObject);
        }
        json_object_object_add(conversationJsonObject, "messages",
                               messagesJsonArray);
        json_object_array_add(conversationsJsonArray, conversationJsonObject);
    }

    STRPTR conversationsJsonString = (STRPTR)json_object_to_json_string_ext(
        conversationsJsonArray, JSON_C_TO_STRING_PRETTY);

    if (Write(file, conversationsJsonString, strlen(conversationsJsonString)) !=
        (LONG)strlen(conversationsJsonString)) {
        displayError(STRING_ERROR_CHAT_HISTORY_SAVE);
        Close(file);
        json_object_put(conversationsJsonArray);
        return RETURN_ERROR;
    }

    Close(file);
    json_object_put(conversationsJsonArray);
    return RETURN_OK;
}

/**
 * Saves the images to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG saveImages() {
    BPTR file = Open("AMIGAGPT:image-history.json", MODE_NEWFILE);
    if (file == 0) {
        displayError(STRING_ERROR_IMAGE_HISTORY_CREATE);
        return RETURN_ERROR;
    }

    LONG totalImageCount;
    get(imageListObject, MUIA_NList_Entries, &totalImageCount);

    struct json_object *imagesJsonArray = json_object_new_array();
    for (LONG i = 0; i < totalImageCount; i++) {
        struct json_object *imageJsonObject = json_object_new_object();
        struct GeneratedImage *generatedImage;

        DoMethod(imageListObject, MUIM_NList_GetEntry, i, &generatedImage);
        json_object_object_add(imageJsonObject, "name",
                               json_object_new_string(generatedImage->name));
        json_object_object_add(
            imageJsonObject, "filePath",
            json_object_new_string(generatedImage->filePath));
        json_object_object_add(imageJsonObject, "prompt",
                               json_object_new_string(generatedImage->prompt));
        json_object_object_add(imageJsonObject, "imageModel",
                               json_object_new_int(generatedImage->imageModel));
        json_object_object_add(imageJsonObject, "width",
                               json_object_new_int(generatedImage->width));
        json_object_object_add(imageJsonObject, "height",
                               json_object_new_int(generatedImage->height));
        json_object_array_add(imagesJsonArray, imageJsonObject);
    }

    STRPTR imagesJsonString = (STRPTR)json_object_to_json_string_ext(
        imagesJsonArray, JSON_C_TO_STRING_PRETTY);

    if (Write(file, imagesJsonString, strlen(imagesJsonString)) !=
        (LONG)strlen(imagesJsonString)) {
        displayError(STRING_ERROR_IMAGE_HISTORY_WRITE);
        Close(file);
        json_object_put(imagesJsonArray);
        return RETURN_ERROR;
    }

    Close(file);
    json_object_put(imagesJsonArray);
    return RETURN_OK;
}

/**
 * Opens and displays the image with scaling
 * @param image the image to open
 * @param width the width of the image
 * @param height the height of the image
 **/
static void openImage(struct GeneratedImage *image, WORD scaledWidth,
                      WORD scaledHeight) {
    if (image == NULL)
        return;

    // Get screen dimensions from main window
    struct Screen *currentScreen;
    get(mainWindowObject, MUIA_Window_Screen, &currentScreen);
    WORD screenWidth = currentScreen ? currentScreen->Width : 640;
    WORD screenHeight = currentScreen ? currentScreen->Height : 480;

    WORD lowestWidth =
        (screenWidth - 16) < scaledWidth ? (screenWidth - 16) : scaledWidth;
    WORD lowestHeight =
        screenHeight < scaledHeight ? screenHeight : scaledHeight;

    updateStatusBar(STRING_LOADING_IMAGE, yellowPen);

    // clang-format off
    Object *imageWindowLoadingTextObject = VGroup,
        Child, VSpace(0),
        Child, TextObject,
            MUIA_Text_Contents, STRING_LOADING_IMAGE,
            MUIA_Text_PreParse, "\033c",
        End,
        Child, VSpace(0),
    End;
    // clang-format on
    DoMethod(imageWindowImageViewGroup, OM_ADDMEMBER,
             imageWindowLoadingTextObject);
    DoMethod(imageWindowImageViewGroup, OM_REMMEMBER, imageWindowImageView);
    MUI_DisposeObject(imageWindowImageView);

    set(imageWindowObject, MUIA_Window_Title, image->name);
    set(imageWindowObject, MUIA_Window_Width, lowestWidth);
    set(imageWindowObject, MUIA_Window_Height, lowestHeight);
    set(imageWindowObject, MUIA_Window_Activate, TRUE);
    set(imageWindowObject, MUIA_Window_Open, TRUE);

    DoMethod(imageWindowImageViewGroup, MUIM_Group_InitChange);
    // clang-format off
    imageWindowImageView = GuigfxObject,
        MUIA_Guigfx_FileName, image->filePath,
        MUIA_Guigfx_Quality, MUIV_Guigfx_Quality_Best,
        MUIA_Guigfx_ScaleMode, NISMF_SCALEFREE | NISMF_KEEPASPECT_PICTURE | NISMF_KEEPASPECT_SCREEN,
        MUIA_Guigfx_Transparency, NITRF_MASK,
    End;
    // clang-format on
    DoMethod(imageWindowImageViewGroup, OM_ADDMEMBER, imageWindowImageView);
    DoMethod(imageWindowImageViewGroup, OM_REMMEMBER,
             imageWindowLoadingTextObject);
    DoMethod(imageWindowImageViewGroup, MUIM_Group_ExitChange);
    MUI_DisposeObject(imageWindowLoadingTextObject);

    updateStatusBar(STRING_READY, greenPen);
}

/**
 * Load the conversations from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG loadConversations() {
    BPTR file = Open("AMIGAGPT:chat-history.json", MODE_OLDFILE);
    if (file == 0) {
        return RETURN_OK;
    }

#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
#else
#ifdef __AMIGAOS4__
    int64_t fileSize = GetFileSize(file);
#else
    struct FileInfoBlock fib;
    ExamineFH64(file, &fib, NULL);
    int64_t fileSize = fib.fib_Size;
#endif
#endif
    STRPTR conversationsJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
    if (Read(file, conversationsJsonString, fileSize) != fileSize) {
        displayError(STRING_ERROR_CHAT_HISTORY_READ);
        Close(file);
        FreeVec(conversationsJsonString);
        return RETURN_ERROR;
    }

    Close(file);

    struct json_object *conversationsJsonArray =
        json_tokener_parse(conversationsJsonString);
    if (conversationsJsonArray == NULL ||
        !json_object_is_type(conversationsJsonArray, json_type_array)) {
        if (conversationsJsonArray != NULL) {
            json_object_put(conversationsJsonArray);
            conversationsJsonArray = NULL;
        }
        if (Rename("AMIGAGPT:chat-history.json",
                   "AMIGAGPT:chat-history.json.bak")) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_BACKUP);
        } else if (copyFile("AMIGAGPT:chat-history.json",
                            "RAM:chat-history.json")) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_BACKUP_RAM);
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
            if (!DeleteFile("AMIGAGPT:chat-history.json")) {
#else
            if (!Delete("AMIGAGPT:chat-history.json")) {
#endif
                displayError(STRING_ERROR_CHAT_HISTORY_DELETE);
            }
        }

        FreeVec(conversationsJsonString);
        return RETURN_ERROR;
    }

    set(conversationListObject, MUIA_NList_Quiet, TRUE);
    for (UWORD i = 0; i < json_object_array_length(conversationsJsonArray);
         i++) {
        struct json_object *conversationJsonObject =
            json_object_array_get_idx(conversationsJsonArray, i);
        struct json_object *conversationNameJsonObject;

        if (conversationJsonObject == NULL ||
            !json_object_is_type(conversationJsonObject, json_type_object)) {
            continue;
        }
        if (!json_object_object_get_ex(conversationJsonObject, "name",
                                       &conversationNameJsonObject)) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
            FreeVec(conversationsJsonString);
            json_object_put(conversationsJsonArray);
            set(conversationListObject, MUIA_NList_Quiet, FALSE);
            return RETURN_ERROR;
        }

        UTF8 *conversationName =
            json_object_get_string(conversationNameJsonObject);

        struct json_object *messagesJsonArray;
        if (!json_object_object_get_ex(conversationJsonObject, "messages",
                                       &messagesJsonArray) ||
            !json_object_is_type(messagesJsonArray, json_type_array)) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
            FreeVec(conversationsJsonString);
            json_object_put(conversationsJsonArray);
            set(conversationListObject, MUIA_NList_Quiet, FALSE);
            return RETURN_ERROR;
        }

        struct Conversation *conversation = newConversation();
        conversation->name = dupStringAlloc(conversationName);
        if (conversation->name == NULL) {
            freeConversation(conversation);
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
            FreeVec(conversationsJsonString);
            json_object_put(conversationsJsonArray);
            return RETURN_ERROR;
        }
        conversationRefreshNameListDisplay(conversation);

        struct json_object *lastResponseIdJsonObject;
        if (json_object_object_get_ex(conversationJsonObject, "lastResponseId",
                                      &lastResponseIdJsonObject)) {
            STRPTR lastResponseId =
                (STRPTR)json_object_get_string(lastResponseIdJsonObject);
            if (lastResponseId != NULL && strlen(lastResponseId) > 0) {
                conversation->lastResponseId =
                    AllocVec(strlen(lastResponseId) + 1, MEMF_ANY | MEMF_CLEAR);
                if (conversation->lastResponseId != NULL) {
                    strncpy(conversation->lastResponseId, lastResponseId,
                            strlen(lastResponseId));
                }
            }
        }

        for (UWORD j = 0; j < json_object_array_length(messagesJsonArray);
             j++) {
            struct json_object *messageJsonObject =
                json_object_array_get_idx(messagesJsonArray, j);
            struct json_object *roleJsonObject;

            if (messageJsonObject == NULL ||
                !json_object_is_type(messageJsonObject, json_type_object)) {
                continue;
            }
            if (!json_object_object_get_ex(messageJsonObject, "role",
                                           &roleJsonObject)) {
                displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
                FreeVec(conversationsJsonString);
                json_object_put(conversationsJsonArray);
                set(conversationListObject, MUIA_NList_Quiet, FALSE);
                return RETURN_ERROR;
            }
            STRPTR role = json_object_get_string(roleJsonObject);
            struct json_object *contentJsonObject;

            if (!json_object_object_get_ex(messageJsonObject, "content",
                                           &contentJsonObject)) {
                displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
                FreeVec(conversationsJsonString);
                json_object_put(conversationsJsonArray);
                set(conversationListObject, MUIA_NList_Quiet, FALSE);
                return RETURN_ERROR;
            }
            UTF8 *content = json_object_get_string(contentJsonObject);
            addTextToConversation(conversation, content, role);
        }
        DoMethod(conversationListObject, MUIM_NList_InsertSingle, conversation,
                 MUIV_NList_Insert_Top);
    }
    set(conversationListObject, MUIA_NList_Quiet, FALSE);
    json_object_put(conversationsJsonArray);
    return RETURN_OK;
}

/**
 * Load the images from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG loadImages() {
    BPTR file = Open("AMIGAGPT:image-history.json", MODE_OLDFILE);
    if (file == 0) {
        return RETURN_OK;
    }

#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
#else
#ifdef __AMIGAOS4__
    int64_t fileSize = GetFileSize(file);
#else
    struct FileInfoBlock fib;
    ExamineFH64(file, &fib, NULL);
    int64_t fileSize = fib.fib_Size;
#endif
#endif
    STRPTR imagesJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
    if (Read(file, imagesJsonString, fileSize) != fileSize) {
        displayError(STRING_ERROR_IMAGE_HISTORY_READ);
        Close(file);
        FreeVec(imagesJsonString);
        return RETURN_ERROR;
    }

    Close(file);

    struct json_object *imagesJsonArray = json_tokener_parse(imagesJsonString);
    if (imagesJsonArray == NULL) {
        if (Rename("AMIGAGPT:image-history.json",
                   "AMIGAGPT:image-history.json.bak")) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_BACKUP);
        } else if (copyFile("AMIGAGPT:image-history.json",
                            "RAM:image-history.json")) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_BACKUP_RAM);
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
            if (!DeleteFile("AMIGAGPT:image-history.json")) {
#else
            if (!Delete("AMIGAGPT:image-history.json")) {
#endif
                displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            }
        }

        FreeVec(imagesJsonString);
        return RETURN_ERROR;
    }

    for (UWORD i = 0; i < json_object_array_length(imagesJsonArray); i++) {
        struct json_object *imageJsonObject =
            json_object_array_get_idx(imagesJsonArray, i);
        struct json_object *imageNameJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "name",
                                       &imageNameJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        STRPTR imageName = json_object_get_string(imageNameJsonObject);

        struct json_object *imageFilePathJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "filePath",
                                       &imageFilePathJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        STRPTR imageFilePath = json_object_get_string(imageFilePathJsonObject);

        struct json_object *imagePromptJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "prompt",
                                       &imagePromptJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        STRPTR imagePrompt = json_object_get_string(imagePromptJsonObject);

        struct json_object *imageModelJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "imageModel",
                                       &imageModelJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        ImageModel imageModel = json_object_get_int(imageModelJsonObject);

        struct json_object *imageWidthJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "width",
                                       &imageWidthJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        UWORD imageWidth = (WORD)json_object_get_int(imageWidthJsonObject);

        struct json_object *imageHeightJsonObject;
        if (!json_object_object_get_ex(imageJsonObject, "height",
                                       &imageHeightJsonObject)) {
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }

        UWORD imageHeight = (WORD)json_object_get_int(imageHeightJsonObject);

        struct GeneratedImage *generatedImage =
            AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
        generatedImage->name = dupStringAlloc(imageName);
        generatedImage->filePath = dupStringAlloc(imageFilePath);
        generatedImage->prompt = dupStringAlloc(imagePrompt);
        if (generatedImage->name == NULL || generatedImage->filePath == NULL ||
            generatedImage->prompt == NULL) {
            if (generatedImage->name != NULL) {
                FreeVec(generatedImage->name);
            }
            if (generatedImage->filePath != NULL) {
                FreeVec(generatedImage->filePath);
            }
            if (generatedImage->prompt != NULL) {
                FreeVec(generatedImage->prompt);
            }
            FreeVec(generatedImage);
            displayError(STRING_ERROR_IMAGE_HISTORY_PARSE_NO_BACKUP);
            FreeVec(imagesJsonString);
            json_object_put(imagesJsonArray);
            return RETURN_ERROR;
        }
        generatedImage->imageModel = imageModel;
        generatedImage->width = imageWidth;
        generatedImage->height = imageHeight;
        DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage,
                 MUIV_NList_Insert_Top);
    }

    json_object_put(imagesJsonArray);
    return RETURN_OK;
}

/**
 * Print the conversation text to the printer
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG printConversation() {
    BPTR printerFile;
    LONG result = RETURN_ERROR;

    printerFile = Open("PRT:", MODE_NEWFILE);
    if (printerFile) {
        STRPTR printerText =
            AllocVec(strlen(READ_BUFFER_LENGTH) + 1, MEMF_ANY | MEMF_CLEAR);
        struct ConversationNode *conversationNode;

        for (conversationNode = (struct ConversationNode *)
                                    currentConversation->messages->mlh_Head;
             conversationNode->node.mln_Succ != NULL;
             conversationNode =
                 (struct ConversationNode *)conversationNode->node.mln_Succ) {
            UTF8 *content = conversationNodeGetDisplay(conversationNode);
            if (strcmp(conversationNode->role, "user") == 0) {
                Write(printerFile, "*******************\n", -1);
                Write(printerFile, "User:\n\n", -1);

                STRPTR convertedConversationString = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)content, CSA_MapForeignChars, TRUE, TAG_DONE);
                if (convertedConversationString != NULL) {
                    Write(printerFile, convertedConversationString, -1);
                    CodesetsFreeA(convertedConversationString, NULL);
                } else {
                    Write(printerFile, content, -1);
                }
            } else if (strcmp(conversationNode->role, "assistant") == 0) {
                Write(printerFile, "\n\n", -1);
                Write(printerFile, "*******************\n", -1);
                Write(printerFile, "AmigaGPT:\n\n", -1);

                STRPTR convertedConversationString = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)content, CSA_MapForeignChars, TRUE, TAG_DONE);
                if (convertedConversationString != NULL) {
                    Write(printerFile, convertedConversationString, -1);
                    CodesetsFreeA(convertedConversationString, NULL);
                } else {
                    Write(printerFile, content, -1);
                }

                Write(printerFile, "\n\n", -1);
            }
        }

        FreeVec(printerText);
        Close(printerFile);
        result = RETURN_OK;
    }

    return RETURN_OK;
}

struct Conversation *getCurrentConversation(void) { return currentConversation; }