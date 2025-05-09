#if defined(__AMIGAOS3__) || defined(__AMIGAOS4__)
#include <amissl/amissl.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#else
#include <openssl/ssl.h>
#include <netdb.h>
#endif
#include <json-c/json.h>
#include <mui/Busy_mcc.h>
#include <proto/exec.h>
#include <proto/socket.h>
#include <string.h>
#include <stdio.h>
#include "openai.h"
#include "speech.h"
#include "gui.h"
#include "MainWindow.h"

#define OPENAI_HOST "api.openai.com"
#define OPENAI_PORT 443
#define AUDIO_BUFFER_SIZE 4096
#define MAX_CONNECTION_RETRIES 10

static ULONG createSSLConnection(CONST_STRPTR host, UWORD port, BOOL useProxy,
                                 CONST_STRPTR proxyHost, UWORD proxyPort,
                                 BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
                                 CONST_STRPTR proxyUsername,
                                 CONST_STRPTR proxyPassword);
static ULONG rangeRand(ULONG maxValue);
static BPTR ErrOutput();
static BPTR GetStdErr();
static LONG createSSLContext();
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);
static STRPTR getModelName(enum ChatModel model);
static ULONG parseChunkLength(UBYTE *buffer, ULONG bufferLength);
static STRPTR base64Encode(CONST_STRPTR input);

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
 * @see enum ChatModel
 **/
CONST_STRPTR CHAT_MODEL_NAMES[] = {
    [CHATGPT_4o_LATEST] = "chatgpt-4o-latest",
    [GPT_3_5_TURBO] = "gpt-3.5-turbo",
    [GPT_4] = "gpt-4",
    [GPT_4_1] = "gpt-4.1",
    [GPT_4_1_2025_04_14] = "gpt-4.1-2025-04-14",
    [GPT_4_1_MINI] = "gpt-4.1-mini",
    [GPT_4_1_MINI_2025_04_14] = "gpt-4.1-mini-2025-04-14",
    [GPT_4_1_NANO] = "gpt-4.1-nano",
    [GPT_4_1_NANO_2025_04_14] = "gpt-4.1-nano-2025-04-14",
    [GPT_4_5_PREVIEW] = "gpt-4.5-preview",
    [GPT_4_5_PREVIEW_2025_02_27] = "gpt-4.5-preview-2025-02-27",
    [GPT_4_TURBO] = "gpt-4-turbo",
    [GPT_4_TURBO_2024_04_09] = "gpt-4-turbo-2024-04-09",
    [GPT_4_TURBO_PREVIEW] = "gpt-4-turbo-preview",
    [GPT_4o] = "gpt-4o",
    [GPT_4o_2024_11_20] = "gpt-4o-2024-11-20",
    [GPT_4o_2024_08_06] = "gpt-4o-2024-08-06",
    [GPT_4o_2024_05_13] = "gpt-4o-2024-05-13",
    [GPT_4o_MINI] = "gpt-4o-mini",
    [GPT_4o_MINI_2024_07_18] = "gpt-4o-mini-2024-07-18",
    [o1] = "o1",
    [o1_2024_12_17] = "o1-2024-12-17",
    // [o1_PREVIEW] = "o1-preview",
    // [o1_PREVIEW_2024_09_12] = "o1-preview-2024-09-12",
    // [o1_MINI] = "o1-mini",
    // [o1_MINI_2024_09_12] = "o1-mini-2024-09-12",
    [o1_PRO] = "o1-pro",
    [o1_PRO_2025_03_19] = "o1-pro-2025-03-19",
    [o3] = "o3",
    [o3_2025_04_16] = "o3-2025-04-16",
    [o3_MINI] = "o3-mini",
    [o3_MINI_2025_01_31] = "o3-mini-2025-01-31",
    [o4_MINI] = "o4-mini",
    [o4_MINI_2025_04_16] = "o4-mini-2025-04-16",
    NULL};

/**
 * The names of the image models
 * @see enum ImageModel
 **/
CONST_STRPTR IMAGE_MODEL_NAMES[] = {[DALL_E_2] = "dall-e-2",
                                    [DALL_E_3] = "dall-e-3",
                                    [GPT_IMAGE_1] = "gpt-image-1",
                                    NULL};

