#include <clib/alib_protos.h>
#include <devices/ahi.h>
#include <dos/dos.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/mui.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <SDI_hook.h>
#include <string.h>
#include "SpeechWaveform.h"

#define WAVE_COLS 256
#define WAVE_BANDS 8
#define FFT_N 32
#define CTRL_BAR 20
#define BTN_W 22
#define BTN_H 16
#define BTN_GAP 3
#define BTN_PAD 4
#define BTN_NONE 0
#define BTN_PLAYPAUSE 1
#define BTN_STOP 2
#define BTN_REWIND 3

struct MUI_CustomClass *speechWaveformClass;

struct SpeechWaveformData {
    struct MUI_EventHandlerNode eh;
    BOOL eventHandlerAdded;
    BOOL dragging;
    BOOL hasAudio;
    UBYTE fileName[256];
    UBYTE peaks[WAVE_COLS];
    UBYTE bands[WAVE_COLS][WAVE_BANDS];
    ULONG durationMs;
    ULONG positionMs;
    struct BitMap *cacheBm;
    struct RastPort cacheRp;
    LONG cacheWidth;
    LONG cacheHeight;
    BOOL cacheValid;
    BOOL playing;
    BOOL paused;
    BOOL hasSpeech;
    UBYTE pressedBtn;
    ULONG command;
    UBYTE *pcm;
    ULONG pcmBytes;
    ULONG pcmAlloc;
    ULONG sampleRate;
    ULONG ahiType;
    ULONG frameSize;
    ULONG playOffset;
    ULONG playStartOffset;
    ULONG playStartMs;
    ULONG rawSampleRate;
    UWORD rawChannels;
    UWORD rawBits;
    BOOL rawLittleEndian;
    struct MsgPort *ahiPort;
    struct AHIRequest *ahiReq;
    BOOL ahiBusy;
    struct MUI_InputHandlerNode ihn;
    BOOL ihnAdded;
    ULONG penWave;
    ULONG penPeak;
    ULONG penSpec;
    ULONG penBack;
    ULONG penBar;
    ULONG penText;
};

static const WORD COS_TAB[FFT_N] = {
    256,  251,  237,  213,  181,  142,   98,   50,    0,  -50,  -98, -142,
    -181, -213, -237, -251, -256, -251, -237, -213, -181, -142,  -98,  -50,
    0,    50,    98,  142,  181,  213,  237,  251};
static const WORD SIN_TAB[FFT_N] = {
    0,    50,    98,  142,  181,  213,  237,  251,  256,  251,  237,  213,
    181,  142,   98,   50,    0,  -50,  -98, -142, -181, -213, -237, -251,
    -256, -251, -237, -213, -181, -142,  -98,  -50};

static UWORD readLE16(const UBYTE *source) {
    return (UWORD)((UWORD)source[0] | ((UWORD)source[1] << 8));
}

static ULONG readLE32(const UBYTE *source) {
    return (ULONG)source[0] | ((ULONG)source[1] << 8) |
           ((ULONG)source[2] << 16) | ((ULONG)source[3] << 24);
}

static APTR readWholeFile(CONST_STRPTR filename, ULONG *size) {
    BPTR file;
    LONG length;
    UBYTE *buffer;

    *size = 0;
    file = Open(filename, MODE_OLDFILE);
    if (file == 0)
        return NULL;
#ifdef __AMIGAOS3__
    Seek(file, 0, OFFSET_END);
    length = Seek(file, 0, OFFSET_BEGINNING);
#elif defined(__AMIGAOS4__)
    length = (LONG)GetFileSize(file);
#else
    {
        struct FileInfoBlock fib;
        ExamineFH64(file, &fib, NULL);
        length = (LONG)fib.fib_Size;
    }
#endif
    if (length <= 0) {
        Close(file);
        return NULL;
    }
    buffer = AllocVec(length, MEMF_ANY);
    if (buffer == NULL || Read(file, buffer, length) != length) {
        if (buffer != NULL)
            FreeVec(buffer);
        Close(file);
        return NULL;
    }
    Close(file);
    *size = (ULONG)length;
    return buffer;
}

static WORD sampleAt(const UBYTE *data, ULONG index, UWORD bits,
                     UWORD channels, BOOL signedPcm, BOOL bigEndian) {
    ULONG frame;
    LONG left;
    LONG right;

    if (bits == 8) {
        frame = index * channels;
        if (signedPcm)
            left = ((BYTE)data[frame]) << 8;
        else
            left = ((LONG)data[frame] - 128) << 8;
        if (channels > 1) {
            if (signedPcm)
                right = ((BYTE)data[frame + 1]) << 8;
            else
                right = ((LONG)data[frame + 1] - 128) << 8;
        } else
            right = left;
    } else if (bits == 24) {
        frame = index * channels * 3;
        left = (WORD)((data[frame + 2] << 8) | data[frame + 1]);
        if (channels > 1)
            right = (WORD)((data[frame + 5] << 8) | data[frame + 4]);
        else
            right = left;
    } else {
        frame = index * channels * 2;
        if (bigEndian)
            left = (WORD)(((UWORD)data[frame] << 8) | data[frame + 1]);
        else
            left = (WORD)readLE16(data + frame);
        if (channels > 1) {
            if (bigEndian)
                right = (WORD)(((UWORD)data[frame + 2] << 8) | data[frame + 3]);
            else
                right = (WORD)readLE16(data + frame + 2);
        } else
            right = left;
    }
    return (WORD)((left + right) / 2);
}

static void analyzeColumn(const WORD *window, UBYTE *peak, UBYTE *bands) {
    ULONG n;
    ULONG k;
    ULONG maxAbs = 0;

    for (n = 0; n < FFT_N; n++) {
        LONG v = window[n];
        if (v < 0)
            v = -v;
        if ((ULONG)v > maxAbs)
            maxAbs = (ULONG)v;
    }
    *peak = (UBYTE)(maxAbs > 32767 ? 255 : (maxAbs * 255) / 32767);

    for (k = 0; k < WAVE_BANDS; k++) {
        LONG re = 0;
        LONG im = 0;
        ULONG bin = k + 1;
        ULONG mag;
        for (n = 0; n < FFT_N; n++) {
            ULONG idx = (bin * n) & (FFT_N - 1);
            re += ((LONG)window[n] * COS_TAB[idx]) >> 8;
            im += ((LONG)window[n] * SIN_TAB[idx]) >> 8;
        }
        if (re < 0)
            re = -re;
        if (im < 0)
            im = -im;
        mag = (ULONG)(re + im);
        if (mag > 32767)
            mag = 32767;
        bands[k] = (UBYTE)((mag * 255) / 32767);
    }
}

static BOOL loadWaveformFromSamples(struct SpeechWaveformData *data,
                                    const UBYTE *pcm, ULONG dataLength,
                                    ULONG sampleRate, UWORD bits,
                                    UWORD channels, BOOL signedPcm,
                                    BOOL bigEndian);

static void freeWaveformCache(struct SpeechWaveformData *data) {
    if (data->cacheBm != NULL) {
        WaitBlit();
        FreeBitMap(data->cacheBm);
        data->cacheBm = NULL;
    }
    data->cacheValid = FALSE;
    data->cacheWidth = 0;
    data->cacheHeight = 0;
}

