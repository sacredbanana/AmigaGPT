#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <json-c/json.h>
#include <sys/socket.h>
#include <netdb.h>
#include "openai-key.h"

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 65536
#define OPENAI_HOST "api.openai.com"
#define OPENAI_PORT 443
#define AUDIO_BUFFER_SIZE 4096

#define UBYTE uint8_t
#define ULONG uint32_t
#define LONG int32_t
#define CONST_STRPTR const char*
#define APTR void*
#define MEMF_ANY 0
#define MEMF_CLEAR 0
#define AllocVec(size, flags) malloc(size)
#define FreeVec(ptr) free(ptr)
#define TRUE true
#define FALSE false
#define displayError(msg) printf("%s\n", msg)

uint8_t *writeBuffer = NULL;
uint8_t *readBuffer = NULL;
X509 *server_cert;
SSL_CTX *ctx = NULL;
BIO *bio, *bio_err;
SSL *ssl;
int sock = -1;
int ssl_err = 0;
uint32_t RangeSeed;

enum TTSVoice {
	ALLOY = 0,
	ECHO,
	FABLE,
	ONYX,
	NOVA,
	SHIMMER
};

/**
 * The Text to Speech model OpenAI should use
**/
enum TTSModel {
	TTS_1 = 0,
	TTS_1_HD
};


/**
 * The names of the TTS models
 * @see enum TTSModel
**/
const char* TTS_MODEL_NAMES[] = {
	[TTS_1] = "tts-1",
	[TTS_1_HD] = "tts-1-hd"
};

/**
 * The names of the TTS voices
 * @see enum TTSVoice
**/
const char* TTS_VOICE_NAMES[] = {
	[ALLOY] = "alloy",
	[ECHO] = "echo",
	[FABLE] = "fable",
	[ONYX] = "onyx",
	[NOVA] = "nova",
	[SHIMMER] = "shimmer"
};

static bool createSSLContext();
static SSL_verify_cb verify_cb(SSL_verify_cb preverify_ok, X509_STORE_CTX *ctx);
static void generateRandomSeed(uint8_t *buffer, uint32_t size);
static uint32_t rangeRand(uint32_t maxValue);
static bool createSSLConnection(const char *host, uint32_t port);
static uint32_t parseChunkLength(uint8_t *buffer);
u_int8_t* postTextToSpeechRequestToOpenAI(const char* text, enum TTSModel ttsModel, enum TTSVoice ttsVoice, const char *openAiApiKey, uint32_t *audioLength);

int main(int argc, char *argv[])
{
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *sbio;
    int len;
    char buf[1024];

    readBuffer = malloc(READ_BUFFER_LENGTH);
    writeBuffer = malloc(WRITE_BUFFER_LENGTH);

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    createSSLContext();

	CONST_STRPTR text = "The story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the namThe story behind the name \"Coles\" really ties back to its founder, George James Coles. When he opened his first store in Collingwood, Melbourne, in 1914, it was called the \"G.J. Coles & Coy Limited,\" using his initials and a traditional business suffix to create a personal yet professional brand name. This approach was fairly common at the time, personalizing businesses to associate them dir ectly with their founders' reputations for integrity and quality.Chunked encoding is useful when larger amounts of data are sent to the client and the total size of the response may not be known until the request has been fully processed. For example, when generating a large HTML table resulting from a database query or when transmitting large images.A chunked response looks like this:";

    postTextToSpeechRequestToOpenAI(text, TTS_1, ALLOY, OPENAI_API_KEY, &len);

    SSL_CTX_free(ctx);

    free(readBuffer);
    free(writeBuffer);

    return EXIT_SUCCESS;
}

static void generateRandomSeed(uint8_t *buffer, uint32_t size) {
	for(int i = 0; i < size/2; i++) {
		((uint16_t *)buffer)[i] = rangeRand(65535);
	}
}

/**
 * Generate a random number
 * @param maxValue the maximum value of the random number
 * @return a random number
**/
static uint32_t rangeRand(uint32_t maxValue) {
	uint32_t a=RangeSeed;
	uint32_t i=maxValue-1;
	do {
		uint32_t b=a;
		a<<=1;
		if((int32_t)b<=0)
		a^=0x1d872b41;
	} while((i>>=1));
	RangeSeed=a;
	if((uint16_t)maxValue)
		return (uint16_t)((uint16_t)a*(uint16_t)maxValue>>16);
  return (uint16_t)a;
}


