#include <devices/printer.h>
#include <devices/prtbase.h>
#include <devices/trackdisk.h>
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
#include <strings.h>
#include <time.h>
#include "AmigaGPTConfig.h"
#include "AmigaGPTTextEditor.h"
#include "gui.h"
#include "menu.h"
#include "MainWindow.h"
#include "openai.h"
#include "speech.h"
#include "SpeechProviderSettingsRequesterWindow.h"
#include "SpeechWaveform.h"
#include <dos/dos.h>

/* Max nesting depth for B/I/U combined. Adjust as needed. */
#define MAX_STYLE_STACK 32

#define CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH (1024 * 512)
#define CHAT_OUTPUT_WIDGET_SAFE_LIMIT (60 * 1024)
#define CONVERSATION_TITLE_FALLBACK_MAX 96

typedef enum { STYLE_BOLD, STYLE_ITALIC, STYLE_UNDERLINE } StyleType;

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
Object *attachFilesButton;
Object *clearAttachmentsButton;
Object *saveResponseFilesButton;
Object *attachmentSummaryText;
Object *chatInputTextEditor;
Object *chatOutputListView;
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
Object *editImageButton;
Object *attachReferenceImagesButton;
Object *clearReferenceImagesButton;
Object *referenceImageSummaryText;
Object *speechInputTextEditor;
Object *speechListObject;
Object *newSpeechButton;
Object *deleteSpeechButton;
Object *generateSpeechButton;
Object *regenerateSpeechButton;
Object *speechProfileInfo;
Object *speechWaveform;
Object *saveSpeechCopyButton;
Object *generateSpeechTextButton;
Object *attachSpeechFilesButton;
Object *clearSpeechFilesButton;
Object *speechAttachmentSummaryText;
Object *speakFileContentsCheckbox;
Object *modeRegisterGroup;
STRPTR chatOutputTextEditorContents = NULL;
static ULONG chatOutputTextEditorContentsCapacity =
    CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH;
static struct MinList pendingChatFiles;
static BOOL pendingChatFilesInitialized = FALSE;
static struct MinList pendingImageFiles;
static BOOL pendingImageFilesInitialized = FALSE;
static struct MinList pendingSpeechFiles;
static BOOL pendingSpeechFilesInitialized = FALSE;
WORD pens[NUMDRIPENS + 1];
struct Conversation *currentConversation = NULL;
struct GeneratedImage *currentImage = NULL;
struct GeneratedSpeech {
    STRPTR title;
    STRPTR filePath;
    STRPTR text;
    STRPTR profileInfo;
};
static struct GeneratedSpeech *currentSpeech = NULL;
static STRPTR pages[4] = {NULL};
static BOOL requestInterfaceBusy = FALSE;

struct Conversation *newConversation();
static struct Conversation *copyConversation(struct Conversation *conversation);
static struct GeneratedImage *
copyGeneratedImage(struct GeneratedImage *generatedImage);
static void sendChatMessage();
static LONG loadConversations();
static LONG saveConversations();
static LONG loadImages();
static LONG saveImages();
static LONG loadSpeechHistory();
static LONG saveSpeechHistory();
static void updateSpeechControls();
static void setImageGenerationControlsDisabled(BOOL disabled);
static void generateSpeech(BOOL regenerate);
static void openImage(struct GeneratedImage *image, WORD scaledWidth,
                      WORD scaledHeight);
static void initStyleStack(StyleStack *s);
static BOOL pushStyle(StyleStack *s, StyleType style);
static BOOL popStyle(StyleStack *s, StyleType style);
static BOOL isTopStyle(const StyleStack *s, StyleType style);
static void outputStyleOn(STRPTR out, size_t outSize, StyleType style);
static void outputStyleOff(STRPTR out, size_t outSize);
static UBYTE parseMarker(CONST_STRPTR input, size_t pos, size_t len,
                         StyleType *foundStyle);
static BOOL ensureChatOutputBufferCapacity(ULONG required);
static void addMainWindowActions();
static void updateAttachmentControls();
static void updateReferenceImageControls();
static void updateResponseFileControl();

static CONST_STRPTR jsonStringValue(struct json_object *object,
                                    CONST_STRPTR key) {
    struct json_object *value = NULL;
    if (object == NULL ||
        !json_object_object_get_ex(object, key, &value) || value == NULL ||
        json_object_is_type(value, json_type_null))
        return NULL;
    return json_object_get_string(value);
}

static ULONG chatFileCount(struct MinList *files) {
    ULONG count = 0;
    struct ChatFile *file;
    for (file = (struct ChatFile *)files->mlh_Head;
         file->node.mln_Succ != NULL;
         file = (struct ChatFile *)file->node.mln_Succ)
        count++;
    return count;
}

static BOOL chatFilePathAlreadyPresent(struct MinList *files,
                                       CONST_STRPTR path) {
    struct ChatFile *file;
    for (file = (struct ChatFile *)files->mlh_Head;
         file->node.mln_Succ != NULL;
         file = (struct ChatFile *)file->node.mln_Succ) {
        if (file->path != NULL && strcmp(file->path, path) == 0)
            return TRUE;
    }
    return FALSE;
}

static void updateAttachmentControls() {
    if (!pendingChatFilesInitialized || attachmentSummaryText == NULL)
        return;
    ULONG count = chatFileCount(&pendingChatFiles);
    UBYTE summary[128];
    if (count == 0) {
        snprintf(summary, sizeof(summary), "%s", STRING_NO_FILES_ATTACHED);
    } else {
        snprintf(summary, sizeof(summary), STRING_FILES_ATTACHED_FORMAT,
                 count);
    }
    set(attachmentSummaryText, MUIA_Text_Contents, summary);
    set(clearAttachmentsButton, MUIA_Disabled, count == 0);
}

static void updateReferenceImageControls() {
    if (!pendingImageFilesInitialized || referenceImageSummaryText == NULL)
        return;
    ULONG count = chatFileCount(&pendingImageFiles);
    UBYTE summary[128];
    if (count == 0) {
        snprintf(summary, sizeof(summary), "%s", STRING_NO_REFERENCE_IMAGES);
    } else {
        snprintf(summary, sizeof(summary),
                 STRING_REFERENCE_IMAGES_ATTACHED_FORMAT, count);
    }
    set(referenceImageSummaryText, MUIA_Text_Contents, summary);
    set(clearReferenceImagesButton, MUIA_Disabled, count == 0);
}

static STRPTR duplicateEditorText(Object *editor) {
    STRPTR source = NULL;
    STRPTR copy;
    BOOL freeSource = FALSE;

    if (editor == NULL)
        return NULL;
    if (isAROS) {
        get(editor, MUIA_String_Contents, &source);
    } else {
        source = (STRPTR)DoMethod(editor, MUIM_TextEditor_ExportText);
        freeSource = source != NULL;
    }
    if (source == NULL)
        source = (STRPTR)"";
    copy = AllocVec(strlen(source) + 1, MEMF_ANY | MEMF_CLEAR);
    if (copy != NULL)
        strcpy(copy, source);
    if (freeSource)
        FreeVec(source);
    return copy;
}

static void setEditorText(Object *editor, CONST_STRPTR text) {
    if (editor == NULL)
        return;
    if (isAROS)
        set(editor, MUIA_String_Contents, text != NULL ? text : "");
    else
        set(editor, MUIA_TextEditor_Contents, text != NULL ? text : "");
}

static void deleteDiskFile(CONST_STRPTR path) {
    if (path == NULL || strlen(path) == 0)
        return;
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    DeleteFile(path);
#else
    Delete(path);
#endif
}

static void setActionButtonLabel(Object *button, LONG pen, CONST_STRPTR label) {
    UBYTE buffer[64];
    if (button == NULL)
        return;
    snprintf(buffer, sizeof(buffer), "\33c\33P[%ld]%s", pen, label);
    set(button, MUIA_Text_Contents, buffer);
}

#define ACTION_BTN_IDLE 0
#define ACTION_BTN_REQUEST_STOP 1
#define ACTION_BTN_SPEAK_STOP 2

static void updateActionButtonLabels(BOOL force) {
    static UBYTE lastMode = 255;
    UBYTE mode;

    if (requestInterfaceBusy)
        mode = ACTION_BTN_REQUEST_STOP;
    else if (isSpeechPlaying())
        mode = ACTION_BTN_SPEAK_STOP;
    else
        mode = ACTION_BTN_IDLE;

    if (!force && mode == lastMode)
        return;
    lastMode = mode;

    if (mode == ACTION_BTN_REQUEST_STOP) {
        setActionButtonLabel(sendMessageButton, redPen, STRING_STOP);
        setActionButtonLabel(createImageButton, redPen, STRING_STOP);
        setActionButtonLabel(generateSpeechButton, redPen, STRING_STOP);
        return;
    }
    if (mode == ACTION_BTN_SPEAK_STOP)
        setActionButtonLabel(sendMessageButton, redPen, STRING_STOP);
    else
        setActionButtonLabel(sendMessageButton, bluePen, STRING_SEND);
    setActionButtonLabel(createImageButton, bluePen, STRING_CREATE_IMAGE);
    setActionButtonLabel(generateSpeechButton, bluePen, STRING_GENERATE_SPEECH);
}

static void setActionButtonsStopMode(BOOL stopMode) {
    (void)stopMode;
    updateActionButtonLabels(TRUE);
}

static void setPrimaryActionButtonsEnabled(void) {
    if (sendMessageButton != NULL)
        set(sendMessageButton, MUIA_Disabled, FALSE);
    if (createImageButton != NULL)
        set(createImageButton, MUIA_Disabled, FALSE);
    if (generateSpeechButton != NULL)
        set(generateSpeechButton, MUIA_Disabled, FALSE);
}

static BOOL abortIfRequestBusy(void) {
    if (!requestInterfaceBusy)
        return FALSE;
    cancelActiveRequest();
    stopSpeech();
    return TRUE;
}

static CONST_STRPTR xaiBuiltinVoiceName(CONST_STRPTR voiceId) {
    static const CONST_STRPTR pretty[] = {"Ara", "Eve", "Leo", "Rex", "Sal"};
    UBYTE i;

    if (voiceId == NULL || voiceId[0] == '\0')
        return NULL;
    for (i = 0; XAI_TTS_VOICE_NAMES[i] != NULL; i++) {
        if (strcasecmp(voiceId, XAI_TTS_VOICE_NAMES[i]) == 0)
            return pretty[i];
    }
    return NULL;
}

static BOOL copyXAIVoiceNameForId(CONST_STRPTR voiceId, STRPTR dest,
                                  ULONG destSize) {
    CONST_STRPTR pretty;
    CONST_STRPTR storedId;
    CONST_STRPTR storedName;
    STRPTR profilesStr;
    struct json_object *arr;
    int i;
    int len;

    if (dest == NULL || destSize == 0 || voiceId == NULL || voiceId[0] == '\0')
        return FALSE;
    dest[0] = '\0';
    pretty = xaiBuiltinVoiceName(voiceId);
    if (pretty == NULL)
        pretty = speechLookupXAIVoiceName(voiceId);
    if (pretty != NULL && pretty[0] != '\0' && strcmp(pretty, voiceId) != 0) {
        strncpy(dest, pretty, destSize - 1);
        dest[destSize - 1] = '\0';
        return TRUE;
    }
    pretty = xaiBuiltinVoiceName(voiceId);
    if (pretty != NULL) {
        strncpy(dest, pretty, destSize - 1);
        dest[destSize - 1] = '\0';
        return TRUE;
    }
    storedId = configGetXAITTSVoiceId();
    storedName = configGetXAITTSVoiceName();
    if (storedId != NULL && storedName != NULL && storedName[0] != '\0' &&
        strcmp(storedId, voiceId) == 0) {
        strncpy(dest, storedName, destSize - 1);
        dest[destSize - 1] = '\0';
        return TRUE;
    }
    profilesStr = configGetSpeechProfiles();
    if (profilesStr == NULL || profilesStr[0] == '\0')
        return FALSE;
    arr = json_tokener_parse(profilesStr);
    if (arr == NULL || !json_object_is_type(arr, json_type_array)) {
        if (arr != NULL)
            json_object_put(arr);
        return FALSE;
    }
    len = json_object_array_length(arr);
    for (i = 0; i < len; i++) {
        struct json_object *profile = json_object_array_get_idx(arr, i);
        struct json_object *idObj;
        struct json_object *nameObj;
        CONST_STRPTR id;
        CONST_STRPTR name;

        if (profile == NULL)
            continue;
        idObj = json_object_object_get(profile, "xaiTTSVoiceId");
        nameObj = json_object_object_get(profile, "xaiTTSVoiceName");
        id = idObj != NULL ? json_object_get_string(idObj) : NULL;
        name = nameObj != NULL ? json_object_get_string(nameObj) : NULL;
        if (id != NULL && name != NULL && name[0] != '\0' &&
            strcmp(id, voiceId) == 0) {
            strncpy(dest, name, destSize - 1);
            dest[destSize - 1] = '\0';
            json_object_put(arr);
            return TRUE;
        }
    }
    json_object_put(arr);
    return FALSE;
}

static void resolveSpeechProfileVoiceName(STRPTR dest, ULONG destSize,
                                          CONST_STRPTR profileInfo) {
    CONST_STRPTR label = STRING_MENU_OPENAI_VOICE;
    CONST_STRPTR cursor;
    UBYTE prefix[64];
    UBYTE voiceId[64];
    UBYTE voiceName[128];
    ULONG prefixLen;

    if (dest == NULL || destSize == 0)
        return;
    dest[0] = '\0';
    if (profileInfo == NULL || profileInfo[0] == '\0')
        return;
    strncpy(dest, profileInfo, destSize - 1);
    dest[destSize - 1] = '\0';
    snprintf(prefix, sizeof(prefix), "%s: ", label != NULL ? label : "Voice");
    prefixLen = strlen(prefix);
    cursor = dest;
    while (cursor[0] != '\0') {
        if (strncmp(cursor, prefix, prefixLen) == 0) {
            CONST_STRPTR start = cursor + prefixLen;
            CONST_STRPTR end = start;
            while (*end != '\0' && *end != '\n')
                end++;
            if ((ULONG)(end - start) >= sizeof(voiceId))
                break;
            memcpy(voiceId, start, (ULONG)(end - start));
            voiceId[end - start] = '\0';
            if (copyXAIVoiceNameForId(voiceId, voiceName, sizeof(voiceName)) &&
                strcmp(voiceName, voiceId) != 0) {
                UBYTE rebuilt[768];
                ULONG head = (ULONG)(cursor - dest);
                snprintf(rebuilt, sizeof(rebuilt), "%.*s%s%s%s", (int)head,
                         dest, prefix, voiceName, end);
                strncpy(dest, rebuilt, destSize - 1);
                dest[destSize - 1] = '\0';
            }
            break;
        }
        while (*cursor != '\0' && *cursor != '\n')
            cursor++;
        if (*cursor == '\n')
            cursor++;
    }
}

static void setSpeechPlayerDisplay(CONST_STRPTR filePath,
                                   CONST_STRPTR profileInfo) {
    CONST_STRPTR contents;
    UBYTE resolved[768];

    resolved[0] = '\0';
    if (profileInfo != NULL && profileInfo[0] != '\0') {
        resolveSpeechProfileVoiceName(resolved, sizeof(resolved), profileInfo);
        contents = resolved;
    } else if (currentSpeech == NULL)
        contents = (CONST_STRPTR)STRING_NO_SPEECH_SELECTED;
    else
        contents = (CONST_STRPTR)STRING_SPEECH_NO_PROFILE;
    if (speechProfileInfo != NULL)
        set(speechProfileInfo, MUIA_Text_Contents, contents);
    if (speechWaveform != NULL && speechWaveformClass != NULL)
        speechWaveformSetFile(speechWaveform, filePath);
}

