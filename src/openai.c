#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
#include <amissl/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#else
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <netdb.h>
#endif
#include <json-c/json.h>
#include <mui/Busy_mcc.h>
#include <proto/exec.h>
#include <proto/socket.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <string.h>
#include <stdio.h>
#include "openai.h"
#include "speech.h"
#include "gui.h"
#include "MainWindow.h"
#include "AmigaGPTConfig.h"

#define OPENAI_HOST "api.openai.com"
#define OPENAI_PORT 443
#define AUDIO_BUFFER_SIZE 4096
#define MAX_CONNECTION_RETRIES 10

/* Static variables to store pending tool call info captured during streaming */
static BOOL pendingToolCall = FALSE;
static UBYTE pendingToolCallId[256] = {0};
static UBYTE pendingToolCommand[4096] = {0};
static UBYTE pendingResponseId[256] = {0};

static ULONG createSSLConnection(CONST_STRPTR host, UWORD port, BOOL useSSL,
                                 BOOL useProxy, CONST_STRPTR proxyHost,
                                 UWORD proxyPort, BOOL proxyUsesSSL,
                                 BOOL proxyRequiresAuth,
                                 CONST_STRPTR proxyUsername,
                                 CONST_STRPTR proxyPassword);
static ULONG rangeRand(ULONG maxValue);
static BPTR ErrOutput();
static BPTR GetStdErr();
static LONG createSSLContext();
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);
static ULONG parseChunkLength(UBYTE *buffer, ULONG bufferLength);
static STRPTR base64Encode(CONST_STRPTR input);
static void drainOpenSslErrorQueue(CONST_STRPTR where);
static void reportSslError(SSL *s, int ret, CONST_STRPTR where);
static STRPTR extractUserFriendlyErrorMessage(CONST_STRPTR rawMessage);

struct Library *SocketBase;
#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
struct Library *AmiSSLMasterBase, *AmiSSLBase, *AmiSSLExtBase;
#endif
#ifdef __AMIGAOS4__
struct AmiSSLMasterIFace *IAmiSSLMaster;
struct AmiSSLIFace *IAmiSSL;
struct SocketIFace *ISocket;
#endif

static SSL_CTX *_ssl_context;
LONG UsesOpenSSLStructs = FALSE;
BOOL amiSSLInitialized = FALSE;
UBYTE *writeBuffer = NULL;
UBYTE *readBuffer = NULL;
X509 *server_cert;
SSL_CTX *ctx = NULL;
BIO *bio, *bio_err;
SSL *ssl;
LONG sock = -1;
LONG ssl_err = 0;
ULONG RangeSeed;

/**
 * The names of the chat models
 * @see ChatModel
 **/
CONST_STRPTR CHAT_MODEL_NAMES[] = {[CHATGPT_5_LATEST] = "gpt-5-chat-latest",
                                   [GPT_3_5_TURBO] = "gpt-3.5-turbo",
                                   [GPT_4] = "gpt-4",
                                   [GPT_4_1] = "gpt-4.1",
                                   [GPT_4_1_MINI] = "gpt-4.1-mini",
                                   [GPT_4_1_NANO] = "gpt-4.1-nano",
                                   [GPT_4_5_PREVIEW] = "gpt-4.5-preview",
                                   [GPT_4_TURBO] = "gpt-4-turbo",
                                   [GPT_4_TURBO_PREVIEW] =
                                       "gpt-4-turbo-preview",
                                   [GPT_4o] = "gpt-4o",
                                   [GPT_4o_MINI] = "gpt-4o-mini",
                                   [GPT_5] = "gpt-5",
                                   [GPT_5_MINI] = "gpt-5-mini",
                                   [GPT_5_NANO] = "gpt-5-nano",
                                   [GPT_5_1] = "gpt-5.1",
                                   [GPT_5_2] = "gpt-5.2",
                                   [GPT_5_2_PRO] = "gpt-5.2-pro",
                                   [o1] = "o1",
                                   [o1_PREVIEW] = "o1-preview",
                                   [o1_MINI] = "o1-mini",
                                   [o1_PRO] = "o1-pro",
                                   [o3] = "o3",
                                   [o3_MINI] = "o3-mini",
                                   [o4_MINI] = "o4-mini",
                                   NULL};

/**
 * The names of the image models
 * @see ImageModel
 **/
CONST_STRPTR IMAGE_MODEL_NAMES[] = {
    [DALL_E_2] = "dall-e-2",           [DALL_E_3] = "dall-e-3",
    [GPT_IMAGE_1] = "gpt-image-1",     [GPT_IMAGE_1_MINI] = "gpt-image-1-mini",
    [GPT_IMAGE_1_5] = "gpt-image-1.5", NULL};

/**
 * The names of the image sizes
 * @see ImageSize
 **/
CONST_STRPTR IMAGE_SIZE_NAMES[] = {[IMAGE_SIZE_256x256] = "256x256",
                                   [IMAGE_SIZE_512x512] = "512x512",
                                   [IMAGE_SIZE_1024x1024] = "1024x1024",
                                   [IMAGE_SIZE_1792x1024] = "1792x1024",
                                   [IMAGE_SIZE_1024x1792] = "1024x1792",
                                   [IMAGE_SIZE_1536x1024] = "1536x1024",
                                   [IMAGE_SIZE_1024x1536] = "1024x1536",
                                   [IMAGE_SIZE_AUTO] = "auto",
                                   NULL};

/**
 * The names of the TTS models
 * @see OpenAITTSModel
 **/
CONST_STRPTR OPENAI_TTS_MODEL_NAMES[] = {
    [OPENAI_TTS_MODEL_TTS_1] = "tts-1",
    [OPENAI_TTS_MODEL_TTS_1_HD] = "tts-1-hd",
    [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS] = "gpt-4o-mini-tts",
    NULL};

/**
 * The image sizes for DALL-E 2
 * @see ImageSize
 **/
const ImageSize IMAGE_SIZES_DALL_E_2[] = {
    IMAGE_SIZE_256x256, IMAGE_SIZE_512x512, IMAGE_SIZE_1024x1024,
    IMAGE_SIZE_NULL};

/**
 * The image sizes for DALL-E 3
 * @see ImageSize
 **/
const ImageSize IMAGE_SIZES_DALL_E_3[] = {
    IMAGE_SIZE_1024x1024, IMAGE_SIZE_1792x1024, IMAGE_SIZE_1024x1792,
    IMAGE_SIZE_NULL};

/**
 * The image sizes for GPT Image 1
 * @see ImageSize
 **/
const ImageSize IMAGE_SIZES_GPT_IMAGE_1[] = {
    IMAGE_SIZE_1024x1024, IMAGE_SIZE_1536x1024, IMAGE_SIZE_1024x1536,
    IMAGE_SIZE_AUTO, IMAGE_SIZE_NULL};

/**
 * The names of the TTS voices
 * @see OpenAITTSVoice
 **/
CONST_STRPTR OPENAI_TTS_VOICE_NAMES[] = {[OPENAI_TTS_VOICE_ALLOY] = "alloy",
                                         [OPENAI_TTS_VOICE_ASH] = "ash",
                                         [OPENAI_TTS_VOICE_BALLAD] = "ballad",
                                         [OPENAI_TTS_VOICE_CORAL] = "coral",
                                         [OPENAI_TTS_VOICE_ECHO] = "echo",
                                         [OPENAI_TTS_VOICE_FABLE] = "fable",
                                         [OPENAI_TTS_VOICE_ONYX] = "onyx",
                                         [OPENAI_TTS_VOICE_NOVA] = "nova",
                                         [OPENAI_TTS_VOICE_SAGE] = "sage",
                                         [OPENAI_TTS_VOICE_SHIMMER] = "shimmer",
                                         [OPENAI_TTS_VOICE_VERSE] = "verse",
                                         NULL};

/**
 * The names of the API endpoints
 * @see APIEndpoint
 **/
CONST_STRPTR API_CHAT_ENDPOINT_NAMES[] = {
    [API_CHAT_ENDPOINT_RESPONSES] = "responses",
    [API_CHAT_ENDPOINT_CHAT_COMPLETIONS] = "chat/completions",
    [API_CHAT_ENDPOINT_MESSAGES] = "messages",
    [API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT] = "models/:generateContent",
    NULL};

CONST_STRPTR API_IMAGE_ENDPOINT_NAMES[] = {
    [API_IMAGE_ENDPOINT_IMAGES_GENERATIONS] = "images/generations",
    [API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT] = "models/:generateContent",
    NULL};

/**
 * The names of the authorization types
 * @see AuthorizationType
 **/
CONST_STRPTR AUTHORIZATION_TYPE_NAMES[] = {
    [AUTHORIZATION_TYPE_NONE] = "",
    [AUTHORIZATION_TYPE_BEARER] = "Authorization: Bearer",
    [AUTHORIZATION_TYPE_X_API_KEY] = "x-api-key:",
    [AUTHORIZATION_TYPE_X_GOOGLE_API_KEY] = "x-goog-api-key:",
    NULL};

/**
 * The names of the image formats
 * @see ImageFormat
 **/
CONST_STRPTR IMAGE_FORMAT_NAMES[] = {
    [IMAGE_FORMAT_JPG] = "jpeg", [IMAGE_FORMAT_PNG] = "png", NULL};

/**
 * Prepopulated chat models for OpenAI
 **/
CONST_STRPTR OPENAI_CHAT_MODELS[] = {"gpt-5-chat-latest",
                                     "gpt-5.2-pro",
                                     "gpt-5.2",
                                     "gpt-5.1",
                                     "gpt-5",
                                     "gpt-5-mini",
                                     "gpt-5-nano",
                                     "gpt-4.5-preview",
                                     "gpt-4.1",
                                     "gpt-4.1-mini",
                                     "gpt-4.1-nano",
                                     "gpt-4o",
                                     "gpt-4o-mini",
                                     "gpt-4-turbo",
                                     "gpt-4",
                                     "gpt-3.5-turbo",
                                     "o4-mini",
                                     "o3",
                                     "o3-mini",
                                     "o1",
                                     "o1-pro",
                                     "o1-mini",
                                     "o1-preview",
                                     NULL};

/**
 * Prepopulated chat models for Google Gemini
 **/
CONST_STRPTR GEMINI_CHAT_MODELS[] = {
    "gemini-2.5-flash",      "gemini-2.5-flash-lite",
    "gemini-2.5-pro",        "gemini-2.0-flash",
    "gemini-2.0-flash-lite", "gemini-3-flash-preview",
    "gemini-3-pro-preview",  NULL};

/**
 * Prepopulated chat models for xAI Grok
 **/
CONST_STRPTR GROK_CHAT_MODELS[] = {
    "grok-4",           "grok-4-fast", "grok-3", "grok-3-mini", "grok-3-fast",
    "grok-3-fast-beta", "grok-3-beta", "grok-2", NULL};

/**
 * Prepopulated chat models for Anthropic Claude
 **/
CONST_STRPTR ANTHROPIC_CHAT_MODELS[] = {"claude-opus-4-5-20251101",
                                        "claude-sonnet-4-5-20250929",
                                        "claude-sonnet-4-20250514",
                                        "claude-3-5-sonnet-20241022",
                                        "claude-3-5-haiku-20241022",
                                        "claude-3-opus-20240229",
                                        "claude-3-sonnet-20240229",
                                        "claude-3-haiku-20240307",
                                        NULL};

/**
 * Prepopulated image models for OpenAI
 **/
CONST_STRPTR OPENAI_IMAGE_MODELS[] = {"gpt-image-1",      "gpt-image-1.5",
                                      "gpt-image-1-mini", "dall-e-3",
                                      "dall-e-2",         NULL};

/**
 * Prepopulated image models for Google Gemini/Imagen
 **/
CONST_STRPTR GEMINI_IMAGE_MODELS[] = {"imagen-4-ultra",
                                      "imagen-4",
                                      "imagen-4-fast",
                                      "imagen-3",
                                      "gemini-3-pro-image-preview",
                                      "gemini-2.5-flash-image",
                                      NULL};

/**
 * Prepopulated image models for xAI Grok/Aurora
 **/
/* xAI docs currently advertise "grok-2-image" for image generation. */
CONST_STRPTR GROK_IMAGE_MODELS[] = {"grok-2-image", "grok-2-image-1212",
                                    "grok-2-aurora", NULL};

/**
 * Generate a random number
 * @param maxValue the maximum value of the random number
 * @return a random number
 **/
static ULONG rangeRand(ULONG maxValue) {
    ULONG a = RangeSeed;
    UWORD i = maxValue - 1;
    do {
        ULONG b = a;
        a <<= 1;
        if ((LONG)b <= 0)
            a ^= 0x1d872b41;
    } while ((i >>= 1));
    RangeSeed = a;
    if ((UWORD)maxValue)
        return (UWORD)((UWORD)a * (UWORD)maxValue >> 16);
    return (UWORD)a;
}

/**
 * Get the error output
 * @return the error output
 **/
static BPTR ErrOutput() { return (((struct Process *)FindTask(NULL))->pr_CES); }

/**
 * Get the standard error
 * @return the standard error
 **/
static BPTR GetStdErr() {
    BPTR err = ErrOutput();
    return (err ? err : Output());
}

/**
 * Execute a shell command and capture its output
 * @param command the command to execute
 * @param exitCode pointer to store the exit code
 * @return a pointer to a new string containing the command output -- Free it
 * with FreeVec() when done
 **/
STRPTR executeShellCommand(CONST_STRPTR command, LONG *exitCode) {
    BPTR stdoutFile = 0;
    BPTR stderrFile = 0;
    STRPTR outputBuffer = NULL;
    STRPTR errorBuffer = NULL;
    STRPTR combinedOutput = NULL;
    LONG stdoutLen = 0;
    LONG stderrLen = 0;
    LONG result = -1;

    /* Create temp files for stdout and stderr */
    stdoutFile = Open("T:amigagpt_stdout.tmp", MODE_NEWFILE);
    stderrFile = Open("T:amigagpt_stderr.tmp", MODE_NEWFILE);

    if (stdoutFile == 0 || stderrFile == 0) {
        if (stdoutFile)
            Close(stdoutFile);
        if (stderrFile)
            Close(stderrFile);
        *exitCode = -1;
        combinedOutput = AllocVec(64, MEMF_CLEAR);
        if (combinedOutput) {
            strncpy(combinedOutput, "Error: Could not create temp files", 63);
        }
        return combinedOutput;
    }

    /* Execute the command with redirected output */
#if defined(__MORPHOS__)
    /* MorphOS may not support SYS_Error, so redirect both to stdout */
    result = SystemTags(command, SYS_Input, 0, SYS_Output, stdoutFile,
                        NP_StackSize, 32768, TAG_DONE);
#else
    result = SystemTags(command, SYS_Input, 0, SYS_Output, stdoutFile,
                        SYS_Error, stderrFile, NP_StackSize, 32768, TAG_DONE);
#endif

    *exitCode = result;

    Close(stdoutFile);
    Close(stderrFile);

    /* Read stdout */
    stdoutFile = Open("T:amigagpt_stdout.tmp", MODE_OLDFILE);
    if (stdoutFile) {
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
        Seek(stdoutFile, 0, OFFSET_END);
        stdoutLen = Seek(stdoutFile, 0, OFFSET_BEGINNING);
#elif defined(__AMIGAOS4__)
        stdoutLen = GetFileSize(stdoutFile);
#else
        /* AROS */
        struct FileInfoBlock fib;
        ExamineFH64(stdoutFile, &fib, NULL);
        stdoutLen = fib.fib_Size;
#endif
        if (stdoutLen > 0) {
            outputBuffer = AllocVec(stdoutLen + 1, MEMF_CLEAR);
            if (outputBuffer) {
                Read(stdoutFile, outputBuffer, stdoutLen);
                outputBuffer[stdoutLen] = '\0';
            }
        }
        Close(stdoutFile);
    }

    /* Read stderr */
    stderrFile = Open("T:amigagpt_stderr.tmp", MODE_OLDFILE);
    if (stderrFile) {
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
        Seek(stderrFile, 0, OFFSET_END);
        stderrLen = Seek(stderrFile, 0, OFFSET_BEGINNING);
#elif defined(__AMIGAOS4__)
        stderrLen = GetFileSize(stderrFile);
#else
        /* AROS */
        struct FileInfoBlock fib2;
        ExamineFH64(stderrFile, &fib2, NULL);
        stderrLen = fib2.fib_Size;
#endif
        if (stderrLen > 0) {
            errorBuffer = AllocVec(stderrLen + 1, MEMF_CLEAR);
            if (errorBuffer) {
                Read(stderrFile, errorBuffer, stderrLen);
                errorBuffer[stderrLen] = '\0';
            }
        }
        Close(stderrFile);
    }

    /* Delete temp files */
#if defined(__AMIGAOS3__) || defined(__MORPHOS__)
    DeleteFile("T:amigagpt_stdout.tmp");
    DeleteFile("T:amigagpt_stderr.tmp");
#else
    Delete("T:amigagpt_stdout.tmp");
    Delete("T:amigagpt_stderr.tmp");
#endif

    /* Combine stdout and stderr */
    LONG totalLen = (stdoutLen > 0 ? stdoutLen : 0) +
                    (stderrLen > 0 ? stderrLen + 16 : 0) + 64;
    combinedOutput = AllocVec(totalLen, MEMF_CLEAR);
    if (combinedOutput) {
        if (outputBuffer && stdoutLen > 0) {
            strncat(combinedOutput, outputBuffer, totalLen - 1);
        }
        if (errorBuffer && stderrLen > 0) {
            if (stdoutLen > 0) {
                strncat(combinedOutput, "\n\n--- STDERR ---\n", totalLen - 1);
            }
            strncat(combinedOutput, errorBuffer, totalLen - 1);
        }
        if (stdoutLen == 0 && stderrLen == 0) {
            snprintf(combinedOutput, totalLen, "(No output)");
        }
    }

    if (outputBuffer)
        FreeVec(outputBuffer);
    if (errorBuffer)
        FreeVec(errorBuffer);

    /* Convert output from system encoding to UTF-8 for JSON/API compatibility
     */
    if (combinedOutput != NULL) {
        UTF8 *utf8Output =
            CodesetsUTF8Create(CSA_SourceCodeset, (Tag)systemCodeset,
                               CSA_Source, (Tag)combinedOutput, TAG_DONE);
        if (utf8Output != NULL) {
            /* Copy UTF-8 output to AllocVec buffer so caller can use FreeVec */
            LONG utf8Len = strlen((char *)utf8Output);
            STRPTR result = AllocVec(utf8Len + 1, MEMF_CLEAR);
            if (result != NULL) {
                strncpy(result, (char *)utf8Output, utf8Len);
                result[utf8Len] = '\0';
            }
            CodesetsFreeA(utf8Output, NULL);
            FreeVec(combinedOutput);
            return result;
        }
        /* If conversion failed, return original (may have encoding issues) */
    }

    return combinedOutput;
}

