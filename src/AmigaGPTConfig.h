#ifndef AMIGAGPTCONFIG_H
#define AMIGAGPTCONFIG_H

#include <exec/types.h>
#include <libraries/mui.h>
#include "openai.h"
#include "speech.h"

/* Config version - increment when schema changes to trigger migration */
#define CONFIG_SCHEMA_VERSION 4

/* Legacy version tracking for enum-based models (deprecated) */
#define CHAT_MODEL_SET_VERSION 10
#define IMAGE_MODEL_SET_VERSION 2
#define SPEECH_SYSTEM_SET_VERSION 1
#define OPENAI_TTS_MODEL_SET_VERSION 1
#define OPENAI_TTS_VOICE_SET_VERSION 1

#define ELEVENLABS_MODEL_V3_ID "eleven_v3"
#define ELEVENLABS_MODEL_V3_NAME "Eleven v3"
#define ELEVENLABS_MODEL_MULTILINGUAL_V2_ID "eleven_multilingual_v2"
#define ELEVENLABS_MODEL_MULTILINGUAL_V2_NAME "Eleven Multilingual v2"
#define ELEVENLABS_MODEL_FLASH_V2_5_ID "eleven_flash_v2_5"
#define ELEVENLABS_MODEL_FLASH_V2_5_NAME "Eleven Flash v2.5"
#define ELEVENLABS_MODEL_TURBO_V2_5_ID "eleven_turbo_v2_5"
#define ELEVENLABS_MODEL_TURBO_V2_5_NAME "Eleven Turbo v2.5"

/* Base tag for AmigaGPTConfig attributes */
#define AmigaGPTConfig_Dummy (0xbe000000UL)

/* Boolean/Integer attributes (ULONG) */
#define MUIA_AmigaGPTConfig_SpeechEnabled (AmigaGPTConfig_Dummy + 0x01)
#define MUIA_AmigaGPTConfig_SpeechSystem (AmigaGPTConfig_Dummy + 0x02)
#define MUIA_AmigaGPTConfig_SpeechFliteVoice (AmigaGPTConfig_Dummy + 0x03)
#define MUIA_AmigaGPTConfig_ChatModel (AmigaGPTConfig_Dummy + 0x04)
#define MUIA_AmigaGPTConfig_ImageModel (AmigaGPTConfig_Dummy + 0x05)
#define MUIA_AmigaGPTConfig_ImageSizeDallE2 (AmigaGPTConfig_Dummy + 0x06)
#define MUIA_AmigaGPTConfig_ImageSizeDallE3 (AmigaGPTConfig_Dummy + 0x07)
#define MUIA_AmigaGPTConfig_ImageSizeGptImage1 (AmigaGPTConfig_Dummy + 0x08)
#define MUIA_AmigaGPTConfig_OpenAITTSModel (AmigaGPTConfig_Dummy + 0x09)
#define MUIA_AmigaGPTConfig_OpenAITTSVoice (AmigaGPTConfig_Dummy + 0x0A)
#define MUIA_AmigaGPTConfig_ProxyEnabled (AmigaGPTConfig_Dummy + 0x0B)
#define MUIA_AmigaGPTConfig_ProxyPort (AmigaGPTConfig_Dummy + 0x0C)
#define MUIA_AmigaGPTConfig_ProxyUsesSSL (AmigaGPTConfig_Dummy + 0x0D)
#define MUIA_AmigaGPTConfig_ProxyRequiresAuth (AmigaGPTConfig_Dummy + 0x0E)
#define MUIA_AmigaGPTConfig_FixedWidthFonts (AmigaGPTConfig_Dummy + 0x0F)
#define MUIA_AmigaGPTConfig_UserTextAlignment (AmigaGPTConfig_Dummy + 0x10)
#define MUIA_AmigaGPTConfig_AssistantTextAlignment (AmigaGPTConfig_Dummy + 0x11)
#define MUIA_AmigaGPTConfig_WebSearchEnabled (AmigaGPTConfig_Dummy + 0x12)
#define MUIA_AmigaGPTConfig_ShellToolEnabled (AmigaGPTConfig_Dummy + 0x18)
#define MUIA_AmigaGPTConfig_CustomChatStreamEnabled                            \
    (AmigaGPTConfig_Dummy + 0x19)
