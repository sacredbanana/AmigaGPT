#include <amissl/amissl.h>
#include <json-c/json.h>
#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <proto/amissl.h>
#include <proto/amisslmaster.h>
#include <proto/exec.h>
#include <proto/socket.h>
#include <proto/utility.h>
#include <string.h>
#include <stdio.h>
#include <utility/utility.h>
#include "openai.h"
#include "speech.h"
#include "gui.h"

#define HOST "api.openai.com"
#define PORT 443

static ULONG rangeRand(ULONG maxValue);
static BPTR ErrOutput();
static BPTR GetStdErr();
static LONG createSSLContext();
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);
static STRPTR getModelName(enum Model model);

struct Library *AmiSSLMasterBase, *AmiSSLBase, *AmiSSLExtBase, *SocketBase = NULL;
#ifdef __AMIGAOS4__
struct AmiSSLMasterIFace *IAmiSSLMaster;
struct AmiSSLIFace *IAmiSSL;
struct SocketIFace *ISocket;
extern struct UtilityIFace *IUtility;
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
 * The names of the models
 * @see enum Model
**/ 
CONST_STRPTR MODEL_NAMES[] = {
	[GPT_4] = "gpt-4",
	[GPT_4_0314] = "gpt-4-0314",
	[GPT_4_0613] = "gpt-4-0613",
	[GPT_4_1106_PREVIEW] = "gpt-4-1106-preview",
	[GPT_4_32K] = "gpt-4-32k",
	[GPT_4_32K_0314] = "gpt-4-32k-0314",
	[GPT_4_32K_0613] = "gpt-4-32k-0613",
	[GPT_3_5_TURBO] = "gpt-3.5-turbo",
	[GPT_3_5_TURBO_0301] = "gpt-3.5-turbo-0301",
	[GPT_3_5_TURBO_0613] = "gpt-3.5-turbo-0613",
	[GPT_3_5_TURBO_1106] = "gpt-3.5-turbo-1106",
	[GPT_3_5_TURBO_16K] = "gpt-3.5-turbo-16k",
	[GPT_3_5_TURBO_16K_0613] = "gpt-3.5-turbo-16k-0613"
};

/**
 * The names of the image models
 * @see enum ImageModel
**/ 
CONST_STRPTR IMAGE_MODEL_NAMES[] = {
	[DALL_E_2] = "dall-e-2",
	[DALL_E_3] = "dall-e-3"
};

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

	#ifdef __AMIGAOS3__
	if ((SocketBase = OpenLibrary("bsdsocket.library", 0)) == NULL) {
		displayError("failed to open bsdsocket.library. You have to install a TCP/IP stack such as AmiTCP, Miami or Roadshow. Please refer to the documentation for more information.");
		return RETURN_ERROR;
	}
	#else
	if ((SocketBase = OpenLibrary("bsdsocket.library", 4)) == NULL) {
		displayError("failed to open bsdsocket.library version 4");
		return RETURN_ERROR;
	}
	if ((ISocket = (struct SocketIFace *)GetInterface(SocketBase, "main", 1, NULL)) == NULL) {
		displayError("failed to get the interface of bsdsocket.library");
		return RETURN_ERROR;
	}
	#endif

	#ifdef __AMIGAOS3__
	if ((AmiSSLMasterBase = OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION)) == NULL) {
		displayError("failed to open amisslmaster.library version 5. You have to install AmiSSL 5. Please refer to the documentation for more information.");
		return RETURN_ERROR;
	}
	#else
	if ((AmiSSLMasterBase = OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION)) == NULL) {
		displayError("failed to open amisslmaster.library version 5. You have to install AmiSSL 5. Please refer to the documentation for more information.");
		return RETURN_ERROR;
	}
	if ((IAmiSSLMaster = (struct AmiSSLMasterIFace *)GetInterface(AmiSSLMasterBase, "main", 1, NULL)) == NULL) {
		displayError("failed to get the interface of amisslmaster.library");
		return RETURN_ERROR;
	}
	#endif

	if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
					  AmiSSL_UsesOpenSSLStructs, TRUE,
					  AmiSSL_InitAmiSSL, TRUE,
					  AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
					  AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
					  AmiSSL_SocketBase, (ULONG)SocketBase,
					  AmiSSL_ErrNoPtr, (ULONG)&errno,
					  TAG_DONE) != 0) {
		displayError("failed to initialize amisslmaster.library");
		return RETURN_ERROR;
	}

	#ifdef __AMIGAOS4__
	if ((IAmiSSL = (struct AmiSSLIFace *)GetInterface(AmiSSLBase, "main", 1, NULL)) == NULL) {
		displayError("failed to get the interface of amissl.library");
		return RETURN_ERROR;
	}
	#endif
		
	amiSSLInitialized = TRUE;

	return createSSLContext();
}

