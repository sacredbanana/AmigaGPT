#include <dos/dos.h>
#include <exec/exec.h>
#include <exec/memory.h>
#include <exec/types.h>
#include <graphics/text.h>
#include <json-c/json.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "config.h"

#define DEFAULT_ACCENT "american.accent"

struct Config config = {
	.speechEnabled = TRUE,
	#ifdef __AMIGAOS3__
	.speechAccent = NULL,
	.speechSystem = SPEECH_SYSTEM_34,
	#else
	.speechVoice = SPEECH_VOICE_KAL,
	.speechSystem = SPEECH_SYSTEM_FLITE,
	#endif
	.chatSystem = NULL,
	.chatModel = GPT_3_5_TURBO,
	.imageModel = DALL_E_3,
	.imageSizeDallE2 = IMAGE_SIZE_256x256,
	.imageSizeDallE3 = IMAGE_SIZE_1024x1024,
	.chatFontName = NULL,
	.chatFontSize = 8,
	.chatFontStyle = FS_NORMAL,
	.chatFontFlags = FPF_DISKFONT | FPF_DESIGNED,
	.uiFontName = NULL,
	.uiFontSize = 8,
	.uiFontStyle = FS_NORMAL,
	.uiFontFlags = FPF_DISKFONT | FPF_DESIGNED,
	.openAiApiKey = NULL,
	.colors = {
		10 << 16,
		0x00000000, 0x00000000, 0x11111111, // darkest blue
		0x00000000, 0x88888888, 0xFFFFFFFF, // light blue
		0xA0000000, 0x00000000, 0x0F0F0F0F, // dark red
		0x00000000, 0x00000000, 0x00000000, // black
		0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, // white
		0x00000000, 0xFFFFFFFF, 0x66666666, // pastel green
		0xFFFFFFFF, 0x00000000, 0x00000000, // red
		0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, // yellow
		0x22222222, 0x22222222, 0x22222222, // dark grey
		0x44444444, 0x44444444, 0x44444444, // grey
		0
	},
	.chatModelSetVersion = CHAT_MODEL_SET_VERSION,
	.imageModelSetVersion = IMAGE_MODEL_SET_VERSION
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
	json_object_object_add(configJsonObject, "speechAccent", config.speechAccent != NULL ? json_object_new_string(config.speechAccent) : NULL);
	#else
	json_object_object_add(configJsonObject, "speechVoice", json_object_new_int(config.speechVoice));
	#endif
	json_object_object_add(configJsonObject, "chatSystem", config.chatSystem != NULL ? json_object_new_string(config.chatSystem) : NULL);
	json_object_object_add(configJsonObject, "chatModel", json_object_new_int(config.chatModel));
	json_object_object_add(configJsonObject, "imageModel", json_object_new_int(config.imageModel));
	json_object_object_add(configJsonObject, "imageSizeDallE2", json_object_new_int(config.imageSizeDallE2));
	json_object_object_add(configJsonObject, "imageSizeDallE3", json_object_new_int(config.imageSizeDallE3));
	json_object_object_add(configJsonObject, "chatFontName", config.chatFontName != NULL ? json_object_new_string(config.chatFontName) : NULL);
	json_object_object_add(configJsonObject, "chatFontSize", json_object_new_int(config.chatFontSize));
	json_object_object_add(configJsonObject, "chatFontStyle", json_object_new_int(config.chatFontStyle));
	json_object_object_add(configJsonObject, "chatFontFlags", json_object_new_int(config.chatFontFlags));
	json_object_object_add(configJsonObject, "uiFontName", config.uiFontName != NULL ? json_object_new_string(config.uiFontName) : NULL);
	json_object_object_add(configJsonObject, "uiFontSize", json_object_new_int(config.uiFontSize));
	json_object_object_add(configJsonObject, "uiFontStyle", json_object_new_int(config.uiFontStyle));
	json_object_object_add(configJsonObject, "uiFontFlags", json_object_new_int(config.uiFontFlags));
	json_object_object_add(configJsonObject, "openAiApiKey", config.openAiApiKey != NULL ? json_object_new_string(config.openAiApiKey) : NULL);
	json_object_object_add(configJsonObject, "chatModelSetVersion", json_object_new_int(CHAT_MODEL_SET_VERSION));
	json_object_object_add(configJsonObject, "imageModelSetVersion", json_object_new_int(IMAGE_MODEL_SET_VERSION));
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
	if (config.speechAccent != NULL) {
		FreeVec(config.speechAccent);
		config.speechAccent = NULL;
	}
	struct json_object *speechAccentObj;
	if (json_object_object_get_ex(configJsonObject, "speechAccent", &speechAccentObj)) {
		CONST_STRPTR speechAccent = json_object_get_string(speechAccentObj);
		if (speechAccent != NULL) {
			config.speechAccent = AllocVec(strlen(speechAccent) + 1, MEMF_CLEAR);
			strncpy(config.speechAccent, speechAccent, strlen(speechAccent));
		}
	}
	if (config.speechAccent == NULL) {
		config.speechAccent = AllocVec(strlen(DEFAULT_ACCENT) + 1, MEMF_CLEAR);
		strncpy(config.speechAccent, DEFAULT_ACCENT, strlen(DEFAULT_ACCENT));
	}
	#else
	struct json_object *speechVoiceObj;
	if (json_object_object_get_ex(configJsonObject, "speechVoice", &speechVoiceObj)) {
		config.speechVoice = json_object_get_int(speechVoiceObj);
	}
	#endif

