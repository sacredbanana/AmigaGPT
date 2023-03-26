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
// #include <stdio.h>
#include <string.h>
#include "openai.h"
#include <stdbool.h>
#include "speech.h"

#define GETINTERFACE(iface, base) TRUE
#define DROPINTERFACE(iface)

#define OPENAI_API_KEY "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"

static void cleanup(void);
static void generateRandomSeed(UBYTE *buffer, LONG size);
static LONG connectToServer(UBYTE *, UWORD, UBYTE *, UWORD);
static LONG verify_cb(LONG preverify_ok, X509_STORE_CTX *ctx);

struct Library *AmiSSLMasterBase, *AmiSSLBase, *SocketBase;
struct UtilityBase *UtilityBase;
extern struct ExecBase *SysBase;
extern struct DosLibrary *DOSBase;
static SSL_CTX *_ssl_context;
LONG UsesOpenSSLStructs = FALSE;
BOOL amiSSLInitialized = FALSE;

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

char buffer[4096]; /* This should be dynamically allocated */
X509 *server_cert;
SSL_CTX *ctx;
BIO *bio_err;
SSL *ssl;

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

	if ((UtilityBase = OpenLibrary("utility.library", 0)) == NULL)
		return RETURN_ERROR;

    UBYTE text[] = "opened utility.library\n";
	Write(Output(), (APTR)text, strlen(text));
	Delay(50);

	if ((SocketBase = OpenLibrary("bsdsocket.library", 0)) == NULL)
		return RETURN_ERROR;

    UBYTE text2[] = "opened bsdsocket.library\n";
	Write(Output(), (APTR)text2, strlen(text2));
	Delay(50);

	if ((AmiSSLMasterBase = OpenLibrary("AMISSL:libs/amisslmaster.library", AMISSLMASTER_MIN_VERSION)) == NULL)
		return RETURN_ERROR;

    UBYTE text3[] = "opened amisslmaster.library\n";
	Write(Output(), (APTR)text3, strlen(text3));
	Delay(50);

    if (!GETINTERFACE(IAmiSSLMaster, AmiSSLMasterBase)) {
        return RETURN_ERROR;
    }

    UBYTE text366[] = "got amisslmaster.library interface\n";
	Write(Output(), (APTR)text366, strlen(text366));
	Delay(50);
    
    if (InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE) == FALSE) {
        return RETURN_ERROR;
    }
		
    UBYTE text4[] = "initialized amisslmaster.library\n";
	Write(Output(), (APTR)text4, strlen(text4));
	Delay(50);

	if ((AmiSSLBase = OpenAmiSSL()) == NULL)
		return RETURN_ERROR;

    UBYTE text555[] = "opened amissl.library\n";
	Write(Output(), (APTR)text555, strlen(text555));
	Delay(50);

    if(InitAmiSSL(AmiSSL_ErrNoPtr, &errno, AmiSSL_SocketBase, SocketBase, TAG_DONE) != 0) {
        cleanup();
        return RETURN_ERROR;
    }

    UBYTE text6[] = "initialized amissl.library\n";
	Write(Output(), (APTR)text6, strlen(text6));
	Delay(50);

	amiSSLInitialized = TRUE;
    return RETURN_OK;
}