/**
 * Check if there is a pending tool call captured during streaming
 * @return TRUE if there is a pending tool call
 **/
BOOL hasPendingToolCall(void) { return pendingToolCall; }

/**
 * Get the pending tool call command
 * @return the command string (do not free)
 **/
STRPTR getPendingToolCommand(void) { return pendingToolCommand; }

/**
 * Get the pending tool call ID
 * @return the call ID string (do not free)
 **/
STRPTR getPendingToolCallId(void) { return pendingToolCallId; }

/**
 * Get the pending response ID
 * @return the response ID string (do not free)
 **/
STRPTR getPendingResponseId(void) { return pendingResponseId; }

/**
 * Clear the pending tool call after processing
 **/
void clearPendingToolCall(void) {
    pendingToolCall = FALSE;
    pendingToolCallId[0] = '\0';
    pendingToolCommand[0] = '\0';
    pendingResponseId[0] = '\0';
}

/**
 * Check if a response contains a shell tool function call
 * @param response the JSON response from the API
 * @return TRUE if the response contains a shell function call
 **/
BOOL hasShellToolCall(struct json_object *response) {
    if (response == NULL) {
        return FALSE;
    }

    /* For streaming mode, response.completed has structure:
     * {"type": "response.completed", "response": {"output": [...]}}
     * For non-streaming, it's: {"output": [...]} */
    struct json_object *outputArray =
        json_object_object_get(response, "output");
    struct json_object *nestedResponse = NULL;

    if (outputArray == NULL ||
        !json_object_is_type(outputArray, json_type_array)) {
        /* Try nested structure for streaming mode */
        nestedResponse = json_object_object_get(response, "response");
        if (nestedResponse != NULL) {
            outputArray = json_object_object_get(nestedResponse, "output");
        }
    }

    if (outputArray == NULL ||
        !json_object_is_type(outputArray, json_type_array)) {
        return FALSE;
    }

    int arrayLength = json_object_array_length(outputArray);

    for (int i = 0; i < arrayLength; i++) {
        struct json_object *item = json_object_array_get_idx(outputArray, i);
        struct json_object *itemTypeObj = json_object_object_get(item, "type");
        if (itemTypeObj != NULL) {
            const char *typeStr = json_object_get_string(itemTypeObj);
            if (strcmp(typeStr, "function_call") == 0) {
                struct json_object *nameObj =
                    json_object_object_get(item, "name");
                if (nameObj != NULL) {
                    const char *nameStr = json_object_get_string(nameObj);
                    if (strcmp(nameStr, "shell") == 0) {
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

/**
 * Get the call ID from a shell tool function call response
 * @param response the JSON response from the API
 * @return the call ID string (do not free) or NULL
 **/
STRPTR getShellToolCallId(struct json_object *response) {
    if (response == NULL)
        return NULL;

    /* For streaming mode, response.completed has structure:
     * {"type": "response.completed", "response": {"output": [...]}}
     * For non-streaming, it's: {"output": [...]} */
    struct json_object *outputArray =
        json_object_object_get(response, "output");
    if (outputArray == NULL ||
        !json_object_is_type(outputArray, json_type_array)) {
        /* Try nested structure for streaming mode */
        struct json_object *nestedResponse =
            json_object_object_get(response, "response");
        if (nestedResponse != NULL) {
            outputArray = json_object_object_get(nestedResponse, "output");
        }
    }
    if (outputArray == NULL ||
        !json_object_is_type(outputArray, json_type_array))
        return NULL;

    int arrayLength = json_object_array_length(outputArray);
    for (int i = 0; i < arrayLength; i++) {
        struct json_object *item = json_object_array_get_idx(outputArray, i);
        struct json_object *typeObj = json_object_object_get(item, "type");
        if (typeObj != NULL) {
            const char *typeStr = json_object_get_string(typeObj);
            if (strcmp(typeStr, "function_call") == 0) {
                struct json_object *nameObj =
                    json_object_object_get(item, "name");
                if (nameObj != NULL) {
                    const char *nameStr = json_object_get_string(nameObj);
                    if (strcmp(nameStr, "shell") == 0) {
                        struct json_object *callIdObj =
                            json_object_object_get(item, "call_id");
                        if (callIdObj != NULL) {
                            return (STRPTR)json_object_get_string(callIdObj);
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

/**
 * Get the command from a shell tool function call response
 * @param response the JSON response from the API
 * @return the command string (must be freed with FreeVec) or NULL
 **/
STRPTR getShellToolCommand(struct json_object *response) {
    if (response == NULL)
        return NULL;

    /* For streaming mode, response.completed has structure:
     * {"type": "response.completed", "response": {"output": [...]}}
     * For non-streaming, it's: {"output": [...]} */
    struct json_object *outputArray =
        json_object_object_get(response, "output");
    if (outputArray == NULL ||
        !json_object_is_type(outputArray, json_type_array)) {
        /* Try nested structure for streaming mode */
        struct json_object *nestedResponse =
            json_object_object_get(response, "response");
        if (nestedResponse != NULL) {
            outputArray = json_object_object_get(nestedResponse, "output");
        }
    }
    if (outputArray == NULL ||
        !json_object_is_type(outputArray, json_type_array))
        return NULL;

    int arrayLength = json_object_array_length(outputArray);
    for (int i = 0; i < arrayLength; i++) {
        struct json_object *item = json_object_array_get_idx(outputArray, i);
        struct json_object *typeObj = json_object_object_get(item, "type");
        if (typeObj != NULL) {
            const char *typeStr = json_object_get_string(typeObj);
            if (strcmp(typeStr, "function_call") == 0) {
                struct json_object *nameObj =
                    json_object_object_get(item, "name");
                if (nameObj != NULL) {
                    const char *nameStr = json_object_get_string(nameObj);
                    if (strcmp(nameStr, "shell") == 0) {
                        struct json_object *argsObj =
                            json_object_object_get(item, "arguments");
                        if (argsObj != NULL) {
                            /* Arguments is a JSON string containing another
                             * JSON object */
                            const char *argsStr =
                                json_object_get_string(argsObj);
                            struct json_object *parsedArgs =
                                json_tokener_parse(argsStr);
                            if (parsedArgs != NULL) {
                                struct json_object *cmdObj =
                                    json_object_object_get(parsedArgs,
                                                           "command");
                                if (cmdObj != NULL) {
                                    const char *cmdStr =
                                        json_object_get_string(cmdObj);
                                    STRPTR result = AllocVec(strlen(cmdStr) + 1,
                                                             MEMF_CLEAR);
                                    if (result) {
                                        strncpy(result, cmdStr, strlen(cmdStr));
                                    }
                                    json_object_put(parsedArgs);
                                    return result;
                                }
                                json_object_put(parsedArgs);
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}

/**
 * Initialize the OpenAI connector
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG initOpenAIConnector() {
    readBuffer = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY);
    writeBuffer = AllocVec(WRITE_BUFFER_LENGTH, MEMF_ANY);

#if defined(__AMIGAOS3__)
    if ((SocketBase = OpenLibrary("bsdsocket.library", 0)) == NULL) {
        displayError(STRING_ERROR_BSDSOCKET_LIB_OPEN);
        return RETURN_ERROR;
    }
#elif defined(__AMIGAOS4__)
    if ((SocketBase = OpenLibrary("bsdsocket.library", 4)) == NULL) {
        displayError(STRING_ERROR_BSDSOCKET_LIB_OPEN_OS4);
        return RETURN_ERROR;
    }
    if ((ISocket = (struct SocketIFace *)GetInterface(SocketBase, "main", 1,
                                                      NULL)) == NULL) {
        displayError(STRING_ERROR_BSDSOCKET_INTERFACE_OPEN);
        return RETURN_ERROR;
    }
#endif

#ifdef __AMIGAOS3__
    if ((AmiSSLMasterBase = OpenLibrary("amisslmaster.library",
                                        AMISSLMASTER_MIN_VERSION)) == NULL) {
        displayError(STRING_ERROR_AMISSLMASTER_LIB_OPEN);
        return RETURN_ERROR;
    }
#else
#ifdef __AMIGAOS4__
    if ((AmiSSLMasterBase = OpenLibrary("amisslmaster.library",
                                        AMISSLMASTER_MIN_VERSION)) == NULL) {
        displayError(STRING_ERROR_AMISSLMASTER_LIB_OPEN);
        return RETURN_ERROR;
    }
    if ((IAmiSSLMaster = (struct AmiSSLMasterIFace *)GetInterface(
             AmiSSLMasterBase, "main", 1, NULL)) == NULL) {
        displayError(STRING_ERROR_AMISSLMASTER_INTERFACE_OPEN);
        return RETURN_ERROR;
    }
#endif
#endif

#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
    if (OpenAmiSSLTags(
            AMISSL_CURRENT_VERSION, AmiSSL_UsesOpenSSLStructs, TRUE,
            AmiSSL_InitAmiSSL, TRUE, AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
            AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase, AmiSSL_SocketBase,
            (ULONG)SocketBase, AmiSSL_ErrNoPtr, (ULONG)&errno, TAG_DONE) != 0) {
        displayError(STRING_ERROR_AMISSLMASTER_LIB_INIT);
        return RETURN_ERROR;
    }
#endif

#ifdef __AMIGAOS4__
    if ((IAmiSSL = (struct AmiSSLIFace *)GetInterface(AmiSSLBase, "main", 1,
                                                      NULL)) == NULL) {
        displayError(STRING_ERROR_AMISSL_INTERFACE_INIT);
        return RETURN_ERROR;
    }
#endif

    amiSSLInitialized = TRUE;

    return createSSLContext();
}

/**
 * Create a new SSL connection to a host with a new socket
 * @param host the host to connect to
 * @param port the port to connect to
 * @param useSSL whether to use SSL or not
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static ULONG createSSLConnection(CONST_STRPTR host, UWORD port, BOOL useSSL,
                                 BOOL useProxy, CONST_STRPTR proxyHost,
                                 UWORD proxyPort, BOOL proxyUsesSSL,
                                 BOOL proxyRequiresAuth,
                                 CONST_STRPTR proxyUsername,
                                 CONST_STRPTR proxyPassword) {
    struct sockaddr_in addr;
    struct hostent *hostent;

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
#endif

    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }

    if (sock > -1) {
        CloseSocket(sock);
        sock = -1;
    }

    if (useSSL || (useProxy && proxyUsesSSL)) {
        /* The following needs to be done once per socket */
        if ((ssl = SSL_new(ctx)) == NULL) {
            displayError(STRING_ERROR_SSL_HANDLE);
            return RETURN_ERROR;
        }
    }

    // Connect to the server first
    if ((hostent = gethostbyname(useProxy ? proxyHost : host)) != NULL) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(useProxy ? proxyPort : port);
        addr.sin_len = hostent->h_length;
        memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length);
    } else {
        displayError(useProxy ? STRING_ERROR_PROXY_HOST : STRING_ERROR_HOST);
        if (ssl != NULL) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = NULL;
        }
        return RETURN_ERROR;
    }

    /* Create a socket and connect to the server */
    if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
        struct timeval timeout;
        timeout.tv_sec = 180;
        timeout.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            displayError(useProxy ? STRING_ERROR_CONNECTION_PROXY
                                  : STRING_ERROR_CONNECTION);
            CloseSocket(sock);
            if (ssl != NULL) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
                ssl = NULL;
            }
            sock = -1;
            return RETURN_ERROR;
        }
    } else {
        displayError(STRING_ERROR_SOCKET_CREATE);
        if (ssl != NULL) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = NULL;
        }
        return RETURN_ERROR;
    }

    if (useProxy && proxyUsesSSL) {
        STRPTR authHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);

        if (proxyRequiresAuth) {
            // Construct the Proxy-Authorization header
            STRPTR credentials =
                AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                         MEMF_ANY | MEMF_CLEAR);
            snprintf(credentials,
                     strlen(proxyUsername) + strlen(proxyPassword) + 2, "%s:%s",
                     proxyUsername, proxyPassword);
            UBYTE *encodedCredentials = base64Encode(credentials);
            snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                     encodedCredentials);
            FreeVec(encodedCredentials);
        }

        // Send CONNECT request with optional Proxy-Authorization header
        STRPTR connectRequest = AllocVec(1024, MEMF_CLEAR | MEMF_ANY);
        snprintf(connectRequest, 1024,
                 "CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n%s\r\n", host, port,
                 host, port, authHeader);

        if (send(sock, connectRequest, strlen(connectRequest), 0) < 0) {
            displayError(STRING_ERROR_CONNECT_REQUEST);
            CloseSocket(sock);
            FreeVec(connectRequest);
            FreeVec(authHeader);
            return RETURN_ERROR;
        }

        FreeVec(connectRequest);
        FreeVec(authHeader);

        STRPTR response = AllocVec(1024, MEMF_ANY | MEMF_CLEAR);
        if (recv(sock, response, 1023, 0) <= 0) {
            if (!strstr(response, "200 Connection established")) {
                displayError(STRING_ERROR_PROXY_AUTH);
            } else {
                displayError(STRING_ERROR_PROXY_NO_RESPONSE);
            }
            CloseSocket(sock);
            FreeVec(response);
            return RETURN_ERROR;
        }
        FreeVec(response);
    }

    if (useSSL) {
        /* Associate the socket with the ssl structure */
        SSL_set_fd(ssl, sock);

        /* Set up SNI (Server Name Indication) */
        SSL_set_tlsext_host_name(ssl, host);

        /* Perform SSL handshake */
        ERR_clear_error();
        ssl_err = SSL_connect(ssl);

        if (ssl_err == 1) {
            /* Handshake successful. */
            // printf("SSL connection to %s using %s\n", host,
            // SSL_get_cipher(ssl));
        } else {
            /* Handshake failed: report with full diagnostics. */
            reportSslError(ssl, ssl_err, "SSL_connect");
            CloseSocket(sock);
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = NULL;
            sock = -1;
            return RETURN_ERROR;
        }
    }

    return RETURN_OK;
}

/**
 * Get the chat models from the OpenAI API
 * @param host the host to use
 * @param port the port to use
 * @param useSSL whether to use SSL or not
 * @param apiKey the OpenAI API key (can be NULL for local LLM)
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @param apiEndpointUrl the API endpoint URL to use
 * @return a pointer to a new json_object array containing the model names or
 * NULL -- Free it with json_object_put() when you are done using it
 **/
struct json_object *
getChatModels(STRPTR host, ULONG port, BOOL useSSL, CONST_STRPTR apiKey,
              BOOL useProxy, CONST_STRPTR proxyHost, ULONG proxyPort,
              BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
              CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword,
              CONST_STRPTR apiEndpointUrl, AuthorizationType authorizationType,
              CONST_STRPTR customHeaders) {
    UBYTE connectionRetryCount = 0;
    if (host == NULL || strlen(host) == 0) {
        host = OPENAI_HOST;
        useSSL = TRUE;
        port = OPENAI_PORT;
    }
    if (port == 0) {
        port = useSSL ? 443 : 80;
    }
    if (useProxy && proxyUsesSSL) {
        useSSL = TRUE;
    }

    /* Build the models endpoint path */
    UBYTE modelsPath[256];
    if (apiEndpointUrl != NULL && strlen(apiEndpointUrl) > 0) {
        snprintf(modelsPath, sizeof(modelsPath), "/%s/models", apiEndpointUrl);
    } else {
        snprintf(modelsPath, sizeof(modelsPath), "/models");
    }

    /* Build the authorization header based on type */
    UBYTE apiAuthHeader[512];
    memset(apiAuthHeader, 0, sizeof(apiAuthHeader));
    if (authorizationType != AUTHORIZATION_TYPE_NONE && apiKey != NULL &&
        strlen(apiKey) > 0) {
        snprintf(apiAuthHeader, sizeof(apiAuthHeader), "%s %s\r\n",
                 AUTHORIZATION_TYPE_NAMES[authorizationType], apiKey);
    }

    /* Build custom headers string with proper line ending */
    UBYTE customHeadersFormatted[1024];
    memset(customHeadersFormatted, 0, sizeof(customHeadersFormatted));
    if (customHeaders != NULL && strlen(customHeaders) > 0) {
        snprintf(customHeadersFormatted, sizeof(customHeadersFormatted),
                 "%s\r\n", customHeaders);
    }

    memset(readBuffer, 0, READ_BUFFER_LENGTH);
    memset(writeBuffer, 0, WRITE_BUFFER_LENGTH);

    // Build HTTP GET request
    STRPTR proxyAuthHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(proxyAuthHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
        FreeVec(credentials);
    }

    if (useSSL || useProxy) {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "GET %s://%s:%d%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "%s"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "%s\r\n",
                 useSSL ? "https" : "http", host, port, modelsPath, host, port,
                 apiAuthHeader, customHeadersFormatted, proxyAuthHeader);
    } else {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "%s"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "%s\r\n",
                 modelsPath, host, port, apiAuthHeader, customHeadersFormatted,
                 proxyAuthHeader);
    }

    FreeVec(proxyAuthHeader);

    updateStatusBar(STRING_CONNECTING, yellowPen);
    while (createSSLConnection(host, port, useSSL, useProxy, proxyHost,
                               proxyPort, proxyUsesSSL, proxyRequiresAuth,
                               proxyUsername, proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            return NULL;
        }
    }

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (models)");
            return NULL;
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
        if (ssl_err <= 0) {
            displayError(STRING_ERROR_REQUEST_WRITE);
            return NULL;
        }
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    // Read response
    struct json_object *response = NULL;
    ULONG totalBytesRead = 0;
    WORD bytesRead = 0;
    BOOL doneReading = FALSE;
    UBYTE *tempReadBuffer = AllocVec(useProxy ? 8192 : TEMP_READ_BUFFER_LENGTH,
                                     MEMF_ANY | MEMF_CLEAR);

    while (!doneReading && totalBytesRead < READ_BUFFER_LENGTH - 1) {
        if (useSSL) {
            ERR_clear_error();
            bytesRead =
                SSL_read(ssl, tempReadBuffer,
                         useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1);
        } else {
            bytesRead =
                recv(sock, tempReadBuffer,
                     useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1, 0);
        }

        if (bytesRead > 0) {
            strncat(readBuffer, tempReadBuffer, bytesRead);
            totalBytesRead += bytesRead;
        }

        /* Parse response.
         *
         * IMPORTANT: Some servers (e.g. Cloudflare) include JSON blobs in HTTP
         * headers (e.g. Report-To / NEL). Do NOT parse from the first '{' in
         * the whole buffer; strip HTTP headers first and parse JSON from body.
         */
        STRPTR body = NULL;
        STRPTR headerEnd = strstr(readBuffer, "\r\n\r\n");
        ULONG headerDelimLen = 4;
        if (headerEnd == NULL) {
            headerEnd = strstr(readBuffer, "\n\n");
            headerDelimLen = 2;
        }
        if (headerEnd != NULL) {
            body = headerEnd + headerDelimLen;
        }

        while (body != NULL && (*body == '\r' || *body == '\n' ||
                                *body == ' ' || *body == '\t')) {
            body++;
        }

        STRPTR jsonStart = NULL;
        if (body != NULL) {
            STRPTR objStart = strchr(body, '{');
            STRPTR arrStart = strchr(body, '[');
            if (objStart != NULL && arrStart != NULL) {
                jsonStart = (objStart < arrStart) ? objStart : arrStart;
            } else if (objStart != NULL) {
                jsonStart = objStart;
            } else if (arrStart != NULL) {
                jsonStart = arrStart;
            }
        }

        if (jsonStart != NULL) {
            response = json_tokener_parse(jsonStart);
            if (response != NULL) {
                doneReading = TRUE;
            }
        }
    }
    FreeVec(tempReadBuffer);

    // Close connection
    CloseSocket(sock);
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    sock = -1;

    if (response == NULL) {
        displayError("Failed to parse models response");
        return NULL;
    }

    // Display error if any
    struct json_object *error = json_object_object_get(response, "error");
    if (error != NULL) {
        displayError(json_object_get_string(error));
        json_object_put(response);
        return NULL;
    }

    /* Extract model IDs from response.
     *
     * OpenAI format: { "data": [ { "id": "gpt-4.1" }, ... ] }
     * Gemini format: { "models": [ { "name": "models/gemini-2.0-flash" }, ... ] }
     */
    struct json_object *modelNames = json_object_new_array();

    struct json_object *dataArray = json_object_object_get(response, "data");
    if (dataArray != NULL && json_object_is_type(dataArray, json_type_array)) {
        int arrayLength = json_object_array_length(dataArray);
        for (int i = 0; i < arrayLength; i++) {
            struct json_object *modelObj =
                json_object_array_get_idx(dataArray, i);
            if (modelObj == NULL)
                continue;
            struct json_object *idObj = json_object_object_get(modelObj, "id");
            if (idObj != NULL) {
                CONST_STRPTR modelId = json_object_get_string(idObj);
                if (modelId != NULL && strlen(modelId) > 0)
                    json_object_array_add(modelNames,
                                          json_object_new_string(modelId));
            }
        }
    } else {
        struct json_object *modelsArray =
            json_object_object_get(response, "models");
        if (modelsArray == NULL ||
            !json_object_is_type(modelsArray, json_type_array)) {
            json_object_put(response);
            json_object_put(modelNames);
            displayError("No 'data' or 'models' field in models response");
            return NULL;
        }

        int arrayLength = json_object_array_length(modelsArray);
        for (int i = 0; i < arrayLength; i++) {
            struct json_object *modelObj =
                json_object_array_get_idx(modelsArray, i);
            if (modelObj == NULL)
                continue;
            struct json_object *nameObj =
                json_object_object_get(modelObj, "name");
            if (nameObj != NULL) {
                CONST_STRPTR fullName = json_object_get_string(nameObj);
                if (fullName != NULL && strlen(fullName) > 0) {
                    /* Gemini prefixes model names with "models/" */
                    CONST_STRPTR name = fullName;
                    if (strncmp(fullName, "models/", 7) == 0)
                        name = fullName + 7;
                    json_object_array_add(modelNames,
                                          json_object_new_string(name));
                }
            }
        }
    }

    json_object_put(response);
    updateStatusBar(STRING_READY, greenPen);

    return modelNames;
}

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
    CONST_STRPTR proxyPassword, BOOL webSearchEnabled,
    APIChatEndpoint apiEndpoint, CONST_STRPTR apiEndpointUrl,
    AuthorizationType authorizationType, CONST_STRPTR customHeaders) {
    if (model == NULL || strlen(model) == 0) {
        displayError("Model not specified");
        return NULL;
    }
    struct json_object **responses =
        AllocVec(sizeof(struct json_object *) * RESPONSE_ARRAY_BUFFER_LENGTH,
                 MEMF_ANY | MEMF_CLEAR);
    static BOOL streamingInProgress = FALSE;
    UWORD responseIndex = 0;
    UBYTE connectionRetryCount = 0;

    /* Always use the connection settings passed in (from the active profile).
     * We do not silently fall back to hardcoded OpenAI defaults here. */
    if (host == NULL || strlen(host) == 0) {
        displayError("Host not specified");
        FreeVec(responses);
        return NULL;
    }
    if (port == 0) {
        port = useSSL ? 443 : 80;
    }

    /* Host heuristics for provider-specific behavior (kept host-driven). */
    BOOL isOpenAiHost = (strcmp(host, OPENAI_HOST) == 0);
    BOOL isXaiHost = (strcmp(host, "api.x.ai") == 0);

    /* OpenAI Responses API supports built-in tools like web_search (and the
     * "shell" tool) in this app. Enable tool injection when talking to OpenAI's
     * own host. */
    BOOL includeOpenAiTools =
        (isOpenAiHost && (apiEndpoint == API_CHAT_ENDPOINT_RESPONSES));

    /* Use Gemini native generateContent when requested by endpoint. */
    BOOL useGeminiGenerateContent =
        (apiEndpoint == API_CHAT_ENDPOINT_GEMINI_GENERATE_CONTENT);
    BOOL effectiveStream = stream;

    if (useProxy && proxyUsesSSL) {
        useSSL = TRUE;
    }

    if (!effectiveStream || !streamingInProgress) {
        memset(readBuffer, 0, READ_BUFFER_LENGTH);
        streamingInProgress = effectiveStream;

        struct json_object *obj = json_object_new_object();
        if (!useGeminiGenerateContent) {
            json_object_object_add(obj, "model", json_object_new_string(model));
        }
        struct json_object *conversationArray = json_object_new_array();

        struct MinNode *conversationNode = conversation->messages->mlh_Head;

        if (useGeminiGenerateContent) {
            /* Gemini native generateContent format:
             * POST /v1beta/models/<model>:generateContent
             * {
             *   "contents": [{ "role": "...", "parts":[{"text":"..."}] }, ...],
             *   "systemInstruction": { "parts":[{"text":"..."}] },
             *   "tools": [{ "google_search": {} }]
             * } */

            if (conversation->system != NULL &&
                strlen(conversation->system) > 0) {
                struct json_object *sysObj = json_object_new_object();
                struct json_object *sysParts = json_object_new_array();
                struct json_object *sysPart0 = json_object_new_object();
                json_object_object_add(
                    sysPart0, "text",
                    json_object_new_string(conversation->system));
                json_object_array_add(sysParts, sysPart0);
                json_object_object_add(sysObj, "parts", sysParts);
                json_object_object_add(obj, "systemInstruction", sysObj);
            }

            while (conversationNode->mln_Succ != NULL) {
                struct ConversationNode *message =
                    (struct ConversationNode *)conversationNode;
                struct json_object *contentObj = json_object_new_object();

                CONST_STRPTR role = "user";
                if (strcmp(message->role, "assistant") == 0) {
                    role = "model";
                } else if (strcmp(message->role, "user") == 0) {
                    role = "user";
                }
                json_object_object_add(contentObj, "role",
                                       json_object_new_string(role));

                struct json_object *partsArr = json_object_new_array();
                struct json_object *partObj = json_object_new_object();
                json_object_object_add(
                    partObj, "text",
                    json_object_new_string(message->content != NULL
                                               ? (const char *)message->content
                                               : ""));
                json_object_array_add(partsArr, partObj);
                json_object_object_add(contentObj, "parts", partsArr);

                json_object_array_add(conversationArray, contentObj);
                conversationNode = conversationNode->mln_Succ;
            }
            json_object_object_add(obj, "contents", conversationArray);

            /* Web search tool (optional) */
            if (webSearchEnabled) {
                struct json_object *toolsArray = json_object_new_array();
                struct json_object *toolObj = json_object_new_object();
                json_object_object_add(toolObj, "google_search",
                                       json_object_new_object());
                json_object_array_add(toolsArray, toolObj);
                json_object_object_add(obj, "tools", toolsArray);
            }

        } else if (apiEndpoint == API_CHAT_ENDPOINT_MESSAGES) {
            /* Anthropic/Claude Messages API format */
            if (conversation->system != NULL &&
                strlen(conversation->system) > 0)
                json_object_object_add(
                    obj, "system",
                    json_object_new_string(conversation->system));
            /* max_tokens is required for Claude API */
            json_object_object_add(obj, "max_tokens",
                                   json_object_new_int(4096));

            /* Build messages array (no system role in messages) */
            while (conversationNode->mln_Succ != NULL) {
                struct ConversationNode *message =
                    (struct ConversationNode *)conversationNode;
                struct json_object *messageObj = json_object_new_object();
                json_object_object_add(messageObj, "role",
                                       json_object_new_string(message->role));
                json_object_object_add(
                    messageObj, "content",
                    json_object_new_string(message->content));
                json_object_array_add(conversationArray, messageObj);
                conversationNode = conversationNode->mln_Succ;
            }
            json_object_object_add(obj, "messages", conversationArray);
            /* Claude streaming is handled differently, disable for now */
            json_object_object_add(obj, "stream",
                                   json_object_new_boolean(FALSE));

            /* Anthropic web search tools (only for Messages API). */
            if (webSearchEnabled) {
                struct json_object *toolsArray = json_object_new_array();
                struct json_object *toolObj = json_object_new_object();
                json_object_object_add(
                    toolObj, "type",
                    json_object_new_string("web_search_20250305"));
                json_object_object_add(toolObj, "name",
                                       json_object_new_string("web_search"));
                json_object_array_add(toolsArray, toolObj);
                json_object_object_add(obj, "tools", toolsArray);
            }
        } else if (apiEndpoint == API_CHAT_ENDPOINT_CHAT_COMPLETIONS) {
            /* OpenAI-compatible chat/completions */
            if (conversation->system != NULL &&
                strlen(conversation->system) > 0) {
                struct json_object *messageObj = json_object_new_object();
                json_object_object_add(messageObj, "role",
                                       json_object_new_string("system"));
                json_object_object_add(
                    messageObj, "content",
                    json_object_new_string(conversation->system));
                json_object_array_add(conversationArray, messageObj);
            }
            while (conversationNode->mln_Succ != NULL) {
                struct ConversationNode *message =
                    (struct ConversationNode *)conversationNode;
                struct json_object *messageObj = json_object_new_object();
                json_object_object_add(messageObj, "role",
                                       json_object_new_string(message->role));
                json_object_object_add(
                    messageObj, "content",
                    json_object_new_string(message->content));
                json_object_array_add(conversationArray, messageObj);
                conversationNode = conversationNode->mln_Succ;
            }
            json_object_object_add(obj, "messages", conversationArray);
            json_object_object_add(
                obj, "stream",
                json_object_new_boolean((json_bool)effectiveStream));

            /* xAI Grok web search tools (host-based heuristic). */
            if (webSearchEnabled && isXaiHost) {
                /* Some providers accept this on chat/completions too. */
                struct json_object *toolsArray = json_object_new_array();
                struct json_object *webObj = json_object_new_object();
                json_object_object_add(webObj, "type",
                                       json_object_new_string("web_search"));
                json_object_array_add(toolsArray, webObj);
                struct json_object *xObj = json_object_new_object();
                json_object_object_add(xObj, "type",
                                       json_object_new_string("x_search"));
                json_object_array_add(toolsArray, xObj);
                json_object_object_add(obj, "tools", toolsArray);
            }
        } else {
            /* Responses-style requests (OpenAI + compatible providers) */
            if (includeOpenAiTools || (webSearchEnabled && isXaiHost)) {
                struct json_object *toolsArray = json_object_new_array();
                if (webSearchEnabled) {
                    if (isXaiHost) {
                        struct json_object *webSearchToolObj =
                            json_object_new_object();
                        json_object_object_add(
                            webSearchToolObj, "type",
                            json_object_new_string("web_search"));
                        json_object_array_add(toolsArray, webSearchToolObj);
                        struct json_object *xSearchToolObj =
                            json_object_new_object();
                        json_object_object_add(
                            xSearchToolObj, "type",
                            json_object_new_string("x_search"));
                        json_object_array_add(toolsArray, xSearchToolObj);
                    } else if (includeOpenAiTools) {
                        struct json_object *webSearchToolObj =
                            json_object_new_object();
                        json_object_object_add(
                            webSearchToolObj, "type",
                            json_object_new_string("web_search"));
                        json_object_array_add(toolsArray, webSearchToolObj);
                    }
                }
                if (includeOpenAiTools && configGetShellToolEnabled()) {
                    struct json_object *shellToolObj = json_object_new_object();
                    json_object_object_add(shellToolObj, "type",
                                           json_object_new_string("function"));
                    json_object_object_add(shellToolObj, "name",
                                           json_object_new_string("shell"));
                    json_object_object_add(
                        shellToolObj, "description",
                        json_object_new_string(
                            "Execute AmigaDOS shell commands on the user's "
                            "Amiga. Use this to run commands, manage files, or "
                            "interact with the system. Returns stdout, stderr, "
                            "and exit code."));
                    struct json_object *paramsObj = json_object_new_object();
                    json_object_object_add(paramsObj, "type",
                                           json_object_new_string("object"));
                    struct json_object *propsObj = json_object_new_object();
                    struct json_object *cmdPropObj = json_object_new_object();
                    json_object_object_add(cmdPropObj, "type",
                                           json_object_new_string("string"));
                    json_object_object_add(
                        cmdPropObj, "description",
                        json_object_new_string(
                            "The AmigaDOS command to execute (e.g. 'list "
                            "RAM:', "
                            "'type myfile.txt', 'cd Work:')"));
                    json_object_object_add(propsObj, "command", cmdPropObj);
                    json_object_object_add(paramsObj, "properties", propsObj);
                    struct json_object *requiredArr = json_object_new_array();
                    json_object_array_add(requiredArr,
                                          json_object_new_string("command"));
                    json_object_object_add(paramsObj, "required", requiredArr);
                    json_object_object_add(shellToolObj, "parameters",
                                           paramsObj);
                    json_object_array_add(toolsArray, shellToolObj);
                }
                json_object_object_add(obj, "tools", toolsArray);
            }

            while (conversationNode->mln_Succ != NULL) {
                struct ConversationNode *message =
                    (struct ConversationNode *)conversationNode;
                struct json_object *messageObj = json_object_new_object();
                json_object_object_add(messageObj, "role",
                                       json_object_new_string(message->role));
                json_object_object_add(
                    messageObj, "content",
                    json_object_new_string(message->content));
                json_object_array_add(conversationArray, messageObj);
                conversationNode = conversationNode->mln_Succ;
            }

            json_object_object_add(obj, "input", conversationArray);
            json_object_object_add(
                obj, "stream",
                json_object_new_boolean((json_bool)effectiveStream));

            if (includeOpenAiTools && configGetShellToolEnabled()) {
                CONST_STRPTR amigaInstructions =
#ifdef __AMIGAOS3__
                    "This system is an Amiga computer running AmigaOS. When "
                    "using the shell tool, "
#elif defined(__AMIGAOS4__)
                    "This system is a PowerPC Amiga computer running AmigaOS. "
                    "When using the shell tool, "
#else
                    "This system is a PowerPC computer running MorphOS. When "
                    "using the shell tool, "
#endif
                    "use AmigaDOS commands (not Unix/Linux). Key differences:\n"
                    "- Directory listing: 'list' or 'dir' (not 'ls')\n"
                    "- Show file contents: 'type' (not 'cat')\n"
                    "- Path separator is ':' for devices and '/' for "
                    "directories (e.g. 'Work:Projects/myfile')\n"
                    "- Devices include RAM:, SYS:, Work:, DH0:, etc.\n"
                    "- Create directory: 'makedir' (not 'mkdir')\n"
                    "- Delete file: 'delete' (not 'rm')\n"
                    "- Copy: 'copy' (not 'cp')\n"
                    "- Rename/move: 'rename' (not 'mv')\n"
                    "- Current directory: 'cd' with no args shows current, 'cd "
                    "<path>' changes\n"
                    "- Redirection: '>' for output, '<' for input, '>>' to "
                    "append\n"
                    "- Pipe character is '|' same as Unix";

                if (conversation->system != NULL &&
                    strlen(conversation->system) > 0) {
                    ULONG combinedLen = strlen(conversation->system) +
                                        strlen(amigaInstructions) + 10;
                    STRPTR combinedInstr = AllocVec(combinedLen, MEMF_CLEAR);
                    if (combinedInstr) {
                        snprintf(combinedInstr, combinedLen, "%s\n\n%s",
                                 conversation->system, amigaInstructions);
                        json_object_object_add(
                            obj, "instructions",
                            json_object_new_string(combinedInstr));
                        FreeVec(combinedInstr);
                    }
                } else {
                    json_object_object_add(
                        obj, "instructions",
                        json_object_new_string(amigaInstructions));
                }
            } else if (conversation->system != NULL &&
                       strlen(conversation->system) > 0) {
                json_object_object_add(
                    obj, "instructions",
                    json_object_new_string(conversation->system));
            }
        }

        CONST_STRPTR jsonString = json_object_to_json_string(obj);

        STRPTR authHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);
        if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
            // Construct the Proxy-Authorization header
            STRPTR credentials =
                AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                         MEMF_ANY | MEMF_CLEAR);
            snprintf(credentials,
                     strlen(proxyUsername) + strlen(proxyPassword) + 2, "%s:%s",
                     proxyUsername, proxyPassword);
            UBYTE *encodedCredentials = base64Encode(credentials);
            snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                     encodedCredentials);
            FreeVec(encodedCredentials);
        }

        CONST_STRPTR effectiveApiEndpointUrl = apiEndpointUrl;
        if (effectiveApiEndpointUrl == NULL) {
            effectiveApiEndpointUrl = "";
        }

        /* Build the API authorization header based on type */
        UBYTE apiAuthHeader[512];
        memset(apiAuthHeader, 0, sizeof(apiAuthHeader));
        if (authorizationType != AUTHORIZATION_TYPE_NONE && apiKey != NULL &&
            strlen(apiKey) > 0) {
            snprintf(apiAuthHeader, sizeof(apiAuthHeader), "%s %s\r\n",
                     AUTHORIZATION_TYPE_NAMES[authorizationType], apiKey);
        }

        /* Build custom headers string with proper line ending */
        UBYTE customHeadersFormatted[1024];
        memset(customHeadersFormatted, 0, sizeof(customHeadersFormatted));
        if (customHeaders != NULL && strlen(customHeaders) > 0) {
            snprintf(customHeadersFormatted, sizeof(customHeadersFormatted),
                     "%s\r\n", customHeaders);
        }

        char endpointNameBuf[256];
        memset(endpointNameBuf, 0, sizeof(endpointNameBuf));
        CONST_STRPTR endpointName = API_CHAT_ENDPOINT_NAMES[apiEndpoint];
        if (useGeminiGenerateContent) {
            if (effectiveStream) {
                snprintf(endpointNameBuf, sizeof(endpointNameBuf),
                         "models/%s:streamGenerateContent?alt=sse", model);
            } else {
                snprintf(endpointNameBuf, sizeof(endpointNameBuf),
                         "models/%s:generateContent", model);
            }
            endpointName = endpointNameBuf;
        }

        if (useSSL || useProxy) {
            snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                     "POST %s://%s:%d%s%s/%s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Content-Type: application/json\r\n"
                     "%s"
                     "%s"
                     "User-Agent: AmigaGPT\r\n"
                     "Content-Length: %lu\r\n"
                     "%s\r\n"
                     "%s\0",
                     useSSL ? "https" : "http", host, port,
                     strlen(effectiveApiEndpointUrl) > 0 ? "/" : "",
                     effectiveApiEndpointUrl, endpointName, host, port,
                     apiAuthHeader, customHeadersFormatted, strlen(jsonString),
                     authHeader, jsonString);
        } else {
            snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                     "POST %s%s/%s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Content-Type: application/json\r\n"
                     "%s"
                     "%s"
                     "User-Agent: AmigaGPT\r\n"
                     "Content-Length: %lu\r\n"
                     "%s\r\n"
                     "%s\0",
                     strlen(effectiveApiEndpointUrl) > 0 ? "/" : "",
                     effectiveApiEndpointUrl, endpointName, host, port,
                     apiAuthHeader, customHeadersFormatted, strlen(jsonString),
                     authHeader, jsonString);
        }

        json_object_put(obj);

        FreeVec(authHeader);

        updateStatusBar(STRING_CONNECTING, yellowPen);
        while (createSSLConnection(host, port, useSSL, useProxy, proxyHost,
                                   proxyPort, proxyUsesSSL, proxyRequiresAuth,
                                   proxyUsername,
                                   proxyPassword) == RETURN_ERROR) {
            if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
                displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
                streamingInProgress = FALSE;
                FreeVec(responses);
                return NULL;
            }
        }
        connectionRetryCount = 0;
        updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
        if (useSSL) {
            ERR_clear_error();
            ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
            if (ssl_err <= 0) {
                reportSslError(ssl, ssl_err, "SSL_write (responses)");
            }
        } else {
            ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
        }
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    if (ssl_err > 0 || effectiveStream) {
        ULONG totalBytesRead = 0;
        WORD bytesRead = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        UBYTE statusMessage[64];
        UBYTE *tempReadBuffer = AllocVec(
            useProxy ? 8192 : TEMP_READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
        while (!doneReading) {
#ifndef DAEMON
            DoMethod(loadingBar, MUIM_Busy_Move);
#endif
            /* IMPORTANT:
             * In streaming mode, we may already have one-or-more complete SSE
             * events buffered in readBuffer from a previous call. Do NOT block
             * on SSL_read/recv if we can return an event immediately. */
            BOOL shouldParseBuffered = FALSE;
            if (effectiveStream && readBuffer != NULL &&
                readBuffer[0] != '\0') {
                STRPTR dataPos = strstr(readBuffer, "data:");
                if (dataPos != NULL) {
                    STRPTR end = strstr(dataPos, "\r\n\r\n");
                    if (end == NULL)
                        end = strstr(dataPos, "\n\n");
                    if (end != NULL) {
                        shouldParseBuffered = TRUE;
                    }
                }
            }

            BOOL didReadBytes = FALSE;
            if (!shouldParseBuffered) {
                if (useSSL) {
                    ERR_clear_error();
                    bytesRead = SSL_read(
                        ssl, tempReadBuffer,
                        useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1);
                } else {
                    bytesRead = recv(
                        sock, tempReadBuffer,
                        useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1, 0);
                }
                didReadBytes = TRUE;
            } else {
                bytesRead = 0;
            }
            snprintf(statusMessage, sizeof(statusMessage),
                     STRING_DOWNLOADING_RESPONSE);
            updateStatusBar(statusMessage, yellowPen);
            // /* Only append when we actually read bytes. */
            if (bytesRead > 0) {
                strncat(readBuffer, tempReadBuffer, bytesRead);
            }
            if (useSSL) {
                /* If we skipped reading because we had buffered events, force
                 * the parsing path. */
                err = didReadBytes ? SSL_get_error(ssl, bytesRead)
                                   : SSL_ERROR_NONE;
            } else {
                err = SSL_ERROR_NONE;
            }
            switch (err) {
            case SSL_ERROR_NONE:
                totalBytesRead += bytesRead;
                if (effectiveStream) {
                    /* Streaming is Server-Sent Events (SSE):
                     *   data: { ...json... }\r\n\r\n
                     *   data: [DONE]\r\n\r\n     (chat.completions style)
                     *
                     * We must extract ONE complete SSE event per call. */

                    /* If we still have HTTP headers in the buffer, validate
                     * status and strip them. */
                    STRPTR httpResponse = strstr(readBuffer, "HTTP/1.1");
                    if (httpResponse != NULL) {
                        STRPTR okCheck = strstr(httpResponse, "200 OK");
                        STRPTR okCheck2 = strstr(httpResponse, "HTTP/1.1 200");
                        if (okCheck == NULL && okCheck2 == NULL) {
                            UBYTE error[256] = {0};
                            for (UWORD i = 0; i < 256; i++) {
                                if (httpResponse[i] == '\r' ||
                                    httpResponse[i] == '\n') {
                                    break;
                                }
                                error[i] = httpResponse[i];
                            }
                            /* IMPORTANT:
                             * In streaming mode, the UI will keep calling us
                             * until it sees either an "error" object or a
                             * completion marker. If we only show an error
                             * dialog here and return no JSON objects, the UI
                             * will retry forever and spam the requester.
                             *
                             * Return a standard {"error":{"message":...}}
                             * object in responses[0] so the caller can stop.
                             */
                            struct json_object *errObj =
                                json_object_new_object();
                            struct json_object *errInner =
                                json_object_new_object();
                            json_object_object_add(
                                errInner, "message",
                                json_object_new_string((CONST_STRPTR)error));
                            json_object_object_add(errObj, "error", errInner);
                            responses[responseIndex++] = errObj;
                            doneReading = TRUE;
                            streamingInProgress = FALSE;
                            break;
                        }

                        STRPTR headerEnd = strstr(readBuffer, "\r\n\r\n");
                        if (headerEnd == NULL)
                            headerEnd = strstr(readBuffer, "\n\n");
                        if (headerEnd != NULL) {
                            STRPTR afterHeaders =
                                headerEnd + ((headerEnd[0] == '\r') ? 4 : 2);
                            memmove(readBuffer, afterHeaders,
                                    strlen(afterHeaders) + 1);
                        }
                    }

                    /* Try to extract a complete SSE event. If we can't, keep
                     * reading from the socket. */
                    while (readBuffer != NULL && readBuffer[0] != '\0') {
                        /* Trim leading CR/LF noise */
                        while (readBuffer[0] == '\r' || readBuffer[0] == '\n') {
                            memmove(readBuffer, readBuffer + 1,
                                    strlen(readBuffer));
                        }

                        STRPTR dataPos = strstr(readBuffer, "data:");
                        if (dataPos == NULL) {
                            break; /* need more bytes */
                        }

                        /* Drop anything before the next 'data:' */
                        if (dataPos != readBuffer) {
                            memmove(readBuffer, dataPos, strlen(dataPos) + 1);
                        }

                        STRPTR eventEnd = strstr(readBuffer, "\r\n\r\n");
                        ULONG delimLen = 4;
                        if (eventEnd == NULL) {
                            eventEnd = strstr(readBuffer, "\n\n");
                            delimLen = 2;
                        }
                        if (eventEnd == NULL) {
                            break; /* incomplete event */
                        }

                        STRPTR payloadStart = readBuffer + 5; /* after data: */
                        while (*payloadStart == ' ' || *payloadStart == '\t') {
                            payloadStart++;
                        }
                        ULONG payloadLen = (ULONG)(eventEnd - payloadStart);
                        while (payloadLen > 0 &&
                               (payloadStart[payloadLen - 1] == '\r' ||
                                payloadStart[payloadLen - 1] == '\n')) {
                            payloadLen--;
                        }

                        /* Consume this event from buffer now or later */
                        STRPTR afterEvent = eventEnd + delimLen;

                        if (payloadLen == 6 &&
                            strncmp(payloadStart, "[DONE]", 6) == 0) {
                            /* Synthesize a response.completed to signal the
                             * UI loop that streaming is finished. */
                            struct json_object *completed =
                                json_object_new_object();
                            json_object_object_add(
                                completed, "type",
                                json_object_new_string("response.completed"));
                            responses[responseIndex++] = completed;

                            memmove(readBuffer, afterEvent,
                                    strlen(afterEvent) + 1);
                            doneReading = TRUE;
                            break;
                        }

                        if (payloadLen > 0 && payloadStart[0] == '{') {
                            STRPTR oneJson =
                                AllocVec(payloadLen + 1, MEMF_ANY | MEMF_CLEAR);
                            if (oneJson != NULL) {
                                CopyMem(payloadStart, oneJson, payloadLen);
                                oneJson[payloadLen] = '\0';
                                struct json_object *parsedResponse =
                                    json_tokener_parse(oneJson);
                                FreeVec(oneJson);
                                if (parsedResponse != NULL) {
                                    responses[responseIndex++] = parsedResponse;
                                    memmove(readBuffer, afterEvent,
                                            strlen(afterEvent) + 1);
                                    doneReading = TRUE;
                                    break;
                                }
                            }
                        }

                        /* If we couldn't parse it, consume and keep scanning to
                         * avoid getting stuck on bad/unknown events. */
                        memmove(readBuffer, afterEvent, strlen(afterEvent) + 1);
                    }

                    /* If we did not produce an event yet, keep reading. */
                    break;
                }

                /* Non-streaming mode: parse the full JSON response.
                 *
                 * IMPORTANT: Some servers (e.g. Cloudflare) include JSON blobs
                 * in HTTP headers (e.g. Report-To / NEL). If we scan from the
                 * start of the buffer, we can accidentally parse a header JSON
                 * object instead of the response body, resulting in an empty
                 * assistant message being saved.
                 *
                 * Always strip HTTP headers first, then parse JSON from body.
                 */
                {
                    if (readBuffer == NULL) {
                        doneReading = TRUE;
                        streamingInProgress = FALSE;
                        break;
                    }

                    STRPTR body = NULL;
                    STRPTR headerEnd = strstr(readBuffer, "\r\n\r\n");
                    ULONG headerDelimLen = 4;
                    if (headerEnd == NULL) {
                        headerEnd = strstr(readBuffer, "\n\n");
                        headerDelimLen = 2;
                    }
                    if (headerEnd != NULL) {
                        body = headerEnd + headerDelimLen;
                    }

                    /* Skip leading whitespace/newlines */
                    while (body != NULL && (*body == '\r' || *body == '\n' ||
                                            *body == ' ' || *body == '\t')) {
                        body++;
                    }

                    /* Find first JSON token in body */
                    STRPTR jsonString = NULL;
                    if (body != NULL) {
                        STRPTR objStart = strchr(body, '{');
                        STRPTR arrStart = strchr(body, '[');
                        if (objStart != NULL && arrStart != NULL) {
                            jsonString =
                                (objStart < arrStart) ? objStart : arrStart;
                        } else if (objStart != NULL) {
                            jsonString = objStart;
                        } else if (arrStart != NULL) {
                            jsonString = arrStart;
                        }
                    }

                    if (jsonString != NULL) {
                        struct json_object *parsedResponse =
                            json_tokener_parse(jsonString);
                        if (parsedResponse != NULL) {
                            responses[responseIndex++] = parsedResponse;
                            doneReading = TRUE;
                        }
                    }
                }

                break;
            case SSL_ERROR_ZERO_RETURN:
                /* Gemini streamGenerateContent typically ends by closing the
                 * connection (no [DONE]). If we reach EOF without having
                 * produced a chunk in this call, synthesize a completion event
                 * so the UI loop doesn't re-send the request forever. */
                if (useGeminiGenerateContent && effectiveStream &&
                    responseIndex == 0) {
                    struct json_object *completed = json_object_new_object();
                    json_object_object_add(
                        completed, "type",
                        json_object_new_string("response.completed"));
                    responses[responseIndex++] = completed;
                    streamingInProgress = FALSE;
                }
                doneReading = TRUE;
                break;
            case SSL_ERROR_WANT_READ:
                printf("SSL_ERROR_WANT_READ\n");
                /* If we've already received the SSE terminator, stop waiting.
                 */
                if (effectiveStream &&
                    strstr(readBuffer, "data: [DONE]") != NULL) {
                    /* Important: return a completion marker to the caller so
                     * the UI loop can terminate instead of re-sending the
                     * request. */
                    struct json_object *completed = json_object_new_object();
                    json_object_object_add(
                        completed, "type",
                        json_object_new_string("response.completed"));
                    responses[responseIndex++] = completed;

                    /* Clear buffer so we don't repeatedly trip this path. */
                    memset(readBuffer, 0, READ_BUFFER_LENGTH);

                    doneReading = TRUE;
                    streamingInProgress = FALSE;
                }
                break;
            case SSL_ERROR_WANT_WRITE:
                printf("SSL_ERROR_WANT_WRITE\n");
                break;
            case SSL_ERROR_WANT_CONNECT:
                printf("SSL_ERROR_WANT_CONNECT\n");
                break;
            case SSL_ERROR_WANT_ACCEPT:
                printf("SSL_ERROR_WANT_ACCEPT\n");
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                printf("SSL_ERROR_WANT_X509_LOOKUP\n");
                break;
            case SSL_ERROR_SYSCALL:
            case SSL_ERROR_SSL:
                // Check if we have a complete JSON response before treating as
                // error
                if (strlen(readBuffer) > 0) {
                    // Try to parse what we have - if it's valid JSON, the
                    // connection closure is normal
                    if (strstr(readBuffer, "data: [DONE]") != NULL) {
                        /* Same as WANT_READ case: make sure caller sees a
                         * completion marker. */
                        struct json_object *completed =
                            json_object_new_object();
                        json_object_object_add(
                            completed, "type",
                            json_object_new_string("response.completed"));
                        responses[responseIndex++] = completed;

                        memset(readBuffer, 0, READ_BUFFER_LENGTH);

                        doneReading = TRUE;
                        streamingInProgress = FALSE;
                        break;
                    }
                    const STRPTR jsonStart = effectiveStream ? "data: {" : "{";
                    STRPTR jsonString = effectiveStream
                                            ? strstr(readBuffer, jsonStart)
                                            : strstr(readBuffer, "{");
                    if (jsonString != NULL) {
                        // We have JSON data - this might be a normal connection
                        // closure
                        doneReading = TRUE;
                        break;
                    }
                }
                // No valid data received - this is a real error
                reportSslError(ssl, bytesRead, "SSL_read (responses)");
                doneReading = TRUE;
                break;
            default:
                printf(STRING_ERROR_UNKNOWN);
                putchar('\n');
                break;
            }
        }
        FreeVec(tempReadBuffer);
    } else {
        displayError(STRING_ERROR_REQUEST_WRITE);
        reportSslError(ssl, ssl_err, "SSL_write");
    }
    if (effectiveStream) {
        // Check if the last response is the end of the stream and set the
        // streamingInProgress flag to FALSE so that the next request will
        // establish a new connection because OpenAI will close the connection
        // after the stream is finished
        if (responseIndex > 0 && responses[responseIndex - 1] != NULL) {
            STRPTR type = json_object_get_string(
                json_object_object_get(responses[responseIndex - 1], "type"));
            if (type != NULL && strcmp(type, "response.completed") == 0) {
                streamingInProgress = FALSE;

                /* Check for tool call in response.completed and save it */
                if (configGetShellToolEnabled()) {
                    struct json_object *completedResponse =
                        responses[responseIndex - 1];
                    if (hasShellToolCall(completedResponse)) {
                        STRPTR callId = getShellToolCallId(completedResponse);
                        STRPTR command = getShellToolCommand(completedResponse);
                        /* Get response ID from nested response object */
                        struct json_object *nestedResp = json_object_object_get(
                            completedResponse, "response");
                        STRPTR respId = NULL;
                        if (nestedResp) {
                            respId = (STRPTR)json_object_get_string(
                                json_object_object_get(nestedResp, "id"));
                        }

                        if (callId && command && respId) {
                            pendingToolCall = TRUE;
                            strncpy(pendingToolCallId, callId,
                                    sizeof(pendingToolCallId) - 1);
                            strncpy(pendingToolCommand, command,
                                    sizeof(pendingToolCommand) - 1);
                            strncpy(pendingResponseId, respId,
                                    sizeof(pendingResponseId) - 1);
                        }
                    }
                }
            }
        }
        struct json_object *error;
        if (json_object_object_get_ex(responses[responseIndex - 1], "error",
                                      &error) &&
            !json_object_is_type(error, json_type_null)) {
            CloseSocket(sock);
            if (ssl != NULL) {
                SSL_shutdown(ssl);
                SSL_free(ssl);
                ssl = NULL;
            }
            sock = -1;
            streamingInProgress = FALSE;
        }
    } else {
        /* Non-streaming mode - check for tool calls in the response */
        if (responseIndex > 0 && responses[responseIndex - 1] != NULL &&
            configGetShellToolEnabled()) {
            struct json_object *response = responses[responseIndex - 1];
            if (hasShellToolCall(response)) {
                STRPTR callId = getShellToolCallId(response);
                STRPTR command = getShellToolCommand(response);
                /* For non-streaming, id is at top level */
                STRPTR respId = (STRPTR)json_object_get_string(
                    json_object_object_get(response, "id"));

                if (callId && command && respId) {
                    pendingToolCall = TRUE;
                    strncpy(pendingToolCallId, callId,
                            sizeof(pendingToolCallId) - 1);
                    strncpy(pendingToolCommand, command,
                            sizeof(pendingToolCommand) - 1);
                    strncpy(pendingResponseId, respId,
                            sizeof(pendingResponseId) - 1);
                }
            }
        }
        CloseSocket(sock);
        if (ssl != NULL) {
            SSL_shutdown(ssl);
            SSL_free(ssl);
            ssl = NULL;
        }
        sock = -1;
    }
    return responses;
}

