#ifndef DAEMON
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <mui/NList_mcc.h>
#include <mui/NListtree_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "agent_internal.h"
#include "AgentPage.h"
#include "AmigaGPTConfig.h"
#include "gui.h"
#include "MainWindow.h"
#include <mui/BestTextEditor_mcc.h>

#define AGENT_TRANSCRIPT_INITIAL_CAPACITY 16384

/* What a row of the session tree stands for, kept in tn_User. A project row
 * holds the project index plus one, so 0 is "no project"; a session row
 * holds the session index */
#define AGENT_NODE_PROJECT 0x10000000UL
#define AGENT_NODE_SESSION 0x20000000UL
#define AGENT_NODE_INDEX_MASK 0x0FFFFFFFUL

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d)                                                    \
    ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

static Object *agentSessionTree = NULL;
static Object *agentAddProjectButton = NULL;
static Object *agentRemoveProjectButton = NULL;
static Object *agentDeleteSessionButton = NULL;
static Object *agentApprovalCycle = NULL;
static Object *agentEffortCycle = NULL;
static Object *agentModelString = NULL;
static Object *agentInstructionsButton = NULL;
static Object *agentMcpButton = NULL;
static Object *agentImportSkillButton = NULL;
static Object *agentNewSessionButton = NULL;
static Object *agentControlsGroup = NULL;
static Object *agentOutput = NULL;
static Object *agentOutputScroller = NULL;
static Object *agentInputEditor = NULL;
static Object *agentSendButton = NULL;
static Object *agentEditorWindow = NULL;
static Object *agentEditorText = NULL;
static Object *agentEditorSaveButton = NULL;
static Object *agentEditorCancelButton = NULL;

static STRPTR agentTranscript = NULL;
static ULONG agentTranscriptCapacity = 0;
static ULONG agentTranscriptLength = 0;
static UBYTE agentEditorPath[AGENT_PATH_LENGTH];
static BOOL agentUpdatingSessionTree = FALSE;
static STRPTR agentApprovalLabels[AGENT_APPROVAL_COUNT + 1] = {NULL};
static STRPTR agentEffortLabels[AGENT_EFFORT_COUNT + 1] = {NULL};

/* The transcript follows new output, so show its end */
static void showTranscript(void) {
    if (agentOutput != NULL)
        DoMethod(agentOutput, MUIM_BestTextEditor_ScrollTo,
                 MUIV_BestTextEditor_ScrollTo_Bottom);
}

/* Add local character set text to the transcript. The text may hold the
 * view's formatting codes; the copy kept here survives the window being
 * rebuilt, the view only ever gets the new part appended */
static void appendTranscript(CONST_STRPTR text) {
    ULONG length;
    if (text == NULL)
        return;
    length = strlen(text);
    if (agentTranscriptLength + length + 1 > agentTranscriptCapacity) {
        ULONG capacity = agentTranscriptCapacity > 0
                             ? agentTranscriptCapacity
                             : AGENT_TRANSCRIPT_INITIAL_CAPACITY;
        STRPTR bigger;
        while (agentTranscriptLength + length + 1 > capacity)
            capacity *= 2;
        bigger = AllocVec(capacity, MEMF_ANY | MEMF_CLEAR);
        if (bigger == NULL)
            return;
        if (agentTranscript != NULL) {
            CopyMem(agentTranscript, bigger, agentTranscriptLength);
            FreeVec(agentTranscript);
        }
        agentTranscript = bigger;
        agentTranscriptCapacity = capacity;
    }
    CopyMem((APTR)text, agentTranscript + agentTranscriptLength, length);
    agentTranscriptLength += length;
    agentTranscript[agentTranscriptLength] = '\0';
    if (agentOutput != NULL)
        DoMethod(agentOutput, MUIM_BestTextEditor_Append, (IPTR)text, -1,
                 MUIF_BestTextEditor_Append_NoScroll);
}

/* What the last block in the transcript was, to space blocks apart */
static LONG lastBlockKind = -1;

/* Bubble styles of the view (see <mui/BestTextEditor_mcc.h>) */
#define AGENT_BUBBLE_USER 1
#define AGENT_BUBBLE_ASSISTANT 2
#define AGENT_BUBBLE_ERROR 4

static UBYTE agentBubbleSide(LONG alignment) {
    switch (alignment) {
    case ALIGN_CENTER:
        return 'c';
    case ALIGN_RIGHT:
        return 'r';
    default:
        return 'l';
    }
}

/* Add text, putting a style code at the start of each of its lines, since
 * a style lasts only until the end of its line */
static void appendStyledLines(CONST_STRPTR style, CONST_STRPTR text) {
    CONST_STRPTR line = text;
    while (line != NULL) {
        CONST_STRPTR end = strchr(line, '\n');
        appendTranscript(style);
        if (end == NULL) {
            appendTranscript(line);
            break;
        }
        {
            ULONG length = (ULONG)(end - line) + 1;
            STRPTR piece = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
            if (piece != NULL) {
                CopyMem((APTR)line, piece, length);
                appendTranscript(piece);
                FreeVec(piece);
            }
        }
        line = end + 1;
    }
}

