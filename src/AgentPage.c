#ifndef DAEMON
#include <libraries/asl.h>
#include <libraries/mui.h>
#include <mui/NFloattext_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <SDI_hook.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "agent_internal.h"
#include "AgentPage.h"
#include "AmigaGPTConfig.h"
#include "gui.h"
#include "MainWindow.h"

#define AGENT_TRANSCRIPT_INITIAL_CAPACITY 16384

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d)                                                    \
    ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

static Object *agentProjectList = NULL;
static Object *agentAddProjectButton = NULL;
static Object *agentRemoveProjectButton = NULL;
static Object *agentApprovalCycle = NULL;
static Object *agentEffortCycle = NULL;
static Object *agentModelString = NULL;
static Object *agentInstructionsButton = NULL;
static Object *agentMcpButton = NULL;
static Object *agentImportSkillButton = NULL;
static Object *agentNewSessionButton = NULL;
static Object *agentControlsGroup = NULL;
static Object *agentOutput = NULL;
static Object *agentOutputListView = NULL;
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
static BOOL agentUpdatingProjectList = FALSE;
static STRPTR agentApprovalLabels[AGENT_APPROVAL_COUNT + 1] = {NULL};
static STRPTR agentEffortLabels[AGENT_EFFORT_COUNT + 1] = {NULL};

static void showTranscript(void) {
    if (agentOutput == NULL || agentTranscript == NULL)
        return;
    set(agentOutput, MUIA_NFloattext_Text, agentTranscript);
    set(agentOutputListView, MUIA_NList_First, MUIV_NList_First_Bottom);
}

/* Add local character set text to the transcript */
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
            /* The list still points at the old text, so empty it first */
            if (agentOutput != NULL)
                set(agentOutput, MUIA_NFloattext_Text, "");
            FreeVec(agentTranscript);
        }
        agentTranscript = bigger;
        agentTranscriptCapacity = capacity;
    }
    CopyMem((APTR)text, agentTranscript + agentTranscriptLength, length);
    agentTranscriptLength += length;
    agentTranscript[agentTranscriptLength] = '\0';
}

static void clearTranscript(void) {
    agentTranscriptLength = 0;
    if (agentTranscript != NULL)
        agentTranscript[0] = '\0';
    if (agentOutput != NULL)
        set(agentOutput, MUIA_NFloattext_Text, "");
}

