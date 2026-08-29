#include <proto/dos.h>
#include <intuition/intuition.h>

extern struct Window *mainWindow;
extern Object *mainWindowObject;
extern Object *newChatButton;
extern Object *deleteChatButton;
extern Object *sendMessageButton;
extern Object *attachFilesButton;
extern Object *clearAttachmentsButton;
extern Object *saveResponseFilesButton;
extern Object *attachmentSummaryText;
extern Object *chatInputTextEditor;
extern Object *chatOutputTextEditor;
extern Object *chatOutputScroller;
extern Object *statusBar;
extern Object *conversationListObject;
extern Object *loadingBar;
extern Object *loadingBarGroup;
extern Object *imageListObject;
extern Object *imageInputTextEditor;
extern Object *speechInputTextEditor;
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

/** Free pending attachment state owned by the main window. */
void freeMainWindowFileState();

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

/**
 * Shows the loading bar and starts the busy meter animating
 **/
void showLoadingBar();

/**
 * Stops the busy meter and hides the loading bar, leaving it blank
 **/
void hideLoadingBar();

/**
 * Refresh the Speech tab Play button: Stop while audio is playing, Play
 * when idle.
 **/
void updatePlayButton();