static ULONG nowMs(void) {
    struct DateStamp stamp;

    DateStamp(&stamp);
    return ((ULONG)stamp.ds_Minute * 60000UL) + ((ULONG)stamp.ds_Tick * 20UL);
}

static void finishAHI(struct SpeechWaveformData *data, BOOL abort) {
    if (data->ahiReq == NULL || !data->ahiBusy)
        return;
    if (CheckIO((struct IORequest *)data->ahiReq) == 0 && abort)
        AbortIO((struct IORequest *)data->ahiReq);
    WaitIO((struct IORequest *)data->ahiReq);
    data->ahiBusy = FALSE;
}

static void closeAHI(struct SpeechWaveformData *data) {
    finishAHI(data, TRUE);
    if (data->ahiReq != NULL) {
        if (data->ahiReq->ahir_Std.io_Device != NULL)
            CloseDevice((struct IORequest *)data->ahiReq);
        DeleteIORequest((struct IORequest *)data->ahiReq);
        data->ahiReq = NULL;
    }
    if (data->ahiPort != NULL) {
        DeleteMsgPort(data->ahiPort);
        data->ahiPort = NULL;
    }
}

static BOOL initAHI(struct SpeechWaveformData *data) {
    if (data->ahiReq != NULL)
        return TRUE;
    data->ahiPort = CreateMsgPort();
    if (data->ahiPort == NULL)
        return FALSE;
    data->ahiReq = (struct AHIRequest *)CreateIORequest(
        data->ahiPort, sizeof(struct AHIRequest));
    if (data->ahiReq == NULL) {
        DeleteMsgPort(data->ahiPort);
        data->ahiPort = NULL;
        return FALSE;
    }
    data->ahiReq->ahir_Version = 4;
    data->ahiReq->ahir_Std.io_Message.mn_ReplyPort = data->ahiPort;
    data->ahiReq->ahir_Std.io_Command = CMD_WRITE;
    data->ahiReq->ahir_Std.io_Data = NULL;
    data->ahiReq->ahir_Std.io_Length = 0;
    data->ahiReq->ahir_Frequency = 24000;
    data->ahiReq->ahir_Type = AHIST_M16S;
    data->ahiReq->ahir_Volume = 0x10000;
    data->ahiReq->ahir_Position = 0x8000;
    if (OpenDevice(AHINAME, AHI_DEFAULT_UNIT, (struct IORequest *)data->ahiReq,
                   0L) != 0) {
        DeleteIORequest((struct IORequest *)data->ahiReq);
        DeleteMsgPort(data->ahiPort);
        data->ahiReq = NULL;
        data->ahiPort = NULL;
        return FALSE;
    }
    return TRUE;
}

static ULONG bytesFromMs(struct SpeechWaveformData *data, ULONG ms) {
    unsigned long long bytes;

    if (data->sampleRate == 0 || data->frameSize == 0)
        return 0;
    bytes = (unsigned long long)ms * data->sampleRate * data->frameSize;
    bytes /= 1000ULL;
    if (bytes > data->pcmBytes)
        bytes = data->pcmBytes;
    bytes -= bytes % data->frameSize;
    return (ULONG)bytes;
}

static ULONG msFromBytes(struct SpeechWaveformData *data, ULONG bytes) {
    unsigned long long ms;

    if (data->sampleRate == 0 || data->frameSize == 0)
        return 0;
    if (bytes > data->pcmBytes)
        bytes = data->pcmBytes;
    ms = (unsigned long long)bytes * 1000ULL;
    ms /= ((unsigned long long)data->sampleRate * data->frameSize);
    return (ULONG)ms;
}

static ULONG currentPlayBytes(struct SpeechWaveformData *data) {
    ULONG elapsed;
    ULONG bytes;

    if (data->pcmBytes == 0)
        return 0;
    if (!data->ahiBusy)
        return data->playOffset;
    elapsed = nowMs() - data->playStartMs;
    bytes = data->playStartOffset + bytesFromMs(data, elapsed);
    if (bytes > data->pcmBytes)
        bytes = data->pcmBytes;
    return bytes;
}

static void startAHIWrite(struct SpeechWaveformData *data, ULONG offset) {
    data->ahiReq->ahir_Std.io_Command = CMD_WRITE;
    data->ahiReq->ahir_Std.io_Flags = 0;
    data->ahiReq->ahir_Std.io_Error = 0;
    data->ahiReq->ahir_Std.io_Offset = 0;
    data->ahiReq->ahir_Std.io_Data = data->pcm + offset;
    data->ahiReq->ahir_Std.io_Length = data->pcmAlloc - offset;
    data->ahiReq->ahir_Frequency = data->sampleRate;
    data->ahiReq->ahir_Type = data->ahiType;
    data->ahiReq->ahir_Volume = 0x10000;
    data->ahiReq->ahir_Position = 0x8000;
    data->ahiReq->ahir_Link = NULL;
    SendIO((struct IORequest *)data->ahiReq);
    data->ahiBusy = TRUE;
}

static void stopPlayback(struct SpeechWaveformData *data, BOOL resetPos) {
    finishAHI(data, TRUE);
    data->playing = FALSE;
    if (resetPos) {
        data->paused = FALSE;
        data->playOffset = 0;
        data->playStartOffset = 0;
        data->positionMs = 0;
    }
}

static void freePCM(struct SpeechWaveformData *data) {
    if (data->pcm != NULL) {
        FreeVec(data->pcm);
        data->pcm = NULL;
    }
    data->pcmBytes = 0;
    data->pcmAlloc = 0;
    data->sampleRate = 0;
    data->ahiType = 0;
    data->frameSize = 0;
    data->playOffset = 0;
    data->playStartOffset = 0;
}

static void clearWaveform(struct SpeechWaveformData *data) {
    stopPlayback(data, TRUE);
    freePCM(data);
    memset(data->peaks, 0, sizeof(data->peaks));
    memset(data->bands, 0, sizeof(data->bands));
    data->hasAudio = FALSE;
    data->durationMs = 0;
    data->positionMs = 0;
    data->paused = FALSE;
    data->fileName[0] = '\0';
    data->cacheValid = FALSE;
}

static BOOL startPlayback(struct SpeechWaveformData *data, ULONG offset) {
    if (data->pcm == NULL || data->pcmAlloc == 0 || data->frameSize == 0)
        return FALSE;
    if (!initAHI(data))
        return FALSE;
    if (offset >= data->pcmBytes)
        offset = 0;
    offset -= offset % data->frameSize;
    finishAHI(data, TRUE);
    startAHIWrite(data, offset);
    data->playOffset = offset;
    data->playStartOffset = offset;
    data->playStartMs = nowMs();
    data->positionMs = msFromBytes(data, offset);
    data->playing = TRUE;
    data->paused = FALSE;
    return TRUE;
}

static BOOL pausePlayback(struct SpeechWaveformData *data) {
    if (!data->playing && !data->ahiBusy)
        return FALSE;
    data->playOffset = currentPlayBytes(data);
    data->positionMs = msFromBytes(data, data->playOffset);
    finishAHI(data, TRUE);
    data->playing = FALSE;
    data->paused = TRUE;
    return TRUE;
}