void updatePlayButton() {
    BOOL playing;
    BOOL paused;
    BOOL hasSpeech;
    ULONG position;
    static BOOL lastPlaying = (BOOL)-1;
    static BOOL lastPaused = (BOOL)-1;
    static BOOL lastHasSpeech = (BOOL)-1;
    static ULONG lastPosition = (ULONG)-1;

    updateActionButtonLabels(FALSE);

    if (speechWaveform == NULL || speechWaveformClass == NULL)
        return;

    playing = isSpeechPlaying();
    paused = isSpeechPaused();
    hasSpeech = currentSpeech != NULL && !requestInterfaceBusy;
    position = speechPlaybackPositionMs();
    if (playing == lastPlaying && paused == lastPaused &&
        hasSpeech == lastHasSpeech && position == lastPosition)
        return;

    SetAttrs(speechWaveform, MUIA_SpeechWaveform_Position, position,
             MUIA_SpeechWaveform_Playing, playing ? TRUE : FALSE,
             MUIA_SpeechWaveform_Paused, paused ? TRUE : FALSE,
             MUIA_SpeechWaveform_HasSpeech, hasSpeech ? TRUE : FALSE,
             TAG_DONE);
    lastPlaying = playing;
    lastPaused = paused;
    lastHasSpeech = hasSpeech;
    lastPosition = position;
}

static void updateSpeechControls() {
    ULONG count = pendingSpeechFilesInitialized
                      ? chatFileCount(&pendingSpeechFiles)
                      : 0;
    UBYTE summary[128];
    STRPTR text = duplicateEditorText(speechInputTextEditor);
    BOOL hasText = text != NULL && strlen(text) > 0;
    ULONG speakFiles = FALSE;
    BOOL canGenerate;

    if (count == 0)
        strcpy(summary, STRING_NO_FILES_ATTACHED);
    else
        snprintf(summary, sizeof(summary), STRING_FILES_ATTACHED_FORMAT,
                 count);
    if (speechAttachmentSummaryText != NULL)
        set(speechAttachmentSummaryText, MUIA_Text_Contents, summary);
    if (clearSpeechFilesButton != NULL)
        set(clearSpeechFilesButton, MUIA_Disabled, count == 0);
    if (speakFileContentsCheckbox != NULL) {
        set(speakFileContentsCheckbox, MUIA_Disabled, count == 0);
        if (count == 0)
            set(speakFileContentsCheckbox, MUIA_Selected, FALSE);
        get(speakFileContentsCheckbox, MUIA_Selected, &speakFiles);
    }
    canGenerate = hasText || (count > 0 && speakFiles);
    if (!requestInterfaceBusy) {
        if (generateSpeechTextButton != NULL)
            set(generateSpeechTextButton, MUIA_Disabled, !hasText);
        if (generateSpeechButton != NULL)
            set(generateSpeechButton, MUIA_Disabled, !canGenerate);
        if (regenerateSpeechButton != NULL)
            set(regenerateSpeechButton, MUIA_Disabled,
                !canGenerate || currentSpeech == NULL);
    }
    if (saveSpeechCopyButton != NULL)
        set(saveSpeechCopyButton, MUIA_Disabled, currentSpeech == NULL);
    updatePlayButton();
    if (text != NULL)
        FreeVec(text);
}

static void setSpeechGenerationControlsDisabled(BOOL disabled) {
    ULONG count = pendingSpeechFilesInitialized
                      ? chatFileCount(&pendingSpeechFiles)
                      : 0;
    if (newSpeechButton != NULL)
        set(newSpeechButton, MUIA_Disabled, disabled);
    if (deleteSpeechButton != NULL)
        set(deleteSpeechButton, MUIA_Disabled, disabled);
    if (speechInputTextEditor != NULL)
        set(speechInputTextEditor, MUIA_Disabled, disabled);
    if (attachSpeechFilesButton != NULL)
        set(attachSpeechFilesButton, MUIA_Disabled, disabled);
    if (clearSpeechFilesButton != NULL)
        set(clearSpeechFilesButton, MUIA_Disabled, disabled || count == 0);
    if (regenerateSpeechButton != NULL)
        set(regenerateSpeechButton, MUIA_Disabled, disabled);
    if (generateSpeechTextButton != NULL)
        set(generateSpeechTextButton, MUIA_Disabled, disabled);
    if (saveSpeechCopyButton != NULL)
        set(saveSpeechCopyButton, MUIA_Disabled, disabled);
    if (speechListObject != NULL)
        set(speechListObject, MUIA_Disabled, disabled);
}

static void setChatControlsDisabled(BOOL disabled) {
    if (newChatButton != NULL)
        set(newChatButton, MUIA_Disabled, disabled);
    if (deleteChatButton != NULL)
        set(deleteChatButton, MUIA_Disabled, disabled);
    if (chatInputTextEditor != NULL)
        set(chatInputTextEditor, MUIA_Disabled, disabled);
    if (attachFilesButton != NULL)
        set(attachFilesButton, MUIA_Disabled, disabled);
    if (clearAttachmentsButton != NULL)
        set(clearAttachmentsButton, MUIA_Disabled,
            disabled || !pendingChatFilesInitialized ||
                chatFileCount(&pendingChatFiles) == 0);
    if (conversationListObject != NULL)
        set(conversationListObject, MUIA_Disabled, disabled);
}

static void setRequestInterfaceBusy(BOOL busy) {
    requestInterfaceBusy = busy;
    setChatControlsDisabled(busy);
    setImageGenerationControlsDisabled(busy);
    setSpeechGenerationControlsDisabled(busy);
    setPrimaryActionButtonsEnabled();
    setActionButtonsStopMode(busy);
    updatePlayButton();
}

static void finishActiveRequest(BOOL restoreReadyStatus) {
    BOOL cancelled = isRequestCancelled();
    endCancellableRequest();
    setRequestInterfaceBusy(FALSE);
    hideLoadingBar();
    if (cancelled)
        updateStatusBar(STRING_REQUEST_CANCELLED, yellowPen);
    else if (restoreReadyStatus)
        updateStatusBar(STRING_READY, greenPen);
    updateAttachmentControls();
    updateReferenceImageControls();
    updateSpeechControls();
    updateResponseFileControl();
}

/* Everything the user can touch while an image request is in flight. Clear is
 * a special case: it stays disabled afterwards when there is nothing to clear.
 */
static void setImageGenerationControlsDisabled(BOOL disabled) {
    set(newImageButton, MUIA_Disabled, disabled);
    set(deleteImageButton, MUIA_Disabled, disabled);
    set(imageInputTextEditor, MUIA_Disabled, disabled);
    set(attachReferenceImagesButton, MUIA_Disabled, disabled);
    set(clearReferenceImagesButton, MUIA_Disabled,
        disabled || chatFileCount(&pendingImageFiles) == 0);
}

static ULONG responseFileCount(struct Conversation *conversation) {
    ULONG count = 0;
    if (conversation == NULL)
        return 0;
    struct ConversationNode *message;
    for (message =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         message->node.mln_Succ != NULL;
         message = (struct ConversationNode *)message->node.mln_Succ) {
        if (strcmp(message->role, "assistant") != 0)
            continue;
        struct ChatFile *file;
        for (file = (struct ChatFile *)message->files.mlh_Head;
             file->node.mln_Succ != NULL;
             file = (struct ChatFile *)file->node.mln_Succ) {
            if (file->fileId != NULL || file->downloadUrl != NULL ||
                file->path != NULL)
                count++;
        }
    }
    return count;
}

static void updateResponseFileControl() {
    if (saveResponseFilesButton != NULL)
        set(saveResponseFilesButton, MUIA_Disabled,
            responseFileCount(currentConversation) == 0);
}

void freeMainWindowFileState() {
    if (pendingChatFilesInitialized) {
        freeChatFiles(&pendingChatFiles);
        pendingChatFilesInitialized = FALSE;
    }
    if (pendingImageFilesInitialized) {
        freeChatFiles(&pendingImageFiles);
        pendingImageFilesInitialized = FALSE;
    }
    if (pendingSpeechFilesInitialized) {
        freeChatFiles(&pendingSpeechFiles);
        pendingSpeechFilesInitialized = FALSE;
    }
}

/** Whether the busy meter page is currently the visible one */
static BOOL loadingBarVisible = FALSE;

/**
 * Shows the loading bar and starts the busy meter animating
 **/
void showLoadingBar() {
    if (loadingBarVisible)
        return;
    loadingBarVisible = TRUE;
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
    set(loadingBarGroup, MUIA_Group_ActivePage, 1);
}

/**
 * Stops the busy meter and hides the loading bar, leaving it blank
 **/
void hideLoadingBar() {
    if (!loadingBarVisible)
        return;
    loadingBarVisible = FALSE;
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
            if (strcmp((const char *)m->role, "user") == 0 &&
                m->content != NULL && strlen((const char *)m->content) > 0) {
                STRPTR out = AllocVec(CONVERSATION_TITLE_FALLBACK_MAX + 1,
                                      MEMF_ANY | MEMF_CLEAR);
                if (out == NULL)
                    return NULL;
                ULONG pos = 0;
                const UTF8 *s = m->content;
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
    if (entry == NULL || entry->name == NULL) {
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

HOOKPROTONHNO(ConstructSpeechLI_TextFunc, APTR,
              struct NList_ConstructMessage *ncm) {
    return ncm->entry;
}
MakeHook(ConstructSpeechLI_TextHook, ConstructSpeechLI_TextFunc);

HOOKPROTONHNO(DestructSpeechLI_TextFunc, void,
              struct NList_DestructMessage *ndm) {
    struct GeneratedSpeech *entry =
        (struct GeneratedSpeech *)ndm->entry;
    if (entry != NULL) {
        if (entry->title != NULL)
            FreeVec(entry->title);
        if (entry->filePath != NULL)
            FreeVec(entry->filePath);
        if (entry->text != NULL)
            FreeVec(entry->text);
        if (entry->profileInfo != NULL)
            FreeVec(entry->profileInfo);
        FreeVec(entry);
    }
}
MakeHook(DestructSpeechLI_TextHook, DestructSpeechLI_TextFunc);

HOOKPROTONHNO(DisplaySpeechLI_TextFunc, void,
              struct NList_DisplayMessage *ndm) {
    struct GeneratedSpeech *entry =
        (struct GeneratedSpeech *)ndm->entry;
    ndm->strings[0] = entry != NULL && entry->title != NULL
                          ? entry->title
                          : (STRPTR)"";
}
MakeHook(DisplaySpeechLI_TextHook, DisplaySpeechLI_TextFunc);

HOOKPROTONHNONP(SpeechRowClickedFunc, void) {
    struct GeneratedSpeech *entry = NULL;
    DoMethod(speechListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &entry);
    if (entry != NULL) {
        currentSpeech = entry;
        setEditorText(speechInputTextEditor, entry->text);
        stopSpeech();
        if (entry->filePath != NULL)
            loadSpeechPlayback(entry->filePath);
        setSpeechPlayerDisplay(entry->filePath, entry->profileInfo);
    }
    updateSpeechControls();
}
MakeHook(SpeechRowClickedHook, SpeechRowClickedFunc);

HOOKPROTONHNONP(ConversationRowClickedFunc, void) {
    set(chatInputTextEditor, MUIA_TextEditor_FixedFont, TRUE);
    struct Conversation *conversation;
    DoMethod(conversationListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &conversation);
    if (conversation) {
        currentConversation = conversation;
        displayConversation(currentConversation);
        updateResponseFileControl();
        DoMethod(chatInputTextEditor, MUIM_GoActive);
    }
}
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
        set(editImageButton, MUIA_Disabled, FALSE);
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
    DoMethod(chatOutputTextEditor, MUIM_NList_Clear);
    updateResponseFileControl();
    DoMethod(chatInputTextEditor, MUIM_GoActive);
}
MakeHook(NewChatButtonClickedHook, NewChatButtonClickedFunc);

HOOKPROTONHNONP(DeleteChatButtonClickedFunc, void) {
    DoMethod(conversationListObject, MUIM_NList_Remove,
             MUIV_NList_Remove_Active);
    currentConversation = NULL;
    DoMethod(chatOutputTextEditor, MUIM_NList_Clear);
    updateResponseFileControl();
    saveConversations();
}
MakeHook(DeleteChatButtonClickedHook, DeleteChatButtonClickedFunc);

static void addPendingFile(struct MinList *files, CONST_STRPTR path,
                           CONST_STRPTR name, BOOL imagesOnly) {
    if (path == NULL || name == NULL || chatFilePathAlreadyPresent(files, path))
        return;
    UTF8 *nameUTF8 = CodesetsUTF8Create(
        CSA_SourceCodeset, (Tag)systemCodeset, CSA_Source, (Tag)name,
        CSA_MapForeignChars, TRUE, TAG_DONE);
    /* Work out the type now rather than when the message is sent: reading the
     * magic database takes a moment, and here it is part of an action the user
     * just asked for. NULL simply means no database is installed, in which case
     * the type is guessed from the file name at send time instead. */
    STRPTR detectedMime = detectMimeTypeFromContents(path);
    /* An image request can only carry pictures, and what the provider says
     * about anything else is not something a user could act on, so the file is
     * turned away here instead. */
    if (imagesOnly && !attachmentLooksLikeImage(path, detectedMime)) {
        displayError(STRING_ERROR_NOT_AN_IMAGE);
    } else if (!addChatFile(files, path,
                            nameUTF8 != NULL ? nameUTF8 : (UTF8 *)name,
                            detectedMime, NULL, NULL, NULL)) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
    }
    if (detectedMime != NULL)
        FreeVec(detectedMime); /* addChatFile took its own copy */
    if (nameUTF8 != NULL)
        CodesetsFreeA(nameUTF8, NULL);
}

static void requestFilesIntoList(struct MinList *files, CONST_STRPTR title,
                                 BOOL imagesOnly) {
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq == NULL)
        return;
    if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                       title, ASLFR_DoMultiSelect, TRUE, ASLFR_RejectIcons,
                       TRUE, TAG_DONE)) {
        if (fileReq->fr_NumArgs > 0 && fileReq->fr_ArgList != NULL) {
            for (LONG i = 0; i < fileReq->fr_NumArgs; i++) {
                UBYTE fullPath[1024] = {0};
                struct WBArg *argument = &fileReq->fr_ArgList[i];
                if (NameFromLock(argument->wa_Lock, fullPath,
                                 sizeof(fullPath))) {
                    AddPart(fullPath, argument->wa_Name, sizeof(fullPath));
                    addPendingFile(files, fullPath, argument->wa_Name,
                                   imagesOnly);
                }
            }
        } else if (fileReq->fr_File != NULL &&
                   strlen(fileReq->fr_File) > 0) {
            ULONG fullPathLength = strlen(fileReq->fr_Drawer) +
                                   strlen(fileReq->fr_File) + 2;
            STRPTR fullPath =
                AllocVec(fullPathLength, MEMF_ANY | MEMF_CLEAR);
            if (fullPath != NULL) {
                strncpy(fullPath, fileReq->fr_Drawer,
                        strlen(fileReq->fr_Drawer));
                AddPart(fullPath, fileReq->fr_File, fullPathLength);
                addPendingFile(files, fullPath, fileReq->fr_File, imagesOnly);
                FreeVec(fullPath);
            }
        }
    }
    FreeAslRequest(fileReq);
}

HOOKPROTONHNONP(AttachFilesButtonClickedFunc, void) {
    requestFilesIntoList(&pendingChatFiles, STRING_ATTACH_FILES, FALSE);
    updateAttachmentControls();
}
MakeHook(AttachFilesButtonClickedHook, AttachFilesButtonClickedFunc);

