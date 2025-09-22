#ifndef SPEECH_H
#define SPEECH_H

#include <proto/dos.h>

/**
 * A speech system
 **/
typedef enum {
    SPEECH_SYSTEM_34 = 0L,
    SPEECH_SYSTEM_37,
    SPEECH_SYSTEM_FLITE,
    SPEECH_SYSTEM_OPENAI
} SpeechSystem;

/**
 * The names of the speech systems
 * @see SpeechSystem
 **/
extern const STRPTR SPEECH_SYSTEM_NAMES[];

/**
 * The Flite voice of the spoken text
 **/
typedef enum {
    SPEECH_FLITE_VOICE_KAL = 0L,
    SPEECH_FLITE_VOICE_KAL16,
    SPEECH_FLITE_VOICE_AWB,
    SPEECH_FLITE_VOICE_RMS,
    SPEECH_FLITE_VOICE_SLT
} SpeechFliteVoice;

/**
 * The names of the speech voices
 * @see SpeechFliteVoice
 **/
extern const STRPTR SPEECH_FLITE_VOICE_NAMES[];

/**
 * The audio format
 **/
typedef enum {
    AUDIO_FORMAT_PCM = 0L,
    AUDIO_FORMAT_MP3,
    AUDIO_FORMAT_OPUS,
    AUDIO_FORMAT_WAV,
    AUDIO_FORMAT_AAC,
    AUDIO_FORMAT_FLAC
} AudioFormat;

/**
 * The names of the audio formats
 * @see AudioFormat
 **/
extern const STRPTR AUDIO_FORMAT_NAMES[];

/**
 * Initialise the speech system
 * @param speechSystem the speech system to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG initSpeech(SpeechSystem speechSystem);

/**
 * Speak the given text aloud
 * @param text the text to speak
 * @param output the output file to save the OpenAI audio to. If NULL, the audio
 * will be played through AHI.
 * @param audioFormat the audio format to save the audio to
 **/
void speakText(STRPTR text, CONST_STRPTR output, AudioFormat *audioFormat);

/**
 * Close the speech system
 **/
void closeSpeech();

#endif // SPEECH_H