static BOOL seekPlayback(struct SpeechWaveformData *data, ULONG ms) {
    ULONG offset;
    BOOL wasPlaying;

    if (data->pcm == NULL)
        return FALSE;
    wasPlaying = data->playing || data->ahiBusy;
    offset = bytesFromMs(data, ms);
    finishAHI(data, TRUE);
    data->playOffset = offset;
    data->positionMs = msFromBytes(data, offset);
    data->paused = !wasPlaying;
    data->playing = FALSE;
    if (wasPlaying)
        return startPlayback(data, offset);
    return TRUE;
}

static void rewindPlayback(struct SpeechWaveformData *data) {
    BOOL wasPlaying = data->playing || data->ahiBusy;

    if (data->pcm == NULL) {
        stopPlayback(data, TRUE);
        return;
    }
    finishAHI(data, TRUE);
    data->playOffset = 0;
    data->positionMs = 0;
    data->paused = !wasPlaying;
    data->playing = FALSE;
    if (wasPlaying)
        startPlayback(data, 0);
}

static void servicePlayback(struct SpeechWaveformData *data) {
    ULONG bytes;

    if (data->ahiBusy) {
        if (CheckIO((struct IORequest *)data->ahiReq) != 0) {
            finishAHI(data, FALSE);
            data->playing = FALSE;
            data->paused = FALSE;
            data->playOffset = data->pcmBytes;
            data->positionMs = data->durationMs;
            return;
        }
    }
    if (!data->playing)
        return;
    bytes = currentPlayBytes(data);
    data->positionMs = msFromBytes(data, bytes);
    if (data->positionMs > data->durationMs)
        data->positionMs = data->durationMs;
}

static BOOL decodeToAHI(struct SpeechWaveformData *data, const UBYTE *src,
                        ULONG srcLength, ULONG sampleRate, UWORD channels,
                        UWORD bits, BOOL littleEndian) {
    ULONG srcFrame;
    ULONG dstFrame;
    ULONG playLength;
    ULONG padBytes;
    ULONG converted = 0;
    ULONG type;

    srcFrame = (ULONG)((bits + 7) / 8) * channels;
    if (srcFrame == 0 || src == NULL || srcLength == 0 || sampleRate == 0)
        return FALSE;
    srcLength -= srcLength % srcFrame;
    if (srcLength == 0)
        return FALSE;
    dstFrame = (ULONG)(bits == 8 ? 1 : 2) * channels;
    padBytes = sampleRate * dstFrame * 2;
    playLength =
        (bits == 24 ? (srcLength / 3) * 2 : srcLength) + padBytes;
    data->pcm = AllocVec(playLength, MEMF_ANY | MEMF_CLEAR);
    if (data->pcm == NULL)
        return FALSE;
    if (bits == 8) {
        ULONG i;
        for (i = 0; i < srcLength; i++)
            data->pcm[i] = src[i] ^ 0x80;
        converted = srcLength;
        type = channels == 1 ? AHIST_M8S : AHIST_S8S;
    } else if (bits == 24) {
        ULONG in = 0;
        ULONG out = 0;
        while (in + 2 < srcLength) {
            if (littleEndian) {
                data->pcm[out++] = src[in + 2];
                data->pcm[out++] = src[in + 1];
            } else {
                data->pcm[out++] = src[in];
                data->pcm[out++] = src[in + 1];
            }
            in += 3;
        }
        converted = out;
        type = channels == 1 ? AHIST_M16S : AHIST_S16S;
    } else if (littleEndian) {
        ULONG i;
        for (i = 0; i + 1 < srcLength; i += 2) {
            data->pcm[i] = src[i + 1];
            data->pcm[i + 1] = src[i];
        }
        converted = srcLength;
        type = channels == 1 ? AHIST_M16S : AHIST_S16S;
    } else {
        memcpy(data->pcm, src, srcLength);
        converted = srcLength;
        type = channels == 1 ? AHIST_M16S : AHIST_S16S;
    }
    data->pcmBytes = converted;
    data->pcmAlloc = playLength;
    data->sampleRate = sampleRate;
    data->ahiType = type;
    data->frameSize = dstFrame;
    data->playOffset = 0;
    data->playStartOffset = 0;
    return TRUE;
}

static BOOL loadAudio(struct SpeechWaveformData *data, CONST_STRPTR filename) {
    ULONG fileLength = 0;
    UBYTE *file;
    ULONG riff = 0;
    ULONG offset;
    ULONG sampleRate = 0;
    ULONG dataLength = 0;
    UBYTE *pcm = NULL;
    UWORD encoding = 0;
    UWORD channels = 1;
    UWORD bits = 16;
    BOOL parsedWav = FALSE;
    BOOL littleEndian = TRUE;
    UWORD analyzeBits;

    clearWaveform(data);
    if (filename == NULL || filename[0] == '\0')
        return FALSE;

    file = readWholeFile(filename, &fileLength);
    if (file == NULL)
        return FALSE;

    while (riff + 12 <= fileLength &&
           (memcmp(file + riff, "RIFF", 4) != 0 ||
            memcmp(file + riff + 8, "WAVE", 4) != 0))
        riff++;
    if (riff + 12 <= fileLength) {
        offset = riff + 12;
        while (offset + 8 <= fileLength) {
            ULONG chunkLength = readLE32(file + offset + 4);
            ULONG chunkData = offset + 8;
            if (chunkData > fileLength)
                break;
            if (chunkLength > fileLength - chunkData)
                chunkLength = fileLength - chunkData;
            if (memcmp(file + offset, "fmt ", 4) == 0 && chunkLength >= 16) {
                encoding = readLE16(file + chunkData);
                channels = readLE16(file + chunkData + 2);
                sampleRate = readLE32(file + chunkData + 4);
                bits = readLE16(file + chunkData + 14);
            } else if (memcmp(file + offset, "data", 4) == 0) {
                pcm = file + chunkData;
                dataLength = chunkLength;
            }
            offset = chunkData + chunkLength + (chunkLength & 1);
        }
        if (encoding == 0xFFFE)
            encoding = 1;
        parsedWav = (encoding == 1 && (channels == 1 || channels == 2) &&
                     sampleRate != 0 && pcm != NULL && dataLength != 0 &&
                     (bits == 8 || bits == 16 || bits == 24));
    }
    if (!parsedWav) {
        pcm = file;
        dataLength = fileLength;
        sampleRate = data->rawSampleRate ? data->rawSampleRate : 24000;
        channels = data->rawChannels ? data->rawChannels : 1;
        bits = data->rawBits ? data->rawBits : 16;
        littleEndian = data->rawLittleEndian;
        if (channels != 1 && channels != 2)
            channels = 1;
        if (bits != 8 && bits != 16 && bits != 24)
            bits = 16;
    }

    if (!decodeToAHI(data, pcm, dataLength, sampleRate, channels, bits,
                     littleEndian)) {
        FreeVec(file);
        return FALSE;
    }
    FreeVec(file);

    analyzeBits = (UWORD)(data->frameSize / channels * 8);
    if (analyzeBits == 0)
        analyzeBits = 16;
    if (!loadWaveformFromSamples(data, data->pcm, data->pcmBytes,
                                 data->sampleRate, analyzeBits, channels, TRUE,
                                 TRUE)) {
        freePCM(data);
        return FALSE;
    }
    strncpy(data->fileName, filename, sizeof(data->fileName) - 1);
    data->fileName[sizeof(data->fileName) - 1] = '\0';
    return TRUE;
}

