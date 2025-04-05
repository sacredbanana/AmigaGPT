#include <proto/dos.h>

extern Object *voiceInstructionsRequesterString;
extern Object *voiceInstructionsRequesterWindowObject;

/**
 * Create the voice instructions requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createVoiceInstructionsRequesterWindow();