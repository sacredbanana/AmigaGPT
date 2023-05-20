#include <amissl/amissl.h>
#include <proto/exec.h>
#include <proto/socket.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <proto/utility.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <utility/utility.h>
#include <string.h>
#include <stdbool.h>
#include "gui.h"
#include "openai.h"

#include <stdio.h>
#include "speech.h"
#include "external/json-c/json.h"

#define HOST "api.openai.com"
#define PORT 443

static ULONG rangeRand(ULONG maxValue);
static BPTR ErrOutput();
static BPTR GetStdErr();
static void cleanup();
static LONG createSSLContext();
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);
static STRPTR getMessageContentFromJson(struct json_object *json);
static void formatText(STRPTR unformattedText);
static STRPTR getModelName(enum Model model);

struct Library *AmiSSLMasterBase, *AmiSSLBase, *AmiSSLExtBase, *SocketBase;
struct UtilityBase *UtilityBase;
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
static SSL_CTX *_ssl_context;
LONG UsesOpenSSLStructs = FALSE;
BOOL amiSSLInitialized = FALSE;
UBYTE *writeBuffer = NULL;
UBYTE *readBuffer = NULL;
UBYTE *tempBuffer = NULL;
X509 *server_cert;
SSL_CTX *ctx;
BIO *bio, *bio_err;
SSL *ssl;
LONG sock = -1;
LONG ssl_err = 0;
ULONG RangeSeed;

/**
 * Generate a random number
 * @param maxValue the maximum value of the random number
 * @return a random number
**/
static ULONG rangeRand(ULONG maxValue) {
    ULONG a=RangeSeed;
    UWORD i=maxValue-1;
    do {
        ULONG b=a;
        a<<=1;
        if((LONG)b<=0)
        a^=0x1d872b41;
    } while((i>>=1));
    RangeSeed=a;
    if((UWORD)maxValue)
        return (UWORD)((UWORD)a*(UWORD)maxValue>>16);
  return (UWORD)a;
}

/**
 * Get the error output
 * @return the error output
**/
static BPTR ErrOutput() {
	return(((struct Process *)FindTask(NULL))->pr_CES);
}

/**
 * Get the standard error
 * @return the standard error
**/
static BPTR GetStdErr() {
	BPTR err = ErrOutput();
	return(err ? err : Output());
}

/**
 * Initialize the OpenAI connector
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
LONG initOpenAIConnector() {
    readBuffer = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY);
    writeBuffer = AllocVec(WRITE_BUFFER_LENGTH, MEMF_ANY);
    tempBuffer = AllocVec(TEMP_BUFFER_LENGTH, MEMF_ANY);

	if ((UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library", 0)) == NULL) {
        printf("failed to open utility.library\n");
        return RETURN_ERROR;
    }

	if ((SocketBase = OpenLibrary("bsdsocket.library", 0)) == NULL) {
        printf("failed to open bsdsocket.library\n");
        return RETURN_ERROR;
    }

	if ((AmiSSLMasterBase = OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION)) == NULL) {
        printf("failed to open amisslmaster.library\n");
        return RETURN_ERROR;
    }

    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
	                  AmiSSL_UsesOpenSSLStructs, TRUE,
                      AmiSSL_InitAmiSSL, TRUE,
	                  AmiSSL_GetAmiSSLBase, &AmiSSLBase,
	                  AmiSSL_GetAmiSSLExtBase, &AmiSSLExtBase,
	                  AmiSSL_SocketBase, SocketBase,
	                  AmiSSL_ErrNoPtr, &errno,
	                  TAG_DONE) != 0) {
        printf("failed to initialize amisslmaster.library\n");
        return RETURN_ERROR;
    }
		
	amiSSLInitialized = TRUE;

    return createSSLContext();
}

/**
 * Format a string with escape sequences into a string with the actual characters
 * @param unformattedText the text to format
**/
static void formatText(STRPTR unformattedText) {
    LONG newStringIndex = 0;
    const LONG oldStringLength = strlen(unformattedText);
    for (LONG oldStringIndex = 0; oldStringIndex < oldStringLength; oldStringIndex++) {
        if (unformattedText[oldStringIndex] == '\\') {
            if (unformattedText[oldStringIndex + 1] == 'n') {
                unformattedText[newStringIndex++] = '\n';
            } else if (unformattedText[oldStringIndex + 1] == 'r') {
                unformattedText[newStringIndex++] = '\r';
            } else if (unformattedText[oldStringIndex + 1] == 't') {
                unformattedText[newStringIndex++] = '\t';
            }
            oldStringIndex++;
        } else {
            unformattedText[newStringIndex++] = unformattedText[oldStringIndex];
        }
    }
    unformattedText[newStringIndex++] = '\0';
}

