#include <proto/dos.h>

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 65536
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
extern CONST_STRPTR MODEL_NAMES[];

/**
 * Initialize the OpenAI connector
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initOpenAIConnector();

/**
 * Post a message to OpenAI
 * @param conversation the conversation to post
 * @param model the model to use
 * @param openAiApiKey the OpenAI API key
 * @param stream whether to stream the response or not
 * @return a pointer to a new array of json_object containing the response(s) -- Free it with json_object_put() for all responses then FreeVec() for the array when you are done using it
**/
struct json_object** postMessageToOpenAI(struct MinList *conversation, enum Model model, STRPTR openAiApiKey, BOOL stream);

/**
 * Cleanup the OpenAI connector and free all resources
**/
void closeOpenAIConnector();