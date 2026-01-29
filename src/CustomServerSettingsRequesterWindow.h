#include <proto/dos.h>

extern Object *customServerHostString;
extern Object *customServerPortString;
extern Object *customServerUsesSSLCycle;
extern Object *customServerApiKeyString;
extern Object *customServerChatModelString;
extern Object *customServerApiEndpointCycle;
extern Object *customServerApiEndpointUrlString;
extern Object *customServerFullUrlPreviewString;
extern Object *customServerSettingsRequesterWindowObject;

/**
 * Create the provider settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createCustomServerSettingsRequesterWindow();

/* Open provider settings in Chat/Image mode */
void openChatProviderSettingsRequesterWindow(void);
void openImageProviderSettingsRequesterWindow(void);