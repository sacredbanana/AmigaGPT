#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <json-c/json.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "agent_internal.h"
#include "AmigaGPTConfig.h"
#include "ARexx.h"
#include "gui.h"
#include "openai.h"
#include "shellpath.h"

#define AGENT_COMMAND_FILE_FORMAT "T:amigagpt_agent_%lu.%s"
#define AGENT_DEFAULT_COMMAND_TIMEOUT 120
#define AGENT_COMMAND_POLL_TICKS 5
#define AGENT_MAX_FILE_BYTES (512UL * 1024UL)
#define AGENT_MAX_SEARCH_FILE_BYTES (256UL * 1024UL)
#define AGENT_MAX_INSTRUCTION_BYTES 16000
#define AGENT_MAX_HISTORY_BYTES (200UL * 1024UL)
#define AGENT_MAX_SEARCH_MATCHES 100
#define AGENT_MAX_SEARCH_DEPTH 8
#define AGENT_MAX_LIST_ENTRIES 500
#define AGENT_DEFAULT_READ_LINES 400

/**
 * What a tool can do to the system, which decides whether it needs approval
 **/
typedef enum {
    AGENT_TOOL_SAFE = 0,
    AGENT_TOOL_EDIT,
    AGENT_TOOL_EXECUTE
} AgentToolClass;

struct AgentToolDefinition {
    CONST_STRPTR name;
    AgentToolClass toolClass;
    CONST_STRPTR description;
    CONST_STRPTR schema;
};

static const struct AgentToolDefinition BUILT_IN_TOOLS[] = {
    {"shell", AGENT_TOOL_EXECUTE,
     "Run an AmigaDOS command in a Shell and return its exit code and output. "
     "The command runs in the project drawer when a project is active. Use "
     "AmigaDOS commands (list, type, copy, delete, makedir, rename), not "
     "Unix ones.",
     "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\","
     "\"description\":\"The AmigaDOS command line to run\"}},"
     "\"required\":[\"command\"]}"},
    {"read_file", AGENT_TOOL_SAFE,
     "Read a text file. Paths without a device name are relative to the "
     "project drawer. Long files are returned in parts: use offset and limit "
     "to read further.",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
     "\"description\":\"The file to read, e.g. Work:Projects/main.c\"},"
     "\"offset\":{\"type\":\"integer\",\"description\":\"First line to "
     "return, starting at 1\"},\"limit\":{\"type\":\"integer\","
     "\"description\":\"Number of lines to return\"}},"
     "\"required\":[\"path\"]}"},
    {"write_file", AGENT_TOOL_EDIT,
     "Create a file or replace its whole contents. Missing drawers are "
     "created. Prefer edit_file for changing part of an existing file.",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
     "\"description\":\"The file to write\"},\"content\":{\"type\":\"string\","
     "\"description\":\"The complete new contents\"}},"
     "\"required\":[\"path\",\"content\"]}"},
    {"edit_file", AGENT_TOOL_EDIT,
     "Replace an exact piece of text in a file. old_string must match the "
     "file exactly and must be unique unless replace_all is true.",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
     "\"description\":\"The file to edit\"},\"old_string\":{\"type\":"
     "\"string\",\"description\":\"The exact text to replace\"},"
     "\"new_string\":{\"type\":\"string\",\"description\":\"The text to put "
     "in its place\"},\"replace_all\":{\"type\":\"boolean\","
     "\"description\":\"Replace every occurrence\"}},"
     "\"required\":[\"path\",\"old_string\",\"new_string\"]}"},
    {"list_directory", AGENT_TOOL_SAFE,
     "List the files and drawers in a drawer. Drawers end in a slash.",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
     "\"description\":\"The drawer to list. Leave empty for the project "
     "drawer\"}},\"required\":[]}"},
    {"search_files", AGENT_TOOL_SAFE,
     "Search the text files below a drawer for a piece of text, ignoring "
     "case. Returns matching lines as path:line: text.",
     "{\"type\":\"object\",\"properties\":{\"pattern\":{\"type\":\"string\","
     "\"description\":\"The text to look for\"},\"path\":{\"type\":\"string\","
     "\"description\":\"The drawer to search. Leave empty for the project "
     "drawer\"}},\"required\":[\"pattern\"]}"},
    {"identify_file", AGENT_TOOL_SAFE,
     "Report the MIME type of a file from its contents.",
     "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
     "\"description\":\"The file to identify\"}},\"required\":[\"path\"]}"},
    {"fetch_url", AGENT_TOOL_EXECUTE,
     "Fetch a web address and return what it serves as text, for reading a "
     "page or calling a web API.",
     "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\","
     "\"description\":\"An http:// or https:// address\"}},"
     "\"required\":[\"url\"]}"},
    {"download_file", AGENT_TOOL_EXECUTE,
     "Download a file from a web address and save it, for pictures, "
     "archives and other files that are not text.",
     "{\"type\":\"object\",\"properties\":{\"url\":{\"type\":\"string\","
     "\"description\":\"The http:// or https:// address of the file\"},\"path\":{"
     "\"type\":\"string\",\"description\":\"Where to save it\"}},"
     "\"required\":[\"url\",\"path\"]}"},
    {"generate_image", AGENT_TOOL_EXECUTE,
     "Generate a picture from a description with the user's image provider "
     "and save it as a file.",
     "{\"type\":\"object\",\"properties\":{\"prompt\":{\"type\":\"string\","
     "\"description\":\"What the picture should show\"},\"path\":{"
     "\"type\":\"string\",\"description\":\"Where to save it, ending in "
     ".png\"}},\"required\":[\"prompt\",\"path\"]}"},
    {"list_arexx_ports", AGENT_TOOL_SAFE,
     "List the public message ports on this machine. Programs that can be "
     "scripted with ARexx appear here by their port name.",
     "{\"type\":\"object\",\"properties\":{},\"required\":[]}"},
    {"arexx", AGENT_TOOL_EXECUTE,
     "Send one ARexx command to the port of a running program and return RC "
     "and RESULT.",
     "{\"type\":\"object\",\"properties\":{\"port\":{\"type\":\"string\","
     "\"description\":\"The ARexx port name, e.g. MULTIVIEW.1\"},"
     "\"command\":{\"type\":\"string\",\"description\":\"The command to "
     "send\"}},\"required\":[\"port\",\"command\"]}"},
    {"load_skill", AGENT_TOOL_SAFE,
     "Load the instructions of one of the available skills. Do this before "
     "starting a task a skill covers, then follow what it says.",
     "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\","
     "\"description\":\"The name of the skill\"}},\"required\":[\"name\"]}"},
    {"run_subagent", AGENT_TOOL_SAFE,
     "Hand a self-contained task to a subagent, which works on it with its "
     "own fresh context and returns only its final report. The subagent "
     "knows nothing about this conversation, so the prompt must say "
     "everything it needs.",
     "{\"type\":\"object\",\"properties\":{\"agent\":{\"type\":\"string\","
     "\"description\":\"The name of the subagent, or general for a general "
     "purpose one\"},\"prompt\":{\"type\":\"string\",\"description\":\"The "
     "complete task\"}},\"required\":[\"agent\",\"prompt\"]}"},
    {NULL, AGENT_TOOL_SAFE, NULL, NULL}};

static struct json_object *settings = NULL;
static struct json_object *history = NULL;
static struct json_object *skills = NULL;
static struct json_object *commands = NULL;
static struct json_object *subagents = NULL;
static APTR requesterWindow = NULL;
static UBYTE effectiveModelBuffer[256];
static ULONG commandCounter = 0;
static BOOL agentRunning = FALSE;

/* ------------------------------------------------------------------------
 * Strings and files
 * ---------------------------------------------------------------------- */

void agentBufferAppendLength(struct AgentBuffer *buffer, CONST_STRPTR text,
                             ULONG length) {
    if (buffer == NULL || text == NULL || length == 0)
        return;
    if (buffer->length + length + 1 > buffer->capacity) {
        ULONG capacity = buffer->capacity > 0 ? buffer->capacity : 1024;
        STRPTR bigger;
        while (buffer->length + length + 1 > capacity)
            capacity *= 2;
        bigger = AllocVec(capacity, MEMF_ANY | MEMF_CLEAR);
        if (bigger == NULL)
            return;
        if (buffer->data != NULL) {
            CopyMem(buffer->data, bigger, buffer->length);
            FreeVec(buffer->data);
        }
        buffer->data = bigger;
        buffer->capacity = capacity;
    }
    CopyMem((APTR)text, buffer->data + buffer->length, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

void agentBufferAppend(struct AgentBuffer *buffer, CONST_STRPTR text) {
    if (text != NULL)
        agentBufferAppendLength(buffer, text, strlen(text));
}

void agentBufferFree(struct AgentBuffer *buffer) {
    if (buffer != NULL && buffer->data != NULL)
        FreeVec(buffer->data);
    if (buffer != NULL) {
        buffer->data = NULL;
        buffer->length = 0;
        buffer->capacity = 0;
    }
}

/* Hand over the text of a buffer, which is never NULL */
static STRPTR bufferTake(struct AgentBuffer *buffer) {
    STRPTR text = buffer->data;
    if (text == NULL)
        text = agentDuplicate("");
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
    return text;
}

STRPTR agentDuplicate(CONST_STRPTR text) {
    ULONG length;
    STRPTR copy;
    if (text == NULL)
        text = "";
    length = strlen(text);
    copy = AllocVec(length + 1, MEMF_ANY | MEMF_CLEAR);
    if (copy != NULL)
        CopyMem((APTR)text, copy, length);
    return copy;
}

STRPTR agentToLocal(CONST_STRPTR utf8) {
    STRPTR converted;
    STRPTR copy;
    if (utf8 == NULL)
        return NULL;
    if (utf8[0] == '\0')
        return agentDuplicate("");
    converted =
        CodesetsUTF8ToStr(CSA_DestCodeset, (Tag)systemCodeset, CSA_Source,
                          (Tag)utf8, CSA_MapForeignChars, TRUE, TAG_DONE);
    if (converted == NULL)
        return agentDuplicate(utf8);
    copy = agentDuplicate(converted);
    CodesetsFreeA(converted, NULL);
    return copy;
}

STRPTR agentToUtf8(CONST_STRPTR local) {
    UTF8 *converted;
    STRPTR copy;
    if (local == NULL)
        return NULL;
    if (local[0] == '\0')
        return agentDuplicate("");
    converted =
        CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset, CSA_Source,
                           (Tag)local, CSA_MapForeignChars, TRUE, TAG_DONE);
    if (converted == NULL)
        return agentDuplicate(local);
    copy = agentDuplicate((CONST_STRPTR)converted);
    CodesetsFreeA(converted, NULL);
    return copy;
}

static BOOL textIsUtf8(CONST_STRPTR text) {
    const UBYTE *p = (const UBYTE *)text;
    while (*p != '\0') {
        ULONG extra;
        if (*p < 0x80) {
            p++;
            continue;
        }
        if (*p >= 0xC2 && *p <= 0xDF)
            extra = 1;
        else if (*p >= 0xE0 && *p <= 0xEF)
            extra = 2;
        else if (*p >= 0xF0 && *p <= 0xF4)
            extra = 3;
        else
            return FALSE;
        p++;
        while (extra-- > 0) {
            if (*p < 0x80 || *p > 0xBF)
                return FALSE;
            p++;
        }
    }
    return TRUE;
}

STRPTR agentReadFile(CONST_STRPTR path, LONG *sizeOut) {
    BPTR file;
    LONG size;
    STRPTR data;

    if (sizeOut != NULL)
        *sizeOut = 0;
    if (path == NULL || path[0] == '\0')
        return NULL;
    file = Open(path, MODE_OLDFILE);
    if (file == 0)
        return NULL;
#if defined(__AMIGAOS4__)
    size = (LONG)GetFileSize(file);
#else
    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_BEGINNING);
#endif
    if (size < 0) {
        Close(file);
        return NULL;
    }
    data = AllocVec(size + 1, MEMF_ANY | MEMF_CLEAR);
    if (data == NULL) {
        Close(file);
        return NULL;
    }
    if (size > 0 && Read(file, data, size) != size) {
        Close(file);
        FreeVec(data);
        return NULL;
    }
    Close(file);
    data[size] = '\0';
    if (sizeOut != NULL)
        *sizeOut = size;
    return data;
}

BOOL agentWriteFile(CONST_STRPTR path, CONST_STRPTR data, ULONG length) {
    BPTR file;
    BOOL ok = TRUE;
    if (path == NULL || path[0] == '\0')
        return FALSE;
    file = Open(path, MODE_NEWFILE);
    if (file == 0)
        return FALSE;
    if (length > 0 && Write(file, (APTR)data, length) != (LONG)length)
        ok = FALSE;
    Close(file);
    return ok;
}

/* Read a text file as UTF-8, whichever of UTF-8 or the local character set
 * it was saved in */
static STRPTR readTextFileUtf8(CONST_STRPTR path) {
    LONG size = 0;
    STRPTR raw = agentReadFile(path, &size);
    STRPTR converted;
    if (raw == NULL)
        return NULL;
    if (textIsUtf8(raw))
        return raw;
    converted = agentToUtf8(raw);
    FreeVec(raw);
    return converted;
}

static BOOL pathExists(CONST_STRPTR path) {
    BPTR lock = Lock(path, SHARED_LOCK);
    if (lock == 0)
        return FALSE;
    UnLock(lock);
    return TRUE;
}

