#include <proto/dos.h>

/**
 * A speech system. Old is the old translator.library, new is the new translator.library (V43), none is no speech system
**/
enum SpeechSystem {
	SPEECH_SYSTEM_NONE = 0,
	SPEECH_SYSTEM_34,
	SPEECH_SYSTEM_37
};

/**
 * The names of the speech systems
 * @see enum SpeechSystem
**/ 
extern const STRPTR SPEECH_SYSTEM_NAMES[];

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
