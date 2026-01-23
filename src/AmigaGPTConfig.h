#ifndef AMIGAGPTCONFIG_H
#define AMIGAGPTCONFIG_H

#include <exec/types.h>
#include <libraries/mui.h>
#include "openai.h"
#include "speech.h"

#define CHAT_MODEL_SET_VERSION 10
#define IMAGE_MODEL_SET_VERSION 2
#define SPEECH_SYSTEM_SET_VERSION 1
#define OPENAI_TTS_MODEL_SET_VERSION 1
#define OPENAI_TTS_VOICE_SET_VERSION 1

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
#define MUIA_AmigaGPTConfig_UseCustomServer (AmigaGPTConfig_Dummy + 0x13)
#define MUIA_AmigaGPTConfig_CustomPort (AmigaGPTConfig_Dummy + 0x14)
#define MUIA_AmigaGPTConfig_CustomUseSSL (AmigaGPTConfig_Dummy + 0x15)
#define MUIA_AmigaGPTConfig_CustomApiEndpoint (AmigaGPTConfig_Dummy + 0x16)
#define MUIA_AmigaGPTConfig_ImageFormat (AmigaGPTConfig_Dummy + 0x17)

/* String attributes (STRPTR) */
#define MUIA_AmigaGPTConfig_SpeechAccent (AmigaGPTConfig_Dummy + 0x20)
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
void configSetImageFormat(ImageFormat value);

/* OpenAI TTS settings */
OpenAITTSModel configGetOpenAITTSModel(void);
void configSetOpenAITTSModel(OpenAITTSModel value);
OpenAITTSVoice configGetOpenAITTSVoice(void);
void configSetOpenAITTSVoice(OpenAITTSVoice value);
STRPTR configGetOpenAIVoiceInstructions(void);
void configSetOpenAIVoiceInstructions(CONST_STRPTR value);

/* API keys */
STRPTR configGetOpenAiApiKey(void);
void configSetOpenAiApiKey(CONST_STRPTR value);

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

/* Custom server settings */
ULONG configGetUseCustomServer(void);
void configSetUseCustomServer(ULONG value);
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
APIEndpoint configGetCustomApiEndpoint(void);
void configSetCustomApiEndpoint(APIEndpoint value);
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

#endif /* AMIGAGPTCONFIG_H */