static void ensureDirectory(CONST_STRPTR path) {
    BPTR lock = Lock(path, SHARED_LOCK);
    if (lock == 0)
        lock = CreateDir(path);
    if (lock != 0)
        UnLock(lock);
}

/* Create every drawer leading up to a file */
static void ensureParentDirectories(CONST_STRPTR path) {
    UBYTE partial[AGENT_PATH_LENGTH];
    ULONG i;
    BOOL pastDevice = strchr(path, ':') == NULL;
    for (i = 0; path[i] != '\0' && i < sizeof(partial) - 1; i++) {
        if (path[i] == ':')
            pastDevice = TRUE;
        else if (path[i] == '/' && pastDevice && i > 0 && path[i - 1] != ':' &&
                 path[i - 1] != '/') {
            partial[i] = '\0';
            ensureDirectory(partial);
        }
        partial[i] = path[i];
    }
}

static void joinPath(STRPTR out, ULONG size, CONST_STRPTR directory,
                     CONST_STRPTR name) {
    strncpy(out, directory, size - 1);
    out[size - 1] = '\0';
    AddPart(out, name, size);
}

typedef void (*AgentDirectoryFunc)(CONST_STRPTR name, BOOL isDirectory,
                                   APTR userData);

static BOOL scanDirectory(CONST_STRPTR path, AgentDirectoryFunc func,
                          APTR userData) {
#if defined(__AMIGAOS4__)
    APTR context =
        ObtainDirContextTags(EX_StringNameInput, path, EX_DataFields,
                             EXF_NAME | EXF_TYPE, TAG_END);
    struct ExamineData *data;
    if (context == NULL)
        return FALSE;
    while ((data = ExamineDir(context)) != NULL)
        func(data->Name, EXD_IS_DIRECTORY(data) ? TRUE : FALSE, userData);
    ReleaseDirContext(context);
    return TRUE;
#else
    BPTR lock = Lock(path, SHARED_LOCK);
    struct FileInfoBlock *fib;
    BOOL ok = FALSE;
    if (lock == 0)
        return FALSE;
    fib = AllocDosObject(DOS_FIB, NULL);
    if (fib != NULL) {
        if (Examine(lock, fib) && fib->fib_DirEntryType > 0) {
            ok = TRUE;
            while (ExNext(lock, fib))
                func(fib->fib_FileName, fib->fib_DirEntryType > 0 ? TRUE : FALSE,
                     userData);
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    return ok;
#endif
}

static void deleteDiskObject(CONST_STRPTR path) {
#ifdef __AMIGAOS4__
    Delete(path);
#else
    DeleteFile(path);
#endif
}

static CONST_STRPTR jsonString(struct json_object *object, CONST_STRPTR key) {
    struct json_object *value;
    CONST_STRPTR text;
    if (object == NULL || !json_object_object_get_ex(object, key, &value))
        return "";
    if (!json_object_is_type(value, json_type_string))
        return "";
    text = json_object_get_string(value);
    return text != NULL ? text : "";
}

static void outputLocal(AgentOutputFunc output, CONST_STRPTR localText) {
    STRPTR utf8;
    if (output == NULL || localText == NULL)
        return;
    utf8 = agentToUtf8(localText);
    if (utf8 != NULL) {
        output(utf8);
        FreeVec(utf8);
    }
}

/* Cut a tool result that would fill the model's context */
static STRPTR limitToolOutput(STRPTR text) {
    ULONG length;
    ULONG cut = AGENT_TOOL_OUTPUT_MAX;
    struct AgentBuffer buffer = {NULL, 0, 0};
    UBYTE note[96];
    if (text == NULL)
        return agentDuplicate("");
    length = strlen(text);
    if (length <= AGENT_TOOL_OUTPUT_MAX)
        return text;
    while (cut > 0 && ((UBYTE)text[cut] & 0xC0) == 0x80)
        cut--;
    agentBufferAppendLength(&buffer, text, cut);
    snprintf(note, sizeof(note), "\n[Output truncated: %lu of %lu bytes shown]",
             cut, length);
    agentBufferAppend(&buffer, note);
    FreeVec(text);
    return bufferTake(&buffer);
}

/* ------------------------------------------------------------------------
 * Settings and projects
 * ---------------------------------------------------------------------- */

static void saveSettings(void) {
    CONST_STRPTR text;
    if (settings == NULL)
        return;
    text = json_object_to_json_string_ext(settings, JSON_C_TO_STRING_PRETTY);
    if (text != NULL)
        agentWriteFile(AGENT_SETTINGS_PATH, text, strlen(text));
}

static LONG settingsGetInt(CONST_STRPTR key, LONG defaultValue) {
    struct json_object *value;
    if (settings == NULL || !json_object_object_get_ex(settings, key, &value))
        return defaultValue;
    return (LONG)json_object_get_int(value);
}

static void settingsSetInt(CONST_STRPTR key, LONG value) {
    if (settings == NULL)
        return;
    json_object_object_add(settings, key, json_object_new_int(value));
    saveSettings();
}

static struct json_object *projectsArray(void) {
    struct json_object *projects;
    if (settings == NULL)
        return NULL;
    if (!json_object_object_get_ex(settings, "projects", &projects) ||
        !json_object_is_type(projects, json_type_array)) {
        projects = json_object_new_array();
        json_object_object_add(settings, "projects", projects);
    }
    return projects;
}

static struct json_object *activeProjectObject(void) {
    LONG index = agentGetActiveProject();
    if (index < 0)
        return NULL;
    return json_object_array_get_idx(projectsArray(), index);
}

/* The drawer of the active project in the local character set, or "" */
static CONST_STRPTR activeProjectPath(void) {
    return jsonString(activeProjectObject(), "path");
}

void agentSetRequesterWindow(APTR windowObject) {
    requesterWindow = windowObject;
}

AgentApprovalMode agentGetApprovalMode(void) {
    LONG mode = settingsGetInt("approvalMode", AGENT_APPROVAL_ASK);
    if (mode < 0 || mode >= AGENT_APPROVAL_COUNT)
        mode = AGENT_APPROVAL_ASK;
    return (AgentApprovalMode)mode;
}

void agentSetApprovalMode(AgentApprovalMode mode) {
    settingsSetInt("approvalMode", (LONG)mode);
}

AgentEffort agentGetEffort(void) {
    LONG effort = settingsGetInt("effort", AGENT_EFFORT_DEFAULT);
    if (effort < 0 || effort >= AGENT_EFFORT_COUNT)
        effort = AGENT_EFFORT_DEFAULT;
    return (AgentEffort)effort;
}

void agentSetEffort(AgentEffort effort) {
    settingsSetInt("effort", (LONG)effort);
}

CONST_STRPTR agentGetModel(void) { return jsonString(settings, "model"); }

void agentSetModel(CONST_STRPTR model) {
    if (settings == NULL)
        return;
    json_object_object_add(settings, "model",
                           json_object_new_string(model != NULL ? model : ""));
    saveSettings();
}

CONST_STRPTR agentEffectiveModel(void) {
    CONST_STRPTR model = agentGetModel();
    if (model[0] == '\0') {
        struct ChatRequestSettings chatSettings;
        configGetActiveChatRequestSettings(&chatSettings);
        model = chatSettings.model != NULL ? chatSettings.model : "";
    }
    strncpy(effectiveModelBuffer, model, sizeof(effectiveModelBuffer) - 1);
    effectiveModelBuffer[sizeof(effectiveModelBuffer) - 1] = '\0';
    return effectiveModelBuffer;
}

ULONG agentProjectCount(void) {
    struct json_object *projects = projectsArray();
    return projects != NULL ? (ULONG)json_object_array_length(projects) : 0;
}

CONST_STRPTR agentProjectName(ULONG index) {
    return jsonString(json_object_array_get_idx(projectsArray(), index),
                      "name");
}

CONST_STRPTR agentProjectPath(ULONG index) {
    return jsonString(json_object_array_get_idx(projectsArray(), index),
                      "path");
}

LONG agentGetActiveProject(void) {
    CONST_STRPTR active = jsonString(settings, "activeProject");
    ULONG count = agentProjectCount();
    ULONG i;
    if (active[0] == '\0')
        return -1;
    for (i = 0; i < count; i++) {
        if (strcmp(agentProjectName(i), active) == 0)
            return (LONG)i;
    }
    return -1;
}

void agentSetActiveProject(LONG index) {
    if (settings == NULL)
        return;
    json_object_object_add(
        settings, "activeProject",
        json_object_new_string(index >= 0 && (ULONG)index < agentProjectCount()
                                   ? agentProjectName((ULONG)index)
                                   : ""));
    saveSettings();
}

LONG agentAddProject(CONST_STRPTR path) {
    struct json_object *projects = projectsArray();
    struct json_object *project;
    UBYTE name[108];
    UBYTE candidate[120];
    CONST_STRPTR part;
    ULONG count = agentProjectCount();
    ULONG suffix = 2;
    ULONG i;
    BOOL taken;

    if (projects == NULL || path == NULL || path[0] == '\0')
        return -1;
    for (i = 0; i < count; i++) {
        if (strcasecmp(agentProjectPath(i), path) == 0) {
            agentSetActiveProject((LONG)i);
            return (LONG)i;
        }
    }

    /* Name the project after its drawer, or its volume for a root drawer */
    part = FilePart(path);
    strncpy(name, part != NULL && part[0] != '\0' ? part : path,
            sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    if (name[0] != '\0' && name[strlen(name) - 1] == ':')
        name[strlen(name) - 1] = '\0';
    strncpy(candidate, name, sizeof(candidate) - 1);
    candidate[sizeof(candidate) - 1] = '\0';
    do {
        taken = FALSE;
        for (i = 0; i < count; i++) {
            if (strcasecmp(agentProjectName(i), candidate) == 0) {
                taken = TRUE;
                snprintf(candidate, sizeof(candidate), "%s %lu", name,
                         suffix++);
                break;
            }
        }
    } while (taken);

    project = json_object_new_object();
    json_object_object_add(project, "name", json_object_new_string(candidate));
    json_object_object_add(project, "path", json_object_new_string(path));
    json_object_object_add(project, "allow", json_object_new_array());
    json_object_array_add(projects, project);
    agentSetActiveProject((LONG)count);
    return (LONG)count;
}

void agentRemoveProject(ULONG index) {
    struct json_object *projects = projectsArray();
    LONG active = agentGetActiveProject();
    if (projects == NULL || index >= agentProjectCount())
        return;
    if (active == (LONG)index)
        json_object_object_add(settings, "activeProject",
                               json_object_new_string(""));
    json_object_array_del_idx(projects, index, 1);
    saveSettings();
}

void agentInstructionsPath(STRPTR buffer, ULONG size) {
    CONST_STRPTR project = activeProjectPath();
    if (project[0] != '\0')
        joinPath(buffer, size, project, "AGENTS.md");
    else {
        strncpy(buffer, AGENT_GLOBAL_INSTRUCTIONS_PATH, size - 1);
        buffer[size - 1] = '\0';
    }
    if (!pathExists(buffer))
        agentWriteFile(buffer, "", 0);
}

void agentMcpConfigPath(STRPTR buffer, ULONG size) {
    static CONST_STRPTR emptyConfig = "{\n  \"mcpServers\": {\n  }\n}\n";
    CONST_STRPTR project = activeProjectPath();
    if (project[0] != '\0') {
        UBYTE directory[AGENT_PATH_LENGTH];
        joinPath(directory, sizeof(directory), project, ".agents");
        ensureDirectory(directory);
        joinPath(buffer, size, directory, "mcp.json");
    } else {
        strncpy(buffer, AGENT_GLOBAL_MCP_PATH, size - 1);
        buffer[size - 1] = '\0';
    }
    if (!pathExists(buffer))
        agentWriteFile(buffer, emptyConfig, strlen(emptyConfig));
}

/* ------------------------------------------------------------------------
 * Skills, commands and subagents
 * ---------------------------------------------------------------------- */

/* The text after the front matter block, or the whole text without one */
static CONST_STRPTR frontMatterBody(CONST_STRPTR text) {
    CONST_STRPTR p = text;
    if (strncmp(p, "---", 3) != 0)
        return text;
    p = strchr(p, '\n');
    while (p != NULL) {
        p++;
        if (strncmp(p, "---", 3) == 0) {
            p = strchr(p, '\n');
            return p != NULL ? p + 1 : text + strlen(text);
        }
        p = strchr(p, '\n');
    }
    return text;
}

/* Read "key: value" from the front matter block at the top of a file */
static BOOL frontMatterValue(CONST_STRPTR text, CONST_STRPTR key, STRPTR out,
                             ULONG size) {
    CONST_STRPTR body = frontMatterBody(text);
    CONST_STRPTR line = text;
    ULONG keyLength = strlen(key);
    out[0] = '\0';
    if (body == text)
        return FALSE;
    while (line != NULL && line < body) {
        if (strncmp(line, key, keyLength) == 0 && line[keyLength] == ':') {
            CONST_STRPTR value = line + keyLength + 1;
            ULONG i = 0;
            while (*value == ' ' || *value == '\t')
                value++;
            if (*value == '"' || *value == '\'')
                value++;
            while (value[i] != '\0' && value[i] != '\n' && value[i] != '\r' &&
                   i + 1 < size) {
                out[i] = value[i];
                i++;
            }
            while (i > 0 && (out[i - 1] == '"' || out[i - 1] == '\'' ||
                             out[i - 1] == ' '))
                i--;
            out[i] = '\0';
            return i > 0;
        }
        line = strchr(line, '\n');
        if (line != NULL)
            line++;
    }
    return FALSE;
}

static struct json_object *findByName(struct json_object *array,
                                      CONST_STRPTR name) {
    ULONG count;
    ULONG i;
    if (array == NULL || name == NULL)
        return NULL;
    count = (ULONG)json_object_array_length(array);
    for (i = 0; i < count; i++) {
        struct json_object *entry = json_object_array_get_idx(array, i);
        if (strcasecmp(jsonString(entry, "name"), name) == 0)
            return entry;
    }
    return NULL;
}

struct CatalogScan {
    CONST_STRPTR directory;
    struct json_object *array;
    BOOL skillDirectories;
};

static void catalogEntryFound(CONST_STRPTR name, BOOL isDirectory,
                              APTR userData) {
    struct CatalogScan *scan = (struct CatalogScan *)userData;
    UBYTE path[AGENT_PATH_LENGTH];
    UBYTE file[AGENT_PATH_LENGTH];
    UBYTE entryName[128];
    UBYTE description[512];
    UBYTE value[256];
    STRPTR text;
    STRPTR nameUtf8;
    struct json_object *entry;

    joinPath(path, sizeof(path), scan->directory, name);
    if (scan->skillDirectories) {
        if (!isDirectory)
            return;
        joinPath(file, sizeof(file), path, "SKILL.md");
        strncpy(entryName, name, sizeof(entryName) - 1);
        entryName[sizeof(entryName) - 1] = '\0';
    } else {
        ULONG length = strlen(name);
        if (isDirectory || length < 4 || strcasecmp(name + length - 3, ".md") != 0)
            return;
        strncpy(file, path, sizeof(file) - 1);
        file[sizeof(file) - 1] = '\0';
        strncpy(entryName, name, sizeof(entryName) - 1);
        entryName[sizeof(entryName) - 1] = '\0';
        if (length - 3 < sizeof(entryName))
            entryName[length - 3] = '\0';
    }

    text = readTextFileUtf8(file);
    if (text == NULL)
        return;
    nameUtf8 = agentToUtf8(entryName);
    if (frontMatterValue(text, "name", value, sizeof(value))) {
        if (nameUtf8 != NULL)
            FreeVec(nameUtf8);
        nameUtf8 = agentDuplicate(value);
    }
    frontMatterValue(text, "description", description, sizeof(description));

    /* The first one found wins, so a project can replace a global entry */
    if (nameUtf8 != NULL && findByName(scan->array, nameUtf8) == NULL) {
        entry = json_object_new_object();
        json_object_object_add(entry, "name", json_object_new_string(nameUtf8));
        json_object_object_add(entry, "description",
                               json_object_new_string(description));
        json_object_object_add(entry, "file", json_object_new_string(file));
        json_object_object_add(entry, "directory",
                               json_object_new_string(
                                   scan->skillDirectories
                                       ? (CONST_STRPTR)path
                                       : scan->directory));
        if (frontMatterValue(text, "model", value, sizeof(value)))
            json_object_object_add(entry, "model",
                                   json_object_new_string(value));
        if (frontMatterValue(text, "tools", value, sizeof(value)))
            json_object_object_add(entry, "tools",
                                   json_object_new_string(value));
        json_object_array_add(scan->array, entry);
    }
    if (nameUtf8 != NULL)
        FreeVec(nameUtf8);
    FreeVec(text);
}

static void scanCatalogDirectory(CONST_STRPTR directory,
                                 struct json_object *array,
                                 BOOL skillDirectories) {
    struct CatalogScan scan;
    scan.directory = directory;
    scan.array = array;
    scan.skillDirectories = skillDirectories;
    scanDirectory(directory, catalogEntryFound, &scan);
}

/* Find every skill, command and subagent the active project can use */
static void loadCatalog(void) {
    static CONST_STRPTR projectFolders[] = {".agents", ".claude", NULL};
    CONST_STRPTR project = activeProjectPath();
    ULONG i;

    if (skills != NULL)
        json_object_put(skills);
    if (commands != NULL)
        json_object_put(commands);
    if (subagents != NULL)
        json_object_put(subagents);
    skills = json_object_new_array();
    commands = json_object_new_array();
    subagents = json_object_new_array();

    if (project[0] != '\0') {
        for (i = 0; projectFolders[i] != NULL; i++) {
            UBYTE base[AGENT_PATH_LENGTH];
            UBYTE directory[AGENT_PATH_LENGTH];
            joinPath(base, sizeof(base), project, projectFolders[i]);
            joinPath(directory, sizeof(directory), base, "skills");
            scanCatalogDirectory(directory, skills, TRUE);
            joinPath(directory, sizeof(directory), base, "commands");
            scanCatalogDirectory(directory, commands, FALSE);
            joinPath(directory, sizeof(directory), base, "agents");
            scanCatalogDirectory(directory, subagents, FALSE);
        }
    }
    scanCatalogDirectory(AGENT_GLOBAL_SKILLS_DIR, skills, TRUE);
    scanCatalogDirectory(AGENT_GLOBAL_COMMANDS_DIR, commands, FALSE);
    scanCatalogDirectory(AGENT_GLOBAL_AGENTS_DIR, subagents, FALSE);
}

static void describeCatalog(struct AgentBuffer *buffer,
                            struct json_object *array, CONST_STRPTR prefix) {
    ULONG count = array != NULL ? (ULONG)json_object_array_length(array) : 0;
    ULONG i;
    for (i = 0; i < count; i++) {
        struct json_object *entry = json_object_array_get_idx(array, i);
        CONST_STRPTR description = jsonString(entry, "description");
        agentBufferAppend(buffer, "- ");
        agentBufferAppend(buffer, prefix);
        agentBufferAppend(buffer, jsonString(entry, "name"));
        if (description[0] != '\0') {
            agentBufferAppend(buffer, ": ");
            agentBufferAppend(buffer, description);
        }
        agentBufferAppend(buffer, "\n");
    }
}

/* ------------------------------------------------------------------------
 * System prompt
 * ---------------------------------------------------------------------- */

static void appendInstructionFile(struct AgentBuffer *buffer,
                                  CONST_STRPTR path, CONST_STRPTR heading) {
    STRPTR text = readTextFileUtf8(path);
    ULONG length;
    if (text == NULL)
        return;
    length = strlen(text);
    if (length > AGENT_MAX_INSTRUCTION_BYTES) {
        length = AGENT_MAX_INSTRUCTION_BYTES;
        while (length > 0 && ((UBYTE)text[length] & 0xC0) == 0x80)
            length--;
    }
    if (length > 0) {
        agentBufferAppend(buffer, "\n\n# ");
        agentBufferAppend(buffer, heading);
        agentBufferAppend(buffer, "\n\n");
        agentBufferAppendLength(buffer, text, length);
    }
    FreeVec(text);
}

static STRPTR buildSystemPrompt(struct json_object *subagent) {
    static CONST_STRPTR approvalNotes[] = {
        "Reading local files is always allowed. The user is asked before a "
        "shell command, an ARexx command, an MCP tool, web access, image "
        "generation or a file change runs, and may refuse it.",
        "Read-only mode is on: shell commands, ARexx commands, MCP tools, web "
        "access, image generation and file changes are refused. Investigate "
        "and report instead.",
        "File writes and edits run without asking. The user is asked before "
        "a shell command, an ARexx command, an MCP tool, web access or image "
        "generation runs.",
        "Every tool runs without asking the user. Be careful with anything "
        "that deletes or overwrites data."};
    struct AgentBuffer buffer = {NULL, 0, 0};
    CONST_STRPTR project = activeProjectPath();

    if (subagent == NULL) {
        agentBufferAppend(
            &buffer,
            "You are AmigaGPT Agent, an assistant that carries out tasks on "
            "the user's own computer by calling tools. Work on the task until "
            "it is done, then give a short plain report of what you did and "
            "what you found. When a tool call is refused or fails, say so and "
            "carry on with what can still be done.");
    } else {
        STRPTR text = readTextFileUtf8(jsonString(subagent, "file"));
        agentBufferAppend(
            &buffer,
            "You are a subagent of AmigaGPT Agent. Carry out the task you are "
            "given with the tools you have, then reply with a complete report: "
            "it is the only thing the main agent will see.");
        if (text != NULL) {
            agentBufferAppend(&buffer, "\n\n");
            agentBufferAppend(&buffer, frontMatterBody(text));
            FreeVec(text);
        }
    }

    agentBufferAppend(&buffer, "\n\n# Environment\n\n");
#if defined(__AMIGAOS3__)
    agentBufferAppend(&buffer,
                      "This is an Amiga computer running AmigaOS 3 (68k).");
#elif defined(__AMIGAOS4__)
    agentBufferAppend(&buffer,
                      "This is a PowerPC Amiga computer running AmigaOS 4.");
#else
    agentBufferAppend(&buffer, "This is a PowerPC computer running MorphOS.");
#endif
    agentBufferAppend(
        &buffer,
        " The shell is AmigaDOS, not Unix:\n"
        "- Paths look like Device:Drawer/File. Devices include SYS:, RAM:, "
        "Work: and T:. A leading slash means the parent drawer.\n"
        "- list or dir (not ls), type (not cat), copy (not cp), delete (not "
        "rm), rename (not mv), makedir (not mkdir).\n"
        "- Wildcards are #? (any text) and ? (one character), not *.\n"
        "- Redirect with > and >>. Run ARexx scripts with rx.\n"
        "- Commands run without a keyboard and are given up on after a time "
        "limit. Do not start anything that waits for input or opens a window "
        "the user must close; start such programs with run >NIL: instead.\n"
        "- You are running inside AmigaGPT. Do not send ARexx commands to "
        "AmigaGPT's own port and do not run askgpt.\n"
        "Files are stored in the ISO 8859-1 character set. Write plain text "
        "that fits it: no emoji, typographic quotes, long dashes or other "
        "symbols outside it, in files and in replies.\n");

    if (project[0] != '\0') {
        STRPTR projectUtf8 = agentToUtf8(project);
        agentBufferAppend(&buffer, "The active project is the drawer ");
        agentBufferAppend(&buffer, projectUtf8 != NULL ? projectUtf8 : "");
        agentBufferAppend(&buffer,
                          ". Shell commands start there and relative paths "
                          "are relative to it.\n");
        if (projectUtf8 != NULL)
            FreeVec(projectUtf8);
    } else {
        agentBufferAppend(&buffer, "No project is active, so always use full "
                                   "paths with a device name.\n");
    }
    agentBufferAppend(&buffer, approvalNotes[agentGetApprovalMode()]);

    if (subagent == NULL) {
        if (skills != NULL && json_object_array_length(skills) > 0) {
            agentBufferAppend(
                &buffer,
                "\n\n# Skills\n\nA skill is a set of instructions for one "
                "kind of task. When a task matches one of these, call "
                "load_skill with its name first and follow it:\n");
            describeCatalog(&buffer, skills, "");
        }
        agentBufferAppend(
            &buffer,
            "\n\n# Subagents\n\nUse run_subagent for a side task whose "
            "details you do not need, such as a broad search. Available "
            "subagents:\n- general: a general purpose agent with the same "
            "tools as you\n");
        describeCatalog(&buffer, subagents, "");
    }

    appendInstructionFile(&buffer, AGENT_GLOBAL_INSTRUCTIONS_PATH,
                          "Instructions from the user");
    if (project[0] != '\0') {
        UBYTE path[AGENT_PATH_LENGTH];
        joinPath(path, sizeof(path), project, "AGENTS.md");
        appendInstructionFile(&buffer, path, "Instructions for this project");
    }
    return bufferTake(&buffer);
}

/* ------------------------------------------------------------------------
 * Tools
 * ---------------------------------------------------------------------- */

static const struct AgentToolDefinition *findBuiltInTool(CONST_STRPTR name) {
    ULONG i;
    for (i = 0; BUILT_IN_TOOLS[i].name != NULL; i++) {
        if (strcmp(BUILT_IN_TOOLS[i].name, name) == 0)
            return &BUILT_IN_TOOLS[i];
    }
    return NULL;
}

/* Whether a name is in a comma separated list */
static BOOL listContains(CONST_STRPTR list, CONST_STRPTR name) {
    ULONG nameLength = strlen(name);
    CONST_STRPTR p = list;
    while (p != NULL && *p != '\0') {
        while (*p == ' ' || *p == ',')
            p++;
        if (strncasecmp(p, name, nameLength) == 0 &&
            (p[nameLength] == '\0' || p[nameLength] == ',' ||
             p[nameLength] == ' '))
            return TRUE;
        p = strchr(p, ',');
    }
    return FALSE;
}

/**
 * Build the tool list for a request
 * @param allowSubagents whether run_subagent is offered
 * @param allowed a comma separated list of tool names to keep, or NULL or ""
 * for every tool
 **/
static struct json_object *buildTools(BOOL allowSubagents,
                                      CONST_STRPTR allowed) {
    struct json_object *tools = json_object_new_array();
    BOOL filter = allowed != NULL && allowed[0] != '\0';
    ULONG i;
    for (i = 0; BUILT_IN_TOOLS[i].name != NULL; i++) {
        const struct AgentToolDefinition *definition = &BUILT_IN_TOOLS[i];
        struct json_object *tool;
        struct json_object *schema;
        if (!allowSubagents && strcmp(definition->name, "run_subagent") == 0)
            continue;
        if (strcmp(definition->name, "load_skill") == 0 &&
            (skills == NULL || json_object_array_length(skills) == 0))
            continue;
        if (filter && !listContains(allowed, definition->name))
            continue;
        schema = json_tokener_parse(definition->schema);
        if (schema == NULL)
            continue;
        tool = json_object_new_object();
        json_object_object_add(tool, "name",
                               json_object_new_string(definition->name));
        json_object_object_add(tool, "description",
                               json_object_new_string(definition->description));
        json_object_object_add(tool, "parameters", schema);
        json_object_array_add(tools, tool);
    }
    if (!filter)
        agentMcpAppendTools(tools);
    return tools;
}

/* Turn a path from the model into a local path, relative to the project */
static void resolvePath(CONST_STRPTR pathUtf8, STRPTR out, ULONG size) {
    STRPTR local = agentToLocal(pathUtf8 != NULL ? pathUtf8 : "");
    CONST_STRPTR project = activeProjectPath();
    CONST_STRPTR path = local != NULL ? local : "";
    while (path[0] == '.' && path[1] == '/')
        path += 2;
    if (strcmp(path, ".") == 0)
        path = "";
    if (strchr(path, ':') != NULL || project[0] == '\0') {
        strncpy(out, path, size - 1);
        out[size - 1] = '\0';
    } else if (path[0] == '\0') {
        strncpy(out, project, size - 1);
        out[size - 1] = '\0';
    } else {
        joinPath(out, size, project, path);
    }
    if (local != NULL)
        FreeVec(local);
}

/**
 * Run a command line in a Shell without blocking the program.
 *
 * The command is started in the background and this waits for it while the
 * GUI keeps handling its events, so the window stays alive, Stop works, and
 * a command that never returns (one waiting for input, or one talking to
 * this program's own ARexx port) cannot freeze everything. A command that is
 * still running at the time limit is left running and reported as such.
 *
 * @param commandLocal the command line in the local character set
 * @param directory the drawer to run it in, or "" for none
 * @param exitCodeOut receives the return code, or -1 if it is not known
 * @param finishedOut receives whether the command ended in time
 * @return the output in the local character set. Free it with FreeVec()
 **/
static STRPTR runCommand(CONST_STRPTR commandLocal, CONST_STRPTR directory,
                         LONG *exitCodeOut, BOOL *finishedOut) {
    UBYTE scriptPath[64];
    UBYTE outputPath[64];
    UBYTE codePath[64];
    UBYTE commandLine[96];
    struct AgentBuffer script = {NULL, 0, 0};
    struct TagItem tags[6];
    LONG timeout = settingsGetInt("commandTimeout",
                                  AGENT_DEFAULT_COMMAND_TIMEOUT);
    LONG waitedTicks = 0;
    BPTR input;
    BPTR output;
    STRPTR text = NULL;
    STRPTR code;

    *exitCodeOut = -1;
    *finishedOut = FALSE;
    if (timeout < 5)
        timeout = AGENT_DEFAULT_COMMAND_TIMEOUT;

    /* Each command gets its own files, because the files of one that was
     * left running are still in use */
    commandCounter++;
    snprintf(scriptPath, sizeof(scriptPath), AGENT_COMMAND_FILE_FORMAT,
             commandCounter, "script");
    snprintf(outputPath, sizeof(outputPath), AGENT_COMMAND_FILE_FORMAT,
             commandCounter, "out");
    snprintf(codePath, sizeof(codePath), AGENT_COMMAND_FILE_FORMAT,
             commandCounter, "rc");

    /* FailAt keeps the script going after a failed command, so the last
     * line can record the return code */
    agentBufferAppend(&script, "FailAt 30000\n");
    if (directory != NULL && directory[0] != '\0') {
        agentBufferAppend(&script, "CD \"");
        agentBufferAppend(&script, directory);
        agentBufferAppend(&script, "\"\n");
    }
    agentBufferAppend(&script, commandLocal);
    agentBufferAppend(&script, "\nEcho >");
    agentBufferAppend(&script, codePath);
    agentBufferAppend(&script, " \"$RC\"\n");
    agentWriteFile(scriptPath, script.data, script.length);
    agentBufferFree(&script);

    /* A background Shell closes both streams itself when it ends. Input
     * comes from NIL: so nothing can sit waiting for the keyboard. */
    input = Open("NIL:", MODE_OLDFILE);
    output = Open(outputPath, MODE_NEWFILE);
    if (input == 0 || output == 0) {
        if (input != 0)
            Close(input);
        if (output != 0)
            Close(output);
        deleteDiskObject(scriptPath);
        return agentDuplicate("Error: could not create the temporary files.");
    }
    tags[0].ti_Tag = SYS_Input;
    tags[0].ti_Data = (ULONG)input;
    tags[1].ti_Tag = SYS_Output;
    tags[1].ti_Data = (ULONG)output;
    tags[2].ti_Tag = SYS_Asynch;
    tags[2].ti_Data = TRUE;
    tags[3].ti_Tag = NP_StackSize;
    tags[3].ti_Data = 32768;
    /* No "Please insert volume" requesters: nobody is watching for them,
     * so a bad device name should fail at once instead of waiting */
    tags[4].ti_Tag = NP_WindowPtr;
    tags[4].ti_Data = (ULONG)-1;
    tags[5].ti_Tag = TAG_DONE;
    tags[5].ti_Data = 0;
    snprintf(commandLine, sizeof(commandLine), "Execute %s", scriptPath);
    if (runShellCommand(commandLine, tags) == -1) {
        Close(input);
        Close(output);
        deleteDiskObject(outputPath);
        deleteDiskObject(scriptPath);
        return agentDuplicate("Error: the command could not be started.");
    }

    /* The Shell holds the output file open for writing until it ends, and
     * a file open for writing cannot be opened by anyone else. So the
     * command has finished once the file can be opened here. */
    while (waitedTicks < timeout * TICKS_PER_SECOND) {
        BPTR probe;
        if (!pumpRequestEvents())
            break;
        probe = Open(outputPath, MODE_OLDFILE);
        if (probe != 0) {
            Close(probe);
            *finishedOut = TRUE;
            break;
        }
        Delay(AGENT_COMMAND_POLL_TICKS);
        waitedTicks += AGENT_COMMAND_POLL_TICKS;
    }
    if (!*finishedOut)
        return agentDuplicate("");

    text = agentReadFile(outputPath, NULL);
    code = agentReadFile(codePath, NULL);
    if (code != NULL) {
        *exitCodeOut = strtol(code, NULL, 10);
        FreeVec(code);
    }
    deleteDiskObject(outputPath);
    deleteDiskObject(codePath);
    deleteDiskObject(scriptPath);
    return text != NULL ? text : agentDuplicate("");
}

/* Run a command and describe the outcome for the model, in UTF-8 */
static STRPTR runCommandForModel(CONST_STRPTR commandLocal,
                                 CONST_STRPTR directory, BOOL *isError) {
    struct AgentBuffer buffer = {NULL, 0, 0};
    LONG exitCode = -1;
    BOOL finished = FALSE;
    STRPTR output = runCommand(commandLocal, directory, &exitCode, &finished);
    STRPTR outputUtf8;
    UBYTE header[320];

    if (!finished) {
        if (output != NULL)
            FreeVec(output);
        *isError = TRUE;
        if (isRequestCancelled())
            return agentDuplicate("The user stopped the agent while this "
                                  "command was running.");
        snprintf(header, sizeof(header),
                 "The command did not finish within %ld seconds and was left "
                 "running in the background. Its output is not available. It "
                 "may be waiting for input or for a window to be closed.",
                 settingsGetInt("commandTimeout",
                                AGENT_DEFAULT_COMMAND_TIMEOUT));
        return agentDuplicate(header);
    }
    if (exitCode < 0)
        snprintf(header, sizeof(header), "Exit code: unknown\n");
    else
        snprintf(header, sizeof(header), "Exit code: %ld\n", exitCode);
    agentBufferAppend(&buffer, header);
    outputUtf8 = agentToUtf8(output != NULL ? output : "");
    agentBufferAppend(&buffer, outputUtf8 != NULL ? outputUtf8 : "");
    if (outputUtf8 != NULL)
        FreeVec(outputUtf8);
    if (output != NULL)
        FreeVec(output);
    if (exitCode >= 10)
        *isError = TRUE;
    return bufferTake(&buffer);
}

/* Run a shell command from the model in the project drawer */
static STRPTR runShellInProject(CONST_STRPTR commandUtf8, BOOL *isError) {
    STRPTR commandLocal = agentToLocal(commandUtf8);
    STRPTR result = runCommandForModel(commandLocal != NULL ? commandLocal : "",
                                       activeProjectPath(), isError);
    if (commandLocal != NULL)
        FreeVec(commandLocal);
    return result;
}

/* Whether a port name is this program's own ARexx port */
static BOOL isOwnArexxPort(CONST_STRPTR port) {
    STRPTR base = NULL;
    ULONG length;
    if (app == NULL || port == NULL)
        return FALSE;
    get(app, MUIA_Application_Base, &base);
    if (base == NULL)
        return FALSE;
    length = strlen(base);
    while (*port == ' ')
        port++;
    return strncasecmp(port, base, length) == 0 &&
           (port[length] == '\0' || port[length] == ' ' ||
            port[length] == ':');
}

static STRPTR toolReadFile(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    LONG size = 0;
    LONG offset = 1;
    LONG limit = AGENT_DEFAULT_READ_LINES;
    LONG line = 1;
    LONG shown = 0;
    struct json_object *value;
    struct AgentBuffer buffer = {NULL, 0, 0};
    STRPTR raw;
    STRPTR text;
    CONST_STRPTR p;

    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    if (json_object_object_get_ex(arguments, "offset", &value))
        offset = json_object_get_int(value);
    if (json_object_object_get_ex(arguments, "limit", &value))
        limit = json_object_get_int(value);
    if (offset < 1)
        offset = 1;
    if (limit < 1)
        limit = AGENT_DEFAULT_READ_LINES;

    raw = agentReadFile(path, &size);
    if (raw == NULL) {
        *isError = TRUE;
        return agentDuplicate("Error: the file could not be opened.");
    }
    if ((ULONG)size > AGENT_MAX_FILE_BYTES ||
        (size > 0 && memchr(raw, '\0', size) != NULL)) {
        FreeVec(raw);
        *isError = TRUE;
        return agentDuplicate(
            (ULONG)size > AGENT_MAX_FILE_BYTES
                ? "Error: the file is too large to read."
                : "Error: this is a binary file, not text.");
    }
    if (textIsUtf8(raw)) {
        text = raw;
    } else {
        text = agentToUtf8(raw);
        FreeVec(raw);
    }
    if (text == NULL) {
        *isError = TRUE;
        return agentDuplicate("Error: out of memory.");
    }

    p = text;
    while (*p != '\0' && shown < limit) {
        CONST_STRPTR end = strchr(p, '\n');
        ULONG length = end != NULL ? (ULONG)(end - p) + 1 : strlen(p);
        if (line >= offset) {
            agentBufferAppendLength(&buffer, p, length);
            shown++;
        }
        line++;
        p += length;
    }
    if (*p != '\0') {
        UBYTE note[96];
        snprintf(note, sizeof(note),
                 "\n[More lines follow. Continue with offset %ld]", line);
        agentBufferAppend(&buffer, note);
    }
    FreeVec(text);
    if (buffer.data == NULL)
        return agentDuplicate("(The file is empty or has no such lines)");
    return bufferTake(&buffer);
}

static STRPTR toolWriteFile(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    UBYTE message[AGENT_PATH_LENGTH + 64];
    STRPTR content = agentToLocal(jsonString(arguments, "content"));
    STRPTR pathUtf8;
    BOOL ok;

    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    ensureParentDirectories(path);
    ok = content != NULL && agentWriteFile(path, content, strlen(content));
    pathUtf8 = agentToUtf8(path);
    if (ok) {
        snprintf(message, sizeof(message), "Wrote %lu bytes to %s",
                 (ULONG)strlen(content), pathUtf8 != NULL ? pathUtf8 : "");
    } else {
        *isError = TRUE;
        snprintf(message, sizeof(message), "Error: could not write %s",
                 pathUtf8 != NULL ? pathUtf8 : "");
    }
    if (pathUtf8 != NULL)
        FreeVec(pathUtf8);
    if (content != NULL)
        FreeVec(content);
    return agentDuplicate(message);
}

static STRPTR toolEditFile(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    CONST_STRPTR oldString = jsonString(arguments, "old_string");
    CONST_STRPTR newString = jsonString(arguments, "new_string");
    ULONG oldLength = strlen(oldString);
    struct json_object *value;
    struct AgentBuffer buffer = {NULL, 0, 0};
    BOOL replaceAll = FALSE;
    ULONG occurrences = 0;
    STRPTR text;
    STRPTR local;
    CONST_STRPTR p;
    CONST_STRPTR match;
    BOOL ok;

    if (json_object_object_get_ex(arguments, "replace_all", &value))
        replaceAll = json_object_get_boolean(value) ? TRUE : FALSE;
    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    *isError = TRUE;
    if (oldLength == 0)
        return agentDuplicate("Error: old_string is empty.");
    text = readTextFileUtf8(path);
    if (text == NULL)
        return agentDuplicate("Error: the file could not be opened.");

    for (p = text; (match = strstr(p, oldString)) != NULL;
         p = match + oldLength)
        occurrences++;
    if (occurrences == 0) {
        FreeVec(text);
        return agentDuplicate(
            "Error: old_string was not found in the file. Read the file "
            "again and copy the text exactly.");
    }
    if (occurrences > 1 && !replaceAll) {
        FreeVec(text);
        return agentDuplicate(
            "Error: old_string appears more than once. Include more of the "
            "surrounding text, or set replace_all.");
    }

    p = text;
    while ((match = strstr(p, oldString)) != NULL) {
        agentBufferAppendLength(&buffer, p, (ULONG)(match - p));
        agentBufferAppend(&buffer, newString);
        p = match + oldLength;
        if (!replaceAll)
            break;
    }
    agentBufferAppend(&buffer, p);
    FreeVec(text);

    local = agentToLocal(buffer.data != NULL ? buffer.data : "");
    agentBufferFree(&buffer);
    ok = local != NULL && agentWriteFile(path, local, strlen(local));
    if (local != NULL)
        FreeVec(local);
    if (!ok)
        return agentDuplicate("Error: the file could not be written.");
    *isError = FALSE;
    return agentDuplicate(occurrences > 1 ? "Replaced every occurrence."
                                          : "The file was edited.");
}

struct ListScan {
    struct AgentBuffer *buffer;
    ULONG count;
};

static void listEntryFound(CONST_STRPTR name, BOOL isDirectory,
                           APTR userData) {
    struct ListScan *scan = (struct ListScan *)userData;
    STRPTR nameUtf8;
    if (scan->count++ >= AGENT_MAX_LIST_ENTRIES)
        return;
    nameUtf8 = agentToUtf8(name);
    agentBufferAppend(scan->buffer, nameUtf8 != NULL ? nameUtf8 : "");
    agentBufferAppend(scan->buffer, isDirectory ? "/\n" : "\n");
    if (nameUtf8 != NULL)
        FreeVec(nameUtf8);
}

static STRPTR toolListDirectory(struct json_object *arguments,
                                BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    struct AgentBuffer buffer = {NULL, 0, 0};
    struct ListScan scan;
    scan.buffer = &buffer;
    scan.count = 0;
    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    if (!scanDirectory(path, listEntryFound, &scan)) {
        agentBufferFree(&buffer);
        *isError = TRUE;
        return agentDuplicate("Error: the drawer could not be read.");
    }
    if (scan.count > AGENT_MAX_LIST_ENTRIES)
        agentBufferAppend(&buffer, "[More entries were not listed]\n");
    if (buffer.data == NULL)
        return agentDuplicate("(The drawer is empty)");
    return bufferTake(&buffer);
}

struct SearchScan {
    CONST_STRPTR directory;
    CONST_STRPTR pattern;
    struct AgentBuffer *buffer;
    ULONG matches;
    ULONG depth;
};

static CONST_STRPTR findIgnoringCase(CONST_STRPTR text, ULONG length,
                                     CONST_STRPTR pattern,
                                     ULONG patternLength) {
    ULONG i;
    if (patternLength == 0 || length < patternLength)
        return NULL;
    for (i = 0; i + patternLength <= length; i++) {
        if (strncasecmp(text + i, pattern, patternLength) == 0)
            return text + i;
    }
    return NULL;
}

static void searchEntryFound(CONST_STRPTR name, BOOL isDirectory,
                             APTR userData) {
    struct SearchScan *scan = (struct SearchScan *)userData;
    UBYTE path[AGENT_PATH_LENGTH];
    LONG size = 0;
    STRPTR raw;
    STRPTR pathUtf8;
    CONST_STRPTR p;
    ULONG line = 1;
    ULONG patternLength = strlen(scan->pattern);

    if (scan->matches >= AGENT_MAX_SEARCH_MATCHES || isRequestCancelled())
        return;
    joinPath(path, sizeof(path), scan->directory, name);
    if (isDirectory) {
        struct SearchScan inner = *scan;
        if (scan->depth >= AGENT_MAX_SEARCH_DEPTH)
            return;
        inner.directory = path;
        inner.depth = scan->depth + 1;
        scanDirectory(path, searchEntryFound, &inner);
        scan->matches = inner.matches;
        return;
    }

    raw = agentReadFile(path, &size);
    if (raw == NULL)
        return;
    if ((ULONG)size > AGENT_MAX_SEARCH_FILE_BYTES ||
        (size > 0 && memchr(raw, '\0', size) != NULL)) {
        FreeVec(raw);
        return;
    }
    pathUtf8 = agentToUtf8(path);
    for (p = raw; *p != '\0' && scan->matches < AGENT_MAX_SEARCH_MATCHES;) {
        CONST_STRPTR end = strchr(p, '\n');
        ULONG length = end != NULL ? (ULONG)(end - p) : strlen(p);
        if (findIgnoringCase(p, length, scan->pattern, patternLength) != NULL) {
            UBYTE prefix[32];
            UBYTE lineText[240];
            STRPTR lineUtf8;
            ULONG copyLength =
                length < sizeof(lineText) - 1 ? length : sizeof(lineText) - 1;
            CopyMem((APTR)p, lineText, copyLength);
            lineText[copyLength] = '\0';
            lineUtf8 = textIsUtf8(lineText) ? agentDuplicate(lineText)
                                            : agentToUtf8(lineText);
            snprintf(prefix, sizeof(prefix), ":%lu: ", line);
            agentBufferAppend(scan->buffer, pathUtf8 != NULL ? pathUtf8 : "");
            agentBufferAppend(scan->buffer, prefix);
            agentBufferAppend(scan->buffer, lineUtf8 != NULL ? lineUtf8 : "");
            agentBufferAppend(scan->buffer, "\n");
            if (lineUtf8 != NULL)
                FreeVec(lineUtf8);
            scan->matches++;
        }
        line++;
        p += length;
        if (*p == '\n')
            p++;
    }
    if (pathUtf8 != NULL)
        FreeVec(pathUtf8);
    FreeVec(raw);
}

static STRPTR toolSearchFiles(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    struct AgentBuffer buffer = {NULL, 0, 0};
    struct SearchScan scan;
    /* Files are searched as stored, so match in the local character set */
    STRPTR pattern = agentToLocal(jsonString(arguments, "pattern"));

    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    if (pattern == NULL || pattern[0] == '\0') {
        if (pattern != NULL)
            FreeVec(pattern);
        *isError = TRUE;
        return agentDuplicate("Error: pattern is empty.");
    }
    scan.directory = path;
    scan.pattern = pattern;
    scan.buffer = &buffer;
    scan.matches = 0;
    scan.depth = 0;
    if (!scanDirectory(path, searchEntryFound, &scan)) {
        FreeVec(pattern);
        agentBufferFree(&buffer);
        *isError = TRUE;
        return agentDuplicate("Error: the drawer could not be read.");
    }
    FreeVec(pattern);
    if (scan.matches >= AGENT_MAX_SEARCH_MATCHES)
        agentBufferAppend(&buffer, "[More matches were not listed]\n");
    if (buffer.data == NULL)
        return agentDuplicate("No matches.");
    return bufferTake(&buffer);
}

static STRPTR toolIdentifyFile(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    STRPTR mime;
    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    if (!pathExists(path)) {
        *isError = TRUE;
        return agentDuplicate("Error: the file was not found.");
    }
    mime = detectMimeTypeFromContents(path);
    if (mime == NULL)
        return agentDuplicate("The file type could not be determined.");
    return mime;
}

static STRPTR toolArexx(struct json_object *arguments, BOOL *isError) {
    STRPTR port = agentToLocal(jsonString(arguments, "port"));
    STRPTR command = agentToLocal(jsonString(arguments, "command"));
    struct AgentBuffer script = {NULL, 0, 0};
    UBYTE rexxPath[64];
    UBYTE commandLine[96];
    STRPTR result;
    CONST_STRPTR p;

    if (port == NULL || command == NULL || port[0] == '\0') {
        if (port != NULL)
            FreeVec(port);
        if (command != NULL)
            FreeVec(command);
        *isError = TRUE;
        return agentDuplicate("Error: port and command are required.");
    }
    /* This program cannot answer its own port in the middle of a tool
     * call without running a second request inside the first */
    if (isOwnArexxPort(port)) {
        FreeVec(port);
        FreeVec(command);
        *isError = TRUE;
        return agentDuplicate(
            "Refused: that is the ARexx port of AmigaGPT itself, which is the "
            "program you are running in. You cannot send it commands.");
    }

    /* A script file avoids quoting the command for both the Shell and
     * ARexx. Single quotes are doubled, which is how ARexx escapes them. */
    agentBufferAppend(&script, "/* AmigaGPT agent */\nOPTIONS RESULTS\n"
                               "IF ~SHOW('P', '");
    for (p = port; *p != '\0'; p++) {
        if (*p == '\'')
            agentBufferAppend(&script, "'");
        agentBufferAppendLength(&script, p, 1);
    }
    agentBufferAppend(&script, "') THEN DO\n  SAY 'Error: no such port.'\n"
                               "  EXIT 10\nEND\nADDRESS '");
    for (p = port; *p != '\0'; p++) {
        if (*p == '\'')
            agentBufferAppend(&script, "'");
        agentBufferAppendLength(&script, p, 1);
    }
    agentBufferAppend(&script, "'\n'");
    for (p = command; *p != '\0'; p++) {
        if (*p == '\'')
            agentBufferAppend(&script, "'");
        if (*p == '\n' || *p == '\r')
            agentBufferAppend(&script, " ");
        else
            agentBufferAppendLength(&script, p, 1);
    }
    agentBufferAppend(&script, "'\nSAY 'RC=' || RC\n"
                               "IF RC = 0 & SYMBOL('RESULT') = 'VAR' THEN "
                               "SAY 'RESULT=' || RESULT\n");
    snprintf(rexxPath, sizeof(rexxPath), AGENT_COMMAND_FILE_FORMAT,
             commandCounter + 1, "rexx");
    agentWriteFile(rexxPath, script.data, script.length);
    agentBufferFree(&script);
    FreeVec(port);
    FreeVec(command);

    snprintf(commandLine, sizeof(commandLine), "RX %s", rexxPath);
    result = runCommandForModel(commandLine, "", isError);
    deleteDiskObject(rexxPath);
    return result;
}

static STRPTR toolListArexxPorts(BOOL *isError) {
    return runCommandForModel("RX \"SAY SHOW('P')\"", "", isError);
}

#define AGENT_MAX_REDIRECTS 5

/**
 * GET a web address, following redirects
 * @param url the address
 * @param statusOut receives the final HTTP status
 * @param headers receives the final response headers
 * @param lengthOut receives the number of bytes in the body
 * @return the body. Free it with FreeVec(). NULL if nothing was received
 **/
static STRPTR httpGetFollowing(CONST_STRPTR url, LONG *statusOut,
                               STRPTR headers, ULONG headersSize,
                               ULONG *lengthOut) {
    struct ChatRequestSettings chatSettings;
    UBYTE current[2048];
    UBYTE host[256];
    UBYTE path[2048];
    UWORD port = 0;
    BOOL useSSL = TRUE;
    ULONG hop;

    strncpy(current, url, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';
    configGetActiveChatRequestSettings(&chatSettings);
    for (hop = 0; hop <= AGENT_MAX_REDIRECTS; hop++) {
        CONST_STRPTR line;
        STRPTR body;
        UBYTE location[2048];

        if (!parseHttpUrl(current, host, sizeof(host), &port, path,
                          sizeof(path), &useSSL))
            return NULL;
        body = httpSendRequestBinary(
            "GET", host, port, useSSL, path, "Accept: */*\r\n", NULL,
            chatSettings.useProxy, chatSettings.proxyHost,
            chatSettings.proxyPort, chatSettings.proxyUsesSSL,
            chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
            chatSettings.proxyPassword, statusOut, headers, headersSize,
            lengthOut);
        if (body == NULL || *statusOut < 300 || *statusOut > 399)
            return body;

        location[0] = '\0';
        for (line = headers; line != NULL && *line != '\0';) {
            if (strncasecmp(line, "Location:", 9) == 0) {
                CONST_STRPTR value = line + 9;
                ULONG i = 0;
                while (*value == ' ')
                    value++;
                while (value[i] != '\0' && value[i] != '\r' &&
                       value[i] != '\n' && i + 1 < sizeof(location)) {
                    location[i] = value[i];
                    i++;
                }
                location[i] = '\0';
                break;
            }
            line = strchr(line, '\n');
            if (line != NULL)
                line++;
        }
        if (location[0] == '\0')
            return body;
        FreeVec(body);
        if (location[0] == '/')
            /* An address on the same server */
            snprintf(current, sizeof(current), "%s://%s:%d%s",
                     useSSL ? "https" : "http", host, port, location);
        else {
            strncpy(current, location, sizeof(current) - 1);
            current[sizeof(current) - 1] = '\0';
        }
    }
    return NULL;
}

static STRPTR toolFetchUrl(struct json_object *arguments, BOOL *isError) {
    struct AgentBuffer buffer = {NULL, 0, 0};
    UBYTE headers[2048];
    UBYTE line[64];
    LONG status = 0;
    ULONG length = 0;
    STRPTR body;
    STRPTR bodyUtf8;

    *isError = TRUE;
    body = httpGetFollowing(jsonString(arguments, "url"), &status, headers,
                            sizeof(headers), &length);
    if (body == NULL)
        return agentDuplicate("Error: nothing was received. Check that the "
                              "address starts with http:// or https://.");
    if (length > 0 && memchr(body, '\0', length) != NULL) {
        FreeVec(body);
        return agentDuplicate("Error: this address serves a file that is not "
                              "text. Use download_file to save it.");
    }
    snprintf(line, sizeof(line), "HTTP status: %ld\n\n", status);
    agentBufferAppend(&buffer, line);
    bodyUtf8 = textIsUtf8(body) ? agentDuplicate(body) : agentToUtf8(body);
    agentBufferAppend(&buffer, bodyUtf8 != NULL ? bodyUtf8 : "");
    if (bodyUtf8 != NULL)
        FreeVec(bodyUtf8);
    FreeVec(body);
    *isError = status >= 400 ? TRUE : FALSE;
    return bufferTake(&buffer);
}

static STRPTR toolDownloadFile(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    UBYTE headers[2048];
    UBYTE message[AGENT_PATH_LENGTH + 64];
    LONG status = 0;
    ULONG length = 0;
    STRPTR body;
    STRPTR pathUtf8;
    BOOL saved;

    *isError = TRUE;
    body = httpGetFollowing(jsonString(arguments, "url"), &status, headers,
                            sizeof(headers), &length);
    if (body == NULL)
        return agentDuplicate("Error: nothing was received. Check that the "
                              "address starts with http:// or https://.");
    if (status != 200 || length == 0) {
        FreeVec(body);
        snprintf(message, sizeof(message),
                 "Error: the server answered with HTTP status %ld and no "
                 "file was saved.",
                 status);
        return agentDuplicate(message);
    }
    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    ensureParentDirectories(path);
    saved = agentWriteFile(path, body, length);
    FreeVec(body);
    if (!saved)
        return agentDuplicate("Error: the file could not be written.");
    pathUtf8 = agentToUtf8(path);
    snprintf(message, sizeof(message), "Saved %lu bytes to %s", length,
             pathUtf8 != NULL ? pathUtf8 : "");
    if (pathUtf8 != NULL)
        FreeVec(pathUtf8);
    *isError = FALSE;
    return agentDuplicate(message);
}

static STRPTR toolGenerateImage(struct json_object *arguments, BOOL *isError) {
    UBYTE path[AGENT_PATH_LENGTH];
    UBYTE message[AGENT_PATH_LENGTH + 64];
    STRPTR prompt = agentToLocal(jsonString(arguments, "prompt"));
    STRPTR pathUtf8;
    BOOL ok;

    *isError = TRUE;
    if (prompt == NULL || prompt[0] == '\0') {
        if (prompt != NULL)
            FreeVec(prompt);
        return agentDuplicate("Error: prompt is empty.");
    }
    resolvePath(jsonString(arguments, "path"), path, sizeof(path));
    ensureParentDirectories(path);
    ok = createImageFile(prompt, path) && pathExists(path);
    FreeVec(prompt);
    if (!ok)
        return agentDuplicate("Error: the picture could not be generated. "
                              "Check the image provider settings.");
    pathUtf8 = agentToUtf8(path);
    snprintf(message, sizeof(message), "Saved the picture to %s",
             pathUtf8 != NULL ? pathUtf8 : "");
    if (pathUtf8 != NULL)
        FreeVec(pathUtf8);
    *isError = FALSE;
    return agentDuplicate(message);
}

static STRPTR toolLoadSkill(struct json_object *arguments, BOOL *isError) {
    struct json_object *skill = findByName(skills, jsonString(arguments, "name"));
    struct AgentBuffer buffer = {NULL, 0, 0};
    STRPTR text;
    STRPTR directoryUtf8;
    if (skill == NULL) {
        *isError = TRUE;
        return agentDuplicate("Error: there is no skill with that name.");
    }
    text = readTextFileUtf8(jsonString(skill, "file"));
    if (text == NULL) {
        *isError = TRUE;
        return agentDuplicate("Error: the skill could not be read.");
    }
    directoryUtf8 = agentToUtf8(jsonString(skill, "directory"));
    agentBufferAppend(&buffer, "Skill drawer: ");
    agentBufferAppend(&buffer, directoryUtf8 != NULL ? directoryUtf8 : "");
    agentBufferAppend(&buffer,
                      "\nFiles the skill mentions are inside that drawer; "
                      "read them with read_file when you need them.\n\n");
    agentBufferAppend(&buffer, frontMatterBody(text));
    if (directoryUtf8 != NULL)
        FreeVec(directoryUtf8);
    FreeVec(text);
    return bufferTake(&buffer);
}

/* ------------------------------------------------------------------------
 * Approval
 * ---------------------------------------------------------------------- */

static AgentToolClass toolClassFor(CONST_STRPTR name) {
    const struct AgentToolDefinition *definition = findBuiltInTool(name);
    if (definition != NULL)
        return definition->toolClass;
    return AGENT_TOOL_EXECUTE;
}

/* The rule that "Always allow" stores for a call. Shell commands are allowed
 * by command name and ARexx by port, so one answer does not unlock both
 * "list" and "delete". */
static void allowRuleFor(CONST_STRPTR name, struct json_object *arguments,
                         STRPTR out, ULONG size) {
    if (strcmp(name, "shell") == 0) {
        CONST_STRPTR command = jsonString(arguments, "command");
        ULONG i = 0;
        ULONG length;
        snprintf(out, size, "shell:");
        length = strlen(out);
        while (*command == ' ')
            command++;
        while (command[i] != '\0' && command[i] != ' ' && command[i] != '\n' &&
               length + i + 1 < size) {
            UBYTE c = (UBYTE)command[i];
            out[length + i] = (c >= 'A' && c <= 'Z') ? (UBYTE)(c + 32) : c;
            i++;
        }
        out[length + i] = '\0';
    } else if (strcmp(name, "arexx") == 0) {
        snprintf(out, size, "arexx:%s", jsonString(arguments, "port"));
    } else {
        strncpy(out, name, size - 1);
        out[size - 1] = '\0';
    }
}

static struct json_object *allowRules(BOOL create) {
    struct json_object *owner = activeProjectObject();
    struct json_object *rules;
    if (owner == NULL)
        owner = settings;
    if (owner == NULL)
        return NULL;
    if (!json_object_object_get_ex(owner, "allow", &rules) ||
        !json_object_is_type(rules, json_type_array)) {
        if (!create)
            return NULL;
        rules = json_object_new_array();
        json_object_object_add(owner, "allow", rules);
    }
    return rules;
}

static BOOL ruleAllows(CONST_STRPTR rule) {
    struct json_object *rules = allowRules(FALSE);
    ULONG count = rules != NULL ? (ULONG)json_object_array_length(rules) : 0;
    ULONG i;
    for (i = 0; i < count; i++) {
        CONST_STRPTR existing =
            json_object_get_string(json_object_array_get_idx(rules, i));
        if (existing != NULL && strcmp(existing, rule) == 0)
            return TRUE;
    }
    return FALSE;
}

/* A one line description of a call for the transcript and the requester */
static STRPTR describeCall(CONST_STRPTR name, struct json_object *arguments) {
    struct AgentBuffer buffer = {NULL, 0, 0};
    CONST_STRPTR detail = "";
    if (strcmp(name, "shell") == 0)
        detail = jsonString(arguments, "command");
    else if (strcmp(name, "arexx") == 0) {
        agentBufferAppend(&buffer, jsonString(arguments, "port"));
        agentBufferAppend(&buffer, ": ");
        detail = jsonString(arguments, "command");
    } else if (strcmp(name, "load_skill") == 0)
        detail = jsonString(arguments, "name");
    else if (strcmp(name, "run_subagent") == 0)
        detail = jsonString(arguments, "agent");
    else if (strcmp(name, "search_files") == 0)
        detail = jsonString(arguments, "pattern");
    else if (strcmp(name, "fetch_url") == 0 ||
             strcmp(name, "download_file") == 0)
        detail = jsonString(arguments, "url");
    else if (strcmp(name, "generate_image") == 0)
        detail = jsonString(arguments, "prompt");
    else if (findBuiltInTool(name) != NULL)
        detail = jsonString(arguments, "path");
    else if (arguments != NULL)
        detail = json_object_to_json_string_ext(
            arguments, JSON_C_TO_STRING_PLAIN | JSON_C_TO_STRING_NOSLASHESCAPE);
    agentBufferAppend(&buffer, detail);
    if (buffer.length > 400) {
        ULONG cut = 400;
        while (cut > 0 && ((UBYTE)buffer.data[cut] & 0xC0) == 0x80)
            cut--;
        buffer.data[cut] = '\0';
        buffer.length = cut;
        agentBufferAppend(&buffer, "...");
    }
    return bufferTake(&buffer);
}

/**
 * Decide whether a call may run, asking the user when the approval mode
 * says so
 * @return TRUE if the call may run
 **/
static BOOL approveCall(CONST_STRPTR name, struct json_object *arguments) {
    AgentToolClass toolClass = toolClassFor(name);
    AgentApprovalMode mode = agentGetApprovalMode();
    UBYTE rule[160];
    struct AgentBuffer body = {NULL, 0, 0};
    STRPTR description;
    STRPTR descriptionLocal;
    STRPTR message;
    ULONG messageSize;
    CONST_STRPTR p;
    LONG answer;

    if (toolClass == AGENT_TOOL_SAFE || mode == AGENT_APPROVAL_FULL_AUTO)
        return TRUE;
    if (mode == AGENT_APPROVAL_READ_ONLY)
        return FALSE;
    if (mode == AGENT_APPROVAL_ACCEPT_EDITS && toolClass == AGENT_TOOL_EDIT)
        return TRUE;
    allowRuleFor(name, arguments, rule, sizeof(rule));
    if (ruleAllows(rule))
        return TRUE;

    description = describeCall(name, arguments);
    descriptionLocal = agentToLocal(description);
    messageSize = strlen(STRING_AGENT_CONFIRM_BODY) + strlen(name) +
                  (descriptionLocal != NULL ? strlen(descriptionLocal) : 0) + 16;
    message = AllocVec(messageSize, MEMF_ANY | MEMF_CLEAR);
    if (message != NULL)
        snprintf(message, messageSize, STRING_AGENT_CONFIRM_BODY, name,
                 descriptionLocal != NULL ? descriptionLocal : "");
    /* The requester treats its text as a format string */
    for (p = message != NULL ? message : ""; *p != '\0'; p++) {
        if (*p == '%')
            agentBufferAppend(&body, "%");
        agentBufferAppendLength(&body, p, 1);
    }
    answer = MUI_Request(app, requesterWindow,
#ifdef __MORPHOS__
                         NULL,
#else
                         MUIV_Requester_Image_Warning,
#endif
                         STRING_AGENT_CONFIRM_TITLE,
                         STRING_AGENT_CONFIRM_BUTTONS,
                         body.data != NULL ? body.data : (STRPTR) "", TAG_DONE);
    agentBufferFree(&body);
    if (message != NULL)
        FreeVec(message);
    if (descriptionLocal != NULL)
        FreeVec(descriptionLocal);
    if (description != NULL)
        FreeVec(description);

    if (answer == 2) {
        struct json_object *rules = allowRules(TRUE);
        if (rules != NULL) {
            json_object_array_add(rules, json_object_new_string(rule));
            saveSettings();
        }
        return TRUE;
    }
    return answer == 1;
}

/* ------------------------------------------------------------------------
 * The loop
 * ---------------------------------------------------------------------- */

static STRPTR runLoop(struct json_object *items, CONST_STRPTR system,
                      struct json_object *tools, CONST_STRPTR model,
                      BOOL isSubagent, AgentOutputFunc output, BOOL *okOut);

static STRPTR toolRunSubagent(struct json_object *arguments,
                              AgentOutputFunc output, BOOL *isError) {
    CONST_STRPTR agentName = jsonString(arguments, "agent");
    CONST_STRPTR prompt = jsonString(arguments, "prompt");
    struct json_object *subagent = findByName(subagents, agentName);
    struct json_object *items;
    struct json_object *item;
    struct json_object *tools;
    CONST_STRPTR model;
    STRPTR system;
    STRPTR report;
    BOOL ok = FALSE;

    if (subagent == NULL && strcasecmp(agentName, "general") != 0 &&
        agentName[0] != '\0') {
        *isError = TRUE;
        return agentDuplicate("Error: there is no subagent with that name.");
    }
    if (prompt[0] == '\0') {
        *isError = TRUE;
        return agentDuplicate("Error: prompt is empty.");
    }

    items = json_object_new_array();
    item = json_object_new_object();
    json_object_object_add(item, "type", json_object_new_string("user"));
    json_object_object_add(item, "text", json_object_new_string(prompt));
    json_object_array_add(items, item);

    {
        /* The general subagent has no file of its own */
        struct json_object *general =
            subagent == NULL ? json_object_new_object() : NULL;
        system = buildSystemPrompt(subagent != NULL ? subagent : general);
        if (general != NULL)
            json_object_put(general);
    }
    tools = buildTools(FALSE, jsonString(subagent, "tools"));
    model = jsonString(subagent, "model");
    if (model[0] == '\0')
        model = agentEffectiveModel();

    report = runLoop(items, system, tools, model, TRUE, output, &ok);
    json_object_put(tools);
    json_object_put(items);
    if (system != NULL)
        FreeVec(system);
    if (!ok)
        *isError = TRUE;
    if (report == NULL || report[0] == '\0') {
        if (report != NULL)
            FreeVec(report);
        return agentDuplicate("The subagent returned no report.");
    }
    return report;
}

static STRPTR executeCall(CONST_STRPTR name, struct json_object *arguments,
                          BOOL isSubagent, AgentOutputFunc output,
                          BOOL *isError) {
    *isError = FALSE;
    if (!approveCall(name, arguments)) {
        *isError = TRUE;
        return agentDuplicate(agentGetApprovalMode() == AGENT_APPROVAL_READ_ONLY
                                  ? "Refused: read-only mode is on, so this "
                                    "tool cannot run."
                                  : "The user refused this action.");
    }
    if (agentMcpOwnsTool(name))
        return agentMcpCall(name, arguments, isError);
    if (strcmp(name, "shell") == 0) {
        CONST_STRPTR command = jsonString(arguments, "command");
        if (command[0] == '\0') {
            *isError = TRUE;
            return agentDuplicate("Error: command is empty.");
        }
        return runShellInProject(command, isError);
    }
    if (strcmp(name, "read_file") == 0)
        return toolReadFile(arguments, isError);
    if (strcmp(name, "write_file") == 0)
        return toolWriteFile(arguments, isError);
    if (strcmp(name, "edit_file") == 0)
        return toolEditFile(arguments, isError);
    if (strcmp(name, "list_directory") == 0)
        return toolListDirectory(arguments, isError);
    if (strcmp(name, "search_files") == 0)
        return toolSearchFiles(arguments, isError);
    if (strcmp(name, "identify_file") == 0)
        return toolIdentifyFile(arguments, isError);
    if (strcmp(name, "fetch_url") == 0)
        return toolFetchUrl(arguments, isError);
    if (strcmp(name, "download_file") == 0)
        return toolDownloadFile(arguments, isError);
    if (strcmp(name, "generate_image") == 0)
        return toolGenerateImage(arguments, isError);
    if (strcmp(name, "list_arexx_ports") == 0)
        return toolListArexxPorts(isError);
    if (strcmp(name, "arexx") == 0)
        return toolArexx(arguments, isError);
    if (strcmp(name, "load_skill") == 0)
        return toolLoadSkill(arguments, isError);
    if (strcmp(name, "run_subagent") == 0 && !isSubagent)
        return toolRunSubagent(arguments, output, isError);
    *isError = TRUE;
    return agentDuplicate("Error: there is no tool with that name.");
}

/* Drop the oldest turns once the history is too big to keep sending */
static void trimHistory(struct json_object *items) {
    while (json_object_array_length(items) > 2) {
        CONST_STRPTR text =
            json_object_to_json_string_ext(items, JSON_C_TO_STRING_PLAIN);
        ULONG count = (ULONG)json_object_array_length(items);
        ULONG next = 1;
        if (text == NULL || strlen(text) <= AGENT_MAX_HISTORY_BYTES)
            return;
        /* Remove a whole turn so no tool result is left without its call */
        while (next < count &&
               strcmp(jsonString(json_object_array_get_idx(items, next),
                                 "type"),
                      "user") != 0)
            next++;
        if (next >= count)
            return;
        json_object_array_del_idx(items, 0, next);
    }
}

static STRPTR runLoop(struct json_object *items, CONST_STRPTR system,
                      struct json_object *tools, CONST_STRPTR model,
                      BOOL isSubagent, AgentOutputFunc output, BOOL *okOut) {
    struct ChatRequestSettings chatSettings;
    LONG maxSteps = settingsGetInt("maxSteps", AGENT_DEFAULT_MAX_STEPS);
    STRPTR finalText = NULL;
    LONG step;

    *okOut = FALSE;
    if (maxSteps < 1)
        maxSteps = AGENT_DEFAULT_MAX_STEPS;

    for (step = 0; step < maxSteps; step++) {
        struct json_object *assistant;
        struct json_object *calls = NULL;
        struct json_object *results;
        struct json_object *toolItem;
        STRPTR error = NULL;
        BOOL paused = FALSE;
        CONST_STRPTR text;
        ULONG callCount;
        ULONG i;

        if (isRequestCancelled())
            return finalText;
        trimHistory(items);
        updateStatusBar(STRING_AGENT_WORKING, yellowPen);
        configGetActiveChatRequestSettings(&chatSettings);
        assistant = agentApiSend(&chatSettings, model, system, items, tools,
                                 agentGetEffort(), &paused, &error);
        if (assistant == NULL) {
            if (!isRequestCancelled() && output != NULL) {
                output("\n");
                output(error != NULL ? error : "The request failed.");
                output("\n");
                outputLocal(output, takeSuppressedError());
                output("\n");
            }
            if (error != NULL)
                FreeVec(error);
            return finalText;
        }
        json_object_array_add(items, assistant);

        text = jsonString(assistant, "text");
        if (text[0] != '\0') {
            if (finalText != NULL)
                FreeVec(finalText);
            finalText = agentDuplicate(text);
            if (output != NULL && !isSubagent) {
                output(text);
                output("\n");
            }
        }

        if (!json_object_object_get_ex(assistant, "calls", &calls))
            calls = NULL;
        callCount = calls != NULL ? (ULONG)json_object_array_length(calls) : 0;
        if (callCount == 0) {
            if (paused)
                continue;
            *okOut = TRUE;
            return finalText;
        }

        results = json_object_new_array();
        for (i = 0; i < callCount; i++) {
            struct json_object *call = json_object_array_get_idx(calls, i);
            struct json_object *arguments = NULL;
            struct json_object *result = json_object_new_object();
            CONST_STRPTR name = jsonString(call, "name");
            BOOL isError = FALSE;
            STRPTR toolOutput;

            json_object_object_get_ex(call, "args", &arguments);
            if (output != NULL) {
                STRPTR description = describeCall(name, arguments);
                output(isSubagent ? "    > " : "> ");
                output(name);
                if (description != NULL && description[0] != '\0') {
                    output(" ");
                    output(description);
                }
                output("\n");
                if (description != NULL)
                    FreeVec(description);
            }
            {
                UBYTE status[160];
                snprintf(status, sizeof(status), STRING_AGENT_RUNNING_TOOL,
                         name);
                updateStatusBar(status, yellowPen);
            }

            if (isRequestCancelled()) {
                toolOutput = agentDuplicate("The user stopped the agent.");
            } else {
                takeSuppressedError();
                toolOutput = limitToolOutput(
                    executeCall(name, arguments, isSubagent, output, &isError));
                /* Say why, when the program reported a reason of its own */
                if (isError) {
                    STRPTR reason = agentToUtf8(takeSuppressedError());
                    if (reason != NULL && reason[0] != '\0') {
                        struct AgentBuffer withReason = {NULL, 0, 0};
                        agentBufferAppend(&withReason,
                                          toolOutput != NULL ? toolOutput : "");
                        agentBufferAppend(&withReason, "\nReason: ");
                        agentBufferAppend(&withReason, reason);
                        if (toolOutput != NULL)
                            FreeVec(toolOutput);
                        toolOutput = bufferTake(&withReason);
                    }
                    if (reason != NULL)
                        FreeVec(reason);
                }
            }

            json_object_object_add(result, "id",
                                   json_object_new_string(jsonString(call, "id")));
            json_object_object_add(result, "name", json_object_new_string(name));
            json_object_object_add(
                result, "output",
                json_object_new_string(toolOutput != NULL ? toolOutput : ""));
            json_object_object_add(result, "isError",
                                   json_object_new_boolean(isError));
            json_object_array_add(results, result);
            if (toolOutput != NULL)
                FreeVec(toolOutput);
        }
        toolItem = json_object_new_object();
        json_object_object_add(toolItem, "type", json_object_new_string("tool"));
        json_object_object_add(toolItem, "results", results);
        json_object_array_add(items, toolItem);
    }

    if (output != NULL && !isSubagent) {
        output("\n");
        outputLocal(output, STRING_AGENT_STEP_LIMIT);
        output("\n");
    }
    return finalText;
}

/* ------------------------------------------------------------------------
 * Session
 * ---------------------------------------------------------------------- */

static void saveSession(void) {
    CONST_STRPTR text;
    if (history == NULL)
        return;
    text = json_object_to_json_string_ext(history, JSON_C_TO_STRING_PLAIN);
    if (text != NULL)
        agentWriteFile(AGENT_SESSION_PATH, text, strlen(text));
}

void agentNewSession(void) {
    if (history != NULL)
        json_object_put(history);
    history = json_object_new_array();
    saveSession();
}

void agentRenderSession(AgentOutputFunc output) {
    ULONG count;
    ULONG i;
    if (history == NULL || output == NULL)
        return;
    count = (ULONG)json_object_array_length(history);
    for (i = 0; i < count; i++) {
        struct json_object *item = json_object_array_get_idx(history, i);
        CONST_STRPTR type = jsonString(item, "type");
        CONST_STRPTR text = jsonString(item, "text");
        if (text[0] == '\0')
            continue;
        if (strcmp(type, "user") == 0) {
            output("\n>>> ");
            output(text);
            output("\n\n");
        } else if (strcmp(type, "assistant") == 0) {
            output(text);
            output("\n");
        }
    }
}

/* ------------------------------------------------------------------------
 * Skill import
 * ---------------------------------------------------------------------- */

struct CopyScan {
    CONST_STRPTR source;
    CONST_STRPTR destination;
    ULONG depth;
};

static void copyEntryFound(CONST_STRPTR name, BOOL isDirectory,
                           APTR userData) {
    struct CopyScan *scan = (struct CopyScan *)userData;
    UBYTE source[AGENT_PATH_LENGTH];
    UBYTE destination[AGENT_PATH_LENGTH];
    joinPath(source, sizeof(source), scan->source, name);
    joinPath(destination, sizeof(destination), scan->destination, name);
    if (isDirectory) {
        struct CopyScan inner;
        if (scan->depth >= AGENT_MAX_SEARCH_DEPTH)
            return;
        ensureDirectory(destination);
        inner.source = source;
        inner.destination = destination;
        inner.depth = scan->depth + 1;
        scanDirectory(source, copyEntryFound, &inner);
    } else {
        copyFile(source, destination);
    }
}

/* Make a name safe to use as a drawer name */
static void sanitizeName(STRPTR name) {
    STRPTR p;
    for (p = name; *p != '\0'; p++) {
        UBYTE c = (UBYTE)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
            *p = '-';
    }
}

BOOL agentImportSkill(CONST_STRPTR source, STRPTR nameOut, ULONG nameSize) {
    UBYTE skillFile[AGENT_PATH_LENGTH];
    UBYTE destination[AGENT_PATH_LENGTH];
    UBYTE name[108];
    STRPTR text = NULL;
    BOOL isUrl;
    BOOL fromDirectory = FALSE;

    if (nameOut != NULL && nameSize > 0)
        nameOut[0] = '\0';
    if (source == NULL || source[0] == '\0')
        return FALSE;
    isUrl = strncmp(source, "http://", 7) == 0 ||
            strncmp(source, "https://", 8) == 0;
    name[0] = '\0';

    if (isUrl) {
        struct ChatRequestSettings chatSettings;
        UBYTE host[256];
        UBYTE path[1024];
        UBYTE url[1280];
        UWORD port = 0;
        BOOL useSSL = TRUE;
        LONG status = 0;
        CONST_STRPTR blob;

        /* A GitHub page address is turned into the address of the file */
        strncpy(url, source, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
        blob = strstr(source, "/blob/");
        if (strncmp(source, "https://github.com/", 19) == 0 && blob != NULL) {
            ULONG repoLength = (ULONG)(blob - (source + 19));
            snprintf(url, sizeof(url), "https://raw.githubusercontent.com/%.*s/%s",
                     (int)repoLength, source + 19, blob + 6);
        }
        if (!parseHttpUrl(url, host, sizeof(host), &port, path, sizeof(path),
                          &useSSL))
            return FALSE;
        configGetActiveChatRequestSettings(&chatSettings);
        text = httpSendRequest(
            "GET", host, port, useSSL, path, "Accept: */*\r\n", NULL,
            chatSettings.useProxy, chatSettings.proxyHost,
            chatSettings.proxyPort, chatSettings.proxyUsesSSL,
            chatSettings.proxyRequiresAuth, chatSettings.proxyUsername,
            chatSettings.proxyPassword, &status, NULL, 0);
        if (text != NULL && status != 200) {
            FreeVec(text);
            text = NULL;
        }
    } else {
        joinPath(skillFile, sizeof(skillFile), source, "SKILL.md");
        if (pathExists(skillFile)) {
            fromDirectory = TRUE;
            strncpy(name, FilePart(source), sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        } else {
            strncpy(skillFile, source, sizeof(skillFile) - 1);
            skillFile[sizeof(skillFile) - 1] = '\0';
        }
        text = agentReadFile(skillFile, NULL);
    }
    if (text == NULL)
        return FALSE;

    /* The skill names itself in its front matter */
    {
        UBYTE value[108];
        if (frontMatterValue(text, "name", value, sizeof(value))) {
            strncpy(name, value, sizeof(name) - 1);
            name[sizeof(name) - 1] = '\0';
        }
    }
    if (name[0] == '\0') {
        FreeVec(text);
        return FALSE;
    }
    sanitizeName(name);

    joinPath(destination, sizeof(destination), AGENT_GLOBAL_SKILLS_DIR, name);
    ensureDirectory(destination);
    if (fromDirectory) {
        struct CopyScan scan;
        scan.source = source;
        scan.destination = destination;
        scan.depth = 0;
        scanDirectory(source, copyEntryFound, &scan);
    } else {
        joinPath(skillFile, sizeof(skillFile), destination, "SKILL.md");
        if (!agentWriteFile(skillFile, text, strlen(text))) {
            FreeVec(text);
            return FALSE;
        }
    }
    FreeVec(text);
    if (nameOut != NULL && nameSize > 0) {
        strncpy(nameOut, name, nameSize - 1);
        nameOut[nameSize - 1] = '\0';
    }
    return TRUE;
}

/* ------------------------------------------------------------------------
 * Slash commands
 * ---------------------------------------------------------------------- */

static void listCatalog(AgentOutputFunc output, struct json_object *array,
                        CONST_STRPTR prefix) {
    struct AgentBuffer buffer = {NULL, 0, 0};
    describeCatalog(&buffer, array, prefix);
    if (buffer.data != NULL)
        output(buffer.data);
    else
        output("-\n");
    agentBufferFree(&buffer);
}

/* Replace $ARGUMENTS in a command file, or add the arguments at the end */
static STRPTR expandCommand(CONST_STRPTR body, CONST_STRPTR arguments) {
    struct AgentBuffer buffer = {NULL, 0, 0};
    CONST_STRPTR p = body;
    CONST_STRPTR match;
    BOOL replaced = FALSE;
    while ((match = strstr(p, "$ARGUMENTS")) != NULL) {
        agentBufferAppendLength(&buffer, p, (ULONG)(match - p));
        agentBufferAppend(&buffer, arguments);
        p = match + 10;
        replaced = TRUE;
    }
    agentBufferAppend(&buffer, p);
    if (!replaced && arguments[0] != '\0') {
        agentBufferAppend(&buffer, "\n\n");
        agentBufferAppend(&buffer, arguments);
    }
    return bufferTake(&buffer);
}

/**
 * Handle a slash command
 * @param promptOut receives an AllocVec'd prompt when the command turns into
 * a task for the model, or NULL when the command was answered here
 **/
static void handleSlashCommand(CONST_STRPTR line, AgentOutputFunc output,
                               STRPTR *promptOut) {
    static CONST_STRPTR approvalNames[] = {"ask", "readonly", "edits", "auto"};
    static CONST_STRPTR effortNames[] = {"default", "low", "medium", "high"};
    UBYTE name[64];
    CONST_STRPTR arguments = line + 1;
    struct json_object *entry;
    ULONG i = 0;

    *promptOut = NULL;
    while (*arguments != '\0' && *arguments != ' ' && *arguments != '\n' &&
           i + 1 < sizeof(name))
        name[i++] = *arguments++;
    name[i] = '\0';
    while (*arguments == ' ' || *arguments == '\n')
        arguments++;

    if (strcasecmp(name, "help") == 0) {
        outputLocal(output, STRING_AGENT_HELP);
        output("\n");
    } else if (strcasecmp(name, "new") == 0 || strcasecmp(name, "clear") == 0) {
        agentNewSession();
        outputLocal(output, STRING_AGENT_SESSION_CLEARED);
        output("\n");
    } else if (strcasecmp(name, "skills") == 0) {
        listCatalog(output, skills, "/");
    } else if (strcasecmp(name, "commands") == 0) {
        listCatalog(output, commands, "/");
    } else if (strcasecmp(name, "agents") == 0) {
        output("- general\n");
        listCatalog(output, subagents, "");
    } else if (strcasecmp(name, "mcp") == 0) {
        struct AgentBuffer buffer = {NULL, 0, 0};
        struct json_object *tools = json_object_new_array();
        agentMcpAppendTools(tools);
        json_object_put(tools);
        agentMcpDescribe(&buffer);
        output(buffer.data != NULL ? buffer.data : "-\n");
        agentBufferFree(&buffer);
    } else if (strcasecmp(name, "projects") == 0) {
        ULONG count = agentProjectCount();
        LONG active = agentGetActiveProject();
        for (i = 0; i < count; i++) {
            STRPTR pathUtf8 = agentToUtf8(agentProjectPath(i));
            output((LONG)i == active ? "* " : "- ");
            output(agentProjectName(i));
            output(" (");
            output(pathUtf8 != NULL ? pathUtf8 : "");
            output(")\n");
            if (pathUtf8 != NULL)
                FreeVec(pathUtf8);
        }
        if (count == 0)
            output("-\n");
    } else if (strcasecmp(name, "project") == 0) {
        ULONG count = agentProjectCount();
        LONG found = -1;
        for (i = 0; i < count; i++) {
            if (strcasecmp(agentProjectName(i), arguments) == 0)
                found = (LONG)i;
        }
        if (found >= 0 || arguments[0] == '\0' ||
            strcasecmp(arguments, "none") == 0) {
            agentSetActiveProject(found);
            output(found >= 0 ? agentProjectName((ULONG)found) : "-");
            output("\n");
        } else {
            outputLocal(output, STRING_AGENT_NO_SUCH_PROJECT);
            output("\n");
        }
    } else if (strcasecmp(name, "approval") == 0) {
        for (i = 0; i < AGENT_APPROVAL_COUNT; i++) {
            if (strcasecmp(arguments, approvalNames[i]) == 0)
                agentSetApprovalMode((AgentApprovalMode)i);
        }
        output(approvalNames[agentGetApprovalMode()]);
        output("\n");
    } else if (strcasecmp(name, "effort") == 0) {
        for (i = 0; i < AGENT_EFFORT_COUNT; i++) {
            if (strcasecmp(arguments, effortNames[i]) == 0)
                agentSetEffort((AgentEffort)i);
        }
        output(effortNames[agentGetEffort()]);
        output("\n");
    } else if (strcasecmp(name, "model") == 0) {
        if (arguments[0] != '\0')
            agentSetModel(strcasecmp(arguments, "default") == 0 ? ""
                                                                 : arguments);
        output(agentEffectiveModel());
        output("\n");
    } else if (strcasecmp(name, "import-skill") == 0) {
        UBYTE skillName[108];
        UBYTE message[256];
        STRPTR sourceLocal = agentToLocal(arguments);
        if (sourceLocal != NULL &&
            agentImportSkill(sourceLocal, skillName, sizeof(skillName)))
            snprintf(message, sizeof(message), STRING_AGENT_SKILL_IMPORTED,
                     skillName);
        else
            snprintf(message, sizeof(message), "%s",
                     STRING_AGENT_SKILL_IMPORT_FAILED);
        if (sourceLocal != NULL)
            FreeVec(sourceLocal);
        outputLocal(output, message);
        output("\n");
    } else if (strcasecmp(name, "init") == 0) {
        UBYTE path[AGENT_PATH_LENGTH];
        struct AgentBuffer buffer = {NULL, 0, 0};
        STRPTR pathUtf8;
        agentInstructionsPath(path, sizeof(path));
        pathUtf8 = agentToUtf8(path);
        agentBufferAppend(
            &buffer,
            "Look through the active project: list its drawers and read the "
            "files that explain what it is and how it is built or used. Then "
            "write the file ");
        agentBufferAppend(&buffer, pathUtf8 != NULL ? pathUtf8 : "");
        agentBufferAppend(
            &buffer,
            " as short instructions for an agent working here: what the "
            "project is, how it is laid out, the commands to build, run or "
            "test it, and any rules the files show must be followed. Keep "
            "what is already in that file if it is still right. ");
        agentBufferAppend(&buffer, arguments);
        if (pathUtf8 != NULL)
            FreeVec(pathUtf8);
        *promptOut = bufferTake(&buffer);
    } else if (strcasecmp(name, "create-skill") == 0) {
        struct AgentBuffer buffer = {NULL, 0, 0};
        agentBufferAppend(
            &buffer,
            "Create a new skill. A skill is a drawer inside "
            AGENT_GLOBAL_SKILLS_DIR
            " named after the skill (lower case letters, digits and hyphens) "
            "holding a file SKILL.md. That file starts with a front matter "
            "block:\n---\nname: the-skill-name\ndescription: one or two "
            "sentences saying what the skill does and when to use it\n---\n"
            "followed by the instructions an agent should follow, written "
            "as clear steps, with the exact commands and paths it needs. "
            "Longer reference material can go in further files in the same "
            "drawer, which SKILL.md mentions by name. Write the skill with "
            "write_file, then tell the user its name. The skill to create: ");
        agentBufferAppend(&buffer, arguments);
        *promptOut = bufferTake(&buffer);
    } else if ((entry = findByName(commands, name)) != NULL) {
        STRPTR text = readTextFileUtf8(jsonString(entry, "file"));
        if (text != NULL) {
            *promptOut = expandCommand(frontMatterBody(text), arguments);
            FreeVec(text);
        }
    } else if ((entry = findByName(skills, name)) != NULL) {
        struct AgentBuffer buffer = {NULL, 0, 0};
        agentBufferAppend(&buffer, "Load the skill \"");
        agentBufferAppend(&buffer, jsonString(entry, "name"));
        agentBufferAppend(&buffer, "\" with load_skill and follow it. ");
        agentBufferAppend(&buffer, arguments);
        *promptOut = bufferTake(&buffer);
    } else {
        UBYTE message[256];
        snprintf(message, sizeof(message), STRING_AGENT_UNKNOWN_COMMAND, name);
        outputLocal(output, message);
        output("\n");
    }
}

BOOL agentRun(CONST_STRPTR prompt, AgentOutputFunc output) {
    struct ChatRequestSettings chatSettings;
    UBYTE projectMcpPath[AGENT_PATH_LENGTH];
    CONST_STRPTR project;
    STRPTR expanded = NULL;
    STRPTR system;
    STRPTR finalText;
    struct json_object *tools;
    struct json_object *item;
    BOOL ok = FALSE;

    if (prompt == NULL || settings == NULL)
        return FALSE;
    while (*prompt == ' ' || *prompt == '\n')
        prompt++;
    if (*prompt == '\0')
        return FALSE;
    /* The GUI keeps handling events while a tool runs, so an ARexx AGENT
     * command could arrive in the middle of a turn */
    if (agentRunning) {
        if (output != NULL)
            output("The agent is already working on a task.\n");
        return FALSE;
    }
    agentRunning = TRUE;
    if (history == NULL)
        history = json_object_new_array();

    loadCatalog();
    project = activeProjectPath();
    configGetActiveChatRequestSettings(&chatSettings);
    projectMcpPath[0] = '\0';
    if (project[0] != '\0') {
        UBYTE directory[AGENT_PATH_LENGTH];
        joinPath(directory, sizeof(directory), project, ".agents");
        joinPath(projectMcpPath, sizeof(projectMcpPath), directory, "mcp.json");
    }
    agentMcpLoad(AGENT_GLOBAL_MCP_PATH,
                 projectMcpPath[0] != '\0' ? (CONST_STRPTR)projectMcpPath
                                           : NULL,
                 &chatSettings);

    if (prompt[0] == '/') {
        handleSlashCommand(prompt, output, &expanded);
        if (expanded == NULL) {
            updateStatusBar(STRING_READY, greenPen);
            agentRunning = FALSE;
            return TRUE;
        }
        prompt = expanded;
    }

    item = json_object_new_object();
    json_object_object_add(item, "type", json_object_new_string("user"));
    json_object_object_add(item, "text", json_object_new_string(prompt));
    json_object_array_add(history, item);

    setErrorRequestersSuppressed(TRUE);
    system = buildSystemPrompt(NULL);
    tools = buildTools(TRUE, NULL);
    finalText = runLoop(history, system, tools, agentEffectiveModel(), FALSE,
                        output, &ok);
    setErrorRequestersSuppressed(FALSE);
    json_object_put(tools);
    if (system != NULL)
        FreeVec(system);
    if (finalText != NULL)
        FreeVec(finalText);
    if (expanded != NULL)
        FreeVec(expanded);

    if (isRequestCancelled() && output != NULL) {
        output("\n");
        outputLocal(output, STRING_AGENT_STOPPED);
        output("\n");
    }
    saveSession();
    updateStatusBar(ok ? STRING_READY : STRING_ERROR, ok ? greenPen : redPen);
    agentRunning = FALSE;
    return ok;
}

/* ------------------------------------------------------------------------
 * Start and end
 * ---------------------------------------------------------------------- */

LONG agentInit(void) {
    STRPTR text;

    ensureDirectory(AGENT_DIR);
    ensureDirectory(AGENT_GLOBAL_SKILLS_DIR);
    ensureDirectory(AGENT_GLOBAL_COMMANDS_DIR);
    ensureDirectory(AGENT_GLOBAL_AGENTS_DIR);

    text = agentReadFile(AGENT_SETTINGS_PATH, NULL);
    if (text != NULL) {
        settings = json_tokener_parse(text);
        FreeVec(text);
    }
    if (settings == NULL || !json_object_is_type(settings, json_type_object)) {
        if (settings != NULL)
            json_object_put(settings);
        settings = json_object_new_object();
        json_object_object_add(settings, "approvalMode",
                               json_object_new_int(AGENT_APPROVAL_ASK));
        json_object_object_add(settings, "effort",
                               json_object_new_int(AGENT_EFFORT_DEFAULT));
        json_object_object_add(settings, "model", json_object_new_string(""));
        json_object_object_add(settings, "maxSteps",
                               json_object_new_int(AGENT_DEFAULT_MAX_STEPS));
        json_object_object_add(settings, "activeProject",
                               json_object_new_string(""));
        json_object_object_add(settings, "projects", json_object_new_array());
        json_object_object_add(settings, "allow", json_object_new_array());
    }

    text = agentReadFile(AGENT_SESSION_PATH, NULL);
    if (text != NULL) {
        history = json_tokener_parse(text);
        FreeVec(text);
    }
    if (history == NULL || !json_object_is_type(history, json_type_array)) {
        if (history != NULL)
            json_object_put(history);
        history = json_object_new_array();
    }
    return settings != NULL && history != NULL ? RETURN_OK : RETURN_ERROR;
}

void agentShutdown(void) {
    saveSession();
    agentMcpFree();
    if (history != NULL)
        json_object_put(history);
    if (skills != NULL)
        json_object_put(skills);
    if (commands != NULL)
        json_object_put(commands);
    if (subagents != NULL)
        json_object_put(subagents);
    if (settings != NULL)
        json_object_put(settings);
    history = NULL;
    skills = NULL;
    commands = NULL;
    subagents = NULL;
    settings = NULL;
}
