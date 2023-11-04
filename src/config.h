#include "openai.h"
#include "speech.h"

struct Config {
	// #ifdef __AMIGAOS3__
	BOOL speechEnabled;
	enum SpeechSystem speechSystem;
	UBYTE speechAccent[32];
	// #endif
	enum Model model;
	UBYTE chatFontName[32];
	UWORD chatFontSize;
	UBYTE chatFontStyle;
	UBYTE chatFontFlags;
	UBYTE uiFontName[32];
	UWORD uiFontSize;
	UBYTE uiFontStyle;
	UBYTE uiFontFlags;
	UBYTE openAiApiKey[64];
	ULONG colors[32];
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