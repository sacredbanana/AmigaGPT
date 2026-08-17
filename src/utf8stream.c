#ifdef UTF8STREAM_HOST
#include <stdlib.h>
#include <string.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define AllocVec(size, flags) calloc(1, (size))
#define FreeVec(p) free(p)
#define MEMF_CLEAR 0
#else
#include <proto/exec.h>
#include <string.h>
#endif

#include "utf8stream.h"

#define UTF8STREAM_GROW 4096

static ULONG utf8_complete_prefix_length(const UBYTE *data, ULONG len) {
    ULONG i = 0;

    while (i < len) {
        UBYTE b = data[i];
        ULONG need;

        if (b < 0x80) {
            i++;
            continue;
        }
        if ((b & 0xE0) == 0xC0) {
            need = 2;
        } else if ((b & 0xF0) == 0xE0) {
            need = 3;
        } else if ((b & 0xF8) == 0xF0) {
            need = 4;
        } else {
            i++;
            continue;
        }

        if (i + need > len) {
            return i;
        }

        {
            ULONG j;
            BOOL valid = TRUE;
            for (j = 1; j < need; j++) {
                if ((data[i + j] & 0xC0) != 0x80) {
                    valid = FALSE;
                    break;
                }
            }
            if (!valid) {
                i++;
                continue;
            }
        }

        i += need;
    }

    return i;
}

struct UTF8StreamBuffer *utf8stream_create(ULONG initial_capacity) {
    struct UTF8StreamBuffer *stream;

    if (initial_capacity < 64) {
        initial_capacity = 64;
    }

    stream = AllocVec(sizeof(struct UTF8StreamBuffer), MEMF_CLEAR);
    if (stream == NULL) {
        return NULL;
    }

    stream->buffer = AllocVec(initial_capacity, MEMF_CLEAR);
    if (stream->buffer == NULL) {
        FreeVec(stream);
        return NULL;
    }

    stream->capacity = initial_capacity;
    stream->used = 0;
    return stream;
}

void utf8stream_free(struct UTF8StreamBuffer *stream) {
    if (stream == NULL) {
        return;
    }
    if (stream->buffer != NULL) {
        FreeVec(stream->buffer);
    }
    FreeVec(stream);
}

static BOOL utf8stream_grow(struct UTF8StreamBuffer *stream, ULONG extra) {
    ULONG new_capacity;
    UBYTE *new_buffer;

    if (stream->used + extra <= stream->capacity) {
        return TRUE;
    }

    new_capacity = stream->capacity + extra;
    if (new_capacity < stream->capacity + UTF8STREAM_GROW) {
        new_capacity = stream->capacity + UTF8STREAM_GROW;
    }

    new_buffer = AllocVec(new_capacity, MEMF_CLEAR);
    if (new_buffer == NULL) {
        return FALSE;
    }

    if (stream->used > 0) {
        memcpy(new_buffer, stream->buffer, stream->used);
    }
    FreeVec(stream->buffer);
    stream->buffer = new_buffer;
    stream->capacity = new_capacity;
    return TRUE;
}

BOOL utf8stream_append(struct UTF8StreamBuffer *stream, const UBYTE *data,
                       ULONG len) {
    if (stream == NULL || data == NULL || len == 0) {
        return TRUE;
    }

    if (!utf8stream_grow(stream, len)) {
        return FALSE;
    }

    memcpy(stream->buffer + stream->used, data, len);
    stream->used += len;
    return TRUE;
}

static ULONG utf8stream_copy_prefix(struct UTF8StreamBuffer *stream, UBYTE *dest,
                                    ULONG dest_capacity, ULONG prefix_len) {
    ULONG copy_len;

    if (dest == NULL || dest_capacity == 0 || prefix_len == 0) {
        return 0;
    }

    copy_len = prefix_len;
    if (copy_len >= dest_capacity) {
        copy_len = dest_capacity - 1;
    }

    memcpy(dest, stream->buffer, copy_len);
    dest[copy_len] = '\0';

    if (prefix_len < stream->used) {
        memmove(stream->buffer, stream->buffer + prefix_len,
                stream->used - prefix_len);
    }
    stream->used -= prefix_len;

    return copy_len;
}

ULONG utf8stream_take_complete(struct UTF8StreamBuffer *stream, UBYTE *dest,
                               ULONG dest_capacity) {
    ULONG prefix_len;

    if (stream == NULL || stream->used == 0) {
        return 0;
    }

    prefix_len = utf8_complete_prefix_length(stream->buffer, stream->used);
    if (prefix_len == 0) {
        return 0;
    }

    return utf8stream_copy_prefix(stream, dest, dest_capacity, prefix_len);
}

ULONG utf8stream_flush(struct UTF8StreamBuffer *stream, UBYTE *dest,
                       ULONG dest_capacity) {
    if (stream == NULL || stream->used == 0) {
        return 0;
    }

    return utf8stream_copy_prefix(stream, dest, dest_capacity, stream->used);
}

static BOOL utf8_is_cjk_codepoint(ULONG cp) {
    /* Hiragana, Katakana */
    if (cp >= 0x3040UL && cp <= 0x30FFUL)
        return TRUE;
    /* CJK Unified + Extension A (BMP) */
    if (cp >= 0x3400UL && cp <= 0x9FFFUL)
        return TRUE;
    /* Hangul syllables */
    if (cp >= 0xAC00UL && cp <= 0xD7AFUL)
        return TRUE;
    /* CJK Compatibility Ideographs (BMP) */
    if (cp >= 0xF900UL && cp <= 0xFAFFUL)
        return TRUE;
    /* CJK Extensions B–F (supplementary) */
    if (cp >= 0x20000UL && cp <= 0x2EBEFUL)
        return TRUE;
    /* CJK Compatibility Ideographs Supplement */
    if (cp >= 0x2F800UL && cp <= 0x2FA1FUL)
        return TRUE;
    /* CJK Extensions G–H */
    if (cp >= 0x30000UL && cp <= 0x323AFUL)
        return TRUE;
    return FALSE;
}

BOOL utf8_contains_cjk(const UBYTE *s) {
    if (s == NULL) {
        return FALSE;
    }

    while (*s != '\0') {
        UBYTE b = *s;
        ULONG need;
        ULONG cp;

        if (b < 0x80) {
            s++;
            continue;
        }
        if ((b & 0xE0) == 0xC0) {
            need = 2;
        } else if ((b & 0xF0) == 0xE0) {
            need = 3;
        } else if ((b & 0xF8) == 0xF0) {
            need = 4;
        } else {
            s++;
            continue;
        }

        {
            ULONG i;
            for (i = 1; i < need; i++) {
                if (s[i] == '\0' || (s[i] & 0xC0) != 0x80) {
                    s++;
                    need = 0;
                    break;
                }
            }
            if (need == 0) {
                continue;
            }
        }

        if (need == 3) {
            cp = ((ULONG)(b & 0x0F) << 12) | ((ULONG)(s[1] & 0x3F) << 6) |
                 (ULONG)(s[2] & 0x3F);
        } else if (need == 4) {
            cp = ((ULONG)(b & 0x07) << 18) | ((ULONG)(s[1] & 0x3F) << 12) |
                 ((ULONG)(s[2] & 0x3F) << 6) | (ULONG)(s[3] & 0x3F);
        } else {
            s += need;
            continue;
        }

        if (utf8_is_cjk_codepoint(cp)) {
            return TRUE;
        }

        s += need;
    }

    return FALSE;
}
