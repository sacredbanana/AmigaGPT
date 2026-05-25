#ifndef CHATEXPORT_H
#define CHATEXPORT_H

#include <exec/types.h>
#include <intuition/intuition.h>
#include "openai.h"

/**
 * Save the current conversation as UTF-8 text (raw_utf8 per message, with
 * role markers). For diagnosis; includes ``` fences as received from the API.
 */
BOOL chatExportConversationRaw(struct Conversation *conversation,
                                 struct Window *aslParent);

#endif
