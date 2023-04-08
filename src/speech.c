#include "speech.h"
#include "config.h"
#include <proto/exec.h>
#include <stdio.h>
#include <proto/translator.h>
#include <devices/narrator.h>

#define TRANSLATION_BUFFER_SIZE 8192

extern struct ExecBase* SysBase;
struct Library *TranslatorBase;
struct MsgPort *NarratorPort;
struct narrator_rb *NarratorIO;
BYTE audioChannels[4] = {3, 5, 10, 12};
UBYTE translationBuffer[TRANSLATION_BUFFER_SIZE];

LONG openSpeechLibraries() {
	#ifdef OLD_SPEECH
		#ifdef EMULATOR
		TranslatorBase = (struct Library *)OpenLibrary("libs:old/translator.library", 0);
		#else
		TranslatorBase = (struct Library *)OpenLibrary("PROGDIR:libs/translator.library", 0);
		#endif
	#else
	TranslatorBase = (struct Library *)OpenLibrary("translator.library", 0);
	#endif
	if (TranslatorBase == NULL) {
		printf("Could not open translator.library\n");
		return RETURN_ERROR;
	}
    else
        return RETURN_OK;
}

LONG openSpeechDevices() {
	NarratorPort = CreateMsgPort();
    if (!NarratorPort) {
		printf("Could not create narrator port\n");
		return RETURN_ERROR;
	}

    NarratorIO = CreateIORequest(NarratorPort, sizeof(struct narrator_rb));
    if (!NarratorIO) {
		printf("Could not create narrator IO request\n");
		return RETURN_ERROR;
	}

	#ifdef OLD_SPEECH
		#ifdef EMULATOR
		if (OpenDevice("devs:old/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
		#else
		if (OpenDevice("PROGDIR:devs/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
		#endif
	#else
	if (OpenDevice("narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
	#endif
			printf("Could not open narrator device\n");
			return RETURN_ERROR;
	}
	
	// NarratorIO->flags = NDF_NEWIORB;
    return RETURN_OK;
}

void closeSpeechLibraries() {
	CloseLibrary(TranslatorBase);
}

void closeSpeechDevices() {
	 if (NarratorIO) {
		CloseDevice((struct IORequest *)NarratorIO);
		DeleteIORequest((struct IORequest *)NarratorIO);
	 }

	 if (NarratorPort)
		DeleteMsgPort(NarratorPort);
}

void speakText(UBYTE *text) {
	LONG textLength = strlen(text);
	Translate(text, textLength, translationBuffer, TRANSLATION_BUFFER_SIZE);
	LONG translatedStringLength = strlen(translationBuffer);

	NarratorIO->ch_masks = audioChannels;
	NarratorIO->nm_masks = sizeof(audioChannels);
	NarratorIO->message.io_Command= CMD_WRITE;
    NarratorIO->message.io_Data = translationBuffer;
    NarratorIO->message.io_Length = translatedStringLength;
    DoIO((struct IORequest *)NarratorIO);
}