static BOOL loadWaveformFromSamples(struct SpeechWaveformData *data,
                                    const UBYTE *pcm, ULONG dataLength,
                                    ULONG sampleRate, UWORD bits,
                                    UWORD channels, BOOL signedPcm,
                                    BOOL bigEndian) {
    ULONG frames;
    ULONG col;
    ULONG bytesPerFrame;

    if (pcm == NULL || dataLength == 0 || sampleRate == 0)
        return FALSE;
    bytesPerFrame = ((bits + 7) / 8) * channels;
    if (bytesPerFrame == 0)
        return FALSE;
    frames = dataLength / bytesPerFrame;
    if (frames == 0)
        return FALSE;

    memset(data->peaks, 0, sizeof(data->peaks));
    memset(data->bands, 0, sizeof(data->bands));
    data->cacheValid = FALSE;
    data->durationMs = (ULONG)(((unsigned long long)frames * 1000ULL) /
                               sampleRate);
    for (col = 0; col < WAVE_COLS; col++) {
        WORD window[FFT_N];
        ULONG start = (col * frames) / WAVE_COLS;
        ULONG end = ((col + 1) * frames) / WAVE_COLS;
        ULONG n;
        if (end <= start)
            end = start + 1;
        for (n = 0; n < FFT_N; n++) {
            ULONG idx = start + ((end - start) * n) / FFT_N;
            if (idx >= frames)
                idx = frames - 1;
            window[n] = sampleAt(pcm, idx, bits, channels, signedPcm,
                                 bigEndian);
        }
        analyzeColumn(window, &data->peaks[col], data->bands[col]);
    }
    data->hasAudio = TRUE;
    data->positionMs = 0;
    return TRUE;
}

static void redrawWaveform(Object *obj) {
    if (obj != NULL && _win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWOBJECT);
}

void speechWaveformSetFile(Object *obj, CONST_STRPTR filename) {
    static const char empty[] = "";

    if (obj == NULL || speechWaveformClass == NULL)
        return;
    set(obj, MUIA_SpeechWaveform_FileName,
        filename != NULL ? filename : (CONST_STRPTR)empty);
}

