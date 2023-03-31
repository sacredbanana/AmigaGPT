#include "support/gcc8_c_support.h"
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
#include "speech.h"
#include "api_key.h"

#define GETINTERFACE(iface, base) TRUE
#define DROPINTERFACE(iface)

static void cleanup(void);
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG connectToServer(UBYTE *, UWORD, UBYTE *, UWORD);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);
static UBYTE* getResponseFromJson(UBYTE *json, LONG jsonLength, LONG *responseLength);
static void replaceWithRealNewLines(UBYTE *content, LONG *stringLength);
static LONG endOfResponse(UBYTE *response);

struct Library *AmiSSLMasterBase, *AmiSSLBase, *SocketBase;
struct UtilityBase *UtilityBase;
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
static SSL_CTX *_ssl_context;
LONG UsesOpenSSLStructs = FALSE;
BOOL amiSSLInitialized = FALSE;
UBYTE *writeBuffer = NULL;
UBYTE *readBuffer = NULL;
UBYTE *printText = NULL;
X509 *server_cert;
SSL_CTX *ctx;
BIO *bio, *bio_err;
SSL *ssl;
int sock = -1;
int ssl_err = 0;

ULONG RangeSeed;

ULONG rangeRand(ULONG maxValue)
{ ULONG a=RangeSeed;
  UWORD i=maxValue-1;
  do
  { ULONG b=a;
    a<<=1;
    if((LONG)b<=0)
      a^=0x1d872b41;
  }while((i>>=1));
  RangeSeed=a;
  if((UWORD)maxValue)
    return (UWORD)((UWORD)a*(UWORD)maxValue>>16);
  return (UWORD)a;
}

static BPTR ErrorOutput(void)
{
	return(((struct Process *)FindTask(NULL))->pr_CES);
}

static BPTR GetStdErr(void)
{
	BPTR err = ErrorOutput();

	return(err ? err : Output());
}

/* Usage: https <host> <port> [proxyhost] [proxyport]
 *
 * host:      name of host (default: "localhost")
 * port:      port to connect to (default: 443)
 * proxyhost: name of proxy (optional)
 * proxyport: name of proxy (optional)
 *
 * If any proxy parameter is omitted, the program will
 * connect directly to the host.
 */
