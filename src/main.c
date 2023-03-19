#include "support/gcc8_c_support.h"
#include <workbench/startup.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "config.h"
#include "speech.h"
#include "gui.h"
#include "openai.h"

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

LONG exitCode;

void openLibraries();
void openDevices();
void closeLibraries();
void closeDevices();
void configureApp();
void cleanExit(LONG);

int _main() {
	exitCode = 0;
	SysBase = *((struct ExecBase**)4UL);
	struct WBStartup *wbStartupMessage = NULL;
	struct Process *currentTask = (struct Process*)FindTask(NULL);
	struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(currentTask->pr_CLI);

	/* If we started from Workbench then we must retrieve the startup message
     before doing anything else. The startup message also contains a lock on
     the program directory. */
	if (cli == NULL) {
		WaitPort(&currentTask->pr_MsgPort);
    	wbStartupMessage = (struct WBStartup*)GetMsg(&currentTask->pr_MsgPort);
	}

	openLibraries();
	if (exitCode)
		goto exit;
	
	openDevices();
	if (exitCode)
		goto exit;

	configureApp();

	exitCode = initVideo();
	if (exitCode)
		goto exit;

	// exitCode = initOpenAIConnector();
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
