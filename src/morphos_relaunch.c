/*
 * MorphOS-only WB relaunch guards (see morphos_relaunch.h).
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/execbase.h>
#include <stdio.h>
#include <string.h>

#include "morphos_relaunch.h"
#include "streamlog.h"

#ifdef __MORPHOS__

/* Runtime locks on T: only (RAM, gone on reboot). No AMIGAGPT: fallback. */
#define MORPHOS_RELAUNCH_LOCK_PATH "T:amigagpt_instance.lock"
#define MORPHOS_RELAUNCH_TEARDOWN_PATH "T:amigagpt_teardown.lock"
#define MORPHOS_RELAUNCH_LEGACY_LOCK_PATH "AMIGAGPT:amigagpt_instance.lock"
#define MORPHOS_RELAUNCH_LEGACY_TEARDOWN_PATH "AMIGAGPT:amigagpt_teardown.lock"

static void morphosLogLockOp(CONST_STRPTR action, CONST_STRPTR path, BOOL ok) {
    UBYTE line[112];

    if (path == NULL) {
        path = "?";
    }
    if (action == NULL) {
        action = "?";
    }
    if (ok) {
        snprintf((STRPTR)line, sizeof(line), "relaunch lock %s %s ok", action,
                 path);
    } else {
        snprintf((STRPTR)line, sizeof(line), "relaunch lock %s %s fail io=%ld",
                 action, path, (long)IoErr());
    }
    streamLogLifecycle((STRPTR)line);
}

static BOOL morphosDeleteLockFile(CONST_STRPTR path) {
    BOOL ok;

    if (path == NULL) {
        return FALSE;
    }
    if (!DeleteFile((STRPTR)path)) {
        ok = IoErr() == ERROR_OBJECT_NOT_FOUND;
    } else {
        ok = TRUE;
    }
    morphosLogLockOp("delete", path, ok);
    return ok;
}

static BOOL morphosLockFileExists(CONST_STRPTR path) {
    BPTR fh;

    if (path == NULL) {
        return FALSE;
    }
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (fh == 0) {
        return FALSE;
    }
    Close(fh);
    return TRUE;
}

static BOOL morphosCreateLockFile(CONST_STRPTR path) {
    BPTR fh;

    if (path == NULL) {
        return FALSE;
    }
    morphosDeleteLockFile(path);
    fh = Open((STRPTR)path, MODE_NEWFILE);
    if (fh == 0) {
        morphosLogLockOp("create", path, FALSE);
        return FALSE;
    }
    Close(fh);
    morphosLogLockOp("create", path, TRUE);
    return TRUE;
}

static void morphosDeleteAllRelaunchLocks(void) {
    morphosDeleteLockFile(MORPHOS_RELAUNCH_TEARDOWN_PATH);
    morphosDeleteLockFile(MORPHOS_RELAUNCH_LOCK_PATH);
}

/** Drop obsolete locks from earlier builds that used AMIGAGPT: as mirror. */
static void morphosDeleteLegacyProgramDirLocks(void) {
    morphosDeleteLockFile(MORPHOS_RELAUNCH_LEGACY_TEARDOWN_PATH);
    morphosDeleteLockFile(MORPHOS_RELAUNCH_LEGACY_LOCK_PATH);
}

static BOOL morphosTempAssignUsable(void) {
    BPTR fh;
    CONST_STRPTR probePath = "T:amigagpt_t_probe";

    fh = Open((STRPTR)probePath, MODE_NEWFILE);
    if (fh == 0) {
        morphosLogLockOp("probe", "T:", FALSE);
        return FALSE;
    }
    Close(fh);
    morphosDeleteLockFile(probePath);
    morphosLogLockOp("probe", "T:", TRUE);
    return TRUE;
}

#define MORPHOS_INSTANCE_LOCK_MAGIC 0x41475254UL /* 'AGRT' */

struct MorphosInstanceLockData {
    ULONG magic;
    APTR taskPtr;
};