/**
 * Get the model name as a string
 * @param model the model
 * @return the model name as a string
**/
static STRPTR getModelName(enum Model model) {
	switch (model) {
		case GPT_4:
			return "gpt-4";
		case GPT_4_0314:
			return "gpt-4-0314";
		case GPT_4_32K:
			return "gpt-4-32k";
		case GPT_4_32K_0314:
			return "gpt-4-32k-0314";
		case GPT_3_5_TURBO:
			return "gpt-3.5-turbo";
		case GPT_3_5_TURBO_0301:
			return "gpt-3.5-turbo-0301";
		default:
			return NULL;
	}
}

/**
 * Post a message to OpenAI
 * @param conversation the conversation to post
 * @param model the model to use
 * @return a pointer to a new string containing the response -- Free it with FreeVec() when you are done using it
 * @todo Handle errors
**/
STRPTR postMessageToOpenAI(struct MinList *conversation, enum Model model, STRPTR openAiApiKey) {
    struct sockaddr_in addr;
	struct hostent *hostent;
    STRPTR response = NULL;

    memset(readBuffer, 0, READ_BUFFER_LENGTH);
    memset(writeBuffer, 0, WRITE_BUFFER_LENGTH);

    struct json_object *obj = json_object_new_object();
    json_object_object_add(obj, "model", json_object_new_string(getModelName(model)));
    struct json_object *conversationArray = json_object_new_array();

    struct MinNode *conversationNode = conversation->mlh_Head;
    while (conversationNode->mln_Succ != NULL) {
        struct ConversationNode *message = (struct ConversationNode *)conversationNode;
        struct json_object *messageObj = json_object_new_object();
        json_object_object_add(messageObj, "role", json_object_new_string(message->role));
        json_object_object_add(messageObj, "content", json_object_new_string(message->content));
        json_object_array_add(conversationArray, messageObj);
        conversationNode = conversationNode->mln_Succ;
    }

    json_object_object_add(obj, "messages", conversationArray);
    
    STRPTR jsonString = json_object_to_json_string(obj);

    sprintf(writeBuffer, "POST /v1/chat/completions HTTP/1.1\r\n"
            "Host: api.openai.com\r\n"
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n"
            "User-Agent: AmigaGPT\r\n"
            "Content-Length: %lu\r\n\r\n"
            "%s", openAiApiKey, strlen(jsonString), jsonString);

    json_object_put(obj);

    /* The following needs to be done once per socket */
        if((ssl = SSL_new(ctx)) != NULL) {
            /* Lookup hostname */
            if ((hostent = gethostbyname(HOST)) != NULL) {
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(PORT);
                addr.sin_len = hostent->h_length;;
                memcpy(&addr.sin_addr,hostent->h_addr,hostent->h_length);
            }
            else {
                printf("Host lookup failed\n");
                return NULL;
            }

            /* Create a socket and connect to the server */
            if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
                if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
                    printf("Couldn't connect to server\n");
                    return NULL;
                }
            }

            /* Check if connection was established */
            if (sock >= 0) {
                /* Associate the socket with the ssl structure */
                SSL_set_fd(ssl, sock);

                /* Set up SNI (Server Name Indication) */
                SSL_set_tlsext_host_name(ssl, HOST);

                /* Perform SSL handshake */
                if((ssl_err = SSL_connect(ssl)) >= 0) {
                    // printf("SSL connection to %s using %s\n\0", HOST, SSL_get_cipher(ssl));
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
                            printf("Unknown error: %ld\n", err);
                            break;
                    }
                    return NULL;
                }
            } else {
                printf("Couldn't connect to host!\n");
                return NULL;
            }
        } else {
            printf("Couldn't create new SSL handle!\n");
            return NULL;
        }

    ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));

    if (ssl_err > 0) {
        ULONG totalBytesRead = 0;
        WORD bytesRead = 0;
        BOOL doneReading = FALSE;
        LONG err = 0;
        while (!doneReading) {
            bytesRead = SSL_read(ssl, tempBuffer, TEMP_BUFFER_LENGTH);
            err = SSL_get_error(ssl, bytesRead);
            switch (err) {
                case SSL_ERROR_NONE:
                    memcpy(readBuffer + totalBytesRead, tempBuffer, bytesRead);
                    totalBytesRead += bytesRead;
                    STRPTR jsonString = strstr(readBuffer, "{");
                    struct json_object *jsonObject = json_tokener_parse(jsonString);
                    if (jsonObject != NULL) {
                        doneReading = TRUE;
                        response = getMessageContentFromJson(jsonObject);
                        json_object_put(jsonObject);
                        if (response != NULL) {
                            formatText(response);
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
                    printf("error: %lu\n", err);
                    break;
                case SSL_ERROR_SSL:
                    printf("SSL_ERROR_SSL\n");
                    Delay(100);
                    break;
                default:
                    printf("Unknown error\n");
                    break;
            }            
        }
    } else {
        printf("Couldn't write request!\n");
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
                printf("Unknown error: %ld\n", err);
                break;
        }
    }
    SSL_free(ssl);
    CloseSocket(sock);
    return response;
}

