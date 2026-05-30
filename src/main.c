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
#include "MainWindow.h"
#include "config.h"
#include "streamlog.h"
#include "version.h"
#if defined(__MORPHOS__) && !defined(DAEMON)
#include "morphos_relaunch.h"
#endif

#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
CONST_STRPTR stack = "$STACK: 32768";
#elif defined(__MORPHOS__)
unsigned long __stack = 32768;
#endif

#ifdef DAEMON
CONST_STRPTR version = APP_VER_STRING_AMIGAGPTD;
#else
CONST_STRPTR version = APP_VER_STRING_AMIGAGPT;
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
    streamLogLifecycle("startup begin");
    SysBase = *((struct ExecBase **)4UL);

#if defined(__MORPHOS__) && !defined(DAEMON)
    if (!morphosRelaunchStartupGuard()) {
        displayError(
            (STRPTR) "AmigaGPT is already running, or the previous instance is "
                     "still closing.\n\nWait a few seconds, then start once.");
        streamLogLifecycle("startup aborted instance guard");
        return RETURN_ERROR;
    }
#endif

#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    struct Process *currentTask = (struct Process *)FindTask(NULL);
    struct CommandLineInterface *cli =
        (struct CommandLineInterface *)BADDR(currentTask->pr_CLI);

    /* If we started from Workbench then we must retrieve the startup message
     before doing anything else. The startup message also contains a lock on
     the program directory. */
    if (cli == NULL) {
        wbStartupMessage = (struct WBStartup *)GetMsg(&currentTask->pr_MsgPort);
        if (wbStartupMessage != NULL) {
            ReplyMsg((struct Message *)wbStartupMessage);
            wbStartupMessage = NULL;
        }
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
        cleanExit();
        return RETURN_ERROR;
    }

    streamLogBootPhase("initVideo call");
    if (initVideo() == RETURN_ERROR) {
        streamLogBootPhase("initVideo fail");
        /* initVideo() already reported a specific GUI/library error dialog. */
        cleanExit();
        return RETURN_ERROR;
    }
    streamLogBootPhase("initVideo done");

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
        cleanExit();
        return RETURN_ERROR;
    }

#ifdef DAEMON
    printf("AmigaGPTD ready - listening for ARexx commands...\n");
#endif

    startGUIRunLoop();
    cleanExit();
    return RETURN_OK;
}

/**
 * Cleanup and exit the app
 **/
static void cleanExit() {
#ifdef DAEMON
    printf("AmigaGPTD shutting down...\n");
#endif

    streamLogLifecycle("shutdown begin");
    streamLogShutdownPhase("shutdown begin");
    mainWindowSignalQuit();

#ifndef DAEMON
#ifdef __MORPHOS__
    mainWindowCaptureGeometryForConfig();
#endif
    if (writeConfig() == RETURN_ERROR) {
        streamLogShutdownPhase("config write failed");
        if (app) {
            displayError(STRING_ERROR_CONFIG_FILE_WRITE);
        }
#ifdef __MORPHOS__
        else {
            KPrintF("[AmigaGPT] config write failed on shutdown io=%ld\n",
                    (long)IoErr());
        }
#endif
    } else {
        streamLogShutdownPhase("config saved");
    }
#endif

    shutdownGUI();
#ifndef DAEMON
    streamLogShutdownPhase("after shutdown gui");
#endif
    freeConfig();
    closeSpeech();
    closeOpenAIConnector();
#ifdef __AMIGAOS4__
    UnregisterApplication(appID, NULL);
#endif
    streamLogLifecycle("process exit");
    streamLogShutdownPhase("process exit");
}