/**
 * Decode a base64 encoded string
 * @param dataB64 the base64 encoded string
 * @param data_len the length of the decoded data
 * @return a pointer to the decoded data
 */
UBYTE *decodeBase64(UBYTE *dataB64, LONG *data_len) {
    LONG dataB64_len = strlen(dataB64);
    *data_len = 3 * dataB64_len / 4;
    UBYTE *data = AllocVec(*data_len, MEMF_ANY | MEMF_CLEAR);
    EVP_DecodeBlock(data, dataB64, dataB64_len);
    while (dataB64[--dataB64_len] == '=')
        (*data_len)--;
    return data;
}

/**
 * Post a image creation request to OpenAI
 * @param prompt the prompt to use
 * @param imageModel the image model to use
 * @param imageSize the size of the image to create
 * @param apiKey the OpenAI API key
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
    APIImageEndpoint apiEndpoint) {
    struct json_object *response = NULL;
    UWORD responseIndex = 0;
    BOOL requestUsesSSL = useSSL;
    BOOL isGeminiGenerateContent =
        (apiEndpoint == API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT);

    if (useProxy && proxyUsesSSL) {
        requestUsesSSL = TRUE;
    }
    struct json_tokener *tokener = json_tokener_new();
    json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);

    memset(readBuffer, 0, READ_BUFFER_LENGTH);

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(host, port, requestUsesSSL, useProxy, proxyHost,
                               proxyPort, proxyUsesSSL, proxyRequiresAuth,
                               proxyUsername, proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            json_tokener_free(tokener);
            return NULL;
        }
    }
    connectionRetryCount = 0;

    struct json_object *obj = json_object_new_object();
    if (apiEndpoint == API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT) {
        /* Gemini native text-to-image endpoint:
         * POST /v1beta/models/<model>:generateContent
         * Header: x-goog-api-key
         * Body: { "contents":[{"parts":[{"text":"..."}]}],
         *         "generationConfig":{"responseModalities":["TEXT","IMAGE"]} }
         * Docs: https://ai.google.dev/gemini-api/docs/image-generation#rest */
        struct json_object *contentsArr = json_object_new_array();
        struct json_object *contentObj = json_object_new_object();
        struct json_object *partsArr = json_object_new_array();
        struct json_object *partObj = json_object_new_object();
        json_object_object_add(partObj, "text",
                               json_object_new_string(prompt ? prompt : ""));
        json_object_array_add(partsArr, partObj);
        json_object_object_add(contentObj, "parts", partsArr);
        json_object_array_add(contentsArr, contentObj);
        json_object_object_add(obj, "contents", contentsArr);

        struct json_object *generationConfigObj = json_object_new_object();
        struct json_object *modalitiesArr = json_object_new_array();
        json_object_array_add(modalitiesArr, json_object_new_string("TEXT"));
        json_object_array_add(modalitiesArr, json_object_new_string("IMAGE"));
        json_object_object_add(generationConfigObj, "responseModalities",
                               modalitiesArr);
        json_object_object_add(obj, "generationConfig", generationConfigObj);
    } else {
        json_object_object_add(
            obj, "model", json_object_new_string(modelName ? modelName : ""));
        json_object_object_add(obj, "prompt", json_object_new_string(prompt));
        /* Provider differences:
         * - xAI image generation docs: size/quality/style not supported.
         * - Gemini OpenAI-compat examples omit size; keep request minimal.
         * - OpenAI gpt-image-* supports size/output_format. */
        BOOL isGptImage =
            (modelName != NULL && strncmp(modelName, "gpt-image-", 10) == 0);
        printf("modelName: %s\n", modelName);
        printf("apiEndpoint: %s\n", API_IMAGE_ENDPOINT_NAMES[apiEndpoint]);
        printf("imageSize: %s\n", IMAGE_SIZE_NAMES[imageSize]);
        printf("imageFormat: %s\n", IMAGE_FORMAT_NAMES[imageFormat]);
        printf("isGptImage: %d\n", isGptImage);
        if (apiEndpoint == API_IMAGE_ENDPOINT_IMAGES_GENERATIONS) {
            if (imageSize != IMAGE_SIZE_NULL &&
                IMAGE_SIZE_NAMES[imageSize] != NULL)
                json_object_object_add(
                    obj, "size",
                    json_object_new_string(IMAGE_SIZE_NAMES[imageSize]));
        }

        if (isGptImage && (apiEndpoint == API_IMAGE_ENDPOINT_IMAGES_GENERATIONS)) {
            json_object_object_add(obj, "moderation",
                                   json_object_new_string("low"));
            json_object_object_add(
                obj, "output_format",
                json_object_new_string(IMAGE_FORMAT_NAMES[imageFormat]));
            json_object_object_add(obj, "output_compression",
                                   json_object_new_int(100));
        } else {
            json_object_object_add(obj, "response_format",
                                   json_object_new_string("b64_json"));
        }
    }
    CONST_STRPTR jsonString = json_object_to_json_string(obj);

    STRPTR authHeader = AllocVec(512, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        // Construct the Proxy-Authorization header
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
    }

    /* Build auth header based on type */
    UBYTE apiAuthHeader[512];
    memset(apiAuthHeader, 0, sizeof(apiAuthHeader));
    if (authorizationType != AUTHORIZATION_TYPE_NONE && apiKey != NULL &&
        strlen(apiKey) > 0) {
        snprintf(apiAuthHeader, sizeof(apiAuthHeader), "%s %s\r\n",
                 AUTHORIZATION_TYPE_NAMES[authorizationType], apiKey);
    }

    /* Custom headers string with line ending */
    UBYTE customHeadersFormatted[1024];
    memset(customHeadersFormatted, 0, sizeof(customHeadersFormatted));
    if (customHeaders != NULL && strlen(customHeaders) > 0) {
        snprintf(customHeadersFormatted, sizeof(customHeadersFormatted),
                 "%s\r\n", customHeaders);
    }

    const char *endpointUrl = NULL;
    endpointUrl = (apiEndpointUrl != NULL && strlen(apiEndpointUrl) > 0)
                      ? apiEndpointUrl
                      : "";

    UBYTE requestPath[256];
    memset(requestPath, 0, sizeof(requestPath));
    if (apiEndpoint == API_IMAGE_ENDPOINT_GEMINI_GENERATE_CONTENT) {
        if (modelName == NULL || strlen(modelName) == 0) {
            displayError("Gemini model not specified");
            json_object_put(obj);
            FreeVec(authHeader);
            json_tokener_free(tokener);
            return NULL;
        }
        snprintf(requestPath, sizeof(requestPath),
                 "%s/models/%s:generateContent", endpointUrl, modelName);
    } else {
        snprintf(requestPath, sizeof(requestPath), "%s/images/generations",
                 endpointUrl);
    }

    if (requestUsesSSL || useProxy) {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "POST %s://%s:%d/%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "%s"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "Content-Length: %lu\r\n"
                 "%s\r\n"
                 "%s\0",
                 requestUsesSSL ? "https" : "http", host, port, requestPath,
                 host, port, apiAuthHeader, customHeadersFormatted,
                 strlen(jsonString), authHeader, jsonString);
    } else {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "POST /%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "%s"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "Content-Length: %lu\r\n"
                 "%s\r\n"
                 "%s\0",
                 requestPath, host, port, apiAuthHeader, customHeadersFormatted,
                 strlen(jsonString), authHeader, jsonString);
    }

    json_object_put(obj);
    printf("writeBuffer: %s\n", writeBuffer);
    FreeVec(authHeader);

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (requestUsesSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (images)");
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    if (ssl_err > 0) {
        ULONG totalBytesRead = 0;
        WORD bytesRead = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        UBYTE statusMessage[64];
        UBYTE *tempReadBuffer = AllocVec(
            useProxy ? 8192 : TEMP_READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);

        /* Robust HTTP body handling (supports chunked transfer encoding). */
        UBYTE headerBuf[8192];
        ULONG headerLen = 0;
        BOOL headersDone = FALSE;
        BOOL isChunked = FALSE;
        BOOL startedJson = FALSE;

        /* Chunked decoding state */
        UBYTE chunkLenLine[32];
        UBYTE chunkLenPos = 0;
        ULONG chunkRemaining = 0;
        UBYTE chunkCrlfToSkip = 0;
        BOOL chunkDone = FALSE;

        while (!doneReading) {
#ifndef DAEMON
            DoMethod(loadingBar, MUIM_Busy_Move);
#endif
            if (requestUsesSSL) {
                ERR_clear_error();
                bytesRead =
                    SSL_read(ssl, tempReadBuffer,
                             useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1);
            } else {
                bytesRead =
                    recv(sock, tempReadBuffer,
                         useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1, 0);
            }
            if (bytesRead > 0) {
                /* Ensure string safety for any incidental debug/str ops. */
                tempReadBuffer[bytesRead] = '\0';
            }
            snprintf(statusMessage, sizeof(statusMessage), "%s (%lu %s)",
                     STRING_DOWNLOADING_IMAGE, totalBytesRead, STRING_BYTES);
            updateStatusBar(statusMessage, yellowPen);
            if (requestUsesSSL) {
                err = SSL_get_error(ssl, bytesRead);
            } else {
                err = SSL_ERROR_NONE;
            }
            switch (err) {
            case SSL_ERROR_NONE: {
                totalBytesRead += bytesRead;
                if (bytesRead <= 0) {
                    /* Connection closed / no more data */
                    doneReading = TRUE;
                    break;
                }

                /* Feed bytes into either header accumulator or body decoder. */
                const UBYTE *p = tempReadBuffer;
                ULONG n = (ULONG)bytesRead;

                while (n > 0 && !doneReading) {
                    if (!headersDone) {
                        /* Accumulate headers until \r\n\r\n (or \n\n). */
                        ULONG space = (sizeof(headerBuf) - 1) - headerLen;
                        ULONG toCopy = (n < space) ? n : space;
                        if (toCopy > 0) {
                            CopyMem((APTR)p, (APTR)(headerBuf + headerLen),
                                    toCopy);
                            headerLen += toCopy;
                            headerBuf[headerLen] = '\0';
                            p += toCopy;
                            n -= toCopy;
                        } else {
                            displayError("HTTP headers too large");
                            doneReading = TRUE;
                            break;
                        }

                        STRPTR hEnd = strstr((STRPTR)headerBuf, "\r\n\r\n");
                        ULONG bodyOffset = 0;
                        if (hEnd != NULL) {
                            bodyOffset = (ULONG)(hEnd - (STRPTR)headerBuf) + 4;
                        } else {
                            hEnd = strstr((STRPTR)headerBuf, "\n\n");
                            if (hEnd != NULL) {
                                bodyOffset =
                                    (ULONG)(hEnd - (STRPTR)headerBuf) + 2;
                            }
                        }
                        if (hEnd == NULL) {
                            /* Need more header bytes. */
                            continue;
                        }

                        headersDone = TRUE;
                        /* Parse status line quickly; fail early if not 200. */
                        STRPTR http = strstr((STRPTR)headerBuf, "HTTP/");
                        if (http != NULL) {
                            STRPTR sp = strchr(http, ' ');
                            if (sp != NULL) {
                                LONG code = atol(sp + 1);
                                if (code != 200) {
                                    UBYTE errorLine[256] = {0};
                                    for (UWORD i = 0; i < 255; i++) {
                                        UBYTE c = (UBYTE)http[i];
                                        if (c == '\r' || c == '\n' || c == 0)
                                            break;
                                        errorLine[i] = c;
                                    }
                                    displayError(errorLine);
                                    doneReading = TRUE;
                                    break;
                                }
                            }
                        }

                        /* Detect chunked transfer encoding. */
                        if (strstr((STRPTR)headerBuf,
                                   "Transfer-Encoding: chunked") != NULL ||
                            strstr((STRPTR)headerBuf,
                                   "transfer-encoding: chunked") != NULL) {
                            isChunked = TRUE;
                        }

                        /* Any body bytes already in headerBuf should be
                         * processed now. */
                        if (bodyOffset < headerLen) {
                            const UBYTE *body = headerBuf + bodyOffset;
                            ULONG bodyLen = headerLen - bodyOffset;

                            /* Process body bytes via the same loop by
                             * re-pointing p/n to the remaining body segment,
                             * then continuing with headersDone=TRUE. */
                            p = body;
                            n = bodyLen;
                            /* Prevent re-processing these bytes via headerBuf
                             * on the next iteration. */
                            headerLen = bodyOffset;
                            /* IMPORTANT: do NOT write a '\0' at bodyOffset.
                             * We may have already received part of the body
                             * (e.g. the first chunk size) in headerBuf, and
                             * null-terminating here would overwrite it. */
                        } else {
                            /* No body bytes yet; continue with remaining input
                             * (if any). */
                            continue;
                        }
                    } /* end header accumulation */

                    /* Body processing: handle chunked or plain bodies. */
                    if (!isChunked) {
                        /* Find the start of JSON once. */
                        if (!startedJson) {
                            const UBYTE *brace =
                                (const UBYTE *)memchr(p, '{', n);
                            if (brace == NULL) {
                                /* No JSON start in this slice. */
                                n = 0;
                                break;
                            }
                            /* Skip everything up to '{'. */
                            n -= (ULONG)(brace - p);
                            p = brace;
                            startedJson = TRUE;
                        }
                        response =
                            json_tokener_parse_ex(tokener, (const char *)p, n);
                        enum json_tokener_error jerr =
                            json_tokener_get_error(tokener);
                        if (jerr != json_tokener_success &&
                            jerr != json_tokener_continue) {
                            displayError(json_tokener_error_desc(jerr));
                            doneReading = TRUE;
                            break;
                        }
                        if (jerr == json_tokener_success && response != NULL) {
                            doneReading = TRUE;
                            break;
                        }
                        /* Need more bytes. */
                        n = 0;
                        break;
                    }

                    /* Chunked transfer decoding:
                     * Read chunk size line -> read chunk bytes -> skip CRLF. */
                    while (n > 0 && !doneReading && !chunkDone) {
                        if (chunkCrlfToSkip > 0) {
                            /* Skip CRLF after a chunk payload. */
                            chunkCrlfToSkip--;
                            p++;
                            n--;
                            continue;
                        }
                        if (chunkRemaining == 0) {
                            /* Parse chunk length line until '\n'. */
                            UBYTE c = *p;
                            p++;
                            n--;
                            if (c == '\n') {
                                chunkLenLine[chunkLenPos] = '\0';
                                /* Strip optional '\r' and chunk extensions. */
                                for (UBYTE i = 0; i < chunkLenPos; i++) {
                                    if (chunkLenLine[i] == '\r' ||
                                        chunkLenLine[i] == ';') {
                                        chunkLenLine[i] = '\0';
                                        break;
                                    }
                                }
                                chunkRemaining = (ULONG)strtoul(
                                    (char *)chunkLenLine, NULL, 16);
                                chunkLenPos = 0;
                                if (chunkRemaining == 0) {
                                    chunkDone = TRUE;
                                    /* There may be trailing headers; ignore. */
                                    break;
                                }
                            } else {
                                if (chunkLenPos < sizeof(chunkLenLine) - 1) {
                                    chunkLenLine[chunkLenPos++] = c;
                                } else {
                                    displayError("Bad chunk length");
                                    doneReading = TRUE;
                                    break;
                                }
                            }
                            continue;
                        }

                        /* We have chunk payload bytes to consume. */
                        ULONG take = (n < chunkRemaining) ? n : chunkRemaining;
                        if (!startedJson) {
                            const UBYTE *brace =
                                (const UBYTE *)memchr(p, '{', take);
                            if (brace != NULL) {
                                /* Skip up to JSON start within this chunk. */
                                ULONG skip = (ULONG)(brace - p);
                                p += skip;
                                take -= skip;
                                chunkRemaining -= skip;
                                startedJson = TRUE;
                            } else {
                                /* No JSON start yet; skip these bytes. */
                                p += take;
                                n -= take;
                                chunkRemaining -= take;
                                if (chunkRemaining == 0)
                                    chunkCrlfToSkip = 2;
                                continue;
                            }
                        }

                        if (take > 0) {
                            response = json_tokener_parse_ex(
                                tokener, (const char *)p, take);
                            enum json_tokener_error jerr =
                                json_tokener_get_error(tokener);
                            if (jerr != json_tokener_success &&
                                jerr != json_tokener_continue) {
                                displayError(json_tokener_error_desc(jerr));
                                doneReading = TRUE;
                                break;
                            }
                            if (jerr == json_tokener_success &&
                                response != NULL) {
                                /* JSON is complete; no need to read more. */
                                doneReading = TRUE;
                                break;
                            }
                            p += take;
                            n -= take;
                            chunkRemaining -= take;
                        }
                        if (chunkRemaining == 0) {
                            chunkCrlfToSkip = 2;
                        }
                    } /* end chunk decoder */

                    if (chunkDone) {
                        /* Finished chunk stream; if JSON wasn't complete,
                         * surface an error. */
                        enum json_tokener_error jerr =
                            json_tokener_get_error(tokener);
                        if (jerr != json_tokener_success) {
                            displayError("Incomplete JSON (chunked)");
                        }
                        doneReading = TRUE;
                        break;
                    }

                    /* All available bytes consumed. */
                    n = 0;
                }
                break;
            }
            case SSL_ERROR_ZERO_RETURN:
                doneReading = TRUE;
                break;
            case SSL_ERROR_WANT_READ:
                break;
            case SSL_ERROR_WANT_WRITE:
                break;
            case SSL_ERROR_WANT_CONNECT:
                break;
            case SSL_ERROR_WANT_ACCEPT:
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                break;
            case SSL_ERROR_SYSCALL:
            case SSL_ERROR_SSL:
                reportSslError(ssl, bytesRead, "SSL_read (images)");
                doneReading = TRUE;
                break;
            default:
                displayError(STRING_ERROR_UNKNOWN);
                break;
            }
        }
        FreeVec(tempReadBuffer);
    } else {
        displayError(STRING_ERROR_REQUEST_WRITE);
        reportSslError(ssl, ssl_err, "SSL_write");
    }

    CloseSocket(sock);
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    sock = -1;

    if (isGeminiGenerateContent && response != NULL) {
        /* Normalize Gemini response to match OpenAI image JSON shape:
         * { "data": [ { "b64_json": "<base64>" } ] }
         * so existing callers can keep reading response.data[0].b64_json */
        struct json_object *geminiErr = NULL;
        if (json_object_object_get_ex(response, "error", &geminiErr) &&
            geminiErr != NULL &&
            json_object_is_type(geminiErr, json_type_object)) {
            struct json_object *msgObj =
                json_object_object_get(geminiErr, "message");
            CONST_STRPTR msg =
                (msgObj != NULL) ? json_object_get_string(msgObj) : NULL;
            struct json_object *wrapped = json_object_new_object();
            struct json_object *errObj = json_object_new_object();
            json_object_object_add(
                errObj, "message",
                json_object_new_string(msg ? msg
                                           : "Gemini image request failed"));
            json_object_object_add(
                errObj, "type",
                json_object_new_string("invalid_request_error"));
            json_object_object_add(wrapped, "error", errObj);
            json_object_put(response);
            response = wrapped;
        } else {
            CONST_STRPTR b64 = NULL;
            struct json_object *candidates = NULL;
            if (json_object_object_get_ex(response, "candidates",
                                          &candidates) &&
                candidates != NULL &&
                json_object_is_type(candidates, json_type_array) &&
                json_object_array_length(candidates) > 0) {
                struct json_object *cand0 =
                    json_object_array_get_idx(candidates, 0);
                struct json_object *content = NULL;
                if (cand0 != NULL &&
                    json_object_object_get_ex(cand0, "content", &content) &&
                    content != NULL &&
                    json_object_is_type(content, json_type_object)) {
                    struct json_object *parts = NULL;
                    if (json_object_object_get_ex(content, "parts", &parts) &&
                        parts != NULL &&
                        json_object_is_type(parts, json_type_array)) {
                        for (size_t i = 0; i < json_object_array_length(parts);
                             i++) {
                            struct json_object *part =
                                json_object_array_get_idx(parts, i);
                            if (part == NULL ||
                                !json_object_is_type(part, json_type_object))
                                continue;
                            struct json_object *inlineObj = NULL;
                            if (!json_object_object_get_ex(part, "inlineData",
                                                           &inlineObj)) {
                                json_object_object_get_ex(part, "inline_data",
                                                          &inlineObj);
                            }
                            if (inlineObj == NULL ||
                                !json_object_is_type(inlineObj,
                                                     json_type_object))
                                continue;
                            struct json_object *dataObj = NULL;
                            if (json_object_object_get_ex(inlineObj, "data",
                                                          &dataObj) &&
                                dataObj != NULL) {
                                b64 = json_object_get_string(dataObj);
                                if (b64 != NULL && strlen(b64) > 0)
                                    break;
                            }
                        }
                    }
                }
            }

            if (b64 != NULL && strlen(b64) > 0) {
                struct json_object *normalized = json_object_new_object();
                struct json_object *dataArr = json_object_new_array();
                struct json_object *dataObj = json_object_new_object();
                json_object_object_add(dataObj, "b64_json",
                                       json_object_new_string(b64));
                json_object_array_add(dataArr, dataObj);
                json_object_object_add(normalized, "data", dataArr);
                json_object_put(response);
                response = normalized;
            } else {
                struct json_object *wrapped = json_object_new_object();
                struct json_object *errObj = json_object_new_object();
                json_object_object_add(
                    errObj, "message",
                    json_object_new_string(
                        "Gemini image response did not include inline image "
                        "data"));
                json_object_object_add(
                    errObj, "type",
                    json_object_new_string("invalid_request_error"));
                json_object_object_add(wrapped, "error", errObj);
                json_object_put(response);
                response = wrapped;
            }
        }
    }

    json_tokener_free(tokener);
    return response;
}

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
                   CONST_STRPTR proxyPassword) {
    struct json_object *response;
    BOOL useSSL = !useProxy || proxyUsesSSL;

    BPTR fileHandle = Open(destination, MODE_NEWFILE);
    if (fileHandle == NULL) {
        displayError(STRING_ERROR_FILE_WRITE_OPEN);
        return RETURN_ERROR;
    }

    UBYTE hostString[64];
    UBYTE pathString[2056];

    // Parse URL (very basic parsing, assuming URL starts with http:// or
    // https://)
    CONST_STRPTR urlStart = strstr(url, "://");
    if (urlStart == NULL) {
        displayError(STRING_ERROR_INVALID_URL);
        FreeVec(writeBuffer);
        Close(fileHandle);
        return RETURN_ERROR;
    }
    urlStart += 3; // Skip past "://"

    // Find the first slash after http:// or https:// to separate host and path
    CONST_STRPTR pathStart = strchr(urlStart, '/');
    if (pathStart == NULL) {
        // URL doesn't have a path, use '/' as default
        strcpy(hostString, urlStart);
        strcpy(pathString, "/");
    } else {
        // Copy host and path into separate strings
        ULONG hostLength = pathStart - urlStart;
        strncpy(hostString, urlStart, hostLength);
        hostString[hostLength] = '\0'; // Null-terminate the host string
        strcpy(pathString, pathStart); // The rest is the path
    }

    STRPTR authHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        // Construct the Proxy-Authorization header
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
    }

    // Construct the HTTP GET request
    snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: AmigaGPT\r\n"
             "%s\r\n\0",
             pathString, hostString, authHeader);

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(hostString, 443, useSSL, useProxy, proxyHost,
                               proxyPort, proxyUsesSSL, proxyRequiresAuth,
                               proxyUsername, proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            Close(fileHandle);
            return RETURN_ERROR;
        }
    }
    connectionRetryCount = 0;

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (download)");
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    if (ssl_err > 0) {
        ULONG totalBytesRead = 0;
        WORD bytesRead = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        UBYTE statusMessage[64];
        LONG contentLength = 0;
        UBYTE *dataStart = NULL;
        BOOL headersRead = FALSE;
        UBYTE *tempReadBuffer =
            AllocVec(useProxy ? 8192 : TEMP_READ_BUFFER_LENGTH, MEMF_CLEAR);

        while (!doneReading) {
#ifndef DAEMON
            DoMethod(loadingBar, MUIM_Busy_Move);
#endif
            if (useSSL) {
                ERR_clear_error();
                bytesRead =
                    SSL_read(ssl, tempReadBuffer,
                             useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1);
            } else {
                bytesRead =
                    recv(sock, tempReadBuffer,
                         useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1, 0);
            }
            if (!headersRead) {
                STRPTR httpResponse = strstr(tempReadBuffer, "HTTP/1.1");
                if (httpResponse != NULL &&
                    strstr(httpResponse, "200 OK") == NULL) {
                    UBYTE error[256] = {0};
                    for (UWORD i = 0; i < 256; i++) {
                        if (httpResponse[i] == '\r' ||
                            httpResponse[i] == '\n') {
                            break;
                        }
                        error[i] = httpResponse[i];
                    }
                    displayError(error);
                    doneReading = TRUE;
                    break;
                }
                if (contentLength == 0) {
                    UBYTE *contentLengthStart =
                        strstr(tempReadBuffer, "Content-Length: ");
                    if (contentLengthStart != NULL) {
                        contentLengthStart += 16;
                        UBYTE *contentLengthEnd =
                            strstr(contentLengthStart, "\r\n");
                        if (contentLengthEnd != NULL) {
                            contentLength = atoi(contentLengthStart);
                        }
                    }
                }
                dataStart = strstr(tempReadBuffer, "\r\n\r\n");
                if (dataStart != NULL) {
                    headersRead = TRUE;
                    dataStart += 4;
                    bytesRead -= (dataStart - tempReadBuffer);
                } else {
                    continue;
                }
            } else {
                dataStart = tempReadBuffer;
            }
            snprintf(statusMessage, sizeof(statusMessage), "%s %lu/%ld %s",
                     STRING_DOWNLOADED, totalBytesRead, contentLength,
                     STRING_BYTES);
            updateStatusBar(statusMessage, yellowPen);
            if (useSSL) {
                err = SSL_get_error(ssl, bytesRead);
            } else {
                err = SSL_ERROR_NONE;
            }
            switch (err) {
            case SSL_ERROR_NONE:
                Write(fileHandle, dataStart, bytesRead);
                totalBytesRead += bytesRead;
                if (totalBytesRead >= contentLength) {
                    doneReading = TRUE;
                }
                break;
            case SSL_ERROR_ZERO_RETURN:
                printf("SSL_ERROR_ZERO_RETURN\n");
                doneReading = TRUE;
                break;
            case SSL_ERROR_WANT_READ:
                printf("SSL_ERROR_WANT_READ\n");
                break;
            case SSL_ERROR_WANT_WRITE:
                printf("SSL_ERROR_WANT_WRITE\n");
                break;
            case SSL_ERROR_WANT_CONNECT:
                printf("SSL_ERROR_WANT_CONNECT\n");
                break;
            case SSL_ERROR_WANT_ACCEPT:
                printf("SSL_ERROR_WANT_ACCEPT\n");
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                printf("SSL_ERROR_WANT_X509_LOOKUP\n");
                break;
            case SSL_ERROR_SSL:
                reportSslError(ssl, bytesRead, "SSL_read (download)");
                /* Attempt reconnect & range resume follows as before */
                updateStatusBar(STRING_ERROR_LOST_CONNECTION, redPen);
                if (createSSLConnection(hostString, 443, useSSL, useProxy,
                                        proxyHost, proxyPort, proxyUsesSSL,
                                        proxyRequiresAuth, proxyUsername,
                                        proxyPassword) == RETURN_ERROR) {
                    if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
                        Close(fileHandle);
                        FreeVec(tempReadBuffer);
                        CloseSocket(sock);
                        SSL_shutdown(ssl);
                        SSL_free(ssl);
                        ssl = NULL;
                        sock = -1;
                        FreeVec(authHeader);
                        return RETURN_ERROR;
                    }
                }
                headersRead = FALSE;
                dataStart = NULL;
                snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                         "GET %s HTTP/1.1\r\n"
                         "Host: %s\r\n"
                         "User-Agent: AmigaGPT\r\n"
                         "Range: bytes=%lu-\r\n"
                         "%s\r\n\0",
                         pathString, hostString, totalBytesRead, authHeader);
                SSL_write(ssl, writeBuffer, strlen(writeBuffer));
                break;
            case SSL_ERROR_SYSCALL:
                reportSslError(ssl, bytesRead, "SSL_read (download)");
                doneReading = TRUE;
                break;
            }
        }
        FreeVec(tempReadBuffer);
    } else {
        displayError(STRING_ERROR_REQUEST_WRITE);
        reportSslError(ssl, ssl_err, "SSL_write (download)");
        Close(fileHandle);
        FreeVec(authHeader);
        return RETURN_ERROR;
    }

    FreeVec(authHeader);

    CloseSocket(sock);
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    sock = -1;

    Close(fileHandle);
    return RETURN_OK;
}