HOOKPROTONHNONP(ClearAttachmentsButtonClickedFunc, void) {
    freeChatFiles(&pendingChatFiles);
    updateAttachmentControls();
}
MakeHook(ClearAttachmentsButtonClickedHook,
         ClearAttachmentsButtonClickedFunc);

HOOKPROTONHNONP(AttachReferenceImagesButtonClickedFunc, void) {
    requestFilesIntoList(&pendingImageFiles, STRING_ATTACH_REFERENCE_IMAGES,
                         TRUE);
    updateReferenceImageControls();
}
MakeHook(AttachReferenceImagesButtonClickedHook,
         AttachReferenceImagesButtonClickedFunc);

HOOKPROTONHNONP(ClearReferenceImagesButtonClickedFunc, void) {
    freeChatFiles(&pendingImageFiles);
    updateReferenceImageControls();
}
MakeHook(ClearReferenceImagesButtonClickedHook,
         ClearReferenceImagesButtonClickedFunc);

static STRPTR readSpeechFile(CONST_STRPTR path) {
    BPTR file;
    LONG length;
    STRPTR contents;
    LONG i;

    if (path == NULL)
        return NULL;
    file = Open(path, MODE_OLDFILE);
    if (file == 0)
        return NULL;
#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    length = Seek(file, 0, OFFSET_BEGINNING);
#elif defined(__AMIGAOS4__)
    length = (LONG)GetFileSize(file);
#else
    {
        struct FileInfoBlock fib;
        ExamineFH64(file, &fib, NULL);
        length = (LONG)fib.fib_Size;
    }
#endif
    if (length <= 0 || length > 8 * 1024 * 1024) {
        Close(file);
        return NULL;
    }
    contents = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
    if (contents == NULL || Read(file, contents, length) != length) {
        if (contents != NULL)
            FreeVec(contents);
        Close(file);
        return NULL;
    }
    Close(file);
    for (i = 0; i < length; i++) {
        if (contents[i] == '\0')
            contents[i] = ' ';
    }
    return contents;
}

static STRPTR buildSpeechText(void) {
    STRPTR result = duplicateEditorText(speechInputTextEditor);
    ULONG selected = FALSE;
    struct ChatFile *file;

    if (result == NULL)
        return NULL;
    get(speakFileContentsCheckbox, MUIA_Selected, &selected);
    if (!selected)
        return result;

    for (file = (struct ChatFile *)pendingSpeechFiles.mlh_Head;
         file->node.mln_Succ != NULL;
         file = (struct ChatFile *)file->node.mln_Succ) {
        STRPTR contents = readSpeechFile(file->path);
        STRPTR expanded;
        ULONG oldLength;
        ULONG addLength;
        if (contents == NULL)
            continue;
        oldLength = strlen(result);
        addLength = strlen(contents);
        expanded = AllocVec(oldLength + addLength + 3,
                            MEMF_ANY | MEMF_CLEAR);
        if (expanded != NULL) {
            strcpy(expanded, result);
            if (oldLength > 0)
                strcat(expanded, "\n\n");
            strcat(expanded, contents);
            FreeVec(result);
            result = expanded;
        }
        FreeVec(contents);
    }
    return result;
}

static STRPTR copySpeechString(CONST_STRPTR text) {
    STRPTR copy;
    ULONG length;

    if (text == NULL)
        return NULL;
    length = strlen(text);
    copy = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
    if (copy != NULL)
        memcpy(copy, text, length);
    return copy;
}

static void appendProfileLine(STRPTR buffer, ULONG size, CONST_STRPTR label,
                              CONST_STRPTR value) {
    ULONG used;

    if (buffer == NULL || label == NULL || value == NULL || value[0] == '\0')
        return;
    used = strlen(buffer);
    if (used + 4 >= size)
        return;
    if (used > 0)
        buffer[used++] = '\n';
    snprintf(buffer + used, size - used, "%s: %s", label, value);
}

static STRPTR formatSpeechProfileInfo(
    const struct SpeechRequestSettings *settings) {
    UBYTE buffer[768];
    CONST_STRPTR voice = NULL;
    CONST_STRPTR model = NULL;
    UBYTE extra[64];

    if (settings == NULL)
        return NULL;
    memset(buffer, 0, sizeof(buffer));
    appendProfileLine(buffer, sizeof(buffer), STRING_SPEECH_PROFILE,
                      settings->activeProfileName);
    if (settings->speechSystem <= SPEECH_SYSTEM_OPENVOX)
        appendProfileLine(buffer, sizeof(buffer), STRING_SPEECH_SYSTEM_LABEL,
                          SPEECH_SYSTEM_NAMES[settings->speechSystem]);

    extra[0] = '\0';
    switch (settings->speechSystem) {
    case SPEECH_SYSTEM_34:
    case SPEECH_SYSTEM_37:
        snprintf(extra, sizeof(extra), "%s, %s",
                 settings->narratorSex ? STRING_NARRATOR_SEX_FEMALE
                                       : STRING_NARRATOR_SEX_MALE,
                 settings->narratorMode ? STRING_NARRATOR_MODE_ROBOTIC
                                        : STRING_NARRATOR_MODE_NATURAL);
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_OPENAI_VOICE,
                          extra);
        snprintf(extra, sizeof(extra), "%u", settings->narratorRate);
        appendProfileLine(buffer, sizeof(buffer), STRING_NARRATOR_RATE_WPM,
                          extra);
        snprintf(extra, sizeof(extra), "%u", settings->narratorPitch);
        appendProfileLine(buffer, sizeof(buffer), STRING_NARRATOR_PITCH_HZ,
                          extra);
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_SPEECH_ACCENT,
                          settings->accentPath);
        break;
    case SPEECH_SYSTEM_FLITE:
        if (settings->fliteVoice <= SPEECH_FLITE_VOICE_SLT)
            voice = SPEECH_FLITE_VOICE_NAMES[settings->fliteVoice];
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_OPENAI_VOICE,
                          voice);
        break;
    case SPEECH_SYSTEM_OPENAI:
        if (settings->openAiTtsVoice <= OPENAI_TTS_VOICE_VERSE)
            voice = OPENAI_TTS_VOICE_NAMES[settings->openAiTtsVoice];
        model = settings->openAiTtsModelId;
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_OPENAI_VOICE,
                          voice);
        appendProfileLine(buffer, sizeof(buffer),
                          STRING_MENU_SPEECH_OPENAI_MODEL, model);
        break;
    case SPEECH_SYSTEM_ELEVENLABS:
        voice = settings->elevenLabsVoiceName;
        if (voice == NULL || voice[0] == '\0')
            voice = settings->elevenLabsVoiceID;
        model = settings->elevenLabsModelName;
        if (model == NULL || model[0] == '\0')
            model = settings->elevenLabsModel;
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_OPENAI_VOICE,
                          voice);
        appendProfileLine(buffer, sizeof(buffer),
                          STRING_MENU_SPEECH_OPENAI_MODEL, model);
        break;
    case SPEECH_SYSTEM_XAI: {
        UBYTE xaiName[128];
        CONST_STRPTR voiceId = settings->xaiVoiceId;
        if (voiceId == NULL || voiceId[0] == '\0') {
            if (settings->xaiVoice >= 0 &&
                XAI_TTS_VOICE_NAMES[settings->xaiVoice] != NULL)
                voiceId = XAI_TTS_VOICE_NAMES[settings->xaiVoice];
        }
        voice = settings->xaiVoiceName;
        if (voice == NULL || voice[0] == '\0') {
            if (copyXAIVoiceNameForId(voiceId, xaiName, sizeof(xaiName)))
                voice = xaiName;
            else
                voice = voiceId;
        }
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_OPENAI_VOICE,
                          voice);
        if (voice != NULL && voiceId != NULL && voice[0] != '\0' &&
            strcmp(voice, voiceId) != 0)
            configSetXAITTSVoiceName(voice);
    }
        appendProfileLine(buffer, sizeof(buffer), STRING_SPEECH_LANGUAGE,
                          settings->xaiLanguage);
        break;
    case SPEECH_SYSTEM_OPENVOX:
        appendProfileLine(buffer, sizeof(buffer), STRING_MENU_OPENAI_VOICE,
                          settings->openVoxVoice);
        appendProfileLine(buffer, sizeof(buffer),
                          STRING_MENU_SPEECH_OPENAI_MODEL,
                          settings->openVoxModel);
        appendProfileLine(buffer, sizeof(buffer), STRING_SPEECH_LANGUAGE,
                          settings->openVoxLanguage);
        break;
    }
    if (buffer[0] == '\0')
        return NULL;
    return copySpeechString(buffer);
}

static STRPTR speechTitle(CONST_STRPTR text) {
    ULONG length = text != NULL ? strlen(text) : 0;
    STRPTR title;
    if (length > 32)
        length = 32;
    title = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
    if (title != NULL && length > 0)
        memcpy(title, text, length);
    return title;
}

static void generateSpeech(BOOL regenerate) {
    STRPTR text;
    UBYTE path[64];
    UBYTE id[12];
    CONST_STRPTR idChars = "abcdefghijklmnopqrstuvwxyz0123456789";
    struct GeneratedSpeech *entry;
    AudioFormat format = AUDIO_FORMAT_WAV;
    BPTR speechDir;
    UBYTE i;
    struct SpeechRequestSettings settings;
    BOOL generated;
    BOOL createdNewEntry = FALSE;
    STRPTR profileInfo = NULL;

    memset(id, 0, sizeof(id));
    memset(path, 0, sizeof(path));
    entry = regenerate ? currentSpeech : NULL;
    text = buildSpeechText();
    if (text == NULL || strlen(text) == 0) {
        if (text != NULL)
            FreeVec(text);
        return;
    }
    speechDir = CreateDir("AMIGAGPT:speech");
    if (speechDir != 0)
        UnLock(speechDir);
    if (entry != NULL && entry->filePath != NULL) {
        strncpy(path, entry->filePath, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
    } else {
        srand(time(NULL));
        for (i = 0; i < 10; i++)
            id[i] = idChars[rand() % strlen(idChars)];
        snprintf(path, sizeof(path), "AMIGAGPT:speech/%s.wav", id);
    }

    beginCancellableRequest();
    setRequestInterfaceBusy(TRUE);
    updateStatusBar(STRING_GENERATING_SPEECH, yellowPen);
    showLoadingBar();
    configGetSpeechRequestSettings(&settings);
    generated = speakTextWithSettings(text, path, &format, &settings);
    if (generated && !isRequestCancelled())
        profileInfo = formatSpeechProfileInfo(&settings);
    configFreeSpeechRequestSettings(&settings);
    if (!generated || isRequestCancelled()) {
        if (entry == NULL)
            deleteDiskFile(path);
        FreeVec(text);
        finishActiveRequest(generated && !isRequestCancelled());
        return;
    }

    if (entry == NULL) {
        entry = AllocVec(sizeof(*entry), MEMF_ANY | MEMF_CLEAR);
        if (entry == NULL) {
            deleteDiskFile(path);
            FreeVec(text);
            if (profileInfo != NULL)
                FreeVec(profileInfo);
            finishActiveRequest(FALSE);
            displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
            return;
        }
        entry->filePath = AllocVec(strlen(path) + 1, MEMF_ANY | MEMF_CLEAR);
        if (entry->filePath != NULL)
            strcpy(entry->filePath, path);
        else {
            deleteDiskFile(path);
            FreeVec(entry);
            FreeVec(text);
            if (profileInfo != NULL)
                FreeVec(profileInfo);
            finishActiveRequest(FALSE);
            displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
            return;
        }
        DoMethod(speechListObject, MUIM_NList_InsertSingle, entry,
                 MUIV_NList_Insert_Top);
        DoMethod(speechListObject, MUIM_NList_SetActive, 0, NULL);
        createdNewEntry = TRUE;
    } else {
        if (entry->text != NULL)
            FreeVec(entry->text);
        if (entry->title != NULL)
            FreeVec(entry->title);
        if (entry->profileInfo != NULL)
            FreeVec(entry->profileInfo);
    }
    entry->text = text;
    entry->title = speechTitle(text);
    entry->profileInfo = profileInfo;
    currentSpeech = entry;
    DoMethod(speechListObject, MUIM_NList_Redraw, MUIV_NList_Redraw_Active);
    if (saveSpeechHistory() != RETURN_OK && createdNewEntry)
        deleteDiskFile(path);
    loadSpeechPlayback(path);
    setSpeechPlayerDisplay(path, entry->profileInfo);
    finishActiveRequest(TRUE);
}

HOOKPROTONHNONP(NewSpeechButtonClickedFunc, void) {
    currentSpeech = NULL;
    setEditorText(speechInputTextEditor, "");
    freeChatFiles(&pendingSpeechFiles);
    unloadSpeechPlayback();
    setSpeechPlayerDisplay(NULL, NULL);
    updateSpeechControls();
    DoMethod(speechInputTextEditor, MUIM_GoActive);
}
MakeHook(NewSpeechButtonClickedHook, NewSpeechButtonClickedFunc);

HOOKPROTONHNONP(DeleteSpeechButtonClickedFunc, void) {
    struct GeneratedSpeech *entry = NULL;
    DoMethod(speechListObject, MUIM_NList_GetEntry,
             MUIV_NList_GetEntry_Active, &entry);
    if (entry != NULL && entry->filePath != NULL)
        deleteDiskFile(entry->filePath);
    DoMethod(speechListObject, MUIM_NList_Remove, MUIV_NList_Remove_Active);
    currentSpeech = NULL;
    setEditorText(speechInputTextEditor, "");
    stopSpeech();
    setSpeechPlayerDisplay(NULL, NULL);
    saveSpeechHistory();
    updateSpeechControls();
}
MakeHook(DeleteSpeechButtonClickedHook, DeleteSpeechButtonClickedFunc);

HOOKPROTONHNONP(GenerateSpeechButtonClickedFunc, void) {
    if (abortIfRequestBusy())
        return;
    generateSpeech(FALSE);
}
MakeHook(GenerateSpeechButtonClickedHook, GenerateSpeechButtonClickedFunc);

HOOKPROTONHNONP(RegenerateSpeechButtonClickedFunc, void) {
    generateSpeech(TRUE);
}
MakeHook(RegenerateSpeechButtonClickedHook,
         RegenerateSpeechButtonClickedFunc);

HOOKPROTONHNONP(PlaySpeechButtonClickedFunc, void) {
    if (isSpeechPlaying() || currentSpeech == NULL ||
        currentSpeech->filePath == NULL) {
        updatePlayButton();
        return;
    }
    if (speechPlaybackDurationMs() == 0)
        loadSpeechPlayback(currentSpeech->filePath);
    if (speechPlaybackPositionMs() >= speechPlaybackDurationMs() &&
        speechPlaybackDurationMs() > 0)
        seekSpeech(0);
    if (!startSpeechPlayback())
        playSpeechFile(currentSpeech->filePath);
    updatePlayButton();
}

HOOKPROTONHNONP(PauseSpeechButtonClickedFunc, void) {
    pauseSpeech();
    updatePlayButton();
}

HOOKPROTONHNONP(StopSpeechButtonClickedFunc, void) {
    stopSpeech();
    updatePlayButton();
}

HOOKPROTONHNONP(RewindSpeechButtonClickedFunc, void) {
    rewindSpeech();
    updatePlayButton();
}

HOOKPROTONHNONP(SpeechWaveformCommandFunc, void) {
    ULONG command = 0;

    if (speechWaveform != NULL)
        get(speechWaveform, MUIA_SpeechWaveform_Command, &command);
    switch (command) {
    case MUIV_SpeechWaveform_Command_Play:
        PlaySpeechButtonClickedFunc();
        break;
    case MUIV_SpeechWaveform_Command_Pause:
        PauseSpeechButtonClickedFunc();
        break;
    case MUIV_SpeechWaveform_Command_Stop:
        StopSpeechButtonClickedFunc();
        break;
    case MUIV_SpeechWaveform_Command_Rewind:
        RewindSpeechButtonClickedFunc();
        break;
    }
}
MakeHook(SpeechWaveformCommandHook, SpeechWaveformCommandFunc);

HOOKPROTONHNONP(SpeechWaveformSeekFunc, void) {
    ULONG positionMs = 0;
    if (speechWaveform != NULL)
        get(speechWaveform, MUIA_SpeechWaveform_Seek, &positionMs);
    seekSpeech(positionMs);
    updatePlayButton();
}
MakeHook(SpeechWaveformSeekHook, SpeechWaveformSeekFunc);

HOOKPROTONHNONP(AttachSpeechFilesButtonClickedFunc, void) {
    ULONG oldCount = chatFileCount(&pendingSpeechFiles);
    requestFilesIntoList(&pendingSpeechFiles, STRING_ATTACH_FILES, FALSE);
    if (oldCount == 0 && chatFileCount(&pendingSpeechFiles) > 0)
        set(speakFileContentsCheckbox, MUIA_Selected, TRUE);
    updateSpeechControls();
}
MakeHook(AttachSpeechFilesButtonClickedHook,
         AttachSpeechFilesButtonClickedFunc);

HOOKPROTONHNONP(ClearSpeechFilesButtonClickedFunc, void) {
    freeChatFiles(&pendingSpeechFiles);
    updateSpeechControls();
}
MakeHook(ClearSpeechFilesButtonClickedHook,
         ClearSpeechFilesButtonClickedFunc);

HOOKPROTONHNONP(SpeechTextChangedFunc, void) {
    if (!isAROS && speechInputTextEditor != NULL)
        set(speechInputTextEditor, MUIA_TextEditor_HasChanged, FALSE);
    updateSpeechControls();
}
MakeHook(SpeechTextChangedHook, SpeechTextChangedFunc);

HOOKPROTONHNONP(SaveSpeechCopyButtonClickedFunc, void) {
    STRPTR filePath;
    STRPTR fileExtension;
    UBYTE fileName[16];
    struct FileRequester *fileReq;

    if (currentSpeech == NULL || currentSpeech->filePath == NULL)
        return;
    filePath = currentSpeech->filePath;
    fileExtension = strrchr(filePath, '.');
    if (fileExtension == NULL)
        fileExtension = ".wav";
    snprintf(fileName, sizeof(fileName), "speech%s", fileExtension);
    fileReq = AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq != NULL) {
        if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                           STRING_SAVE_SPEECH_COPY, ASLFR_InitialFile, fileName,
                           ASLFR_InitialDrawer, "SYS:", ASLFR_DoSaveMode, TRUE,
                           TAG_DONE)) {
            STRPTR savePath = fileReq->fr_Drawer;
            STRPTR saveName = fileReq->fr_File;
            UWORD fullPathLength = strlen(savePath) + strlen(saveName) + 2;
            STRPTR fullPath = AllocVec(fullPathLength, MEMF_CLEAR);
            if (fullPath != NULL) {
                strncpy(fullPath, savePath, strlen(savePath));
                AddPart(fullPath, saveName, fullPathLength);
                copyFile(filePath, fullPath);
                FreeVec(fullPath);
            }
        }
        FreeAslRequest(fileReq);
    }
}
MakeHook(SaveSpeechCopyButtonClickedHook, SaveSpeechCopyButtonClickedFunc);