/* Text without the line breaks at its end, which would leave empty lines at
 * the bottom of a bubble */
static void appendTrimmed(CONST_STRPTR text) {
    ULONG length = strlen(text);
    STRPTR copy;
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == ' '))
        length--;
    copy = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
    if (copy == NULL)
        return;
    CopyMem((APTR)text, copy, length);
    appendTranscript(copy);
    FreeVec(copy);
}

/**
 * Add one block of the conversation in the style of its kind: the user's
 * and the agent's messages in chat bubbles, reasoning and tool calls as
 * coloured text between them
 * @param text the text in the local character set, without format codes
 **/
static void appendBlock(AgentOutputKind kind, CONST_STRPTR text) {
    UBYTE codes[40];

    /* Every block starts on a line of its own, since the codes that choose
     * its bubble and Markdown only work at the start of a line */
    if (agentTranscriptLength > 0)
        appendTranscript("\n");
    /* Blocks outside bubbles are kept apart by a blank line, except tool
     * calls in a row */
    if (agentTranscriptLength > 1 && lastBlockKind >= 0 &&
        (kind == AGENT_OUTPUT_REASONING || kind == AGENT_OUTPUT_TOOL ||
         kind == AGENT_OUTPUT_INFO) &&
        lastBlockKind != AGENT_OUTPUT_USER &&
        lastBlockKind != AGENT_OUTPUT_ERROR &&
        lastBlockKind != (LONG)kind &&
        !(kind == AGENT_OUTPUT_TOOL && lastBlockKind == AGENT_OUTPUT_TOOL))
        appendTranscript("\033[B:0]\n");

    switch (kind) {
    case AGENT_OUTPUT_USER:
        /* The same as a user message on the Chat tab */
        snprintf(codes, sizeof(codes), "\033[B:%d%c]\033[M:0]",
                 AGENT_BUBBLE_USER,
                 agentBubbleSide(configGetUserTextAlignment()));
        appendTranscript(codes);
        appendTrimmed(text);
        break;
    case AGENT_OUTPUT_REASONING:
        /* Grey italics, still with Markdown */
        appendTranscript("\033[B:0]\033[M:1]");
        appendStyledLines("\033i\033P[8C8C8C]", text);
        break;
    case AGENT_OUTPUT_TOOL:
        /* Amber, exactly as the tool reported it */
        appendTranscript("\033[B:0]\033[M:0]");
        appendStyledLines("\033P[D08A1E]", text);
        break;
    case AGENT_OUTPUT_ERROR:
        snprintf(codes, sizeof(codes), "\033[B:%d]\033[M:0]",
                 AGENT_BUBBLE_ERROR);
        appendTranscript(codes);
        appendTrimmed(text);
        break;
    case AGENT_OUTPUT_INFO:
        appendTranscript("\033[B:0]\033[M:1]");
        appendTrimmed(text);
        break;
    default:
        snprintf(codes, sizeof(codes), "\033[B:%d%c]\033[M:1]",
                 AGENT_BUBBLE_ASSISTANT,
                 agentBubbleSide(configGetAssistantTextAlignment()));
        appendTranscript(codes);
        appendTrimmed(text);
        break;
    }
    lastBlockKind = kind;
}

static void clearTranscript(void) {
    lastBlockKind = -1;
    agentTranscriptLength = 0;
    if (agentTranscript != NULL)
        agentTranscript[0] = '\0';
    if (agentOutput != NULL)
        DoMethod(agentOutput, MUIM_TextEditor_ClearText);
}

/* Transcript text arrives from the agent in UTF-8 */
static void agentOutputReceived(AgentOutputKind kind, CONST_STRPTR text) {
    STRPTR local = agentToLocal(text);
    STRPTR p;
    if (local == NULL)
        return;
    /* The text object reads ESC as the start of a formatting code */
    for (p = local; *p != '\0'; p++) {
        if (*p == '\33')
            *p = ' ';
    }
    appendBlock(kind, local);
    FreeVec(local);
    showTranscript();
}

static void setAgentSendButtonLabel(BOOL busy) {
    UBYTE label[64];
    if (agentSendButton == NULL)
        return;
    snprintf(label, sizeof(label), "\33c\33P[%ld]%s", busy ? redPen : bluePen,
             busy ? STRING_STOP : STRING_SEND);
    set(agentSendButton, MUIA_Text_Contents, label);
}

/* New is green with a +, Delete is red with a -, as on the other tabs.
 * MUI copies MUIA_Text_Contents, so the buffer can live on the stack.
 * ESC P[p] is the text pen (MUI_Text.doc); the pen comes from ObtainBestPen. */