/**
 * Create a new SSL context
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static LONG createSSLContext() {
    /* Basic intialization. Next few steps (up to SSL_new()) need
     * to be done only once per AmiSSL opener.
     */
    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS |
                         OPENSSL_INIT_ADD_ALL_DIGESTS,
                     NULL);

    /* Seed the entropy engine */
    generateRandomSeed(writeBuffer, 128);
    RAND_seed(writeBuffer, 128);

    /* Note: BIO writing routines are prepared for NULL BIO handle */
    if ((bio_err = BIO_new(BIO_s_file())) != NULL)
#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
        BIO_set_fp_amiga(bio_err, GetStdErr(), BIO_NOCLOSE | BIO_FP_TEXT);
#else
        BIO_set_fp(bio_err, GetStdErr(), BIO_NOCLOSE);
#endif

    /* Get a new SSL context */
    if ((ctx = SSL_CTX_new(TLS_client_method())) != NULL) {
        /* Basic certificate handling */
        SSL_CTX_set_default_verify_paths(ctx);

        SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);

        // Disable certificate verification. This is done because proxy servers
        // often use self-signed certificates
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
        // SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER |
        // SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_cb);
    } else {
        displayError(STRING_ERROR_SSL_CONTEXT);
        return RETURN_ERROR;
    }

    return RETURN_OK;
}

