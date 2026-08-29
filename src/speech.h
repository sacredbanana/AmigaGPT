#ifndef SPEECH_H
#define SPEECH_H

#include <devices/narrator.h>
#include <proto/dos.h>

#ifdef __AMIGAOS3__
extern struct narrator_rb *NarratorIO;
#elif defined(__AMIGAOS4__)
extern struct FliteRequest *fliteRequest;
#endif
extern struct AHIRequest *ahiRequest;

/**
 * A speech system
 **/
typedef enum {
    SPEECH_SYSTEM_34 = 0L,
    SPEECH_SYSTEM_37,
    SPEECH_SYSTEM_FLITE,
    SPEECH_SYSTEM_OPENAI,
    SPEECH_SYSTEM_ELEVENLABS,
    SPEECH_SYSTEM_XAI,
    SPEECH_SYSTEM_OPENVOX
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

/* Speak using an explicitly provided settings struct (no config reads/writes).
 */
struct SpeechRequestSettings;
BOOL speakTextWithSettings(STRPTR text, CONST_STRPTR output,
                           AudioFormat *audioFormat,
                           const struct SpeechRequestSettings *settings);

/**
 * Play a saved mono PCM WAV file through AHI without contacting a speech
 * provider. Starts from the beginning of the file.
 **/
BOOL playSpeechFile(CONST_STRPTR filename);

/**
 * Decode a saved WAV into the playback buffer without starting AHI. The
 * same file can then be played, paused, seeked and rewound.
 **/
BOOL loadSpeechPlayback(CONST_STRPTR filename);

/** Abort playback and discard the loaded file buffer. */
void unloadSpeechPlayback(void);

/** Play or resume the loaded WAV from the current position. */
BOOL startSpeechPlayback(void);

/** Abort the current AHI write and keep the playhead where it is. */
void pauseSpeech(void);

/** Seek the loaded WAV to an offset in milliseconds. */
BOOL seekSpeech(ULONG positionMs);

/** Seek the loaded WAV to the start. Continues playing if it was playing. */
void rewindSpeech(void);

/** Stop any narrator, flite or AHI playback currently in progress. */
void stopSpeech();

/**
 * TRUE if narrator, flite or AHI playback is still in flight. Reaps a
 * completed request so a later call reports idle.
 **/
BOOL isSpeechPlaying(void);

/** TRUE if a WAV is loaded and the playhead is held mid-file. */
BOOL isSpeechPaused(void);

/** Playhead position of the loaded WAV, in milliseconds. */
ULONG speechPlaybackPositionMs(void);

/** Duration of the loaded WAV, in milliseconds. Zero if nothing is loaded. */
ULONG speechPlaybackDurationMs(void);

/** TRUE if a decoded file is sitting in the playback buffer. */
BOOL speechPlaybackHasAudio(void);

/** Pointer to the decoded PCM currently loaded for file playback. */
const UBYTE *speechPlaybackSamples(void);

/** Number of PCM bytes in the loaded file, not including the AHI pad. */
ULONG speechPlaybackSampleBytes(void);

/** Sample rate of the loaded file. */
ULONG speechPlaybackSampleRate(void);

/** 1 for mono, 2 for stereo. */
UWORD speechPlaybackChannelCount(void);

/** 8 or 16. Loaded PCM is signed; 16-bit samples are big-endian. */
UWORD speechPlaybackBitsPerSample(void);

/**
 * Reap a finished playhead timer and AHI write. Call this from the GUI
 * idle loop so the waveform can advance while audio is playing.
 **/
void speechServicePlayback(void);

/**
 * Exec signal bits for in-flight playback message ports. OR these into
 * Wait() so the UI wakes when playback ends.
 **/
ULONG speechPlaybackSignalMask(void);

/**
 * Close the speech system
 **/
void closeSpeech();

#endif // SPEECH_H
