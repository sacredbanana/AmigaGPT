#ifndef UTF8STREAM_H
#define UTF8STREAM_H

#ifdef UTF8STREAM_HOST
#include <stddef.h>
typedef unsigned char UBYTE;
typedef unsigned long ULONG;
typedef int BOOL;
#else
#include <exec/types.h>
#endif

/**
 * Buffers streaming UTF-8 bytes and yields only complete codepoint sequences.
 * Incomplete trailing bytes stay in the buffer until the next append or flush.
 */
struct UTF8StreamBuffer {
    UBYTE *buffer;
    ULONG capacity;
    ULONG used;
};

struct UTF8StreamBuffer *utf8stream_create(ULONG initial_capacity);
void utf8stream_free(struct UTF8StreamBuffer *stream);

/** Append raw bytes; returns FALSE on allocation failure. */
BOOL utf8stream_append(struct UTF8StreamBuffer *stream, const UBYTE *data,
                       ULONG len);

/**
 * Copy complete UTF-8 prefix into dest (NUL-terminated if dest_capacity > 0).
 * Removes copied bytes from the internal buffer.
 * @return byte length written to dest, or 0 if nothing complete yet.
 */
ULONG utf8stream_take_complete(struct UTF8StreamBuffer *stream, UBYTE *dest,
                               ULONG dest_capacity);

/**
 * Emit all remaining bytes at end of stream (including a trailing incomplete
 * sequence, passed through as-is).
 */
ULONG utf8stream_flush(struct UTF8StreamBuffer *stream, UBYTE *dest,
                       ULONG dest_capacity);

/**
 * TRUE if s contains CJK Unified Ideographs / Hangul / kana (typical East-Asian
 * scripts). Used to avoid MorphOS TTEngine live-redraw freezes and pointless TTS.
 */
BOOL utf8_contains_cjk(const UBYTE *s);

#endif
