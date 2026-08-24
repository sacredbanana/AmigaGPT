#ifndef OPENAI_H
#define OPENAI_H

#include <libraries/codesets.h>
#include <proto/dos.h>
#include "speech.h"

struct json_object;

#define READ_BUFFER_LENGTH 65536
#define TEMP_READ_BUFFER_LENGTH READ_BUFFER_LENGTH / 2
#define WRITE_BUFFER_LENGTH 131072
#define RESPONSE_ARRAY_BUFFER_LENGTH 1024
#define CHAT_SYSTEM_LENGTH 512
#define OPENAI_API_KEY_LENGTH 256
#define MAX_PROVIDER_MODELS 32
#define MAX_ATTACHMENT_FILE_BYTES (8UL * 1024UL * 1024UL)
#define MAX_ATTACHMENT_TOTAL_BYTES (16UL * 1024UL * 1024UL)

/**
 * A file associated with a chat message. Local files have a path and are
 * encoded into the next API request. Files returned by a provider have a
 * file ID, container ID, or download URL and can be saved by the user.
 **/
struct ChatFile {
    struct MinNode node;
    STRPTR path;
    UTF8 *name;
    UTF8 *mimeType;
    UTF8 *fileId;
    UTF8 *containerId;
    UTF8 *downloadUrl;
};

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
    /**
     * Local attachments sent with this message, or files returned with it.
     **/
    struct MinList files;
};

struct Conversation {
    /**
     * The messages in the conversation
     **/
    struct MinList *messages;
    /**
     * The name of the conversation
     **/
    UTF8 *name;
    /**
     * The system of the conversation
     **/
    UTF8 *system;
    /**
     * Last response ID returned by the Responses API for stateful follow-ups
     **/
    UTF8 *lastResponseId;
};

/**
 * Sets conversation->lastResponseId from a Responses API JSON payload (e.g.
 * after submitting function_call_output). Keeps stateful follow-ups aligned
 * with the latest response in the chain.
 **/
void conversationSyncLastResponseIdFromPayload(struct Conversation *conversation,
                                               struct json_object *payload);

/**
 * Prepopulated chat models for each built-in provider
 **/
extern CONST_STRPTR OPENAI_CHAT_MODELS[];
extern CONST_STRPTR GEMINI_CHAT_MODELS[];
extern CONST_STRPTR GROK_CHAT_MODELS[];
extern CONST_STRPTR ANTHROPIC_CHAT_MODELS[];

/**
 * Prepopulated image models for each built-in provider
 **/
extern CONST_STRPTR OPENAI_IMAGE_MODELS[];
extern CONST_STRPTR GEMINI_IMAGE_MODELS[];
extern CONST_STRPTR GROK_IMAGE_MODELS[];

/**
 * The chat model OpenAI should use. Commented out models are not supported by
 * the responses endpoint. Do not exceed 32 models or the menu will not display
 * correctly.
 * @deprecated Use string-based model names instead
 **/
typedef enum {
    CHATGPT_5_LATEST = 0L,
    GPT_3_5_TURBO,
    GPT_4,
    GPT_4_1,
    GPT_4_1_MINI,
    GPT_4_1_NANO,
    GPT_4_5_PREVIEW,
    GPT_4_TURBO,
    GPT_4_TURBO_PREVIEW,
    GPT_4o,
    GPT_4o_MINI,
    GPT_5,
    GPT_5_MINI,
    GPT_5_NANO,
    GPT_5_1,
    GPT_5_2,
    GPT_5_2_PRO,
    o1,
    o1_PREVIEW,
    o1_MINI,
    o1_PRO,
    o3,
    o3_MINI,
    o4_MINI,
} ChatModel;

/**
 * The names of the chat models before the migration to string-based model
 * names. Do not modify this array!
 * @see ChatModel
 * @deprecated Use provider-specific model arrays instead
 **/
extern CONST_STRPTR CHAT_MODEL_NAMES[];

