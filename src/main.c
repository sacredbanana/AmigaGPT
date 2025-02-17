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
#include "AmigaGPT_cat.h"
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

    UBYTE *upper = (UBYTE *)currentTask->pr_Task.tc_SPUpper;
    UBYTE *lower = (UBYTE *)currentTask->pr_Task.tc_SPLower;
    ULONG total = upper - lower;

    if (total < 32768) {
        printf(STRING_WARNING_STACK, total);
    }
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
        REGAPP_Description, STRING_APP_DESCRIPTION, TAG_DONE);
#endif
    readConfig();

    if (initVideo() == RETURN_ERROR) {
        printf(STRING_ERROR_VIDEO_INIT);
        exit(RETURN_ERROR);
    }

    if (initSpeech(config.speechSystem) == RETURN_ERROR) {
#ifdef __AMIGAOS3__
        displayError(STRING_ERROR_SPEECH_OS3_INIT);
#else
        displayError(STRING_ERROR_SPEECH_OS4_INIT);
#endif
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
