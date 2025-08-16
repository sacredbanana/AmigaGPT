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
extern STRPTR chatOutputTextEditorContents;
extern WORD pens[NUMDRIPENS + 1];
extern LONG sendMessageButtonPen;
extern LONG newChatButtonPen;
extern LONG deleteButtonPen;
// Removed isPublicScreen - MUI handles screen management automatically

/**
 * Create the main window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createMainWindow();

/**
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 *
 **/
void updateStatusBar(CONST_STRPTR message, const ULONG pen);

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
 **/
struct Conversation *newConversation();

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
 **/
void displayConversation(struct Conversation *conversation);

/**
 * Sets the system of the conversation
 * @param conversation the conversation to set the system of
 * @param system the system to set
 **/
void setConversationSystem(struct Conversation *conversation,
                           CONST_STRPTR system);

/**
 * Get the message content from the JSON response from OpenAI
 * @param json the JSON response from OpenAI
 * @param stream whether the response is a stream or not
 * @return a pointer to a new UTF8 string containing the message content -- Free
 *it with FreeVec() when you are done using it If found role in the json instead
 *of content then return an empty string
 * @todo Handle errors
 **/
UTF8 *getMessageContentFromJson(struct json_object *json, BOOL stream);

/**
 * Add a block of text to the conversation list
 * @param conversation The conversation to add the text to
 * @param text The text to add to the conversation
 * @param role The role of the text (user or assistant)
 **/
void addTextToConversation(struct Conversation *conversation, UTF8 *text,
                           STRPTR role);

/**
 * Free the conversation
 * @param conversation The conversation to free
 **/
void freeConversation(struct Conversation *conversation);

/**
 * Print the conversation text to the printer
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG printConversation();