void speechWaveformService(Object *obj) {
    struct SpeechWaveformData *data;
    ULONG oldPos;
    BOOL oldPlaying;

    if (obj == NULL || speechWaveformClass == NULL)
        return;
    data = INST_DATA(speechWaveformClass->mcc_Class, obj);
    oldPos = data->positionMs;
    oldPlaying = data->playing;
    servicePlayback(data);
    if ((oldPos != data->positionMs || oldPlaying != data->playing) &&
        _win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
}

static void addEventHandler(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    Object *window = _win(obj);

    if (data->eventHandlerAdded || window == NULL)
        return;
    DoMethod(window, MUIM_Window_AddEventHandler, &data->eh);
    data->eventHandlerAdded = TRUE;
}

static void remEventHandler(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    Object *window = _win(obj);

    if (!data->eventHandlerAdded)
        return;
    if (window != NULL)
        DoMethod(window, MUIM_Window_RemEventHandler, &data->eh);
    data->eventHandlerAdded = FALSE;
}

static ULONG seekFromX(struct SpeechWaveformData *data, Object *obj, LONG x) {
    LONG left = _mleft(obj);
    LONG width = _mwidth(obj);
    ULONG ms;

    if (width <= 0 || !data->hasAudio || data->durationMs == 0)
        return data->positionMs;
    if (x < left)
        x = left;
    if (x >= left + width)
        x = left + width - 1;
    ms = ((ULONG)(x - left) * data->durationMs) / (ULONG)width;
    if (ms > data->durationMs)
        ms = data->durationMs;
    return ms;
}

static ULONG resolvedPen(ULONG custom, ULONG fallback) {
    return custom != MUIV_SpeechWaveform_Pen_Default ? custom : fallback;
}

static void themePens(struct DrawInfo *dri, ULONG *shadow, ULONG *shine,
                      ULONG *fill, ULONG *face) {
    *shadow = dri != NULL ? dri->dri_Pens[SHADOWPEN] : 1;
    *shine = dri != NULL ? dri->dri_Pens[SHINEPEN] : 2;
    *fill = dri != NULL ? dri->dri_Pens[FILLPEN] : 3;
    *face = dri != NULL ? dri->dri_Pens[BACKGROUNDPEN] : 0;
}

static LONG waveformHeight(Object *obj) {
    LONG height = _mheight(obj);

    if (height > CTRL_BAR + 8)
        return height - CTRL_BAR;
    if (height > 8)
        return height - 8;
    return height;
}

static void buttonBox(Object *obj, UBYTE btn, LONG *x, LONG *y) {
    LONG index = (LONG)btn - 1;

    *x = _mleft(obj) + BTN_PAD + index * (BTN_W + BTN_GAP);
    *y = _mtop(obj) + waveformHeight(obj) + (CTRL_BAR - BTN_H) / 2;
}

static BOOL buttonEnabled(struct SpeechWaveformData *data, UBYTE btn) {
    if (!data->hasSpeech || !data->hasAudio)
        return FALSE;
    if (btn == BTN_PLAYPAUSE)
        return TRUE;
    if (btn == BTN_STOP)
        return data->playing || data->paused || data->positionMs > 0;
    if (btn == BTN_REWIND)
        return data->playing || data->positionMs > 0;
    return FALSE;
}

static UBYTE buttonAt(Object *obj, LONG mx, LONG my) {
    UBYTE btn;
    LONG x;
    LONG y;

    for (btn = BTN_PLAYPAUSE; btn <= BTN_REWIND; btn++) {
        buttonBox(obj, btn, &x, &y);
        if (mx >= x && mx < x + BTN_W && my >= y && my < y + BTN_H)
            return btn;
    }
    return BTN_NONE;
}

static BOOL inWaveform(Object *obj, LONG mx, LONG my) {
    LONG left = _mleft(obj);
    LONG top = _mtop(obj);
    LONG width = _mwidth(obj);
    LONG height = waveformHeight(obj);

    return mx >= left && mx < left + width && my >= top && my < top + height;
}

static void formatPlayTime(UBYTE *buf, ULONG posMs, ULONG durMs) {
    ULONG pm = posMs / 60000UL;
    ULONG ps = (posMs / 1000UL) % 60UL;
    ULONG dm = durMs / 60000UL;
    ULONG ds = (durMs / 1000UL) % 60UL;
    UBYTE *p = buf;

    if (pm > 99)
        pm = 99;
    if (dm > 99)
        dm = 99;
    if (pm >= 10)
        *p++ = (UBYTE)('0' + (pm / 10));
    *p++ = (UBYTE)('0' + (pm % 10));
    *p++ = ':';
    *p++ = (UBYTE)('0' + (ps / 10));
    *p++ = (UBYTE)('0' + (ps % 10));
    *p++ = ' ';
    *p++ = '/';
    *p++ = ' ';
    if (dm >= 10)
        *p++ = (UBYTE)('0' + (dm / 10));
    *p++ = (UBYTE)('0' + (dm % 10));
    *p++ = ':';
    *p++ = (UBYTE)('0' + (ds / 10));
    *p++ = (UBYTE)('0' + (ds % 10));
    *p = '\0';
}

static void startIcon(struct RastPort *rp, ULONG pen) {
    SetDrMd(rp, JAM1);
    SetAPen(rp, pen);
}

static void drawIconPlay(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    static const UBYTE widths[10] = {2, 3, 5, 6, 8, 8, 6, 5, 3, 2};
    LONG row;
    LONG col;

    startIcon(rp, pen);
    for (row = 0; row < 10; row++) {
        for (col = 0; col < (LONG)widths[row]; col++)
            WritePixel(rp, x + 6 + col, y + 3 + row);
    }
}

static void drawIconPause(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    startIcon(rp, pen);
    RectFill(rp, x + 5, y + 3, x + 8, y + 12);
    RectFill(rp, x + 13, y + 3, x + 16, y + 12);
}

static void drawIconStop(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    startIcon(rp, pen);
    RectFill(rp, x + 6, y + 4, x + 15, y + 12);
}

static void drawIconRewind(struct RastPort *rp, LONG x, LONG y, ULONG pen) {
    static const UBYTE widths[10] = {2, 3, 5, 6, 8, 8, 6, 5, 3, 2};
    LONG row;
    LONG col;

    startIcon(rp, pen);
    RectFill(rp, x + 3, y + 3, x + 5, y + 12);
    for (row = 0; row < 10; row++) {
        LONG start = x + 7 + (8 - (LONG)widths[row]);
        for (col = 0; col < (LONG)widths[row]; col++)
            WritePixel(rp, start + col, y + 3 + row);
    }
}

static void drawControlButton(struct RastPort *rp, Object *obj,
                              struct SpeechWaveformData *data, UBYTE btn,
                              ULONG shine, ULONG shadow, ULONG fill) {
    LONG x;
    LONG y;
    BOOL enabled = buttonEnabled(data, btn);
    BOOL pressed = (data->pressedBtn == btn);
    ULONG iconPen;

    buttonBox(obj, btn, &x, &y);
    SetDrMd(rp, JAM1);
    SetAPen(rp, fill);
    RectFill(rp, x, y, x + BTN_W - 1, y + BTN_H - 1);
    SetAPen(rp, pressed ? shadow : shine);
    Move(rp, x, y + BTN_H - 1);
    Draw(rp, x, y);
    Draw(rp, x + BTN_W - 1, y);
    SetAPen(rp, pressed ? shine : shadow);
    Draw(rp, x + BTN_W - 1, y + BTN_H - 1);
    Draw(rp, x, y + BTN_H - 1);

    iconPen = enabled ? shine : shadow;
    if (btn == BTN_PLAYPAUSE) {
        if (data->playing)
            drawIconPause(rp, x, y, iconPen);
        else
            drawIconPlay(rp, x, y, iconPen);
    } else if (btn == BTN_STOP)
        drawIconStop(rp, x, y, iconPen);
    else
        drawIconRewind(rp, x, y, iconPen);
}

static void drawControlBar(struct RastPort *rp, Object *obj,
                           struct SpeechWaveformData *data,
                           struct DrawInfo *dri) {
    LONG left = _mleft(obj);
    LONG width = _mwidth(obj);
    LONG barTop = _mtop(obj) + waveformHeight(obj);
    LONG barBottom = _mtop(obj) + _mheight(obj) - 1;
    ULONG shadow;
    ULONG shine;
    ULONG fill;
    ULONG face;
    ULONG usedBar;
    ULONG usedText;
    UBYTE timeText[24];
    LONG textW;
    LONG textX;
    LONG textY;

    if (width <= 0 || barBottom < barTop)
        return;

    themePens(dri, &shadow, &shine, &fill, &face);
    (void)fill;
    usedBar = resolvedPen(data->penBar, face);
    usedText = resolvedPen(data->penText, shine);
    SetAPen(rp, usedBar);
    RectFill(rp, left, barTop, left + width - 1, barBottom);
    SetAPen(rp, shine);
    Move(rp, left, barTop);
    Draw(rp, left + width - 1, barTop);

    drawControlButton(rp, obj, data, BTN_PLAYPAUSE, shine, shadow, face);
    drawControlButton(rp, obj, data, BTN_STOP, shine, shadow, face);
    drawControlButton(rp, obj, data, BTN_REWIND, shine, shadow, face);

    formatPlayTime(timeText, data->positionMs, data->durationMs);
    SetAPen(rp, usedText);
    SetDrMd(rp, JAM1);
    textW = TextLength(rp, (STRPTR)timeText, strlen((char *)timeText));
    textX = left + width - 4 - textW;
    if (rp->TxHeight > 0)
        textY = barTop + ((CTRL_BAR - rp->TxHeight) / 2) + rp->TxBaseline;
    else
        textY = barTop + 13;
    if (textX > left + BTN_PAD + 3 * (BTN_W + BTN_GAP)) {
        Move(rp, textX, textY);
        Text(rp, (STRPTR)timeText, strlen((char *)timeText));
    }
}

static BOOL doCommand(struct SpeechWaveformData *data, ULONG command) {
    if (command == MUIV_SpeechWaveform_Command_Play) {
        if (data->positionMs >= data->durationMs && data->durationMs > 0)
            return startPlayback(data, 0);
        return startPlayback(data, data->playOffset);
    }
    if (command == MUIV_SpeechWaveform_Command_Pause)
        return pausePlayback(data);
    if (command == MUIV_SpeechWaveform_Command_Stop) {
        stopPlayback(data, TRUE);
        return TRUE;
    }
    if (command == MUIV_SpeechWaveform_Command_Rewind) {
        rewindPlayback(data);
        return TRUE;
    }
    return FALSE;
}

static ULONG commandForButton(struct SpeechWaveformData *data, UBYTE btn) {
    if (btn == BTN_PLAYPAUSE)
        return data->playing ? MUIV_SpeechWaveform_Command_Pause
                             : MUIV_SpeechWaveform_Command_Play;
    if (btn == BTN_STOP)
        return MUIV_SpeechWaveform_Command_Stop;
    if (btn == BTN_REWIND)
        return MUIV_SpeechWaveform_Command_Rewind;
    return 0;
}

static void renderStaticWaveform(struct RastPort *rp, LONG left, LONG top,
                                 LONG width, LONG height,
                                 struct SpeechWaveformData *data,
                                 struct DrawInfo *dri) {
    LONG x;
    LONG mid;
    ULONG shadow;
    ULONG shine;
    ULONG fill;
    ULONG face;
    ULONG wavePen;
    ULONG headPen;
    ULONG specPen;
    ULONG backPen;

    themePens(dri, &shadow, &shine, &fill, &face);
    (void)face;
    wavePen = resolvedPen(data->penWave, shine);
    headPen = resolvedPen(data->penPeak, shine);
    specPen = resolvedPen(data->penSpec, fill);
    backPen = resolvedPen(data->penBack, shadow);
    SetAPen(rp, backPen);
    RectFill(rp, left, top, left + width - 1, top + height - 1);

    if (!data->hasAudio) {
        SetAPen(rp, shine);
        Move(rp, left + 4, top + height / 2);
        Draw(rp, left + width - 5, top + height / 2);
        return;
    }

    mid = top + height / 2;
    for (x = 0; x < width; x++) {
        ULONG col = ((ULONG)x * WAVE_COLS) / (ULONG)width;
        LONG peak;
        LONG y0;
        LONG y1;
        LONG band;
        LONG bandH;

        if (col >= WAVE_COLS)
            col = WAVE_COLS - 1;
        peak = (LONG)data->peaks[col] * (height / 2 - 2) / 255;
        if (peak < 1)
            peak = 1;
        y0 = mid - peak;
        y1 = mid + peak;
        if (y0 < top + 1)
            y0 = top + 1;
        if (y1 > top + height - 2)
            y1 = top + height - 2;

        bandH = (y1 - y0 + 1) / WAVE_BANDS;
        if (bandH < 1)
            bandH = 1;
        for (band = 0; band < WAVE_BANDS; band++) {
            LONG by0 = y1 - (band + 1) * bandH;
            LONG by1 = y1 - band * bandH;
            UBYTE energy = data->bands[col][band];
            ULONG pen = shadow;
            if (energy > 160)
                pen = headPen;
            else if (energy > 80)
                pen = wavePen;
            else if (energy > 24)
                pen = specPen;
            if (by0 < y0)
                by0 = y0;
            if (by1 > y1)
                by1 = y1;
            if (by1 >= by0) {
                SetAPen(rp, pen);
                if (by1 > by0)
                    RectFill(rp, left + x, by0, left + x, by1);
                else
                    WritePixel(rp, left + x, by0);
            }
        }

        SetAPen(rp, wavePen);
        WritePixel(rp, left + x, y0);
        WritePixel(rp, left + x, y1);
    }
}

static void drawPlayhead(struct RastPort *rp, LONG left, LONG top, LONG width,
                         LONG height, struct SpeechWaveformData *data,
                         struct DrawInfo *dri) {
    LONG playX;
    ULONG shadow;
    ULONG shine;
    ULONG fill;
    ULONG face;
    ULONG headPen;

    if (!data->hasAudio || data->durationMs == 0 || width <= 0)
        return;
    themePens(dri, &shadow, &shine, &fill, &face);
    (void)shadow;
    (void)fill;
    (void)face;
    headPen = resolvedPen(data->penPeak, shine);
    playX = left + (LONG)((data->positionMs * (ULONG)width) / data->durationMs);
    if (playX < left)
        playX = left;
    if (playX > left + width - 1)
        playX = left + width - 1;
    SetAPen(rp, headPen);
    Move(rp, playX, top);
    Draw(rp, playX, top + height - 1);
}

static BOOL ensureWaveformCache(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj);
    LONG width = _mwidth(obj);
    LONG height = waveformHeight(obj);
    struct BitMap *friendBm;
    ULONG depth;

    if (rp == NULL || rp->BitMap == NULL || width <= 0 || height <= 0)
        return FALSE;
    if (data->cacheValid && data->cacheBm != NULL &&
        data->cacheWidth == width && data->cacheHeight == height)
        return TRUE;

    freeWaveformCache(data);
    friendBm = rp->BitMap;
    depth = (ULONG)friendBm->Depth;
    if (depth < 1)
        depth = 1;
    data->cacheBm =
        AllocBitMap((ULONG)width, (ULONG)height, depth,
                    BMF_MINPLANES | BMF_CLEAR, friendBm);
    if (data->cacheBm == NULL)
        return FALSE;
    InitRastPort(&data->cacheRp);
    data->cacheRp.BitMap = data->cacheBm;
    data->cacheWidth = width;
    data->cacheHeight = height;
    renderStaticWaveform(&data->cacheRp, 0, 0, width, height, data, _dri(obj));
    WaitBlit();
    data->cacheValid = TRUE;
    return TRUE;
}