/**
 * Get some suitable random seed data
 * @param buffer the buffer to fill with random data
 * @param size the size of the buffer
 **/
static void generateRandomSeed(UBYTE *buffer, LONG size) {
    for (int i = 0; i < size / 2; i++) {
        ((UWORD *)buffer)[i] = rangeRand(65535);
    }
}

/**
 * This callback is called everytime OpenSSL verifies a certificate
 * in the chain during a connection, indicating success or failure.
 * @param preverify_ok 1 if the certificate passed verification, 0 otherwise
 * @param ctx the X509 certificate store context
 * @return 1 if the certificate passed verification, 0 otherwise
 **/
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx) {
    if (!preverify_ok) {
        /* Here, you could ask the user whether to ignore the failure,
         * displaying information from the certificate, for example.
         */
        // printf("Certificate verification failed (%s)\n",
        //         X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)));
    } else {
        // printf("Certificate verification successful (hash %08lx)\n",
        //         X509_issuer_and_serial_hash(X509_STORE_CTX_get_current_cert(ctx)));
    }
    return preverify_ok;
}

/**
 * Helper function to parse the chunk length
 * @param buffer the buffer to parse
 * @param bufferLength the length of the buffer
 * @return the chunk length
 * @see
 *https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
 **/
static ULONG parseChunkLength(UBYTE *buffer, ULONG bufferLength) {
    UBYTE chunkLenStr[10] = {0}; // Enough for the chunk length in hex
    UBYTE i;

    // Loop until we find the CRLF which ends the chunk length line
    for (i = 0; i < 8 && i < bufferLength && buffer[i] != '\r' &&
                buffer[i + 1] != '\n';
         i++) {
        chunkLenStr[i] = buffer[i];
    }

    if (i == 8) {
        printf(STRING_ERROR_BAD_CHUNK);
        printf("\n%x %x %x %x %x %x %x %x\n", buffer[0], buffer[1], buffer[2],
               buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
        return 0;
    }

    // Convert hex string to unsigned long
    return strtoul(chunkLenStr, NULL, 16);
}

/**
 * Drain and print the entire OpenSSL error queue with context.
 * Uses ERR_error_string_n/ERR_lib_error_string/ERR_reason_error_string when
 * available.
 * @param where the source of the error
 */
static void drainOpenSslErrorQueue(CONST_STRPTR where) {
    unsigned long err;
    if (ERR_peek_error() == 0) {
        return;
    }
    UBYTE line[512];
    while ((err = ERR_get_error()) != 0) {
        char text[256];
        const char *lib = ERR_lib_error_string(err);
        const char *reason = ERR_reason_error_string(err);
        ERR_error_string_n(err, text, sizeof text);
        snprintf(line, sizeof line, "%s: [0x%lx] lib=%s reason=%s :: %s",
                 where ? where : "OpenSSL", err, lib ? lib : "?",
                 reason ? reason : "?", text);
        displayError(line);
    }
}

/**
 * Helper function to report SSL errors
 * Retryable errors print to stdout, fatal errors call
 *displayError/drain_openssl_error_queue.
 * @param s the SSL connection
 * @param ret the return value from the SSL function
 * @param where the source of the error
 **/
static void reportSslError(SSL *s, int ret, CONST_STRPTR where) {
    int why = SSL_get_error(s, ret);
    switch (why) {
    case SSL_ERROR_NONE:
        return;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_CONNECT:
    case SSL_ERROR_WANT_ACCEPT:
    case SSL_ERROR_WANT_X509_LOOKUP:
        // Retryable conditions: log to console only
        printf("%s: %s\n", where,
               (why == SSL_ERROR_WANT_READ)      ? "SSL_ERROR_WANT_READ"
               : (why == SSL_ERROR_WANT_WRITE)   ? "SSL_ERROR_WANT_WRITE"
               : (why == SSL_ERROR_WANT_CONNECT) ? "SSL_ERROR_WANT_CONNECT"
               : (why == SSL_ERROR_WANT_ACCEPT)  ? "SSL_ERROR_WANT_ACCEPT"
                                                : "SSL_ERROR_WANT_X509_LOOKUP");
        break;
    case SSL_ERROR_ZERO_RETURN:
        displayError("SSL: ZERO_RETURN (connection closed)");
        break;
    case SSL_ERROR_SYSCALL:
        if (ERR_peek_error() == 0) {
            displayError(strerror(errno));
        } else {
            drainOpenSslErrorQueue(where);
        }
        break;
    case SSL_ERROR_SSL:
    default:
        drainOpenSslErrorQueue(where);
        break;
    }
}

/**
 * Post a text to speech request to OpenAI
 * @param text the text to speak
 * @param openAITTSModel the TTS model to use
 * @param openAITTSVoice the voice to use
 * @param voiceInstructions the voice instructions to use
 * @param apiKey the OpenAI API key
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
    CONST_STRPTR text, OpenAITTSModel openAITTSModel,
    OpenAITTSVoice openAITTSVoice, CONST_STRPTR voiceInstructions,
    CONST_STRPTR apiKey, ULONG *audioLength, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword, AudioFormat *audioFormat) {
    struct json_object *response;
    BOOL useSSL = !useProxy || proxyUsesSSL;

    *audioLength = 0;

    /* Prevent crashes on NULL apiKey (also avoids a guaranteed 401). */
    if (apiKey == NULL || strlen(apiKey) == 0) {
        displayError(STRING_ERROR_NO_API_KEY);
        return NULL;
    }

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useSSL, useProxy,
                               proxyHost, proxyPort, proxyUsesSSL,
                               proxyRequiresAuth, proxyUsername,
                               proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            return NULL;
        }
    }
    connectionRetryCount = 0;

    // Allocate a buffer for the audio data. This buffer will be resized if
    // needed
    ULONG audioBufferSize = AUDIO_BUFFER_SIZE;
    UBYTE *audioData = AllocVec(audioBufferSize, MEMF_ANY);

    struct json_object *obj = json_object_new_object();
    json_object_object_add(
        obj, "model",
        json_object_new_string(OPENAI_TTS_MODEL_NAMES[openAITTSModel]));
    json_object_object_add(
        obj, "voice",
        json_object_new_string(OPENAI_TTS_VOICE_NAMES[openAITTSVoice]));
    json_object_object_add(obj, "input", json_object_new_string(text));
    if (voiceInstructions != NULL) {
        json_object_object_add(obj, "instructions",
                               json_object_new_string(voiceInstructions));
    }
    json_object_object_add(
        obj, "response_format",
        json_object_new_string(
            AUDIO_FORMAT_NAMES[audioFormat != NULL ? *audioFormat
                                                   : AUDIO_FORMAT_PCM]));
    CONST_STRPTR jsonString = json_object_to_json_string(obj);

    STRPTR authHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        // Construct the Proxy-Authorization header
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
    }

    snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
             "POST %s/v1/audio/speech HTTP/1.1\r\n"
             "Host: api.openai.com\r\n"
             "Content-Type: application/json\r\n"
             "Authorization: Bearer %s\r\n"
             "User-Agent: AmigaGPT\r\n"
             "Content-Length: %lu\r\n"
             "%s\r\n"
             "%s\0",
             useSSL ? "" : "https://api.openai.com", apiKey, strlen(jsonString),
             authHeader, jsonString);

    json_object_put(obj);

    FreeVec(authHeader);

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (tts)");
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    BOOL isErrorResponse = FALSE;

    if (ssl_err > 0) {
        LONG bytesRead = 0;
        ULONG bytesRemainingInBuffer = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        UBYTE statusMessage[64];
        UBYTE tempChunkHeaderBuffer[10] = {0};
        UBYTE tempChunkDataBufferLength = 0;
        BOOL hasReadHeader = FALSE;
        BOOL newChunkNeeded = TRUE;
        ULONG chunkLength = 0;
        ULONG chunkBytesNeedingRead = 0;
        UBYTE *dataStart = NULL;

        while (!doneReading) {
#ifndef DAEMON
            DoMethod(loadingBar, MUIM_Busy_Move);
#endif
            memset(readBuffer, 0, READ_BUFFER_LENGTH);
            if (useSSL) {
                ERR_clear_error();
                bytesRead =
                    SSL_read(ssl, readBuffer,
                             useProxy ? 8192 - 1 : READ_BUFFER_LENGTH - 1);
            } else {
                bytesRead =
                    recv(sock, readBuffer,
                         useProxy ? 8192 - 1 : READ_BUFFER_LENGTH - 1, 0);
            }
            if (newChunkNeeded && bytesRead == 1)
                continue;
            bytesRemainingInBuffer = bytesRead;
            dataStart = readBuffer;

            snprintf(statusMessage, sizeof(statusMessage), "%s %lu %s",
                     STRING_DOWNLOADED, *audioLength, STRING_BYTES);
            updateStatusBar(statusMessage, yellowPen);
            if (useSSL) {
                err = SSL_get_error(ssl, bytesRead);
            } else {
                err = SSL_ERROR_NONE;
            }
            switch (err) {
            case SSL_ERROR_NONE:
                while (bytesRemainingInBuffer > 0) {
                    if (!hasReadHeader) {
                        dataStart = strstr(readBuffer, "\r\n\r\n");
                        if (dataStart != NULL) {
                            hasReadHeader = TRUE;
                            dataStart += 4;
                            newChunkNeeded = TRUE;
                            bytesRemainingInBuffer -= (dataStart - readBuffer);
                            if (dataStart[0] == '{') {
                                CONST_STRPTR jsonString =
                                    strstr(dataStart, "{");
                                response = json_tokener_parse(jsonString);
                                struct json_object *error;
                                if (response != NULL &&
                                    json_object_object_get_ex(response, "error",
                                                              &error)) {
                                    struct json_object *message =
                                        json_object_object_get(error,
                                                               "message");
                                    STRPTR rawMessageString =
                                        json_object_get_string(message);
                                    STRPTR cleanMessage =
                                        extractUserFriendlyErrorMessage(
                                            rawMessageString);
                                    displayError(cleanMessage);
                                    if (cleanMessage != rawMessageString) {
                                        FreeVec(cleanMessage);
                                    }
                                    FreeVec(audioData);
                                    return NULL;
                                }
                            } else {
                                STRPTR httpResponse =
                                    strstr(readBuffer, "HTTP/1.1");
                                if (httpResponse != NULL &&
                                    strstr(httpResponse, "200 OK") == NULL) {
                                    // Mark this as an error response but
                                    // continue downloading to get the complete
                                    // JSON error message
                                    isErrorResponse = TRUE;
                                }
                            }
                        }
                    }

                    if (newChunkNeeded) {
                        tempChunkDataBufferLength = 0;
                        memset(tempChunkHeaderBuffer, 0, 10);
                        while (!strstr(tempChunkHeaderBuffer, "\r\n") &&
                               tempChunkDataBufferLength < 10) {
                            if (bytesRemainingInBuffer > 0) {
                                memcpy(tempChunkHeaderBuffer +
                                           tempChunkDataBufferLength,
                                       dataStart, 1);
                                dataStart++;
                                bytesRemainingInBuffer--;
                                tempChunkDataBufferLength++;
                            } else {
                                UBYTE singleByte[1];
                                if (useSSL) {
                                    bytesRead = SSL_read(ssl, singleByte, 1);
                                } else {
                                    bytesRead = recv(sock, singleByte, 1, 0);
                                }
                                if (bytesRead <= 0) {
                                    // Set error flag to trigger reconnection
                                    // logic
                                    err = SSL_ERROR_SYSCALL;
                                    doneReading = TRUE;
                                    break;
                                }
                                memcpy(tempChunkHeaderBuffer +
                                           tempChunkDataBufferLength,
                                       singleByte, bytesRead);
                                tempChunkDataBufferLength += bytesRead;
                            }
                        }

                        chunkLength = parseChunkLength(
                            tempChunkHeaderBuffer, tempChunkDataBufferLength);
                        if (chunkLength == 0) {
                            doneReading = TRUE;
                            break;
                        }
                        chunkBytesNeedingRead = chunkLength;
                        if (bytesRemainingInBuffer == 0) {
                            newChunkNeeded = FALSE;
                            continue;
                        }
                    }

                    // Create a larger audio buffer if needed
                    if (*audioLength + chunkBytesNeedingRead >
                        audioBufferSize) {
                        audioBufferSize <<= 1;
                        APTR oldAudioData = audioData;
                        audioData = AllocVec(audioBufferSize, MEMF_ANY);
                        if (audioData == NULL) {
                            FreeVec(oldAudioData);
                            displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
                            return NULL;
                        }
                        memcpy(audioData, oldAudioData, *audioLength);
                        FreeVec(oldAudioData);
                    }

                    if (chunkBytesNeedingRead > bytesRemainingInBuffer) {
                        memcpy(audioData + *audioLength, dataStart,
                               bytesRemainingInBuffer);
                        *audioLength += bytesRemainingInBuffer;
                        chunkBytesNeedingRead -= bytesRemainingInBuffer;
                        newChunkNeeded = FALSE;
                        bytesRemainingInBuffer = 0;
                    } else {
                        memcpy(audioData + *audioLength, dataStart,
                               chunkBytesNeedingRead);
                        *audioLength += chunkBytesNeedingRead;
                        bytesRemainingInBuffer -= chunkBytesNeedingRead;
                        while (bytesRemainingInBuffer < 2) {
                            if (useSSL) {
                                bytesRead = SSL_read(ssl, readBuffer, 1);
                            } else {
                                bytesRead = recv(sock, readBuffer, 1, 0);
                            }
                            if (bytesRead <= 0) {
                                // Connection closed or error during trailer
                                // read

                                // For TTS downloads, we must restart completely
                                // as streams are not resumable But first, let's
                                // check if we're at the end and can complete
                                // with current data
                                if (chunkBytesNeedingRead == 0) {
                                    newChunkNeeded = TRUE;
                                    break; // Continue to next chunk
                                } else {
                                    err = SSL_ERROR_SYSCALL;
                                    doneReading = TRUE;
                                    break;
                                }
                            }
                            bytesRemainingInBuffer += bytesRead;
                        }
                        if (doneReading)
                            break;
                        dataStart += chunkBytesNeedingRead + 2;
                        bytesRemainingInBuffer -= 2;
                        chunkBytesNeedingRead = 0;
                        newChunkNeeded = TRUE;
                    }
                }
                break;
            case SSL_ERROR_ZERO_RETURN:
                // Process any remaining data in buffer before exiting
                if (bytesRemainingInBuffer > 0 && chunkBytesNeedingRead > 0) {
                    // Process remaining buffer data using the same logic as the
                    // main loop
                    while (bytesRemainingInBuffer > 0 &&
                           chunkBytesNeedingRead > 0) {
                        // Create a larger audio buffer if needed
                        if (*audioLength + chunkBytesNeedingRead >
                            audioBufferSize) {
                            audioBufferSize <<= 1;
                            APTR oldAudioData = audioData;
                            audioData = AllocVec(audioBufferSize, MEMF_ANY);
                            if (audioData == NULL) {
                                FreeVec(oldAudioData);
                                displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
                                return NULL;
                            }
                            memcpy(audioData, oldAudioData, *audioLength);
                            FreeVec(oldAudioData);
                        }

                        if (chunkBytesNeedingRead > bytesRemainingInBuffer) {
                            memcpy(audioData + *audioLength, dataStart,
                                   bytesRemainingInBuffer);
                            *audioLength += bytesRemainingInBuffer;
                            chunkBytesNeedingRead -= bytesRemainingInBuffer;
                            bytesRemainingInBuffer = 0;
                        } else {
                            memcpy(audioData + *audioLength, dataStart,
                                   chunkBytesNeedingRead);
                            *audioLength += chunkBytesNeedingRead;
                            bytesRemainingInBuffer -= chunkBytesNeedingRead;
                            chunkBytesNeedingRead = 0;
                            newChunkNeeded = TRUE;
                        }
                    }
                }

                // Now we can safely exit
                doneReading = TRUE;
                break;
            case SSL_ERROR_WANT_READ:
                printf("SSL_ERROR_WANT_READ\n");
                break;
            case SSL_ERROR_WANT_WRITE:
                printf("SSL_ERROR_WANT_WRITE\n");
                break;
            case SSL_ERROR_WANT_CONNECT:
                printf("SSL_ERROR_WANT_CONNECT\n");
                break;
            case SSL_ERROR_WANT_ACCEPT:
                printf("SSL_ERROR_WANT_ACCEPT\n");
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                printf("SSL_ERROR_WANT_X509_LOOKUP\n");
                break;
            case SSL_ERROR_SYSCALL:
            case SSL_ERROR_SSL: {
                reportSslError(ssl, bytesRead, "SSL_read (tts)");
                updateStatusBar(STRING_ERROR_LOST_CONNECTION, redPen);
                if (createSSLConnection(
                        OPENAI_HOST, OPENAI_PORT, useSSL, useProxy, proxyHost,
                        proxyPort, proxyUsesSSL, proxyRequiresAuth,
                        proxyUsername, proxyPassword) == RETURN_ERROR) {
                    if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
                        displayError(STRING_ERROR_CONNECTION_MAX_RETRIES);
                        FreeVec(audioData);
                        return NULL;
                    }
                } else {
                    // Successful reconnection - restart the download
                    *audioLength = 0; // Reset audio length
                    hasReadHeader = FALSE;
                    newChunkNeeded = TRUE;
                    chunkBytesNeedingRead = 0;
                    bytesRemainingInBuffer = 0;
                    // Re-send the HTTP request
                    if (useSSL) {
                        ERR_clear_error();
                        ssl_err =
                            SSL_write(ssl, writeBuffer, strlen(writeBuffer));
                        if (ssl_err <= 0) {
                            reportSslError(ssl, ssl_err,
                                           "SSL_write (tts retry)");
                            continue;
                        }
                    } else {
                        ssl_err =
                            send(sock, writeBuffer, strlen(writeBuffer), 0);
                        if (ssl_err <= 0) {
                            continue;
                        }
                    }
                }
                break;
            }
            default:
                printf(STRING_ERROR_UNKNOWN);
                putchar('\n');
                break;
            }
        }

        // CRITICAL: Process any remaining buffer data before exiting main loop
        if (bytesRemainingInBuffer > 0 && chunkBytesNeedingRead > 0) {
            // Create a larger audio buffer if needed
            if (*audioLength + chunkBytesNeedingRead > audioBufferSize) {
                audioBufferSize <<= 1;
                APTR oldAudioData = audioData;
                audioData = AllocVec(audioBufferSize, MEMF_ANY);
                if (audioData == NULL) {
                    FreeVec(oldAudioData);
                    displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
                    return NULL;
                }
                memcpy(audioData, oldAudioData, *audioLength);
                FreeVec(oldAudioData);
            }

            // Process the remaining data
            ULONG bytesToSave = (chunkBytesNeedingRead < bytesRemainingInBuffer)
                                    ? chunkBytesNeedingRead
                                    : bytesRemainingInBuffer;
            memcpy(audioData + *audioLength, dataStart, bytesToSave);
            *audioLength += bytesToSave;
        }
    } else {
        displayError(STRING_ERROR_REQUEST_WRITE);
        reportSslError(ssl, ssl_err, "SSL_write");
        FreeVec(audioData);
        return NULL;
    }

    CloseSocket(sock);
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    sock = -1;

    // Check if this was an error response and parse JSON error message
    if (isErrorResponse && *audioLength > 0) {
        // Try to parse the downloaded data as JSON error response
        STRPTR jsonStart = strstr((STRPTR)audioData, "{");
        if (jsonStart != NULL) {
            struct json_object *errorResponse = json_tokener_parse(jsonStart);
            if (errorResponse != NULL) {
                struct json_object *error;
                if (json_object_object_get_ex(errorResponse, "error", &error)) {
                    struct json_object *message =
                        json_object_object_get(error, "message");
                    if (message != NULL) {
                        STRPTR rawMessageString =
                            json_object_get_string(message);
                        STRPTR cleanMessage =
                            extractUserFriendlyErrorMessage(rawMessageString);
                        displayError(cleanMessage);
                        if (cleanMessage != rawMessageString) {
                            FreeVec(cleanMessage); // Free the allocated clean
                                                   // message
                        }
                        json_object_put(errorResponse);
                        FreeVec(audioData);
                        return NULL;
                    }
                }
                json_object_put(errorResponse);
            }
        }
        // If we couldn't parse the JSON error, show a generic error
        displayError("HTTP error occurred but could not parse error details");
        FreeVec(audioData);
        return NULL;
    }

    updateStatusBar(STRING_DOWNLOAD_COMPLETE, greenPen);

    return audioData;
}