/**
 * The image model OpenAI should use
 * @deprecated Use string-based model names instead
 **/
typedef enum {
    DALL_E_2 = 0L,
    DALL_E_3,
    GPT_IMAGE_1,
    GPT_IMAGE_1_MINI,
    GPT_IMAGE_1_5
} ImageModel;

/**
 * The names of the image models (legacy, for backward compatibility)
 * @see ImageModel
 * @deprecated Use provider-specific model arrays instead
 **/
extern CONST_STRPTR IMAGE_MODEL_NAMES[];

/**
 * The Text to Speech model OpenAI should use
 **/
typedef enum {
    OPENAI_TTS_MODEL_TTS_1 = 0L,
    OPENAI_TTS_MODEL_TTS_1_HD,
    OPENAI_TTS_MODEL_GPT_4o_MINI_TTS
} OpenAITTSModel;

/**
 * The names of the Text to Speech models
 * @see OpenAITTSModel
 **/
extern CONST_STRPTR OPENAI_TTS_MODEL_NAMES[];

/**
 * The voice OpenAI should use
 **/
typedef enum {
    OPENAI_TTS_VOICE_ALLOY = 0L,
    OPENAI_TTS_VOICE_ASH,
    OPENAI_TTS_VOICE_BALLAD,
    OPENAI_TTS_VOICE_CORAL,
    OPENAI_TTS_VOICE_ECHO,
    OPENAI_TTS_VOICE_FABLE,
    OPENAI_TTS_VOICE_ONYX,
    OPENAI_TTS_VOICE_NOVA,
    OPENAI_TTS_VOICE_SAGE,
    OPENAI_TTS_VOICE_SHIMMER,
    OPENAI_TTS_VOICE_VERSE
} OpenAITTSVoice;

/**
 * The names of the voices
 * @see OpenAITTSModel
 **/
extern CONST_STRPTR OPENAI_TTS_VOICE_NAMES[];

/**
 * The voice xAI should use
 **/
typedef enum {
    XAI_TTS_VOICE_ARA = 0L,
    XAI_TTS_VOICE_EVE,
    XAI_TTS_VOICE_LEO,
    XAI_TTS_VOICE_REX,
    XAI_TTS_VOICE_SAL
} XAITTSVoice;

/**
 * The names of the xAI voices (id strings sent to the API)
 * @see XAITTSVoice
 **/
extern CONST_STRPTR XAI_TTS_VOICE_NAMES[];

/**
 * The size of the requested image
 **/
typedef enum {
    IMAGE_SIZE_NULL = -1L,
    IMAGE_SIZE_256x256 = 0L,
    IMAGE_SIZE_512x512,
    IMAGE_SIZE_1024x1024,
    IMAGE_SIZE_1792x1024,
    IMAGE_SIZE_1024x1792,
    IMAGE_SIZE_1536x1024,
    IMAGE_SIZE_1024x1536,
    IMAGE_SIZE_AUTO
} ImageSize;

/**
 * The names of the image sizes
 * @see ImageSize
 **/
extern CONST_STRPTR IMAGE_SIZE_NAMES[];

/**
 * The image sizes for DALL-E 2
 * @see ImageSize
 **/
extern const ImageSize IMAGE_SIZES_DALL_E_2[];

/**
 * The image sizes for DALL-E 3
 * @see ImageSize
 **/
extern const ImageSize IMAGE_SIZES_DALL_E_3[];

/**
 * The image sizes for GPT Image 1
 * @see ImageSize
 **/
extern const ImageSize IMAGE_SIZES_GPT_IMAGE_1[];

/**
 * Struct representing a generated image
 * @see ImageModel
 **/
struct GeneratedImage {
    UTF8 *name;
    STRPTR filePath;
    UTF8 *prompt;
    ImageModel imageModel;
    ULONG width;
    ULONG height;
};

/**
 * Chat API endpoint to use
 **/