#define MUIA_AmigaGPTConfig_OpenAiChatStreamEnabled                            \
    (AmigaGPTConfig_Dummy + 0x1A)
#define MUIA_AmigaGPTConfig_GeminiChatStreamEnabled                            \
    (AmigaGPTConfig_Dummy + 0x1B)
#define MUIA_AmigaGPTConfig_GrokChatStreamEnabled (AmigaGPTConfig_Dummy + 0x1C)
#define MUIA_AmigaGPTConfig_AnthropicChatStreamEnabled                         \
    (AmigaGPTConfig_Dummy + 0x1D)
#define MUIA_AmigaGPTConfig_CustomPort (AmigaGPTConfig_Dummy + 0x14)
#define MUIA_AmigaGPTConfig_CustomUseSSL (AmigaGPTConfig_Dummy + 0x15)
#define MUIA_AmigaGPTConfig_CustomApiEndpoint (AmigaGPTConfig_Dummy + 0x16)
#define MUIA_AmigaGPTConfig_ImageFormat (AmigaGPTConfig_Dummy + 0x17)
#define MUIA_AmigaGPTConfig_ImageApiEndpoint (AmigaGPTConfig_Dummy + 0x53)

/* Workbench narrator.device settings (AmigaOS 3.x profiles) */
#define MUIA_AmigaGPTConfig_NarratorRate34 (AmigaGPTConfig_Dummy + 0x6B)
#define MUIA_AmigaGPTConfig_NarratorPitch34 (AmigaGPTConfig_Dummy + 0x6C)
#define MUIA_AmigaGPTConfig_NarratorMode34 (AmigaGPTConfig_Dummy + 0x6D)
#define MUIA_AmigaGPTConfig_NarratorSex34 (AmigaGPTConfig_Dummy + 0x6E)
#define MUIA_AmigaGPTConfig_NarratorRate37 (AmigaGPTConfig_Dummy + 0x6F)
#define MUIA_AmigaGPTConfig_NarratorPitch37 (AmigaGPTConfig_Dummy + 0x70)
#define MUIA_AmigaGPTConfig_NarratorMode37 (AmigaGPTConfig_Dummy + 0x71)
#define MUIA_AmigaGPTConfig_NarratorSex37 (AmigaGPTConfig_Dummy + 0x72)

/* String attributes (STRPTR) */
#define MUIA_AmigaGPTConfig_SpeechAccent (AmigaGPTConfig_Dummy + 0x20)
#define MUIA_AmigaGPTConfig_SpeechAccent34 (AmigaGPTConfig_Dummy + 0x67)
#define MUIA_AmigaGPTConfig_SpeechAccent37 (AmigaGPTConfig_Dummy + 0x68)
#define MUIA_AmigaGPTConfig_ChatSystem (AmigaGPTConfig_Dummy + 0x21)
#define MUIA_AmigaGPTConfig_OpenAIVoiceInstructions                            \
    (AmigaGPTConfig_Dummy + 0x22)