/**
 * Cleanup the OpenAI connector and free all resources
 **/
void closeOpenAIConnector() {
    if (amiSSLInitialized) {
        if (ctx) {
            SSL_CTX_free(ctx);
            ctx = NULL;
        }
        BIO_free(bio_err);
#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
        CleanupAmiSSLA(NULL);
#endif
    }

#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
    if (AmiSSLBase) {
#ifdef __AMIGAOS4__
        DropInterface((struct Interface *)IAmiSSL);
#endif
        CloseAmiSSL();
        AmiSSLBase = NULL;
    }

    if (AmiSSLMasterBase) {
#ifdef __AMIGAOS4__
        DropInterface((struct Interface *)IAmiSSLMaster);
#endif
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
    }

    if (SocketBase) {
#ifdef __AMIGAOS4__
        DropInterface((struct Interface *)ISocket);
#endif
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
#endif

    sock = -1;

    FreeVec(writeBuffer);
    FreeVec(readBuffer);
}

/**
 * Encode a string to base64
 * @param input the string to encode
 * @return a pointer to a buffer containing the base64 encoded string -- Free it
 * with FreeVec() when you are done using it
 */
static STRPTR base64Encode(CONST_STRPTR input) {
    const UBYTE base64Table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    ULONG len = strlen(input);
    UBYTE *encoded = AllocVec((len + 2) / 3 * 4 + 1, MEMF_CLEAR | MEMF_ANY);
    UBYTE *p = encoded;
    ULONG i;
    for (i = 0; i < len - 2; i += 3) {
        *p++ = base64Table[(input[i] >> 2) & 0x3F];
        *p++ = base64Table[((input[i] & 0x03) << 4) |
                           ((input[i + 1] >> 4) & 0x0F)];
        *p++ = base64Table[((input[i + 1] & 0x0F) << 2) |
                           ((input[i + 2] >> 6) & 0x03)];
        *p++ = base64Table[input[i + 2] & 0x3F];
    }
    if (i < len) {
        *p++ = base64Table[(input[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = base64Table[(input[i] & 0x03) << 4];
            *p++ = '=';
        } else {
            *p++ = base64Table[((input[i] & 0x03) << 4) |
                               ((input[i + 1] >> 4) & 0x0F)];
            *p++ = base64Table[(input[i + 1] & 0x0F) << 2];
        }
        *p++ = '=';
    }
    return encoded;
}

/**
 * Extract user-friendly error message from OpenAI validation error
 * @param rawMessage The raw error message from OpenAI API
 * @return A user-friendly error message, or the original if parsing fails
 */
static STRPTR extractUserFriendlyErrorMessage(CONST_STRPTR rawMessage) {
    if (rawMessage == NULL) {
        return "Unknown error occurred";
    }

    // Look for the 'msg': pattern in validation errors
    STRPTR msgStart = strstr(rawMessage, "'msg': \"");
    if (msgStart != NULL) {
        msgStart += 8; // Skip past "'msg': \""
        STRPTR msgEnd = strstr(msgStart, "\"");
        if (msgEnd != NULL) {
            // Calculate length and create a copy
            ULONG msgLength = msgEnd - msgStart;
            STRPTR cleanMessage = AllocVec(msgLength + 1, MEMF_ANY);
            if (cleanMessage != NULL) {
                strncpy(cleanMessage, msgStart, msgLength);
                cleanMessage[msgLength] = '\0';
                return cleanMessage;
            }
        }
    }

    // Fallback: return a copy of the original message
    ULONG originalLength = strlen(rawMessage);
    STRPTR messageCopy = AllocVec(originalLength + 1, MEMF_ANY);
    if (messageCopy != NULL) {
        strcpy(messageCopy, rawMessage);
    }
    return messageCopy;
}

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
    CONST_STRPTR apiKey, ULONG *audioLength, BOOL useProxy,
    CONST_STRPTR proxyHost, UWORD proxyPort, BOOL proxyUsesSSL,
    BOOL proxyRequiresAuth, CONST_STRPTR proxyUsername,
    CONST_STRPTR proxyPassword) {

#define ELEVENLABS_HOST "api.elevenlabs.io"
#define ELEVENLABS_PORT 443

    BOOL useSSL = !useProxy || proxyUsesSSL;
    *audioLength = 0;

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(ELEVENLABS_HOST, ELEVENLABS_PORT, useSSL,
                               useProxy, proxyHost, proxyPort, proxyUsesSSL,
                               proxyRequiresAuth, proxyUsername,
                               proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            return NULL;
        }
    }

    /* Allocate buffer for audio data */
    ULONG audioBufferSize = AUDIO_BUFFER_SIZE;
    UBYTE *audioData = AllocVec(audioBufferSize, MEMF_ANY);
    if (audioData == NULL) {
        displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
        return NULL;
    }

    /* Build JSON request body */
    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "text", json_object_new_string(text));
    if (modelId != NULL && strlen(modelId) > 0) {
        json_object_object_add(obj, "model_id",
                               json_object_new_string(modelId));
    }
    CONST_STRPTR jsonString = json_object_to_json_string(obj);

    /* Build proxy auth header if needed */
    STRPTR authHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
        FreeVec(credentials);
    }

    /* Build the endpoint with voice ID and output format */
    UBYTE endpoint[256];
    snprintf(endpoint, sizeof(endpoint),
             "/v1/text-to-speech/%s?output_format=pcm_24000",
             voiceId != NULL ? voiceId : "");

    snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
             "POST %s%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "xi-api-key: %s\r\n"
             "User-Agent: AmigaGPT\r\n"
             "Content-Length: %lu\r\n"
             "Connection: close\r\n"
             "%s\r\n"
             "%s",
             useSSL ? "" : "https://api.elevenlabs.io", endpoint,
             ELEVENLABS_HOST, apiKey ? apiKey : "", strlen(jsonString),
             authHeader, jsonString);

    json_object_put(obj);
    FreeVec(authHeader);

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (elevenlabs tts)");
            FreeVec(audioData);
            return NULL;
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
        if (ssl_err <= 0) {
            displayError(STRING_ERROR_REQUEST_WRITE);
            FreeVec(audioData);
            return NULL;
        }
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    /* Read response */
    LONG bytesRead = 0;
    BOOL hasReadHeader = FALSE;
    UBYTE *dataStart = NULL;
    UBYTE statusMessage[64];

    while (1) {
#ifndef DAEMON
        DoMethod(loadingBar, MUIM_Busy_Move);
#endif
        memset(readBuffer, 0, READ_BUFFER_LENGTH);
        if (useSSL) {
            ERR_clear_error();
            bytesRead = SSL_read(ssl, readBuffer, READ_BUFFER_LENGTH - 1);
        } else {
            bytesRead = recv(sock, readBuffer, READ_BUFFER_LENGTH - 1, 0);
        }

        if (bytesRead <= 0) {
            break;
        }

        dataStart = readBuffer;
        ULONG bytesToCopy = bytesRead;

        if (!hasReadHeader) {
            /* Look for end of HTTP headers */
            dataStart = strstr(readBuffer, "\r\n\r\n");
            if (dataStart != NULL) {
                hasReadHeader = TRUE;
                dataStart += 4;
                bytesToCopy = bytesRead - (dataStart - readBuffer);

                /* Check for error response */
                if (strstr(readBuffer, "HTTP/1.1 200") == NULL) {
                    /* Try to parse error JSON */
                    if (dataStart[0] == '{') {
                        struct json_object *response =
                            json_tokener_parse(dataStart);
                        if (response != NULL) {
                            struct json_object *detail;
                            if (json_object_object_get_ex(response, "detail",
                                                          &detail)) {
                                struct json_object *message =
                                    json_object_object_get(detail, "message");
                                if (message != NULL) {
                                    displayError(
                                        json_object_get_string(message));
                                } else {
                                    displayError(
                                        json_object_get_string(detail));
                                }
                            }
                            json_object_put(response);
                        }
                    }
                    FreeVec(audioData);
                    return NULL;
                }
            } else {
                continue;
            }
        }

        /* Expand buffer if needed */
        if (*audioLength + bytesToCopy > audioBufferSize) {
            audioBufferSize <<= 1;
            APTR oldAudioData = audioData;
            audioData = AllocVec(audioBufferSize, MEMF_ANY);
            if (audioData == NULL) {
                FreeVec(oldAudioData);
                displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
                return NULL;
            }
            memcpy(audioData, oldAudioData, *audioLength);
            FreeVec(oldAudioData);
        }

        memcpy(audioData + *audioLength, dataStart, bytesToCopy);
        *audioLength += bytesToCopy;

        snprintf(statusMessage, sizeof(statusMessage), "%s %lu %s",
                 STRING_DOWNLOADED, *audioLength, STRING_BYTES);
        updateStatusBar(statusMessage, yellowPen);
    }

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif

    updateStatusBar(STRING_READY, greenPen);

    if (*audioLength == 0) {
        FreeVec(audioData);
        return NULL;
    }

    return audioData;
}

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
makeHttpsGetRequest(CONST_STRPTR host, UWORD port, CONST_STRPTR endpoint,
                    CONST_STRPTR apiKey, CONST_STRPTR apiKeyHeader,
                    BOOL useBearer, BOOL useProxy, CONST_STRPTR proxyHost,
                    UWORD proxyPort, BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
                    CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword) {

    UBYTE connectionRetryCount = 0;
    BOOL useSSL = TRUE;

    if (port == 0) {
        port = 443;
    }
    if (useProxy && proxyUsesSSL) {
        useSSL = TRUE;
    }

    memset(readBuffer, 0, READ_BUFFER_LENGTH);
    memset(writeBuffer, 0, WRITE_BUFFER_LENGTH);

    /* Build HTTP GET request */
    STRPTR authHeader = AllocVec(512, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(authHeader, 512, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
        FreeVec(credentials);
    }

    /* Build the API key header */
    UBYTE apiKeyHeaderStr[512];
    if (useBearer) {
        snprintf(apiKeyHeaderStr, sizeof(apiKeyHeaderStr), "%s: Bearer %s\r\n",
                 apiKeyHeader, apiKey ? apiKey : "");
    } else {
        snprintf(apiKeyHeaderStr, sizeof(apiKeyHeaderStr), "%s: %s\r\n",
                 apiKeyHeader, apiKey ? apiKey : "");
    }

    if (useSSL || useProxy) {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "GET https://%s:%d%s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "Accept: application/json\r\n"
                 "Connection: close\r\n"
                 "%s\r\n",
                 host, port, endpoint, host, port, apiKeyHeaderStr, authHeader);
    } else {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "Accept: application/json\r\n"
                 "Connection: close\r\n"
                 "%s\r\n",
                 endpoint, host, port, apiKeyHeaderStr, authHeader);
    }

    FreeVec(authHeader);

