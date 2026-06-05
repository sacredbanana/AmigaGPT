#ifdef __MORPHOS__

#include "ChatFindScintilla.h"

#include "AmigaGPT_cat.h"
#include "ChatOutputScintilla.h"

#include <Scintilla/Scintilla.h>
#include "CodeBlocksScintilla.h"
#include "gui.h"
#include <SDI_hook.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <mui/Scintilla_mcc.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

#define CHAT_FIND_MAX 256
#define CHAT_FIND_COUNT_MAX 32

static Object *chatFindApp;
static Object *chatFindBar;
static Object *chatFindString;
static Object *chatFindPrevBtn;
static Object *chatFindNextBtn;
static Object *chatFindCloseBtn;
static Object *chatFindCountText;
static Object *chatUserCountText;
static Object *chatUserPrevBtn;
static Object *chatUserNextBtn;
static Object *chatFindWin;
static Object *chatFindSci;
static char chatFindCountBuf[CHAT_FIND_COUNT_MAX];
static char chatUserCountBuf[CHAT_FIND_COUNT_MAX];
static char chatFindLast[CHAT_FIND_MAX];

static ULONG *chatUserStarts;
static ULONG chatUserCount;
static BOOL chatFindBusy;

static BOOL chatFindBarVisible(void);

static void chatFindRefocusString(void)
{
    if (!chatFindBarVisible() || !chatFindWin || !chatFindString)
        return;

    if (chatFindSci)
        SetAttrs(chatFindSci, SCIA_ActiveEditor, FALSE, TAG_DONE);
    SetAttrs(chatFindWin, MUIA_Window_ActiveObject, chatFindString, TAG_DONE);
}

HOOKPROTONHNONP(ChatFindStringAckDoFunc, void)
{
    if (mainWindowIsShuttingDown() || chatFindBusy)
        return;

    chatFindScintillaNext(chatFindSci);
    chatFindRefocusString();
}
MakeHook(ChatFindStringAckDoHook, ChatFindStringAckDoFunc);

HOOKPROTONHNONP(ChatFindStringAckNotifyFunc, void)
{
    if (app == NULL || mainWindowIsShuttingDown())
        return;

    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &ChatFindStringAckDoHook);
}
MakeHook(ChatFindStringAckNotifyHook, ChatFindStringAckNotifyFunc);

HOOKPROTONHNONP(ChatFindStringUpdateDoFunc, void)
{
    if (mainWindowIsShuttingDown() || chatFindBusy)
        return;

    chatFindScintillaUpdateCounter(chatFindSci);
}
MakeHook(ChatFindStringUpdateDoHook, ChatFindStringUpdateDoFunc);

HOOKPROTONHNONP(ChatFindStringUpdateNotifyFunc, void)
{
    if (app == NULL || mainWindowIsShuttingDown())
        return;

    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &ChatFindStringUpdateDoHook);
}
MakeHook(ChatFindStringUpdateNotifyHook, ChatFindStringUpdateNotifyFunc);

static BOOL chatFindBarVisible(void)
{
    LONG shown = 0;

    if (!chatFindBar)
        return FALSE;

    GetAttr(MUIA_ShowMe, chatFindBar, (ULONG *)&shown);
    return shown ? TRUE : FALSE;
}

void chatFindScintillaSetContext(Object *mainWindow, Object *chatSci)
{
    chatFindWin = mainWindow;
    chatFindSci = chatSci;
}

void chatFindScintillaOnStringFocus(void)
{
    chatFindRefocusString();
}

void chatFindScintillaOnEditorFocus(void)
{
    if (!chatFindBarVisible())
        return;

    if (chatFindSci)
        SetAttrs(chatFindSci, SCIA_ActiveEditor, TRUE, TAG_DONE);

    if (chatFindWin && chatFindSci) {
        SetAttrs(chatFindWin, MUIA_Window_ActiveObject, chatFindSci, TAG_DONE);
        SetAttrs(chatFindWin, MUIA_Window_Activate, TRUE, TAG_DONE);
    }
}

