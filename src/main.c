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
struct Library *ApplicationBase;
extern struct ExecIFace *IExec;
extern struct DOSIFace *IDOS;
extern struct UtilityIFace *IUtility;
struct ApplicationIFace *IApplication;
static uint32 appID;
extern struct Library *UtilityBase;
#else
struct Library *UtilityBase;
#endif

struct WBStartup *wbStartupMessage = NULL;

static LONG openLibraries();
static void closeLibraries();
static void cleanExit();

/**
 * App entry point
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG main(int argc, char **argv) {
	atexit(cleanExit);
	SysBase = *((struct ExecBase**)4UL);

	if (openLibraries() == RETURN_ERROR) {
		printf("Failed to open libraries\n");
		exit(RETURN_ERROR);
	}

	#ifdef __AMIGAOS3__
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
		printf("Warning: The stack size of %ld bytes is too small. The minimum recommended stack size is 32768 bytes to avoid crashes.\n", total);
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

	config.speechSystem = SPEECH_SYSTEM_OPENAI;

	if (initVideo() == RETURN_ERROR) {
		printf("Failed to initialize video\n");
		cleanExit(RETURN_ERROR);
	}

	if (initSpeech(config.speechSystem) == RETURN_ERROR) {
		#ifdef __AMIGAOS3__
		displayError("Failed to open speech system. Make sure translator.library v43 and the relevant narrator.device are installed. See the guide for more information.");
		#else
		displayError("Failed to open speech system. Make sure Flite Device is installed. See the guide for more information.");
		#endif
		config.speechSystem = SPEECH_SYSTEM_NONE;
		closeSpeech();
		initSpeech(config.speechSystem);
	}

	if (initOpenAIConnector() == RETURN_ERROR) {
		displayError("Failed to open OpenAI connector.");
		exit(RETURN_ERROR);
	}

	#ifdef __AMIGAOS3__
	if (wbStartupMessage != NULL)
		ReplyMsg((struct Message *)wbStartupMessage);
	#endif

	if (startGUIRunLoop() == RETURN_ERROR) {
		printf("GUI run loop returned an error\n");
		cleanExit(RETURN_ERROR);
	}

	exit(RETURN_OK);
	return 0;
}

/**
 * Open libraries needed by the whole app
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG openLibraries() {
	#ifdef __AMIGAOS3__
	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 40)) == NULL) {
		printf("Failed to open dos.library v40. This app requires AmigaOS 3.9 or 3.2+\n");
		return RETURN_ERROR;
	}
	#else
	if ((DOSBase = OpenLibrary("dos.library", 50)) == NULL) {
		printf("Failed to open dos.library v50. Is this even AmigaOS 4??\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((UtilityBase = OpenLibrary("utility.library", 0)) == NULL) {
		printf( "Could not open utility.library\n");
		return RETURN_ERROR;
	}
	#else
	if ((UtilityBase = OpenLibrary("utility.library", 50)) == NULL) {
		printf( "Could not open utility.library\n");
		return RETURN_ERROR;
	}
	if ((IUtility = (struct UtilityIFace *)GetInterface(UtilityBase, "main", 1, NULL)) == NULL) {
		printf( "Could not get interface for utility.library\n");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS4__
	if ((ApplicationBase = OpenLibrary("application.library", 53)) == NULL) {
		printf( "Could not open application.library\n");
		return RETURN_ERROR;
	}
	if ((IApplication = (struct UtilityIFace *)GetInterface(ApplicationBase, "application", 2, NULL)) == NULL) {
		printf( "Could not get interface for application.library\n");
		return RETURN_ERROR;
	}
	#endif

	return RETURN_OK;
}

/**
 * Close libraries needed by the whole app
**/
static void closeLibraries() {
	#ifdef __AMIGAOS4__
	DropInterface((struct Interface*)IDOS);
	DropInterface((struct Interface*)IUtility);
	DropInterface((struct Interface *)IApplication);
	CloseLibrary(ApplicationBase);
	#endif
	CloseLibrary(DOSBase);
	CloseLibrary(UtilityBase);
}

/**
 * Cleanup and exit the app
**/
static void cleanExit() {
	freeConfig();
	shutdownGUI();
	closeSpeech();
	closeOpenAIConnector();
	closeLibraries();
	#ifdef __AMIGAOS4__
	UnregisterApplication(appID, NULL);
	#endif
}
