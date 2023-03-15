#include "support/gcc8_c_support.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include "config.h"
#include "speech.h"
#include "gui.h"

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

LONG exitCode;

void openLibraries();
void openDevices();
void closeLibraries();
void closeDevices();
void configureApp();
void cleanExit(LONG);

int main() {
	exitCode = 0;
	SysBase = *((struct ExecBase**)4UL);

	openLibraries();
	if (exitCode)
		goto exit;
	
	openDevices();
	if (exitCode)
		goto exit;

	configureApp();

	initVideo();
	if (exitCode)
		goto exit;

	if (startGUIRunLoop() != 0)
		cleanExit(RETURN_ERROR);

exit:
	return exitCode;
}

void openLibraries() {
	if (openGUILibraries() != 0)
		cleanExit(RETURN_ERROR);

	if (openSpeechLibraries() != 0)
		cleanExit(RETURN_ERROR);
}

void openDevices() {
	if (openSpeechDevices() != 0)
		cleanExit(RETURN_ERROR);
}

void closeLibraries() {
	closeGUILibraries();
	closeSpeechLibraries();
}

void closeDevices() {
	 closeSpeechDevices();
}

void cleanExit(LONG returnValue) {
	// There seems to be a bug with the Exit() call causing the program to guru. Use dirty goto's for now
	shutdownGUI();
	closeLibraries();
	closeDevices();
	exitCode = returnValue;
	// Exit(returnValue);
}
