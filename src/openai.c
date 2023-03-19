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

#define GETINTERFACE(iface, base) TRUE
#define DROPINTERFACE(iface)

#define OPENAI_API_KEY "your_api_key_here"

static void Cleanup(void);
static void GenerateRandomSeed(char *buffer, int size);
static LONG ConnectToServer(char *, short, char *, short);
static LONG verify_cb(int preverify_ok, X509_STORE_CTX *ctx);

struct Library *AmiSSLMasterBase, *AmiSSLBase, *SocketBase;
struct UtilityBase *UtilityBase;

BOOL AmiSSLInitialized;

static BPTR GetStdErr(void) {
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
    AmiSSLInitialized = FALSE;

	if (!(UtilityBase = (struct UtilityBase *)OpenLibrary("utility.library", 0)))
		return RETURN_ERROR;

	if (!(SocketBase = OpenLibrary("bsdsocket.library", 4)))
		return RETURN_ERROR;

	if (!(AmiSSLMasterBase = OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION)))
		return RETURN_ERROR;
	
    if (!InitAmiSSLMaster(AMISSL_CURRENT_VERSION, TRUE))
		return RETURN_ERROR;

	if (!(AmiSSLBase = OpenAmiSSL()))
		return RETURN_ERROR;

	if (InitAmiSSL(AmiSSL_ErrNoPtr, &errno,
	                    AmiSSL_SocketBase, SocketBase,
	                    TAG_DONE) != 0)
		return RETURN_ERROR;
	

	AmiSSLInitialized = TRUE;

	return(AmiSSLInitialized);






    // // Initialize AmiSSL
    // if (!InitAmiSSL(0)) {
    //     // printf("Failed to initialize AmiSSL\n");
    //     return 1;
    // }

    // // Create an SSL context
    // SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    // if (!ctx) {
    //     // printf("Failed to create SSL context\n");
    //     // CleanupAmiSSL();
    //     return 1;
    // }

    // // Set up the connection
    // int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // if (sockfd < 0) {
    //     // printf("Failed to create a socket\n");
    //     SSL_CTX_free(ctx);
    //     CleanupAmiSSLA();
    //     return 1;
    // }
    

    // struct sockaddr_in server_addr;
    // server_addr.sin_family = AF_INET;
    // server_addr.sin_port = htons(443);
    // server_addr.sin_addr.s_addr = inet_addr("api.openai.com"); // Use the IP address of api.openai.com

    // if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    //     // printf("Failed to connect to the server\n");
    //     close(sockfd);
    //     SSL_CTX_free(ctx);
    //     CleanupAmiSSL();
    //     return 1;
    // }

    // // Create an SSL connection
    // SSL *ssl = SSL_new(ctx);
    // if (!ssl) {
    //     // printf("Failed to create SSL connection\n");
    //     close(sockfd);
    //     SSL_CTX_free(ctx);
    //     CleanupAmiSSL();
    //     return 1;
    // }

    // // Associate the SSL connection with the socket
    // SSL_set_fd(ssl, sockfd);

    // // Establish the SSL connection
    // if (SSL_connect(ssl) <= 0) {
    //     // printf("Failed to establish SSL connection\n");
    //     SSL_free(ssl);
    //     close(sockfd);
    //     SSL_CTX_free(ctx);
    //     CleanupAmiSSL();
    //     return 1;
    // }

    // // Compose the POST request
    // char request[1024];
    // snprintf(request, sizeof(request),
    //          "POST /v1/engines/davinci-codex/completions HTTP/1.1\r\n"
    //          "Host: api.openai.com\r\n"
    //          "Content-Type: application/json\r\n"
    //          "Authorization: Bearer %s\r\n"
    //          "Content-Length: %zu\r\n"
    //          "\r\n"
    //          "{\"prompt\": \"Hello, how can I help you today?\"}",
    //          OPENAI_API_KEY, strlen("{\"prompt\": \"Hello, how can I help you today?\"}"));

    // // Send the POST request
    // if (SSL_write(ssl, request, strlen(request)) < 0) {
    //     // printf("Failed to send POST request\n");
    //     SSL_shutdown(ssl);
    //     SSL_free(ssl);
    //     close(sockfd);
    //     SSL_CTX_free(ctx);
    //     CleanupAmiSSL();
    //     return 1;
    // }

    // // Read the response
    // char response[4096];
    // int bytes_read = SSL_read(ssl, response, sizeof(response)-1);
    // if (bytes_read < 0) {
    //     // printf("Failed to read the response\n");
    //     SSL_shutdown(ssl);
    //     SSL_free(ssl);
    //     close(sockfd);
    //     SSL_CTX_free(ctx);
    //     CleanupAmiSSL();
    //     return 1;
    // }

    // // Null-terminate the response
    // response[bytes_read] = '\0';

    // // Print the response
    // // printf("Response:\n%s\n", response);

    // // Clean up
    // SSL_shutdown(ssl);
    // SSL_free(ssl);
    // close(sockfd);
    // SSL_CTX_free(ctx);
    // CleanupAmiSSL();

    // return 0;
}