/* Transcript text arrives from the agent in UTF-8 */
static void agentOutputReceived(CONST_STRPTR text) {
    STRPTR local = agentToLocal(text);
    STRPTR p;
    if (local == NULL)
        return;
    /* The text object reads ESC as the start of a formatting code */
    for (p = local; *p != '\0'; p++) {
        if (*p == '\33')
            *p = ' ';
    }
    appendTranscript(local);
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

static void refreshProjectList(void) {
    ULONG count = agentProjectCount();
    ULONG i;
    if (agentProjectList == NULL)
        return;
    agentUpdatingProjectList = TRUE;
    set(agentProjectList, MUIA_NList_Quiet, TRUE);
    DoMethod(agentProjectList, MUIM_NList_Clear);
    DoMethod(agentProjectList, MUIM_NList_InsertSingle,
             (ULONG)STRING_AGENT_NO_PROJECT, MUIV_NList_Insert_Bottom);
    for (i = 0; i < count; i++)
        DoMethod(agentProjectList, MUIM_NList_InsertSingle,
                 (ULONG)agentProjectName(i), MUIV_NList_Insert_Bottom);
    set(agentProjectList, MUIA_NList_Quiet, FALSE);
    /* Row 0 is "no project", so a project sits one row below its index */
    set(agentProjectList, MUIA_NList_Active, agentGetActiveProject() + 1);
    agentUpdatingProjectList = FALSE;
    if (agentRemoveProjectButton != NULL)
        set(agentRemoveProjectButton, MUIA_Disabled,
            agentGetActiveProject() < 0);
}

HOOKPROTONHNONP(AgentProjectSelectedFunc, void) {
    LONG active = 0;
    if (agentUpdatingProjectList || agentProjectList == NULL)
        return;
    get(agentProjectList, MUIA_NList_Active, &active);
    if (active < 0)
        return;
    agentSetActiveProject(active - 1);
    if (agentRemoveProjectButton != NULL)
        set(agentRemoveProjectButton, MUIA_Disabled, active < 1);
}
MakeHook(AgentProjectSelectedHook, AgentProjectSelectedFunc);

HOOKPROTONHNONP(AgentAddProjectFunc, void) {
    struct FileRequester *fileReq =
        AllocAslRequestTags(ASL_FileRequest, TAG_END);
    if (fileReq == NULL)
        return;
    if (AslRequestTags(fileReq, ASLFR_Window, mainWindow, ASLFR_TitleText,
                       STRING_AGENT_SELECT_PROJECT_DRAWER, ASLFR_DrawersOnly,
                       TRUE, TAG_DONE) &&
        fileReq->fr_Drawer != NULL && strlen(fileReq->fr_Drawer) > 0) {
        agentAddProject(fileReq->fr_Drawer);
        refreshProjectList();
    }
    FreeAslRequest(fileReq);
}
MakeHook(AgentAddProjectHook, AgentAddProjectFunc);

HOOKPROTONHNONP(AgentRemoveProjectFunc, void) {
    LONG active = agentGetActiveProject();
    if (active < 0)
        return;
    /* Only the entry in the project list goes; the drawer is left alone */
    agentRemoveProject((ULONG)active);
    refreshProjectList();
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
    clearTranscript();
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
            appendTranscript(message);
            appendTranscript("\n");
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
    /* A new session starts with an empty transcript */
    if (strcasecmp(text, "/new") == 0 || strcasecmp(text, "/clear") == 0)
        clearTranscript();
    appendTranscript("\n>>> ");
    appendTranscript(text);
    appendTranscript("\n\n");
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
    /* A command may have changed what the controls show */
    refreshProjectList();
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

Object *agentPagePromptEditor(void) { return agentInputEditor; }

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
                Child, agentEditorText = TextEditorObject,
                    MUIA_TextEditor_FixedFont, TRUE,
                    MUIA_TextEditor_Slider, slider,
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
                MUIA_NListview_NList, agentProjectList = NListObject,
                    MUIA_ContextMenu, NULL,
                    MUIA_NList_MultiSelect, MUIV_NList_MultiSelect_None,
                    MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                    MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                    MUIA_NList_Format, "BAR MINW=100 MAXW=200",
                    MUIA_NList_AutoVisible, TRUE,
                    MUIA_NList_Title, FALSE,
                End,
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
            Child, agentNewSessionButton = MUI_MakeObject(MUIO_Button, STRING_AGENT_NEW_SESSION,
                MUIA_CycleChain, TRUE,
                MUIA_InputMode, MUIV_InputMode_RelVerify,
            TAG_DONE),
        End,
        Child, VGroup,
            Child, HGroup, MUIA_VertWeight, 60,
                Child, agentOutputListView = NListviewObject,
                    MUIA_NListview_Horiz_ScrollBar, MUIV_NListview_HSB_None,
                    MUIA_NListview_Vert_ScrollBar, MUIV_NListview_VSB_Auto,
                    MUIA_NListview_NList, agentOutput = NFloattextObject,
                        MUIA_Font, configGetFixedWidthFonts() ? MUIV_NList_Font_Fixed : MUIV_NList_Font,
                        MUIA_Frame, MUIV_Frame_Text,
                        MUIA_ContextMenu, NULL,
                        MUIA_NFloattext_Text, agentTranscript != NULL ? agentTranscript : (STRPTR)"",
                    End,
                End,
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
    if (agentProjectList == NULL)
        return;
    createAgentEditorWindow();
    agentSetRequesterWindow(mainWindowObject);

    DoMethod(agentAddProjectButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentAddProjectButton, 2, MUIM_CallHook, &AgentAddProjectHook);
    DoMethod(agentRemoveProjectButton, MUIM_Notify, MUIA_Pressed, FALSE,
             agentRemoveProjectButton, 2, MUIM_CallHook,
             &AgentRemoveProjectHook);
    DoMethod(agentProjectList, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             agentProjectList, 2, MUIM_CallHook, &AgentProjectSelectedHook);
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
    if (agentProjectList == NULL)
        return;
    refreshProjectList();
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
