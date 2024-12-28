#include <proto/dos.h>

extern Object *proxyHostString;
extern Object *proxyPortString;
extern Object *proxyUsesSSLCycle;
extern Object *proxyRequiresAuthCycle;
extern Object *proxyUsernameString;
extern Object *proxyPasswordString;
extern Object *proxySettingsRequesterWindowObject;

/**
 * Create the proxy settings requester window
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG createProxySettingsRequesterWindow();