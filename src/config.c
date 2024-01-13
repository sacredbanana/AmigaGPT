#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/text.h>
#include <json-c/json.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "config.h"

struct Config config = {
	.speechEnabled = TRUE,
	#ifdef __AMIGAOS3__
	.speechAccent = "american.accent",
	.speechSystem = SPEECH_SYSTEM_34,
	#else
	.speechVoice = SPEECH_VOICE_KAL,
	.speechSystem = SPEECH_SYSTEM_FLITE,
	#endif
	.chatModel = GPT_3_5_TURBO,
	.imageModel = DALL_E_3,
	.chatFontName = {0},
	.chatFontSize = 8,
	.chatFontStyle = FS_NORMAL,
	.chatFontFlags = FPF_DISKFONT | FPF_DESIGNED,
	.uiFontName = {0},
	.uiFontSize = 8,
	.uiFontStyle = FS_NORMAL,
	.uiFontFlags = FPF_DISKFONT | FPF_DESIGNED,
	.openAiApiKey = NULL,
	.colors = {
		8 << 16,
		0x00000000, 0x00000000, 0x11111111, // darkest blue
		0x00000000, 0x88888888, 0xFFFFFFFF, // light blue
		0xA0000000, 0x00000000, 0x0F0F0F0F, // dark red
		0x00000000, 0x00000000, 0x00000000, // black
		0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, // white
		0x00000000, 0xFFFFFFFF, 0x66666666, // pastel green
		0xFFFFFFFF, 0x00000000, 0x00000000, // red
		0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, // yellow
		0
	}
};

