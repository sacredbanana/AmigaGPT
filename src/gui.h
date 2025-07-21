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
extern ULONG redPen, greenPen, bluePen, yellowPen;
extern Object *imageWindowObject;
extern Object *imageWindowImageView;
extern Object *imageWindowImageViewGroup;
extern BOOL isMUI5;
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
 * Shutdown the GUI
 **/
void shutdownGUI();
