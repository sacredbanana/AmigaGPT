#include "support/gcc8_c_support.h"
#include <workbench/startup.h>
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

	exitCode = initOpenAIConnector();
	if (exitCode)
		goto exit;

	exitCode = connectToOpenAI();
	if (exitCode)
		goto exit;
	
	exitCode = initVideo();
	if (exitCode)
		goto exit;

	// UBYTE *response = postMessageToOpenAI("Write a haiku about the Commodore Amiga", "gpt-3.5-turbo", "user");
	// if (response != NULL) {
	// 	Write(Output(), (APTR)response, strlen(response));
	// 	speakText(response);
	// 	Delay(50);
	// 	FreeVec(response);
	// } else {
	// 	UBYTE text66[] = "No response from OpenAI!\n";
	// 	Write(Output(), (APTR)text66, strlen(text66));
	// }

	exitCode = startGUIRunLoop();
	if (exitCode)
		goto exit;

exit:
	cleanExit();
	return exitCode;
}

LONG openLibraries() {
	DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
	if (DOSBase == NULL) 
        return RETURN_ERROR;

	if (openGUILibraries() != 0)
		return RETURN_ERROR;

	if (openSpeechLibraries() != 0)
		return RETURN_ERROR;

	return RETURN_OK;
}

LONG openDevices() {
	if (openSpeechDevices() != 0)
		return RETURN_ERROR;

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
	closeOpenAIConnector();
	closeLibraries();
	closeDevices();
	// Exit(returnValue);
}
