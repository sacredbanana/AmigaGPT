#ifndef BESTTEXTEDITOR_VERSION_H
#define BESTTEXTEDITOR_VERSION_H

/* Shared by BestTextEditor.mcc and BestTextEditor.mcp.
 *
 * Keep LIB_VERSION at 20 or above. MUI opens preference classes with a
 * minimum version, and exec refuses a disk library whose RomTag version is
 * lower without running any of its code, so a .mcp below that never shows
 * up in MUI preferences. */
#define LIB_VERSION 20
#define LIB_REVISION 0
#define LIB_DATE "16.11.2026"
#define LIB_REV_STRING "20.0"
#define LIB_COPYRIGHT "Copyright (C) 2026 Cameron Armstrong"

#endif
