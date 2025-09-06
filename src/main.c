#include <dos/dos.h>
#ifdef __AMIGAOS4__
#include <libraries/application.h>
#include <proto/application.h>
#endif

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
#include <stdio.h>
#include <stdlib.h>
#include <workbench/startup.h>
#include "gui.h"
#include "config.h"
#include "version.h"

#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
CONST_STRPTR stack = "$STACK: 32768";
#elif defined(__MORPHOS__)
unsigned long __stack = 32768;
#endif

#ifdef DAEMON
CONST_STRPTR version = "$VER: AmigaGPTD " APP_VERSION " (" BUILD_DATE
                       ") � 2023-2025 Cameron Armstrong";
#else
CONST_STRPTR version = "$VER: AmigaGPT " APP_VERSION " (" BUILD_DATE
                       ") � 2023-2025 Cameron Armstrong";
#endif

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
    SysBase = *((struct ExecBase **)4UL);

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    struct Process *currentTask = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli =
        (struct CommandLineInterface *)BADDR(currentTask->pr_CLI);

    /* If we started from Workbench then we must retrieve the startup message
     before doing anything else. The startup message also contains a lock on
     the program directory. */
    if (cli == NULL) {
        wbStartupMessage = (struct WBStartup *)GetMsg(&currentTask->pr_MsgPort);
    }

#ifdef __AMIGAOS3__
    UBYTE *upper = (UBYTE *)currentTask->pr_Task.tc_SPUpper;
    UBYTE *lower = (UBYTE *)currentTask->pr_Task.tc_SPLower;
    ULONG total = upper - lower;

    if (total < 32768) {
        STRPTR warningMessage = AllocVec(512, MEMF_ANY);
        snprintf(warningMessage, 512, "%s %ld %s.\n", STRING_WARNING_STACK,
                 total, STRING_BYTES);
        displayError(warningMessage);
        FreeVec(warningMessage);
    }
#endif
#else
    if (argc == 0) {
        wbStartupMessage = (struct WBStartup *)argv;
    }

    char fileName[256];
    NameFromLock(GetProgramDir(), fileName, sizeof(fileName));
    AddPart(fileName, argv[0], sizeof(fileName));

    appID = RegisterApplication(
        NULL, REGAPP_UniqueApplication, TRUE, REGAPP_URLIdentifier,
        "minotaurcreative.net", REGAPP_WBStartup, (ULONG)wbStartupMessage,
#ifdef DAEMON
        REGAPP_Description, "AmigaGPT Daemon", TAG_DONE);
#else
        REGAPP_Description, STRING_APP_DESCRIPTION, TAG_DONE);
#endif
#endif

#ifdef DAEMON
    printf("AmigaGPTD " APP_VERSION " starting...\n");
#endif

    if (readConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_READ);
        exit(RETURN_ERROR);
    }

    if (initVideo() == RETURN_ERROR) {
        displayError(STRING_ERROR_VIDEO_INIT);
        exit(RETURN_ERROR);
    }

    if (initSpeech(config.speechSystem) == RETURN_ERROR) {
        switch (config.speechSystem) {
        case SPEECH_SYSTEM_34:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_34);
            break;
        case SPEECH_SYSTEM_37:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_37);
        case SPEECH_SYSTEM_FLITE:
            displayError(STRING_ERROR_SPEECH_INIT_FLITE);
            break;
        default:
            displayError(STRING_ERROR_SPEECH_UNKNOWN_SYSTEM);
            break;
        }
        config.speechEnabled = FALSE;
        closeSpeech();
    }

    if (initOpenAIConnector() == RETURN_ERROR) {
        displayError(STRING_ERROR_OPENAI_INIT);
        exit(RETURN_ERROR);
    }

#ifdef __AMIGAOS3__
    if (wbStartupMessage != NULL)
        ReplyMsg((struct Message *)wbStartupMessage);
#endif

#ifdef DAEMON
    printf("AmigaGPTD ready - listening for ARexx commands...\n");
#endif

    startGUIRunLoop();

    exit(RETURN_OK);
    return 0;
}

/**
 * Cleanup and exit the app
 **/
static void cleanExit() {
#ifdef DAEMON
    printf("AmigaGPTD shutting down...\n");
#endif

    if (writeConfig() == RETURN_ERROR) {
        displayError(STRING_ERROR_CONFIG_FILE_WRITE);
    }
    freeConfig();
    shutdownGUI();
    closeSpeech();
    closeOpenAIConnector();
#ifdef __AMIGAOS4__
    UnregisterApplication(appID, NULL);
#endif
}