static void drawWaveform(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct RastPort *rp = _rp(obj);
    struct DrawInfo *dri = _dri(obj);
    LONG left = _mleft(obj);
    LONG top = _mtop(obj);
    LONG width = _mwidth(obj);
    LONG height = waveformHeight(obj);

    if (rp == NULL || width <= 0 || _mheight(obj) <= 0)
        return;

    if (height > 0) {
        if (ensureWaveformCache(cl, obj)) {
            BltBitMapRastPort(data->cacheBm, 0, 0, rp, left, top, width,
                              height, 0xC0);
            WaitBlit();
        } else {
            renderStaticWaveform(rp, left, top, width, height, data, dri);
        }
        drawPlayhead(rp, left, top, width, height, data, dri);
    }
    drawControlBar(rp, obj, data, dri);
}

static SAVEDS ULONG mNew(struct IClass *cl, Object *obj, struct opSet *msg) {
    if (!(obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg)))
        return 0;
    {
        struct SpeechWaveformData *data = INST_DATA(cl, obj);
        struct TagItem *tags;
        struct TagItem *tag;
        CONST_STRPTR fileName = NULL;

        memset(data, 0, sizeof(*data));
        data->rawSampleRate = 24000;
        data->rawChannels = 1;
        data->rawBits = 16;
        data->rawLittleEndian = TRUE;
        data->hasSpeech = TRUE;
        data->penWave = MUIV_SpeechWaveform_Pen_Default;
        data->penPeak = MUIV_SpeechWaveform_Pen_Default;
        data->penSpec = MUIV_SpeechWaveform_Pen_Default;
        data->penBack = MUIV_SpeechWaveform_Pen_Default;
        data->penBar = MUIV_SpeechWaveform_Pen_Default;
        data->penText = MUIV_SpeechWaveform_Pen_Default;
        for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
            switch (tag->ti_Tag) {
            case MUIA_SpeechWaveform_SampleRate:
                if (tag->ti_Data != 0)
                    data->rawSampleRate = tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_Channels:
                if (tag->ti_Data == 1 || tag->ti_Data == 2)
                    data->rawChannels = (UWORD)tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_Bits:
                if (tag->ti_Data == 8 || tag->ti_Data == 16 ||
                    tag->ti_Data == 24)
                    data->rawBits = (UWORD)tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_LittleEndian:
                data->rawLittleEndian = tag->ti_Data ? TRUE : FALSE;
                break;
            case MUIA_SpeechWaveform_HasSpeech:
                data->hasSpeech = tag->ti_Data ? TRUE : FALSE;
                break;
            case MUIA_SpeechWaveform_FileName:
                fileName = (CONST_STRPTR)tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_WavePen:
                data->penWave = tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_PeakPen:
                data->penPeak = tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_SpectrumPen:
                data->penSpec = tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_BackPen:
                data->penBack = tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_BarPen:
                data->penBar = tag->ti_Data;
                break;
            case MUIA_SpeechWaveform_TextPen:
                data->penText = tag->ti_Data;
                break;
            }
        }
        if (fileName != NULL && fileName[0] != '\0')
            loadAudio(data, fileName);
    }
    return (ULONG)obj;
}