void agentPageColorButtons(void) {
    UBYTE label[64];

    if (agentAddProjectButton != NULL) {
        snprintf(label, sizeof(label), "\33c\33P[%ld]+ %s", greenPen,
                 STRING_AGENT_ADD_PROJECT);
        set(agentAddProjectButton, MUIA_Text_Contents, label);
    }
    if (agentRemoveProjectButton != NULL) {
        snprintf(label, sizeof(label), "\33c\33P[%ld]- %s", redPen,
                 STRING_AGENT_REMOVE_PROJECT);
        set(agentRemoveProjectButton, MUIA_Text_Contents, label);
    }
    if (agentNewSessionButton != NULL) {
        snprintf(label, sizeof(label), "\33c\33P[%ld]+ %s", greenPen,
                 STRING_AGENT_NEW_SESSION);
        set(agentNewSessionButton, MUIA_Text_Contents, label);
    }
    if (agentDeleteSessionButton != NULL) {
        snprintf(label, sizeof(label), "\33c\33P[%ld]- %s", redPen,
                 STRING_AGENT_DELETE_SESSION);
        set(agentDeleteSessionButton, MUIA_Text_Contents, label);
    }
}

/* The tree row that is selected, decoded, or 0 when there is none */
static ULONG selectedTreeNode(void) {
    struct MUI_NListtree_TreeNode *node = NULL;
    if (agentSessionTree == NULL)
        return 0;
    get(agentSessionTree, MUIA_NListtree_Active, &node);
    /* MUIV_NListtree_Active_Off is 0 */
    if (node == NULL)
        return 0;
    return (ULONG)(IPTR)node->tn_User;
}

static void updateTreeButtons(void) {
    ULONG node = selectedTreeNode();
    if (agentRemoveProjectButton != NULL)
        set(agentRemoveProjectButton, MUIA_Disabled,
            !((node & AGENT_NODE_PROJECT) &&
              (node & AGENT_NODE_INDEX_MASK) > 0));
    if (agentDeleteSessionButton != NULL)
        set(agentDeleteSessionButton, MUIA_Disabled,
            !(node & AGENT_NODE_SESSION));
}

static struct MUI_NListtree_TreeNode *
insertTreeNode(CONST_STRPTR name, ULONG user,
               struct MUI_NListtree_TreeNode *list, ULONG flags) {
    return (struct MUI_NListtree_TreeNode *)DoMethod(
        agentSessionTree, MUIM_NListtree_Insert, (IPTR)name, (IPTR)user,
        (IPTR)list, MUIV_NListtree_Insert_PrevNode_Tail, flags);
}

/* Add the saved sessions of one project under its row, newest first */
static struct MUI_NListtree_TreeNode *
insertSessions(struct MUI_NListtree_TreeNode *list, LONG project,
               LONG currentSession) {
    struct MUI_NListtree_TreeNode *current = NULL;
    ULONG count = agentSessionCount();
    ULONG projectCount = agentProjectCount();
    ULONG i;
    for (i = 0; i < count; i++) {
        CONST_STRPTR path = agentSessionProjectPath(i);
        LONG owner = -1;
        ULONG j;
        STRPTR title;
        struct MUI_NListtree_TreeNode *node;
        for (j = 0; j < projectCount && path[0] != '\0'; j++) {
            if (strcasecmp(agentProjectPath(j), path) == 0)
                owner = (LONG)j;
        }
        /* Sessions of a project that was removed are shown with no project */
        if (owner != project)
            continue;
        title = agentToLocal(agentSessionTitle(i));
        node = insertTreeNode(title != NULL && title[0] != '\0'
                                  ? title
                                  : (STRPTR) "-",
                              AGENT_NODE_SESSION | i, list, 0);
        if (title != NULL)
            FreeVec(title);
        if ((LONG)i == currentSession)
            current = node;
    }
    return current;
}

/* Projects, each with its sessions under it, like folders */
static void refreshSessionTree(void) {
    struct MUI_NListtree_TreeNode *activeNode = NULL;
    struct MUI_NListtree_TreeNode *row;
    struct MUI_NListtree_TreeNode *found;
    ULONG count = agentProjectCount();
    LONG activeProject = agentGetActiveProject();
    LONG currentSession = agentCurrentSession();
    ULONG i;

    if (agentSessionTree == NULL)
        return;
    agentUpdatingSessionTree = TRUE;
    set(agentSessionTree, MUIA_NListtree_Quiet, TRUE);
    DoMethod(agentSessionTree, MUIM_NListtree_Clear, NULL, 0);

    row = insertTreeNode(STRING_AGENT_NO_PROJECT, AGENT_NODE_PROJECT,
                         NULL,
                         TNF_LIST | TNF_OPEN);
    if (row != NULL) {
        found = insertSessions(row, -1, currentSession);
        if (found != NULL)
            activeNode = found;
        if (activeProject < 0 && activeNode == NULL)
            activeNode = row;
    }
    for (i = 0; i < count; i++) {
        row = insertTreeNode(agentProjectName(i), AGENT_NODE_PROJECT | (i + 1),
                             NULL,
                             TNF_LIST | TNF_OPEN);
        if (row == NULL)
            continue;
        found = insertSessions(row, (LONG)i, currentSession);
        if (found != NULL)
            activeNode = found;
        /* A new session that is not saved yet shows as its project */
        if ((LONG)i == activeProject && currentSession < 0)
            activeNode = row;
    }

    set(agentSessionTree, MUIA_NListtree_Quiet, FALSE);
    set(agentSessionTree, MUIA_NListtree_Active,
        activeNode != NULL ? (IPTR)activeNode : MUIV_NListtree_Active_Off);
    agentUpdatingSessionTree = FALSE;
    updateTreeButtons();
}