typedef enum {
    API_CHAT_ENDPOINT_RESPONSES = 0L,
    API_CHAT_ENDPOINT_CHAT_COMPLETIONS,
    API_CHAT_ENDPOINT_MESSAGES,
    /* Gemini native text endpoint (generateContent / streamGenerateContent) */
    API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT
} APIChatEndpoint;

extern CONST_STRPTR API_CHAT_ENDPOINT_NAMES[];

/**
 * Image API endpoint to use
 **/
typedef enum {
    /* OpenAI-compatible image generation endpoint */
    API_IMAGE_ENDPOINT_IMAGES_GENERATIONS = 0L,
    /* Gemini native image endpoint (generateContent with responseModalities) */
    API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT
} APIImageEndpoint;

extern CONST_STRPTR API_IMAGE_ENDPOINT_NAMES[];

/**
 * The authorization type to use
 * @see AuthorizationType
 **/
typedef enum {
    AUTHORIZATION_TYPE_NONE = 0,
    AUTHORIZATION_TYPE_BEARER,
    AUTHORIZATION_TYPE_X_API_KEY,
    AUTHORIZATION_TYPE_X_GOOGLE_API_KEY,
    AUTHORIZATION_TYPE_XI_API_KEY
} AuthorizationType;

/**
 * The names of the authorization types
 * @see AuthorizationType
 **/
extern CONST_STRPTR AUTHORIZATION_TYPE_NAMES[];

/**
 * The format of the image
 * @see ImageFormat
 **/
typedef enum {
    IMAGE_FORMAT_NULL = -1L,
    IMAGE_FORMAT_JPG = 0L,
    IMAGE_FORMAT_PNG
} ImageFormat;

/**
 * The names of the image formats
 * @see ImageFormat
 **/
extern CONST_STRPTR IMAGE_FORMAT_NAMES[];

/**
 * Initialize the OpenAI connector
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG initOpenAIConnector();

/**
 * Get the chat models from the OpenAI API
 * @param host the host to use
 * @param port the port to use
 * @param useSSL whether to use SSL or not
 * @param apiKey the API key (can be NULL for local LLM)
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @param apiEndpointUrl the API endpoint URL to use
 * @param authorizationType the authorization type to use
 * @param customHeaders custom HTTP headers to add to the request
 * @return a pointer to a new json_object array containing the model names or
 * NULL -- Free it with json_object_put() when you are done using it
 **/
struct json_object *
getChatModels(STRPTR host, ULONG port, BOOL useSSL, CONST_STRPTR apiKey,
              BOOL useProxy, CONST_STRPTR proxyHost, ULONG proxyPort,
              BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
              CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword,
              CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
              CONST_STRPTR customHeaders);

/**
 * Post a chat message to OpenAI
 * @param conversation the conversation to post
 * @param host the host to use set to NULL to use OpenAI default host
 * @param port the port to use set to 0 to use OpenAI default port
 * @param useSSL whether to use SSL or not
 * @param model the model to use
 * @param apiKey the API key
 * @param stream whether to stream the response or not
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @param webSearchEnabled whether to enable web search or not
 * @param shellToolEnabled whether to enable the AmigaDOS shell tool
 * @param codeInterpreterEnabled whether to enable the hosted code interpreter
 * @param apiEndpoint the API endpoint to use
 * @param apiEndpointUrl the API endpoint URL to use
 * @param authorizationType the authorization type to use
 * @param customHeaders custom HTTP headers to add to the request
 * @return a pointer to a new array of json_object containing the response(s) or
 *NULL -- Free it with json_object_put() for all responses then FreeVec() for
 *the array when you are done using it
 **/
struct json_object **postChatMessageToOpenAI(
    struct Conversation *conversation, STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR model, CONST_STRPTR apiKey, BOOL stream, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, BOOL webSearchEnabled, BOOL shellToolEnabled,
    BOOL codeInterpreterEnabled, APIChatEndpoint apiEndpoint,
    CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
    CONST_STRPTR customHeaders);