/**
 * Write the config to disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG writeConfig() {
	BPTR file = Open("PROGDIR:config.json", MODE_NEWFILE);
	if (file == 0) {
		printf("Failed to open the config file\n");
		return RETURN_ERROR;
	}

	struct json_object *configJsonObject = json_object_new_object();

	json_object_object_add(configJsonObject, "speechEnabled", json_object_new_boolean(config.speechEnabled));
	json_object_object_add(configJsonObject, "speechSystem", json_object_new_int(config.speechSystem));
	#ifdef __AMIGAOS3__
	json_object_object_add(configJsonObject, "speechAccent", json_object_new_string(config.speechAccent));
	#else
	json_object_object_add(configJsonObject, "speechVoice", json_object_new_int(config.speechVoice));
	#endif
	json_object_object_add(configJsonObject, "chatModel", json_object_new_int(config.chatModel));
	json_object_object_add(configJsonObject, "imageModel", json_object_new_int(config.imageModel));
	json_object_object_add(configJsonObject, "chatFontName", json_object_new_string(config.chatFontName));
	json_object_object_add(configJsonObject, "chatFontSize", json_object_new_int(config.chatFontSize));
	json_object_object_add(configJsonObject, "chatFontStyle", json_object_new_int(config.chatFontStyle));
	json_object_object_add(configJsonObject, "chatFontFlags", json_object_new_int(config.chatFontFlags));
	json_object_object_add(configJsonObject, "uiFontName", json_object_new_string(config.uiFontName));
	json_object_object_add(configJsonObject, "uiFontSize", json_object_new_int(config.uiFontSize));
	json_object_object_add(configJsonObject, "uiFontStyle", json_object_new_int(config.uiFontStyle));
	json_object_object_add(configJsonObject, "uiFontFlags", json_object_new_int(config.uiFontFlags));
	json_object_object_add(configJsonObject, "openAiApiKey", json_object_new_string(config.openAiApiKey));
	STRPTR configJsonString = (STRPTR)json_object_to_json_string_ext(configJsonObject, JSON_C_TO_STRING_PRETTY);

	if (Write(file, configJsonString, strlen(configJsonString)) != strlen(configJsonString)) {
		printf("Failed to write the data to the config file\n");
		Close(file);
		json_object_put(configJsonObject);
		return RETURN_ERROR;
	}

	Close(file);
	json_object_put(configJsonObject);
	return RETURN_OK;
}

/**
 * Read the config from disk
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG readConfig() {
	BPTR file = Open("PROGDIR:config.json", MODE_OLDFILE);
	if (file == 0) {
		// No config exists. Create a new one from defaults
		writeConfig();
		return RETURN_OK;
	}

	#ifdef __AMIGAOS3__
	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
	#else
	int64 fileSize = GetFileSize(file);
	#endif
	STRPTR configJsonString = AllocVec(fileSize + 1, MEMF_CLEAR);
	if (Read(file, configJsonString, fileSize) != fileSize) {
		printf("Failed to read the config file\n");
		Close(file);
		FreeVec(configJsonString);
		return RETURN_ERROR;
	}

	Close(file);

	struct json_object *configJsonObject = json_tokener_parse(configJsonString);
	if (configJsonObject == NULL) {
		printf("Failed to parse the config file\n");
		FreeVec(configJsonString);
		return RETURN_ERROR;
	}

	struct json_object *speechEnabledObj;
	if (json_object_object_get_ex(configJsonObject, "speechEnabled", &speechEnabledObj)) {
		config.speechEnabled = json_object_get_boolean(speechEnabledObj);
	}

	struct json_object *speechSystemObj;
	if (json_object_object_get_ex(configJsonObject, "speechSystem", &speechSystemObj)) {
		config.speechSystem = json_object_get_int(speechSystemObj);
	}

	#ifdef __AMIGAOS3__
	if (config.speechSystem == SPEECH_SYSTEM_FLITE || config.speechSystem == SPEECH_SYSTEM_NONE) {
		config.speechSystem = SPEECH_SYSTEM_34;
	}
	#else
	if (config.speechSystem == SPEECH_SYSTEM_34 || config.speechSystem == SPEECH_SYSTEM_37 || config.speechSystem == SPEECH_SYSTEM_NONE) {
		config.speechSystem = SPEECH_SYSTEM_FLITE;
	}
	#endif

	#ifdef __AMIGAOS3__
	struct json_object *speechAccentObj;
	if (json_object_object_get_ex(configJsonObject, "speechAccent", &speechAccentObj)) {
		STRPTR speechAccent = json_object_get_string(speechAccentObj);
		if (speechAccent != NULL && speechAccent[0] != '\0') {
			memset(config.speechAccent, 0, sizeof(config.speechAccent));
			strncpy(config.speechAccent, speechAccent, sizeof(config.speechAccent) - 1);
		}
	}
	#else
	struct json_object *speechVoiceObj;
	if (json_object_object_get_ex(configJsonObject, "speechVoice", &speechVoiceObj)) {
		config.speechVoice = json_object_get_int(speechVoiceObj);
	}
	#endif
	
	struct json_object *chatModelObj;
	if (json_object_object_get_ex(configJsonObject, "chatModel", &chatModelObj) || json_object_object_get_ex(configJsonObject, "model", &chatModelObj)) {
		config.chatModel = json_object_get_int(chatModelObj);
	}

	struct json_object *imageModelObj;
	if (json_object_object_get_ex(configJsonObject, "imageModel", &imageModelObj)) {
		config.imageModel = json_object_get_int(imageModelObj);
	}

	struct json_object *chatFontNameObj;
	if (json_object_object_get_ex(configJsonObject, "chatFontName", &chatFontNameObj)) {
		STRPTR chatFontName = json_object_get_string(chatFontNameObj);
		if (chatFontName != NULL && chatFontName[0] != '\0') {
			memset(config.chatFontName, 0, sizeof(config.chatFontName));
			strncpy(config.chatFontName, chatFontName, sizeof(config.chatFontName) - 1);
		}
	}

	struct json_object *chatFontSizeObj;
	if (json_object_object_get_ex(configJsonObject, "chatFontSize", &chatFontSizeObj)) {
		config.chatFontSize = json_object_get_int(chatFontSizeObj);
	}

	struct json_object *chatFontStyleObj;
	if (json_object_object_get_ex(configJsonObject, "chatFontStyle", &chatFontStyleObj)) {
		config.chatFontStyle = json_object_get_int(chatFontStyleObj);
	}

	struct json_object *chatFontFlagsObj;
	if (json_object_object_get_ex(configJsonObject, "chatFontFlags", &chatFontFlagsObj)) {
		config.chatFontFlags = json_object_get_int(chatFontFlagsObj);
	}

	struct json_object *uiFontNameObj;
	if (json_object_object_get_ex(configJsonObject, "uiFontName", &uiFontNameObj)) {
		STRPTR uiFontName = json_object_get_string(uiFontNameObj);
		if (uiFontName != NULL && uiFontName[0] != '\0') {
			memset(config.uiFontName, 0, sizeof(config.uiFontName));
			strncpy(config.uiFontName, uiFontName, sizeof(config.uiFontName) - 1);
		}
	}

	struct json_object *uiFontSizeObj;
	if (json_object_object_get_ex(configJsonObject, "uiFontSize", &uiFontSizeObj)) {
		config.uiFontSize = json_object_get_int(uiFontSizeObj);
	}

	struct json_object *uiFontStyleObj;
	if (json_object_object_get_ex(configJsonObject, "uiFontStyle", &uiFontStyleObj)) {
		config.uiFontStyle = json_object_get_int(uiFontStyleObj);
	}

	struct json_object *uiFontFlagsObj;
	if (json_object_object_get_ex(configJsonObject, "uiFontFlags", &uiFontFlagsObj)) {
		config.uiFontFlags = json_object_get_int(uiFontFlagsObj);
	}

	struct json_object *openAiApiKeyObj;
	if (json_object_object_get_ex(configJsonObject, "openAiApiKey", &openAiApiKeyObj)) {
		STRPTR openAiApiKey = json_object_get_string(openAiApiKeyObj);
		if (openAiApiKey != NULL && openAiApiKey[0] != '\0') {
			memset(config.openAiApiKey, 0, sizeof(config.openAiApiKey));
			strncpy(config.openAiApiKey, openAiApiKey, strlen(openAiApiKey));
		}
	}

	FreeVec(configJsonString);
	json_object_put(configJsonObject);
	return RETURN_OK;
}