/* Show the transcript of the current session from the start */
static void showSessionTranscript(void) {
    clearTranscript();
    agentRenderSession(agentOutputReceived);
    if (agentTranscriptLength == 0)
        appendBlock(AGENT_OUTPUT_INFO, STRING_AGENT_WELCOME);
    showTranscript();
}

/* A row was chosen. This runs after the notification has returned, because
 * it rebuilds the tree */
HOOKPROTONHNONP(AgentTreeSelectedDeferredFunc, void) {
    ULONG node = selectedTreeNode();
    ULONG serial = agentSessionSerial();
    if (node == 0 || mainWindowRequestBusy())
        return;
    if (node & AGENT_NODE_SESSION) {
        agentLoadSession(node & AGENT_NODE_INDEX_MASK);
    } else if (node & AGENT_NODE_PROJECT) {
        LONG project = (LONG)(node & AGENT_NODE_INDEX_MASK) - 1;
        /* Choosing a project starts a new session in it */
        if (project != agentGetActiveProject())
            agentSetActiveProject(project);
        else if (agentCurrentSession() >= 0)
            agentNewSession();
    }
    if (agentSessionSerial() != serial)
        showSessionTranscript();
    refreshSessionTree();
}
MakeHook(AgentTreeSelectedDeferredHook, AgentTreeSelectedDeferredFunc);

HOOKPROTONHNONP(AgentTreeSelectedFunc, void) {
    if (agentUpdatingSessionTree || agentSessionTree == NULL)
        return;
    updateTreeButtons();
    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &AgentTreeSelectedDeferredHook);
}
MakeHook(AgentTreeSelectedHook, AgentTreeSelectedFunc);

HOOKPROTONHNONP(AgentDeleteSessionFunc, void) {
    ULONG node = selectedTreeNode();
    ULONG serial = agentSessionSerial();
    if (!(node & AGENT_NODE_SESSION) || mainWindowRequestBusy())
        return;
    agentDeleteSession(node & AGENT_NODE_INDEX_MASK);
    if (agentSessionSerial() != serial)
        showSessionTranscript();
    refreshSessionTree();
}
MakeHook(AgentDeleteSessionHook, AgentDeleteSessionFunc);

HOOKPROTONHNONP(AgentAddProjectFunc, void) {
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq == NULL)
        return;
    if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                       STRING_AGENT_SELECT_PROJECT_DRAWER, ASLFR_DrawersOnly,
                       TRUE, TAG_DONE) &&
        fileReq->fr_Drawer != NULL && strlen(fileReq->fr_Drawer) > 0) {
        ULONG serial = agentSessionSerial();
        agentAddProject(fileReq->fr_Drawer);
        if (agentSessionSerial() != serial)
            showSessionTranscript();
        refreshSessionTree();
    }
    FreeAslRequest(fileReq);
}
MakeHook(AgentAddProjectHook, AgentAddProjectFunc);

HOOKPROTONHNONP(AgentRemoveProjectFunc, void) {
    ULONG node = selectedTreeNode();
    ULONG serial = agentSessionSerial();
    if (!(node & AGENT_NODE_PROJECT) || (node & AGENT_NODE_INDEX_MASK) == 0 ||
        mainWindowRequestBusy())
        return;
    /* Only the entry in the project list goes; the drawer is left alone,
     * and the project's sessions are listed with no project */
    agentRemoveProject((node & AGENT_NODE_INDEX_MASK) - 1);
    if (agentSessionSerial() != serial)
        showSessionTranscript();
    refreshSessionTree();
}
MakeHook(AgentRemoveProjectHook, AgentRemoveProjectFunc);

HOOKPROTONHNONP(AgentApprovalChangedFunc, void) {
    LONG active = 0;
    get(agentApprovalCycle, MUIA_Cycle_Active, &active);
    agentSetApprovalMode((AgentApprovalMode)active);
}
MakeHook(AgentApprovalChangedHook, AgentApprovalChangedFunc);

HOOKPROTONHNONP(AgentEffortChangedFunc, void) {
    LONG active = 0;
    get(agentEffortCycle, MUIA_Cycle_Active, &active);
    agentSetEffort((AgentEffort)active);
}
MakeHook(AgentEffortChangedHook, AgentEffortChangedFunc);

