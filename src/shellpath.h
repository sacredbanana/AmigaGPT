#ifndef SHELLPATH_H
#define SHELLPATH_H

#include <dos/dos.h>
#include <utility/tagitem.h>

/**
 * Run a command through SystemTagList() with a proper Shell command search
 * path.
 *
 * A program started from Workbench has no CLI, so a Shell spawned with
 * SystemTagList() would otherwise only search the current directory and C:.
 * This gives the new Shell the same path a Shell opened from Workbench would
 * have (the caller's own path when it was started from a Shell), plus a few
 * system drawers as a safety net so RX, TS and friends are always found.
 *
 * @param command the command line to run
 * @param tags extra SystemTagList()/CreateNewProc() tags, or NULL
 * @return the SystemTagList() result
 **/
LONG runShellCommand(CONST_STRPTR command, struct TagItem *tags);

#endif
