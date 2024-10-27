#include <proto/dos.h>

#define READ_BUFFER_LENGTH 65536
#define WRITE_BUFFER_LENGTH 131072
#define RESPONSE_ARRAY_BUFFER_LENGTH 1024
#define CHAT_SYSTEM_LENGTH 512
#define OPENAI_API_KEY_LENGTH 256

/**
 * A node in the conversation
**/
struct ConversationNode {
	/**
	 * The linking node
	**/
	struct MinNode node;
	/**
	 * The role of the speaker. Currently the only roles supported by OpenAI are "user", "assistant" and "system"
	**/
	UBYTE role[10];
	/**
	 * The text of the message
	**/
	STRPTR content;
};

struct Conversation {
	struct MinList *messages;
	STRPTR name;
};

/**
 * The chat model OpenAI should use
**/
enum ChatModel {
	GPT_4o = 0L,
	GPT_4o_2024_05_13,
	GPT_4o_MINI,
	GPT_4o_MINI_2024_07_18,
	GPT_4_TURBO,
	GPT_4_TURBO_2024_04_09,
	GPT_4_TURBO_PREVIEW,
	GPT_4_0125_PREVIEW,
	GPT_4_1106_PREVIEW,
	GPT_4,
	GPT_4_0613,
	GPT_3_5_TURBO,
	GPT_3_5_TURBO_0125,
	GPT_3_5_TURBO_1106,
};

/**
 * The names of the models
 * @see enum ChatModel
**/ 
extern CONST_STRPTR CHAT_MODEL_NAMES[];

/**
 * The image model OpenAI should use
**/
enum ImageModel {
	DALL_E_2 = 0L,
	DALL_E_3
};

/**
 * The names of the image models
 * @see enum ImageModel
**/ 
extern CONST_STRPTR IMAGE_MODEL_NAMES[];

/**
 * The Text to Speech model OpenAI should use
**/
enum OpenAITTSModel {
	OPENAI_TTS_MODEL_TTS_1 = 0L,
	OPENAI_TTS_MODEL_TTS_1_HD
};

/**
 * The names of the Text to Speech models
 * @see enum OpenAITTSModel
**/ 
extern CONST_STRPTR OPENAI_TTS_MODEL_NAMES[];

/**
 * The voice OpenAI should use
**/
enum OpenAITTSVoice {
	OPENAI_TTS_VOICE_ALLOY = 0L,
	OPENAI_TTS_VOICE_ECHO,
	OPENAI_TTS_VOICE_FABLE,
	OPENAI_TTS_VOICE_ONYX,
	OPENAI_TTS_VOICE_NOVA,
	OPENAI_TTS_VOICE_SHIMMER
};

/**
 * The names of the voices
 * @see enum OpenAITTSModel
**/ 
extern CONST_STRPTR OPENAI_TTS_VOICE_NAMES[];

/**
 * The size of the requested image
**/
extern enum ImageSize {
	IMAGE_SIZE_256x256 = 0L,
	IMAGE_SIZE_512x512,
	IMAGE_SIZE_1024x1024,
	IMAGE_SIZE_1792x1024,
	IMAGE_SIZE_1024x1792
};

/**
 * The names of the image sizes
 * @see enum ImageSize
**/
extern CONST_STRPTR IMAGE_SIZE_NAMES[];

/**
 * Struct representing a generated image
 * @see enum ImageModel
 **/ 
struct GeneratedImage {
	STRPTR name;
	STRPTR filePath;
	STRPTR prompt;
	enum ImageModel imageModel;
	ULONG width;
	ULONG height;
};

/**
 * Initialize the OpenAI connector
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initOpenAIConnector();

/**
 * Post a chat message to OpenAI
 * @param conversation the conversation to post
 * @param model the model to use
 * @param openAiApiKey the OpenAI API key
 * @param stream whether to stream the response or not
 * @return a pointer to a new array of json_object containing the response(s) or NULL -- Free it with json_object_put() for all responses then FreeVec() for the array when you are done using it
**/
struct json_object** postChatMessageToOpenAI(struct Conversation *conversation, enum ChatModel model, CONST_STRPTR openAiApiKey, BOOL stream);

/**
 * Post a image creation request to OpenAI
 * @param prompt the prompt to use
 * @param imageModel the image model to use
 * @param imageSize the size of the image to create
 * @param openAiApiKey the OpenAI API key
 * @return a pointer to a new json_object containing the response or NULL -- Free it with json_object_put when you are done using it
**/
struct json_object* postImageCreationRequestToOpenAI(CONST_STRPTR prompt, enum ImageModel imageModel, enum ImageSize ImageSize, CONST_STRPTR openAiApiKey);

/**
 * Download a file from the internet
 * @param url the URL to download from
 * @param destination the destination to save the file to
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/ 
ULONG downloadFile(CONST_STRPTR url, CONST_STRPTR destination);

/**
 * Post a text to speech request to OpenAI
 * @param text the text to speak
 * @param openAITTSModel the TTS model to use
 * @param openAITTSVoice the voice to use
 * @param openAiApiKey the OpenAI API key
 * @return a pointer to a buffer containing the audio data or NULL -- Free it with FreeVec() when you are done using it
 **/
APTR postTextToSpeechRequestToOpenAI(CONST_STRPTR text, enum OpenAITTSModel openAITTSModel, enum OpenAITTSVoice openAITTSVoice, CONST_STRPTR openAiApiKey, ULONG *audioLength);

/**
 * Cleanup the OpenAI connector and free all resources
**/
void closeOpenAIConnector();