static void saveModelString(void) {
    STRPTR model = NULL;
    if (agentModelString == NULL)
        return;
    get(agentModelString, MUIA_String_Contents, &model);
    if (model == NULL)
        model = (STRPTR) "";
    if (strcmp(model, agentGetModel()) != 0)
        agentSetModel(model);
}

HOOKPROTONHNONP(AgentModelChangedFunc, void) { saveModelString(); }
MakeHook(AgentModelChangedHook, AgentModelChangedFunc);

HOOKPROTONHNONP(AgentNewSessionFunc, void) {
    agentNewSession();
    showSessionTranscript();
    refreshSessionTree();
}
MakeHook(AgentNewSessionHook, AgentNewSessionFunc);

/* Open a text file in the editor window */
static void openAgentEditor(CONST_STRPTR path) {
    STRPTR text;
    if (agentEditorWindow == NULL || agentEditorText == NULL)
        return;
    strncpy(agentEditorPath, path, sizeof(agentEditorPath) - 1);
    agentEditorPath[sizeof(agentEditorPath) - 1] = '\0';
    text = agentReadFile(agentEditorPath, NULL);
    set(agentEditorText, MUIA_TextEditor_Contents, text != NULL ? text : "");
    if (text != NULL)
        FreeVec(text);
    set(agentEditorWindow, MUIA_Window_Title, agentEditorPath);
    set(agentEditorWindow, MUIA_Window_Open, TRUE);
}

HOOKPROTONHNONP(AgentEditInstructionsFunc, void) {
    UBYTE path[AGENT_PATH_LENGTH];
    agentInstructionsPath(path, sizeof(path));
    openAgentEditor(path);
}
MakeHook(AgentEditInstructionsHook, AgentEditInstructionsFunc);

HOOKPROTONHNONP(AgentEditMcpFunc, void) {
    UBYTE path[AGENT_PATH_LENGTH];
    agentMcpConfigPath(path, sizeof(path));
    openAgentEditor(path);
}
MakeHook(AgentEditMcpHook, AgentEditMcpFunc);

HOOKPROTONHNONP(AgentEditorSaveFunc, void) {
    STRPTR text;
    if (agentEditorText == NULL || agentEditorPath[0] == '\0')
        return;
    text = (STRPTR)DoMethod(agentEditorText, MUIM_TextEditor_ExportText);
    if (text != NULL) {
        if (!agentWriteFile(agentEditorPath, text, strlen(text)))
            displayError(STRING_AGENT_ERROR_SAVING_FILE);
        FreeVec(text);
    }
    set(agentEditorWindow, MUIA_Window_Open, FALSE);
}
MakeHook(AgentEditorSaveHook, AgentEditorSaveFunc);

HOOKPROTONHNONP(AgentImportSkillFunc, void) {
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq == NULL)
        return;
    if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                       STRING_AGENT_SELECT_SKILL, ASLFR_RejectIcons, TRUE,
                       TAG_DONE)) {
        UBYTE source[AGENT_PATH_LENGTH];
        UBYTE name[108];
        UBYTE message[256];
        strncpy(source, fileReq->fr_Drawer != NULL ? fileReq->fr_Drawer : "",
                sizeof(source) - 1);
        source[sizeof(source) - 1] = '\0';
        /* Choosing SKILL.md itself imports the drawer it is in, so the
         * files the skill refers to come along */
        if (fileReq->fr_File != NULL && strlen(fileReq->fr_File) > 0 &&
            strcasecmp(fileReq->fr_File, "SKILL.md") != 0)
            AddPart(source, fileReq->fr_File, sizeof(source));
        if (agentImportSkill(source, name, sizeof(name))) {
            snprintf(message, sizeof(message), STRING_AGENT_SKILL_IMPORTED,
                     name);
            appendBlock(AGENT_OUTPUT_INFO, message);
            showTranscript();
        } else {
            displayError(STRING_AGENT_SKILL_IMPORT_FAILED);
        }
    }
    FreeAslRequest(fileReq);
}
MakeHook(AgentImportSkillHook, AgentImportSkillFunc);

