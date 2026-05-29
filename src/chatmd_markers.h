#ifndef CHATMD_MARKERS_H
#define CHATMD_MARKERS_H

#ifdef CHATMD_MARKERS_HOST
#include <stddef.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef unsigned long ULONG;
#else
#include <exec/types.h>
#endif

/**
 * CommonMark-style: may this `*` open emphasis (left-flanking + heuristics)?
 * Use with stack toggle: pass closing=FALSE when not already italic.
 */
BOOL chatMdItalicStarCanOpen(const char *input, ULONG pos, ULONG len);

/** CommonMark-style: may this `*` close emphasis (right-flanking + heuristics)? */
BOOL chatMdItalicStarCanClose(const char *input, ULONG pos, ULONG len);

/** `**` at pos (two asterisks); use with stack: open when !boldActive, close when boldActive. */
BOOL chatMdBoldDoubleStarCanOpen(const char *input, ULONG pos, ULONG len);
BOOL chatMdBoldDoubleStarCanClose(const char *input, ULONG pos, ULONG len);

#endif