static void chatFindActivateEditor(void)
{
    if (chatFindSci)
        SetAttrs(chatFindSci, SCIA_ActiveEditor, TRUE, TAG_DONE);

    if (chatFindWin && chatFindSci) {
        SetAttrs(chatFindWin, MUIA_Window_ActiveObject, chatFindSci, TAG_DONE);
        SetAttrs(chatFindWin, MUIA_Window_Activate, TRUE, TAG_DONE);
    }
}

static void chatFindReadStringField(void)
{
    STRPTR contents = NULL;

    if (!chatFindString)
        return;

    GetAttr(MUIA_String_Contents, chatFindString, (ULONG *)&contents);
    if (!contents)
        return;

    strncpy(chatFindLast, (const char *)contents, sizeof(chatFindLast) - 1);
    chatFindLast[sizeof(chatFindLast) - 1] = '\0';
}

static void chatFindCopySelection(Object *sci)
{
    LONG selStart;
    LONG selEnd;
    LONG len;
    char buf[CHAT_FIND_MAX];

    if (!sci)
        return;

    selStart =
        (LONG)codeBlocksScintillaCommand(sci, SCI_GETSELECTIONSTART, 0, 0);
    selEnd = (LONG)codeBlocksScintillaCommand(sci, SCI_GETSELECTIONEND, 0, 0);
    if (selEnd <= selStart)
        return;

    len = selEnd - selStart;
    if (len >= (LONG)sizeof(buf))
        len = (LONG)sizeof(buf) - 1;

    buf[0] = '\0';
    codeBlocksScintillaCommand(sci, SCI_GETSELTEXT, 0, (sptr_t)buf);
    if (buf[0] == '\0')
        return;

    strncpy(chatFindLast, buf, sizeof(chatFindLast) - 1);
    chatFindLast[sizeof(chatFindLast) - 1] = '\0';
}

