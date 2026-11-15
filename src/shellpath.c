#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <workbench/startup.h>
#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
#include <proto/wb.h>
#include <workbench/workbench.h>
#endif
#ifdef __AMIGAOS4__
#include <dos/path.h>
#endif
#include "shellpath.h"

/* Set in main() when the program was started from Workbench */
extern struct WBStartup *wbStartupMessage;

/* A Shell search path is a BPTR-linked list of these nodes. OS4 calls it
 * struct PathNode; the NDK does not name it, so use our own definition. */
struct ShellPathNode {
    BPTR next; /* BPTR to the next node, or 0 */
    BPTR lock; /* Shared lock on a directory to search */
};

#define NEXT_PATH_NODE(n) ((struct ShellPathNode *)BADDR((n)->next))

#ifdef __AMIGAOS4__
#define CLI_PATH(cli) ((cli)->cli_PathList)
#else
#define CLI_PATH(cli) ((cli)->cli_CommandDir)
#endif

/* Upper bound on the number of entries copied from another process' path */
#define MAX_LAUNCHER_PATH_ENTRIES 64

/* Appended after the inherited path (unless already present) so the commands
 * AmigaGPT itself relies on, such as RX, TS and TCO, are always found even
 * when the inherited path is empty. The current directory and C: are always
 * searched by the Shell and must not be added here. */
static CONST_STRPTR fallbackDirs[] = {
#ifdef __MORPHOS__
    "SYS:Utilities", "MOSSYS:C", "SYS:S", "MOSSYS:S",
#else
    "SYS:Utilities", "SYS:Rexxc", "SYS:System",
    "SYS:S",         "SYS:Prefs", "SYS:Tools",
    "SYS:Tools/Commodities",
#endif
    NULL};

/**
 * Append a copy of a lock to the end of a path list
 * @param head pointer to the BPTR head of the list, updated as needed
 * @param lock the directory lock to add. It is duplicated, not consumed.
 * @return TRUE on success, FALSE if the entry could not be added
 **/
static BOOL appendPathLock(BPTR *head, BPTR lock) {
    if (lock == 0)
        return FALSE;

#ifdef __AMIGAOS4__
    /* Use the DOS path API so the list is allocated the way DOS frees it.
     * Both functions duplicate the lock themselves. */
    struct PathNode *first = (struct PathNode *)BADDR(*head);
    struct PathNode *result =
        first == NULL ? AllocateCmdPathList(lock)
                      : AddCmdPathNode(first, lock, ADDCMDPATHNODE_TAIL);
    if (result == NULL)
        return FALSE;
    *head = MKBADDR(result);
    return TRUE;
#else
    /* DOS frees the nodes with FreeVec() when the Shell exits */
    struct ShellPathNode *node;
    BPTR duplicate = DupLock(lock);
    if (duplicate == 0)
        return FALSE;

    node = AllocVec(sizeof(struct ShellPathNode), MEMF_PUBLIC | MEMF_CLEAR);
    if (node == NULL) {
        UnLock(duplicate);
        return FALSE;
    }
    node->lock = duplicate;

    if (*head == 0) {
        *head = MKBADDR(node);
    } else {
        struct ShellPathNode *last = (struct ShellPathNode *)BADDR(*head);
        while (last->next != 0)
            last = NEXT_PATH_NODE(last);
        last->next = MKBADDR(node);
    }
    return TRUE;
#endif
}

/**
 * Free a path list that was not handed over to a new process
 * @param head the BPTR head of the list
 **/
static void freePath(BPTR head) {
    if (head == 0)
        return;
#ifdef __AMIGAOS4__
    FreeCmdPathList((struct PathNode *)BADDR(head));
#else
    struct ShellPathNode *node = (struct ShellPathNode *)BADDR(head);
    while (node != NULL) {
        struct ShellPathNode *next = NEXT_PATH_NODE(node);
        if (node->lock)
            UnLock(node->lock);
        FreeVec(node);
        node = next;
    }
#endif
}

/**
 * Check whether a path list already contains a directory
 * @param head the BPTR head of the list
 * @param lock a lock on the directory to look for
 * @return TRUE if the directory is already in the list
 **/
static BOOL pathContains(BPTR head, BPTR lock) {
    struct ShellPathNode *node;
    for (node = (struct ShellPathNode *)BADDR(head); node != NULL;
         node = NEXT_PATH_NODE(node)) {
        if (node->lock && SameLock(node->lock, lock) == LOCK_SAME)
            return TRUE;
    }
    return FALSE;
}

/**
 * Append copies of every entry of a path list to another list
 * @param head pointer to the BPTR head of the destination list
 * @param source the BPTR head of the list to copy
 **/