#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);
#endif

    updateStatusBar(STRING_CONNECTING, yellowPen);
    while (createSSLConnection(host, port, useSSL, useProxy, proxyHost,
                               proxyPort, proxyUsesSSL, proxyRequiresAuth,
                               proxyUsername, proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
#ifndef DAEMON
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif
            return NULL;
        }
    }

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (generic GET)");
#ifndef DAEMON
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif
            return NULL;
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
        if (ssl_err <= 0) {
            displayError(STRING_ERROR_REQUEST_WRITE);
#ifndef DAEMON
            set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif
            return NULL;
        }
    }

    LONG totalRead = 0;
    LONG bytesRead;

    do {
        if (useSSL) {
            ERR_clear_error();
            bytesRead = SSL_read(ssl, readBuffer + totalRead,
                                 READ_BUFFER_LENGTH - totalRead - 1);
        } else {
            bytesRead = recv(sock, readBuffer + totalRead,
                             READ_BUFFER_LENGTH - totalRead - 1, 0);
        }
        if (bytesRead > 0) {
            totalRead += bytesRead;
        }
    } while (bytesRead > 0 && totalRead < READ_BUFFER_LENGTH - 1);

    readBuffer[totalRead] = '\0';

    updateStatusBar(STRING_READY, greenPen);

    /* Find JSON body (after headers) */
    STRPTR jsonStart = strstr(readBuffer, "\r\n\r\n");
    if (jsonStart == NULL) {
        jsonStart = strstr(readBuffer, "\n\n");
        if (jsonStart != NULL)
            jsonStart += 2;
    } else {
        jsonStart += 4;
    }

    if (jsonStart == NULL) {
#ifndef DAEMON
        set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif
        return NULL;
    }

    /* Check for chunked transfer encoding */
    if (strstr(readBuffer, "Transfer-Encoding: chunked") != NULL) {
        /* Skip chunk size line */
        STRPTR chunkData = strchr(jsonStart, '\n');
        if (chunkData != NULL) {
            jsonStart = chunkData + 1;
        }
    }

    /* Parse JSON */
    struct json_object *result = json_tokener_parse(jsonStart);