static SAVEDS ULONG mGet(struct IClass *cl, Object *obj, struct opGet *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    switch (msg->opg_AttrID) {
    case MUIA_SpeechWaveform_FileName:
        *msg->opg_Storage = (ULONG)data->fileName;
        return TRUE;
    case MUIA_SpeechWaveform_Position:
        *msg->opg_Storage = data->positionMs;
        return TRUE;
    case MUIA_SpeechWaveform_Duration:
        *msg->opg_Storage = data->durationMs;
        return TRUE;
    case MUIA_SpeechWaveform_Seek:
        *msg->opg_Storage = data->positionMs;
        return TRUE;
    case MUIA_SpeechWaveform_Playing:
        *msg->opg_Storage = (ULONG)data->playing;
        return TRUE;
    case MUIA_SpeechWaveform_Paused:
        *msg->opg_Storage = (ULONG)data->paused;
        return TRUE;
    case MUIA_SpeechWaveform_HasSpeech:
        *msg->opg_Storage = (ULONG)data->hasSpeech;
        return TRUE;
    case MUIA_SpeechWaveform_Command:
        *msg->opg_Storage = data->command;
        return TRUE;
    case MUIA_SpeechWaveform_SampleRate:
        *msg->opg_Storage = data->hasAudio ? data->sampleRate
                                           : data->rawSampleRate;
        return TRUE;
    case MUIA_SpeechWaveform_Channels:
        *msg->opg_Storage =
            data->hasAudio
                ? (ULONG)((data->ahiType == AHIST_S8S ||
                           data->ahiType == AHIST_S16S)
                              ? 2
                              : 1)
                : (ULONG)data->rawChannels;
        return TRUE;
    case MUIA_SpeechWaveform_Bits:
        *msg->opg_Storage =
            data->hasAudio
                ? (ULONG)((data->ahiType == AHIST_M8S ||
                           data->ahiType == AHIST_S8S)
                              ? 8
                              : 16)
                : (ULONG)data->rawBits;
        return TRUE;
    case MUIA_SpeechWaveform_LittleEndian:
        *msg->opg_Storage = (ULONG)data->rawLittleEndian;
        return TRUE;
    case MUIA_SpeechWaveform_WavePen:
        *msg->opg_Storage = data->penWave;
        return TRUE;
    case MUIA_SpeechWaveform_PeakPen:
        *msg->opg_Storage = data->penPeak;
        return TRUE;
    case MUIA_SpeechWaveform_SpectrumPen:
        *msg->opg_Storage = data->penSpec;
        return TRUE;
    case MUIA_SpeechWaveform_BackPen:
        *msg->opg_Storage = data->penBack;
        return TRUE;
    case MUIA_SpeechWaveform_BarPen:
        *msg->opg_Storage = data->penBar;
        return TRUE;
    case MUIA_SpeechWaveform_TextPen:
        *msg->opg_Storage = data->penText;
        return TRUE;
    }
    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static SAVEDS ULONG mSet(struct IClass *cl, Object *obj, struct opSet *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct TagItem *tags;
    struct TagItem *tag;
    BOOL redraw = FALSE;
    BOOL update = FALSE;

    for (tags = msg->ops_AttrList; (tag = NextTagItem(&tags));) {
        switch (tag->ti_Tag) {
        case MUIA_SpeechWaveform_FileName:
            loadAudio(data, (CONST_STRPTR)tag->ti_Data);
            data->cacheValid = FALSE;
            redraw = TRUE;
            break;
        case MUIA_SpeechWaveform_SampleRate:
            if (tag->ti_Data != 0)
                data->rawSampleRate = tag->ti_Data;
            break;
        case MUIA_SpeechWaveform_Channels:
            if (tag->ti_Data == 1 || tag->ti_Data == 2)
                data->rawChannels = (UWORD)tag->ti_Data;
            break;
        case MUIA_SpeechWaveform_Bits:
            if (tag->ti_Data == 8 || tag->ti_Data == 16 || tag->ti_Data == 24)
                data->rawBits = (UWORD)tag->ti_Data;
            break;
        case MUIA_SpeechWaveform_LittleEndian:
            data->rawLittleEndian = tag->ti_Data ? TRUE : FALSE;
            break;
        case MUIA_SpeechWaveform_Position:
            if (data->hasAudio)
                seekPlayback(data, tag->ti_Data);
            update = TRUE;
            break;
        case MUIA_SpeechWaveform_Playing:
            if (tag->ti_Data)
                startPlayback(data, data->playOffset);
            else
                pausePlayback(data);
            update = TRUE;
            break;
        case MUIA_SpeechWaveform_Paused:
            data->paused = tag->ti_Data ? TRUE : FALSE;
            update = TRUE;
            break;
        case MUIA_SpeechWaveform_HasSpeech:
            if (data->hasSpeech != (BOOL)tag->ti_Data) {
                data->hasSpeech = (BOOL)tag->ti_Data;
                update = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_Command:
            data->command = tag->ti_Data;
            break;
        case MUIA_SpeechWaveform_WavePen:
            if (data->penWave != tag->ti_Data) {
                data->penWave = tag->ti_Data;
                data->cacheValid = FALSE;
                redraw = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_PeakPen:
            if (data->penPeak != tag->ti_Data) {
                data->penPeak = tag->ti_Data;
                data->cacheValid = FALSE;
                redraw = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_SpectrumPen:
            if (data->penSpec != tag->ti_Data) {
                data->penSpec = tag->ti_Data;
                data->cacheValid = FALSE;
                redraw = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_BackPen:
            if (data->penBack != tag->ti_Data) {
                data->penBack = tag->ti_Data;
                data->cacheValid = FALSE;
                redraw = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_BarPen:
            if (data->penBar != tag->ti_Data) {
                data->penBar = tag->ti_Data;
                update = TRUE;
            }
            break;
        case MUIA_SpeechWaveform_TextPen:
            if (data->penText != tag->ti_Data) {
                data->penText = tag->ti_Data;
                update = TRUE;
            }
            break;
        }
    }

    if (_win(obj) != NULL) {
        if (redraw)
            MUI_Redraw(obj, MADF_DRAWOBJECT);
        else if (update)
            MUI_Redraw(obj, MADF_DRAWUPDATE);
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

static void addInputHandler(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    Object *app = _app(obj);

    (void)cl;
    if (data->ihnAdded || app == NULL)
        return;
#ifdef MUIIHNF_TIMER
    memset(&data->ihn, 0, sizeof(data->ihn));
    data->ihn.ihn_Object = obj;
    data->ihn.ihn_Flags = MUIIHNF_TIMER;
    data->ihn.ihn_Millis = 80;
    DoMethod(app, MUIM_Application_AddInputHandler, &data->ihn);
    data->ihnAdded = TRUE;
#endif
}

static void remInputHandler(struct IClass *cl, Object *obj) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    Object *app = _app(obj);

    (void)cl;
    if (!data->ihnAdded)
        return;
#ifdef MUIIHNF_TIMER
    if (app != NULL)
        DoMethod(app, MUIM_Application_RemInputHandler, &data->ihn);
#endif
    data->ihnAdded = FALSE;
}

static SAVEDS ULONG mSetup(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    if (!(DoSuperMethodA(cl, obj, msg)))
        return FALSE;

    data->eh.ehn_Class = cl;
    data->eh.ehn_Object = obj;
    data->eh.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
    data->eh.ehn_Flags = MUI_EHF_GUIMODE;
    data->eh.ehn_Priority = 0;
    addEventHandler(cl, obj);
    addInputHandler(cl, obj);
    return TRUE;
}

static SAVEDS ULONG mCleanup(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    remInputHandler(cl, obj);
    remEventHandler(cl, obj);
    stopPlayback(data, FALSE);
    closeAHI(data);
    freeWaveformCache(data);
    return DoSuperMethodA(cl, obj, msg);
}

static SAVEDS ULONG mDispose(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    stopPlayback(data, TRUE);
    closeAHI(data);
    freePCM(data);
    freeWaveformCache(data);
    return DoSuperMethodA(cl, obj, msg);
}

static SAVEDS ULONG mPlay(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    (void)msg;
    doCommand(data, MUIV_SpeechWaveform_Command_Play);
    if (_win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return TRUE;
}

static SAVEDS ULONG mPause(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    (void)msg;
    doCommand(data, MUIV_SpeechWaveform_Command_Pause);
    if (_win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return TRUE;
}

static SAVEDS ULONG mStop(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    (void)msg;
    doCommand(data, MUIV_SpeechWaveform_Command_Stop);
    if (_win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return TRUE;
}

static SAVEDS ULONG mRewind(struct IClass *cl, Object *obj, Msg msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);

    (void)msg;
    doCommand(data, MUIV_SpeechWaveform_Command_Rewind);
    if (_win(obj) != NULL)
        MUI_Redraw(obj, MADF_DRAWUPDATE);
    return TRUE;
}

static SAVEDS ULONG mService(struct IClass *cl, Object *obj, Msg msg) {
    (void)cl;
    (void)msg;
    speechWaveformService(obj);
    return TRUE;
}

static SAVEDS ULONG mAskMinMax(struct IClass *cl, Object *obj,
                               struct MUIP_AskMinMax *msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    msg->MinMaxInfo->MinWidth += 160;
    msg->MinMaxInfo->DefWidth += 240;
    msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;
    msg->MinMaxInfo->MinHeight += 68;
    msg->MinMaxInfo->DefHeight += 116;
    msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
    return 0;
}

static SAVEDS ULONG mDraw(struct IClass *cl, Object *obj,
                          struct MUIP_Draw *msg) {
    DoSuperMethodA(cl, obj, (Msg)msg);
    drawWaveform(cl, obj);
    return 0;
}

static SAVEDS ULONG mHandleEvent(struct IClass *cl, Object *obj,
                                 struct MUIP_HandleEvent *msg) {
    struct SpeechWaveformData *data = INST_DATA(cl, obj);
    struct IntuiMessage *imsg = msg->imsg;
    LONG mx;
    LONG my;

    if (imsg == NULL) {
        speechWaveformService(obj);
        return 0;
    }

    mx = imsg->MouseX;
    my = imsg->MouseY;

    if (imsg->Class == IDCMP_MOUSEBUTTONS) {
        if (imsg->Code == SELECTDOWN) {
            UBYTE btn = buttonAt(obj, mx, my);
            if (btn != BTN_NONE) {
                if (!buttonEnabled(data, btn))
                    return MUI_EventHandlerRC_Eat;
                data->pressedBtn = btn;
                data->dragging = FALSE;
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                return MUI_EventHandlerRC_Eat;
            }
            if (data->hasAudio && inWaveform(obj, mx, my)) {
                ULONG ms = seekFromX(data, obj, mx);
                data->dragging = TRUE;
                data->pressedBtn = BTN_NONE;
                seekPlayback(data, ms);
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                set(obj, MUIA_SpeechWaveform_Seek, ms);
                return MUI_EventHandlerRC_Eat;
            }
        } else if (imsg->Code == SELECTUP) {
            UBYTE pressed = data->pressedBtn;
            data->dragging = FALSE;
            data->pressedBtn = BTN_NONE;
            if (pressed != BTN_NONE) {
                MUI_Redraw(obj, MADF_DRAWUPDATE);
                if (buttonAt(obj, mx, my) == pressed &&
                    buttonEnabled(data, pressed)) {
                    ULONG command = commandForButton(data, pressed);
                    if (command != 0) {
                        doCommand(data, command);
                        MUI_Redraw(obj, MADF_DRAWUPDATE);
                        set(obj, MUIA_SpeechWaveform_Command, command);
                    }
                }
                return MUI_EventHandlerRC_Eat;
            }
        }
    } else if (imsg->Class == IDCMP_MOUSEMOVE && data->dragging &&
               data->hasAudio) {
        ULONG ms = seekFromX(data, obj, mx);
        if (ms != data->positionMs) {
            seekPlayback(data, ms);
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            set(obj, MUIA_SpeechWaveform_Seek, ms);
        }
        return MUI_EventHandlerRC_Eat;
    }

    return DoSuperMethodA(cl, obj, (Msg)msg);
}

DISPATCHER(SpeechWaveformDispatcher) {
    switch (msg->MethodID) {
    case OM_NEW:
        return (mNew(cl, obj, (APTR)msg));
    case OM_DISPOSE:
        return (mDispose(cl, obj, (APTR)msg));
    case OM_GET:
        return (mGet(cl, obj, (APTR)msg));
    case OM_SET:
        return (mSet(cl, obj, (APTR)msg));
    case MUIM_Setup:
        return (mSetup(cl, obj, (APTR)msg));
    case MUIM_Cleanup:
        return (mCleanup(cl, obj, (APTR)msg));
    case MUIM_SpeechWaveform_Play:
        return (mPlay(cl, obj, (APTR)msg));
    case MUIM_SpeechWaveform_Pause:
        return (mPause(cl, obj, (APTR)msg));
    case MUIM_SpeechWaveform_Stop:
        return (mStop(cl, obj, (APTR)msg));
    case MUIM_SpeechWaveform_Rewind:
        return (mRewind(cl, obj, (APTR)msg));
    case MUIM_SpeechWaveform_Service:
        return (mService(cl, obj, (APTR)msg));
    case MUIM_AskMinMax:
        return (mAskMinMax(cl, obj, (APTR)msg));
    case MUIM_Draw:
        return (mDraw(cl, obj, (APTR)msg));
    case MUIM_HandleEvent:
        return (mHandleEvent(cl, obj, (APTR)msg));
    }
    return DoSuperMethodA(cl, obj, msg);
}

LONG createSpeechWaveformClass(void) {
    if (speechWaveformClass != NULL)
        return RETURN_OK;

    if (!(speechWaveformClass = MUI_CreateCustomClass(
              NULL, MUIC_Area, NULL, sizeof(struct SpeechWaveformData),
              ENTRY(SpeechWaveformDispatcher))))
        return RETURN_ERROR;
    return RETURN_OK;
}

void deleteSpeechWaveformClass(void) {
    if (speechWaveformClass == NULL)
        return;
    MUI_DeleteCustomClass(speechWaveformClass);
    speechWaveformClass = NULL;
}
