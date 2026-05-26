#ifndef STREAMLOG_H
#define STREAMLOG_H

#include <exec/types.h>

#define STREAMLOG_SSE_SNIPPET_MAX 200

/** MorphOS RAM disk (standard assign). */
#define STREAMLOG_STREAM_PATH "T:amigagpt_stream.log"
#define STREAMLOG_UTF8_PATH   "T:amigagpt_utf8.log"

void streamLogSyncFromConfig(void);

BOOL streamLogIsEnabled(void);

void streamLogChatEnd(CONST_STRPTR outcome, ULONG messageLen,
                      BOOL completedOk, BOOL truncated,
                      CONST_STRPTR lastSseSnippet);

/** Chat/API errors that return before finishChatStream (e.g. web_search + GPT-3.5). */
void streamLogApiError(CONST_STRPTR kind, CONST_STRPTR detail);

void streamLogUtf8(CONST_STRPTR detail);

/** Startup trace (only when debugStreamLog is on); helps diagnose silent WB start. */
void streamLogBootPhase(CONST_STRPTR phase);

#endif
