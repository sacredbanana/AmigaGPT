#include <workbench/startup.h>
#include <stdio.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "speech.h"
#include "gui.h"
#include "openai.h"

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

static LONG openLibraries();
static void closeLibraries();
static void cleanExit(ULONG returnCode);

int main() {
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

	if (initSpeech(SpeechSystemOld) == RETURN_ERROR) {
		printf("Failed to open speech system\n");
		cleanExit(RETURN_ERROR);
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

static LONG openLibraries() {
	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 47)) == NULL) {
		printf("Failed to open dos.library v47. This app requires AmigaOS 3.2 or higher\n");
        return RETURN_ERROR;
	}
	return RETURN_OK;
}

static void closeLibraries() {
	CloseLibrary(DOSBase);
}

static void cleanExit(ULONG returnCode) {
	shutdownGUI();
	closeSpeech();
	closeOpenAIConnector();
	closeLibraries();
	exit(returnCode);
}