#define MUIA_AmigaGPTConfig_OpenAiApiKey (AmigaGPTConfig_Dummy + 0x23)
#define MUIA_AmigaGPTConfig_ProxyHost (AmigaGPTConfig_Dummy + 0x24)
#define MUIA_AmigaGPTConfig_ProxyUsername (AmigaGPTConfig_Dummy + 0x25)
#define MUIA_AmigaGPTConfig_ProxyPassword (AmigaGPTConfig_Dummy + 0x26)
#define MUIA_AmigaGPTConfig_CustomHost (AmigaGPTConfig_Dummy + 0x27)
#define MUIA_AmigaGPTConfig_CustomApiKey (AmigaGPTConfig_Dummy + 0x28)
#define MUIA_AmigaGPTConfig_CustomChatModel (AmigaGPTConfig_Dummy + 0x29)
#define MUIA_AmigaGPTConfig_CustomApiEndpointUrl (AmigaGPTConfig_Dummy + 0x2A)
#define MUIA_AmigaGPTConfig_ElevenLabsAPIKey (AmigaGPTConfig_Dummy + 0x2B)
#define MUIA_AmigaGPTConfig_ElevenLabsVoiceID (AmigaGPTConfig_Dummy + 0x2C)
#define MUIA_AmigaGPTConfig_ElevenLabsVoiceName (AmigaGPTConfig_Dummy + 0x2D)
#define MUIA_AmigaGPTConfig_ElevenLabsModel (AmigaGPTConfig_Dummy + 0x2E)
#define MUIA_AmigaGPTConfig_ElevenLabsModelName (AmigaGPTConfig_Dummy + 0x2F)
#define MUIA_AmigaGPTConfig_CustomAuthorizationType                            \
    (AmigaGPTConfig_Dummy + 0x30)
#define MUIA_AmigaGPTConfig_CustomHeaders (AmigaGPTConfig_Dummy + 0x31)
#define MUIA_AmigaGPTConfig_CustomServerProfiles (AmigaGPTConfig_Dummy + 0x32)
#define MUIA_AmigaGPTConfig_ActiveProfileName (AmigaGPTConfig_Dummy + 0x33)
#define MUIA_AmigaGPTConfig_ChatModelName (AmigaGPTConfig_Dummy + 0x36)
#define MUIA_AmigaGPTConfig_ImageModelName (AmigaGPTConfig_Dummy + 0x37)
#define MUIA_AmigaGPTConfig_GeminiApiKey (AmigaGPTConfig_Dummy + 0x38)
#define MUIA_AmigaGPTConfig_GrokApiKey (AmigaGPTConfig_Dummy + 0x39)
#define MUIA_AmigaGPTConfig_AnthropicApiKey (AmigaGPTConfig_Dummy + 0x3A)
#define MUIA_AmigaGPTConfig_OpenAiChatModelName (AmigaGPTConfig_Dummy + 0x3B)
#define MUIA_AmigaGPTConfig_GeminiChatModelName (AmigaGPTConfig_Dummy + 0x3C)
#define MUIA_AmigaGPTConfig_GrokChatModelName (AmigaGPTConfig_Dummy + 0x3D)
#define MUIA_AmigaGPTConfig_AnthropicChatModelName (AmigaGPTConfig_Dummy + 0x3E)
/* Provider-specific image model names (locked image profiles) */
#define MUIA_AmigaGPTConfig_OpenAiImageModelName (AmigaGPTConfig_Dummy + 0x45)
#define MUIA_AmigaGPTConfig_GeminiImageModelName (AmigaGPTConfig_Dummy + 0x46)
#define MUIA_AmigaGPTConfig_GrokImageModelName (AmigaGPTConfig_Dummy + 0x47)

/* Custom image provider settings (separate from chat custom provider) */
#define MUIA_AmigaGPTConfig_CustomImagePort (AmigaGPTConfig_Dummy + 0x1E)
#define MUIA_AmigaGPTConfig_CustomImageUseSSL (AmigaGPTConfig_Dummy + 0x1F)
#define MUIA_AmigaGPTConfig_CustomImageAuthorizationType                       \
    (AmigaGPTConfig_Dummy + 0x52)

#define MUIA_AmigaGPTConfig_CustomImageHost (AmigaGPTConfig_Dummy + 0x60)
#define MUIA_AmigaGPTConfig_CustomImageApiKey (AmigaGPTConfig_Dummy + 0x61)
#define MUIA_AmigaGPTConfig_CustomImageModel (AmigaGPTConfig_Dummy + 0x62)
#define MUIA_AmigaGPTConfig_CustomImageApiEndpointUrl                          \
    (AmigaGPTConfig_Dummy + 0x63)