/**
 * Post a image creation request to OpenAI
 * @param prompt the prompt to use
 * @param imageModel the image model to use
 * @param imageSize the size of the image to create
 * @param apiKey the API key
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @param imageFormat the image format to use
 * @param apiEndpoint the API endpoint to use
 * @return a pointer to a new json_object containing the response or NULL --
 *Free it with json_object_put when you are done using it
 **/
struct json_object *postImageCreationRequestToOpenAI(
    CONST_STRPTR prompt, CONST_STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
    CONST_STRPTR customHeaders, CONST_STRPTR modelName, ImageSize imageSize,
    CONST_STRPTR apiKey, BOOL useProxy, CONST_STRPTR proxyHost, UWORD proxyPort,
    BOOL proxyUsesSSL, BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, ImageFormat imageFormat,
    APIImageEndpoint apiEndpoint);

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
 * Download an authenticated binary file from the active provider.
 **/
ULONG downloadProviderFile(
    CONST_STRPTR host, UWORD port, BOOL useSSL, CONST_STRPTR endpoint,
    CONST_STRPTR destination, CONST_STRPTR apiKey, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, AuthorizationType authorizationType,
    CONST_STRPTR customHeaders);

/**
 * Find downloadable files in a provider response and append them to files.
 * Duplicate IDs and URLs are ignored.
 **/
ULONG collectResponseFiles(struct json_object *response,
                           struct MinList *files);

/**
 * Strip directory components from a response file name so it is safe to
 * write into a destination drawer.
 **/
CONST_STRPTR chatFileSafeName(CONST_STRPTR name);

/**
 * Build a path in drawer that does not yet exist. Adds -2, -3, ... before
 * the extension when the name is already taken. Caller must FreeVec().
 **/
STRPTR uniqueChatFileDestination(CONST_STRPTR drawer, CONST_STRPTR filename);

/**
 * Save one received chat file to destination. Tries a local copy first,
 * then downloadUrl, then the provider Files API. On success, file->path
 * is updated to destination.
 **/
ULONG saveChatFileToPath(
    struct ChatFile *file, CONST_STRPTR destination, CONST_STRPTR host,
    UWORD port, BOOL useSSL, CONST_STRPTR apiKey, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, CONST_STRPTR apiEndpointUrl,
    AuthorizationType authorizationType, CONST_STRPTR customHeaders);

/**
 * Post a text to speech request to OpenAI
 * @param text the text to speak
 * @param openAITTSModelId the TTS model id (e.g., "tts-1", "gpt-4o-mini-tts")
 * @param openAITTSVoice the voice to use
 * @param voiceInstructions the voice instructions to use
 * @param apiKey the API key
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @param audioFormat the audio format to use
 * @return a pointer to a buffer containing the audio data or NULL -- Free it
 *with FreeVec() when you are done using it
 **/
APTR postTextToSpeechRequestToOpenAI(
    CONST_STRPTR text, CONST_STRPTR openAITTSModelId,
    OpenAITTSVoice openAITTSVoice, CONST_STRPTR voiceInstructions,
    CONST_STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
    CONST_STRPTR apiKey, ULONG *audioLength, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, AudioFormat *audioFormat);

/**
 * Cleanup the OpenAI connector and free all resources
 **/
void closeOpenAIConnector();

/**
 * Make a generic HTTPS GET request and return the JSON response
 * @param host the host to connect to
 * @param port the port to use
 * @param endpoint the API endpoint (e.g., "/v1/models")
 * @param apiKey the API key to use
 * @param apiKeyHeader the header name for the API key (e.g., "xi-api-key" or
 * "Authorization")
 * @param useBearer whether to use "Bearer " prefix for the API key
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return a pointer to a new json_object or NULL -- Free it with
 * json_object_put() when you are done using it
 **/
struct json_object *
makeHttpsGetRequest(CONST_STRPTR host, UWORD port, BOOL useSSL,
                    CONST_STRPTR endpoint, CONST_STRPTR apiKey,
                    CONST_STRPTR apiKeyHeader,
                    BOOL useBearer, BOOL useProxy, CONST_STRPTR proxyHost,
                    UWORD proxyPort, BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
                    CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword);