/**
 * Create a new SSL context
 * @return RETURN_OK on success, RETURN_ERROR on failure
**/
static bool createSSLContext() {
	/* Basic intialization. Next few steps (up to SSL_new()) need
	 * to be done only once per AmiSSL opener.
	 */
	OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);

	/* Seed the entropy engine */
	generateRandomSeed(writeBuffer, 128);
	RAND_seed(writeBuffer, 128);

	/* Note: BIO writing routines are prepared for NULL BIO handle */
	if ((bio_err = BIO_new(BIO_s_file())) != NULL)
		BIO_set_fp(bio_err, stderr, BIO_NOCLOSE);

	/* Get a new SSL context */
	if ((ctx = SSL_CTX_new(TLS_client_method())) != NULL) {
		/* Basic certificate handling */
		SSL_CTX_set_default_verify_paths(ctx);
		// SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_cb);
	} else {
		printf("Couldn't create new context!\n");
		return false;
	}

	return true;
}

/**
 * This callback is called everytime OpenSSL verifies a certificate
 * in the chain during a connection, indicating success or failure.
 * @param preverify_ok 1 if the certificate passed verification, 0 otherwise
 * @param ctx the X509 certificate store context
 * @return 1 if the certificate passed verification, 0 otherwise
**/
static SSL_verify_cb verify_cb(SSL_verify_cb preverify_ok, X509_STORE_CTX *ctx) {
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
 * Create a new SSL connection to a host with a new socket
 * @param host the host to connect to
 * @param port the port to connect to
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/ 
static bool createSSLConnection(const char *host, uint32_t port) {
    struct sockaddr_in addr;
	struct hostent *hostent;

	if (ssl != NULL) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
	}

	if (sock > -1) {
		close(sock);
	}

	/* The following needs to be done once per socket */
	if((ssl = SSL_new(ctx)) != NULL) {
        /* Lookup hostname */
		if ((hostent = gethostbyname(host)) != NULL) {
			memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);  // Make sure port is passed in network byte order
            memcpy(&addr.sin_addr, hostent->h_addr, hostent->h_length);
		} else {
			 printf("Host lookup failed: %s\n", hstrerror(h_errno));
			SSL_shutdown(ssl);
			SSL_free(ssl);
			ssl = NULL;
			return false;
		}

		/* Create a socket and connect to the server */
		if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0)) {
			if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
 				printf("Couldn't connect to server");
                 perror("Connect failed");  // Print error message with perror
                printf("Error code: %d\n", errno);  // Optionally print the error code
                printf("Error description: %s\n", strerror(errno));
				close(sock);
				SSL_shutdown(ssl);
				SSL_free(ssl);
				ssl = NULL;
				sock = -1;
				return false;
			}
		} else {
			printf("Couldn't create socket");
			SSL_shutdown(ssl);
			SSL_free(ssl);
			ssl = NULL;
			return false;
		}

		/* Check if connection was established */
		if (sock >= 0) {
			/* Associate the socket with the ssl structure */
			SSL_set_fd(ssl, sock);

			/* Set up SNI (Server Name Indication) */
			SSL_set_tlsext_host_name(ssl, host);

			/* Perform SSL handshake */
			if((ssl_err = SSL_connect(ssl)) >= 0) {
				// printf("SSL connection to %s using %s\n\0", host, SSL_get_cipher(ssl));
			}
			
			/* If there were errors, print them */
			if (ssl_err < 0) {
				int err = SSL_get_error(ssl, ssl_err);
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
				close(sock);
				SSL_shutdown(ssl);
				SSL_free(ssl);
				ssl = NULL;
				sock = -1;
				return false;
			}
		} else {
			printf("Couldn't connect to host!\n");
			close(sock);
			SSL_shutdown(ssl);
			SSL_free(ssl);
			ssl = NULL;
			sock = -1;
			return false;
		}
	} else {
		printf("Couldn't create new SSL handle!\n");
		return false;
	}

	return true;
}


/**
 * Helper function to parse the chunk length
 * @param buffer the buffer to parse
 * @return the chunk length
 * @see https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
**/
static uint32_t parseChunkLength(uint8_t *buffer) {
    uint8_t chunkLenStr[10] = {0}; // Enough for the chunk length in hex
    uint8_t i = 0;
    
    // Loop until we find the CRLF which ends the chunk length line
    while (i < 8 && buffer[i] != '\r' && buffer[i+1] != '\n') {
        chunkLenStr[i] = buffer[i];
		printf("chunkLenStr[%d]: %x %d %p %x\n", i, chunkLenStr[i], chunkLenStr[i], chunkLenStr[i], chunkLenStr[i]);
        i++;
    }

    if (i == 8) {
        printf("Couldn't find CRLF in chunk length line\n");
        return 0;
    } else if (i == 0) {
        printf("Couldn't find chunk length\n");
        return 0;
    }
	
    // Convert hex string to unsigned long
    return strtoul(chunkLenStr, NULL, 16);
}

