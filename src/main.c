#include <workbench/startup.h>
#include <stdio.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "gui.h"
#include "config.h"

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

static LONG openLibraries();
static void closeLibraries();
static void cleanExit(ULONG returnCode);

/**
 * App entry point
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG main() {
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

	if (openLibraries() == RETURN_ERROR) {
		printf("Failed to open libraries\n");
		cleanExit(RETURN_ERROR);
	}

	readConfig();

	if (initSpeech(config.speechSystem) == RETURN_ERROR) {
		printf("Failed to open speech system\n");
		config.speechSystem = SPEECH_SYSTEM_NONE;
		closeSpeech();
		initSpeech(config.speechSystem);
	}

	if (initOpenAIConnector() == RETURN_ERROR) {
		printf("Failed to open OpenAI connector\n");
		cleanExit(RETURN_ERROR);
	}

	if (initVideo() == RETURN_ERROR) {
		printf("Failed to initialize video\n");
		cleanExit(RETURN_ERROR);
	}

	if (startGUIRunLoop() == RETURN_ERROR) {
		printf("GUI run loop returned an error\n");
		cleanExit(RETURN_ERROR);
	}

	cleanExit(RETURN_OK);
	return 0;
}

/**
 * Open libraries needed by the whole app
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG openLibraries() {
	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 47)) == NULL) {
		printf("Failed to open dos.library v47. This app requires AmigaOS 3.2 or higher\n");
        return RETURN_ERROR;
	}
	return RETURN_OK;
}

/**
 * Close libraries needed by the whole app
**/
static void closeLibraries() {
	CloseLibrary(DOSBase);
}

/**
 * Cleanup and exit the app
 * @param returnCode the return code to exit with
**/
static void cleanExit(ULONG returnCode) {
	shutdownGUI();
	closeSpeech();
	closeOpenAIConnector();
	closeLibraries();
	exit(returnCode);
}