static LONG chatFindCountMatches(Object *sci, const char *needle)
{
    struct Sci_TextToFind ft;
    LONG docLen;
    LONG pos;
    LONG found;
    LONG count = 0;

    if (!sci || !needle || needle[0] == '\0')
        return 0;

    docLen = (LONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    if (docLen <= 0)
        return 0;

    ft.lpstrText = needle;
    pos = 0;

    while (pos < docLen) {
        ft.chrg.cpMin = pos;
        ft.chrg.cpMax = docLen;
        ft.chrgText.cpMin = 0;
        ft.chrgText.cpMax = 0;

        found =
            (LONG)codeBlocksScintillaCommand(sci, SCI_FINDTEXT, 0, (sptr_t)&ft);
        if (found < 0)
            break;

        count++;
        pos = found + 1;
    }

    return count;
}

static LONG chatFindMatchIndex(Object *sci, const char *needle, LONG selStart)
{
    struct Sci_TextToFind ft;
    LONG docLen;
    LONG pos;
    LONG found;
    LONG count = 0;

    if (!sci || !needle || needle[0] == '\0')
        return 0;

    docLen = (LONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    if (docLen <= 0)
        return 0;

    ft.lpstrText = needle;
    pos = 0;

    while (pos < docLen) {
        ft.chrg.cpMin = pos;
        ft.chrg.cpMax = docLen;
        ft.chrgText.cpMin = 0;
        ft.chrgText.cpMax = 0;

        found =
            (LONG)codeBlocksScintillaCommand(sci, SCI_FINDTEXT, 0, (sptr_t)&ft);
        if (found < 0)
            break;

        count++;
        if (selStart >= (LONG)ft.chrgText.cpMin &&
            selStart <= (LONG)ft.chrgText.cpMax)
            return count;

        pos = found + 1;
    }

    return 0;
}

static void chatFindRefreshCounter(Object *sci)
{
    LONG total;
    LONG index;
    LONG selStart;

    if (!chatFindCountText)
        return;

    if (!chatFindBarVisible()) {
        chatFindCountBuf[0] = '\0';
        SetAttrs(chatFindCountText, MUIA_Text_Contents,
                 (ULONG)chatFindCountBuf, TAG_DONE);
        chatUserCountBuf[0] = '\0';
        if (chatUserCountText) {
            SetAttrs(chatUserCountText, MUIA_Text_Contents,
                     (ULONG)chatUserCountBuf, TAG_DONE);
        }
        return;
    }

    chatFindReadStringField();
    if (chatFindLast[0] == '\0' || !sci) {
        chatFindCountBuf[0] = '\0';
        SetAttrs(chatFindCountText, MUIA_Text_Contents,
                 (ULONG)chatFindCountBuf, TAG_DONE);
        return;
    }

    total = chatFindCountMatches(sci, chatFindLast);
    selStart =
        (LONG)codeBlocksScintillaCommand(sci, SCI_GETSELECTIONSTART, 0, 0);
    index = chatFindMatchIndex(sci, chatFindLast, selStart);

    snprintf(chatFindCountBuf, sizeof(chatFindCountBuf),
             (const char *)STRING_FIND_MATCHES, index, total);
    SetAttrs(chatFindCountText, MUIA_Text_Contents, (ULONG)chatFindCountBuf,
             TAG_DONE);
}

static ULONG chatUserNavIndexAtPos(LONG pos)
{
    ULONG j;
    ULONG index = 0;

    for (j = 0; j < chatUserCount; j++) {
        if ((LONG)chatUserStarts[j] <= pos)
            index = j + 1;
        else
            break;
    }

    return index;
}

static void chatUserNavRefreshCounter(Object *sci)
{
    LONG pos;
    ULONG index;

    if (!chatUserCountText)
        return;

    if (!chatFindBarVisible() || !sci || chatUserCount == 0) {
        chatUserCountBuf[0] = '\0';
        SetAttrs(chatUserCountText, MUIA_Text_Contents, (ULONG)chatUserCountBuf,
                 TAG_DONE);
        return;
    }

    pos = (LONG)codeBlocksScintillaCommand(sci, SCI_GETSELECTIONSTART, 0, 0);
    index = chatUserNavIndexAtPos(pos);
    if (index == 0) {
        chatUserCountBuf[0] = '\0';
    } else {
        snprintf(chatUserCountBuf, sizeof(chatUserCountBuf),
                 (const char *)STRING_FIND_MATCHES, (long)index,
                 (long)chatUserCount);
    }
    SetAttrs(chatUserCountText, MUIA_Text_Contents, (ULONG)chatUserCountBuf,
             TAG_DONE);
}

static BOOL chatFindSelectMatch(Object *sci, LONG start, LONG end)
{
    if (!sci || start < 0 || end < start)
        return FALSE;

    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETSEL, (uptr_t)start, (sptr_t)end);
    codeBlocksScintillaCommand(sci, SCI_SCROLLCARET, 0, 0);
    codeBlocksScintillaCommand(sci, SCI_SETREADONLY, 1, 0);
    chatFindRefreshCounter(sci);
    chatUserNavRefreshCounter(sci);
    return TRUE;
}

void chatFindScintillaUpdateCounter(Object *sci)
{
    if (!sci)
        sci = chatFindSci;
    chatFindRefreshCounter(sci);
    chatUserNavRefreshCounter(sci);
}

static BOOL chatFindLastInRange(Object *sci, const char *needle, LONG rangeMin,
                                LONG rangeMax, LONG *outStart, LONG *outEnd)
{
    struct Sci_TextToFind ft;
    LONG pos;
    LONG found;
    LONG bestStart = -1;
    LONG bestEnd = -1;

    if (!sci || !needle || needle[0] == '\0' || rangeMax <= rangeMin)
        return FALSE;

    ft.lpstrText = needle;
    pos = rangeMin;

    while (pos < rangeMax) {
        ft.chrg.cpMin = pos;
        ft.chrg.cpMax = rangeMax;
        ft.chrgText.cpMin = 0;
        ft.chrgText.cpMax = 0;

        found =
            (LONG)codeBlocksScintillaCommand(sci, SCI_FINDTEXT, 0, (sptr_t)&ft);
        if (found < 0)
            break;

        bestStart = (LONG)ft.chrgText.cpMin;
        bestEnd = (LONG)ft.chrgText.cpMax;
        pos = found + 1;
    }

    if (bestStart < 0)
        return FALSE;

    if (outStart)
        *outStart = bestStart;
    if (outEnd)
        *outEnd = bestEnd;
    return TRUE;
}

static BOOL chatFindSearch(Object *sci, const char *needle, LONG startPos,
                           BOOL wrap)
{
    struct Sci_TextToFind ft;
    LONG docLen;
    LONG found;

    if (!sci || !needle || needle[0] == '\0')
        return FALSE;

    docLen = (LONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    if (docLen <= 0)
        return FALSE;

    if (startPos < 0)
        startPos = 0;
    if (startPos > docLen)
        startPos = docLen;

    ft.lpstrText = needle;
    ft.chrg.cpMin = startPos;
    ft.chrg.cpMax = docLen;
    ft.chrgText.cpMin = 0;
    ft.chrgText.cpMax = 0;

    found = (LONG)codeBlocksScintillaCommand(sci, SCI_FINDTEXT, 0, (sptr_t)&ft);
    if (found < 0 && wrap && startPos > 0) {
        ft.chrg.cpMin = 0;
        ft.chrg.cpMax = startPos;
        found =
            (LONG)codeBlocksScintillaCommand(sci, SCI_FINDTEXT, 0, (sptr_t)&ft);
    }

    if (found < 0)
        return FALSE;

    return chatFindSelectMatch(sci, (LONG)ft.chrgText.cpMin,
                               (LONG)ft.chrgText.cpMax);
}

static BOOL chatFindSearchPrev(Object *sci, const char *needle, LONG beforePos,
                               BOOL wrap)
{
    LONG docLen;
    LONG start;
    LONG end;

    if (!sci || !needle || needle[0] == '\0')
        return FALSE;

    docLen = (LONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    if (docLen <= 0)
        return FALSE;

    if (beforePos < 0)
        beforePos = 0;
    if (beforePos > docLen)
        beforePos = docLen;

    if (chatFindLastInRange(sci, needle, 0, beforePos, &start, &end))
        return chatFindSelectMatch(sci, start, end);

    if (!wrap || docLen <= 0)
        return FALSE;

    return chatFindLastInRange(sci, needle, 0, docLen, &start, &end) &&
           chatFindSelectMatch(sci, start, end);
}

BOOL chatFindScintillaInit(void)
{
    Object *promptStringGroup;
    Object *findUserSpacer;

    if (chatFindBar)
        return TRUE;

    chatFindLast[0] = '\0';
    chatFindCountBuf[0] = '\0';
    chatUserCountBuf[0] = '\0';

    promptStringGroup = MUI_NewObject(
        MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 0,
        MUIA_Group_Child,
        MUI_NewObject(MUIC_Text, MUIA_Text_Contents, (ULONG)STRING_FIND_PROMPT,
                      TAG_DONE),
        MUIA_Group_Child,
        chatFindString = MUI_NewObject(
            MUIC_String, MUIA_String_Contents, (ULONG)chatFindLast,
            MUIA_String_MaxLen, (ULONG)(sizeof(chatFindLast) - 1),
            MUIA_String_AdvanceOnCR, FALSE, MUIA_HorizWeight, 100, TAG_DONE),
        TAG_DONE);

    findUserSpacer = RectangleObject, MUIA_FixWidth, 24, End;

    chatFindBar = MUI_NewObject(
        MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_ShowMe, FALSE,
        MUIA_Group_Child, promptStringGroup,
        MUIA_Group_Child,
        chatFindCountText = MUI_NewObject(
            MUIC_Text, MUIA_Text_Contents, (ULONG)chatFindCountBuf, TAG_DONE),
        MUIA_Group_Child,
        chatFindPrevBtn = MUI_MakeObject(MUIO_Button, (ULONG)STRING_FIND_PREVIOUS,
                                         MUIA_InputMode, MUIV_InputMode_RelVerify,
                                         TAG_DONE),
        MUIA_Group_Child,
        chatFindNextBtn = MUI_MakeObject(MUIO_Button, (ULONG)STRING_FIND_NEXT,
                                         MUIA_InputMode, MUIV_InputMode_RelVerify,
                                         TAG_DONE),
        MUIA_Group_Child, findUserSpacer,
        MUIA_Group_Child,
        chatUserPrevBtn = MUI_MakeObject(MUIO_Button, (ULONG)STRING_CHAT_USER_PREV,
                                         MUIA_InputMode, MUIV_InputMode_RelVerify,
                                         TAG_DONE),
        MUIA_Group_Child,
        chatUserNextBtn = MUI_MakeObject(MUIO_Button, (ULONG)STRING_CHAT_USER_NEXT,
                                         MUIA_InputMode, MUIV_InputMode_RelVerify,
                                         TAG_DONE),
        MUIA_Group_Child,
        chatUserCountText = MUI_NewObject(
            MUIC_Text, MUIA_Text_Contents, (ULONG)chatUserCountBuf, TAG_DONE),
        MUIA_Group_Child,
        chatFindCloseBtn = MUI_MakeObject(MUIO_Button, (ULONG)STRING_FIND_CLOSE,
                                          MUIA_InputMode,
                                          MUIV_InputMode_RelVerify, TAG_DONE),
        TAG_DONE);

    return (chatFindBar && chatFindString && chatFindCountText &&
            chatFindPrevBtn && chatFindNextBtn && chatFindCloseBtn &&
            chatUserPrevBtn && chatUserNextBtn && chatUserCountText &&
            promptStringGroup && findUserSpacer);
}

BOOL chatFindScintillaWireToApp(Object *app)
{
    if (!app || !chatFindBar)
        return FALSE;

    chatFindApp = app;

    DoMethod(chatFindPrevBtn, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
             MUIM_Application_ReturnID, APP_ID_CHAT_FIND_PREV);
    DoMethod(chatFindNextBtn, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
             MUIM_Application_ReturnID, APP_ID_CHAT_FIND_NEXT);
    DoMethod(chatFindCloseBtn, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
             MUIM_Application_ReturnID, APP_ID_CHAT_FIND_CLOSE);
    DoMethod(chatUserPrevBtn, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
             MUIM_Application_ReturnID, APP_ID_CHAT_USER_PREV);
    DoMethod(chatUserNextBtn, MUIM_Notify, MUIA_Pressed, FALSE, app, 2,
             MUIM_Application_ReturnID, APP_ID_CHAT_USER_NEXT);
    DoMethod(chatFindString, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             app, 3, MUIM_CallHook, &ChatFindStringAckNotifyHook);
    DoMethod(chatFindString, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
             app, 3, MUIM_CallHook, &ChatFindStringUpdateNotifyHook);

    if (chatFindWin && chatFindSci) {
        DoMethod(chatFindWin, MUIM_Window_SetCycleChain, chatFindString,
                 chatFindPrevBtn, chatFindNextBtn, chatUserPrevBtn,
                 chatUserNextBtn, chatFindCloseBtn, chatFindSci, NULL);
    }

    return TRUE;
}

Object *chatFindScintillaGetBar(void)
{
    return chatFindBar;
}

void chatFindScintillaHide(void)
{
    if (!chatFindBar)
        return;

    chatFindBusy = FALSE;
    SetAttrs(chatFindBar, MUIA_ShowMe, FALSE, TAG_DONE);
    chatFindActivateEditor();
}

void chatFindScintillaShow(Object *sci)
{
    if (!chatFindBar || !chatFindString)
        return;

    if (sci)
        chatFindCopySelection(sci);

    SetAttrs(chatFindString, MUIA_String_Contents, (ULONG)chatFindLast,
             TAG_DONE);
    SetAttrs(chatFindBar, MUIA_ShowMe, TRUE, TAG_DONE);
    chatFindScintillaUpdateCounter(sci);
    chatFindScintillaOnStringFocus();
}

BOOL chatFindScintillaNext(Object *sci)
{
    LONG pos;
    LONG docLen;
    BOOL found;

    if (!sci || chatFindBusy)
        return FALSE;

    chatFindReadStringField();
    if (chatFindLast[0] == '\0')
        return FALSE;

    chatFindBusy = TRUE;
    pos = (LONG)codeBlocksScintillaCommand(sci, SCI_GETSELECTIONEND, 0, 0);
    docLen = (LONG)codeBlocksScintillaCommand(sci, SCI_GETLENGTH, 0, 0);
    if (pos < docLen)
        pos++;

    found = chatFindSearch(sci, chatFindLast, pos, TRUE);
    chatFindBusy = FALSE;
    return found;
}

BOOL chatFindScintillaPrev(Object *sci)
{
    LONG pos;
    BOOL found;

    if (!sci || chatFindBusy)
        return FALSE;

    chatFindReadStringField();
    if (chatFindLast[0] == '\0')
        return FALSE;

    chatFindBusy = TRUE;
    pos = (LONG)codeBlocksScintillaCommand(sci, SCI_GETSELECTIONSTART, 0, 0);
    found = chatFindSearchPrev(sci, chatFindLast, pos, TRUE);
    chatFindBusy = FALSE;
    return found;
}

void chatUserNavClear(void)
{
    if (chatUserStarts != NULL) {
        FreeVec(chatUserStarts);
        chatUserStarts = NULL;
    }
    chatUserCount = 0;
}

void chatUserNavRebuild(const UBYTE *displayStyles, ULONG displayLen)
{
    ULONG i;
    ULONG count = 0;

    chatUserNavClear();

    if (displayStyles == NULL || displayLen == 0)
        return;

    for (i = 0; i < displayLen; i++) {
        if (displayStyles[i] == CHAT_OUTPUT_STYLE_USER &&
            (i == 0 || displayStyles[i - 1] != CHAT_OUTPUT_STYLE_USER))
            count++;
    }

    if (count == 0)
        return;

    chatUserStarts = (ULONG *)AllocVec(count * sizeof(ULONG), MEMF_ANY);
    if (chatUserStarts == NULL)
        return;

    chatUserCount = 0;
    for (i = 0; i < displayLen; i++) {
        if (displayStyles[i] == CHAT_OUTPUT_STYLE_USER &&
            (i == 0 || displayStyles[i - 1] != CHAT_OUTPUT_STYLE_USER))
            chatUserStarts[chatUserCount++] = i;
    }
    if (chatFindSci != NULL)
        chatUserNavRefreshCounter(chatFindSci);
}

static BOOL chatUserNavGoto(Object *sci, ULONG pos)
{
    if (!sci)
        return FALSE;

    codeBlocksScintillaCommand(sci, SCI_GOTOPOS, (uptr_t)pos, 0);
    codeBlocksScintillaCommand(sci, SCI_SCROLLCARET, 0, 0);
    chatUserNavRefreshCounter(sci);
    return TRUE;
}

BOOL chatUserNavNext(Object *chatSci)
{
    LONG pos;
    ULONG j;

    if (!chatSci || chatUserCount == 0)
        return FALSE;

    pos = (LONG)codeBlocksScintillaCommand(chatSci, SCI_GETSELECTIONSTART, 0, 0);

    for (j = 0; j < chatUserCount; j++) {
        if ((LONG)chatUserStarts[j] > pos)
            return chatUserNavGoto(chatSci, chatUserStarts[j]);
    }

    return chatUserNavGoto(chatSci, chatUserStarts[0]);
}

BOOL chatUserNavPrev(Object *chatSci)
{
    LONG pos;
    LONG j;

    if (!chatSci || chatUserCount == 0)
        return FALSE;

    pos = (LONG)codeBlocksScintillaCommand(chatSci, SCI_GETSELECTIONSTART, 0, 0);

    for (j = (LONG)chatUserCount - 1; j >= 0; j--) {
        if ((LONG)chatUserStarts[j] < pos)
            return chatUserNavGoto(chatSci, chatUserStarts[j]);
    }

    return chatUserNavGoto(chatSci, chatUserStarts[chatUserCount - 1]);
}

#endif /* __MORPHOS__ */
