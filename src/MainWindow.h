#include <proto/dos.h>
#include <intuition/intuition.h>

extern struct Window *mainWindow;
extern Object *mainWindowObject;
extern Object *newChatButton;
extern Object *deleteChatButton;
extern Object *sendMessageButton;
extern Object *chatInputTextEditor;
extern Object *chatOutputTextEditor;
extern Object *chatOutputScroller;
extern Object *statusBar;
extern Object *conversationListObject;
extern Object *loadingBar;
extern Object *imageListObject;
extern Object *imageInputTextEditor;
extern Object *createImageButton;
extern Object *newImageButton;
extern Object *deleteImageButton;
extern Object *imageView;
extern WORD pens[NUMDRIPENS + 1];
extern LONG sendMessageButtonPen;
extern LONG newChatButtonPen;
extern LONG deleteButtonPen;
extern BOOL isPublicScreen;

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow();

/**
 * Display an error message
 * @param message the message to display
 **/
void displayError(STRPTR message);

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 *
 **/
void updateStatusBar(CONST_STRPTR message, const ULONG pen);
