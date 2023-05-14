#include <proto/dos.h>

/**
 * A speech system. Old is the old translator.library, new is the new translator.library (V43), none is no speech system
**/
enum SpeechSystem {
    SpeechSystemNone,
    SpeechSystemOld,
    SpeechSystemNew
};

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