/**
 * function_call_output strings sent back to the API (not shown to the user)
 **/
#define TOOL_CALL_OUTPUT_DENIED "The user denied this command."
#define TOOL_CALL_OUTPUT_UNAVAILABLE "This tool call could not be executed."

/**
 * Check if there is a pending tool call captured during streaming
 * @return TRUE if there is a pending tool call
 **/
BOOL hasPendingToolCall(void);

/**
 * Number of unanswered function_call items captured from the last response
 **/
UWORD getPendingToolCallCount(void);

/**
 * Get one captured function_call. Pointers stay valid until
 * clearPendingToolCall() or the next capture.
 * @return TRUE if index is valid
 **/
BOOL getPendingToolCallAt(UWORD index, STRPTR *callIdOut, STRPTR *nameOut,
                          STRPTR *commandOut);

/**
 * Get the pending tool call command
 * @return the command string (do not free)
 **/
STRPTR getPendingToolCommand(void);

/**
 * Get the pending tool call ID
 * @return the call ID string (do not free)
 **/
STRPTR getPendingToolCallId(void);

/**
 * Get the pending response ID
 * @return the response ID string (do not free)
 **/
STRPTR getPendingResponseId(void);

/**
 * Clear the pending tool call after processing
 **/
void clearPendingToolCall(void);

/**
 * Check if a response contains a shell tool function call
 * @param response the JSON response from the API
 * @return TRUE if the response contains a shell function call
 **/
BOOL hasShellToolCall(struct json_object *response);

/**
 * Get the call ID from a shell tool function call response
 * @param response the JSON response from the API
 * @return the call ID string (do not free) or NULL
 **/
UTF8 *getShellToolCallId(struct json_object *response);

/**
 * Get the command from a shell tool function call response
 * @param response the JSON response from the API
 * @return the command string (must be freed with FreeVec) or NULL
 **/
UTF8 *getShellToolCommand(struct json_object *response);

/**
 * Execute a shell command and capture its output
 * @param command the command to execute
 * @param exitCode pointer to store the exit code
 * @return a pointer to a new string containing the command output -- Free
 * it with FreeVec() when done
 **/
STRPTR executeShellCommand(UTF8 *command, LONG *exitCode);

/**
 * Post a tool result (shell command output) back to the API
 * This continues the conversation after a tool call
 * @param previousResponseId the ID from the previous response
 * @param callId the call_id from the function call
 * @param output the output from the shell command
 * @param model the model to use
 * @param host the host to use
 * @param port the port to use
 * @param useSSL whether to use SSL
 * @param apiKey the API key
 * @param useProxy whether to use a proxy
 * @param proxyHost the proxy host
 * @param proxyPort the proxy port
 * @param proxyUsesSSL whether the proxy uses SSL
 * @param proxyRequiresAuth whether the proxy requires auth
 * @param proxyUsername the proxy username
 * @param proxyPassword the proxy password
 * @param shellToolEnabled whether the shell tool is enabled
 * @param codeInterpreterEnabled whether the hosted code interpreter is enabled
 * @param apiEndpointUrl the API endpoint base URL (e.g. "v1")
 * @param authorizationType the authorization type to use
 * @param customHeaders custom HTTP headers to add to the request
 * @return a pointer to a new json_object containing the response or NULL
 **/
struct json_object *postToolResultToOpenAI(
    CONST_STRPTR previousResponseId, CONST_STRPTR callId, CONST_STRPTR output,
    CONST_STRPTR model, STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR apiKey, BOOL useProxy, CONST_STRPTR proxyHost, UWORD proxyPort,
    BOOL proxyUsesSSL, BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, BOOL shellToolEnabled,
    BOOL codeInterpreterEnabled, CONST_STRPTR apiEndpointUrl,
    AuthorizationType authorizationType, CONST_STRPTR customHeaders);