HOOKPROTONHNONP(AgentSendDeferredFunc, void) {
    struct ChatRequestSettings chatSettings;
    STRPTR text;
    STRPTR prompt;
    BOOL freeText;
    ULONG serial;
    ULONG turnStart;

    if (mainWindowRequestBusy() || agentInputEditor == NULL)
        return;
    if (isAROS) {
        get(agentInputEditor, MUIA_String_Contents, &text);
        freeText = FALSE;
    } else {
        text = (STRPTR)DoMethod(agentInputEditor, MUIM_TextEditor_ExportText);
        freeText = text != NULL;
    }
    if (text == NULL)
        text = (STRPTR) "";
    while (strlen(text) > 0 && text[strlen(text) - 1] == '\n')
        text[strlen(text) - 1] = '\0';
    if (strlen(text) == 0) {
        if (freeText)
            FreeVec(text);
        return;
    }

    /* Slash commands work without a key; a prompt for the model does not */
    configGetActiveChatRequestSettings(&chatSettings);
    if (text[0] != '/' &&
        chatSettings.authorizationType != AUTHORIZATION_TYPE_NONE &&
        (chatSettings.apiKey == NULL || strlen(chatSettings.apiKey) == 0)) {
        displayError(STRING_ERROR_NO_API_KEY);
        if (freeText)
            FreeVec(text);
        return;
    }

    saveModelString();
    serial = agentSessionSerial();
    turnStart = agentTranscriptLength;
    {
        /* What the user typed, without anything the text object would read
         * as a format code */
        STRPTR shown = agentDuplicate(text);
        STRPTR p;
        for (p = shown; p != NULL && *p != '\0'; p++) {
            if (*p == '\33')
                *p = ' ';
        }
        appendBlock(AGENT_OUTPUT_USER, shown != NULL ? shown : text);
        if (shown != NULL)
            FreeVec(shown);
    }
    showTranscript();
    prompt = agentToUtf8(text);
    if (freeText)
        FreeVec(text);
    if (isAROS)
        set(agentInputEditor, MUIA_String_Contents, "");
    else
        DoMethod(agentInputEditor, MUIM_TextEditor_ClearText);

    mainWindowBeginRequest();
    if (prompt != NULL) {
        agentRun(prompt, agentOutputReceived);
        FreeVec(prompt);
    }
    mainWindowFinishRequest(FALSE);
    /* A command such as /new, /resume or /project switched to another
     * session: show that session, followed by this turn */
    if (agentSessionSerial() != serial) {
        STRPTR turn = agentDuplicate(agentTranscript != NULL &&
                                             turnStart <= agentTranscriptLength
                                         ? agentTranscript + turnStart
                                         : (STRPTR) "");
        clearTranscript();
        agentRenderSession(agentOutputReceived);
        appendTranscript(turn);
        showTranscript();
        if (turn != NULL)
            FreeVec(turn);
    }
    /* A command may have changed what the controls show */
    refreshSessionTree();
    set(agentApprovalCycle, MUIA_Cycle_Active, (LONG)agentGetApprovalMode());
    set(agentEffortCycle, MUIA_Cycle_Active, (LONG)agentGetEffort());
    set(agentModelString, MUIA_String_Contents, agentGetModel());
    if (mainWindowObject != NULL)
        set(mainWindowObject, MUIA_Window_ActiveObject, agentInputEditor);
}
MakeHook(AgentSendDeferredHook, AgentSendDeferredFunc);

HOOKPROTONHNONP(AgentSendButtonClickedFunc, void) {
    if (mainWindowAbortIfRequestBusy())
        return;
    DoMethod(app, MUIM_Application_PushMethod, app, 2, MUIM_CallHook,
             &AgentSendDeferredHook);
}
MakeHook(AgentSendButtonClickedHook, AgentSendButtonClickedFunc);

/* Iconifying closes every window of the program, so the agent sees and
 * clicks what is underneath, and the user can still bring it back */
static void hideApplication(void) {
    if (app != NULL)
        set(app, MUIA_Application_Iconified, TRUE);
}

static void showApplication(void) {
    if (app != NULL)
        set(app, MUIA_Application_Iconified, FALSE);
}

Object *agentPagePromptEditor(void) { return agentInputEditor; }

Object *agentPageTranscriptView(void) { return agentOutput; }

void agentPageSetBusy(BOOL busy) {
    if (agentControlsGroup != NULL)
        set(agentControlsGroup, MUIA_Disabled, busy);
    if (agentInputEditor != NULL)
        set(agentInputEditor, MUIA_Disabled, busy);
    if (agentSendButton != NULL)
        set(agentSendButton, MUIA_Disabled, FALSE);
    setAgentSendButtonLabel(busy);
}

static void createAgentEditorWindow(void) {
    Object *slider;
    if (agentEditorWindow != NULL || isAROS)
        return;
    slider = ScrollbarObject, End;
    // clang-format off
    agentEditorWindow = WindowObject,
        MUIA_Window_Title, STRING_AGENT_INSTRUCTIONS,
        MUIA_Window_ID, MAKE_ID('A', 'G', 'E', 'D'),
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_SizeGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_Width, MUIV_Window_Width_Screen(60),
        MUIA_Window_Height, MUIV_Window_Height_Screen(60),
        WindowContents, VGroup,
            Child, HGroup,
                MUIA_Group_Spacing, 0,
                Child, agentEditorText = BestTextEditorObject,
                    MUIA_TextEditor_FixedFont, TRUE,
                    MUIA_TextEditor_Slider, slider,
                    MUIA_TextEditor_WrapMode, MUIV_TextEditor_WrapMode_SoftWrap,
                    MUIA_BestTextEditor_Markdown, TRUE,
                    MUIA_CycleChain, TRUE,
                End,
                Child, slider,
            End,
            Child, HGroup,
                Child, agentEditorSaveButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_SAVE,
                    MUIA_CycleChain, TRUE,
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                TAG_DONE),
                Child, agentEditorCancelButton = MUI_MakeObject(MUIO_Button, STRING_CANCEL,
                    MUIA_CycleChain, TRUE,
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                TAG_DONE),
            End,
        End,
    End;
    // clang-format on
    if (agentEditorWindow == NULL)
        return;
    DoMethod(app, OM_ADDMEMBER, agentEditorWindow);
    DoMethod(agentEditorSaveButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentEditorSaveButton, 2, MUIM_CallHook, &AgentEditorSaveHook);
    DoMethod(agentEditorCancelButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentEditorWindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);
    DoMethod(agentEditorWindow, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             agentEditorWindow, 3, MUIM_Set, MUIA_Window_Open, FALSE);
}

