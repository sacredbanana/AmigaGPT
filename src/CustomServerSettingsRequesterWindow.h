#include <proto/dos.h>

extern Object *customServerHostString;
extern Object *customServerPortString;
extern Object *customServerUsesSSLCycle;
extern Object *customServerApiKeyString;
extern Object *customServerChatModelString;
extern Object *customServerApiEndpointCycle;
extern Object *customServerApiEndpoinUrlString;
extern Object *customServerSettingsRequesterWindowObject;

/**
 * Create the custom server settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createCustomServerSettingsRequesterWindow();