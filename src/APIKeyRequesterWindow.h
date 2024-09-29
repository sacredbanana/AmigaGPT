#include <proto/dos.h>

extern Object *apiKeyRequesterString;
extern Object *apiKeyRequesterWindowObject;

/**
 * Create the API key requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createAPIKeyRequesterWindow();