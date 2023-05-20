#include "speech.h"
#include <proto/exec.h>
#include <stdio.h>
#include <proto/translator.h>
#include <devices/narrator.h>

#define TRANSLATION_BUFFER_SIZE 8192

extern struct ExecBase* SysBase;
struct Library *TranslatorBase;
struct MsgPort *NarratorPort;
struct narrator_rb *NarratorIO;
static BYTE audioChannels[4] = {3, 5, 10, 12};
static UBYTE translationBuffer[TRANSLATION_BUFFER_SIZE];

/**
 * Initialise the speech system
 * @param speechSystem the speech system to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initSpeech(enum SpeechSystem speechSystem) {
	if (!(NarratorPort = CreateMsgPort())) {
		printf("Could not create narrator port\n");
		return RETURN_ERROR;
	}

	if (!(NarratorIO = CreateIORequest(NarratorPort, sizeof(struct narrator_rb)))) {
		printf("Could not create narrator IO request\n");
		return RETURN_ERROR;
	}

	switch (speechSystem) {
		case SpeechSystemNone:
			return RETURN_OK;
		case SpeechSystem34:
			if ((TranslatorBase = (struct Library *)OpenLibrary("PROGDIR:libs/speech/34/translator.library", 34)) == NULL) {
				printf("Could not open translator.library v34\n");
				return RETURN_ERROR;
			}
			if (OpenDevice("PROGDIR:devs/speech/34/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
				printf("Could not open narrator.device v34\n");
				return RETURN_ERROR;
			}
			break;
		case SpeechSystem37:
			if ((TranslatorBase = (struct Library *)OpenLibrary("PROGDIR:libs/speech/37/translator.library", 37)) == NULL) {
				printf("Could not open translator.library v37\n");
				return RETURN_ERROR;
			}
			if (OpenDevice("PROGDIR:devs/speech/37/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
				printf("Could not open narrator.device v37\n");
				return RETURN_ERROR;
			}
			NarratorIO->flags = NDF_NEWIORB;
			break;
		case SpeechSystem43:
			if ((TranslatorBase = (struct Library *)OpenLibrary("PROGDIR:libs/speech/43/translator.library", 43)) == NULL) {
				printf("Could not open translator.library v43\n");
				return RETURN_ERROR;
			}
			if (OpenDevice("PROGDIR:devs/speech/37/narrator.device", 0, (struct IORequest *)NarratorIO, 0L) != 0) {
				printf("Could not open narrator.device v37\n");
				return RETURN_ERROR;
			}
			NarratorIO->flags = NDF_NEWIORB;
			break;
	}
	return RETURN_OK;
}

/**
 * Close the speech system
**/
void closeSpeech() {
	CloseLibrary(TranslatorBase);
	Forbid();
	RemLibrary(TranslatorBase);
	Permit();
	
	if (NarratorIO) {
		if (CheckIO((struct IORequest *)NarratorIO) == 0) {
			AbortIO((struct IORequest *)NarratorIO);
		}
		CloseDevice((struct IORequest *)NarratorIO);
		Forbid();
		RemDevice((struct Device *)((struct IORequest *)NarratorIO)->io_Device);
		Permit();
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
	if (CheckIO((struct IORequest *)NarratorIO) == 0) {
		AbortIO((struct IORequest *)NarratorIO);
	}
	Translate(text, strlen(text), translationBuffer, TRANSLATION_BUFFER_SIZE);
	NarratorIO->ch_masks = audioChannels;
	NarratorIO->nm_masks = sizeof(audioChannels);
	NarratorIO->message.io_Command= CMD_WRITE;
    NarratorIO->message.io_Data = translationBuffer;
    NarratorIO->message.io_Length = strlen(translationBuffer);
    SendIO((struct IORequest *)NarratorIO);
}