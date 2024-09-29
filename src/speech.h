#include <proto/dos.h>

/**
 * A speech system
**/
enum SpeechSystem {
	SPEECH_SYSTEM_34 = 0L,
	SPEECH_SYSTEM_37,
	SPEECH_SYSTEM_FLITE,
	SPEECH_SYSTEM_OPENAI
};

/**
 * The names of the speech systems
 * @see enum SpeechSystem
**/ 
extern const STRPTR SPEECH_SYSTEM_NAMES[];

#ifdef __AMIGAOS4__

/**
 * The Flite voice of the spoken text
**/
enum SpeechFliteVoice {
	SPEECH_FLITE_VOICE_KAL = 0L,
	SPEECH_FLITE_VOICE_KAL16,
	SPEECH_FLITE_VOICE_AWB,
	SPEECH_FLITE_VOICE_RMS,
	SPEECH_FLITE_VOICE_SLT
};

/**
 * The names of the speech voices
 * @see enum SpeechFliteVoice
**/ 
extern const STRPTR SPEECH_FLITE_VOICE_NAMES[];

#endif

/**
 * Initialise the speech system
 * @param speechSystem the speech system to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initSpeech(enum SpeechSystem speechSystem);

/**
 * Speak the given text aloud
 * @param text the text to speak
**/
void speakText(STRPTR text);

/**
 * Close the speech system
**/
void closeSpeech();