#define MUIA_AmigaGPTConfig_CustomImageHeaders (AmigaGPTConfig_Dummy + 0x64)
#define MUIA_AmigaGPTConfig_CustomImageServerProfiles                          \
    (AmigaGPTConfig_Dummy + 0x65)
#define MUIA_AmigaGPTConfig_ActiveImageProfileName (AmigaGPTConfig_Dummy + 0x66)

/* Speech provider profiles */
#define MUIA_AmigaGPTConfig_SpeechProfiles (AmigaGPTConfig_Dummy + 0x69)
#define MUIA_AmigaGPTConfig_ActiveSpeechProfileName (AmigaGPTConfig_Dummy + 0x6A)

/* Per-locked-profile web search */
#define MUIA_AmigaGPTConfig_OpenAiWebSearchEnabled                             \
    (AmigaGPTConfig_Dummy + 0x73)
#define MUIA_AmigaGPTConfig_GeminiWebSearchEnabled                             \
    (AmigaGPTConfig_Dummy + 0x74)
#define MUIA_AmigaGPTConfig_GrokWebSearchEnabled (AmigaGPTConfig_Dummy + 0x75)
#define MUIA_AmigaGPTConfig_AnthropicWebSearchEnabled                          \
    (AmigaGPTConfig_Dummy + 0x76)

/* Per-locked-profile shell tool */
#define MUIA_AmigaGPTConfig_OpenAiShellToolEnabled                             \
    (AmigaGPTConfig_Dummy + 0x77)
#define MUIA_AmigaGPTConfig_GeminiShellToolEnabled                             \
    (AmigaGPTConfig_Dummy + 0x78)
#define MUIA_AmigaGPTConfig_GrokShellToolEnabled (AmigaGPTConfig_Dummy + 0x79)
#define MUIA_AmigaGPTConfig_AnthropicShellToolEnabled                          \
    (AmigaGPTConfig_Dummy + 0x7A)

/* Per-locked-profile chat system prompt */
#define MUIA_AmigaGPTConfig_OpenAiChatSystem (AmigaGPTConfig_Dummy + 0x7B)
#define MUIA_AmigaGPTConfig_GeminiChatSystem (AmigaGPTConfig_Dummy + 0x7C)
#define MUIA_AmigaGPTConfig_GrokChatSystem (AmigaGPTConfig_Dummy + 0x7D)
#define MUIA_AmigaGPTConfig_AnthropicChatSystem (AmigaGPTConfig_Dummy + 0x7E)

/* Per-mode (image/speech) API keys for locked providers */
#define MUIA_AmigaGPTConfig_OpenAiImageApiKey (AmigaGPTConfig_Dummy + 0x82)
#define MUIA_AmigaGPTConfig_OpenAiSpeechApiKey (AmigaGPTConfig_Dummy + 0x83)
#define MUIA_AmigaGPTConfig_GeminiImageApiKey (AmigaGPTConfig_Dummy + 0x84)
#define MUIA_AmigaGPTConfig_GrokImageApiKey (AmigaGPTConfig_Dummy + 0x85)

/* OpenAI TTS model id (string). Replaces the legacy enum-based
 * MUIA_AmigaGPTConfig_OpenAITTSModel for selection; the enum field is kept for
 * backwards-compatible reads from older configs. */
#define MUIA_AmigaGPTConfig_OpenAITTSModelId (AmigaGPTConfig_Dummy + 0x86)

/* xAI TTS voice (ULONG, XAITTSVoice enum). */
#define MUIA_AmigaGPTConfig_XAITTSVoice (AmigaGPTConfig_Dummy + 0x87)

/* xAI TTS voice id (string, supports custom voices). */
#define MUIA_AmigaGPTConfig_XAITTSVoiceId (AmigaGPTConfig_Dummy + 0x88)