u_int8_t* postTextToSpeechRequestToOpenAI(const char* text, enum TTSModel ttsModel, enum TTSVoice ttsVoice, const char *openAiApiKey, uint32_t *audioLength) {
	// Allocate a buffer for the audio data. This buffer will be resized if needed
	ULONG audioBufferSize = AUDIO_BUFFER_SIZE;
	UBYTE *audioData = AllocVec(audioBufferSize, MEMF_ANY);

    u_int8_t readBuffer[READ_BUFFER_LENGTH];

	FILE *file2 = fopen("/tmp/rawdata", "wb");  
    if (file2 == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

	struct json_object *response;

	*audioLength = 0;

	memset(readBuffer, 0, READ_BUFFER_LENGTH);

	printf("Connecting...\n");
	if (createSSLConnection(OPENAI_HOST, OPENAI_PORT) == false) {
		return NULL;
	}

	struct json_object *obj = json_object_new_object();
	json_object_object_add(obj, (const char*)"model", json_object_new_string(TTS_MODEL_NAMES[ttsModel]));
	json_object_object_add(obj, (const char*)"voice", json_object_new_string(TTS_VOICE_NAMES[ttsVoice]));
	json_object_object_add(obj, (const char*)"input", json_object_new_string(text));
	json_object_object_add(obj, (const char*)"response_format", json_object_new_string("pcm"));
	const char *jsonString = json_object_to_json_string(obj);

	snprintf(writeBuffer, WRITE_BUFFER_LENGTH, "POST /v1/audio/speech HTTP/1.1\r\n"
			"Host: api.openai.com\r\n"
			"Content-Type: application/json\r\n"
			"Authorization: Bearer %s\r\n"
			"User-Agent: AmigaGPT\r\n"
			"Content-Length: %lu\r\n\r\n"
			"%s\0", openAiApiKey, strlen(jsonString), jsonString);

	json_object_put(obj);

	printf("Sending request..\n");
	ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer));

	if (ssl_err > 0) {
		int bytesRead = 0;
        int bytesRemainingInBuffer = 0;
		bool doneReading = false;
		int err = 0;
		char statusMessage[64];
        uint8_t tempChunkHeaderBuffer[10] = {0};
        uint8_t tempChunkDataBufferLength = 0;
		uint8_t *tempReadBuffer = malloc(READ_BUFFER_LENGTH);
		bool hasReadHeader = false;
		bool newChunkNeeded = true;
		uint32_t chunkLength = 0;
		uint32_t chunkBytesNeedingRead = 0;
		uint8_t *dataStart = NULL;

		while (!doneReading) {
            memset(tempReadBuffer, 0, READ_BUFFER_LENGTH);
			bytesRead = SSL_read(ssl, tempReadBuffer, READ_BUFFER_LENGTH - 1);
			printf("Read %lu bytes\n", bytesRead);
			if (newChunkNeeded && bytesRead == 1) continue;
			if (bytesRead == 5) {
				printf("%x %x %x %x %x\n", tempReadBuffer[0], tempReadBuffer[1], tempReadBuffer[2], tempReadBuffer[3], tempReadBuffer[4]);
			}

			fwrite(tempReadBuffer, sizeof(uint8_t), bytesRead, file2);
            bytesRemainingInBuffer = bytesRead;
			dataStart = tempReadBuffer;
			
			snprintf(statusMessage, sizeof(statusMessage), "Downloaded %lu bytes\n", *audioLength);
			printf(statusMessage);
			err = SSL_get_error(ssl, bytesRead);
			switch (err) {
				case SSL_ERROR_NONE:
					while (bytesRemainingInBuffer > 0) {
						if (!hasReadHeader) {
							dataStart = strstr(tempReadBuffer, "\r\n\r\n");
							if (dataStart != NULL) {
								hasReadHeader = TRUE;
								dataStart += 4;
                                memcpy(tempChunkHeaderBuffer + tempChunkDataBufferLength, tempReadBuffer, 10 - tempChunkDataBufferLength);
								chunkLength = parseChunkLength(dataStart);
								chunkBytesNeedingRead = chunkLength;
								dataStart = strstr(dataStart, "\r\n") + 2;
								bytesRemainingInBuffer -= (dataStart - tempReadBuffer);
							} else {
								continue;
							}
						} else {
							if (newChunkNeeded) {
								printf("New chunk needed\n");
								chunkLength = parseChunkLength(dataStart);
								printf("Chunk length: %lu\n", chunkLength);
								if (chunkLength == 0) {
									doneReading = TRUE;
									break;
								}
								chunkBytesNeedingRead = chunkLength;
								UBYTE *oldDataStart = dataStart;
								dataStart = strstr(dataStart, "\r\n") + 2;
								if (dataStart == NULL) {
									printf("Couldn't find CRLF in chuuuuunk\n");
								}
								bytesRemainingInBuffer -= (dataStart - oldDataStart);
							}
						}

						// Create a larger audio buffer if needed
						if (*audioLength + chunkBytesNeedingRead > audioBufferSize) {
							audioBufferSize <<= 1;
							APTR oldAudioData = audioData;
							audioData = AllocVec(audioBufferSize, MEMF_ANY);
							if (audioData == NULL) {
								FreeVec(oldAudioData);
								displayError("Not enough memory for audio buffer");
								return NULL;
							}
							memcpy(audioData, oldAudioData, *audioLength);
							FreeVec(oldAudioData);
							printf("Resized audio buffer to %lu bytes\n", audioBufferSize);
						}

						printf("Chunk bytes needing read: %lu\n", chunkBytesNeedingRead);
						printf("Bytes remaining in buffer: %lu\n", bytesRemainingInBuffer);

						if (bytesRemainingInBuffer - chunkBytesNeedingRead == 2) {
							printf("2\n");
							// memcpy(audioData + *audioLength, dataStart, chunkBytesNeedingRead);
							// *audioLength += chunkBytesNeedingRead;
							// bytesRemainingInBuffer = 0;
							// doneReading = true;
							// break;
						}

						if (chunkBytesNeedingRead > bytesRemainingInBuffer) {	
							memcpy(audioData + *audioLength, dataStart, bytesRemainingInBuffer);
							*audioLength += bytesRemainingInBuffer;
							chunkBytesNeedingRead -= bytesRemainingInBuffer;
							newChunkNeeded = FALSE;
                            bytesRemainingInBuffer = 0;
							printf("Buffer empty. Chunk bytes still needing read: %lu\n", chunkBytesNeedingRead);
						} else {
							memcpy(audioData + *audioLength, dataStart, chunkBytesNeedingRead);
							*audioLength += chunkBytesNeedingRead;
							// if (chunkBytesNeedingRead == bytesRead) {
							// 	// We have the rest of the chunk but we don't have the closing CRLF
							// 	bytesRemainingInBuffer = 0;
							// 	bytesRead = SSL_read(ssl, tempReadBuffer, 2);
							// 	printf("Want to read 2 bytes. Read %lu bytes\n", bytesRead);
							// } else {
								bytesRemainingInBuffer -= chunkBytesNeedingRead;
								while (bytesRemainingInBuffer < 2) {
									bytesRead = SSL_read(ssl, tempReadBuffer, 1);
									printf("Want to read 1 byte. Read %lu bytes\n", bytesRead);
									printf("Read a %x\n", tempReadBuffer[0]);
									bytesRemainingInBuffer += bytesRead;
								}
								dataStart += chunkBytesNeedingRead + 2;
								bytesRemainingInBuffer -= 2;
								chunkBytesNeedingRead = 0;
							// }
							printf("New chunk bytes needing read: %lu\n", chunkBytesNeedingRead);
							printf("New bytes remaining in buffer: %lu\n", bytesRemainingInBuffer);
							newChunkNeeded = TRUE;
						}
					}
					break;
				case SSL_ERROR_ZERO_RETURN:
					printf("SSL_ERROR_ZERO_RETURN\n");
					doneReading = true;
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
					// int err = SSL_get_error();
					// printf("error: %lu\n", err);
				case SSL_ERROR_SSL:
					printf("Lost connection.");
					close(sock);
					SSL_shutdown(ssl);
					SSL_free(ssl);
					ssl = NULL;
					sock = -1;;
					doneReading = true;
					break;
				default:
					printf("Unknown error\n");
					break;
			}            
		}
		free(tempReadBuffer);
	} else {
		printf("Couldn't write request!\n");
		int err = SSL_get_error(ssl, ssl_err);
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
		return NULL;
	}

	close(sock);
	SSL_shutdown(ssl);
	SSL_free(ssl);
	ssl = NULL;
	sock = -1;

	printf("Download complete.\n");

    // write audioData to file
    FILE *file = fopen("/tmp/audio.pcm", "wb");  
    if (file == NULL) {
        printf("Error opening file!\n");
        exit(1);
    }

    fwrite(audioData, sizeof(uint8_t), *audioLength, file);
    fclose(file);
	fclose(file2);

	return audioData;
}