/**
 * Post a chat message to OpenAI
 * @param conversation the conversation to post
 * @param model the model to use
 * @param openAiApiKey the OpenAI API key
 * @param stream whether to stream the response or not
 * @return a pointer to a new array of json_object containing the response(s) -- Free it with json_object_put() for all responses then FreeVec() for the array when you are done using it
**/
struct json_object** postMessageToOpenAI(struct MinList *conversation, enum Model model, CONST_STRPTR openAiApiKey, BOOL stream) {
	struct sockaddr_in addr;
	struct hostent *hostent;
	struct json_object **responses = AllocVec(sizeof(struct json_object *) * RESPONSE_ARRAY_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
	static BOOL streamingInProgress = FALSE;
	UWORD responseIndex = 0;

	memset(writeBuffer, 0, WRITE_BUFFER_LENGTH);

	if (!stream || !streamingInProgress) {
		memset(readBuffer, 0, READ_BUFFER_LENGTH);
		streamingInProgress = stream;
		if (ssl != NULL) {
			SSL_shutdown(ssl);
			SSL_free(ssl);
		}

		if (sock >- 0) {
			CloseSocket(sock);
		}

		struct json_object *obj = json_object_new_object();
		json_object_object_add(obj, "model", json_object_new_string(MODEL_NAMES[model]));
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

		json_object_object_add(obj, "stream", json_object_new_boolean((json_bool)stream));
		
		STRPTR jsonString = json_object_to_json_string(obj);

		snprintf(writeBuffer, WRITE_BUFFER_LENGTH, "POST /v1/chat/completions HTTP/1.1\r\n"
				"Host: api.openai.com\r\n"
				"Content-Type: application/json\r\n"
				"Authorization: Bearer %s\r\n"
				"User-Agent: AmigaGPT\r\n"
				"Content-Length: %lu\r\n\r\n"
				"%s", openAiApiKey, strlen(jsonString), jsonString);

		json_object_put(obj);

		updateStatusBar("Connecting...", 7);

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
				displayError("Host lookup failed");
				return NULL;
			}

			/* Create a socket and connect to the server */
			if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
				if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
					displayError("Couldn't connect to server");
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
				displayError("Couldn't connect to host!");
				return NULL;
			}
		} else {
			displayError("Couldn't create new SSL handle!");
			return NULL;
		}

		updateStatusBar("Sending request...", 7);

		ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));
	}

	if (ssl_err > 0 || stream) {
		ULONG totalBytesRead = 0;
		WORD bytesRead = 0;
		BOOL doneReading = FALSE;
		LONG err = 0;
		STRPTR statusMessage[64];
		while (!doneReading) {
			UBYTE *tempReadBuffer = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
			bytesRead = SSL_read(ssl, tempReadBuffer, READ_BUFFER_LENGTH);
			snprintf(statusMessage, 64, "Downloading response... (%lu bytes)", totalBytesRead);
			updateStatusBar(statusMessage, 7);
			strcat(readBuffer, tempReadBuffer);
			FreeVec(tempReadBuffer);
			err = SSL_get_error(ssl, bytesRead);
			switch (err) {
				case SSL_ERROR_NONE:
					totalBytesRead += bytesRead;
					const STRPTR jsonStart = stream ? "data: {" : "{";
					STRPTR jsonString = readBuffer;
					// Check for error in stream
 					if (stream && strstr(jsonString, jsonStart) == NULL) {
 						jsonString = strstr(jsonString, "{");
 						responses[0] = json_tokener_parse(jsonString);
						if (json_object_object_get_ex(responses[0], "error", NULL)) {
							streamingInProgress = FALSE;
							doneReading = TRUE;
							break;
						}
 					}
					STRPTR lastJsonString = jsonString;
					while (jsonString = strstr(jsonString, jsonStart)) {
						lastJsonString = jsonString;
						if (stream)
							jsonString += 6; // Get to the start of the JSON
						struct json_object *parsedResponse = json_tokener_parse(jsonString);
						if (parsedResponse != NULL) {
							responses[responseIndex++] = parsedResponse;
							if (!stream)
								break;
						} else if (!stream) {
							jsonString = NULL;
							break;
						}
					}
					
					if (stream) {
						if (json_tokener_parse(lastJsonString + 6) == NULL) {
							snprintf(readBuffer, READ_BUFFER_LENGTH, "%s\0", lastJsonString);
						} else {
							memset(readBuffer, 0, READ_BUFFER_LENGTH);
						}
						if (strstr(readBuffer, "data: [DONE]"))
							streamingInProgress = FALSE;
					} else if (json_tokener_parse(lastJsonString) == NULL) {
							snprintf(readBuffer, READ_BUFFER_LENGTH, "%s\0", lastJsonString);
							continue;
					}
					
					doneReading = TRUE;

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
		displayError("Couldn't write request!\n");
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
	if (!stream) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
	}
	return responses;
}

/**
 * Post a image creation request to OpenAI
 * @param prompt the prompt to use
 * @param imageModel the image model to use
 * @param openAiApiKey the OpenAI API key
 * @param stream whether to stream the response or not
 * @return a pointer to a new json_object containing the response -- Free it with json_object_put when you are done using it
**/
struct json_object* postImageCreationRequestToOpenAI(CONST_STRPTR prompt, enum ImageModel imageModel, UWORD width, CONST_STRPTR openAiApiKey) {
	struct sockaddr_in addr;
	struct hostent *hostent;
	struct json_object *response;
	static BOOL streamingInProgress = FALSE;
	UWORD responseIndex = 0;

	memset(writeBuffer, 0, WRITE_BUFFER_LENGTH);
	memset(readBuffer, 0, READ_BUFFER_LENGTH);

	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}

	if (sock >- 0) {
		CloseSocket(sock);
	}

	updateStatusBar("Connecting...", 7);

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, "model", json_object_new_string(IMAGE_MODEL_NAMES[imageModel]));
	json_object_object_add(obj, "prompt", json_object_new_string(prompt));

	UBYTE sizeString[10];
	snprintf(sizeString, 10, "%dx%d\0", width, width);
	json_object_object_add(obj, "size", json_object_new_string(sizeString));	
	STRPTR jsonString = json_object_to_json_string(obj);

	snprintf(writeBuffer, WRITE_BUFFER_LENGTH, "POST /v1/images/generations HTTP/1.1\r\n"
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
			displayError("Host lookup failed");
			return NULL;
		}

		/* Create a socket and connect to the server */
		if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
			if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				displayError("Couldn't connect to server");
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
			displayError("Couldn't connect to host!");
			return NULL;
		}
	} else {
		displayError("Couldn't create new SSL handle!");
		return NULL;
	}

	updateStatusBar("Sending request...", 7);

	ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));

	if (ssl_err > 0) {
		ULONG totalBytesRead = 0;
		WORD bytesRead = 0;
		BOOL doneReading = FALSE;
		LONG err = 0;
		STRPTR statusMessage[64];
		while (!doneReading) {
			UBYTE *tempReadBuffer = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
			bytesRead = SSL_read(ssl, tempReadBuffer, READ_BUFFER_LENGTH);
			snprintf(statusMessage, 64, "Downloading image... (%lu bytes)", totalBytesRead);
			updateStatusBar(statusMessage, 7);
			strcat(readBuffer, tempReadBuffer);
			FreeVec(tempReadBuffer);
			err = SSL_get_error(ssl, bytesRead);
			switch (err) {
				case SSL_ERROR_NONE:
					totalBytesRead += bytesRead;
					CONST_STRPTR jsonStart = "{";
					STRPTR jsonString = readBuffer;
					STRPTR lastJsonString = jsonString;
					while (jsonString = strstr(jsonString, jsonStart)) {
						lastJsonString = jsonString;

						struct json_object *parsedResponse = json_tokener_parse(jsonString);
						if (parsedResponse != NULL) {
							response = parsedResponse;
							break;
						} else {
							jsonString = NULL;
							break;
						}
					}
					
					if (json_tokener_parse(lastJsonString) == NULL) {
							snprintf(readBuffer, READ_BUFFER_LENGTH, "%s\0", lastJsonString);
							continue;
					}
					
					doneReading = TRUE;

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
		displayError("Couldn't write request!\n");
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

	SSL_shutdown(ssl);
	SSL_free(ssl);
	ssl = NULL;

	return response;
}

/**
 * Download a file from the internet
 * @param url the URL to download from
 * @param destination the destination to save the file to
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/ 
ULONG downloadFile(CONST_STRPTR url, CONST_STRPTR destination) {
	struct sockaddr_in addr;
	struct hostent *hostent;
	struct json_object *response;

	BPTR fileHandle = Open(destination, MODE_NEWFILE);
	if (fileHandle == NULL) {
		displayError("Couldn't open file for writing");
		return RETURN_ERROR;
	}

	APTR downloadBuffer = AllocVec(DOWNLOAD_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
	APTR writeBuffer = AllocVec(WRITE_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);

	if (downloadBuffer == NULL) {
        displayError("Couldn't allocate memory for download buffer");
        Close(fileHandle);
        return RETURN_ERROR;
    }

    UBYTE hostString[64];
    UBYTE pathString[2056];

    // Parse URL (very basic parsing, assuming URL starts with http:// or https://)
    CONST_STRPTR urlStart = strstr(url, "://");
    if (urlStart == NULL) {
        displayError("Invalid URL format");
        FreeVec(downloadBuffer);
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

    // Construct the HTTP GET request
    snprintf(writeBuffer, WRITE_BUFFER_LENGTH, "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n\r\n"
		"User-Agent: AmigaGPT\r\n"
		, pathString, hostString);

	/* The following needs to be done once per socket */
	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}

	if (sock >- 0) {
		CloseSocket(sock);
	}

	updateStatusBar("Connecting...", 7);

	if((ssl = SSL_new(ctx)) != NULL) {
		/* Lookup hostname */
		if ((hostent = gethostbyname(hostString)) != NULL) {
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(PORT);
			addr.sin_len = hostent->h_length;;
			memcpy(&addr.sin_addr,hostent->h_addr,hostent->h_length);
		}
		else {
			displayError("Host lookup failed");
			FreeVec(downloadBuffer);
			FreeVec(writeBuffer);
			Close(fileHandle);
			return NULL;
		}

		/* Create a socket and connect to the server */
		if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
			if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				displayError("Couldn't connect to server");
				FreeVec(downloadBuffer);
				FreeVec(writeBuffer);
				Close(fileHandle);
				return NULL;
			}
		}

		/* Check if connection was established */
		if (sock >= 0) {
			/* Associate the socket with the ssl structure */
			SSL_set_fd(ssl, sock);

			/* Set up SNI (Server Name Indication) */
			SSL_set_tlsext_host_name(ssl, hostString);

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
				FreeVec(downloadBuffer);
				FreeVec(writeBuffer);
				Close(fileHandle);
				return NULL;
			}
		} else {
			displayError("Couldn't connect to host!");
			FreeVec(downloadBuffer);
			FreeVec(writeBuffer);
			Close(fileHandle);
			return NULL;
		}
	} else {
		displayError("Couldn't create new SSL handle!");
		FreeVec(downloadBuffer);
		FreeVec(writeBuffer);
		Close(fileHandle);
		return NULL;
	}

	updateStatusBar("Sending request...", 7);
	ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));

	if (ssl_err > 0) {
		ULONG totalBytesRead = 0;
		WORD bytesRead = 0;
		BOOL doneReading = FALSE;
		LONG err = 0;
		STRPTR statusMessage[64];
		while (!doneReading) {
			UBYTE *tempReadBuffer = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY | MEMF_CLEAR);
			bytesRead = SSL_read(ssl, tempReadBuffer, READ_BUFFER_LENGTH - 1);
			snprintf(statusMessage, 64, "Downloaded... %lu bytes", totalBytesRead);
			updateStatusBar(statusMessage, 7);
			memcpy(downloadBuffer + totalBytesRead, tempReadBuffer, bytesRead);
			FreeVec(tempReadBuffer);
			err = SSL_get_error(ssl, bytesRead);
			switch (err) {
				case SSL_ERROR_NONE:
					totalBytesRead += bytesRead;
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
					doneReading = TRUE;
					break;
				default:
					printf("Unknown error\n");
					break;
			}            
		}

		APTR pngData = strstr(downloadBuffer, "\x89\x50\x4E\x47");
		if (pngData == NULL) {
			displayError("Couldn't find PNG header in response");
			FreeVec(downloadBuffer);
			FreeVec(writeBuffer);
			Close(fileHandle);
			return RETURN_ERROR;
		}
		Write(fileHandle, pngData, totalBytesRead - (pngData - downloadBuffer));
	} else {
		displayError("Couldn't write request!\n");
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
		FreeVec(downloadBuffer);
		FreeVec(writeBuffer);
		Close(fileHandle);
		return RETURN_ERROR;
	}

	SSL_shutdown(ssl);
	SSL_free(ssl);
	ssl = NULL;

	FreeVec(downloadBuffer);
	FreeVec(writeBuffer);
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
		if (ctx) {
			SSL_CTX_free(ctx);
			ctx = NULL;
		}
		BIO_free(bio_err);
		CleanupAmiSSLA(NULL);
	}

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

	sock = NULL;

	FreeVec(writeBuffer);
	FreeVec(readBuffer);
}
