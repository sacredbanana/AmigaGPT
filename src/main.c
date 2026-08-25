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
#include "AmigaGPTConfig.h"
#include "gui.h"
#include "version.h"

#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
CONST_STRPTR stack = "$STACK: 32768";
#elif defined(__MORPHOS__)
unsigned long __stack = 32768;
#endif

#ifdef DAEMON
CONST_STRPTR version = "$VER: AmigaGPTD " APP_VERSION " (" BUILD_DATE
                       ") © 2023-2026 Cameron Armstrong";
#else
CONST_STRPTR version = "$VER: AmigaGPT " APP_VERSION " (" BUILD_DATE
                       ") © 2023-2026 Cameron Armstrong";
#endif

/* When a libnix program is started from Workbench it has no stdio streams, so
   the startup code opens the console described by __stdiowin and points
   stdin/stdout/stderr at it. The default is "CON://///AUTO/CLOSE/WAIT" - an
   untitled AUTO window that looks empty apart from its ":". The GUI reports
   everything through requesters, so send stray stdio to NIL: instead.
   Shell-started runs inherit the Shell's streams and never look at __stdiowin.
   The daemon keeps the default because its status output is the whole point of
   it. AmigaOS 4 (newlib) already defaults to no console window.

   libnix loads this as a char * (Open(__stdiowin)). It MUST be a pointer. An
   array "NIL:" is read as the address 0x4E494C3A; Open() fails and libnix
   calls exit(20) before main(), so the program appears not to launch from
   Workbench while still working from the Shell. */
#if (defined(__AMIGAOS3__) || defined(__MORPHOS__)) && !defined(DAEMON) &&     \
    !defined(DEBUG)
char *__stdiowin = "NIL:";
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

#ifdef __AMIGAOS3__
    struct Process *currentTask = (struct Process *)FindTask(NULL);
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
#ifdef __AMIGAOS4__
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

    if (initVideo() == RETURN_ERROR) {
        displayError(STRING_ERROR_VIDEO_INIT);
        exit(RETURN_ERROR);
    }

    /* Only open the configured speech device (which may require AHI) if the
     * user has actually enabled speech; otherwise leave it uninitialized so
     * users without AHI installed don't see a spurious error on every
     * launch. */
    if (configGetSpeechEnabled() &&
        initSpeech(configGetSpeechSystem()) == RETURN_ERROR) {
        switch (configGetSpeechSystem()) {
        case SPEECH_SYSTEM_34:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_34);
            break;
        case SPEECH_SYSTEM_37:
            displayError(STRING_ERROR_SPEECH_INIT_WORKBENCH_37);
            break;
        case SPEECH_SYSTEM_FLITE:
            displayError(STRING_ERROR_SPEECH_INIT_FLITE);
            break;
        default:
            displayError(STRING_ERROR_SPEECH_UNKNOWN_SYSTEM);
            break;
        }
        configSetSpeechEnabled(FALSE);
        closeSpeech();
    }

    if (initOpenAIConnector() == RETURN_ERROR) {
        displayError(STRING_ERROR_OPENAI_INIT);
        exit(RETURN_ERROR);
    }

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

    freeConfig();
    shutdownGUI();
    closeSpeech();
    closeOpenAIConnector();
#ifdef __AMIGAOS4__
    UnregisterApplication(appID, NULL);
#endif
}
