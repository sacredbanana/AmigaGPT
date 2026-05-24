#include <proto/dos.h>
#include <intuition/intuition.h>

struct Conversation;

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
extern Object *modeRegisterGroup;
extern STRPTR chatOutputTextEditorContents;
extern WORD pens[NUMDRIPENS + 1];
extern LONG sendMessageButtonPen;
extern LONG newChatButtonPen;
extern LONG deleteButtonPen;

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow();

/** Phase 7c: remove chat wheel EH before MUI_DisposeObject(app). */
void chatOutputWheelShutdown(void);

/** Phase 7c: delete chat NFloattext subclass after MUI_DisposeObject(app). */
void chatOutputWheelDisposeClass(void);

/** Current chat (NULL if none). For tools like the code-block viewer. */
struct Conversation *getCurrentConversation(void);

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
 **/
void displayConversation(struct Conversation *conversation);

/**
 * Print the conversation text to the printer
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG printConversation();