/**
 * The names of the image sizes
 * @see enum ImageSize
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
 * @see enum OpenAITTSModel
 **/
CONST_STRPTR OPENAI_TTS_MODEL_NAMES[] = {
    [OPENAI_TTS_MODEL_TTS_1] = "tts-1",
    [OPENAI_TTS_MODEL_TTS_1_HD] = "tts-1-hd",
    [OPENAI_TTS_MODEL_GPT_4o_MINI_TTS] = "gpt-4o-mini-tts",
    NULL};

/**
 * The image sizes for DALL-E 2
 * @see enum ImageSize
 **/
const enum ImageSize IMAGE_SIZES_DALL_E_2[] = {
    IMAGE_SIZE_256x256, IMAGE_SIZE_512x512, IMAGE_SIZE_1024x1024,
    IMAGE_SIZE_NULL};

/**
 * The image sizes for DALL-E 3
 * @see enum ImageSize
 **/
const enum ImageSize IMAGE_SIZES_DALL_E_3[] = {
    IMAGE_SIZE_1024x1024, IMAGE_SIZE_1792x1024, IMAGE_SIZE_1024x1792,
    IMAGE_SIZE_NULL};

/**
 * The image sizes for GPT Image 1
 * @see enum ImageSize
 **/
const enum ImageSize IMAGE_SIZES_GPT_IMAGE_1[] = {
    IMAGE_SIZE_1024x1024, IMAGE_SIZE_1536x1024, IMAGE_SIZE_1024x1536,
    IMAGE_SIZE_AUTO, IMAGE_SIZE_NULL};

