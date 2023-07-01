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

	struct WBStartup *wbStartupMessage = NULL;
	struct Process *currentTask = (struct Process*)FindTask(NULL);
	struct CommandLineInterface *cli = (struct CommandLineInterface *)BADDR(currentTask->pr_CLI);

	/* If we started from Workbench then we must retrieve the startup message
	 before doing anything else. The startup message also contains a lock on
	 the program directory. */
	if (cli == NULL) {
		wbStartupMessage = (struct WBStartup*)GetMsg(&currentTask->pr_MsgPort);
	}

	if (openLibraries() == RETURN_ERROR) {
		printf("Failed to open libraries\n");
		exit(RETURN_ERROR);
	}

	#ifdef __AMIGAOS4__

	char fileName[256];
	NameFromLock(GetProgramDir(), fileName, sizeof(fileName));
	AddPart(fileName, argv[0], sizeof(fileName));

	appID = RegisterApplication(PROGRAM_NAME,
		REGAPP_UniqueApplication, FALSE,
		REGAPP_URLIdentifier, "sacredbanana.net",
		REGAPP_Hidden, FALSE,
		REGAPP_LoadPrefs, TRUE,
		REGAPP_SavePrefs, TRUE,
		REGAPP_FileName, fileName,
		REGAPP_WBStartup, (ULONG)wbStartupMessage,
		REGAPP_NoIcon, cli != NULL,
		REGAPP_AppIconInfo, cli != NULL ? APPICONT_None : APPICONT_ProgramIcon,
		REGAPP_AppNotifications, FALSE,
		REGAPP_BlankerNotifications, FALSE,
		REGAPP_AllowsBlanker, TRUE,
		REGAPP_NeedsGameMode, FALSE,
		REGAPP_HasPrefsWindow, TRUE,
		REGAPP_HasIconifyFeature, TRUE,
		REGAPP_CanCreateNewDocs, TRUE,
		REGAPP_CanPrintDocs, TRUE,
		REGAPP_Description, "A ChatGPT client for AmigaOS",
		TAG_DONE);

	#endif
	readConfig();

	#ifdef __AMIGAOS3__
	if (initSpeech(config.speechSystem) == RETURN_ERROR) {
		printf("Failed to open speech system\n");
		config.speechSystem = SPEECH_SYSTEM_NONE;
		closeSpeech();
		initSpeech(config.speechSystem);
	}
	#endif

	if (initVideo() == RETURN_ERROR) {
		printf("Failed to initialize video\n");
		cleanExit(RETURN_ERROR);
	}

	if (initOpenAIConnector() == RETURN_ERROR) {
		printf("Failed to open OpenAI connector\n");
		exit(RETURN_ERROR);
	}

	if (wbStartupMessage != NULL)
		ReplyMsg((struct Message *)wbStartupMessage);

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
	if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 47)) == NULL) {
		printf("Failed to open dos.library v47. This app requires AmigaOS 3.2 or higher\n");
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
	shutdownGUI();
	#ifdef __AMIGAOS3__
	closeSpeech();
	#endif
	closeOpenAIConnector();
	closeLibraries();
	#ifdef __AMIGAOS4__
	UnregisterApplication(appID, NULL);
	#endif
}