LONG initOpenAIConnector() {
    long errno = 0;

    printText = AllocVec(PRINT_BUFFER_LENGTH, MEMF_ANY);
    if (printText == NULL) {
        return RETURN_ERROR;
    }
    readBuffer = AllocVec(READ_BUFFER_LENGTH, MEMF_ANY);
    if (readBuffer == NULL) {
        return RETURN_ERROR;
    }
    writeBuffer = AllocVec(WRITE_BUFFER_LENGTH, MEMF_ANY);
    if (writeBuffer == NULL) {
        return RETURN_ERROR;
    }

	if ((UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library", 0)) == NULL)
		return RETURN_ERROR;

    sprintf(printText,"opened utility.library\n");
	Write(Output(), (APTR)printText, strlen(printText));

	if ((SocketBase = OpenLibrary("bsdsocket.library", 0)) == NULL)
		return RETURN_ERROR;

    sprintf(printText, "opened bsdsocket.library\n");
	Write(Output(), (APTR)printText, strlen(printText));

	if ((AmiSSLMasterBase = OpenLibrary("AMISSL:libs/amisslmaster.library", AMISSLMASTER_MIN_VERSION)) == NULL)
		return RETURN_ERROR;

    sprintf(printText, "opened amisslmaster.library\n");
	Write(Output(), (APTR)printText, strlen(printText));

    if (!GETINTERFACE(IAmiSSLMaster, AmiSSLMasterBase)) {
        return RETURN_ERROR;
    }

    sprintf(printText, "got amisslmaster.library interface\n");
	Write(Output(), (APTR)printText, strlen(printText));
    
    if (InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE) == FALSE) {
        return RETURN_ERROR;
    }
		
    sprintf(printText, "initialized amisslmaster.library\n");
	Write(Output(), (APTR)printText, strlen(printText));

	if ((AmiSSLBase = OpenAmiSSL()) == NULL)
		return RETURN_ERROR;

    sprintf(printText, "opened amissl.library\n");
	Write(Output(), (APTR)printText, strlen(printText));

    if(InitAmiSSL(AmiSSL_ErrNoPtr, &errno, AmiSSL_SocketBase, SocketBase, TAG_DONE) != 0) {
        return RETURN_ERROR;
    }

    sprintf(printText, "initialized amissl.library\n");
	Write(Output(), (APTR)printText, strlen(printText));

	amiSSLInitialized = TRUE;
    return RETURN_OK;
}

static void replaceWithRealNewLines(UBYTE *string, LONG *stringLength) {
    LONG newStringIndex = 0;
    const LONG oldStringLength = *stringLength;
    for (LONG oldStringIndex = 0; oldStringIndex < oldStringLength; oldStringIndex++) {
        if (string[oldStringIndex] == '\\' && string[oldStringIndex + 1] == 'n') {
            string[newStringIndex++] = '\n';
            oldStringIndex++;
        } else {
            string[newStringIndex++] = string[oldStringIndex];
        }
    }
    string[newStringIndex++] = '\0';
    *stringLength = newStringIndex;
}    

UBYTE* postMessageToOpenAI(UBYTE *content, UBYTE *model, UBYTE *role) {
    // Compose the POST request
    UBYTE auth[512];
    sprintf(auth, "Authorization: Bearer %s", openAiApiKey);

    sprintf(printText, "Auth: \n");
    Write(Output(), (APTR)printText, strlen(printText));
    Write(Output(), (APTR)auth, strlen(auth));
    Delay(50);

    UBYTE body[1024];
    sprintf(body,
            "{\"model\": \"%s\",\r\n"
            "\"messages\": [{\"role\": \"%s\", \"content\": \"%s\"}]\r\n"
            "}\0",
            model, role, content);
    ULONG bodyLength = strlen(body);

    sprintf(printText, "Body: \n");
    Write(Output(), (APTR)printText, strlen(printText));
    Write(Output(), (APTR)body, strlen(body));
    Delay(50);

    UBYTE headers[] = "POST /v1/chat/completions HTTP/1.1\r\n"
            "Host: api.openai.com\r\n"
            "Content-Type: application/json\r\n";
    sprintf(headers, "%s%s\r\n", headers, auth);
    sprintf(headers, "%sContent-Length:", headers);

    sprintf(printText, "Headers: \n");
    Write(Output(), (APTR)printText, strlen(printText));
    Write(Output(), (APTR)headers, strlen(auth));
    Delay(50);

    sprintf(writeBuffer, "%s ", headers);
    sprintf(writeBuffer, "%s%lu\r\n\r\n", writeBuffer, bodyLength);
    sprintf(writeBuffer, "%s%s", writeBuffer, body);

    memclr(readBuffer, READ_BUFFER_LENGTH);
    LONG endOfResponseIndex = -1;

    sprintf(printText, "Write buffer: \n");
    Write(Output(), (APTR)printText, strlen(printText));
    Write(Output(), (APTR)writeBuffer, strlen(writeBuffer));
    Delay(50);

    if (ssl == NULL) {
        sprintf(printText, "SSL is NULL!\n");
        Write(Output(), (APTR)printText, strlen(printText));
    } else {
        sprintf(printText, "SSL is not NULL!\n");
        Write(Output(), (APTR)printText, strlen(printText));
    }


    if ((ssl_err = SSL_write(ssl, writeBuffer, strlen(writeBuffer))) > 0) {
        /* Dump everything to output */
        UWORD readBufferIndex = 0;
        while ((ssl_err = SSL_read(ssl, readBuffer, READ_BUFFER_LENGTH)) > 0) {
            endOfResponseIndex = endOfResponse(readBuffer);
            if (endOfResponseIndex != -1) {
                break;
            }
        }

        LONG responseLength = 0;
        UBYTE *response = getResponseFromJson(readBuffer, endOfResponseIndex + 1, &responseLength);
        replaceWithRealNewLines(response, &responseLength);
        return response;
    }
    else {
        sprintf(printText, "Couldn't write request!\n");
        Write(Output(), (APTR)printText, strlen(printText));
        WORD err = SSL_get_error(ssl, ssl_err);
        switch (err) {
            case SSL_ERROR_WANT_READ:
                sprintf(printText, "SSL_ERROR_WANT_READ\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            case SSL_ERROR_WANT_WRITE:
                sprintf(printText, "SSL_ERROR_WANT_WRITE\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            case SSL_ERROR_WANT_CONNECT:
                sprintf(printText, "SSL_ERROR_WANT_CONNECT\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            case SSL_ERROR_WANT_ACCEPT:
                sprintf(printText, "SSL_ERROR_WANT_ACCEPT\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            case SSL_ERROR_WANT_X509_LOOKUP:
                sprintf(printText, "SSL_ERROR_WANT_X509_LOOKUP\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            case SSL_ERROR_SYSCALL:
                sprintf(printText, "SSL_ERROR_SYSCALL\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            case SSL_ERROR_SSL:
                sprintf(printText, "SSL_ERROR_SSL\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
            default:
                sprintf(printText, "Unknown error!\n");
                Write(Output(), (APTR)printText, strlen(printText));
                break;
        }
        ERR_print_errors(bio_err);
    }
    return NULL;
}

static LONG endOfResponse(UBYTE *response) {
    const UBYTE substring[] = "finish_reason\":\"stop\",\"index\":0}]}";
    int i = 0, j = 0;

    while (response[i] != '\0') {
        if (response[i] == substring[j]) {
            j++;
            if (substring[j] == '\0') {
                return i;
            }
        } else {
            j = 0;
        }
        i++;
    }

    return -1;
}

static UBYTE* getResponseFromJson(UBYTE *json, LONG jsonLength, LONG *responseLength) {
    const UBYTE content[] = "\"content\":\"";
    UBYTE matchedIndex = 0;
    const ULONG contentLength = strlen(content);
    UBYTE braceDepth = 0;
    *responseLength = 0;

    for (ULONG i = 0; i < jsonLength; i++) {
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
            for (ULONG j = i; j < jsonLength; j++) {
                if (json[j + 1] == '\"') {
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

LONG connectToOpenAI() {
    BOOL ok = TRUE;
    /* Basic intialization. Next few steps (up to SSL_new()) need
     * to be done only once per AmiSSL opener.
     */
    OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);

    sprintf(printText, "initialized openssl\n");
    Write(Output(), (APTR)printText, strlen(printText));

    /* Seed the entropy engine */
    generateRandomSeed(writeBuffer, 128);
    RAND_seed(writeBuffer, 128);

    sprintf(printText, "seeded random\n");
    Write(Output(), (APTR)printText, strlen(printText));

    /* Note: BIO writing routines are prepared for NULL BIO handle */
    if((bio_err = BIO_new(BIO_s_file())) != NULL)
        BIO_set_fp_amiga(bio_err, GetStdErr(), BIO_NOCLOSE | BIO_FP_TEXT);

    sprintf(printText, "created bio\n");
    Write(Output(), (APTR)printText, strlen(printText));

    /* Get a new SSL context */
    if((ctx = SSL_CTX_new(TLS_client_method())) != NULL)
    {
        sprintf(printText, "created ssl context\n");
        Write(Output(), (APTR)printText, strlen(printText));

        /* Basic certificate handling. OpenSSL documentation has more
            * information on this.
            */
        SSL_CTX_set_default_verify_paths(ctx);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                            verify_cb);

        sprintf(printText, "set ssl context\n");
        Write(Output(), (APTR)printText, strlen(printText));

        /* The following needs to be done once per socket */
        if((ssl = SSL_new(ctx)) != NULL)
        {
            sprintf(printText, "created ssl\n");
            Write(Output(), (APTR)printText, strlen(printText));

            LONG sock;
            LONG port, pport;
            UBYTE *host, *proxy;

            /* Connect to the HTTPS server, directly or through a proxy */
            host = "api.openai.com";
            port = 443;
            proxy = NULL;
            pport = 0;

            sock = connectToServer(host, port, proxy, pport);

            /* Check if connection was established */
            if (sock >= 0)
            {
                sprintf(printText, "connected to server\n");
                Write(Output(), (APTR)printText, strlen(printText));

                /* Associate the socket with the ssl structure */
                SSL_set_fd(ssl, sock);

                /* Set up SNI (Server Name Indication) */
                SSL_set_tlsext_host_name(ssl, host);

                /* Perform SSL handshake */
                if((ssl_err = SSL_connect(ssl)) >= 0) {
                    sprintf(printText, "SSL connection to %s using %s\n\0", host, SSL_get_cipher(ssl));
                    Write(Output(), (APTR)printText, strlen(printText));

                    // /* Certificate checking. This example is *very* basic */
                    // if((server_cert = SSL_get_peer_certificate(ssl)))
                    // {
                    //     UBYTE *str;

                    //     sprintf(printText, "Server certificate:\n");
                    //     Write(Output(), (APTR)printText, strlen(printText));
                    //     Delay(50);

                    //     if((str = X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0)))
                    //     {
                    //         sprintf(printText, "\tSubject: %s\n", str);
                    //         Write(Output(), (APTR)printText, strlen(printText));
                    //         Delay(50);
                    //         OPENSSL_free(str);
                    //     }
                    //     else {
                    //         sprintf(printText, "Warning: couldn't read subject name in certificate!\n");
                    //         Write(Output(), (APTR)printText, strlen(printText));
                    //         Delay(50);
                    //     }

                    //     if((str = X509_NAME_oneline(X509_get_issuer_name(server_cert),
                    //                                 0, 0)) != NULL)
                    //     {
                    //         sprintf(printText, "\tIssuer: %s\n", str);
                    //         Write(Output(), (APTR)printText, strlen(printText));
                    //         Delay(50);
                    //         OPENSSL_free(str);
                    //     }
                    //     else {
                    //         sprintf(printText, "Warning: couldn't read issuer name in certificate!\n");
                    //         Write(Output(), (APTR)printText, strlen(printText));
                    //         Delay(50);
                    //     }

                    //     X509_free(server_cert);
                    // }
                    // else {
                    //     sprintf(printText, "Couldn't get server certificate!\n");
                    //     Write(Output(), (APTR)printText, strlen(printText));
                    // }                    
                }
                else {
                    sprintf(printText, "Couldn't establish SSL connection!\n");
                    Write(Output(), (APTR)printText, strlen(printText));
                }
                
                /* If there were errors, print them */
                if (ssl_err < 0) {
                    sprintf(printText, "SSL error: %d\n", ssl_err);
                    Write(Output(), (APTR)printText, strlen(printText));
                    ERR_print_errors(bio_err);
                }
            }
            else {
                sprintf(printText, "Couldn't connect to host!\n");
                Write(Output(), (APTR)printText, strlen(printText));
            }
        }
        else {
            sprintf(printText, "Couldn't create new SSL handle!\n");
            Write(Output(), (APTR)printText, strlen(printText));
        }
    }
    else {
        sprintf(printText, "Couldn't create new context!\n");
        Write(Output(), (APTR)printText, strlen(printText));
    }

	return(ok ? RETURN_OK : RETURN_ERROR);
}

/* Get some suitable random seed data
 */
static void generateRandomSeed(UBYTE *buffer, LONG size)
{
	for(int i = 0; i < size/2; i++) {
		((UWORD *)buffer)[i] = rangeRand(65535);
	}
}

/* This callback is called everytime OpenSSL verifies a certificate
 * in the chain during a connection, indicating success or failure.
 */
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx)
{
	if (!preverify_ok)
	{
		/* Here, you could ask the user whether to ignore the failure,
		 * displaying information from the certificate, for example.
		 */
		// FPrintf(GetStdErr(),"Certificate verification failed (%s)\n",
		//         X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx)));
	}
	else
	{
		// FPrintf(GetStdErr(),"Certificate verification successful (hash %08lx)\n",
		//         X509_issuer_and_serial_hash(X509_STORE_CTX_get_current_cert(ctx)));
	}
	return preverify_ok;
}

/* Connect to the specified server, either directly or through the specified
 * proxy using HTTP CONNECT method.
 */
static LONG connectToServer(UBYTE *host, UWORD port, UBYTE *proxy, UWORD pport)
{
	struct sockaddr_in addr;
	struct hostent *hostent;
	BOOL ok = FALSE;
	char *s1, *s2;

	/* Lookup hostname */
	if ((hostent = gethostbyname((proxy && pport) ? proxy : host)) != NULL)
	{
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons((proxy && pport) ? pport : port);
		addr.sin_len = hostent->h_length;;
		memcpy(&addr.sin_addr,hostent->h_addr,hostent->h_length);
	}
	else {
        sprintf(printText, "Host lookup failed\n");
        Write(Output(), (APTR)printText, strlen(printText));
    }

	/* Create a socket and connect to the server */
	if (hostent && ((sock = socket(AF_INET, SOCK_STREAM, 0)) >= 0))
	{
		if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) >= 0)
		{
			/* For proxy connection, use SSL tunneling. First issue a HTTP CONNECT
			 * request and then proceed as with direct HTTPS connection.
			 */
			if (proxy && pport)
			{
				/* This should be done with snprintf to prevent buffer
				 * overflows, but some compilers don't have it and
				 * handling that would be an overkill for this example
				 */
				sprintf(writeBuffer, "CONNECT %s:%ld HTTP/1.0\r\n\r\n",
				        host, (long)port);

				/* In a real application, it would be necessary to loop
				 * until everything is sent or an error occurrs, but here we
				 * hope that everything gets sent at once.
				 */
				if (send(sock, writeBuffer, strlen(writeBuffer), 0) >= 0)
				{
					int len;

					/* Again, some optimistic behaviour: HTTP response might not be
					 * received with only one recv
					 */
					if ((len = recv(sock, writeBuffer, sizeof(writeBuffer) - 1, 0)) >= 0)
					{
						/* Assuming it was received, find the end of
						 * the line and cut it off
						 */
						// if ((s1 = strchr(buffer, '\r'))
						//     || (s1 = strchr(buffer, '\n')))
						// 	*s1 = '\0';
						// else
						// 	buffer[len] = '\0';

						// Printf("Proxy returned: %s\n", buffer);

						/* Check if HTTP response makes sense */
						if (strncmp(writeBuffer, "HTTP/", 4) == 0)
						    // && (s1 = strchr(buffer, ' '))
						    // && (s2 = strchr(++s1, ' '))
						    // && (s2 - s1 == 3))
						{
							/* Only accept HTTP 200 OK response */
							if (atol(s1) == 200)
								ok = TRUE;
							else {
                                // FPrintf(GetStdErr(), "Proxy responce indicates error!\n");
                            }
						}
						else {
                                // FPrintf(GetStdErr(), "Amibigous proxy responce!\n");
                         }
					}
					else {
                        // FPrintf(GetStdErr(), "Couldn't get proxy response!\n");
                    }
				}
				else {
                    // FPrintf(GetStdErr(), "Couldn't send request to proxy!\n");
                }
			}
			else {
                ok = TRUE;
            }
		}
		else {
            // FPrintf(GetStdErr(), "Couldn't connect to server\n");
        }

		if (!ok) {
			CloseSocket(sock);
			sock = -1;
		}
	}

	return(sock);
}

void closeOpenAIConnector()
{
	if (amiSSLInitialized)
	{	/* Must always call after successful InitAmiSSL() */
        SSL_shutdown(ssl);
        SSL_free(ssl);
        CloseSocket(sock);
        SSL_CTX_free(ctx);
        BIO_free(bio_err);
		CleanupAmiSSLA(NULL);
	}

	if (AmiSSLBase)
	{
		DROPINTERFACE(IAmiSSL);
		CloseAmiSSL();
		AmiSSLBase = NULL;
	}

	DROPINTERFACE(IAmiSSLMaster);
	CloseLibrary(AmiSSLMasterBase);
	AmiSSLMasterBase = NULL;

	DROPINTERFACE(ISocket);
	CloseLibrary(SocketBase);
	SocketBase = NULL;

	DROPINTERFACE(IUtility);
	CloseLibrary((struct Library *)UtilityBase);
	UtilityBase = NULL;

    FreeMem(printText, PRINT_BUFFER_LENGTH);
    FreeMem(writeBuffer, WRITE_BUFFER_LENGTH);
    FreeMem(readBuffer, READ_BUFFER_LENGTH);
}
