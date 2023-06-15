#include <proto/exec.h>
#include <proto/translator.h>
#include <devices/narrator.h>
#include "config.h"

#define TRANSLATION_BUFFER_SIZE 8192

struct Library *TranslatorBase = NULL;
static struct MsgPort *NarratorPort = NULL;
static struct narrator_rb *NarratorIO = NULL;
static BYTE audioChannels[4] = {3, 5, 10, 12};
static STRPTR translationBuffer;

/**
 * The names of the speech systems
 * @see enum SpeechSystem
**/ 
const STRPTR SPEECH_SYSTEM_NAMES[] = {
	[SPEECH_SYSTEM_NONE] = "None",
	[SPEECH_SYSTEM_34] = "Workbench 1.x v34",
	[SPEECH_SYSTEM_37] = "Workbench 2.0 v37",
};

/**
 * Initialise the speech system
 * @param speechSystem the speech system to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initSpeech(enum SpeechSystem speechSystem) {
	translationBuffer = AllocVec(TRANSLATION_BUFFER_SIZE, MEMF_ANY);

	if (!(NarratorPort = CreateMsgPort())) {
		printf("Could not create narrator port\n");
		return RETURN_ERROR;
	}

	if (!(NarratorIO = CreateIORequest(NarratorPort, sizeof(struct narrator_rb)))) {
		printf("Could not create narrator IO request\n");
		return RETURN_ERROR;
	}

	switch (speechSystem) {
		case SPEECH_SYSTEM_NONE:
			return RETURN_OK;
		case SPEECH_SYSTEM_34:
			if (OpenDevice("PROGDIR:devs/speech/34/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
				printf("Could not open narrator.device v34\n");
				return RETURN_ERROR;
			}
			break;
		case SPEECH_SYSTEM_37:
			if (OpenDevice("PROGDIR:devs/speech/37/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
				printf("Could not open narrator.device v37\n");
				return RETURN_ERROR;
			}
			NarratorIO->flags = NDF_NEWIORB;
			break;
	}

	if ((TranslatorBase = (struct Library *)OpenLibrary("translator.library", 43)) == NULL) {
		printf("Could not open translator.library\n");
		return RETURN_ERROR;
	}
	return RETURN_OK;
}

/**
 * Close the speech system
**/
void closeSpeech() {
	FreeVec(translationBuffer);
	if (TranslatorBase != NULL) {
		CloseLibrary(TranslatorBase);
		Forbid();
		RemLibrary(TranslatorBase);
		Permit();
	}
	if (NarratorIO) {
		if (CheckIO((struct IORequest *)NarratorIO) == 0) {
			AbortIO((struct IORequest *)NarratorIO);
		}
		if (((struct IORequest *)NarratorIO)->io_Device != NULL) {
			CloseDevice((struct IORequest *)NarratorIO);
			Forbid();
			RemDevice((struct Device *)((struct IORequest *)NarratorIO)->io_Device);
			Permit();
		}
		
		DeleteIORequest((struct IORequest *)NarratorIO);
	 }

	 if (NarratorPort)
		DeleteMsgPort(NarratorPort);
}

/**
 * Speak the given text aloud
 * @param text the text to speak
**/
void speakText(STRPTR text) {
	if (config.speechSystem == SPEECH_SYSTEM_NONE) return;
	if (CheckIO((struct IORequest *)NarratorIO) == 0) {
		WaitIO((struct IORequest *)NarratorIO);
	}
	memset(translationBuffer, 0, TRANSLATION_BUFFER_SIZE);
	TranslateAs(text, strlen(text), translationBuffer, TRANSLATION_BUFFER_SIZE, config.speechAccent);
	NarratorIO->ch_masks = audioChannels;
	NarratorIO->nm_masks = sizeof(audioChannels);
	NarratorIO->message.io_Command= CMD_WRITE;
	NarratorIO->message.io_Data = translationBuffer;
	NarratorIO->message.io_Length = strlen(translationBuffer);
	SendIO((struct IORequest *)NarratorIO);
}