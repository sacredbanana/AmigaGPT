#include <libraries/codesets.h>
#include "openai.h"
#include "speech.h"

#define CHAT_MODEL_SET_VERSION 10
#define IMAGE_MODEL_SET_VERSION 2
#define SPEECH_SYSTEM_SET_VERSION 1
#define OPENAI_TTS_MODEL_SET_VERSION 1
#define OPENAI_TTS_VOICE_SET_VERSION 1

struct Config {
    ULONG speechEnabled; // Making it a ULONG so it can be directly triggered.
                         // BOOL is too small
    SpeechSystem speechSystem;
    STRPTR speechAccent;
    SpeechFliteVoice speechFliteVoice;
    STRPTR chatSystem;
    ChatModel chatModel;
    ImageModel imageModel;
    ImageSize imageSizeDallE2;
    ImageSize imageSizeDallE3;
    ImageSize imageSizeGptImage1;
    OpenAITTSModel openAITTSModel;
    OpenAITTSVoice openAITTSVoice;
    STRPTR openAIVoiceInstructions;
    STRPTR openAiApiKey;
    UWORD chatModelSetVersion;    // This is used to determine if the chat model
                                  // set has changed and so the selected model
                                  // should be reset to the default
    UWORD imageModelSetVersion;   // Ditto for the image model set
    UWORD speechSystemSetVersion; // Ditto for the speech system
    UWORD openAITTSModelSetVersion; // Ditto for the TTS model set
    UWORD openAITTSVoiceSetVersion; // Ditto for the TTS voice set;
    ULONG proxyEnabled;
    STRPTR proxyHost;
    UWORD proxyPort;
    ULONG proxyUsesSSL;
    ULONG proxyRequiresAuth;
    STRPTR proxyUsername;
    STRPTR proxyPassword;
    /** MorphOS: chat Scintilla Mono vs Sans (Ansicht → Feste Schriftbreite); Eingabe = MUI-Standard. */
    ULONG fixedWidthFonts;
    ULONG markdownFormatting;
    /** MorphOS chat Scintilla font size in points (8–24). */
    ULONG chatFontSize;
    /** MorphOS chat Scintilla: SCI_SETWRAPMODE SC_WRAP_WORD when TRUE (Ansicht-Menü). */
    ULONG chatLineWrap;
    LONG userTextAlignment;
    LONG assistantTextAlignment;
    LONG webSearchEnabled;
    ULONG useCustomServer;
    STRPTR customHost;
    ULONG customPort;
    ULONG customUseSSL;
    STRPTR customApiKey;
    STRPTR customChatModel;
    APIEndpoint customApiEndpoint;
    STRPTR customApiEndpointUrl;
    ImageFormat imageFormat;
    STRPTR elevenLabsAPIKey;
    STRPTR elevenLabsVoiceID;
    STRPTR elevenLabsVoiceName;
    STRPTR elevenLabsModel;
    STRPTR elevenLabsModelName;
    /** Phase 9: append T:amigagpt_stream.log (+ KPrintF for LogTool). */
    ULONG debugStreamLog;
    /** MorphOS: AMIGAGPT:amigagpt_lifecycle.log (+ T: mirror, LogTool). Default off. */
    ULONG debugLifecycleLog;
    /** MorphOS main window geometry from config.json (0 = use MUI/Application_Load default). */
    LONG mainWindowLeft;
    LONG mainWindowTop;
    ULONG mainWindowWidth;
    ULONG mainWindowHeight;
};

/**
 * The global app config
 **/
extern struct Config config;

#define CHAT_OUTPUT_FONT_SIZE_DEFAULT 12
#define CHAT_OUTPUT_FONT_SIZE_MIN 8
#define CHAT_OUTPUT_FONT_SIZE_MAX 24

ULONG configClampChatFontSize(ULONG size);

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

/** Allocated copy of src (Phase 8); NULL if src is NULL or OOM. */
STRPTR configDupString(CONST_STRPTR src);

/** Replace *slot with a dup of src; safe when src points into *slot (MUI strings). */
void configAssignString(STRPTR *slot, CONST_STRPTR src);