/**
 * Post every function_call_output for one previous response in a single
 * request. The Responses API requires all call_ids from that response.
 **/
struct json_object *postToolResultsToOpenAI(
    CONST_STRPTR previousResponseId, CONST_STRPTR *callIds,
    CONST_STRPTR *outputs, UWORD outputCount, CONST_STRPTR model, STRPTR host,
    UWORD port, BOOL useSSL, CONST_STRPTR apiKey, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, BOOL shellToolEnabled,
    BOOL codeInterpreterEnabled, CONST_STRPTR apiEndpointUrl,
    AuthorizationType authorizationType, CONST_STRPTR customHeaders);

/**
 * Post a text to speech request to the ElevenLabs API
 * @param text the text to be spoken
 * @param voiceId the ElevenLabs voice ID
 * @param modelId the ElevenLabs model ID
 * @param apiKey the ElevenLabs API key
 * @param audioLength a pointer to a variable that will be set to the length of
 * the audio data
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return a pointer to a buffer containing the PCM audio data or NULL -- Free
 * it with FreeVec() when you are done using it
 **/
APTR postTextToSpeechRequestToElevenLabs(
    CONST_STRPTR text, CONST_STRPTR voiceId, CONST_STRPTR modelId,
    CONST_STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
    CONST_STRPTR apiKey, ULONG *audioLength, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword);

/**
 * Post a text to speech request to xAI
 * @param text the text to speak
 * @param voiceId the voice_id string (built-in like "eve" or 8-char custom id)
 * @param language the BCP-47 language code (e.g. "en", "auto"); defaults to
 * "en" if NULL/empty
 * @param apiKey the xAI API key
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @param audioFormat the audio format to use
 * @return a pointer to a buffer containing the audio data or NULL -- Free it
 * with FreeVec() when you are done using it
 **/
APTR postTextToSpeechRequestToXAI(
    CONST_STRPTR text, CONST_STRPTR voiceId, CONST_STRPTR language,
    CONST_STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
    CONST_STRPTR apiKey, ULONG *audioLength, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, AudioFormat *audioFormat);

/**
 * Synthesize speech with an OpenVox server. OpenVox returns a WAV file (or a
 * base64 WAV in an audio.chunk event); this function returns its raw
 * little-endian 16-bit mono PCM payload for the shared AHI playback path.
 */
APTR postTextToSpeechRequestToOpenVox(
    CONST_STRPTR text, CONST_STRPTR model, CONST_STRPTR voice,
    CONST_STRPTR language, CONST_STRPTR host, UWORD port, BOOL useSSL,
    CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
    CONST_STRPTR apiKey, ULONG *audioLength, ULONG *sampleRate, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword);

/**
 * Fetch the list of custom voices from xAI (GET /v1/custom-voices).
 * Returns the parsed JSON object or NULL on error. The caller must
 * json_object_put() the result.
 **/
struct json_object *getXAICustomVoices(
    CONST_STRPTR host, UWORD port, BOOL useSSL, CONST_STRPTR apiEndpointUrl,
    AuthorizationType authorizationType, CONST_STRPTR apiKey, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword);

/**
 * Determine an attachment's MIME type by inspecting its contents with
 * libmagic.
 * @param path the file to inspect
 * @returns an AllocVec'd MIME type string the caller must FreeVec, or NULL if
 * no magic database is available -- callers should fall back to guessing from
 * the file name in that case
 **/
STRPTR detectMimeTypeFromContents(CONST_STRPTR path);

/**
 * True for api.openai.com and for AmiKit's OpenAI-compatible proxy. The proxy
 * hostname can change; any host or endpoint URL that contains "amikit"
 * (case-insensitive) is treated as OpenAI for Files API uploads, hosted tools
 * and attachment handling.
 **/
BOOL hostIsOpenAICompatible(CONST_STRPTR host, CONST_STRPTR apiEndpointUrl);

#endif
