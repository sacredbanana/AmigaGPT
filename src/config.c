#include <exec/types.h>
#include <graphics/text.h>
#include <exec/memory.h>
#include <stdlib.h>
#include "config.h"
#include "external/json-c/json.h"

extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;

struct Config config = {
	.speechEnabled = TRUE,
	.speechAccent = "!USA.accent",
	.speechSystem = SPEECH_SYSTEM_34,
	.model = GPT_3_5_TURBO,
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
		5l<<16 | 0,
		0x00000000, 0x11111111, 0x55555555,
		0xAAAAAAAA, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x33333333,
		0xBBBBBBBB, 0xFFFFFFFF, 0x22222222,
		0x00000000, 0x00000000, 0x00000000,
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
	json_object_object_add(configJsonObject, "speechAccent", json_object_new_string(config.speechAccent));
	json_object_object_add(configJsonObject, "speechSystem", json_object_new_int(config.speechSystem));
	json_object_object_add(configJsonObject, "model", json_object_new_int(config.model));
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

	Seek(file, 0, OFFSET_END);
	LONG fileSize = Seek(file, 0, OFFSET_BEGINNING);
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

	struct json_object *speechAccentObj;
	if (json_object_object_get_ex(configJsonObject, "speechAccent", &speechAccentObj)) {
		STRPTR speechAccent = json_object_get_string(speechAccentObj);
		if (speechAccent != NULL && speechAccent[0] != '\0') {
			memset(config.speechAccent, 0, sizeof(config.speechAccent));
			strncpy(config.speechAccent, speechAccent, sizeof(config.speechAccent) - 1);
		}
	}

	struct json_object *speechSystemObj;
	if (json_object_object_get_ex(configJsonObject, "speechSystem", &speechSystemObj)) {
		config.speechSystem = json_object_get_int(speechSystemObj);
	}

	struct json_object *modelObj;
	if (json_object_object_get_ex(configJsonObject, "model", &modelObj)) {
		config.model = json_object_get_int(modelObj);
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