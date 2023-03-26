#include <proto/dos.h>

LONG initOpenAIConnector();
LONG connectToOpenAI();
UBYTE* postMessageToOpenAI(UBYTE* content, UBYTE* model, UBYTE* role);
void closeOpenAIConnector();