/**
 * The names of the TTS voices
 * @see enum OpenAITTSVoice
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
                                         NULL};

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
 * @param useProxy whether to use a proxy or not
 * @param proxyHost the proxy host to use
 * @param proxyPort the proxy port to use
 * @param proxyUsesSSL whether the proxy uses SSL or not
 * @param proxyRequiresAuth whether the proxy requires authentication or not
 * @param proxyUsername the proxy username to use
 * @param proxyPassword the proxy password to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
static ULONG createSSLConnection(CONST_STRPTR host, UWORD port, BOOL useProxy,
                                 CONST_STRPTR proxyHost, UWORD proxyPort,
                                 BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
                                 CONST_STRPTR proxyUsername,
                                 CONST_STRPTR proxyPassword) {
    struct sockaddr_in addr;
    struct hostent *hostent;
    BOOL useSSL = !useProxy || proxyUsesSSL;

    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_User);

    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }

    if (sock > -1) {
        CloseSocket(sock);
        sock = -1;
    }

    if (useSSL) {
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
        if ((ssl_err = SSL_connect(ssl)) >= 0) {
            // printf("SSL connection to %s using %s\n\0", host,
            // SSL_get_cipher(ssl));
        }

        /* If there were errors, print them */
        if (ssl_err < 0) {
            LONG err = SSL_get_error(ssl, ssl_err);
            switch (err) {
            case SSL_ERROR_ZERO_RETURN:
                printf("SSL_ERROR_ZERO_RETURN\n");
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
                printf("SSL_ERROR_SYSCALL\n");
                break;
            case SSL_ERROR_SSL:
                printf("SSL_ERROR_SSL\n");
                break;
            default:
                printf("%s: %ld\n", STRING_ERROR_UNKNOWN, err);
                break;
            }
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
                        CONST_STRPTR proxyUsername,
                        CONST_STRPTR proxyPassword) {
    struct json_object **responses =
        AllocVec(sizeof(struct json_object *) * RESPONSE_ARRAY_BUFFER_LENGTH,
                 MEMF_ANY | MEMF_CLEAR);
    static BOOL streamingInProgress = FALSE;
    UWORD responseIndex = 0;
    UBYTE connectionRetryCount = 0;
    BOOL useSSL = !useProxy || proxyUsesSSL;

    if (!stream || !streamingInProgress) {
        memset(readBuffer, 0, READ_BUFFER_LENGTH);
        streamingInProgress = stream;

        struct json_object *obj = json_object_new_object();
        json_object_object_add(obj, "model",
                               json_object_new_string(CHAT_MODEL_NAMES[model]));
        struct json_object *conversationArray = json_object_new_array();

        struct MinNode *conversationNode = conversation->messages->mlh_Head;
        while (conversationNode->mln_Succ != NULL) {
            struct ConversationNode *message =
                (struct ConversationNode *)conversationNode;
            struct json_object *messageObj = json_object_new_object();
            json_object_object_add(messageObj, "role",
                                   json_object_new_string(message->role));
            json_object_object_add(messageObj, "content",
                                   json_object_new_string(message->content));
            json_object_array_add(conversationArray, messageObj);
            conversationNode = conversationNode->mln_Succ;
        }

        json_object_object_add(obj, "input", conversationArray);
        json_object_object_add(obj, "stream",
                               json_object_new_boolean((json_bool)stream));

        if (conversation->system != NULL && strlen(conversation->system) > 0) {
            json_object_object_add(
                obj, "instructions",
                json_object_new_string(conversation->system));
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

        snprintf(writeBuffer, WRITE_BUFFER_LENGTH,
                 "POST https://api.openai.com/v1/responses HTTP/1.1\r\n"
                 "Host: api.openai.com\r\n"
                 "Content-Type: application/json\r\n"
                 "Authorization: Bearer %s\r\n"
                 "User-Agent: AmigaGPT\r\n"
                 "Content-Length: %lu\r\n"
                 "%s\r\n"
                 "%s\0",
                 openAiApiKey, strlen(jsonString), authHeader, jsonString);

        json_object_put(obj);

        FreeVec(authHeader);

        updateStatusBar(STRING_CONNECTING, yellowPen);
        while (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useProxy,
                                   proxyHost, proxyPort, proxyUsesSSL,
                                   proxyRequiresAuth, proxyUsername,
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
            ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
        } else {
            ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
        }
    }

    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);

    if (ssl_err > 0 || stream) {
        ULONG totalBytesRead = 0;
        WORD bytesRead = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        UBYTE statusMessage[64];
        UBYTE *tempReadBuffer = AllocVec(
            useProxy ? 8192 : TEMP_READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
        while (!doneReading) {
            DoMethod(loadingBar, MUIM_Busy_Move);
            if (useSSL) {
                bytesRead =
                    SSL_read(ssl, tempReadBuffer,
                             useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1);
            } else {
                bytesRead =
                    recv(sock, tempReadBuffer,
                         useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1, 0);
            }
            snprintf(statusMessage, sizeof(statusMessage),
                     STRING_DOWNLOADING_RESPONSE);
            updateStatusBar(statusMessage, yellowPen);
            strncat(readBuffer, tempReadBuffer, bytesRead);
            if (useSSL) {
                err = SSL_get_error(ssl, bytesRead);
            } else {
                err = SSL_ERROR_NONE;
            }
            switch (err) {
            case SSL_ERROR_NONE:
                totalBytesRead += bytesRead;
                const STRPTR jsonStart = stream ? "data: {" : "{";
                STRPTR jsonString = readBuffer;
                // Check for error in stream
                if (stream) {
                    if (strstr(jsonString, jsonStart) == NULL) {
                        jsonString = strstr(jsonString, "{");
                        if (jsonString == NULL) {
                            STRPTR httpResponse =
                                strstr(readBuffer, "HTTP/1.1");
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
                                streamingInProgress = FALSE;
                            }
                        }
                    }
                }
                STRPTR lastJsonString = jsonString;
                while (jsonString = strstr(jsonString, jsonStart)) {
                    lastJsonString = jsonString;
                    if (stream)
                        jsonString += 6; // Get to the start of the JSON
                    struct json_object *parsedResponse =
                        json_tokener_parse(jsonString);
                    if (parsedResponse != NULL) {
                        responses[responseIndex++] = parsedResponse;
                        if (!stream) {
                            break;
                        }
                    } else if (!stream) {
                        jsonString = NULL;
                        break;
                    }
                }

                if (stream) {
                    if (json_tokener_parse(lastJsonString + 6) == NULL) {
                        snprintf(readBuffer, READ_BUFFER_LENGTH, "%s\0",
                                 lastJsonString);
                    } else {
                        memset(readBuffer, 0, READ_BUFFER_LENGTH);
                    }
                    doneReading = TRUE;
                } else if (jsonString != NULL) {
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
            case SSL_ERROR_SYSCALL:
                printf("SSL_ERROR_SYSCALL\n");
                ULONG err = ERR_get_error();
                printf("%s: %lu\n", STRING_ERROR, err);
                break;
            case SSL_ERROR_SSL:
                updateStatusBar(STRING_ERROR_LOST_CONNECTION, redPen);
                if (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useProxy,
                                        proxyHost, proxyPort, proxyUsesSSL,
                                        proxyRequiresAuth, proxyUsername,
                                        proxyPassword) == RETURN_ERROR) {
                    if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
                        displayError(STRING_ERROR_CONNECTION_MAX_RETRIES);
                        doneReading = TRUE;
                        streamingInProgress = FALSE;
                        FreeVec(tempReadBuffer);
                        struct json_object *response;
                        for (UWORD i = 0; i <= responseIndex; i++) {
                            response = responses[i];
                            json_object_put(response);
                        }
                        FreeVec(responses);
                        return NULL;
                    }
                }
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
        LONG err = SSL_get_error(ssl, ssl_err);
        switch (err) {
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
            printf("SSL_ERROR_SYSCALL\n");
            break;
        case SSL_ERROR_SSL:
            printf("SSL_ERROR_SSL\n");
            break;
        default:
            printf("%s: %ld\n", STRING_ERROR_UNKNOWN, err);
            break;
        }
    }
    if (stream) {
        // Check if the last response is the end of the stream and set the
        // streamingInProgress flag to FALSE so that the next request will
        // establish a new connection because OpenAI will close the connection
        // after the stream is finished
        STRPTR type = json_object_get_string(
            json_object_object_get(responses[responseIndex - 1], "type"));
        if (type != NULL && strcmp(type, "response.completed") == 0) {
            streamingInProgress = FALSE;
        }
    } else {
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
    CONST_STRPTR prompt, enum ImageModel imageModel, enum ImageSize imageSize,
    CONST_STRPTR openAiApiKey, BOOL useProxy, CONST_STRPTR proxyHost,
    UWORD proxyPort, BOOL proxyUsesSSL, BOOL proxyRequiresAuth,
    CONST_STRPTR proxyUsername, CONST_STRPTR proxyPassword) {
    struct json_object *response = NULL;
    UWORD responseIndex = 0;
    BOOL useSSL = !useProxy || proxyUsesSSL;
    struct json_tokener *tokener = json_tokener_new();
    json_tokener_set_flags(tokener, JSON_TOKENER_STRICT);

    memset(readBuffer, 0, READ_BUFFER_LENGTH);

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useProxy, proxyHost,
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
    json_object_object_add(
        obj, "model", json_object_new_string(IMAGE_MODEL_NAMES[imageModel]));
    json_object_object_add(obj, "prompt", json_object_new_string(prompt));
    json_object_object_add(obj, "size",
                           json_object_new_string(IMAGE_SIZE_NAMES[imageSize]));
    if (imageModel == GPT_IMAGE_1) {
        json_object_object_add(obj, "moderation",
                               json_object_new_string("low"));
    } else {
        json_object_object_add(obj, "response_format",
                               json_object_new_string("b64_json"));
    }
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
             "POST https://api.openai.com/v1/images/generations HTTP/1.1\r\n"
             "Host: api.openai.com\r\n"
             "Content-Type: application/json\r\n"
             "Authorization: Bearer %s\r\n"
             "User-Agent: AmigaGPT\r\n"
             "Content-Length: %lu\r\n"
             "%s\r\n"
             "%s\0",
             openAiApiKey, strlen(jsonString), authHeader, jsonString);

    json_object_put(obj);

    FreeVec(authHeader);

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
    }

    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);

    if (ssl_err > 0) {
        ULONG totalBytesRead = 0;
        WORD bytesRead = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        UBYTE statusMessage[64];
        UBYTE *tempReadBuffer = AllocVec(
            useProxy ? 8192 : TEMP_READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
        BOOL foundJson = FALSE;

        while (!doneReading) {
            DoMethod(loadingBar, MUIM_Busy_Move);
            if (useSSL) {
                bytesRead =
                    SSL_read(ssl, tempReadBuffer,
                             useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1);
            } else {
                bytesRead =
                    recv(sock, tempReadBuffer,
                         useProxy ? 8192 - 1 : TEMP_READ_BUFFER_LENGTH - 1, 0);
            }
            snprintf(statusMessage, sizeof(statusMessage), "%s (%lu %s)",
                     STRING_DOWNLOADING_IMAGE, totalBytesRead, STRING_BYTES);
            updateStatusBar(statusMessage, yellowPen);
            if (!foundJson) {
                strcat(readBuffer, tempReadBuffer);
            }
            if (useSSL) {
                err = SSL_get_error(ssl, bytesRead);
            } else {
                err = SSL_ERROR_NONE;
            }
            switch (err) {
            case SSL_ERROR_NONE: {
                STRPTR httpResponse = strstr(readBuffer, "HTTP/1.1");
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
                }
                totalBytesRead += bytesRead;
                if (!foundJson) {
                    CONST_STRPTR jsonStart = "{";
                    jsonString = readBuffer;
                    jsonString = strstr(jsonString, jsonStart);
                    if (jsonString == NULL) {
                        continue;
                    }
                    foundJson = TRUE;
                }
                enum json_tokener_error jerr;
                jerr = json_tokener_get_error(tokener);
                if (jerr != json_tokener_continue) {
                    response = json_tokener_parse_ex(tokener, jsonString,
                                                     strlen(jsonString));
                } else {
                    response = json_tokener_parse_ex(tokener, tempReadBuffer,
                                                     bytesRead);
                }
                jerr = json_tokener_get_error(tokener);

                if (jerr != json_tokener_success &&
                    jerr != json_tokener_continue) {
                    displayError(json_tokener_error_desc(jerr));
                }

                if (response == NULL) {
                    continue;
                }

                doneReading = TRUE;
                break;
            }
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
            case SSL_ERROR_SYSCALL:
                printf("SSL_ERROR_SYSCALL\n");
                ULONG err = ERR_get_error();
                printf("%s: %lu\n", STRING_ERROR, err);
                break;
            case SSL_ERROR_SSL:
                updateStatusBar(STRING_ERROR_LOST_CONNECTION, redPen);
                if (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useProxy,
                                        proxyHost, proxyPort, proxyUsesSSL,
                                        proxyRequiresAuth, proxyUsername,
                                        proxyPassword) == RETURN_ERROR) {
                    if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
                        displayError(STRING_ERROR_CONNECTION_MAX_RETRIES);
                        doneReading = TRUE;
                    }
                }
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
        LONG err = SSL_get_error(ssl, ssl_err);
        switch (err) {
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
            printf("SSL_ERROR_SYSCALL\n");
            break;
        case SSL_ERROR_SSL:
            printf("SSL_ERROR_SSL\n");
            break;
        default:
            printf("%s: %ld\n", STRING_ERROR_UNKNOWN, err);
            break;
        }
    }

    CloseSocket(sock);
    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        ssl = NULL;
    }
    sock = -1;
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
             url, hostString, authHeader);

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(hostString, 443, useProxy, proxyHost, proxyPort,
                               proxyUsesSSL, proxyRequiresAuth, proxyUsername,
                               proxyPassword) == RETURN_ERROR) {
        if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
            displayError(STRING_ERROR_CONNECTING_MAX_RETRIES);
            Close(fileHandle);
            return RETURN_ERROR;
        }
    }
    connectionRetryCount = 0;

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
    }

    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);

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
            DoMethod(loadingBar, MUIM_Busy_Move);
            if (useSSL) {
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
                updateStatusBar(STRING_ERROR_LOST_CONNECTION, redPen);
                if (createSSLConnection(hostString, 443, useProxy, proxyHost,
                                        proxyPort, proxyUsesSSL,
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
                         url, hostString, totalBytesRead, authHeader);
                SSL_write(ssl, writeBuffer, strlen(writeBuffer));
                break;
            }
        }
        FreeVec(tempReadBuffer);
    } else {
        displayError(STRING_ERROR_REQUEST_WRITE);
        LONG err = SSL_get_error(ssl, ssl_err);
        switch (err) {
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
            printf("SSL_ERROR_SYSCALL\n");
            break;
        case SSL_ERROR_SSL:
            printf("SSL_ERROR_SSL\n");
            break;
        default:
            printf("%s: %ld\n", STRING_ERROR_UNKNOWN, err);
            break;
        }
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
    CONST_STRPTR proxyPassword) {
    struct json_object *response;
    BOOL useSSL = !useProxy || proxyUsesSSL;

    *audioLength = 0;

    updateStatusBar(STRING_CONNECTING, yellowPen);
    UBYTE connectionRetryCount = 0;
    while (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useProxy, proxyHost,
                               proxyPort, proxyUsesSSL, proxyRequiresAuth,
                               proxyUsername, proxyPassword) == RETURN_ERROR) {
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
    json_object_object_add(obj, "response_format",
                           json_object_new_string("pcm"));
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
             "POST https://api.openai.com/v1/audio/speech HTTP/1.1\r\n"
             "Host: api.openai.com\r\n"
             "Content-Type: application/json\r\n"
             "Authorization: Bearer %s\r\n"
             "User-Agent: AmigaGPT\r\n"
             "Content-Length: %lu\r\n"
             "%s\r\n"
             "%s\0",
             openAiApiKey, strlen(jsonString), authHeader, jsonString);

    json_object_put(obj);

    FreeVec(authHeader);

    updateStatusBar(STRING_SENDING_REQUEST, yellowPen);
    if (useSSL) {
        ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
    } else {
        ssl_err = send(sock, writeBuffer, strlen(writeBuffer), 0);
    }

    set(loadingBar, MUIA_Busy_Speed, MUIV_Busy_Speed_Off);

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
            DoMethod(loadingBar, MUIM_Busy_Move);
            memset(readBuffer, 0, READ_BUFFER_LENGTH);
            if (useSSL) {
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
                                if (json_object_object_get_ex(response, "error",
                                                              &error)) {
                                    struct json_object *message =
                                        json_object_object_get(error,
                                                               "message");
                                    STRPTR messageString =
                                        json_object_get_string(message);
                                    displayError(messageString);
                                    FreeVec(audioData);
                                    return NULL;
                                }
                            } else {
                                STRPTR httpResponse =
                                    strstr(readBuffer, "HTTP/1.1");
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
                                    FreeVec(audioData);
                                    return NULL;
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
                            bytesRemainingInBuffer += bytesRead;
                        }
                        dataStart += chunkBytesNeedingRead + 2;
                        bytesRemainingInBuffer -= 2;
                        chunkBytesNeedingRead = 0;
                        newChunkNeeded = TRUE;
                    }
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
            case SSL_ERROR_SYSCALL:
                printf("SSL_ERROR_SYSCALL\n");
                ULONG err = ERR_get_error();
                printf("%s: %lu\n", STRING_ERROR, err);
            case SSL_ERROR_SSL:
                updateStatusBar(STRING_ERROR_LOST_CONNECTION, redPen);
                if (createSSLConnection(OPENAI_HOST, OPENAI_PORT, useProxy,
                                        proxyHost, proxyPort, proxyUsesSSL,
                                        proxyRequiresAuth, proxyUsername,
                                        proxyPassword) == RETURN_ERROR) {
                    if (connectionRetryCount++ >= MAX_CONNECTION_RETRIES) {
                        displayError(STRING_ERROR_CONNECTION_MAX_RETRIES);
                        FreeVec(audioData);
                        return NULL;
                    }
                }
                break;
            default:
                printf(STRING_ERROR_UNKNOWN);
                putchar('\n');
                break;
            }
        }
    } else {
        displayError(STRING_ERROR_REQUEST_WRITE);
        LONG err = SSL_get_error(ssl, ssl_err);
        switch (err) {
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
            printf("SSL_ERROR_SYSCALL\n");
            break;
        case SSL_ERROR_SSL:
            printf("SSL_ERROR_SSL\n");
            break;
        default:
            printf("%s: %ld\n", STRING_ERROR_UNKNOWN, err);
            break;
        }
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