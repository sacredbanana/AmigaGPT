#include <workbench/startup.h>
#include <stdio.h>
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

LONG openLibraries();
LONG openDevices();
void closeLibraries();
void closeDevices();
void configureApp();
void cleanExit();

int main() {
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

	exitCode = openLibraries();
	if (exitCode)
		goto exit;

	exitCode = openDevices();
	if (exitCode)
		goto exit;

	configureApp();

	#ifndef EMULATOR

	exitCode = initOpenAIConnector();
	if (exitCode)
		goto exit;

	exitCode = connectToOpenAI();
	if (exitCode)
		goto exit;
	#endif

	exitCode = initVideo();
	if (exitCode)
		goto exit;

	exitCode = startGUIRunLoop();
	if (exitCode)
		goto exit;

exit:
	cleanExit();
	return exitCode;
}

LONG openLibraries() {
	DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 47);

	if (DOSBase == NULL)  {
		printf("Failed to open dos.library v37. This app requires AmigaOS 3.2 or higher\n");
        return RETURN_ERROR;
	}

	if (openGUILibraries() != 0) {
		printf("Failed to open GUI libraries\n");
		return RETURN_ERROR;
	}

	if (openSpeechLibraries() != 0) {
		printf("Failed to open speech libraries\n");
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

LONG openDevices() {
	if (openSpeechDevices() != 0) {
		printf("Failed to open speech devices\n");
		return RETURN_ERROR;
	}

	return RETURN_OK;
}

void closeLibraries() {
	closeGUILibraries();
	closeSpeechLibraries();
	CloseLibrary(DOSBase);
}

void closeDevices() {
	 closeSpeechDevices();
}

void cleanExit() {
	// There seems to be a bug with the Exit() call causing the program to guru. Use dirty goto's for now
	shutdownGUI();
	#ifndef EMULATOR
	closeOpenAIConnector();
	#endif
	closeLibraries();
	closeDevices();
	Exit(RETURN_OK);
}
