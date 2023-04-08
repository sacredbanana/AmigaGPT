#include <proto/dos.h>

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 4096
#define TEMP_BUFFER_LENGTH 64

struct OpenAIMessage {
    UBYTE *content;
    UBYTE *role;
    struct OpenAIMessage *next;
};

struct OpenAIConversation {
    UBYTE *name;
    UBYTE *model;
    struct OpenAIMessage *firstMessage;
};

LONG initOpenAIConnector();
LONG connectToOpenAI();
UBYTE* postMessageToOpenAI(UBYTE* content, UBYTE* model, UBYTE* role);
void closeOpenAIConnector();