	if (config.chatSystem != NULL) {
		FreeVec(config.chatSystem);
		config.chatSystem = NULL;
	}
	struct json_object *chatSystemObj;
	if (json_object_object_get_ex(configJsonObject, "chatSystem", &chatSystemObj)) {
		CONST_STRPTR chatSystem = json_object_get_string(chatSystemObj);
		if (chatSystem != NULL) {
			config.chatSystem = AllocVec(strlen(chatSystem) + 1, MEMF_CLEAR);
			strncpy(config.chatSystem, chatSystem, strlen(chatSystem));
		}
	}
	
	struct json_object *chatModelObj;
	if (json_object_object_get_ex(configJsonObject, "chatModel", &chatModelObj) || json_object_object_get_ex(configJsonObject, "model", &chatModelObj)) {
		config.chatModel = json_object_get_int(chatModelObj);
	}

	struct json_object *imageModelObj;
	if (json_object_object_get_ex(configJsonObject, "imageModel", &imageModelObj)) {
		config.imageModel = json_object_get_int(imageModelObj);
	}

	struct json_object *imageSizeDallE2Obj;
	if (json_object_object_get_ex(configJsonObject, "imageSizeDallE2", &imageSizeDallE2Obj)) {
		config.imageSizeDallE2 = json_object_get_int(imageSizeDallE2Obj);
	}

	struct json_object *imageSizeDallE3Obj;
	if (json_object_object_get_ex(configJsonObject, "imageSizeDallE3", &imageSizeDallE3Obj)) {
		config.imageSizeDallE3 = json_object_get_int(imageSizeDallE3Obj);
	}

	if (config.chatFontName != NULL) {
		FreeVec(config.chatFontName);
		config.chatFontName = NULL;
	}
	struct json_object *chatFontNameObj;
	if (json_object_object_get_ex(configJsonObject, "chatFontName", &chatFontNameObj)) {
		CONST_STRPTR chatFontName = json_object_get_string(chatFontNameObj);
		if (chatFontName != NULL) {
			config.chatFontName = AllocVec(strlen(chatFontName) + 1, MEMF_CLEAR);
			strncpy(config.chatFontName, chatFontName, strlen(chatFontName));
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

	if (config.uiFontName != NULL) {
		FreeVec(config.uiFontName);
		config.uiFontName = NULL;
	}
	struct json_object *uiFontNameObj;
	if (json_object_object_get_ex(configJsonObject, "uiFontName", &uiFontNameObj)) {
		CONST_STRPTR uiFontName = json_object_get_string(uiFontNameObj);
		if (uiFontName != NULL) {
			config.uiFontName = AllocVec(strlen(uiFontName) + 1, MEMF_CLEAR);
			strncpy(config.uiFontName, uiFontName, strlen(uiFontName));
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

	if (config.openAiApiKey != NULL) {
		FreeVec(config.openAiApiKey);
		config.openAiApiKey = NULL;
	}
	struct json_object *openAiApiKeyObj;
	if (json_object_object_get_ex(configJsonObject, "openAiApiKey", &openAiApiKeyObj)) {
		CONST_STRPTR openAiApiKey = json_object_get_string(openAiApiKeyObj);
		if (openAiApiKey != NULL) {
			config.openAiApiKey = AllocVec(strlen(openAiApiKey) + 1, MEMF_CLEAR);
			strncpy(config.openAiApiKey, openAiApiKey, strlen(openAiApiKey));
		}
	}

	struct json_object *chatModelSetVersionObj;
	if (json_object_object_get_ex(configJsonObject, "chatModelSetVersion", &chatModelSetVersionObj)) {
		config.chatModelSetVersion = json_object_get_int(chatModelSetVersionObj);
	} else {
		config.chatModelSetVersion = 0;
	}

	if (config.chatModelSetVersion != CHAT_MODEL_SET_VERSION) {
		config.chatModel = GPT_4;
	}

	struct json_object *imageModelSetVersionObj;
	if (json_object_object_get_ex(configJsonObject, "imageModelSetVersion", &imageModelSetVersionObj)) {
		config.imageModelSetVersion = json_object_get_int(imageModelSetVersionObj);
	} else {
		config.imageModelSetVersion = 0;
	}

	if (config.imageModelSetVersion != IMAGE_MODEL_SET_VERSION) {
		config.imageModel = DALL_E_3;
	}

	FreeVec(configJsonString);
	json_object_put(configJsonObject);
	return RETURN_OK;
}

/**
 * Free the config
**/ 
void freeConfig() {
	#ifdef __AMIGAOS3__
	if (config.speechAccent != NULL) {
		FreeVec(config.speechAccent);
		config.speechAccent = NULL;
	}
	#endif
	if (config.chatSystem != NULL) {
		FreeVec(config.chatSystem);
		config.chatSystem = NULL;
	}
	if (config.chatFontName != NULL) {
		FreeVec(config.chatFontName);
		config.chatFontName = NULL;
	}
	if (config.uiFontName != NULL) {
		FreeVec(config.uiFontName);
		config.uiFontName = NULL;
	}
	if (config.openAiApiKey != NULL) {
		FreeVec(config.openAiApiKey);
		config.openAiApiKey = NULL;
	}
}