HOOKPROTONHNONP(GenerateSpeechTextButtonClickedFunc, void) {
    STRPTR text = duplicateEditorText(speechInputTextEditor);
    struct Conversation *conversation;
    struct ConversationNode *userMessage;
    struct ChatRequestSettings chatSettings;
    struct json_object **responses;
    UTF8 *textUTF8;
    UTF8 *content;
    STRPTR displayText;

    if (text == NULL || strlen(text) == 0) {
        if (text != NULL)
            FreeVec(text);
        return;
    }

    conversation = newConversation();
    if (conversation == NULL) {
        FreeVec(text);
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        return;
    }
    textUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                                  CSA_Source, (Tag)text, CSA_MapForeignChars,
                                  TRUE, TAG_DONE);
    userMessage = addTextToConversation(
        conversation, textUTF8 != NULL ? textUTF8 : (UTF8 *)text, "user");
    if (textUTF8 != NULL)
        CodesetsFreeA(textUTF8, NULL);
    if (userMessage == NULL ||
        !copyChatFiles(&userMessage->files, &pendingSpeechFiles)) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        freeConversation(conversation);
        FreeVec(text);
        return;
    }

    beginCancellableRequest();
    setRequestInterfaceBusy(TRUE);
    updateStatusBar(STRING_SENDING_MESSAGE, yellowPen);
    showLoadingBar();
    configGetActiveChatRequestSettings(&chatSettings);
    setConversationSystem(conversation, chatSettings.chatSystem);
    responses = postChatMessageToOpenAI(
        conversation, chatSettings.host, chatSettings.port, chatSettings.useSSL,
        chatSettings.model, chatSettings.apiKey, FALSE, chatSettings.useProxy,
        chatSettings.proxyHost, chatSettings.proxyPort,
        chatSettings.proxyUsesSSL, chatSettings.proxyRequiresAuth,
        chatSettings.proxyUsername, chatSettings.proxyPassword,
        chatSettings.webSearchEnabled, FALSE, FALSE, chatSettings.apiEndpoint,
        chatSettings.apiEndpointUrl, chatSettings.authorizationType,
        chatSettings.customHeaders);
    if (isRequestCancelled()) {
        if (responses != NULL) {
            if (responses[0] != NULL)
                json_object_put(responses[0]);
            FreeVec(responses);
        }
        freeConversation(conversation);
        FreeVec(text);
        finishActiveRequest(FALSE);
        return;
    }
    if (responses == NULL || responses[0] == NULL) {
        if (responses != NULL)
            FreeVec(responses);
        freeConversation(conversation);
        FreeVec(text);
        finishActiveRequest(FALSE);
        displayError(STRING_ERROR_CONNECTING_OPENAI);
        return;
    }
    content = getMessageContentFromJson(responses[0], FALSE, FALSE,
                                        chatSettings.apiEndpoint);
    if (content != NULL && strlen((char *)content) > 0) {
        displayText = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)content,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        setEditorText(speechInputTextEditor,
                      displayText != NULL ? displayText : (STRPTR)content);
        if (displayText != NULL)
            CodesetsFreeA(displayText, NULL);
        if (speakFileContentsCheckbox != NULL)
            set(speakFileContentsCheckbox, MUIA_Selected, FALSE);
    }
    json_object_put(responses[0]);
    FreeVec(responses);
    freeConversation(conversation);
    FreeVec(text);
    finishActiveRequest(TRUE);
}
MakeHook(GenerateSpeechTextButtonClickedHook,
         GenerateSpeechTextButtonClickedFunc);


HOOKPROTONHNONP(SaveResponseFilesButtonClickedFunc, void) {
    if (currentConversation == NULL ||
        responseFileCount(currentConversation) == 0)
        return;
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq == NULL)
        return;
    if (!AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                        STRING_SAVE_RESPONSE_FILES, ASLFR_DrawersOnly, TRUE,
                        ASLFR_InitialDrawer, "SYS:", TAG_DONE)) {
        FreeAslRequest(fileReq);
        return;
    }

    struct ChatRequestSettings chatSettings;
    configGetActiveChatRequestSettings(&chatSettings);
    ULONG savedCount = 0;
    struct ConversationNode *message;
    for (message =
             (struct ConversationNode *)currentConversation->messages->mlh_Head;
         message->node.mln_Succ != NULL;
         message = (struct ConversationNode *)message->node.mln_Succ) {
        if (strcmp(message->role, "assistant") != 0)
            continue;
        struct ChatFile *file;
        for (file = (struct ChatFile *)message->files.mlh_Head;
             file->node.mln_Succ != NULL;
             file = (struct ChatFile *)file->node.mln_Succ) {
            STRPTR systemName = CodesetsUTF8ToStr(
                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                (Tag)file->name, CSA_MapForeignChars, TRUE, TAG_DONE);
            CONST_STRPTR usableName = chatFileSafeName(
                systemName != NULL ? systemName : (STRPTR)file->name);
            STRPTR destination =
                uniqueChatFileDestination(fileReq->fr_Drawer, usableName);
            if (destination == NULL) {
                if (systemName != NULL)
                    CodesetsFreeA(systemName, NULL);
                continue;
            }

            if (saveChatFileToPath(
                    file, destination, chatSettings.host, chatSettings.port,
                    chatSettings.useSSL, chatSettings.apiKey,
                    chatSettings.useProxy, chatSettings.proxyHost,
                    chatSettings.proxyPort, chatSettings.proxyUsesSSL,
                    chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
                    chatSettings.proxyPassword, chatSettings.apiEndpointUrl,
                    chatSettings.authorizationType,
                    chatSettings.customHeaders) == RETURN_OK)
                savedCount++;
            FreeVec(destination);
            if (systemName != NULL)
                CodesetsFreeA(systemName, NULL);
        }
    }
    FreeAslRequest(fileReq);
    if (savedCount > 0) {
        UBYTE statusMessage[128];
        snprintf(statusMessage, sizeof(statusMessage),
                 STRING_RESPONSE_FILES_SAVED_FORMAT, savedCount);
        updateStatusBar(statusMessage, greenPen);
        saveConversations();
    }
}
MakeHook(SaveResponseFilesButtonClickedHook,
         SaveResponseFilesButtonClickedFunc);