/* Version tracking attributes (read-only, for internal use) */
#define MUIA_AmigaGPTConfig_ChatModelSetVersion (AmigaGPTConfig_Dummy + 0x40)
#define MUIA_AmigaGPTConfig_ImageModelSetVersion (AmigaGPTConfig_Dummy + 0x41)
#define MUIA_AmigaGPTConfig_SpeechSystemSetVersion (AmigaGPTConfig_Dummy + 0x42)
#define MUIA_AmigaGPTConfig_OpenAITTSModelSetVersion                           \
    (AmigaGPTConfig_Dummy + 0x43)
#define MUIA_AmigaGPTConfig_OpenAITTSVoiceSetVersion                           \
    (AmigaGPTConfig_Dummy + 0x44)

/* Methods */
#define MUIM_AmigaGPTConfig_Load (AmigaGPTConfig_Dummy + 0x80)
#define MUIM_AmigaGPTConfig_Save (AmigaGPTConfig_Dummy + 0x81)

/* Class pointer */
#define MUIC_AmigaGPTConfig amigaGPTConfigClass->mcc_Class

#if defined(__AROS__) && !defined(NO_INLINE_STDARG)
#define AmigaGPTConfigObject MUIOBJMACRO_START(MUIC_AmigaGPTConfig)
#else
#define AmigaGPTConfigObject NewObject(MUIC_AmigaGPTConfig, NULL
#endif

/**
 * @brief The AmigaGPTConfig MUI custom class
 * @note This class manages all application configuration and automatically
 * saves to disk when attributes change
 */
extern struct MUI_CustomClass *amigaGPTConfigClass;

/**
 * @brief The global config object instance
 */
extern Object *configObj;

/* ============================================== */
/* Speech provider request settings               */
/* ============================================== */

struct SpeechRequestSettings {
    SpeechSystem speechSystem;
    STRPTR activeProfileName;
    STRPTR accentPath;
    SpeechFliteVoice fliteVoice;

    /* Workbench narrator.device settings (rate is words/minute, pitch in Hz) */
    UWORD narratorRate;
    UWORD narratorPitch;
    /* mode: 0 natural, 1 robotic */
    UWORD narratorMode;
    /* sex: 0 male, 1 female */
    UWORD narratorSex;

    /* OpenAI TTS */
    STRPTR host;
    UWORD port;
    BOOL useSSL;
    STRPTR apiEndpointUrl;
    AuthorizationType authorizationType;
    STRPTR openAiApiKey;
    STRPTR openAiTtsModelId;
    OpenAITTSVoice openAiTtsVoice;
    STRPTR openAiVoiceInstructions;

    /* ElevenLabs */
    STRPTR elevenLabsApiKey;
    STRPTR elevenLabsVoiceID;
    STRPTR elevenLabsVoiceName;
    STRPTR elevenLabsModel;
    STRPTR elevenLabsModelName;

    /* xAI TTS */
    STRPTR xaiApiKey;
    /* Preferred form: a voice_id string (works for built-in *and* custom
     * voices fetched from /v1/custom-voices). When empty, fall back to the
     * legacy enum-based xaiVoice. */
    STRPTR xaiVoiceId;
    XAITTSVoice xaiVoice;
    STRPTR xaiLanguage;
};

void configGetSpeechRequestSettings(struct SpeechRequestSettings *out);
void configFreeSpeechRequestSettings(struct SpeechRequestSettings *out);

/**
 * @brief Create the AmigaGPTConfig class
 * @return RETURN_OK if successful, RETURN_ERROR otherwise
 */
LONG createAmigaGPTConfigClass(void);

/**
 * @brief Delete the AmigaGPTConfig class
 */
void deleteAmigaGPTConfigClass(void);

/**
 * @brief Initialize the global config object and load settings
 * @return RETURN_OK if successful, RETURN_ERROR otherwise
 */
LONG initConfig(void);

/**
 * @brief Cleanup and dispose the global config object
 */
void cleanupConfig(void);

/**
 * @brief Initialize the MUI config class and create the config object.
 * Call this AFTER MUIMasterBase is open (after initVideo() opens libraries).
 * @return RETURN_OK on success, RETURN_ERROR on failure
 */
