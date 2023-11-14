#include <stdio.h>
#include <proto/exec.h>
#ifdef __AMIGAOS3__
#include <proto/translator.h>
#include <devices/narrator.h>
#else
#include <exec/exec.h>
#include <dos/dos.h>
#include <devices/flite.h>
#include <proto/flite.h>
#endif
#include "config.h"
#include "version.h"

#ifdef __AMIGAOS3__
#define TRANSLATION_BUFFER_SIZE 8192

struct Library *TranslatorBase = NULL;
static struct MsgPort *NarratorPort = NULL;
static struct narrator_rb *NarratorIO = NULL;
static BYTE audioChannels[4] = {3, 5, 10, 12};
static STRPTR translationBuffer;
#else
static struct MsgPort *fliteMessagePort = NULL;
static struct FliteRequest *fliteRequest = NULL;
struct Device *FliteBase = NULL;
struct FliteIFace *IFlite = NULL;
struct FliteVoice *voice = NULL;

/**
 * The names of the speech voices
 * @see enum SpeechVoice
**/ 
const STRPTR SPEECH_VOICE_NAMES[] = {
	[SPEECH_VOICE_KAL] = "kal",
	[SPEECH_VOICE_KAL16] = "kal16",
	[SPEECH_VOICE_AWB] = "awb",
	[SPEECH_VOICE_RMS] = "rms",
	[SPEECH_VOICE_SLT] = "slt"
};
#endif

/**
 * The names of the speech systems
 * @see enum SpeechSystem
**/ 
const STRPTR SPEECH_SYSTEM_NAMES[] = {
	[SPEECH_SYSTEM_NONE] = "None",
	[SPEECH_SYSTEM_34] = "Workbench 1.x v34",
	[SPEECH_SYSTEM_37] = "Workbench 2.0 v37",
	[SPEECH_SYSTEM_FLITE] = "Flite"
};

/**
 * Initialise the speech system
 * @param speechSystem the speech system to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initSpeech(enum SpeechSystem speechSystem) {
	#ifdef __AMIGAOS3__
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
	#else
	
	/* Note: we could use a struct IOStdReq here if we didn't need any of
       the more "advanced" features of the device. */
	/* Any additional fields will be ignored by flite.device 52.1 */
	fliteMessagePort = AllocSysObject(ASOT_PORT, NULL);
	fliteRequest = AllocSysObjectTags(ASOT_IOREQUEST,
		ASOIOR_Size,		sizeof(struct FliteRequest),
		ASOIOR_ReplyPort,	fliteMessagePort,
		TAG_END);

	if (fliteRequest) {
		/* minimum version/revision */
		/* This information will unfortunately be ignored by 52.1, */
		/* so an additional check after OpenDevice() may be in order. */
		fliteRequest->fr_Version = 53;
		fliteRequest->fr_Revision = 1;

		if (OpenDevice("flite.device", 0, (struct IORequest *)fliteRequest, 0) == IOERR_SUCCESS) {
			FliteBase = fliteRequest->fr_Std.io_Device;
			/* "main" interface is only available in 53.1 and higher versions */
			IFlite = (struct FliteIFace *)GetInterface(
				(struct Library *)FliteBase, "main", 1, NULL);
			if (!IFlite) {
				PutErrStr(APP_NAME": failed to obtain interface for flite.device\n");
				return RETURN_ERROR;
			}
		} else {
			PutErrStr(APP_NAME": failed to open flite.device\n");
			return RETURN_ERROR;
		}
	} else {
		PrintFault(ERROR_NO_FREE_STORE, APP_NAME);
		return RETURN_ERROR;
	}
	#endif
	
	return RETURN_OK;
}

/**
 * Close the speech system
**/
void closeSpeech() {
	#ifdef __AMIGAOS3__
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

	FreeVec(translationBuffer);
	#else
	if (IFlite && voice) CloseVoice(voice);
	voice = NULL;
	DropInterface((struct Interface *)IFlite);
	CloseDevice((struct IORequest *)fliteRequest);
	FreeSysObject(ASOT_IOREQUEST, fliteRequest);
	FreeSysObject(ASOT_PORT, fliteMessagePort);
	#endif
}

/**
 * Speak the given text aloud
 * @param text the text to speak
**/
void speakText(STRPTR text) {
	if (config.speechSystem == SPEECH_SYSTEM_NONE) return;
	#ifdef __AMIGAOS3__
	memset(translationBuffer, 0, TRANSLATION_BUFFER_SIZE);
	if (CheckIO((struct IORequest *)NarratorIO) == 0) {
		WaitIO((struct IORequest *)NarratorIO);
	}
	TranslateAs(text, strlen(text), translationBuffer, TRANSLATION_BUFFER_SIZE, config.speechAccent);
	NarratorIO->ch_masks = audioChannels;
	NarratorIO->nm_masks = sizeof(audioChannels);
	NarratorIO->message.io_Command= CMD_WRITE;
	NarratorIO->message.io_Data = translationBuffer;
	NarratorIO->message.io_Length = strlen(translationBuffer);
	SendIO((struct IORequest *)NarratorIO);
	#else
	if (IFlite && voice) CloseVoice(voice);
	voice = NULL;
	UBYTE voiceName[32];
	snprintf(voiceName, 32, "%s.voice\0", SPEECH_VOICE_NAMES[config.speechVoice]);
	voice = OpenVoice(voiceName);
	if (!voice) {
		PutErrStr(APP_NAME": failed to open voice\n");
		return;
	}
	fliteRequest->fr_Std.io_Command = CMD_WRITE;
	fliteRequest->fr_Std.io_Data = (APTR)text;
	fliteRequest->fr_Std.io_Length = ~0; /* io_Data is NULL-terminated */
	fliteRequest->fr_Voice = voice;

	SendIO((struct IORequest *)fliteRequest);
	Wait((1 << fliteMessagePort->mp_SigBit)|SIGBREAKF_CTRL_C);

	/* Note: Never use CheckIO() or WaitIO() on an unused IORequest! */
	if (!CheckIO((struct IORequest *)fliteRequest)) {
		/* Aborting only works if the IORequest has not yet been
			removed from the MsgPort by the flite.device process. */
		AbortIO((struct IORequest *)fliteRequest);
	}
	/* Wait for request to finish, and perform cleanup afterwards */
	WaitIO((struct IORequest *)fliteRequest);

	switch (fliteRequest->fr_Std.io_Error) {
		case IOERR_SUCCESS:
			break;
		case IOERR_ABORTED:
			PutErrStr(APP_NAME": Speech IO aborted\n");
			break;
		case FLERR_FLITE:
			PutErrStr(APP_NAME": libflite error\n");
			break;
		case FLERR_NOVOICE:
			PutErrStr(APP_NAME": no voice\n");
			break;
		default:
			PutErrStr(APP_NAME": unknown speech IO error\n");
			break;
	}
	#endif
}
