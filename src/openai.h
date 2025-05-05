#include <libraries/codesets.h>
#include <proto/dos.h>

#define READ_BUFFER_LENGTH 65536
#define TEMP_READ_BUFFER_LENGTH READ_BUFFER_LENGTH / 2
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
     * The role of the speaker. Currently the only roles supported by OpenAI are
     *"user", "assistant" and "system"
     **/
    UBYTE role[10];
    /**
     * The text of the message
     **/
    UTF8 *content;
};

struct Conversation {
    struct MinList *messages;
    UTF8 *name;
};

/**
 * The chat model OpenAI should use
 **/
enum ChatModel {
    CHATGPT_4o_LATEST = 0L,
    GPT_3_5_TURBO,
    GPT_3_5_TURBO_0125,
    GPT_3_5_TURBO_1106,
    GPT_4,
    GPT_4_0613,
    GPT_4_0314,
    GPT_4_1,
    GPT_4_1_2024_04_14,
    GPT_4_5_PREVIEW,
    GPT_4_5_PREVIEW_2025_02_27,
    GPT_4_TURBO,
    GPT_4_TURBO_2024_04_09,
    GPT_4_TURBO_PREVIEW,
    GPT_4o,
    GPT_4o_2024_11_20,
    GPT_4o_2024_08_06,
    GPT_4o_2024_05_13,
    GPT_4o_MINI,
    GPT_4o_MINI_2024_07_18,
    o1,
    o1_2024_12_17,
    o1_PREVIEW,
    o1_PREVIEW_2024_09_12,
    o1_MINI,
    o1_MINI_2024_09_12,
    o1_PRO,
    o1_PRO_2025_03_19,
    o3,
    o3_2025_04_16,
    o3_MINI,
    o3_MINI_2025_01_31,
    o4_MINI,
    o4_MINI_2025_04_16
};

/**
 * The names of the models
 * @see enum ChatModel
 **/
extern CONST_STRPTR CHAT_MODEL_NAMES[];

/**
 * The image model OpenAI should use
 **/
enum ImageModel { DALL_E_2 = 0L, DALL_E_3, GPT_IMAGE_1 };

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
    OPENAI_TTS_MODEL_TTS_1_HD,
    OPENAI_TTS_MODEL_GPT_4o_MINI_TTS

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
    OPENAI_TTS_VOICE_ASH,
    OPENAI_TTS_VOICE_BALLAD,
    OPENAI_TTS_VOICE_CORAL,
    OPENAI_TTS_VOICE_ECHO,
    OPENAI_TTS_VOICE_FABLE,
    OPENAI_TTS_VOICE_ONYX,
    OPENAI_TTS_VOICE_NOVA,
    OPENAI_TTS_VOICE_SAGE,
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
enum ImageSize {
    IMAGE_SIZE_NULL = -1L,
    IMAGE_SIZE_256x256 = 0L,
    IMAGE_SIZE_512x512,
    IMAGE_SIZE_1024x1024,
    IMAGE_SIZE_1792x1024,
    IMAGE_SIZE_1024x1792,
    IMAGE_SIZE_1536x1024,
    IMAGE_SIZE_1024x1536,
    IMAGE_SIZE_AUTO
};

/**
 * The names of the image sizes
 * @see enum ImageSize
 **/
extern CONST_STRPTR IMAGE_SIZE_NAMES[];

/**
 * The image sizes for DALL-E 2
 * @see enum ImageSize
 **/
extern const enum ImageSize IMAGE_SIZES_DALL_E_2[];

/**
 * The image sizes for DALL-E 3
 * @see enum ImageSize
 **/
extern const enum ImageSize IMAGE_SIZES_DALL_E_3[];

/**
 * The image sizes for GPT Image 1
 * @see enum ImageSize
 **/
extern const enum ImageSize IMAGE_SIZES_GPT_IMAGE_1[];

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
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return a pointer to a new array of json_object containing the response(s) or
 *NULL -- Free it with json_object_put() for all responses then FreeVec() for
 *the array when you are done using it
 **/
struct json_object **
postChatMessageToOpenAI(struct Conversation *conversation, enum ChatModel model,
                        CONST_STRPTR openAiApiKey, BOOL stream, BOOL useProxy,
                        CONST_STRPTR proxyHost, UWORD proxyPort,
                        BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
                        CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword);

/**
 * Post a image creation request to OpenAI
 * @param prompt the prompt to use
 * @param imageModel the image model to use
 * @param imageSize the size of the image to create
 * @param openAiApiKey the OpenAI API key
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return a pointer to a new json_object containing the response or NULL --
 *Free it with json_object_put when you are done using it
 **/
struct json_object *postImageCreationRequestToOpenAI(
    CONST_STRPTR prompt, enum ImageModel imageModel, enum ImageSize ImageSize,
    CONST_STRPTR openAiApiKey, BOOL useProxy, CONST_STRPTR proxyHost,
    UWORD proxyPort, BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
    CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword);

/**
 * Download a file from the internet
 * @param url the URL to download from
 * @param destination the destination to save the file to
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
ULONG downloadFile(CONST_STRPTR url, CONST_STRPTR destination, BOOL useProxy,
                   CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
                   BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
                   CONST_STRPTR proxyPassword);

/**
 * Post a text to speech request to OpenAI
 * @param text the text to speak
 * @param openAITTSModel the TTS model to use
 * @param openAITTSVoice the voice to use
 * @param voiceInstructions the voice instructions to use
 * @param openAiApiKey the OpenAI API key
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return a pointer to a buffer containing the audio data or NULL -- Free it
 *with FreeVec() when you are done using it
 **/
APTR postTextToSpeechRequestToOpenAI(
    CONST_STRPTR text, enum OpenAITTSModel openAITTSModel,
    enum OpenAITTSVoice openAITTSVoice, CONST_STRPTR voiceInstructions,
    CONST_STRPTR openAiApiKey, ULONG *audioLength, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword);

/**
 * Cleanup the OpenAI connector and free all resources
 **/
void closeOpenAIConnector();

/**
 * Decode a base64 encoded string
 * @param dataB64 the base64 encoded string
 * @param data_len the length of the decoded data
 * @return a pointer to the decoded data
 */
UBYTE *decodeBase64(UBYTE *dataB64, LONG *data_len);