static void appendPath(BPTR *head, BPTR source) {
    struct ShellPathNode *node;
    for (node = (struct ShellPathNode *)BADDR(source); node != NULL;
         node = NEXT_PATH_NODE(node)) {
        appendPathLock(head, node->lock);
    }
}

#ifndef __AMIGAOS4__
/**
 * Copy the search path of the process that started us from Workbench. This
 * is the Workbench process itself (Ambient on MorphOS), whose path is the one
 * every Shell opened from Workbench starts with. Only used when
 * workbench.library cannot hand us a copy (MorphOS, or Workbench older than
 * V44 on AmigaOS 3).
 * @return a new path list, or 0 if none could be found
 **/
static BPTR copyLauncherPath(void) {
    BPTR locks[MAX_LAUNCHER_PATH_ENTRIES];
    LONG count = 0;
    LONG i;
    BPTR path = 0;
    struct MsgPort *port;

    if (wbStartupMessage == NULL)
        return 0;
    port = wbStartupMessage->sm_Message.mn_ReplyPort;
    if (port == NULL)
        return 0;

    /* Collect the locks without calling DOS, which could break the Forbid().
     * DupLock() further down does break it, but the Workbench path is set
     * once at boot and practically never changes afterwards. */
    Forbid();
    if ((port->mp_Flags & PF_ACTION) == PA_SIGNAL && port->mp_SigTask != NULL) {
        struct Process *launcher = (struct Process *)port->mp_SigTask;
        if (launcher->pr_Task.tc_Node.ln_Type == NT_PROCESS &&
            launcher->pr_CLI != 0) {
            struct CommandLineInterface *cli =
                (struct CommandLineInterface *)BADDR(launcher->pr_CLI);
            struct ShellPathNode *node;
            for (node = (struct ShellPathNode *)BADDR(CLI_PATH(cli));
                 node != NULL && count < MAX_LAUNCHER_PATH_ENTRIES;
                 node = NEXT_PATH_NODE(node)) {
                if (node->lock)
                    locks[count++] = node->lock;
            }
        }
    }
    Permit();

    for (i = 0; i < count; i++)
        appendPathLock(&path, locks[i]);

    return path;
}
#endif

/**
 * Get a copy of the search path a Shell opened from Workbench would have
 * @return a new path list, or 0 if none could be found
 **/
static BPTR duplicateWorkbenchPath(void) {
    BPTR path = 0;

#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
    /* WBCTRLA_DuplicateSearchPath is the supported way to do this (V44) */
    struct Library *WorkbenchBase = OpenLibrary("workbench.library", 44);
    if (WorkbenchBase != NULL) {
#ifdef __AMIGAOS4__
        struct WorkbenchIFace *IWorkbench = (struct WorkbenchIFace *)GetInterface(
            WorkbenchBase, "main", 1, NULL);
        if (IWorkbench != NULL) {
#endif
            if (!WorkbenchControl(NULL, WBCTRLA_DuplicateSearchPath,
                                  (ULONG)&path, TAG_DONE))
                path = 0;
#ifdef __AMIGAOS4__
            DropInterface((struct Interface *)IWorkbench);
        }
#endif
        CloseLibrary(WorkbenchBase);
    }
#endif

#ifndef __AMIGAOS4__
    if (path == 0)
        path = copyLauncherPath();
#endif

    return path;
}

/**
 * Build the search path for a new Shell: our own path when we were started
 * from a Shell, otherwise the Workbench path, followed by the fallback
 * directories that are not in it yet
 * @return a new path list, or 0 if it is empty
 **/
static BPTR buildCommandPath(void) {
    BPTR path = 0;
    struct CommandLineInterface *cli = Cli();
    CONST_STRPTR const *dir;

    if (cli != NULL && CLI_PATH(cli) != 0)
        appendPath(&path, CLI_PATH(cli));
    else
        path = duplicateWorkbenchPath();

    for (dir = fallbackDirs; *dir != NULL; dir++) {
        BPTR lock = Lock(*dir, SHARED_LOCK);
        if (lock == 0)
            continue;
        if (!pathContains(path, lock))
            appendPathLock(&path, lock);
        UnLock(lock);
    }

    return path;
}

LONG runShellCommand(CONST_STRPTR command, struct TagItem *tags) {
    struct TagItem pathTags[2];
    BPTR path = buildCommandPath();
    LONG result;

    if (path == 0)
        return SystemTagList((STRPTR)command, tags);

    pathTags[0].ti_Tag = NP_Path;
    pathTags[0].ti_Data = (ULONG)path;
    pathTags[1].ti_Tag = tags != NULL ? TAG_MORE : TAG_DONE;
    pathTags[1].ti_Data = (ULONG)tags;

    result = SystemTagList((STRPTR)command, pathTags);

    /* On success the new Shell owns the path and frees it when it exits */
    if (result == -1)
        freePath(path);

    return result;
}
