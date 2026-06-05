#ifndef CHATFINDSCINTILLA_H
#define CHATFINDSCINTILLA_H

#ifdef __MORPHOS__

#include <libraries/mui.h>

/** Build find bar MUI objects (call once before createMainWindow layout). */
BOOL chatFindScintillaInit(void);

Object *chatFindScintillaGetBar(void);

void chatFindScintillaSetContext(Object *mainWindow, Object *chatSci);

BOOL chatFindScintillaWireToApp(Object *app);

void chatFindScintillaShow(Object *chatSci);
void chatFindScintillaHide(void);

BOOL chatFindScintillaNext(Object *chatSci);
BOOL chatFindScintillaPrev(Object *chatSci);

void chatFindScintillaOnStringFocus(void);
void chatFindScintillaOnEditorFocus(void);
void chatFindScintillaUpdateCounter(Object *chatSci);

/** Index user-message starts (style byte CHAT_OUTPUT_STYLE_USER). */
void chatUserNavRebuild(const UBYTE *displayStyles, ULONG displayLen);
void chatUserNavClear(void);

BOOL chatUserNavNext(Object *chatSci);
BOOL chatUserNavPrev(Object *chatSci);

#endif /* __MORPHOS__ */

#endif
