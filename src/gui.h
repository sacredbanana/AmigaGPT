#include <proto/dos.h>

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