/**
 * Get the message content from the JSON response from OpenAI
 * @param json the JSON response from OpenAI
 * @return a pointer to a new string containing the message content -- Free it with FreeVec() when you are done using it
 * @todo Handle errors
**/
static STRPTR getMessageContentFromJson(struct json_object *json) {
    if (json == NULL) return NULL;
    struct json_object *error;
    if (json_object_object_get_ex(json, "error", &error)) {
        struct json_object *message = json_object_object_get(error, "message");
        STRPTR messageString = json_object_get_string(message);
        displayError(messageString);
        json_object_put(json);
        return NULL;
    }
    
    STRPTR json_str = json_object_to_json_string(json);
    struct json_object *choices = json_object_object_get(json, "choices");
    struct json_object *choice = json_object_array_get_idx(choices, 0);
    struct json_object *message = json_object_object_get(choice, "message");
    struct json_object *content = json_object_object_get(message, "content");
    STRPTR contentString = json_object_get_string(content);
    STRPTR response = AllocVec(strlen(contentString) + 1, MEMF_ANY | MEMF_CLEAR);
    memcpy(response, contentString, strlen(contentString));
    return response;
}

/**
 * Create a new SSL context
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static LONG createSSLContext() {
    /* Basic intialization. Next few steps (up to SSL_new()) need
     * to be done only once per AmiSSL opener.
     */
    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);

    /* Seed the entropy engine */
    generateRandomSeed(writeBuffer, 128);
    RAND_seed(writeBuffer, 128);

    /* Note: BIO writing routines are prepared for NULL BIO handle */
    if ((bio_err = BIO_new(BIO_s_file())) != NULL)
        BIO_set_fp_amiga(bio_err, GetStdErr(), BIO_NOCLOSE | BIO_FP_TEXT);

    /* Get a new SSL context */
    if ((ctx = SSL_CTX_new(TLS_client_method())) != NULL) {
        /* Basic certificate handling */
        SSL_CTX_set_default_verify_paths(ctx);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_cb);
    } else {
        printf("Couldn't create new context!\n");
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
	for(int i = 0; i < size/2; i++) {
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
 * Cleanup the OpenAI connector and free all resources
**/
void closeOpenAIConnector() {
	if (amiSSLInitialized) {
        SSL_shutdown(ssl);
        SSL_CTX_free(ctx);
        BIO_free(bio_err);
		CleanupAmiSSLA(NULL);
	}

	if (AmiSSLBase) {
		CloseAmiSSL();
		AmiSSLBase = NULL;
	}

    FreeVec(writeBuffer);
    FreeVec(readBuffer);
    FreeVec(tempBuffer);
}
