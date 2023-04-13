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
#include "openai.h"
#include <stdbool.h>
#include <stdio.h>
#include "speech.h"
#include "api_key.h"

#define HOST "api.openai.com"
#define PORT 443

static void cleanup();
static LONG connectToOpenAI();
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);
static UBYTE* getResponseFromJson(UBYTE *json, LONG jsonLength, LONG *responseLength);
static void formatText(UBYTE *content, LONG *stringLength);

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

ULONG rangeRand(ULONG maxValue) {
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

static BPTR ErrOutput() {
	return(((struct Process *)FindTask(NULL))->pr_CES);
}

static BPTR GetStdErr() {
	BPTR err = ErrOutput();
	return(err ? err : Output());
}

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

    return connectToOpenAI();
}

static void formatText(UBYTE *string, LONG *stringLength) {
    LONG newStringIndex = 0;
    const LONG oldStringLength = *stringLength;
    for (LONG oldStringIndex = 0; oldStringIndex < oldStringLength; oldStringIndex++) {
        if (string[oldStringIndex] == '\\') {
            if (string[oldStringIndex + 1] == 'n') {
                string[newStringIndex++] = '\n';
            } else if (string[oldStringIndex + 1] == 'r') {
                string[newStringIndex++] = '\r';
            } else if (string[oldStringIndex + 1] == 't') {
                string[newStringIndex++] = '\t';
            } else if (string[oldStringIndex + 1] == '\\') {
                string[newStringIndex++] = '\\';
            } else if (string[oldStringIndex + 1] == '"') {
                string[newStringIndex++] = '"';
            }
            oldStringIndex++;
        } else {
            string[newStringIndex++] = string[oldStringIndex];
        }
    }
    string[newStringIndex++] = '\0';
    *stringLength = newStringIndex;
}    

UBYTE* postMessageToOpenAI(UBYTE *content, UBYTE *model, UBYTE *role) {
    struct sockaddr_in addr;
	struct hostent *hostent;
    UBYTE *response = NULL;

    memset(readBuffer, 0, READ_BUFFER_LENGTH);
    memset(writeBuffer, 0, WRITE_BUFFER_LENGTH);

    sprintf(readBuffer,
            "{\"model\": \"%s\",\r\n"
            "\"messages\": [{\"role\": \"%s\", \"content\": \"%s\"}]\r\n"
            "}\0",
            model, role, content);
    ULONG bodyLength = strlen(readBuffer);

    sprintf(writeBuffer, "POST /v1/chat/completions HTTP/1.1\r\n"
            "Host: api.openai.com\r\n"
            "Content-Type: application/json\r\n"
            "Authorization: Bearer %s\r\n"
            "Content-Length: %lu\r\n\r\n"
            "%s", openAiApiKey, bodyLength, readBuffer);

    memset(readBuffer, 0, READ_BUFFER_LENGTH);

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
                    printf("SSL connection to %s using %s\n\0", HOST, SSL_get_cipher(ssl));
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
                printf( "Couldn't connect to host!\n");
                return NULL;
            }
        } else {
            printf("Couldn't create new SSL handle!\n");
            return NULL;
        }

    ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));

    if (ssl_err > 0) {
        /* Dump everything to output */
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
                    LONG responseLength = 0;
                    response = getResponseFromJson(readBuffer, totalBytesRead, &responseLength);
                    if (response != NULL) {
                        formatText(response, &responseLength);
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
                    break;
                case SSL_ERROR_SSL:
                    printf("SSL_ERROR_SSL\n");
                    break;
                default:
                    printf("Unknown error: %ld\n", err);
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

static UBYTE* getResponseFromJson(UBYTE *json, LONG jsonLength, LONG *responseLength) {
    const UBYTE content[] = "\"content\":\"";
    const UBYTE responseTail[] = "\"finish_reason\":\"stop\",\"index\":0}]}";
    if ((strstr(json, responseTail) == NULL) || (strstr(json, content) == NULL))
        return NULL;
    UBYTE matchedIndex = 0;
    const ULONG contentLength = strlen(content);
    UBYTE braceDepth = 0;
    *responseLength = 0;

    for (LONG i = 0; i < jsonLength; i++) {
        if (json[i] == '{') {
            braceDepth++;
        } else if (json[i] == '}') {
            braceDepth--;
        }

        if (braceDepth == 0)
            continue;

        if (json[i] == content[matchedIndex]) {
            matchedIndex++;
        } else {
            matchedIndex = 0;
        }

        if (matchedIndex == contentLength) {
            UBYTE *response = AllocVec(jsonLength - matchedIndex, MEMF_ANY);
            UBYTE *responseIndex = (UBYTE *)response;
            for (LONG j = i; j < jsonLength; j++) {
                if (json[j + 1] == '\"' && json[j] != '\\') {
                    *responseIndex = 0;
                    return response;
                }
                (*responseLength)++;
                *responseIndex++ = json[j + 1];
            }
            // Close the string in case there's no closing quote found
            *responseIndex = 0;
            return response;
        }
    }
    *responseLength = -1;
    return NULL; // Return NULL if the "content" field is not found in the JSON string
}

static LONG connectToOpenAI() {
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

/* Get some suitable random seed data
 */
static void generateRandomSeed(UBYTE *buffer, LONG size) {
	for(int i = 0; i < size/2; i++) {
		((UWORD *)buffer)[i] = rangeRand(65535);
	}
}

/* This callback is called everytime OpenSSL verifies a certificate
 * in the chain during a connection, indicating success or failure.
 */
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