HOOKPROTONHNONP(SendMessageButtonClickedFunc, void) {
    if (abortIfRequestBusy())
        return;
    if (isSpeechPlaying()) {
        stopSpeech();
        updatePlayButton();
        return;
    }
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

HOOKPROTONHNONP(NewImageButtonClickedFunc, void) {
    currentImage = NULL;
    freeChatFiles(&pendingImageFiles);
    updateReferenceImageControls();
    set(imageInputTextEditor, MUIA_Disabled, FALSE);
    set(createImageButton, MUIA_Disabled, FALSE);
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    set(editImageButton, MUIA_Disabled, TRUE);
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
    set(editImageButton, MUIA_Disabled, TRUE);
    currentImage = NULL;
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

/* Start a fresh prompt with the image on screen attached as its reference, so
 * that "make the sky darker" is one click away from a picture that just came
 * back. The list selection is dropped -- the next Create adds a new entry
 * rather than replacing this one -- but the preview stays put so the user can
 * still see what they are working from. */
HOOKPROTONHNONP(EditImageButtonClickedFunc, void) {
    struct GeneratedImage *source = currentImage;
    if (source == NULL || source->filePath == NULL)
        return;
    CONST_STRPTR name = FilePart(source->filePath);
    if (name == NULL || *name == '\0')
        name = source->filePath;
    freeChatFiles(&pendingImageFiles);
    addPendingFile(&pendingImageFiles, source->filePath, name, TRUE);
    updateReferenceImageControls();

    currentImage = NULL;
    if (isAROS) {
        set(imageInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(imageInputTextEditor, MUIM_TextEditor_ClearText);
    }
    set(imageInputTextEditor, MUIA_Disabled, FALSE);
    set(createImageButton, MUIA_Disabled, FALSE);
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    set(editImageButton, MUIA_Disabled, TRUE);
    DoMethod(imageInputTextEditor, MUIM_GoActive);
}
MakeHook(EditImageButtonClickedHook, EditImageButtonClickedFunc);

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
    if (abortIfRequestBusy())
        return;
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
    set(editImageButton, MUIA_Disabled, TRUE);
    beginCancellableRequest();
    setRequestInterfaceBusy(TRUE);
    showLoadingBar();
    updateStatusBar(STRING_PREPARING_REQUEST, yellowPen);
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
        configGetImageFormat(), imageSettings.imageApiEndpoint,
        &pendingImageFiles);

    if (response == NULL) {
        if (!isRequestCancelled())
            displayError(STRING_ERROR_CONNECTION);
        finishActiveRequest(FALSE);
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
        finishActiveRequest(FALSE);
        json_object_put(response);
        if (!isAROS) {
            FreeVec(text);
        }
        return;
    }

    struct array_list *data =
        json_object_get_array(json_object_object_get(response, "data"));
    struct json_object *dataObject = (struct json_object *)data->array[0];

    STRPTR b64 =
        json_object_get_string(json_object_object_get(dataObject, "b64_json"));

    STRPTR imageData;
    ULONG b64Len = strlen(b64);
    CodesetsDecodeB64(CSA_B64SourceString, (Tag)b64, CSA_B64SourceLen,
                      (Tag)b64Len, CSA_B64DestPtr, (Tag)&imageData, TAG_DONE);
    if (imageData == NULL) {
        displayError(STRING_ERROR_INVALID_BASE64);
        finishActiveRequest(FALSE);
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

    FILE *file = fopen(fullPath, "wb");
    fwrite(imageData, 1, data_len, file);
    fclose(file);
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
        if (hostIsOpenAICompatible(nameSettings.host,
                                   nameSettings.apiEndpointUrl)) {
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
            if (hostIsOpenAICompatible(nameSettings.host,
                                       nameSettings.apiEndpointUrl)) {
                titleModel = DEFAULT_OPENAI_CHAT_MODEL;
            } else if (nameSettings.host != NULL &&
                       strcmp(nameSettings.host,
                              "generativelanguage.googleapis.com") == 0) {
                titleModel = DEFAULT_GEMINI_CHAT_MODEL;
            } else if (nameSettings.host != NULL &&
                       strcmp(nameSettings.host, "api.x.ai") == 0) {
                titleModel = DEFAULT_GROK_CHAT_MODEL;
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
        FALSE, nameSettings.apiEndpoint, nameSettings.apiEndpointUrl,
        nameSettings.authorizationType, nameSettings.customHeaders);

    struct GeneratedImage *generatedImage =
        AllocVec(sizeof(struct GeneratedImage), MEMF_ANY);
    if (responses == NULL) {
        displayError(STRING_ERROR_GENERATING_IMAGE_NAME);
    } else if (responses[0] != NULL) {
        if (json_object_object_get_ex(responses[0], "error", &error) &&
            !json_object_is_type(error, json_type_null)) {
            finishActiveRequest(FALSE);
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
        while ((r = responses[ri++]) != NULL) {
            UTF8 *part = getMessageContentFromJson(r, FALSE, FALSE,
                                                   nameSettings.apiEndpoint);
            if (part != NULL)
                combinedLen += strlen(part) + 1;
        }
        UTF8 *combined = AllocVec(combinedLen, MEMF_ANY | MEMF_CLEAR);
        if (combined == NULL) {
            combined = (UTF8 *)"";
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
        UTF8 *responseString = combined;
        generatedImage->name =
            AllocVec(strlen(responseString) + 1, MEMF_ANY | MEMF_CLEAR);
        strncpy(generatedImage->name, responseString, strlen(responseString));
        updateStatusBar(STRING_READY, 5);
        ri = 0;
        while ((r = responses[ri++]) != NULL) {
            json_object_put(r);
        }
        FreeVec(responses);
        if (combined != NULL && combined != (STRPTR) "" &&
            combined != (STRPTR)responseString) {
            /* unreachable, but keep for safety */
        }
        if (combined != NULL && combined != (STRPTR) "") {
            FreeVec(combined);
        }
    } else {
        generatedImage->name = AllocVec(11, MEMF_ANY | MEMF_CLEAR);
        strncpy(generatedImage->name, id, 10);
        updateStatusBar(STRING_READY, 5);
        if (responses != NULL) {
            FreeVec(responses);
        }
        displayError("Failed to generate image name. Using ID instead.");
    }
    freeConversation(imageNameConversation);

    generatedImage->filePath =
        AllocVec(strlen(fullPath) + 1, MEMF_ANY | MEMF_CLEAR);
    strncpy(generatedImage->filePath, fullPath, strlen(fullPath));
    generatedImage->prompt = AllocVec(strlen(text) + 1, MEMF_ANY | MEMF_CLEAR);
    strncpy(generatedImage->prompt, text, strlen(text));
    generatedImage->imageModel = imageModel;
    generatedImage->width = imageWidth;
    generatedImage->height = imageHeight;
    DoMethod(imageListObject, MUIM_NList_InsertSingle, generatedImage,
             MUIV_NList_Insert_Top);
    DoMethod(imageListObject, MUIM_NList_SetActive, 0, NULL);
    currentImage = generatedImage;
    ImageRowClickedFunc();

    saveImages();
    finishActiveRequest(TRUE);

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
            strncpy(fullPath, savePath, strlen(savePath));
            AddPart(fullPath, saveName, fullPathLength);
            copyFile(filePath, fullPath);
            FreeVec(fullPath);
        }
        FreeAslRequest(fileReq);
    }
}
MakeHook(SaveImageCopyButtonClickedHook, SaveImageCopyButtonClickedFunc);

HOOKPROTONHNONP(ConfigureForScreenFunc, void) {
    // Get the current screen to allocate pens
    struct Screen *currentScreen;
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
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s\0",
             greenPen, STRING_NEW_IMAGE);
    set(newImageButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]+ %s\0",
             greenPen, STRING_NEW_PHRASE);
    set(newSpeechButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s\0",
             redPen, STRING_DELETE_PHRASE);
    set(deleteSpeechButton, MUIA_Text_Contents, buttonLabelText);
    snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE, "\33c\33P[%ld]- %s\0",
             redPen, STRING_DELETE_IMAGE);
    set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
    setActionButtonsStopMode(requestInterfaceBusy);
    FreeVec(buttonLabelText);
    SetAttrs(mainWindowObject, MUIA_Window_ActiveObject, chatInputTextEditor,
             TAG_DONE);

    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
    set(mainWindowObject, MUIA_Window_Open, TRUE);
    addMenuActions();

    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    set(editImageButton, MUIA_Disabled, TRUE);

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
        strncat(out, "\033b", outSize - strlen(out) - 1);
        break;
    case STYLE_ITALIC:
        strncat(out, "\033i", outSize - strlen(out) - 1);
        break;
    case STYLE_UNDERLINE:
        strncat(out, "\033u", outSize - strlen(out) - 1);
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
    strncat(out, "\033n", outSize - strlen(out) - 1);
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
                         StyleType *foundStyle) {
    if (pos >= len)
        return 0;

    // Check for "__" (underline)
    if (pos + 1 < len && input[pos] == '_' && input[pos + 1] == '_') {
        *foundStyle = STYLE_UNDERLINE;
        return 2;
    }
    // Check for "**" (bold)
    if (pos + 1 < len && input[pos] == '*' && input[pos + 1] == '*') {
        *foundStyle = STYLE_BOLD;
        return 2;
    }
    // Check for single "*"
    if (input[pos] == '*') {
        *foundStyle = STYLE_ITALIC;
        return 1;
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
        UBYTE markerLen = parseMarker(input, i, inLen, &styleFound);
        if (markerLen > 0) {
            // It's either a 1-char or 2-char marker
            if (isTopStyle(&styleStack, styleFound)) {
                // It's a closing marker for the top style
                popStyle(&styleStack, styleFound);
                // Output style off
                outputStyleOff(out, outCap);

                // Now re-enable any style(s) that remain on the stack
                // because \033n turns off everything.
                for (UWORD s = 0; s <= styleStack.top; s++) {
                    outputStyleOn(out, outCap, styleStack.stack[s]);
                }
            } else {
                // It's an opening marker for a new style
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

                    strncat(out, tempBuf, availableSpace);
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

static Object *createPromptInputEditor(struct Hook *submitHook, ULONG objectId,
                                       ULONG weight) {
    // clang-format off
    if (amigaGPTTextEditorClass != NULL) {
        return NewObject(
            MUIC_AmigaGPTTextEditor, NULL,
            MUIA_Background, MUII_BACKGROUND,
            MUIA_CycleChain, TRUE,
            objectId != 0 ? MUIA_ObjectID : TAG_IGNORE, objectId,
            weight != 0 ? MUIA_Weight : TAG_IGNORE, weight,
            MUIA_AmigaGPTTextEditor_SubmitHook, submitHook,
            isAROS ? TAG_DONE : TAG_SKIP, NULL,
            MUIA_TextEditor_FixedFont, configGetFixedWidthFonts(),
            MUIA_TextEditor_ReadOnly, FALSE,
            MUIA_TextEditor_TabSize, 4,
            MUIA_TextEditor_Rows, 3,
            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
            TAG_DONE);
    }
    return MUI_NewObject(
        isAROS ? MUIC_BetterString : MUIC_TextEditor,
        MUIA_Background, MUII_BACKGROUND,
        MUIA_CycleChain, TRUE,
        objectId != 0 ? MUIA_ObjectID : TAG_IGNORE, objectId,
        weight != 0 ? MUIA_Weight : TAG_IGNORE, weight,
        isAROS ? TAG_DONE : TAG_SKIP, NULL,
        MUIA_TextEditor_FixedFont, configGetFixedWidthFonts(),
        MUIA_TextEditor_ReadOnly, FALSE,
        MUIA_TextEditor_TabSize, 4,
        MUIA_TextEditor_Rows, 3,
        MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_EMail,
        TAG_DONE);
    // clang-format on
}

static Object *promptEditorForPage(LONG page) {
    if (page == 0)
        return chatInputTextEditor;
    if (page == 1)
        return imageInputTextEditor;
    return speechInputTextEditor;
}

HOOKPROTONHNONP(ModePageChangedFunc, void) {
    LONG page = 0;
    Object *editor;

    get(modeRegisterGroup, MUIA_Group_ActivePage, &page);
    editor = promptEditorForPage(page);
    if (editor != NULL && mainWindowObject != NULL) {
        set(mainWindowObject, MUIA_Window_ActiveObject, editor);
        DoMethod(editor, MUIM_GoActive);
    }
    updateEditMenuItemsEnabled();
}
MakeHook(ModePageChangedHook, ModePageChangedFunc);

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow() {
    if (!pendingChatFilesInitialized) {
        NewList((struct List *)&pendingChatFiles);
        pendingChatFilesInitialized = TRUE;
    }
    if (!pendingImageFilesInitialized) {
        NewList((struct List *)&pendingImageFiles);
        pendingImageFilesInitialized = TRUE;
    }
    if (!pendingSpeechFilesInitialized) {
        NewList((struct List *)&pendingSpeechFiles);
        pendingSpeechFilesInitialized = TRUE;
    }
    BOOL useCustomTextEditor = createAmigaGPTTextEditor() == RETURN_OK;
    if (!useCustomTextEditor) {
        displayError("Could not create custom class.");
    }
    if (createSpeechWaveformClass() != RETURN_OK)
        displayError("Could not create custom class.");

    if (mainWindowObject != NULL) {
        MUI_DisposeObject(mainWindowObject);
    }

    chatInputTextEditor = createPromptInputEditor(
        &SendMessageButtonClickedHook, OBJECT_ID_CHAT_INPUT_TEXT_EDITOR, 0);
    imageInputTextEditor = createPromptInputEditor(
        &CreateImageButtonClickedHook, OBJECT_ID_IMAGE_INPUT_TEXT_EDITOR, 80);
    speechInputTextEditor = createPromptInputEditor(
        &GenerateSpeechButtonClickedHook, OBJECT_ID_SPEECH_INPUT_TEXT_EDITOR,
        80);

    createMenu();

    if (chatOutputTextEditorContents == NULL) {
        chatOutputTextEditorContents = AllocVec(
            CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH, MEMF_ANY | MEMF_CLEAR);
        chatOutputTextEditorContentsCapacity =
            CHAT_OUTPUT_TEXT_EDITOR_CONTENTS_LENGTH;
    }

    pages[0] = STRING_MENU_CHAT;
    pages[1] = STRING_MENU_IMAGE;
    pages[2] = STRING_MENU_SPEECH;

    if (speechWaveformClass != NULL) {
        speechWaveform = NewObject(
            MUIC_SpeechWaveform, NULL,
            MUIA_Frame, MUIV_Frame_Group,
            MUIA_Background, MUII_SHADOW,
            MUIA_FillArea, FALSE,
            MUIA_Weight, 200,
            TAG_DONE);
    } else {
        speechWaveform = RectangleObject,
            MUIA_Frame, MUIV_Frame_Group,
            MUIA_Weight, 200,
        End;
    }
    if (speechWaveform == NULL)
        speechWaveform = HVSpace;

    // clang-format off
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
                                Child, chatOutputListView = NListviewObject,
                                MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_None,
                                MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                                MUIA_NListview_NList, chatOutputTextEditor = NFloattextObject,
                                    MUIA_Font, configGetFixedWidthFonts() ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
                                    MUIA_Frame, MUIV_Frame_Text,
                                    MUIA_ContextMenu, NULL,
                                    MUIA_NFloattext_Text, chatOutputTextEditorContents,
                                    End,
                                End,
                            End,
                            Child, HGroup,
                                Child, attachFilesButton = MUI_MakeObject(MUIO_Button, STRING_ATTACH_FILES,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, clearAttachmentsButton = MUI_MakeObject(MUIO_Button, STRING_CLEAR_ATTACHMENTS,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, attachmentSummaryText = TextObject,
                                    MUIA_Text_Contents, STRING_NO_FILES_ATTACHED,
                                    MUIA_Text_SetMin, FALSE,
                                End,
                                Child, saveResponseFilesButton = MUI_MakeObject(MUIO_Button, STRING_SAVE_RESPONSE_FILES,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
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
                                // Edit image button: reuse this image as a reference for a new prompt
                                Child, editImageButton = MUI_MakeObject(MUIO_Button, STRING_EDIT_IMAGE,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                            End,
                            // Reference images sent along with the prompt
                            Child, HGroup,
                                Child, attachReferenceImagesButton = MUI_MakeObject(MUIO_Button, STRING_ATTACH_REFERENCE_IMAGES,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, clearReferenceImagesButton = MUI_MakeObject(MUIO_Button, STRING_CLEAR_REFERENCE_IMAGES,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, referenceImageSummaryText = TextObject,
                                    MUIA_Text_Contents, STRING_NO_REFERENCE_IMAGES,
                                    MUIA_Text_SetMin, FALSE,
                                End,
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
                    Child, HGroup,
                        GroupFrame,
                        Child, VGroup, MUIA_Weight, 30,
                            Child, newSpeechButton = MUI_MakeObject(MUIO_Button, STRING_NEW_PHRASE,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            Child, deleteSpeechButton = MUI_MakeObject(MUIO_Button, STRING_DELETE_PHRASE,
                                MUIA_Background, MUII_FILL,
                                MUIA_CycleChain, TRUE,
                                MUIA_InputMode, MUIV_InputMode_RelVerify,
                            TAG_DONE),
                            Child, NListviewObject,
                                MUIA_CycleChain, 1,
                                MUIA_NListview_NList, speechListObject = NListObject,
                                    MUIA_ContextMenu, NULL,
                                    MUIA_NList_DefaultObjectOnClick, TRUE,
                                    MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
                                    MUIA_NList_ConstructHook2, &ConstructSpeechLI_TextHook,
                                    MUIA_NList_DestructHook2, &DestructSpeechLI_TextHook,
                                    MUIA_NList_DisplayHook2, &DisplaySpeechLI_TextHook,
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
                            Child, speechProfileInfo = TextObject,
                                MUIA_Frame, MUIV_Frame_Group,
                                MUIA_Text_Contents, STRING_NO_SPEECH_SELECTED,
                                MUIA_Text_SetMin, FALSE,
                                MUIA_Weight, 20,
                            End,
                            Child, speechWaveform,
                            Child, HGroup,
                                Child, regenerateSpeechButton = MUI_MakeObject(MUIO_Button, STRING_REGENERATE,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, saveSpeechCopyButton = MUI_MakeObject(MUIO_Button, STRING_SAVE_SPEECH_COPY,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                            End,
                            Child, HGroup,
                                Child, attachSpeechFilesButton = MUI_MakeObject(MUIO_Button, STRING_ATTACH_FILES,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, clearSpeechFilesButton = MUI_MakeObject(MUIO_Button, STRING_CLEAR_ATTACHMENTS,
                                    MUIA_CycleChain, TRUE,
                                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                                TAG_DONE),
                                Child, speechAttachmentSummaryText = TextObject,
                                    MUIA_Text_Contents, STRING_NO_FILES_ATTACHED,
                                    MUIA_Text_SetMin, FALSE,
                                End,
                            End,
                            Child, HGroup,
                                Child, speakFileContentsCheckbox = MUI_MakeObject(MUIO_Checkmark, NULL),
                                Child, LLabel(STRING_SPEAK_CONTENTS_OF_FILES),
                                Child, HSpace(0),
                            End,
                            Child, HGroup,
                                Child, speechInputTextEditor,
                                Child, VGroup,
                                    MUIA_Weight, 20,
                                    Child, generateSpeechButton = MUI_MakeObject(MUIO_Button, STRING_GENERATE_SPEECH,
                                        MUIA_CycleChain, TRUE,
                                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                                    TAG_DONE),
                                    Child, generateSpeechTextButton = MUI_MakeObject(MUIO_Button, STRING_GENERATE_TEXT_WITH_AI,
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
        // clang-format on
        displayError(STRING_ERROR_MAIN_WINDOW);
        return RETURN_ERROR;
    }

    get(mainWindowObject, MUIA_Window, &mainWindow);

    DoMethod(app, OM_ADDMEMBER, mainWindowObject);

    DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_Screen, MUIV_EveryTime, MUIV_Notify_Self, 2, MUIM_CallHook, &ConfigureForScreenHook);

    addMainWindowActions();
    
    // Open the main window immediately after creation
    DoMethod(app, MUIM_Application_Load, MUIV_Application_Load_ENVARC);
    set(mainWindowObject, MUIA_Window_Open, TRUE);
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
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW_PHRASE);
        set(newSpeechButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_PHRASE);
        set(deleteSpeechButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]+ %s\0", greenPen, STRING_NEW_IMAGE);
        set(newImageButton, MUIA_Text_Contents, buttonLabelText);
        snprintf(buttonLabelText, BUTTON_LABEL_BUFFER_SIZE,
                 "\33c\33P[%ld]- %s\0", redPen, STRING_DELETE_IMAGE);
        set(deleteImageButton, MUIA_Text_Contents, buttonLabelText);
        setActionButtonsStopMode(FALSE);
        FreeVec(buttonLabelText);

        // Force a layout refresh to ensure buttons display correctly
        DoMethod(mainWindowObject, MUIM_Group_InitChange);
        DoMethod(mainWindowObject, MUIM_Group_ExitChange);
    }
    
    set(openImageButton, MUIA_Disabled, TRUE);
    set(saveImageCopyButton, MUIA_Disabled, TRUE);
    set(editImageButton, MUIA_Disabled, TRUE);
    updateAttachmentControls();
    updateReferenceImageControls();
    updateResponseFileControl();
    updateSpeechControls();
    
    updateStatusBar(STRING_READY, greenPen);

    loadConversations();
    loadImages();
    loadSpeechHistory();

    return RETURN_OK;
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
    DoMethod(attachFilesButton, MUIM_Notify, MUIA_Pressed, FALSE,
             attachFilesButton, 2, MUIM_CallHook,
             &AttachFilesButtonClickedHook);
    DoMethod(clearAttachmentsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             clearAttachmentsButton, 2, MUIM_CallHook,
             &ClearAttachmentsButtonClickedHook);
    DoMethod(saveResponseFilesButton, MUIM_Notify, MUIA_Pressed, FALSE,
             saveResponseFilesButton, 2, MUIM_CallHook,
             &SaveResponseFilesButtonClickedHook);
    DoMethod(createImageButton, MUIM_Notify, MUIA_Pressed, FALSE,
             createImageButton, 2, MUIM_CallHook,
             &CreateImageButtonClickedHook);
    DoMethod(openImageButton, MUIM_Notify, MUIA_Pressed, FALSE, openImageButton,
             2, MUIM_CallHook, &OpenImageButtonClickedHook);
    DoMethod(saveImageCopyButton, MUIM_Notify, MUIA_Pressed, FALSE,
             saveImageCopyButton, 2, MUIM_CallHook,
             &SaveImageCopyButtonClickedHook);
    DoMethod(editImageButton, MUIM_Notify, MUIA_Pressed, FALSE, editImageButton,
             2, MUIM_CallHook, &EditImageButtonClickedHook);
    DoMethod(attachReferenceImagesButton, MUIM_Notify, MUIA_Pressed, FALSE,
             attachReferenceImagesButton, 2, MUIM_CallHook,
             &AttachReferenceImagesButtonClickedHook);
    DoMethod(clearReferenceImagesButton, MUIM_Notify, MUIA_Pressed, FALSE,
             clearReferenceImagesButton, 2, MUIM_CallHook,
             &ClearReferenceImagesButtonClickedHook);
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
    DoMethod(newSpeechButton, MUIM_Notify, MUIA_Pressed, FALSE, newSpeechButton,
             2, MUIM_CallHook, &NewSpeechButtonClickedHook);
    DoMethod(deleteSpeechButton, MUIM_Notify, MUIA_Pressed, FALSE,
             MUIV_Notify_Application, 3, MUIM_CallHook,
             &DeleteSpeechButtonClickedHook, MUIV_TriggerValue);
    DoMethod(generateSpeechButton, MUIM_Notify, MUIA_Pressed, FALSE,
             generateSpeechButton, 2, MUIM_CallHook,
             &GenerateSpeechButtonClickedHook);
    DoMethod(regenerateSpeechButton, MUIM_Notify, MUIA_Pressed, FALSE,
             regenerateSpeechButton, 2, MUIM_CallHook,
             &RegenerateSpeechButtonClickedHook);
    if (speechWaveformClass != NULL && speechWaveform != NULL) {
        DoMethod(speechWaveform, MUIM_Notify, MUIA_SpeechWaveform_Seek,
                 MUIV_EveryTime, speechWaveform, 2, MUIM_CallHook,
                 &SpeechWaveformSeekHook);
        DoMethod(speechWaveform, MUIM_Notify, MUIA_SpeechWaveform_Command,
                 MUIV_EveryTime, speechWaveform, 2, MUIM_CallHook,
                 &SpeechWaveformCommandHook);
    }
    DoMethod(saveSpeechCopyButton, MUIM_Notify, MUIA_Pressed, FALSE,
             saveSpeechCopyButton, 2, MUIM_CallHook,
             &SaveSpeechCopyButtonClickedHook);
    DoMethod(generateSpeechTextButton, MUIM_Notify, MUIA_Pressed, FALSE,
             generateSpeechTextButton, 2, MUIM_CallHook,
             &GenerateSpeechTextButtonClickedHook);
    DoMethod(attachSpeechFilesButton, MUIM_Notify, MUIA_Pressed, FALSE,
             attachSpeechFilesButton, 2, MUIM_CallHook,
             &AttachSpeechFilesButtonClickedHook);
    DoMethod(clearSpeechFilesButton, MUIM_Notify, MUIA_Pressed, FALSE,
             clearSpeechFilesButton, 2, MUIM_CallHook,
             &ClearSpeechFilesButtonClickedHook);
    DoMethod(speechListObject, MUIM_Notify, MUIA_NList_EntryClick,
             MUIV_EveryTime, MUIV_Notify_Window, 3, MUIM_CallHook,
             &SpeechRowClickedHook, MUIV_TriggerValue);
    DoMethod(speakFileContentsCheckbox, MUIM_Notify, MUIA_Selected,
             MUIV_EveryTime, speakFileContentsCheckbox, 2, MUIM_CallHook,
             &SpeechTextChangedHook);
    DoMethod(modeRegisterGroup, MUIM_Notify, MUIA_Group_ActivePage,
             MUIV_EveryTime, modeRegisterGroup, 2, MUIM_CallHook,
             &ModePageChangedHook);
    if (isAROS) {
        DoMethod(speechInputTextEditor, MUIM_Notify, MUIA_String_Contents,
                 MUIV_EveryTime, speechInputTextEditor, 2, MUIM_CallHook,
                 &SpeechTextChangedHook);
    } else {
        DoMethod(speechInputTextEditor, MUIM_Notify, MUIA_TextEditor_HasChanged,
                 TRUE, speechInputTextEditor, 2, MUIM_CallHook,
                 &SpeechTextChangedHook);
    }
    DoMethod(mainWindowObject, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             MUIV_Notify_Application, 2, MUIM_Application_ReturnID,
             MUIV_Application_ReturnID_Quit);
}

/**
 * @brief Sends a chat message to the OpenAI API and displays the response
 *and speaks it if speech is enabled
 * @details This function sends a chat message to the OpenAI API and
 *displays the response in the chat window. It also speaks the response if
 *speech is enabled.
 **/
static void sendChatMessage() {
    BOOL isNewConversation = FALSE;
    struct json_object **responses;
    if (currentConversation == NULL) {
        isNewConversation = TRUE;
        currentConversation = newConversation();
        chatOutputTextEditorContents[0] = '\0';
        set(chatOutputTextEditor, MUIA_NFloattext_Text,
            chatOutputTextEditorContents);
        set(conversationListObject, MUIA_NList_Active, MUIV_NList_Active_Off);
    }
    updateStatusBar(STRING_SENDING_MESSAGE, yellowPen);
    showLoadingBar();
    UTF8 *receivedMessage = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
    struct MinList receivedFiles;
    NewList((struct List *)&receivedFiles);
    STRPTR text;
    if (isAROS) {
        get(chatInputTextEditor, MUIA_String_Contents, &text);
    } else {
        text = DoMethod(chatInputTextEditor, MUIM_TextEditor_ExportText);
    }
    BOOL freeExportedText = !isAROS && text != NULL;
    if (text == NULL)
        text = (STRPTR)"";
    while (strlen(text) > 0 && text[strlen(text) - 1] == '\n')
        text[strlen(text) - 1] = '\0';
    if (strlen(text) == 0 && chatFileCount(&pendingChatFiles) == 0) {
        displayError(STRING_ERROR_MESSAGE_OR_ATTACHMENT_REQUIRED);
        hideLoadingBar();
        FreeVec(receivedMessage);
        if (isNewConversation) {
            freeConversation(currentConversation);
            currentConversation = NULL;
        }
        if (freeExportedText)
            FreeVec(text);
        return;
    }

    beginCancellableRequest();
    setRequestInterfaceBusy(TRUE);

    UBYTE userStyleString[] = "\033r\033b\0333";
    UBYTE userAlignment = 'l';
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
    strncat(chatOutputTextEditorContents, userStyleString,
            strlen(userStyleString));
    size_t currentLength = strlen(chatOutputTextEditorContents);
    for (ULONG i = 0; i < strlen(text); i++) {
        if (currentLength >= chatOutputTextEditorContentsCapacity - 10)
            break;

        chatOutputTextEditorContents[currentLength++] = text[i];
        chatOutputTextEditorContents[currentLength] = '\0';

        // If it's a newline, add the styling codes after it
        if (text[i] == '\n') {
            strncat(chatOutputTextEditorContents, userStyleString,
                    strlen(userStyleString));
            currentLength = strlen(chatOutputTextEditorContents);
        }
    }

    set(chatOutputTextEditor, MUIA_NFloattext_Text,
        chatOutputTextEditorContents);
    set(chatOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);

    UTF8 *textUTF8 = CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,

                                        CSA_Source, (Tag)text,
                                        CSA_MapForeignChars, TRUE, TAG_DONE);

    struct ConversationNode *userMessage = addTextToConversation(
        currentConversation, textUTF8 != NULL ? textUTF8 : (UTF8 *)"",
        "user");
    if (textUTF8 != NULL)
        CodesetsFreeA(textUTF8, NULL);
    if (userMessage == NULL ||
        !copyChatFiles(&userMessage->files, &pendingChatFiles)) {
        displayError(STRING_ERROR_MEMORY_CONVERSATION_NODE);
        finishActiveRequest(FALSE);
        if (userMessage != NULL) {
            RemTail((struct List *)currentConversation->messages);
            freeConversationNode(userMessage);
        }
        if (isNewConversation) {
            freeConversation(currentConversation);
            currentConversation = NULL;
            chatOutputTextEditorContents[0] = '\0';
            set(chatOutputTextEditor, MUIA_NFloattext_Text,
                chatOutputTextEditorContents);
        }
        FreeVec(receivedMessage);
        if (freeExportedText)
            FreeVec(text);
        return;
    }

    if (isAROS) {
        set(chatInputTextEditor, MUIA_String_Contents, "");
    } else {
        DoMethod(chatInputTextEditor, MUIM_TextEditor_ClearText);
    }
    DoMethod(chatInputTextEditor, MUIM_GoActive);

    BOOL dataStreamFinished = FALSE;
    ULONG speechIndex = 0;
    UWORD chunkCounter = 0;
    ULONG streamedCharsSinceFlush = 0;
    ULONG flushesSinceScroll = 0;
    const clock_t uiFlushIntervalTicks = CLOCKS_PER_SEC / 2;
    clock_t nextUiFlushTick = clock() + uiFlushIntervalTicks;
    struct ChatRequestSettings chatSettings;
    configGetActiveChatRequestSettings(&chatSettings);

    /* Responses API streaming repeats substantial response metadata in SSE
     * events. Image inputs can therefore produce a response many times larger
     * than the final JSON object, which is unreliable on classic Amiga TCP
     * stacks even though the upload itself completed. Use the compact response
     * form for messages with local files on cloud SSL hosts. xAI attachments
     * become an agentic search and need stream events. Custom/local servers
     * such as LM Studio keep the profile's streaming setting. */
    BOOL xaiHost = chatSettings.host != NULL &&
                   strcmp(chatSettings.host, "api.x.ai") == 0;
    BOOL cloudAttachmentHost =
        (chatSettings.host != NULL &&
         (hostIsOpenAICompatible(chatSettings.host,
                                 chatSettings.apiEndpointUrl) ||
          strcmp(chatSettings.host, "generativelanguage.googleapis.com") ==
              0 ||
          strcmp(chatSettings.host, "api.anthropic.com") == 0));
    BOOL requestStream = chatSettings.stream;
    if (chatFileCount(&userMessage->files) > 0) {
        if (xaiHost)
            requestStream = TRUE;
        else if (cloudAttachmentHost)
            requestStream = FALSE;
    }

    setConversationSystem(currentConversation, chatSettings.chatSystem);

    strncat(chatOutputTextEditorContents, "\n", 1);

    do {
        responses = postChatMessageToOpenAI(
            currentConversation, chatSettings.host, chatSettings.port,
            chatSettings.useSSL, chatSettings.model, chatSettings.apiKey,
            requestStream, chatSettings.useProxy, chatSettings.proxyHost,
            chatSettings.proxyPort, chatSettings.proxyUsesSSL,
            chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
            chatSettings.proxyPassword, chatSettings.webSearchEnabled,
            chatSettings.shellToolEnabled, chatSettings.codeInterpreterEnabled,
            chatSettings.apiEndpoint, chatSettings.apiEndpointUrl,
            chatSettings.authorizationType, chatSettings.customHeaders);
        if (responses == NULL) {
            if (!isRequestCancelled())
                displayError(STRING_ERROR_CONNECTING_OPENAI);
            finishActiveRequest(FALSE);
            struct ConversationNode *failedMessage =
                (struct ConversationNode *)RemTail(
                    (struct List *)currentConversation->messages);
            freeConversationNode(failedMessage);
            if (isNewConversation) {
                freeConversation(currentConversation);
                currentConversation = NULL;
                chatOutputTextEditorContents[0] = '\0';
                set(chatOutputTextEditor, MUIA_NFloattext_Text,
                    chatOutputTextEditorContents);
            }
            if (isAROS) {
                set(chatInputTextEditor, MUIA_String_Contents, text);
            } else {
                set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
            }
            freeChatFiles(&receivedFiles);
            FreeVec(receivedMessage);
            if (freeExportedText) {
                FreeVec(text);
            }
            return;
        }

        UWORD responseIndex = 0;

        struct json_object *response;
        while (response = responses[responseIndex++]) {
            collectResponseFiles(response, &receivedFiles);
            UTF8 *messageString = getApiErrorMessageFromJson(response);
            if (messageString != NULL) {
                STRPTR formattedMessageSystemEncoded = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)messageString, CSA_MapForeignChars, TRUE, TAG_DONE);
                if (formattedMessageSystemEncoded != NULL) {
                    SetIoErr(0);
                    displayError(formattedMessageSystemEncoded);
                    CodesetsFreeA(formattedMessageSystemEncoded, NULL);
                } else {
                    SetIoErr(0);
                    displayError(messageString);
                }
                if (isAROS) {
                    set(chatInputTextEditor, MUIA_String_Contents, text);
                } else {
                    set(chatInputTextEditor, MUIA_TextEditor_Contents, text);
                }
                struct Node *lastMessage =
                    RemTail((struct List *)currentConversation->messages);
                freeConversationNode((struct ConversationNode *)lastMessage);
                if (isNewConversation &&
                    currentConversation->messages->mlh_Head->mln_Succ ==
                        NULL) {
                    freeConversation(currentConversation);
                    currentConversation = NULL;
                    chatOutputTextEditorContents[0] = '\0';
                    set(chatOutputTextEditor, MUIA_NFloattext_Text,
                        chatOutputTextEditorContents);
                } else {
                    displayConversation(currentConversation);
                }
                json_object_put(response);

                finishActiveRequest(FALSE);

                freeChatFiles(&receivedFiles);
                FreeVec(receivedMessage);
                if (freeExportedText) {
                    FreeVec(text);
                }
                return;
            }

            UTF8 *contentString = getMessageContentFromJson(
                response, requestStream, FALSE, chatSettings.apiEndpoint);
            if (!requestStream) {
                strncpy(receivedMessage, contentString,
                        READ_BUFFER_LENGTH - strlen(receivedMessage) -
                            strlen(contentString) - 1);
                json_object_put(response);
                dataStreamFinished = TRUE;
                continue;
            } else {
                if (contentString != NULL) {
                    // Text for printing
                    if (strlen(contentString) > 0) {
                        STRPTR formattedMessageSystemEncoded =
                            CodesetsUTF8ToStr(
                                CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                                (Tag)contentString, CSA_MapForeignChars, TRUE,
                                TAG_DONE);
                        strncat(receivedMessage, contentString,
                                READ_BUFFER_LENGTH - strlen(receivedMessage) -
                                    strlen(contentString) - 1);
                        CONST_STRPTR displayStr = formattedMessageSystemEncoded;
                        STRPTR latin1StreamFb = NULL;
                        if (displayStr == NULL) {
                            latin1StreamFb = utf8ToLatin1(contentString);
                            displayStr = latin1StreamFb
                                             ? latin1StreamFb
                                             : (CONST_STRPTR)contentString;
                        }
                        ULONG displayLen = strlen(displayStr);
                        strncat(chatOutputTextEditorContents, displayStr,
                                chatOutputTextEditorContentsCapacity -
                                    strlen(chatOutputTextEditorContents) - 1);
                        if (formattedMessageSystemEncoded != NULL)
                            CodesetsFreeA(formattedMessageSystemEncoded, NULL);
                        if (latin1StreamFb != NULL)
                            FreeVec(latin1StreamFb);
                        chunkCounter++;
                        streamedCharsSinceFlush += displayLen;

                        /* Reduce flicker/jitter by coalescing frequent tiny
                         * updates into timed UI flushes.
                         */
                        clock_t nowTick = clock();
                        if (streamedCharsSinceFlush >= 256 ||
                            nowTick >= nextUiFlushTick) {
                            set(chatOutputTextEditor, MUIA_NFloattext_Text,
                                chatOutputTextEditorContents);
                            if (++flushesSinceScroll % 3 == 0) {
                                set(chatOutputListView, MUIA_NList_First,
                                    MUIV_NList_First_Bottom);
                            }
                            streamedCharsSinceFlush = 0;
                            nextUiFlushTick = nowTick + uiFlushIntervalTicks;
                        }
                    }

                    /* End-of-stream detection:
                     * - Responses API: "type"=="response.completed"
                     * - chat.completions: choices[0].finish_reason is set (and
                     *   we may also synthesize response.completed from [DONE])
                     * - Gemini streamGenerateContent:
                     * candidates[0].finishReason
                     */
                    STRPTR type = json_object_get_string(
                        json_object_object_get(response, "type"));
                    if (type != NULL &&
                        strcmp(type, "response.completed") == 0) {
                        dataStreamFinished = TRUE;
                    } else if (json_object_object_get(response, "choices") !=
                               NULL) {
                        /* OpenAI-compatible chat.completions streaming chunk */
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
                        /* Gemini streamGenerateContent */
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
    } while (!dataStreamFinished);

    /* Ensure the final streamed text is shown immediately even when the stream
     * ended before the next coalesced refresh threshold. */
    if (requestStream && chunkCounter > 0) {
        set(chatOutputTextEditor, MUIA_NFloattext_Text,
            chatOutputTextEditorContents);
        set(chatOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);
    }

    /* Resolve every unanswered function_call before the next user message. */
    while (requestStream && hasPendingToolCall()) {
        UTF8 *responseId = getPendingResponseId();
        UWORD pendingCount = getPendingToolCallCount();
        STRPTR *callIds = NULL;
        STRPTR *outputs = NULL;
        UWORD i;
        BOOL userDenied = FALSE;
        BOOL denyRest = FALSE;
        BOOL allocFailed = FALSE;
        struct json_object *toolResponse;
        struct json_object *error;

        if (responseId == NULL || strlen(responseId) == 0 || pendingCount == 0) {
            clearPendingToolCall();
            break;
        }

        callIds = AllocVec(pendingCount * sizeof(STRPTR), MEMF_ANY | MEMF_CLEAR);
        outputs = AllocVec(pendingCount * sizeof(STRPTR), MEMF_ANY | MEMF_CLEAR);
        if (callIds == NULL || outputs == NULL) {
            if (callIds != NULL)
                FreeVec(callIds);
            if (outputs != NULL)
                FreeVec(outputs);
            clearPendingToolCall();
            break;
        }

        for (i = 0; i < pendingCount; i++) {
            STRPTR callId = NULL;
            STRPTR command = NULL;
            getPendingToolCallAt(i, &callId, NULL, &command);
            callIds[i] = callId;

            if (denyRest) {
                ULONG len = strlen(TOOL_CALL_OUTPUT_DENIED);
                outputs[i] = AllocVec(len + 1, MEMF_ANY | MEMF_CLEAR);
                if (outputs[i] != NULL)
                    strncpy(outputs[i], TOOL_CALL_OUTPUT_DENIED, len);
            } else if (chatSettings.shellToolEnabled && command != NULL &&
                       strlen(command) > 0) {
                UBYTE confirmMsg[4096];
                LONG result;
                snprintf(confirmMsg, sizeof(confirmMsg),
                         STRING_SHELL_TOOL_CONFIRMATION_BODY, command);
                result = MUI_Request(app, mainWindowObject,
#ifdef __MORPHOS__
                                     NULL,
#else
                                     MUIV_Requester_Image_Warning,
#endif
                                     STRING_SHELL_TOOL_CONFIRMATION_TITLE,
                                     STRING_SHELL_TOOL_CONFIRMATION_BUTTONS,
                                     confirmMsg, TAG_DONE);
                if (result != 1) {
                    ULONG len = strlen(TOOL_CALL_OUTPUT_DENIED);
                    outputs[i] = AllocVec(len + 1, MEMF_ANY | MEMF_CLEAR);
                    if (outputs[i] != NULL)
                        strncpy(outputs[i], TOOL_CALL_OUTPUT_DENIED, len);
                    userDenied = TRUE;
                    denyRest = TRUE;
                    strncat(chatOutputTextEditorContents,
                            STRING_SHELL_TOOL_DENIED_BANNER,
                            chatOutputTextEditorContentsCapacity -
                                strlen(chatOutputTextEditorContents) - 1);
                    strncat(receivedMessage, STRING_SHELL_TOOL_DENIED_BANNER,
                            READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
                    set(chatOutputTextEditor, MUIA_NFloattext_Text,
                        chatOutputTextEditorContents);
                    set(chatOutputListView, MUIA_NList_First,
                        MUIV_NList_First_Bottom);
                } else {
                    LONG exitCode = 0;
                    STRPTR output;
                    UBYTE statusMsg[256];
                    UBYTE cmdDisplay[512];
                    UBYTE outputDisplay[4096];
                    UBYTE toolOutput[8192];
                    ULONG len;

                    snprintf(statusMsg, sizeof(statusMsg),
                             STRING_EXECUTING_COMMAND);
                    updateStatusBar(statusMsg, yellowPen);
                    snprintf(cmdDisplay, sizeof(cmdDisplay),
                             STRING_SHELL_TOOL_EXECUTING_BANNER_FORMAT, command);
                    strncat(chatOutputTextEditorContents, cmdDisplay,
                            chatOutputTextEditorContentsCapacity -
                                strlen(chatOutputTextEditorContents) - 1);
                    strncat(receivedMessage, cmdDisplay,
                            READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
                    set(chatOutputTextEditor, MUIA_NFloattext_Text,
                        chatOutputTextEditorContents);
                    set(chatOutputListView, MUIA_NList_First,
                        MUIV_NList_First_Bottom);

                    output = executeShellCommand(command, &exitCode);
                    snprintf(outputDisplay, sizeof(outputDisplay),
                             STRING_SHELL_TOOL_OUTPUT_DISPLAY_FORMAT, exitCode,
                             output != NULL
                                 ? output
                                 : (STRPTR)STRING_SHELL_TOOL_NO_OUTPUT);
                    strncat(chatOutputTextEditorContents, outputDisplay,
                            chatOutputTextEditorContentsCapacity -
                                strlen(chatOutputTextEditorContents) - 1);
                    strncat(receivedMessage, outputDisplay,
                            READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);
                    set(chatOutputTextEditor, MUIA_NFloattext_Text,
                        chatOutputTextEditorContents);
                    set(chatOutputListView, MUIA_NList_First,
                        MUIV_NList_First_Bottom);

                    snprintf(toolOutput, sizeof(toolOutput),
                             STRING_SHELL_TOOL_TOOL_OUTPUT_FORMAT, exitCode,
                             output != NULL
                                 ? output
                                 : (STRPTR)STRING_SHELL_TOOL_NO_OUTPUT);
                    len = strlen(toolOutput);
                    outputs[i] = AllocVec(len + 1, MEMF_ANY | MEMF_CLEAR);
                    if (outputs[i] != NULL)
                        strncpy(outputs[i], toolOutput, len);
                    if (output != NULL)
                        FreeVec(output);
                }
            } else {
                ULONG len = strlen(TOOL_CALL_OUTPUT_UNAVAILABLE);
                outputs[i] = AllocVec(len + 1, MEMF_ANY | MEMF_CLEAR);
                if (outputs[i] != NULL)
                    strncpy(outputs[i], TOOL_CALL_OUTPUT_UNAVAILABLE, len);
            }
            if (outputs[i] == NULL)
                allocFailed = TRUE;
        }

        if (allocFailed) {
            for (i = 0; i < pendingCount; i++) {
                if (outputs[i] != NULL)
                    FreeVec(outputs[i]);
            }
            FreeVec(callIds);
            FreeVec(outputs);
            clearPendingToolCall();
            break;
        }

        toolResponse = postToolResultsToOpenAI(
            responseId, (CONST_STRPTR *)callIds, (CONST_STRPTR *)outputs,
            pendingCount, chatSettings.model, chatSettings.host,
            chatSettings.port, chatSettings.useSSL, chatSettings.apiKey,
            configGetProxyEnabled(), chatSettings.proxyHost,
            chatSettings.proxyPort, chatSettings.proxyUsesSSL,
            chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
            chatSettings.proxyPassword, chatSettings.shellToolEnabled,
            chatSettings.codeInterpreterEnabled, chatSettings.apiEndpointUrl,
            chatSettings.authorizationType, chatSettings.customHeaders);

        for (i = 0; i < pendingCount; i++) {
            if (outputs[i] != NULL)
                FreeVec(outputs[i]);
        }
        FreeVec(callIds);
        FreeVec(outputs);

        if (toolResponse != NULL) {
            collectResponseFiles(toolResponse, &receivedFiles);
            if (json_object_object_get_ex(toolResponse, "error", &error) &&
                !json_object_is_type(error, json_type_null)) {
                clearPendingToolCall();
                struct json_object *message =
                    json_object_object_get(error, "message");
                if (message != NULL) {
                    SetIoErr(0);
                    displayError(json_object_get_string(message));
                }
                json_object_put(toolResponse);
                break;
            }

            conversationSyncLastResponseIdFromPayload(currentConversation,
                                                      toolResponse);

            if (hasPendingToolCall()) {
                json_object_put(toolResponse);
                continue;
            }

            if (!userDenied) {
                UTF8 *toolContentString = getMessageContentFromJson(
                    toolResponse, FALSE, FALSE, API_CHAT_ENDPOINT_RESPONSES);
                if (toolContentString != NULL && strlen(toolContentString) > 0) {
                    strncat(receivedMessage, toolContentString,
                            READ_BUFFER_LENGTH - strlen(receivedMessage) - 1);

                    STRPTR formattedToolResponse = CodesetsUTF8ToStr(
                        CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                        (Tag)toolContentString, CSA_MapForeignChars, TRUE,
                        TAG_DONE);
                    if (formattedToolResponse != NULL) {
                        strncat(chatOutputTextEditorContents,
                                formattedToolResponse,
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
                }
            }
            json_object_put(toolResponse);
        } else {
            clearPendingToolCall();
            break;
        }
    } /* end of while (tool calls) */

    hideLoadingBar();

    if (responses != NULL) {
        struct ConversationNode *assistantMessage = addTextToConversation(
            currentConversation, receivedMessage, "assistant");
        if (assistantMessage != NULL)
            moveChatFiles(&assistantMessage->files, &receivedFiles);
        else
            freeChatFiles(&receivedFiles);
        freeChatFiles(&pendingChatFiles);
        updateAttachmentControls();
        updateResponseFileControl();
        displayConversation(currentConversation);

        if (configGetSpeechEnabled()) {
            SpeechSystem speechSys = configGetSpeechSystem();
            if (speechSys == SPEECH_SYSTEM_OPENAI ||
                speechSys == SPEECH_SYSTEM_XAI) {
                speakText(receivedMessage, NULL, NULL);
            } else {
                STRPTR receivedMessageSystemEncoded = CodesetsUTF8ToStr(
                    CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                    (Tag)receivedMessage, CSA_MapForeignChars, TRUE, TAG_DONE);
                if (receivedMessageSystemEncoded != NULL) {
                    speakText(receivedMessageSystemEncoded, NULL, NULL);
                    CodesetsFreeA(receivedMessageSystemEncoded, NULL);
                }
            }
        }
        FreeVec(responses);
        FreeVec(receivedMessage);
        if (freeExportedText) {
            FreeVec(text);
        }
        if (isNewConversation) {
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
                chatSettings.proxyPassword, FALSE, FALSE, FALSE,
                chatSettings.apiEndpoint, chatSettings.apiEndpointUrl,
                chatSettings.authorizationType, chatSettings.customHeaders);
            struct Node *titleRequestNode =
                RemTail((struct List *)currentConversation->messages);
            freeConversationNode(
                (struct ConversationNode *)titleRequestNode);
            hideLoadingBar();
            if (responses == NULL) {
                if (!isRequestCancelled())
                    displayError(STRING_ERROR_CONNECTING_OPENAI);
                finishActiveRequest(FALSE);
                return;
            }
            if (responses[0] != NULL) {
                /* Concatenate all text parts across response objects */
                ULONG combinedLen = 1;
                UWORD ri = 0;
                struct json_object *r = NULL;
                while ((r = responses[ri++]) != NULL) {
                    UTF8 *part = getMessageContentFromJson(
                        r, FALSE, FALSE, chatSettings.apiEndpoint);
                    if (part != NULL)
                        combinedLen += strlen(part) + 1;
                }
                STRPTR combined = AllocVec(combinedLen, MEMF_ANY | MEMF_CLEAR);
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
                UTF8 *responseString = combined;
                if (currentConversation->name == NULL) {
                    currentConversation->name = allocNewConversationTitle(
                        currentConversation, responseString);
                }
                DoMethod(conversationListObject, MUIM_NList_InsertSingle,
                         currentConversation, MUIV_NList_Insert_Top);
                DoMethod(conversationListObject, MUIM_NList_SetActive,
                         MUIV_NList_Active_Top, NULL);
                if (combined != NULL && combined != (STRPTR) "") {
                    FreeVec(combined);
                }
            }
            UWORD ri = 0;
            struct json_object *r = NULL;
            while ((r = responses[ri++]) != NULL) {
                json_object_put(r);
            }
            FreeVec(responses);
        }
    }

    saveConversations();
    finishActiveRequest(TRUE);
}

static void appendMessageFileSummary(struct ConversationNode *message) {
    if (message == NULL || message->files.mlh_Head->mln_Succ == NULL)
        return;
    CONST_STRPTR prefix = strcmp(message->role, "assistant") == 0
                              ? STRING_RECEIVED_FILES_PREFIX
                              : STRING_ATTACHED_FILES_PREFIX;
    strncat(chatOutputTextEditorContents, "\n[",
            chatOutputTextEditorContentsCapacity -
                strlen(chatOutputTextEditorContents) - 1);
    strncat(chatOutputTextEditorContents, prefix,
            chatOutputTextEditorContentsCapacity -
                strlen(chatOutputTextEditorContents) - 1);
    struct ChatFile *file;
    ULONG index = 0;
    for (file = (struct ChatFile *)message->files.mlh_Head;
         file->node.mln_Succ != NULL;
         file = (struct ChatFile *)file->node.mln_Succ) {
        if (index++ > 0)
            strncat(chatOutputTextEditorContents, ", ",
                    chatOutputTextEditorContentsCapacity -
                        strlen(chatOutputTextEditorContents) - 1);
        STRPTR name = CodesetsUTF8ToStr(
            CSA_DestCodeset, (Tag)systemCodeset, CSA_Source, (Tag)file->name,
            CSA_MapForeignChars, TRUE, TAG_DONE);
        strncat(chatOutputTextEditorContents,
                name != NULL ? name : (STRPTR)file->name,
                chatOutputTextEditorContentsCapacity -
                    strlen(chatOutputTextEditorContents) - 1);
        if (name != NULL)
            CodesetsFreeA(name, NULL);
    }
    strncat(chatOutputTextEditorContents, "]",
            chatOutputTextEditorContentsCapacity -
                strlen(chatOutputTextEditorContents) - 1);
}

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
 **/
void displayConversation(struct Conversation *conversation) {
    if (conversation == NULL) {
        conversation = currentConversation;
    }
    struct ConversationNode *conversationNode;
    chatOutputTextEditorContents[0] = '\0';

    ULONG estimatedRequired = 1024;
    for (conversationNode =
             (struct ConversationNode *)conversation->messages->mlh_Head;
         conversationNode->node.mln_Succ != NULL;
        conversationNode =
             (struct ConversationNode *)conversationNode->node.mln_Succ) {
        estimatedRequired += strlen(conversationNode->content) * 3 + 512;
        struct ChatFile *file;
        for (file = (struct ChatFile *)conversationNode->files.mlh_Head;
             file->node.mln_Succ != NULL;
             file = (struct ChatFile *)file->node.mln_Succ)
            estimatedRequired += strlen(file->name) * 3 + 8;
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
        if ((strlen(chatOutputTextEditorContents) +
             strlen(conversationNode->content) + 256) >
            chatOutputTextEditorContentsCapacity) {
            ULONG need = strlen(chatOutputTextEditorContents) +
                         strlen(conversationNode->content) * 3 + 1024;
            if (!ensureChatOutputBufferCapacity(need)) {
                displayError(STRING_ERROR_CONVERSATION_MAX_LENGTH_EXCEEDED);
                set(sendMessageButton, MUIA_Disabled, TRUE);
                return;
            }
        }
        if (strcmp(conversationNode->role, "user") == 0) {
            STRPTR content =
                CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset,
                                  CSA_Source, (Tag)conversationNode->content,
                                  CSA_MapForeignChars, TRUE, TAG_DONE);
            STRPTR latin1Fallback = NULL;
            BOOL freeWithCodesets = (content != NULL);
            if (content == NULL) {
                latin1Fallback = utf8ToLatin1(conversationNode->content);
                content = latin1Fallback ? latin1Fallback
                                         : (STRPTR)conversationNode->content;
            }
            UBYTE userAlignment = 'l';
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
            strncat(chatOutputTextEditorContents, userStyleString,
                    strlen(userStyleString));
            for (ULONG i = 0; i < strlen(content); i++) {
                strncat(chatOutputTextEditorContents, content + i, 1);
                if (content[i] == '\n') {
                    strncat(chatOutputTextEditorContents, userStyleString,
                            strlen(userStyleString));
                }
            }
            if (freeWithCodesets)
                CodesetsFreeA(content, NULL);
            else if (latin1Fallback != NULL)
                FreeVec(latin1Fallback);
            appendMessageFileSummary(conversationNode);
        } else if (strcmp(conversationNode->role, "assistant") == 0) {
            set(chatOutputListView, MUIA_NFloattext_Align,
                configGetAssistantTextAlignment());
            UBYTE assistantStyleString[] = "\n\n";
            strncat(chatOutputTextEditorContents, assistantStyleString,
                    strlen(assistantStyleString));
            UTF8 *formattedContentSystemEncoded =
                CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset,
                                  CSA_Source, (Tag)conversationNode->content,
                                  CSA_MapForeignChars, TRUE, TAG_DONE);
            CONST_STRPTR contentForFormatting = formattedContentSystemEncoded;
            STRPTR latin1Fallback = NULL;
            if (contentForFormatting == NULL) {
                latin1Fallback = utf8ToLatin1(conversationNode->content);
                contentForFormatting = latin1Fallback
                                           ? latin1Fallback
                                           : (STRPTR)conversationNode->content;
            }
            STRPTR formattedContent =
                convertMarkdownFormattingToMUI(contentForFormatting);
            if (formattedContentSystemEncoded != NULL)
                CodesetsFreeA(formattedContentSystemEncoded, NULL);
            if (latin1Fallback != NULL)
                FreeVec(latin1Fallback);
            if (formattedContent != NULL) {
                strncat(chatOutputTextEditorContents, formattedContent,
                        strlen(formattedContent));
                FreeVec(formattedContent);
            }
            appendMessageFileSummary(conversationNode);
            const UBYTE messageSeparatorStyleString[] = "\n\n";
            strncat(chatOutputTextEditorContents, messageSeparatorStyleString,
                    strlen(messageSeparatorStyleString));
        }
    }

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
        struct ConversationNode *copiedMessage = addTextToConversation(
            copy, conversationNode->content, conversationNode->role);
        if (copiedMessage != NULL)
            copyChatFiles(&copiedMessage->files, &conversationNode->files);
    }
    if (conversation->name != NULL) {
        copy->name = AllocVec(strlen(conversation->name) + 1, MEMF_CLEAR);
        strncpy(copy->name, conversation->name, strlen(conversation->name));
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
    newEntry->name = AllocVec(strlen(generatedImage->name) + 1, MEMF_CLEAR);
    strncpy(newEntry->name, generatedImage->name, strlen(generatedImage->name));
    newEntry->filePath =
        AllocVec(strlen(generatedImage->filePath) + 1, MEMF_CLEAR);
    strncpy(newEntry->filePath, generatedImage->filePath,
            strlen(generatedImage->filePath));
    newEntry->prompt = AllocVec(strlen(generatedImage->prompt) + 1, MEMF_CLEAR);
    strncpy(newEntry->prompt, generatedImage->prompt,
            strlen(generatedImage->prompt));
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
                json_object_new_string(conversationNode->content));
            struct json_object *filesJsonArray = json_object_new_array();
            struct ChatFile *chatFile;
            for (chatFile =
                     (struct ChatFile *)conversationNode->files.mlh_Head;
                 chatFile->node.mln_Succ != NULL;
                 chatFile = (struct ChatFile *)chatFile->node.mln_Succ) {
                struct json_object *fileJsonObject = json_object_new_object();
                json_object_object_add(
                    fileJsonObject, "name",
                    json_object_new_string(chatFile->name));
                if (chatFile->path != NULL)
                    json_object_object_add(
                        fileJsonObject, "path",
                        json_object_new_string(chatFile->path));
                if (chatFile->mimeType != NULL)
                    json_object_object_add(
                        fileJsonObject, "mimeType",
                        json_object_new_string(chatFile->mimeType));
                if (chatFile->fileId != NULL)
                    json_object_object_add(
                        fileJsonObject, "fileId",
                        json_object_new_string(chatFile->fileId));
                if (chatFile->containerId != NULL)
                    json_object_object_add(
                        fileJsonObject, "containerId",
                        json_object_new_string(chatFile->containerId));
                if (chatFile->downloadUrl != NULL)
                    json_object_object_add(
                        fileJsonObject, "downloadUrl",
                        json_object_new_string(chatFile->downloadUrl));
                json_object_array_add(filesJsonArray, fileJsonObject);
            }
            if (json_object_array_length(filesJsonArray) > 0) {
                json_object_object_add(messageJsonObject, "files",
                                       filesJsonArray);
            } else {
                json_object_put(filesJsonArray);
            }
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
 * Saves the generated speech phrases to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG saveSpeechHistory() {
    BPTR file = Open("AMIGAGPT:speech-history.json", MODE_NEWFILE);
    LONG totalSpeechCount;
    struct json_object *speechJsonArray;
    LONG i;
    STRPTR speechJsonString;

    if (file == 0) {
        displayError(STRING_ERROR_SPEECH_HISTORY_CREATE);
        return RETURN_ERROR;
    }

    get(speechListObject, MUIA_NList_Entries, &totalSpeechCount);
    speechJsonArray = json_object_new_array();
    for (i = 0; i < totalSpeechCount; i++) {
        struct json_object *speechJsonObject = json_object_new_object();
        struct GeneratedSpeech *entry = NULL;

        DoMethod(speechListObject, MUIM_NList_GetEntry, i, &entry);
        if (entry == NULL)
            continue;
        json_object_object_add(
            speechJsonObject, "title",
            json_object_new_string(entry->title != NULL ? entry->title : ""));
        json_object_object_add(speechJsonObject, "filePath",
                               json_object_new_string(entry->filePath != NULL
                                                          ? entry->filePath
                                                          : ""));
        json_object_object_add(
            speechJsonObject, "text",
            json_object_new_string(entry->text != NULL ? entry->text : ""));
        json_object_object_add(speechJsonObject, "profileInfo",
                               json_object_new_string(
                                   entry->profileInfo != NULL
                                       ? entry->profileInfo
                                       : ""));
        json_object_array_add(speechJsonArray, speechJsonObject);
    }

    speechJsonString = (STRPTR)json_object_to_json_string_ext(
        speechJsonArray, JSON_C_TO_STRING_PRETTY);

    if (Write(file, speechJsonString, strlen(speechJsonString)) !=
        (LONG)strlen(speechJsonString)) {
        displayError(STRING_ERROR_SPEECH_HISTORY_WRITE);
        Close(file);
        json_object_put(speechJsonArray);
        return RETURN_ERROR;
    }

    Close(file);
    json_object_put(speechJsonArray);
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
    if (conversationsJsonArray == NULL) {
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
                                       &messagesJsonArray)) {
            displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
            FreeVec(conversationsJsonString);
            json_object_put(conversationsJsonArray);
            set(conversationListObject, MUIA_NList_Quiet, FALSE);
            return RETURN_ERROR;
        }

        struct Conversation *conversation = newConversation();
        conversation->name =
            AllocVec(strlen(conversationName) + 1, MEMF_ANY | MEMF_CLEAR);
        strncpy(conversation->name, conversationName, strlen(conversationName));

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
            if (!json_object_object_get_ex(messageJsonObject, "role",
                                           &roleJsonObject)) {
                displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
                FreeVec(conversationsJsonString);
                json_object_put(conversationsJsonArray);
                set(conversationListObject, MUIA_NList_Quiet, FALSE);
                return RETURN_ERROR;
            }
            STRPTR role = json_object_get_string(roleJsonObject);

            struct json_object *contentJsonObject =
                json_object_array_get_idx(messagesJsonArray, j);
            if (!json_object_object_get_ex(messageJsonObject, "content",
                                           &contentJsonObject)) {
                displayError(STRING_ERROR_CHAT_HISTORY_PARSE_NO_BACKUP);
                FreeVec(conversationsJsonString);
                json_object_put(conversationsJsonArray);
                set(conversationListObject, MUIA_NList_Quiet, FALSE);
                return RETURN_ERROR;
            }
            UTF8 *content = json_object_get_string(contentJsonObject);
            struct ConversationNode *message =
                addTextToConversation(conversation, content, role);
            struct json_object *filesJsonArray = NULL;
            if (message != NULL &&
                json_object_object_get_ex(messageJsonObject, "files",
                                          &filesJsonArray) &&
                json_object_is_type(filesJsonArray, json_type_array)) {
                for (ULONG k = 0;
                     k < json_object_array_length(filesJsonArray); k++) {
                    struct json_object *fileJsonObject =
                        json_object_array_get_idx(filesJsonArray, k);
                    CONST_STRPTR name =
                        jsonStringValue(fileJsonObject, "name");
                    if (name == NULL)
                        continue;
                    addChatFile(
                        &message->files,
                        jsonStringValue(fileJsonObject, "path"), name,
                        jsonStringValue(fileJsonObject, "mimeType"),
                        jsonStringValue(fileJsonObject, "fileId"),
                        jsonStringValue(fileJsonObject, "containerId"),
                        jsonStringValue(fileJsonObject, "downloadUrl"));
                }
            }
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
        generatedImage->name = AllocVec(strlen(imageName) + 1, MEMF_ANY);
        strcpy(generatedImage->name, imageName);
        generatedImage->filePath =
            AllocVec(strlen(imageFilePath) + 1, MEMF_ANY);
        strcpy(generatedImage->filePath, imageFilePath);
        generatedImage->prompt = AllocVec(strlen(imagePrompt) + 1, MEMF_ANY);
        strcpy(generatedImage->prompt, imagePrompt);
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
 * Load the generated speech phrases from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG loadSpeechHistory() {
    BPTR file = Open("AMIGAGPT:speech-history.json", MODE_OLDFILE);
    STRPTR speechJsonString;
    struct json_object *speechJsonArray;
    UWORD i;
#ifdef __AMIGAOS3__
    LONG fileSize;
#else
#ifdef __AMIGAOS4__
    int64_t fileSize;
#else
    struct FileInfoBlock fib;
    int64_t fileSize;
#endif
#endif

    if (file == 0)
        return RETURN_OK;

#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    fileSize = Seek(file, 0, OFFSET_BEGINNING);
#else
#ifdef __AMIGAOS4__
    fileSize = GetFileSize(file);
#else
    ExamineFH64(file, &fib, NULL);
    fileSize = fib.fib_Size;
#endif
#endif
    speechJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
    if (Read(file, speechJsonString, fileSize) != fileSize) {
        displayError(STRING_ERROR_SPEECH_HISTORY_READ);
        Close(file);
        FreeVec(speechJsonString);
        return RETURN_ERROR;
    }

    Close(file);

    speechJsonArray = json_tokener_parse(speechJsonString);
    if (speechJsonArray == NULL) {
        if (Rename("AMIGAGPT:speech-history.json",
                   "AMIGAGPT:speech-history.json.bak")) {
            displayError(STRING_ERROR_SPEECH_HISTORY_PARSE_BACKUP);
        } else if (copyFile("AMIGAGPT:speech-history.json",
                            "RAM:speech-history.json")) {
            displayError(STRING_ERROR_SPEECH_HISTORY_PARSE_BACKUP_RAM);
            deleteDiskFile("AMIGAGPT:speech-history.json");
        }

        FreeVec(speechJsonString);
        return RETURN_ERROR;
    }

    for (i = 0; i < json_object_array_length(speechJsonArray); i++) {
        struct json_object *speechJsonObject =
            json_object_array_get_idx(speechJsonArray, i);
        CONST_STRPTR title = jsonStringValue(speechJsonObject, "title");
        CONST_STRPTR filePath = jsonStringValue(speechJsonObject, "filePath");
        CONST_STRPTR text = jsonStringValue(speechJsonObject, "text");
        CONST_STRPTR profileInfo =
            jsonStringValue(speechJsonObject, "profileInfo");
        struct GeneratedSpeech *entry;

        if (filePath == NULL || text == NULL) {
            displayError(STRING_ERROR_SPEECH_HISTORY_PARSE_NO_BACKUP);
            FreeVec(speechJsonString);
            json_object_put(speechJsonArray);
            return RETURN_ERROR;
        }

        entry = AllocVec(sizeof(*entry), MEMF_ANY | MEMF_CLEAR);
        if (entry == NULL)
            continue;
        entry->filePath = AllocVec(strlen(filePath) + 1, MEMF_ANY | MEMF_CLEAR);
        entry->text = AllocVec(strlen(text) + 1, MEMF_ANY | MEMF_CLEAR);
        if (title != NULL) {
            entry->title = AllocVec(strlen(title) + 1, MEMF_ANY | MEMF_CLEAR);
            if (entry->title != NULL)
                strcpy(entry->title, title);
        } else {
            entry->title = speechTitle(text);
        }
        if (entry->filePath != NULL)
            strcpy(entry->filePath, filePath);
        if (entry->text != NULL)
            strcpy(entry->text, text);
        if (profileInfo != NULL && profileInfo[0] != '\0')
            entry->profileInfo = copySpeechString(profileInfo);
        DoMethod(speechListObject, MUIM_NList_InsertSingle, entry,
                 MUIV_NList_Insert_Top);
    }

    FreeVec(speechJsonString);
    json_object_put(speechJsonArray);
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
            UTF8 *content = conversationNode->content;
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
