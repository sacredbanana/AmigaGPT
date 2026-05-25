#ifndef CHATOUTPUTSCINTILLA_H
#define CHATOUTPUTSCINTILLA_H

#ifdef __MORPHOS__

#include <libraries/mui.h>

/** Read-only chat output: UTF-8, TTEngine, font from config.fixedWidthFonts. */
void chatOutputScintillaInitViewer(Object *sci);

/** Replace document text; scroll caret into view. */
void chatOutputScintillaSetUtf8Text(Object *sci, const char *utf8);

/** Reapply TTEngine font from config.fixedWidthFonts (no full window rebuild). */
void chatOutputScintillaRefreshFont(Object *sci);

#endif /* __MORPHOS__ */

#endif