LONG initConfigMUI(void);

/**
 * @brief Free the config and cleanup all resources
 */
void freeConfig(void);

/* Convenience macros for getting/setting config values */
#define CONFIG_GET(attr, var) get(configObj, attr, &(var))
#define CONFIG_SET(attr, val) set(configObj, attr, (ULONG)(val))

/*
 * Config accessor functions
 * Getters return the current value
 * Setters update the value and trigger auto-save
 */

/* Speech settings */
ULONG configGetSpeechEnabled(void);
void configSetSpeechEnabled(ULONG value);
SpeechSystem configGetSpeechSystem(void);
void configSetSpeechSystem(SpeechSystem value);
STRPTR configGetSpeechAccent(void);
void configSetSpeechAccent(CONST_STRPTR value);
STRPTR configGetSpeechAccent34(void);
void configSetSpeechAccent34(CONST_STRPTR value);
STRPTR configGetSpeechAccent37(void);
void configSetSpeechAccent37(CONST_STRPTR value);
ULONG configGetNarratorRate34(void);
void configSetNarratorRate34(ULONG value);
ULONG configGetNarratorPitch34(void);
void configSetNarratorPitch34(ULONG value);
ULONG configGetNarratorMode34(void);
void configSetNarratorMode34(ULONG value);
ULONG configGetNarratorSex34(void);
void configSetNarratorSex34(ULONG value);
ULONG configGetNarratorRate37(void);
void configSetNarratorRate37(ULONG value);
ULONG configGetNarratorPitch37(void);
void configSetNarratorPitch37(ULONG value);
ULONG configGetNarratorMode37(void);
void configSetNarratorMode37(ULONG value);
ULONG configGetNarratorSex37(void);
void configSetNarratorSex37(ULONG value);
STRPTR configGetSpeechProfiles(void);
void configSetSpeechProfiles(CONST_STRPTR value);
STRPTR configGetActiveSpeechProfileName(void);
void configSetActiveSpeechProfileName(CONST_STRPTR value);
SpeechFliteVoice configGetSpeechFliteVoice(void);
void configSetSpeechFliteVoice(SpeechFliteVoice value);

/* Chat settings */
STRPTR configGetChatSystem(void);
void configSetChatSystem(CONST_STRPTR value);
ChatModel configGetChatModel(void);
void configSetChatModel(ChatModel value);

/* Image settings */
ImageModel configGetImageModel(void);
void configSetImageModel(ImageModel value);
ImageSize configGetImageSizeDallE2(void);
void configSetImageSizeDallE2(ImageSize value);
ImageSize configGetImageSizeDallE3(void);
void configSetImageSizeDallE3(ImageSize value);
ImageSize configGetImageSizeGptImage1(void);
void configSetImageSizeGptImage1(ImageSize value);
ImageFormat configGetImageFormat(void);
APIImageEndpoint configGetImageApiEndpoint(void);
void configSetImageFormat(ImageFormat value);
void configSetImageApiEndpoint(APIImageEndpoint value);

/* OpenAI TTS settings */
OpenAITTSModel configGetOpenAITTSModel(void);
void configSetOpenAITTSModel(OpenAITTSModel value);
STRPTR configGetOpenAITTSModelId(void);
void configSetOpenAITTSModelId(CONST_STRPTR value);
OpenAITTSVoice configGetOpenAITTSVoice(void);
void configSetOpenAITTSVoice(OpenAITTSVoice value);
STRPTR configGetOpenAIVoiceInstructions(void);
void configSetOpenAIVoiceInstructions(CONST_STRPTR value);

/* xAI TTS settings */
XAITTSVoice configGetXAITTSVoice(void);
void configSetXAITTSVoice(XAITTSVoice value);
STRPTR configGetXAITTSVoiceId(void);
void configSetXAITTSVoiceId(CONST_STRPTR value);