Object *createAgentPage(void) {
    Object *page;

    agentApprovalLabels[AGENT_APPROVAL_ASK] = STRING_AGENT_APPROVAL_ASK;
    agentApprovalLabels[AGENT_APPROVAL_READ_ONLY] =
        STRING_AGENT_APPROVAL_READ_ONLY;
    agentApprovalLabels[AGENT_APPROVAL_ACCEPT_EDITS] =
        STRING_AGENT_APPROVAL_ACCEPT_EDITS;
    agentApprovalLabels[AGENT_APPROVAL_FULL_AUTO] =
        STRING_AGENT_APPROVAL_FULL_AUTO;
    agentApprovalLabels[AGENT_APPROVAL_COUNT] = NULL;
    agentEffortLabels[AGENT_EFFORT_DEFAULT] = STRING_AGENT_EFFORT_DEFAULT;
    agentEffortLabels[AGENT_EFFORT_LOW] = STRING_AGENT_EFFORT_LOW;
    agentEffortLabels[AGENT_EFFORT_MEDIUM] = STRING_AGENT_EFFORT_MEDIUM;
    agentEffortLabels[AGENT_EFFORT_HIGH] = STRING_AGENT_EFFORT_HIGH;
    agentEffortLabels[AGENT_EFFORT_COUNT] = NULL;

    if (agentTranscript == NULL) {
        agentTranscript = AllocVec(AGENT_TRANSCRIPT_INITIAL_CAPACITY,
                                   MEMF_ANY | MEMF_CLEAR);
        agentTranscriptCapacity =
            agentTranscript != NULL ? AGENT_TRANSCRIPT_INITIAL_CAPACITY : 0;
        agentTranscriptLength = 0;
    }

    agentInputEditor =
        createPromptInputEditor(&AgentSendButtonClickedHook, 0, 0);
    agentOutputScroller = createTranscriptView(&agentOutput);
    /* The transcript outlives the window, which is rebuilt when the fonts
     * change */
    if (agentOutput != NULL && agentTranscript != NULL)
        set(agentOutput, MUIA_TextEditor_Contents, agentTranscript);

    // clang-format off
    page = HGroup,
        Child, agentControlsGroup = VGroup, MUIA_Weight, 30,
            Child, agentAddProjectButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_ADD_PROJECT,
                MUIA_CycleChain, TRUE,
                MUIA_InputMode, MUIV_InputMode_RelVerify,
            TAG_DONE),
            Child, agentRemoveProjectButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_REMOVE_PROJECT,
                MUIA_CycleChain, TRUE,
                MUIA_InputMode, MUIV_InputMode_RelVerify,
            TAG_DONE),
            Child, NListviewObject,
                MUIA_CycleChain, 1,
                MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_Auto,
                MUIA_NListview_NList, agentSessionTree = NListtreeObject,
                    MUIA_ContextMenu, NULL,
                    MUIA_NListtree_MultiSelect, MUIV_NListtree_MultiSelect_None,
                    MUIA_NListtree_DoubleClick, MUIV_NListtree_DoubleClick_Tree,
                    MUIA_NListtree_EmptyNodes, FALSE,
                    MUIA_NListtree_AutoVisible, MUIV_NListtree_AutoVisible_Expand,
                    MUIA_NListtree_Title, FALSE,
                End,
            End,
            Child, HGroup,
                Child, agentNewSessionButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_NEW_SESSION,
                    MUIA_CycleChain, TRUE,
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                TAG_DONE),
                Child, agentDeleteSessionButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_DELETE_SESSION,
                    MUIA_CycleChain, TRUE,
                    MUIA_InputMode, MUIV_InputMode_RelVerify,
                TAG_DONE),
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_AGENT_APPROVAL,
                Child, agentApprovalCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, agentApprovalLabels,
                    MUIA_Cycle_Active, (LONG)agentGetApprovalMode(),
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_AGENT_EFFORT,
                Child, agentEffortCycle = CycleObject,
                    MUIA_CycleChain, TRUE,
                    MUIA_Cycle_Entries, agentEffortLabels,
                    MUIA_Cycle_Active, (LONG)agentGetEffort(),
                End,
            End,
            Child, VGroup,
                MUIA_Frame, MUIV_Frame_Group,
                MUIA_FrameTitle, STRING_AGENT_MODEL,
                Child, agentModelString = StringObject,
                    MUIA_Frame, MUIV_Frame_String,
                    MUIA_CycleChain, TRUE,
                    MUIA_String_Contents, agentGetModel(),
                    MUIA_String_MaxLen, 128,
                    MUIA_ShortHelp, STRING_AGENT_MODEL_HELP,
                End,
            End,
            Child, agentInstructionsButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_INSTRUCTIONS,
                MUIA_CycleChain, TRUE,
                MUIA_InputMode, MUIV_InputMode_RelVerify,
            TAG_DONE),
            Child, agentMcpButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_MCP_SERVERS,
                MUIA_CycleChain, TRUE,
                MUIA_InputMode, MUIV_InputMode_RelVerify,
            TAG_DONE),
            Child, agentImportSkillButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_IMPORT_SKILL,
                MUIA_CycleChain, TRUE,
                MUIA_InputMode, MUIV_InputMode_RelVerify,
            TAG_DONE),
        End,
        Child, VGroup,
            Child, HGroup, MUIA_VertWeight, 60,
                Child, agentOutputScroller,
            End,
            Child, HGroup, MUIA_VertWeight, 20,
                Child, agentInputEditor,
                Child, VGroup, MUIA_HorizWeight, 10,
                    Child, agentSendButton = MUI_MakeObject(MUIO_Button, STRING_SEND,
                        MUIA_CycleChain, TRUE,
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                    TAG_DONE),
                End,
            End,
        End,
    End;
    // clang-format on
    return page;
}

