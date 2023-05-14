#include <proto/dos.h>

/**
 * Create the GUI
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initVideo();

/**
 * The main run loop of the GUI
 * @return The return code of the application
 * @see RETURN_OK
 * @see RETURN_ERROR
**/ 
LONG startGUIRunLoop();

/**
 * Shutdown the GUI
**/
void shutdownGUI();