/** True if task is on Exec ready/wait lists (not self). */
static BOOL morphosTaskInExecLists(struct List *list, struct Task *task) {
    struct Node *n;

    for (n = list->lh_Head; n->ln_Succ != (struct Node *)&list->lh_Tail;
         n = n->ln_Succ) {
        if ((struct Task *)n == task) {
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL morphosTaskStillRunning(struct Task *task, struct Task *self) {
    BOOL alive;

    if (task == NULL || task == self) {
        return FALSE;
    }
    Forbid();
    alive = (SysBase->ThisTask == task) ||
            morphosTaskInExecLists(&SysBase->TaskReady, task) ||
            morphosTaskInExecLists(&SysBase->TaskWait, task);
    Permit();
    return alive;
}

static BOOL morphosReadInstanceLock(struct MorphosInstanceLockData *out) {
    BPTR fh;
    LONG got;

    if (out == NULL) {
        return FALSE;
    }
    memset(out, 0, sizeof(*out));
    fh = Open((STRPTR)MORPHOS_RELAUNCH_LOCK_PATH, MODE_OLDFILE);
    if (fh == 0) {
        return FALSE;
    }
    got = Read(fh, out, (LONG)sizeof(*out));
    Close(fh);
    if (got != (LONG)sizeof(*out) || out->magic != MORPHOS_INSTANCE_LOCK_MAGIC ||
        out->taskPtr == NULL) {
        return FALSE;
    }
    return TRUE;
}

static BOOL morphosWriteInstanceLock(void) {
    struct MorphosInstanceLockData data;
    struct Task *self = FindTask(NULL);
    BPTR fh;
    LONG wrote;

    memset(&data, 0, sizeof(data));
    data.magic = MORPHOS_INSTANCE_LOCK_MAGIC;
    data.taskPtr = (APTR)self;

    morphosDeleteLockFile(MORPHOS_RELAUNCH_LOCK_PATH);
    fh = Open((STRPTR)MORPHOS_RELAUNCH_LOCK_PATH, MODE_NEWFILE);
    if (fh == 0) {
        morphosLogLockOp("write", MORPHOS_RELAUNCH_LOCK_PATH, FALSE);
        return FALSE;
    }
    wrote = Write(fh, &data, (LONG)sizeof(data));
    Close(fh);
    if (wrote != (LONG)sizeof(data)) {
        morphosLogLockOp("write", MORPHOS_RELAUNCH_LOCK_PATH, FALSE);
        return FALSE;
    }
    morphosLogLockOp("write", MORPHOS_RELAUNCH_LOCK_PATH, TRUE);
    return TRUE;
}

static BOOL morphosAnotherInstanceRunning(void) {
    struct MorphosInstanceLockData data;
    struct Task *self = FindTask(NULL);

    if (!morphosReadInstanceLock(&data)) {
        return FALSE;
    }
    return morphosTaskStillRunning((struct Task *)data.taskPtr, self);
}

/** Remove T: lock files left after crash/freeze (no live peer task in lock). */
static void morphosPurgeStaleRelaunchLocks(void) {
    if (morphosAnotherInstanceRunning()) {
        return;
    }
    if (morphosLockFileExists(MORPHOS_RELAUNCH_LOCK_PATH) ||
        morphosLockFileExists(MORPHOS_RELAUNCH_TEARDOWN_PATH)) {
        morphosDeleteAllRelaunchLocks();
        streamLogLifecycle("startup cleared stale relaunch locks on T");
    }
}

static void morphosWaitForTeardownMarker(void) {
    ULONG polls = 0;

    while (polls < MORPHOS_RELAUNCH_TEARDOWN_POLL_MAX) {
        if (!morphosLockFileExists(MORPHOS_RELAUNCH_TEARDOWN_PATH)) {
            if (polls > 0) {
                streamLogLifecycle("startup teardown wait done");
            }
            return;
        }
        if (polls == 0) {
            streamLogLifecycle("startup teardown wait begin");
        }
        Delay(MORPHOS_RELAUNCH_TEARDOWN_POLL_DELAY);
        polls++;
    }
    if (morphosAnotherInstanceRunning()) {
        streamLogLifecycle("startup teardown wait timeout peer alive");
        return;
    }
    streamLogLifecycle("startup teardown wait timeout stale");
    morphosDeleteLockFile(MORPHOS_RELAUNCH_TEARDOWN_PATH);
}

BOOL morphosRelaunchStartupGuard(void) {
    morphosDeleteLegacyProgramDirLocks();

    if (!morphosTempAssignUsable()) {
        streamLogLifecycle(
            "relaunch guards skipped T assign unusable (no lock files)");
        Delay(MORPHOS_RELAUNCH_STARTUP_DELAY);
        streamLogLifecycle("startup post-relaunch cooldown done");
        return TRUE;
    }

    morphosPurgeStaleRelaunchLocks();

    if (!morphosAnotherInstanceRunning()) {
        morphosWaitForTeardownMarker();
    } else {
        streamLogLifecycle("startup teardown wait skipped peer running");
    }

    if (morphosAnotherInstanceRunning()) {
        streamLogLifecycle("startup blocked another instance");
        return FALSE;
    }

    if (!morphosWriteInstanceLock()) {
        streamLogLifecycle("startup instance lock unavailable on T");
        /* Non-fatal: continue without lock. */
    } else {
        streamLogLifecycle("startup instance lock ok");
    }

    Delay(MORPHOS_RELAUNCH_STARTUP_DELAY);
    streamLogLifecycle("startup post-relaunch cooldown done");
    return TRUE;
}

void morphosRelaunchShutdownBegin(void) {
    streamLogLifecycle("shutdown relaunch teardown marker begin");
    if (!morphosTempAssignUsable()) {
        streamLogLifecycle("shutdown relaunch teardown skipped T unusable");
        return;
    }
    /*
     * Drop instance lock before long MUI dispose — FindTask(name) falsely blocks
     * the next start while this task is still alive in Delay(post-dispose).
     */
    morphosRelaunchReleaseInstanceLock();
    streamLogShutdownPhase("instance lock released early");
    if (morphosCreateLockFile(MORPHOS_RELAUNCH_TEARDOWN_PATH)) {
        streamLogLifecycle("shutdown relaunch teardown marker done");
    } else {
        streamLogLifecycle("shutdown relaunch teardown marker fail");
    }
}

void morphosRelaunchReleaseInstanceLock(void) {
    if (morphosTempAssignUsable()) {
        morphosDeleteLockFile(MORPHOS_RELAUNCH_LOCK_PATH);
    } else {
        streamLogLifecycle("shutdown instance lock release skipped T unusable");
    }
    streamLogLifecycle("shutdown instance lock released");
}

void morphosRelaunchShutdownEnd(void) {
    ULONG attempt;

    if (morphosTempAssignUsable()) {
        for (attempt = 0; attempt < 3; attempt++) {
            morphosDeleteAllRelaunchLocks();
            if (!morphosLockFileExists(MORPHOS_RELAUNCH_LOCK_PATH) &&
                !morphosLockFileExists(MORPHOS_RELAUNCH_TEARDOWN_PATH)) {
                break;
            }
            if (attempt < 2) {
                Delay(2);
            }
        }
        if (morphosLockFileExists(MORPHOS_RELAUNCH_LOCK_PATH) ||
            morphosLockFileExists(MORPHOS_RELAUNCH_TEARDOWN_PATH)) {
            streamLogLifecycle("shutdown relaunch locks delete incomplete");
            streamLogShutdownPhase("relaunch locks remain");
        }
    } else {
        streamLogLifecycle("shutdown relaunch locks release skipped T unusable");
        streamLogShutdownPhase("relaunch locks skipped T unusable");
    }
    streamLogLifecycle("shutdown relaunch locks released");
    streamLogShutdownPhase("relaunch locks released");
}

#endif /* __MORPHOS__ */
