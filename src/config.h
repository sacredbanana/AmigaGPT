#include "openai.h"
#include "speech.h"

#define CHAT_MODEL_SET_VERSION 3
#define IMAGE_MODEL_SET_VERSION 0
#define SPEECH_SYSTEM_SET_VERSION 1
#define OPENAI_TTS_MODEL_SET_VERSION 0
#define OPENAI_TTS_VOICE_SET_VERSION 0

struct Config {
	ULONG speechEnabled; // Making it a ULONG so it can be directly triggered. BOOL is too small
	enum SpeechSystem speechSystem;
	#ifdef __AMIGAOS3__
	STRPTR speechAccent;
	#else
	#ifdef __AMIGAOS4__
	enum SpeechFliteVoice speechFliteVoice;
	#endif
	#endif
	STRPTR chatSystem;
	enum ChatModel chatModel;
	enum ImageModel imageModel;
	enum ImageSize imageSizeDallE2;
	enum ImageSize imageSizeDallE3;
	enum OpenAITTSModel openAITTSModel;
	enum OpenAITTSVoice openAITTSVoice;
	STRPTR chatFontName;
	UWORD chatFontSize;
	UBYTE chatFontStyle;
	UBYTE chatFontFlags;
	STRPTR uiFontName;
	UWORD uiFontSize;
	UBYTE uiFontStyle;
	UBYTE uiFontFlags;
	STRPTR openAiApiKey;
	ULONG colors[16 * 3 + 2];
	UWORD chatModelSetVersion;  // This is used to determine if the chat model set has changed and so the selected model should be reset to the default
	UWORD imageModelSetVersion; // Ditto for the image model set
	UWORD speechSystemSetVersion; // Ditto for the speech system
	UWORD openAITTSModelSetVersion;   // Ditto for the TTS model set
	UWORD openAITTSVoiceSetVersion;   // Ditto for the TTS voice set
};

/**
 * The global app config
**/
extern struct Config config;

/**
 * Write the config to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG writeConfig();

/**
 * Read the config from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG readConfig();

/**
 * Free the config
**/ 
void freeConfig();