/* Proxy settings */
ULONG configGetProxyEnabled(void);
void configSetProxyEnabled(ULONG value);
STRPTR configGetProxyHost(void);
void configSetProxyHost(CONST_STRPTR value);
ULONG configGetProxyPort(void);
void configSetProxyPort(ULONG value);
ULONG configGetProxyUsesSSL(void);
void configSetProxyUsesSSL(ULONG value);
ULONG configGetProxyRequiresAuth(void);
void configSetProxyRequiresAuth(ULONG value);
STRPTR configGetProxyUsername(void);
void configSetProxyUsername(CONST_STRPTR value);
STRPTR configGetProxyPassword(void);
void configSetProxyPassword(CONST_STRPTR value);

/* View settings */
ULONG configGetFixedWidthFonts(void);
void configSetFixedWidthFonts(ULONG value);
LONG configGetUserTextAlignment(void);
void configSetUserTextAlignment(LONG value);
LONG configGetAssistantTextAlignment(void);
void configSetAssistantTextAlignment(LONG value);

/* Web search */
LONG configGetWebSearchEnabled(void);
void configSetWebSearchEnabled(LONG value);

/* Shell tool */
LONG configGetShellToolEnabled(void);
void configSetShellToolEnabled(LONG value);

/* Custom provider settings */
STRPTR configGetCustomHost(void);
void configSetCustomHost(CONST_STRPTR value);
ULONG configGetCustomPort(void);
void configSetCustomPort(ULONG value);
ULONG configGetCustomUseSSL(void);
void configSetCustomUseSSL(ULONG value);
STRPTR configGetCustomApiKey(void);
void configSetCustomApiKey(CONST_STRPTR value);
STRPTR configGetCustomChatModel(void);
void configSetCustomChatModel(CONST_STRPTR value);
APIChatEndpoint configGetCustomApiEndpoint(void);
void configSetCustomApiEndpoint(APIChatEndpoint value);
STRPTR configGetCustomApiEndpointUrl(void);
void configSetCustomApiEndpointUrl(CONST_STRPTR value);
AuthorizationType configGetCustomAuthorizationType(void);
void configSetCustomAuthorizationType(AuthorizationType value);
STRPTR configGetCustomHeaders(void);
void configSetCustomHeaders(CONST_STRPTR value);
STRPTR configGetCustomServerProfiles(void);
void configSetCustomServerProfiles(CONST_STRPTR value);
STRPTR configGetActiveProfileName(void);
void configSetActiveProfileName(CONST_STRPTR value);
BOOL configGetCustomChatStreamEnabled(void);
void configSetCustomChatStreamEnabled(BOOL enabled);

/* Custom image provider settings (separate from chat custom provider) */
STRPTR configGetCustomImageHost(void);
void configSetCustomImageHost(CONST_STRPTR value);
ULONG configGetCustomImagePort(void);
void configSetCustomImagePort(ULONG value);
ULONG configGetCustomImageUseSSL(void);
void configSetCustomImageUseSSL(ULONG value);
AuthorizationType configGetCustomImageAuthorizationType(void);
void configSetCustomImageAuthorizationType(AuthorizationType value);
STRPTR configGetCustomImageApiKey(void);
void configSetCustomImageApiKey(CONST_STRPTR value);
STRPTR configGetCustomImageModel(void);
void configSetCustomImageModel(CONST_STRPTR value);
STRPTR configGetCustomImageApiEndpointUrl(void);
void configSetCustomImageApiEndpointUrl(CONST_STRPTR value);
STRPTR configGetCustomImageHeaders(void);
void configSetCustomImageHeaders(CONST_STRPTR value);
STRPTR configGetCustomImageServerProfiles(void);
void configSetCustomImageServerProfiles(CONST_STRPTR value);
STRPTR configGetActiveImageProfileName(void);
void configSetActiveImageProfileName(CONST_STRPTR value);

