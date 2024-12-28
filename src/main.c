#include <dos/dos.h>
#ifdef __AMIGAOS4__
#include <libraries/application.h>
#include <proto/application.h>
#endif
#include <proto/dos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <workbench/startup.h>
#include "gui.h"
#include "config.h"

CONST_STRPTR stack = "$STACK: 32768";

#ifdef __AMIGAOS4__
static uint32 appID;
#endif

struct WBStartup *wbStartupMessage = NULL;

static void cleanExit();

/**
 * App entry point
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG main(int argc, char **argv) {
	atexit(cleanExit);
	SysBase = *((struct ExecBase**)4UL);

	#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
	struct Process *currentTask = (struct Process*)FindTask(NULL);
	struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(currentTask->pr_CLI);

	/* If we started from Workbench then we must retrieve the startup message
	 before doing anything else. The startup message also contains a lock on
	 the program directory. */
	if (cli == NULL) {
		wbStartupMessage = (struct WBStartup*)GetMsg(&currentTask->pr_MsgPort);
	}

	UBYTE *upper = (UBYTE *)currentTask->pr_Task.tc_SPUpper;
	UBYTE *lower = (UBYTE *)currentTask->pr_Task.tc_SPLower;
	ULONG total = upper - lower;

	if (total < 32768) {
		printf("Warning: The stack size of %ld bytes is too small. The minimum recommended stack size is 20000 bytes to avoid crashes.\n", total);
	}
	#else
	if (argc == 0) {
		wbStartupMessage = (struct WBStartup *)argv;
	}

	char fileName[256];
	NameFromLock(GetProgramDir(), fileName, sizeof(fileName));
	AddPart(fileName, argv[0], sizeof(fileName));

	appID = RegisterApplication(NULL,
		REGAPP_UniqueApplication, TRUE,
		REGAPP_URLIdentifier, "sacredbanana.net",
		REGAPP_WBStartup, (ULONG)wbStartupMessage,
		REGAPP_Description, "A ChatGPT client for AmigaOS",
		TAG_DONE);
	#endif
	readConfig();

	if (initVideo() == RETURN_ERROR) {
		printf("Failed to initialize video\n");
		exit(RETURN_ERROR);
	}

	if (initSpeech(config.speechSystem) == RETURN_ERROR) {
		#ifdef __AMIGAOS3__
		displayError("Failed to open speech system. Make sure translator.library v43 and the relevant narrator.device are installed. See the guide for more information.");
		#else
		displayError("Failed to open speech system. Make sure Flite Device is installed. See the guide for more information.");
		#endif
		config.speechEnabled = FALSE;
		closeSpeech();
	}

	if (initOpenAIConnector() == RETURN_ERROR) {
		displayError("Failed to open OpenAI connector.");
		exit(RETURN_ERROR);
	}

	#ifdef __AMIGAOS3__
	if (wbStartupMessage != NULL)
		ReplyMsg((struct Message *)wbStartupMessage);
	#endif

	startGUIRunLoop();

	writeConfig();

	exit(RETURN_OK);
	return 0;
}

/**
 * Cleanup and exit the app
**/
static void cleanExit() {
	writeConfig();
	freeConfig();
	shutdownGUI();
	closeSpeech();
	closeOpenAIConnector();
	#ifdef __AMIGAOS4__
	UnregisterApplication(appID, NULL);
	#endif
}
