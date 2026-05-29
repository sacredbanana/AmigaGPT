#ifndef STREAMLOG_H
#define STREAMLOG_H

#include <exec/types.h>

#define STREAMLOG_SSE_SNIPPET_MAX 200

/** MorphOS RAM disk (debug stream only). */
#define STREAMLOG_STREAM_PATH "T:amigagpt_stream.log"
#define STREAMLOG_UTF8_PATH   "T:amigagpt_utf8.log"
/** Always-on lifecycle: persistent (survives reboot); T: mirror for quick inspection. */
#define STREAMLOG_LIFECYCLE_PATH "AMIGAGPT:amigagpt_lifecycle.log"
#define STREAMLOG_LIFECYCLE_MIRROR_PATH "T:amigagpt_lifecycle.log"

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

/** Always-on lifecycle trace for startup/shutdown races. */
void streamLogLifecycle(CONST_STRPTR phase);

#endif