/* ElevenLabs settings */
STRPTR configGetElevenLabsAPIKey(void);
void configSetElevenLabsAPIKey(CONST_STRPTR value);
STRPTR configGetElevenLabsVoiceID(void);
void configSetElevenLabsVoiceID(CONST_STRPTR value);
STRPTR configGetElevenLabsVoiceName(void);
void configSetElevenLabsVoiceName(CONST_STRPTR value);
STRPTR configGetElevenLabsModel(void);
void configSetElevenLabsModel(CONST_STRPTR value);
STRPTR configGetElevenLabsModelName(void);
void configSetElevenLabsModelName(CONST_STRPTR value);

/* Active chat request settings helper (driven by active profile name) */
struct ChatRequestSettings {
    CONST_STRPTR profileName;
    STRPTR host;
    UWORD port;
    BOOL useSSL;
    CONST_STRPTR model;
    CONST_STRPTR apiKey;
    BOOL stream;
    BOOL useProxy;
    STRPTR proxyHost;
    UWORD proxyPort;
    BOOL proxyUsesSSL;
    BOOL proxyRequiresAuth;
    STRPTR proxyUsername;
    STRPTR proxyPassword;
    BOOL webSearchEnabled;
    BOOL shellToolEnabled;
    APIChatEndpoint apiEndpoint;
    CONST_STRPTR apiEndpointUrl;
    AuthorizationType authorizationType;
    CONST_STRPTR customHeaders;
    CONST_STRPTR chatSystem;
};

void configGetActiveChatRequestSettings(struct ChatRequestSettings *out);
void configGetChatRequestSettingsWithStreamOverride(
    struct ChatRequestSettings *out, BOOL streamOverride);
STRPTR configGetImageModelName(void);
void configSetImageModelName(CONST_STRPTR value);
STRPTR configGetOpenAiImageModelName(void);
void configSetOpenAiImageModelName(CONST_STRPTR value);
STRPTR configGetGeminiImageModelName(void);
void configSetGeminiImageModelName(CONST_STRPTR value);
STRPTR configGetGrokImageModelName(void);
void configSetGrokImageModelName(CONST_STRPTR value);

/* Provider-specific API keys (chat mode -- the "primary" key) */
STRPTR configGetOpenAiApiKey(void);
void configSetOpenAiApiKey(CONST_STRPTR value);
STRPTR configGetGeminiApiKey(void);
void configSetGeminiApiKey(CONST_STRPTR value);
STRPTR configGetGrokApiKey(void);
void configSetGrokApiKey(CONST_STRPTR value);
STRPTR configGetAnthropicApiKey(void);
void configSetAnthropicApiKey(CONST_STRPTR value);

/* Provider-specific API keys (image mode) */
STRPTR configGetOpenAiImageApiKey(void);
void configSetOpenAiImageApiKey(CONST_STRPTR value);
STRPTR configGetGeminiImageApiKey(void);
void configSetGeminiImageApiKey(CONST_STRPTR value);
STRPTR configGetGrokImageApiKey(void);
void configSetGrokImageApiKey(CONST_STRPTR value);

/* Provider-specific API keys (speech mode -- OpenAI only) */
STRPTR configGetOpenAiSpeechApiKey(void);
void configSetOpenAiSpeechApiKey(CONST_STRPTR value);

/* Image request settings helper (driven by active image profile name) */
struct ImageRequestSettings {
    CONST_STRPTR profileName;
    STRPTR host;
    UWORD port;
    BOOL useSSL;
    CONST_STRPTR model;
    CONST_STRPTR apiKey;
    BOOL useProxy;
    STRPTR proxyHost;
    UWORD proxyPort;
    BOOL proxyUsesSSL;
    BOOL proxyRequiresAuth;
    STRPTR proxyUsername;
    STRPTR proxyPassword;
    CONST_STRPTR apiEndpointUrl;
    AuthorizationType authorizationType;
    CONST_STRPTR customHeaders;
    APIImageEndpoint imageApiEndpoint;
};

void configGetActiveImageRequestSettings(struct ImageRequestSettings *out);

#endif /* AMIGAGPTCONFIG_H */
