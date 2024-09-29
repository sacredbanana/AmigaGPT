#include <proto/dos.h>

/**
 *  The object IDs for the GUI
 * */
enum {
	OBJECT_ID_MAIN_WINDOW = 1,
	OBJECT_ID_MAIN_GROUP,
	OBJECT_ID_MODE_CLICK_TAB,
	OBJECT_ID_SEND_MESSAGE_BUTTON,
	OBJECT_ID_CHAT_INPUT_TEXT_EDITOR,
	OBJECT_ID_CHAT_OUTPUT_TEXT_EDITOR,
	OBJECT_ID_CHAT_OUTPUT_SCROLLER,
	OBJECT_ID_STATUS_BAR,
	OBJECT_ID_CONVERSATION_LIST,
	OBJECT_ID_DATA_TYPE,
	OBJECT_ID_APP,
	OBJECT_ID_IMAGE_WINDOW,
	OBJECT_ID_IMAGE_LIST,
	OBJECT_ID_IMAGE_DATA_TYPE,
	OBJECT_ID_IMAGE_GROUP,
	OBJECT_ID_IMAGE_WINDOW_OBJECT,
	OBJECT_ID_ABOUT_AMIGAGPT
};

extern struct Screen *screen;
extern Object *app;

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initVideo();

/**
 * Start the main run loop of the GUI
**/ 
void startGUIRunLoop();

/**
 * Shutdown the GUI
**/
void shutdownGUI();

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

/**
 * Prints the conversation to the conversation window
 * @param conversation the conversation to display
**/
void displayConversation(struct Conversation *conversation);

/**
 * Free the conversation
 * @param conversation The conversation to free
**/
void freeConversation(struct Conversation *conversation);

/**
 * Saves the conversations to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG saveConversations();

/**
 * Copy a conversation
 * @param conversation The conversation to copy
 * @return A pointer to the copied conversation
**/
struct Conversation* copyConversation(struct Conversation *conversation);

/**
 * Sends a chat message to the OpenAI API and displays the response and speaks it if speech is enabled
**/
void sendChatMessage();