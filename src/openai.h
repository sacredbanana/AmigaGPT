#include <proto/dos.h>

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 4096
#define TEMP_BUFFER_LENGTH 1024

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

enum Model {
    GPT_4,
    GPT_4_0314,
    GPT_4_32K,
    GPT_4_32K_0314,
    GPT_3_5_TURBO,
    GPT_3_5_TURBO_0301
};

LONG initOpenAIConnector();
UBYTE* postMessageToOpenAI(UBYTE* content, enum Model model, UBYTE* role);
void closeOpenAIConnector();