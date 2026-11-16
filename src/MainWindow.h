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

/**
 * Create a prompt editor like the one on the Chat tab
 * @param submitHook the hook called when Return is pressed
 * @param objectId the MUI object ID, or 0 for none
 * @param weight the layout weight, or 0 for the default
 **/
Object *createPromptInputEditor(struct Hook *submitHook, ULONG objectId,
                                ULONG weight);

/**
 * Convert **bold**, *italic* and __underline__ Markdown to MUI text codes
 * @return a new string. Free it with FreeVec()
 **/
STRPTR convertMarkdownFormattingToMUI(CONST_STRPTR input);

/** Whether a request is running */
BOOL mainWindowRequestBusy(void);

/**
 * Cancel the running request, if there is one
 * @return TRUE if a request was running
 **/
BOOL mainWindowAbortIfRequestBusy(void);

/** Lock the window and show the busy meter for a new request */
void mainWindowBeginRequest(void);

/** Unlock the window after a request */
void mainWindowFinishRequest(BOOL restoreReadyStatus);

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
 * Refresh the Speech tab transport, playhead and time display.
 **/
void updatePlayButton();
