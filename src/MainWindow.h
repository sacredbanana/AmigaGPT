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
 * @param utf8 TRUE if its text is UTF-8 (MUIA_BestTextEditor_UTF8), FALSE
 *        for the system codeset
 **/
Object *createPromptInputEditor(struct Hook *submitHook, ULONG objectId,
                                ULONG weight, BOOL utf8);

/**
 * A read only view for a conversation, with Markdown, chat bubbles, clickable
 * links and its own scroll bar
 * @param viewOut receives the text object
 * @return the group holding the view and its scroll bar
 **/
Object *createTranscriptView(Object **viewOut);

/**
 * The hook that opens links clicked in a conversation view
 **/
struct Hook *mainWindowLinkHook(void);

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
 * Print the conversation text to the printer. The printing itself runs in a
 * separate process; this returns as soon as it has been started.
 * @return RETURN_OK when printing has started, RETURN_ERROR otherwise
 **/
LONG printConversation();

/**
 * The signal mask the main loop waits on for the print job to finish
 * @return the signal mask, or 0 when nothing is printing
 **/
ULONG printJobSignalMask(void);

/**
 * Report and clean up the print job once its process has finished
 **/
void servicePrintJob(void);

/**
 * Wait for a running print job to finish, before the program exits
 **/
void waitForPrintJob(void);

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
