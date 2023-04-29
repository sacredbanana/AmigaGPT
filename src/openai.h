#include <proto/dos.h>

#define READ_BUFFER_LENGTH 8192
#define WRITE_BUFFER_LENGTH 8192
#define TEMP_BUFFER_LENGTH 1024

struct ConversationNode {
	struct Node node;
	UBYTE role[64];
	UBYTE content[READ_BUFFER_LENGTH];
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
UBYTE* postMessageToOpenAI(struct MinList *conversation, enum Model model);
void closeOpenAIConnector();