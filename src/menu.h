#include <proto/dos.h>

enum MenuItemID {
    MENU_ITEM_NULL = 0L,
	MENU_ITEM_PROJECT_ABOUT_AMIGAGPT,
	MENU_ITEM_PROJECT_ABOUT_MUI,
	MENU_ITEM_PROJECT_QUIT,
	MENU_ITEM_EDIT_CUT,
	MENU_ITEM_EDIT_COPY,
	MENU_ITEM_EDIT_PASTE,
	MENU_ITEM_EDIT_CLEAR,
	MENU_ITEM_EDIT_SELECT_ALL,
	MENU_ITEM_VIEW_MUI_SETTINGS,
	MENU_ITEM_SPEECH_ENABLED,
	MENU_ITEM_SPEECH_SYSTEM_34,
	MENU_ITEM_SPEECH_SYSTEM_37,
	MENU_ITEM_SPEECH_SYSTEM_FLITE,
	MENU_ITEM_SPEECH_SYSTEM_OPENAI,
	MENU_ITEM_SPEECH_ACCENT,
	MENU_ITEM_SPEECH_FLITE_VOICE_AWB,
	MENU_ITEM_SPEECH_FLITE_VOICE_KAL,
	MENU_ITEM_SPEECH_FLITE_VOICE_KAL16,
	MENU_ITEM_SPEECH_FLITE_VOICE_RMS,
	MENU_ITEM_SPEECH_FLITE_VOICE_SLT,
	MENU_ITEM_SPEECH_OPENAI_VOICE_ALLOY,
	MENU_ITEM_SPEECH_OPENAI_VOICE_ECHO,
	MENU_ITEM_SPEECH_OPENAI_VOICE_FABLE,
	MENU_ITEM_SPEECH_OPENAI_VOICE_ONYX,
	MENU_ITEM_SPEECH_OPENAI_VOICE_NOVA,
	MENU_ITEM_SPEECH_OPENAI_VOICE_SHIMMER,
	MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1,
	MENU_ITEM_SPEECH_OPENAI_MODEL_TTS_1_HD,
	MENU_ITEM_OPENAI_API_KEY,
	MENU_ITEM_OPENAI_CHAT_SYSTEM,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_2024_05_13,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4o_MINI_2024_07_18,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_2024_04_09,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_TURBO_PREVIEW,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0125_PREVIEW,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_1106_PREVIEW,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_4_0613,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_0125,
	MENU_ITEM_OPENAI_CHAT_MODEL_GPT_3_5_TURBO_1106,
	MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_2,
	MENU_ITEM_OPENAI_IMAGE_MODEL_DALL_E_3,
	MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_256X256,
	MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_512X512,
	MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_2_1024X1024,
	MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1024,
	MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1792X1024,
	MENU_ITEM_OPENAI_IMAGE_SIZE_DALL_E_3_1024X1792,
	MENU_ITEM_HELP_OPEN_DOCUMENTATION
};

extern Object *menuStrip;

void createMenu();

void addMenuActions();