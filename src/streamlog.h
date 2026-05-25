#ifndef STREAMLOG_H
#define STREAMLOG_H

#include <exec/types.h>

#define STREAMLOG_SSE_SNIPPET_MAX 200

/** File log paths (MorphOS assign T: = RAM disk). */
#define STREAMLOG_STREAM_PATH "T:amigagpt_stream.log"
#define STREAMLOG_UTF8_PATH   "T:amigagpt_utf8.log"

void streamLogSyncFromConfig(void);

BOOL streamLogIsEnabled(void);

/**
 * Log one chat stream end (Phase 9 / R4.2).
 * outcome: "OK", "PARTIAL", or "FAILED"
 */
void streamLogChatEnd(CONST_STRPTR outcome, ULONG messageLen,
                      BOOL completedOk, BOOL truncated,
                      CONST_STRPTR lastSseSnippet);

/** UTF-8 stream buffer / append issues (sparse). */
void streamLogUtf8(CONST_STRPTR detail);

#endif