void addAgentPageActions(void) {
    if (agentSessionTree == NULL)
        return;
    createAgentEditorWindow();
    agentSetRequesterWindow(mainWindowObject);
    agentSetDesktopHooks(hideApplication, showApplication);

    DoMethod(agentAddProjectButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentAddProjectButton, 2, MUIM_CallHook, &AgentAddProjectHook);
    DoMethod(agentRemoveProjectButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentRemoveProjectButton, 2, MUIM_CallHook,
             &AgentRemoveProjectHook);
    DoMethod(agentSessionTree, MUIM_Notify, MUIA_NListtree_Active,
             MUIV_EveryTime, agentSessionTree, 2, MUIM_CallHook,
             &AgentTreeSelectedHook);
    DoMethod(agentDeleteSessionButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentDeleteSessionButton, 2, MUIM_CallHook,
             &AgentDeleteSessionHook);
    DoMethod(agentApprovalCycle, MUIM_Notify, MUIA_Cycle_Active,
             MUIV_EveryTime, agentApprovalCycle, 2, MUIM_CallHook,
             &AgentApprovalChangedHook);
    DoMethod(agentEffortCycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             agentEffortCycle, 2, MUIM_CallHook, &AgentEffortChangedHook);
    DoMethod(agentModelString, MUIM_Notify, MUIA_String_Acknowledge,
             MUIV_EveryTime, agentModelString, 2, MUIM_CallHook,
             &AgentModelChangedHook);
    DoMethod(agentInstructionsButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentInstructionsButton, 2, MUIM_CallHook,
             &AgentEditInstructionsHook);
    DoMethod(agentMcpButton, MUIM_Notify, MUIA_Pressed, FALSE, agentMcpButton,
             2, MUIM_CallHook, &AgentEditMcpHook);
    DoMethod(agentImportSkillButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentImportSkillButton, 2, MUIM_CallHook, &AgentImportSkillHook);
    DoMethod(agentNewSessionButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentNewSessionButton, 2, MUIM_CallHook, &AgentNewSessionHook);
    DoMethod(agentSendButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentSendButton, 2, MUIM_CallHook, &AgentSendButtonClickedHook);
    if (isAROS) {
        set(agentInstructionsButton, MUIA_Disabled, TRUE);
        set(agentMcpButton, MUIA_Disabled, TRUE);
    }
}

void agentPageWindowOpened(void) {
    if (agentSessionTree == NULL)
        return;
    refreshSessionTree();
    agentPageColorButtons();
    setAgentSendButtonLabel(FALSE);
    if (agentTranscriptLength == 0) {
        agentRenderSession(agentOutputReceived);
        if (agentTranscriptLength == 0) {
            appendTranscript(STRING_AGENT_WELCOME);
            appendTranscript("\n");
            showTranscript();
        }
    } else {
        showTranscript();
    }
}

void freeAgentPage(void) {
    if (agentTranscript != NULL)
        FreeVec(agentTranscript);
    agentTranscript = NULL;
    agentTranscriptCapacity = 0;
    agentTranscriptLength = 0;
}
#endif /* DAEMON */
