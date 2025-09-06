#include <guigfx/guigfx.h>
#include <intuition/classusr.h>
#include <clib/alib_protos.h>
#include <proto/amigaguide.h>
#include <proto/arexx.h>
#include <proto/asl.h>
#define USE_INLINE_STDARG
#include <proto/codesets.h>
#undef USE_INLINE_STDARG
#include <proto/dos.h>
#include <proto/gadtools.h>
#include <proto/guigfx.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/locale.h>
#include <proto/muimaster.h>
#include <proto/rexxsyslib.h>
#include <proto/utility.h>
#include "AmigaGPT_cat.h"

#define APP_ID_PRINT 1000L

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

extern Object *app;
extern ULONG redPen, greenPen, bluePen, yellowPen;
extern Object *imageWindowObject;
extern Object *imageWindowImageView;
extern Object *imageWindowImageViewGroup;
extern BOOL isMUI5;
extern BOOL isMUI39;
extern BOOL isAROS;
extern struct codeset *systemCodeset;

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
 * Update the status bar
 * @param message the message to display
 * @param pen the pen to use for the text
 *
 **/
void updateStatusBar(CONST_STRPTR message, const ULONG pen);
/**
 * Copies a file from one location to another
 * @param source The source file to copy
 * @param destination The destination to copy the file to
 * @return TRUE if the file was copied successfully, FALSE otherwise
 **/
BOOL copyFile(STRPTR source, STRPTR destination);

/**
 * Display an error message
 * @param message the message to display
 **/
void displayError(STRPTR message);

/**
 * Creates a new conversation
 * @return A pointer to the new conversation
 **/
struct Conversation *newConversation();

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
 * Shutdown the GUI
 **/
void shutdownGUI();
