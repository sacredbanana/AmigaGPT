#ifndef MORPHOS_RELAUNCH_H
#define MORPHOS_RELAUNCH_H

#include <exec/types.h>

/*
 * MorphOS Workbench quit/restart coordination (timeboxed).
 * Normal use rarely restarts within seconds; stress tests and WB double-launch
 * can overlap MUI_DisposeObject with the next ApplicationObject.
 */

#ifdef __MORPHOS__

/** ~3 s at 50 Hz — MUI/Intuition teardown after MUI_DisposeObject(app). */
#define MORPHOS_RELAUNCH_POST_DISPOSE_DELAY 150

/** Extra pause after teardown wait, before initVideo. */
#define MORPHOS_RELAUNCH_STARTUP_DELAY 75

/** Wait while previous instance wrote teardown marker (up to ~6 s). */
#define MORPHOS_RELAUNCH_TEARDOWN_POLL_DELAY 10
#define MORPHOS_RELAUNCH_TEARDOWN_POLL_MAX 60

/**
 * Call at startup (after "startup begin" log). Waits out teardown, blocks if
 * another AmigaGPT task is still alive. Returns FALSE to exit main early.
 * On failure shows an Intuition requester (before MUI app exists).
 */
BOOL morphosRelaunchStartupGuard(void);

/** Intuition alert before MUI ApplicationObject (MorphOS WB start failures). */
void morphosStartupShowAlert(CONST_STRPTR body);

/** Call at start of shutdownGUI before MUI_DisposeObject. */
void morphosRelaunchShutdownBegin(void);

/** Release T: instance lock (also called early from morphosRelaunchShutdownBegin). */
void morphosRelaunchReleaseInstanceLock(void);

/** Call after post-dispose Delay in shutdownGUI. */
void morphosRelaunchShutdownEnd(void);

#else

#define MORPHOS_RELAUNCH_POST_DISPOSE_DELAY 0
#define MORPHOS_RELAUNCH_STARTUP_DELAY 0

static inline BOOL morphosRelaunchStartupGuard(void) { return TRUE; }
static inline void morphosRelaunchShutdownBegin(void) {}
static inline void morphosRelaunchReleaseInstanceLock(void) {}
static inline void morphosRelaunchShutdownEnd(void) {}
static inline void morphosStartupShowAlert(CONST_STRPTR body) { (void)body; }

#endif

#endif /* MORPHOS_RELAUNCH_H */