LONG connectToOpenAI() {
    BOOL ok = TRUE;
    /* Basic intialization. Next few steps (up to SSL_new()) need
		 * to be done only once per AmiSSL opener.
		 */
		OPENSSL_init_ssl(OPENSSL_INIT_SSL_DEFAULT | OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS, NULL);

		/* Seed the entropy engine */
		generateRandomSeed(buffer, 128);
		RAND_seed(buffer, 128);

		/* Note: BIO writing routines are prepared for NULL BIO handle */
		if((bio_err = BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp_amiga(bio_err, GetStdErr(), BIO_NOCLOSE | BIO_FP_TEXT);

		/* Get a new SSL context */
		if((ctx = SSL_CTX_new(TLS_client_method())) != NULL)
		{
			/* Basic certificate handling. OpenSSL documentation has more
			 * information on this.
			 */
			SSL_CTX_set_default_verify_paths(ctx);
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			                   verify_cb);

			/* The following needs to be done once per socket */
			if((ssl = SSL_new(ctx)) != NULL)
			{
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
					int ssl_err = 0;

					/* Associate the socket with the ssl structure */
					SSL_set_fd(ssl, sock);

					/* Set up SNI (Server Name Indication) */
					SSL_set_tlsext_host_name(ssl, host);

					/* Perform SSL handshake */
					if((ssl_err = SSL_connect(ssl)) >= 0)
					{
                        UBYTE text[50];
                        sprintf(text, "SSL connection to %s using %s\n", host, SSL_get_cipher(ssl));
                        Write(Output(), (APTR)text, strlen(text));
                        Delay(50);

						/* Certificate checking. This example is *very* basic */
						if((server_cert = SSL_get_peer_certificate(ssl)))
						{
							UBYTE *str;

                            UBYTE text2[] = "Server certificate:\n";
                            Write(Output(), (APTR)text2, strlen(text2));
                            Delay(50);

							if((str = X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0)))
							{
                                UBYTE text3[50];
                                sprintf(text3, "\tSubject: %s\n", str);
                                Write(Output(), (APTR)text3, strlen(text3));
                                Delay(50);
								OPENSSL_free(str);
							}
							else {
                                UBYTE text4[] = "Warning: couldn't read subject name in certificate!\n";
                                Write(Output(), (APTR)text4, strlen(text4));
                                Delay(50);
                            }

							if((str = X509_NAME_oneline(X509_get_issuer_name(server_cert),
							                            0, 0)) != NULL)
							{
                                UBYTE text5[50];
                                sprintf(text5, "\tIssuer: %s\n", str);
                                Write(Output(), (APTR)text5, strlen(text5));
                                Delay(50);
								OPENSSL_free(str);
							}
							else {
                                UBYTE text6[] = "Warning: couldn't read issuer name in certificate!\n";
                                Write(Output(), (APTR)text6, strlen(text6));
                                Delay(50);
                            }

							X509_free(server_cert);

							/* Send a HTTP request. Again, this is just
							 * a very basic example.
							 */
							sprintf(buffer,"GET / HTTP/1.0\r\nHost: %s\r\n\r\n",host);
							if ((ssl_err = SSL_write(ssl, buffer, strlen(buffer))) > 0) {
								/* Dump everything to output */
								while ((ssl_err = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
                                    Write(Output(), (APTR)buffer, strlen(buffer));
                                    Delay(50);
                                }

								/* This is not entirely true, check
								 * the SSL_read documentation
								 */
								ok = ssl_err == 0;
							}
							else {
                                UBYTE text7[] = "Couldn't write request!\n";
                                Write(Output(), (APTR)text7, strlen(text7));
                                Delay(50);
                            }
						}
						else {
                            UBYTE text8[] = "Couldn't get server certificate!\n";
                                Write(Output(), (APTR)text8, strlen(text8));
                                Delay(50);
                        }
							
						/* Send SSL close notification */
						SSL_shutdown(ssl);
					}
					else {
                        UBYTE text9[] = "Couldn't establish SSL connection!\n";
                                Write(Output(), (APTR)text9, strlen(text9));
                                Delay(50);
                    }
					
					/* If there were errors, print them */
					if (ssl_err < 0)
						ERR_print_errors(bio_err);

					/* Close the socket */
					CloseSocket(sock);
				}
				else {
                    UBYTE text10[] = "Couldn't connect to host!\n";
                                Write(Output(), (APTR)text10, strlen(text10));
                                Delay(50);
                }

                UBYTE text11[] = "before SSL_free()\n";
                                Write(Output(), (APTR)text11, strlen(text11));
                                Delay(50);
				SSL_free(ssl);
			}
			else {
                UBYTE text12[] = "Couldn't create new SSL handle!\n";
                                Write(Output(), (APTR)text12, strlen(text12));
                                Delay(50);
            }
				
            UBYTE text13[] = "before SSL_CTX_free()\n";
                                Write(Output(), (APTR)text13, strlen(text13));
                                Delay(50);
			SSL_CTX_free(ctx);
		}
		else {
            UBYTE text14[] = "Couldn't create new context!\n";
                                Write(Output(), (APTR)text14, strlen(text14));
                                Delay(50);
        }

		BIO_free(bio_err);

        UBYTE text15[] = "before Cleanup()\n";
                                Write(Output(), (APTR)text15, strlen(text15));
                                Delay(50);
		cleanup();

    UBYTE text16[] = "before end of connectToOpenAI()\n";
                                Write(Output(), (APTR)text16, strlen(text16));
                                Delay(50);
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
	int sock = -1;

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
        // FPrintf(GetStdErr(), "Host lookup failed\n");
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
				sprintf(buffer, "CONNECT %s:%ld HTTP/1.0\r\n\r\n",
				        host, (long)port);

                memclr(buffer, sizeof(buffer));

				/* In a real application, it would be necessary to loop
				 * until everything is sent or an error occurrs, but here we
				 * hope that everything gets sent at once.
				 */
				if (send(sock, buffer, strlen(buffer), 0) >= 0)
				{
					int len;

					/* Again, some optimistic behaviour: HTTP response might not be
					 * received with only one recv
					 */
					if ((len = recv(sock, buffer, sizeof(buffer) - 1, 0)) >= 0)
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
						if (strncmp(buffer, "HTTP/", 4) == 0)
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

static void cleanup(void)
{
	if (amiSSLInitialized)
	{	/* Must always call after successful InitAmiSSL() */
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
}


//     // // Create an SSL context
//     SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
//     if (!ctx) {
//         // printf("Failed to create SSL context\n");
//         // CleanupAmiSSL();
//         return 1;
//     }

//     UBYTE text7[] = "created SSL context\n";
// 	Write(Output(), (APTR)text7, strlen(text7));
// 	Delay(50);

//     // // Set up the connection
//     int sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0) {
//         // printf("Failed to create a socket\n");
//         // SSL_CTX_free(ctx);
//         // CleanupAmiSSLA();
//         return 1;
//     }

//     UBYTE text8[] = "created socket\n";
// 	Write(Output(), (APTR)text8, strlen(text8));
// 	Delay(50);

//     struct sockaddr_in server_addr;
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(443);
//     server_addr.sin_addr.s_addr = inet_addr("api.openai.com"); // Use the IP address of api.openai.com

//     if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         // printf("Failed to connect to the server\n");
//         CloseSocket(sockfd);
//         SSL_CTX_free(ctx);
//         // CleanupAmiSSL();
//         return 1;
//     }

//     UBYTE text9[] = "connected to server\n";
// 	Write(Output(), (APTR)text9, strlen(text9));
// 	Delay(50);

//     // Create an SSL connection
//     SSL *ssl = SSL_new(ctx);
//     if (!ssl) {
//         // printf("Failed to create SSL connection\n");
//         CloseSocket(sockfd);
//         SSL_CTX_free(ctx);
//         // CleanupAmiSSL();
//         return 1;
//     }

//     UBYTE text10[] = "created SSL connection\n";
// 	Write(Output(), (APTR)text10, strlen(text10));
// 	Delay(50);

//     // Associate the SSL connection with the socket
//     SSL_set_fd(ssl, sockfd);

//     // Establish the SSL connection
//     if (SSL_connect(ssl) <= 0) {
//         // printf("Failed to establish SSL connection\n");
//         SSL_free(ssl);
//         CloseSocket(sockfd);
//         SSL_CTX_free(ctx);
//         // CleanupAmiSSL();
//         return 1;
//     }

//     UBYTE text11[] = "established SSL connection\n";
// 	Write(Output(), (APTR)text11, strlen(text11));
// 	Delay(50);

//     // Compose the POST request
//     char request[1024];
//     sprintf(request,
//              "POST /v1/engines/davinci-codex/completions HTTP/1.1\r\n"
//              "Host: api.openai.com\r\n"
//              "Content-Type: application/json\r\n"
//              "Authorization: Bearer %s\r\n"
//              "Content-Length: %zu\r\n"
//              "\r\n"
//              "{\"prompt\": \"Hello, how can I help you today?\"}",
//              OPENAI_API_KEY, strlen("{\"prompt\": \"Hello, how can I help you today?\"}"));

//     // Send the POST request
//     if (SSL_write(ssl, request, strlen(request)) < 0) {
//         // printf("Failed to send POST request\n");
//         SSL_shutdown(ssl);
//         SSL_free(ssl);
//         CloseSocket(sockfd);
//         SSL_CTX_free(ctx);
//         // CleanupAmiSSL();
//         return 1;
//     }

//     UBYTE text12[] = "sent POST request\n";
// 	Write(Output(), (APTR)text12, strlen(text12));
// 	Delay(50);

//     // Read the response
//     char response[4096];
//     int bytes_read = SSL_read(ssl, response, sizeof(response)-1);
//     if (bytes_read < 0) {
//         // printf("Failed to read the response\n");
//         SSL_shutdown(ssl);
//         SSL_free(ssl);
//         CloseSocket(sockfd);
//         SSL_CTX_free(ctx);
//         // CleanupAmiSSL();
//         return 1;
//     }

//     UBYTE text13[] = "read response\n";
// 	Write(Output(), (APTR)text13, strlen(text13));
// 	Delay(50);

//     // Null-terminate the response
//     response[bytes_read] = '\0';

//     // Print the response
//     // printf("Response:\n%s\n", response);

// 	Write(Output(), (APTR)response, strlen(response));
// 	Delay(50);
//     speakText(response);

//     // Clean up
//     SSL_shutdown(ssl);
//     SSL_free(ssl);
//     CloseSocket(sockfd);
//     SSL_CTX_free(ctx);
//     // CleanupAmiSSL();
// //    return(AmiSSLInitialized);
//     return 0;