#ifndef DAEMON
    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);
#endif
    return result;
}

/**
 * Post a tool result (shell command output) back to the OpenAI API
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
 * @return a pointer to a new json_object containing the response or NULL --
 * Free it with json_object_put() when done
 **/
struct json_object *
postToolResultToOpenAI(CONST_STRPTR previousResponseId, CONST_STRPTR callId,
                       CONST_STRPTR output, CONST_STRPTR model, STRPTR host,
                       UWORD port, BOOL useSSL, CONST_STRPTR apiKey,
                       BOOL useProxy, CONST_STRPTR proxyHost, UWORD proxyPort,
                       BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
                       CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword) {

    UBYTE connectionRetryCount = 0;
    if (host == NULL || strlen(host) == 0) {
        displayError("Host not specified");
        return NULL;
    }
    if (port == 0) {
        port = useSSL ? 443 : 80;
    }
    if (useProxy && proxyUsesSSL) {
        useSSL = TRUE;
    }

    /* Build the tool result JSON - do this BEFORE clearing pending tool call
     * since callId and previousResponseId may point to the static buffers */
    struct json_object *obj = json_object_new_object();

    /* Add the model - required by the API */
    if (model == NULL || strlen(model) == 0) {
        json_object_put(obj);
        displayError("Model not specified");
        return NULL;
    }
    json_object_object_add(obj, "model", json_object_new_string(model));

    /* Use the previous response ID to continue the conversation */
    json_object_object_add(obj, "previous_response_id",
                           json_object_new_string(previousResponseId));

    /* Build the input array with the function call output */
    struct json_object *inputArray = json_object_new_array();
    struct json_object *toolResultObj = json_object_new_object();
    json_object_object_add(toolResultObj, "type",
                           json_object_new_string("function_call_output"));
    json_object_object_add(toolResultObj, "call_id",
                           json_object_new_string(callId));
    json_object_object_add(toolResultObj, "output",
                           json_object_new_string(output));
    json_object_array_add(inputArray, toolResultObj);
    json_object_object_add(obj, "input", inputArray);

    /* Add tools array so the model knows it can still use the shell tool */
    struct json_object *toolsArray = json_object_new_array();
    struct json_object *shellToolObj = json_object_new_object();
    json_object_object_add(shellToolObj, "type",
                           json_object_new_string("function"));
    json_object_object_add(shellToolObj, "name",
                           json_object_new_string("shell"));
    json_object_object_add(
        shellToolObj, "description",
        json_object_new_string(
            "Execute AmigaDOS shell commands on the user's Amiga. "
            "Use this to run commands, manage files, or interact "
            "with the system. Returns stdout, stderr, and exit code."));
    struct json_object *paramsObj = json_object_new_object();
    json_object_object_add(paramsObj, "type", json_object_new_string("object"));
    struct json_object *propsObj = json_object_new_object();
    struct json_object *cmdPropObj = json_object_new_object();
    json_object_object_add(cmdPropObj, "type",
                           json_object_new_string("string"));
    json_object_object_add(
        cmdPropObj, "description",
        json_object_new_string(
            "The AmigaDOS command to execute (e.g. 'list RAM:', "
            "'type myfile.txt', 'cd Work:')"));
    json_object_object_add(propsObj, "command", cmdPropObj);
    json_object_object_add(paramsObj, "properties", propsObj);
    struct json_object *requiredArr = json_object_new_array();
    json_object_array_add(requiredArr, json_object_new_string("command"));
    json_object_object_add(paramsObj, "required", requiredArr);
    json_object_object_add(shellToolObj, "parameters", paramsObj);
    json_object_array_add(toolsArray, shellToolObj);
    json_object_object_add(obj, "tools", toolsArray);

    CONST_STRPTR jsonString = json_object_to_json_string(obj);

    /* Now safe to clear pending tool call - JSON has copies of the strings */
    clearPendingToolCall();

    /* Build auth header */
    UBYTE apiAuthHeader[512];
    memset(apiAuthHeader, 0, sizeof(apiAuthHeader));
    if (apiKey != NULL && strlen(apiKey) > 0) {
        snprintf(apiAuthHeader, sizeof(apiAuthHeader),
                 "Authorization: Bearer %s\r\n", apiKey);
    }

    /* Build proxy auth header if needed */
    STRPTR authHeader = AllocVec(256, MEMF_CLEAR | MEMF_ANY);
    if (useProxy && proxyRequiresAuth && !proxyUsesSSL) {
        STRPTR credentials =
            AllocVec(strlen(proxyUsername) + strlen(proxyPassword) + 2,
                     MEMF_ANY | MEMF_CLEAR);
        snprintf(credentials, strlen(proxyUsername) + strlen(proxyPassword) + 2,
                 "%s:%s", proxyUsername, proxyPassword);
        UBYTE *encodedCredentials = base64Encode(credentials);
        snprintf(authHeader, 256, "Proxy-Authorization: Basic %s\r\n",
                 encodedCredentials);
        FreeVec(encodedCredentials);
        FreeVec(credentials);
    }

    /* Build the HTTP request */
    if (useSSL || useProxy) {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "POST %s://%s:%d/v1/responses HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "Content-Length: %lu\r\n"
                 "%s\r\n"
                 "%s\0",
                 useSSL ? "https" : "http", host, port, host, port,
                 apiAuthHeader, strlen(jsonString), authHeader, jsonString);
    } else {
        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "POST /v1/responses HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "%s"
                 "User-Agent: AmigaGPT\r\n"
                 "Content-Length: %lu\r\n"
                 "%s\r\n"
                 "%s\0",
                 host, port, apiAuthHeader, strlen(jsonString), authHeader,
                 jsonString);
    }

    json_object_put(obj);
    FreeVec(authHeader);

    /* Connect and send */
    updateStatusBar(STRING_CONNECTING, yellowPen);
    while (createSSLConnection(host, port, useSSL, useProxy, proxyHost,
                               proxyPort, proxyUsesSSL, proxyRequiresAuth,
                               proxyUsername, proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            return NULL;
        }
    }

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ERR_clear_error();
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        if (ssl_err <= 0) {
            reportSslError(ssl, ssl_err, "SSL_write (tool result)");
            return NULL;
        }
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
        if (ssl_err <= 0) {
            displayError(STRING_ERROR_REQUEST_WRITE);
            return NULL;
        }
    }

    /* Read response */
    memset(readBuffer, 0, READ_BUFFER_LENGTH);
    LONG totalBytesRead = 0;
    WORD bytesRead = 0;
    LONG err = 0;
    BOOL doneReading = FALSE;
    UBYTE statusMessage[64];
    UBYTE *tempReadBuffer =
        AllocVec(TEMP_READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);

    while (!doneReading) {
#ifndef DAEMON
        DoMethod(loadingBar, MUIM_Busy_Move);
#endif
        if (useSSL) {
            ERR_clear_error();
            bytesRead =
                SSL_read(ssl, tempReadBuffer, TEMP_READ_BUFFER_LENGTH - 1);
        } else {
            bytesRead =
                recv(sock, tempReadBuffer, TEMP_READ_BUFFER_LENGTH - 1, 0);
        }

        snprintf(statusMessage, sizeof(statusMessage),
                 STRING_DOWNLOADING_RESPONSE);
        updateStatusBar(statusMessage, yellowPen);

        if (bytesRead <= 0) {
            doneReading = TRUE;
            break;
        }

        strncat(readBuffer, tempReadBuffer, bytesRead);
        totalBytesRead += bytesRead;

        /* Check if we have a complete response.
         *
         * IMPORTANT: Some servers (e.g. Cloudflare) include JSON blobs in HTTP
         * headers (e.g. Report-To / NEL). Only parse JSON from the HTTP body.
         */
        STRPTR jsonStart = NULL;
        {
            STRPTR body = NULL;
            STRPTR headerEnd = strstr(readBuffer, "\r\n\r\n");
            ULONG headerDelimLen = 4;
            if (headerEnd == NULL) {
                headerEnd = strstr(readBuffer, "\n\n");
                headerDelimLen = 2;
            }
            if (headerEnd != NULL) {
                body = headerEnd + headerDelimLen;
            }
            while (body != NULL && (*body == '\r' || *body == '\n' ||
                                    *body == ' ' || *body == '\t')) {
                body++;
            }
            if (body != NULL) {
                STRPTR objStart = strchr(body, '{');
                STRPTR arrStart = strchr(body, '[');
                if (objStart != NULL && arrStart != NULL) {
                    jsonStart = (objStart < arrStart) ? objStart : arrStart;
                } else if (objStart != NULL) {
                    jsonStart = objStart;
                } else if (arrStart != NULL) {
                    jsonStart = arrStart;
                }
            }
        }

        if (jsonStart != NULL) {
            struct json_object *parsedResponse = json_tokener_parse(jsonStart);
            if (parsedResponse != NULL) {
                /* Check if the response contains another tool call */
                if (configGetShellToolEnabled() &&
                    hasShellToolCall(parsedResponse)) {
                    STRPTR cId = getShellToolCallId(parsedResponse);
                    STRPTR cmd = getShellToolCommand(parsedResponse);
                    /* For non-streaming, id is at top level */
                    STRPTR rId = (STRPTR)json_object_get_string(
                        json_object_object_get(parsedResponse, "id"));

                    if (cId && cmd && rId) {
                        pendingToolCall = TRUE;
                        strncpy(pendingToolCallId, cId,
                                sizeof(pendingToolCallId) - 1);
                        strncpy(pendingToolCommand, cmd,
                                sizeof(pendingToolCommand) - 1);
                        strncpy(pendingResponseId, rId,
                                sizeof(pendingResponseId) - 1);
                    }
                }
                FreeVec(tempReadBuffer);
                updateStatusBar(STRING_READY, greenPen);
                return parsedResponse;
            }
        }

        if (totalBytesRead >= READ_BUFFER_LENGTH - 1) {
            doneReading = TRUE;
        }
    }

    FreeVec(tempReadBuffer);

    /* Try to parse whatever we got */
    STRPTR jsonStart = NULL;
    {
        STRPTR body = NULL;
        STRPTR headerEnd = strstr(readBuffer, "\r\n\r\n");
        ULONG headerDelimLen = 4;
        if (headerEnd == NULL) {
            headerEnd = strstr(readBuffer, "\n\n");
            headerDelimLen = 2;
        }
        if (headerEnd != NULL) {
            body = headerEnd + headerDelimLen;
        }
        while (body != NULL && (*body == '\r' || *body == '\n' ||
                                *body == ' ' || *body == '\t')) {
            body++;
        }
        if (body != NULL) {
            STRPTR objStart = strchr(body, '{');
            STRPTR arrStart = strchr(body, '[');
            if (objStart != NULL && arrStart != NULL) {
                jsonStart = (objStart < arrStart) ? objStart : arrStart;
            } else if (objStart != NULL) {
                jsonStart = objStart;
            } else if (arrStart != NULL) {
                jsonStart = arrStart;
            }
        }
    }
    if (jsonStart != NULL) {
        struct json_object *parsedResponse = json_tokener_parse(jsonStart);
        if (parsedResponse != NULL) {
            /* Check if the response contains another tool call */
            if (configGetShellToolEnabled() &&
                hasShellToolCall(parsedResponse)) {
                STRPTR callId = getShellToolCallId(parsedResponse);
                STRPTR command = getShellToolCommand(parsedResponse);
                /* For non-streaming, id is at top level */
                STRPTR respId = (STRPTR)json_object_get_string(
                    json_object_object_get(parsedResponse, "id"));

                if (callId && command && respId) {
                    pendingToolCall = TRUE;
                    strncpy(pendingToolCallId, callId,
                            sizeof(pendingToolCallId) - 1);
                    strncpy(pendingToolCommand, command,
                            sizeof(pendingToolCommand) - 1);
                    strncpy(pendingResponseId, respId,
                            sizeof(pendingResponseId) - 1);
                }
            }
            updateStatusBar(STRING_READY, greenPen);
            return parsedResponse;
        }
    }

    updateStatusBar(STRING_ERROR, redPen);
    return NULL;
}