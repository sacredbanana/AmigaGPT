#include <proto/dos.h>

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 8192
#define TEMP_BUFFER_LENGTH 1024

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
	UBYTE content[READ_BUFFER_LENGTH];
};

/**
 * The model OpenAI should use
**/
enum Model {
    GPT_4,
    GPT_4_0314,
    GPT_4_32K,
    GPT_4_32K_0314,
    GPT_3_5_TURBO,
    GPT_3_5_TURBO_0301
};

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
 * @return a pointer to a new string containing the response -- Free it with FreeVec() when you are done using it
 * @todo Handle errors
**/
STRPTR postMessageToOpenAI(struct MinList *conversation, enum Model model, STRPTR openAiApiKey);

/**
 * Cleanup the OpenAI connector and free all resources
**/
void closeOpenAIConnector();