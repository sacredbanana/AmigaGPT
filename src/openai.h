#include <proto/dos.h>

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 65536
#define DOWNLOAD_BUFFER_LENGTH 4194304
#define RESPONSE_ARRAY_BUFFER_LENGTH 1024

/**
 * A node in the conversation
**/
struct ConversationNode {
	/**
	 * The linking node
	**/
	struct MinNode node;
	/**
	 * The role of the speaker. Currently the only roles supported by OpenAI are "user" and "assistant"
	**/
	UBYTE role[64];
	/**
	 * The text of the message
	**/
	STRPTR content;
};

/**
 * The model OpenAI should use
**/
enum Model {
	GPT_4 = 0,
	GPT_4_0314,
	GPT_4_0613,
	GPT_4_1106_PREVIEW,
	GPT_4_32K,
	GPT_4_32K_0314,
	GPT_4_32K_0613,
	GPT_3_5_TURBO,
	GPT_3_5_TURBO_0301,
	GPT_3_5_TURBO_0613,
	GPT_3_5_TURBO_1106,
	GPT_3_5_TURBO_16K,
	GPT_3_5_TURBO_16K_0613
};

/**
 * The names of the models
 * @see enum Model
**/ 
extern CONST_STRPTR CHAT_MODEL_NAMES[];

/**
 * The image model OpenAI should use
**/
enum ImageModel {
	DALL_E_2 = 0,
	DALL_E_3
};

/**
 * The names of the image models
 * @see enum ImageModel
**/ 
extern CONST_STRPTR IMAGE_MODEL_NAMES[];

/**
 * The size of the requested image
**/
extern enum ImageSize {
	IMAGE_SIZE_256x256 = 0,
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
 * @return a pointer to a new array of json_object containing the response(s) -- Free it with json_object_put() for all responses then FreeVec() for the array when you are done using it
**/
struct json_object** postMessageToOpenAI(struct MinList *conversation, enum Model model, CONST_STRPTR openAiApiKey, BOOL stream);

/**
 * Post a image creation request to OpenAI
 * @param prompt the prompt to use
 * @param imageModel the image model to use
 * @param imageSize the size of the image to create
 * @param openAiApiKey the OpenAI API key
 * @return a pointer to a new json_object containing the response -- Free it with json_object_put when you are done using it
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
 * Cleanup the OpenAI connector and free all resources
**/
void closeOpenAIConnector();