#include <proto/dos.h>

extern Object *chatSystemRequesterString;
extern Object *chatSystemRequesterWindowObject;

/**
 * Create the chat system requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createChatSystemRequesterWindow();