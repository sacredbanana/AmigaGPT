#include <stdio.h>
#include <devices/ahi.h>
#include <devices/timer.h>
#include <proto/ahi.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>
#ifdef __AMIGAOS3__
#include <clib/compiler-specific.h>
#include <devices/audio.h>
#include <dos/dostags.h>
#include <proto/translator.h>
#elif defined(__AMIGAOS4__)
#include <exec/exec.h>
#include <dos/dos.h>
#include <devices/flite.h>
#include <proto/flite.h>
#endif
#include "AmigaGPTConfig.h"
#include "gui.h"
#include "openai.h"
#include "version.h"

#ifndef DAEMON
#include <libraries/mui.h>
#include <proto/muimaster.h>
#endif

#ifdef __AMIGAOS3__
#define TRANSLATION_BUFFER_SIZE 8192

struct Library *TranslatorBase = NULL;
static struct MsgPort *NarratorPort = NULL;
struct narrator_rb *NarratorIO = NULL;
static BYTE audioChannels[4] = {3, 5, 10, 12};
static STRPTR translationBuffer = NULL;

#define DEVICE_BEGINIO_OFFSET (-30)
#define NARRATOR_CAPTURE_MIN_BYTES (128UL * 1024UL)
#define NARRATOR_CAPTURE_MAX_BYTES (8UL * 1024UL * 1024UL)

typedef void __ASM__ (*AudioBeginIOFunction)(
    __REG__(a1, struct IORequest *request),
    __REG__(a6, struct Device *device));

static struct MsgPort *NarratorCapturePort = NULL;
static struct IOAudio *NarratorCaptureIO = NULL;
static UBYTE *narratorCaptureBuffer = NULL;
static volatile ULONG narratorCaptureLength = 0;
static ULONG narratorCaptureCapacity = 0;
static ULONG narratorCaptureUnit = 0;
static ULONG narratorCaptureSampleRate = DEFFREQ;
static UBYTE narratorCaptureOutput[1024] = {0};
static AudioFormat narratorCaptureFormat = AUDIO_FORMAT_WAV;
static volatile BOOL narratorCaptureSucceeded = FALSE;
static volatile BOOL narratorCaptureMuted = FALSE;
static volatile BOOL narratorCaptureActive = FALSE;
static volatile BOOL narratorCapturePending = FALSE;
static volatile BOOL narratorCaptureOverflow = FALSE;
static BOOL narratorCaptureDeviceOpen = FALSE;
static AudioBeginIOFunction narratorOriginalBeginIO = NULL;
static struct Task *narratorCaptureOwner = NULL;
static BYTE narratorCaptureDoneSignal = -1;
#elif defined(__AMIGAOS4__)
static struct MsgPort *fliteMessagePort = NULL;
struct FliteRequest *fliteRequest = NULL;
static struct MsgPort *fliteFileMessagePort = NULL;
static struct FliteRequest *fliteFileRequest = NULL;
static STRPTR fliteTextBuffer = NULL;
static BOOL fliteRequestPending = FALSE;
static BOOL fliteFileRequestPending = FALSE;
struct Device *FliteBase = NULL;
struct FliteIFace *IFlite = NULL;
struct FliteVoice *voice = NULL;
#endif

static struct MsgPort *AHImp = NULL;
struct AHIRequest *ahiRequest = NULL;
static BOOL ahiIOInFlight = FALSE;
#ifdef __AMIGAOS3__
static BOOL narratorIOInFlight = FALSE;
#endif
UBYTE *audioBuffer = NULL;
static ULONG playbackDataLength = 0;
static ULONG playbackAllocLength = 0;
static ULONG playbackFrequency = 0;
static ULONG playbackType = 0;
static ULONG playbackFrameSize = 0;
static ULONG playbackOffset = 0;
static ULONG playbackStartOffset = 0;
static ULONG playbackStartMs = 0;
static BOOL playbackPaused = FALSE;
static BOOL playbackIsFile = FALSE;
static UBYTE playbackPath[256];
#ifdef __AMIGAOS4__
typedef struct TimeRequest SpeechTimeRequest;
#define SPEECH_TIMER_IO(req) ((req)->Request)
#define SPEECH_TIMER_SECS(req) ((req)->Time.Seconds)
#define SPEECH_TIMER_MICRO(req) ((req)->Time.Microseconds)
#else
typedef struct timerequest SpeechTimeRequest;
#define SPEECH_TIMER_IO(req) ((req)->tr_node)
#define SPEECH_TIMER_SECS(req) ((req)->tr_time.tv_secs)
#define SPEECH_TIMER_MICRO(req) ((req)->tr_time.tv_micro)
#endif

static struct MsgPort *playTimerPort = NULL;
static SpeechTimeRequest *playTimerReq = NULL;
static BOOL playTimerPending = FALSE;

static LONG initAHIPlayback(void);
static APTR loadAudioFile(CONST_STRPTR filename, ULONG *size);

static void finishAHIPlayback(BOOL abort) {
    if (ahiRequest == NULL || !ahiIOInFlight)
        return;
    if (CheckIO((struct IORequest *)ahiRequest) == 0 && abort)
        AbortIO((struct IORequest *)ahiRequest);
    WaitIO((struct IORequest *)ahiRequest);
    ahiIOInFlight = FALSE;
}

static void startAHIWrite(APTR data, ULONG length, ULONG frequency,
                          ULONG type) {
    ahiRequest->ahir_Std.io_Command = CMD_WRITE;
    ahiRequest->ahir_Std.io_Flags = 0;
    ahiRequest->ahir_Std.io_Error = 0;
    ahiRequest->ahir_Std.io_Offset = 0;
    ahiRequest->ahir_Std.io_Data = data;
    ahiRequest->ahir_Std.io_Length = length;
    ahiRequest->ahir_Frequency = frequency;
    ahiRequest->ahir_Type = type;
    ahiRequest->ahir_Volume = 0x10000;
    ahiRequest->ahir_Position = 0x8000;
    ahiRequest->ahir_Link = NULL;
    SendIO((struct IORequest *)ahiRequest);
    ahiIOInFlight = TRUE;
}

static ULONG nowMs(void) {
    struct DateStamp stamp;

    DateStamp(&stamp);
    return ((ULONG)stamp.ds_Minute * 60000UL) + ((ULONG)stamp.ds_Tick * 20UL);
}

static void resetPlaybackMeta(void) {
    playbackDataLength = 0;
    playbackAllocLength = 0;
    playbackFrequency = 0;
    playbackType = 0;
    playbackFrameSize = 0;
    playbackOffset = 0;
    playbackStartOffset = 0;
    playbackStartMs = 0;
    playbackPaused = FALSE;
    playbackIsFile = FALSE;
    playbackPath[0] = '\0';
}

static void freePlaybackBuffer(void) {
    if (audioBuffer != NULL) {
        FreeVec(audioBuffer);
        audioBuffer = NULL;
    }
    resetPlaybackMeta();
}

static ULONG bytesFromMs(ULONG ms) {
    unsigned long long bytes;

    if (playbackFrequency == 0 || playbackFrameSize == 0)
        return 0;
    bytes = (unsigned long long)ms * playbackFrequency * playbackFrameSize;
    bytes /= 1000ULL;
    if (bytes > playbackDataLength)
        bytes = playbackDataLength;
    bytes -= bytes % playbackFrameSize;
    return (ULONG)bytes;
}

static ULONG msFromBytes(ULONG bytes) {
    unsigned long long ms;

    if (playbackFrequency == 0 || playbackFrameSize == 0)
        return 0;
    if (bytes > playbackDataLength)
        bytes = playbackDataLength;
    ms = (unsigned long long)bytes * 1000ULL;
    ms /= ((unsigned long long)playbackFrequency * playbackFrameSize);
    return (ULONG)ms;
}

static ULONG currentPlaybackBytes(void) {
    ULONG elapsed;
    ULONG bytes;

    if (!playbackIsFile || playbackDataLength == 0)
        return 0;
    if (!ahiIOInFlight)
        return playbackOffset;
    elapsed = nowMs() - playbackStartMs;
    bytes = playbackStartOffset + bytesFromMs(elapsed);
    if (bytes > playbackDataLength)
        bytes = playbackDataLength;
    return bytes;
}

static void stopPlayTimer(void) {
    if (!playTimerPending || playTimerReq == NULL)
        return;
    if (CheckIO((struct IORequest *)playTimerReq) == 0) {
        AbortIO((struct IORequest *)playTimerReq);
        WaitIO((struct IORequest *)playTimerReq);
    } else {
        WaitIO((struct IORequest *)playTimerReq);
    }
    playTimerPending = FALSE;
}

static void closePlayTimer(void) {
    stopPlayTimer();
    if (playTimerReq != NULL) {
        if (SPEECH_TIMER_IO(playTimerReq).io_Device != NULL)
            CloseDevice((struct IORequest *)playTimerReq);
        DeleteIORequest((struct IORequest *)playTimerReq);
        playTimerReq = NULL;
    }
    if (playTimerPort != NULL) {
        DeleteMsgPort(playTimerPort);
        playTimerPort = NULL;
    }
}

static BOOL initPlayTimer(void) {
    if (playTimerReq != NULL)
        return TRUE;
    playTimerPort = CreateMsgPort();
    if (playTimerPort == NULL)
        return FALSE;
    playTimerReq = (SpeechTimeRequest *)CreateIORequest(
        playTimerPort, sizeof(SpeechTimeRequest));
    if (playTimerReq == NULL) {
        DeleteMsgPort(playTimerPort);
        playTimerPort = NULL;
        return FALSE;
    }
    if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)playTimerReq,
                   0) != 0) {
        DeleteIORequest((struct IORequest *)playTimerReq);
        DeleteMsgPort(playTimerPort);
        playTimerReq = NULL;
        playTimerPort = NULL;
        return FALSE;
    }
    return TRUE;
}

static void armPlayTimer(void) {
    if (!initPlayTimer() || playTimerPending)
        return;
    SPEECH_TIMER_IO(playTimerReq).io_Command = TR_ADDREQUEST;
    SPEECH_TIMER_IO(playTimerReq).io_Flags = 0;
    SPEECH_TIMER_SECS(playTimerReq) = 0;
    SPEECH_TIMER_MICRO(playTimerReq) = 80000;
    SendIO((struct IORequest *)playTimerReq);
    playTimerPending = TRUE;
}

static void servicePlayTimer(void) {
    if (!playTimerPending || playTimerReq == NULL)
        return;
    if (CheckIO((struct IORequest *)playTimerReq) == 0)
        return;
    WaitIO((struct IORequest *)playTimerReq);
    playTimerPending = FALSE;
    if (ahiIOInFlight)
        armPlayTimer();
}

static BOOL startPlaybackFromOffset(ULONG offset) {
    if (audioBuffer == NULL || playbackAllocLength == 0 ||
        playbackFrameSize == 0)
        return FALSE;
    if (initAHIPlayback() == RETURN_ERROR)
        return FALSE;
    if (offset >= playbackDataLength)
        offset = 0;
    offset -= offset % playbackFrameSize;
    finishAHIPlayback(TRUE);
    startAHIWrite(audioBuffer + offset, playbackAllocLength - offset,
                  playbackFrequency, playbackType);
    playbackOffset = offset;
    playbackStartOffset = offset;
    playbackStartMs = nowMs();
    playbackPaused = FALSE;
    playbackIsFile = TRUE;
    armPlayTimer();
    return TRUE;
}

/**
 * The names of the speech voices
 * @see SpeechFliteVoice
 **/
const STRPTR SPEECH_FLITE_VOICE_NAMES[] = {
    [SPEECH_FLITE_VOICE_KAL] = "kal", [SPEECH_FLITE_VOICE_KAL16] = "kal16",
    [SPEECH_FLITE_VOICE_AWB] = "awb", [SPEECH_FLITE_VOICE_RMS] = "rms",
    [SPEECH_FLITE_VOICE_SLT] = "slt", NULL};

static UWORD readLittleEndian16(const UBYTE *source) {
    return (UWORD)((UWORD)source[0] | ((UWORD)source[1] << 8));
}

static ULONG readLittleEndian32(const UBYTE *source) {
    return (ULONG)source[0] | ((ULONG)source[1] << 8) |
           ((ULONG)source[2] << 16) | ((ULONG)source[3] << 24);
}

static void writeLittleEndian16(UBYTE *destination, UWORD value) {
    destination[0] = (UBYTE)(value & 0xff);
    destination[1] = (UBYTE)((value >> 8) & 0xff);
}

static void writeLittleEndian32(UBYTE *destination, ULONG value) {
    destination[0] = (UBYTE)(value & 0xff);
    destination[1] = (UBYTE)((value >> 8) & 0xff);
    destination[2] = (UBYTE)((value >> 16) & 0xff);
    destination[3] = (UBYTE)((value >> 24) & 0xff);
}

static BOOL savePcmAsWav(CONST_STRPTR filename, const UBYTE *pcmData,
                         ULONG dataLength, ULONG sampleRate,
                         UWORD bitsPerSample) {
    UBYTE header[44];
    BPTR outputFile;
    BOOL success;
    UWORD blockAlign = bitsPerSample / 8;

    memcpy(header, "RIFF", 4);
    writeLittleEndian32(header + 4, dataLength + 36);
    memcpy(header + 8, "WAVEfmt ", 8);
    writeLittleEndian32(header + 16, 16);
    writeLittleEndian16(header + 20, 1); /* PCM */
    writeLittleEndian16(header + 22, 1); /* Mono */
    writeLittleEndian32(header + 24, sampleRate);
    writeLittleEndian32(header + 28, sampleRate * blockAlign);
    writeLittleEndian16(header + 32, blockAlign);
    writeLittleEndian16(header + 34, bitsPerSample);
    memcpy(header + 36, "data", 4);
    writeLittleEndian32(header + 40, dataLength);

    outputFile = Open(filename, MODE_NEWFILE);
    if (!outputFile)
        return FALSE;

    success = Write(outputFile, header, sizeof(header)) ==
                  (LONG)sizeof(header) &&
              Write(outputFile, (APTR)pcmData, dataLength) ==
                  (LONG)dataLength;
    Close(outputFile);
    return success;
}

static BOOL bufferLooksLikeWav(const UBYTE *data, ULONG dataLength) {
    ULONG offset;
    ULONG limit;

    if (data == NULL || dataLength < 12)
        return FALSE;
    limit = dataLength > 16 ? 16 : dataLength;
    for (offset = 0; offset + 12 <= limit; offset++) {
        if (memcmp(data + offset, "RIFF", 4) == 0 &&
            memcmp(data + offset + 8, "WAVE", 4) == 0)
            return TRUE;
    }
    return FALSE;
}

static BOOL saveRawAudio(CONST_STRPTR filename, const UBYTE *data,
                         ULONG dataLength) {
    BPTR outputFile;
    BOOL success;

    if (filename == NULL || data == NULL || dataLength == 0)
        return FALSE;
    outputFile = Open(filename, MODE_NEWFILE);
    if (outputFile == 0)
        return FALSE;
    success = Write(outputFile, (APTR)data, dataLength) == (LONG)dataLength;
    Close(outputFile);
    return success;
}

static BOOL saveWavPayloadAsRaw(CONST_STRPTR wavFilename,
                                CONST_STRPTR rawFilename) {
    ULONG wavLength = 0;
    UBYTE *wav = loadAudioFile(wavFilename, &wavLength);
    ULONG offset = 12;
    BOOL success = FALSE;

    if (wav == NULL)
        return FALSE;
    if (wavLength < 12 || memcmp(wav, "RIFF", 4) != 0 ||
        memcmp(wav + 8, "WAVE", 4) != 0)
        goto done;

    while (offset + 8 <= wavLength) {
        ULONG chunkLength = readLittleEndian32(wav + offset + 4);
        ULONG dataOffset = offset + 8;
        if (dataOffset > wavLength || chunkLength > wavLength - dataOffset)
            break;
        if (memcmp(wav + offset, "data", 4) == 0) {
            success = saveRawAudio(rawFilename, wav + dataOffset, chunkLength);
            break;
        }
        offset = dataOffset + chunkLength + (chunkLength & 1);
    }

done:
    FreeVec(wav);
    return success;
}

#ifdef __AMIGAOS3__
static ULONG estimateNarratorCaptureSize(CONST_STRPTR text, UWORD rate,
                                          UWORD sampleRate) {
    ULONG wordCount = 0;
    ULONG seconds;
    ULONG size;
    BOOL inWord = FALSE;
    CONST_STRPTR cursor;

    for (cursor = text; cursor != NULL && *cursor != '\0'; cursor++) {
        BOOL separator = *cursor == ' ' || *cursor == '\t' ||
                         *cursor == '\n' || *cursor == '\r';
        if (separator) {
            inWord = FALSE;
        } else if (!inWord) {
            wordCount++;
            inWord = TRUE;
        }
    }

    if (wordCount == 0)
        wordCount = 1;
    if (rate < MINRATE || rate > MAXRATE)
        rate = DEFRATE;
    if (sampleRate < MINFREQ || sampleRate > MAXFREQ)
        sampleRate = DEFFREQ;

    /* Add 25 percent plus three seconds for pauses and synthesis tails. */
    seconds = ((wordCount * 75UL) + rate - 1) / rate + 3;
    if (seconds > NARRATOR_CAPTURE_MAX_BYTES / sampleRate)
        return NARRATOR_CAPTURE_MAX_BYTES;

    size = seconds * sampleRate;
    if (size < NARRATOR_CAPTURE_MIN_BYTES)
        size = NARRATOR_CAPTURE_MIN_BYTES;
    return size;
}

static void captureNarratorAudioRequest(struct IOAudio *request) {
    ULONG unit;
    ULONG available;
    ULONG copyLength;
    UBYTE narratorChannels;

    if (!narratorCaptureActive || request == NULL ||
        request->ioa_Request.io_Command != CMD_WRITE ||
        request->ioa_Data == NULL || request->ioa_Length == 0)
        return;

    unit = (ULONG)request->ioa_Request.io_Unit;
    narratorChannels = NarratorIO != NULL ? NarratorIO->chanmask : 0;
    if (narratorChannels != 0 && (unit & narratorChannels) == 0)
        return;

    /* narrator.device sends identical mono data to a left/right pair. */
    if (narratorCaptureUnit == 0)
        narratorCaptureUnit = unit;
    if (unit != narratorCaptureUnit)
        return;

    if (narratorCaptureLength >= narratorCaptureCapacity) {
        narratorCaptureOverflow = TRUE;
        return;
    }

    available = narratorCaptureCapacity - narratorCaptureLength;
    copyLength = request->ioa_Length;
    if (copyLength > available) {
        copyLength = available;
        narratorCaptureOverflow = TRUE;
    }

    memcpy(narratorCaptureBuffer + narratorCaptureLength, request->ioa_Data,
           copyLength);
    narratorCaptureLength += copyLength;

    /* narrator.device has no file-only output mode. Let audio.device complete
       each request normally, but silence the hardware while generating a
       file. The unmodified sample bytes above are still captured. */
    if (narratorCaptureMuted)
        request->ioa_Volume = 0;
}

static void __ASM__ narratorAudioBeginIOHook(
    __REG__(a1, struct IORequest *request),
    __REG__(a6, struct Device *device)) {
    captureNarratorAudioRequest((struct IOAudio *)request);
    if (narratorOriginalBeginIO != NULL)
        narratorOriginalBeginIO(request, device);
}

static void releaseNarratorCaptureResources(void) {
    if (narratorCaptureDeviceOpen && NarratorCaptureIO != NULL) {
        CloseDevice((struct IORequest *)NarratorCaptureIO);
        narratorCaptureDeviceOpen = FALSE;
    }
    if (NarratorCaptureIO != NULL) {
        DeleteIORequest((struct IORequest *)NarratorCaptureIO);
        NarratorCaptureIO = NULL;
    }
    if (NarratorCapturePort != NULL) {
        DeleteMsgPort(NarratorCapturePort);
        NarratorCapturePort = NULL;
    }
    if (narratorCaptureBuffer != NULL) {
        FreeVec(narratorCaptureBuffer);
        narratorCaptureBuffer = NULL;
    }
}

static void finishNarratorCapture(void) {
    ULONG i;

    Forbid();
    Disable();
    narratorCaptureActive = FALSE;
    if (narratorOriginalBeginIO != NULL && NarratorCaptureIO != NULL) {
        SetFunction(
            (struct Library *)NarratorCaptureIO->ioa_Request.io_Device,
            DEVICE_BEGINIO_OFFSET, (ULONG (*)())narratorOriginalBeginIO);
    }
    Enable();
    Permit();
    narratorOriginalBeginIO = NULL;

    narratorCaptureSucceeded = FALSE;
    /* Paula/audio.device samples are signed; 8-bit WAV PCM is unsigned. */
    if (narratorCaptureFormat == AUDIO_FORMAT_WAV &&
        narratorCaptureBuffer != NULL) {
        for (i = 0; i < narratorCaptureLength; i++)
            narratorCaptureBuffer[i] ^= 0x80;
    }

    if (narratorCaptureLength > 0 && narratorCaptureOutput[0] != '\0') {
        if (narratorCaptureFormat == AUDIO_FORMAT_PCM) {
            narratorCaptureSucceeded = saveRawAudio(
                narratorCaptureOutput, narratorCaptureBuffer,
                narratorCaptureLength);
        } else {
            narratorCaptureSucceeded = savePcmAsWav(
                narratorCaptureOutput, narratorCaptureBuffer,
                narratorCaptureLength, narratorCaptureSampleRate, 8);
        }
    }
    narratorCaptureMuted = FALSE;
    releaseNarratorCaptureResources();
}

static void narratorCaptureProcess(void) {
    struct Task *owner = narratorCaptureOwner;
    ULONG doneMask = 1UL << narratorCaptureDoneSignal;

    while (CheckIO((struct IORequest *)NarratorIO) == 0)
        Delay(1);
    WaitIO((struct IORequest *)NarratorIO);
    finishNarratorCapture();
    if (owner != NULL)
        Signal(owner, doneMask);
}

static void waitForNarratorCapture(BOOL abortPlayback) {
    ULONG doneMask;

    if (!narratorCapturePending)
        return;

    doneMask = 1UL << narratorCaptureDoneSignal;
    if (abortPlayback && NarratorIO != NULL &&
        CheckIO((struct IORequest *)NarratorIO) == 0) {
        AbortIO((struct IORequest *)NarratorIO);
    }
    Wait(doneMask);
    FreeSignal(narratorCaptureDoneSignal);
    narratorCaptureDoneSignal = -1;
    narratorCaptureOwner = NULL;
    narratorCapturePending = FALSE;
}

static BOOL prepareNarratorCapture(CONST_STRPTR text, UWORD rate,
                                   UWORD sampleRate, CONST_STRPTR output,
                                   AudioFormat format) {
    if (output == NULL || strlen(output) == 0)
        return FALSE;
    if (sampleRate < MINFREQ || sampleRate > MAXFREQ)
        sampleRate = DEFFREQ;

    narratorCaptureCapacity =
        estimateNarratorCaptureSize(text, rate, sampleRate);
    narratorCaptureBuffer = AllocVec(narratorCaptureCapacity, MEMF_PUBLIC);
    if (narratorCaptureBuffer == NULL)
        goto failure;

    NarratorCapturePort = CreateMsgPort();
    if (NarratorCapturePort == NULL)
        goto failure;
    NarratorCaptureIO = (struct IOAudio *)CreateIORequest(
        NarratorCapturePort, sizeof(struct IOAudio));
    if (NarratorCaptureIO == NULL)
        goto failure;
    NarratorCaptureIO->ioa_Data = NULL;
    NarratorCaptureIO->ioa_Length = 0;
    NarratorCaptureIO->ioa_AllocKey = 0;
    if (OpenDevice(AUDIONAME, 0, (struct IORequest *)NarratorCaptureIO, 0) !=
        0)
        goto failure;
    narratorCaptureDeviceOpen = TRUE;

    narratorCaptureDoneSignal = AllocSignal(-1);
    if (narratorCaptureDoneSignal < 0)
        goto failure;

    narratorCaptureLength = 0;
    narratorCaptureUnit = 0;
    narratorCaptureSampleRate = sampleRate;
    strncpy(narratorCaptureOutput, output,
            sizeof(narratorCaptureOutput) - 1);
    narratorCaptureOutput[sizeof(narratorCaptureOutput) - 1] = '\0';
    narratorCaptureFormat = format;
    narratorCaptureSucceeded = FALSE;
    narratorCaptureMuted = TRUE;
    narratorCaptureOverflow = FALSE;
    narratorCaptureOwner = FindTask(NULL);

    Forbid();
    Disable();
    narratorOriginalBeginIO = (AudioBeginIOFunction)SetFunction(
        (struct Library *)NarratorCaptureIO->ioa_Request.io_Device,
        DEVICE_BEGINIO_OFFSET, (ULONG (*)())narratorAudioBeginIOHook);
    narratorCaptureActive = TRUE;
    Enable();
    Permit();
    narratorCapturePending = TRUE;
    return TRUE;

failure:
    narratorCaptureMuted = FALSE;
    if (narratorCaptureDoneSignal >= 0) {
        FreeSignal(narratorCaptureDoneSignal);
        narratorCaptureDoneSignal = -1;
    }
    releaseNarratorCaptureResources();
    return FALSE;
}

static void startNarratorCaptureProcess(void) {
    if (CreateNewProcTags(NP_Entry, narratorCaptureProcess, NP_Name,
                          "AmigaGPT narrator capture", NP_StackSize, 8192,
                          TAG_DONE) == NULL) {
        WaitIO((struct IORequest *)NarratorIO);
        finishNarratorCapture();
        FreeSignal(narratorCaptureDoneSignal);
        narratorCaptureDoneSignal = -1;
        narratorCaptureOwner = NULL;
        narratorCapturePending = FALSE;
    }
}
#elif defined(__AMIGAOS4__)
static void finishFliteRequest(struct FliteRequest *request, BOOL *pending,
                               BOOL abortRequest) {
    if (request == NULL || !*pending)
        return;

    if (abortRequest && CheckIO((struct IORequest *)request) == 0)
        AbortIO((struct IORequest *)request);
    WaitIO((struct IORequest *)request);
    *pending = FALSE;
}

static void finishFliteRequests(BOOL abortRequests) {
    finishFliteRequest(fliteFileRequest, &fliteFileRequestPending,
                       abortRequests);
    finishFliteRequest(fliteRequest, &fliteRequestPending, abortRequests);

    if (fliteTextBuffer != NULL) {
        FreeVec(fliteTextBuffer);
        fliteTextBuffer = NULL;
    }
}
#endif

/**
 * The names of the speech systems
 * @see SpeechSystem
 **/
const STRPTR SPEECH_SYSTEM_NAMES[] = {[SPEECH_SYSTEM_34] = "Workbench 1.x v34",
                                      [SPEECH_SYSTEM_37] = "Workbench 2.0 v37",
                                      [SPEECH_SYSTEM_FLITE] = "Flite",
                                      [SPEECH_SYSTEM_OPENAI] = "OpenAI",
                                      [SPEECH_SYSTEM_ELEVENLABS] = "ElevenLabs",
                                      [SPEECH_SYSTEM_XAI] = "SpaceXAI",
                                      [SPEECH_SYSTEM_OPENVOX] = "OpenVox",
                                      NULL};

/**
 * The names of the audio formats
 * @see AudioFormat
 **/
const STRPTR AUDIO_FORMAT_NAMES[] = {[AUDIO_FORMAT_PCM] = "pcm",
                                     [AUDIO_FORMAT_MP3] = "mp3",
                                     [AUDIO_FORMAT_OPUS] = "opus",
                                     [AUDIO_FORMAT_WAV] = "wav",
                                     [AUDIO_FORMAT_AAC] = "aac",
                                     [AUDIO_FORMAT_FLAC] = "flac",
                                     NULL};

static LONG initAHIPlayback(void) {
    if (ahiRequest != NULL)
        return RETURN_OK;

    AHImp = CreateMsgPort();
    if (AHImp == NULL)
        return RETURN_ERROR;
    ahiRequest = (struct AHIRequest *)CreateIORequest(
        AHImp, sizeof(struct AHIRequest));
    if (ahiRequest == NULL) {
        DeleteMsgPort(AHImp);
        AHImp = NULL;
        return RETURN_ERROR;
    }
    ahiRequest->ahir_Version = 4;
    ahiRequest->ahir_Std.io_Message.mn_ReplyPort = AHImp;
    ahiRequest->ahir_Std.io_Command = CMD_WRITE;
    ahiRequest->ahir_Std.io_Data = NULL;
    ahiRequest->ahir_Std.io_Length = 0;
    ahiRequest->ahir_Frequency = 24000;
    ahiRequest->ahir_Type = AHIST_M16S;
    ahiRequest->ahir_Volume = 0x10000;
    ahiRequest->ahir_Position = 0x8000;

    if (OpenDevice(AHINAME, AHI_DEFAULT_UNIT,
                   (struct IORequest *)ahiRequest, 0L) != 0) {
        DeleteIORequest((struct IORequest *)ahiRequest);
        DeleteMsgPort(AHImp);
        ahiRequest = NULL;
        AHImp = NULL;
        displayError(STRING_ERROR_AHI_DEVICE_OPEN);
        return RETURN_ERROR;
    }
    return RETURN_OK;
}

/**
 * Initialise the speech system
 * @param speechSystem the speech system to use
 * @return RETURN_OK on success, RETURN_ERROR on failure
 **/
LONG initSpeech(SpeechSystem speechSystem) {
    if (speechSystem == SPEECH_SYSTEM_OPENAI ||
        speechSystem == SPEECH_SYSTEM_ELEVENLABS ||
        speechSystem == SPEECH_SYSTEM_XAI ||
        speechSystem == SPEECH_SYSTEM_OPENVOX) {
        if (initAHIPlayback() == RETURN_ERROR) {
            configSetSpeechEnabled(FALSE);
            return RETURN_ERROR;
        }

        return RETURN_OK;
    }

#ifdef __AMIGAOS3__
    if (!translationBuffer)
        translationBuffer = AllocVec(TRANSLATION_BUFFER_SIZE, MEMF_ANY);
    if (!(NarratorPort = CreateMsgPort())) {
        displayError(STRING_ERROR_NARRATOR_PORT);
        configSetSpeechEnabled(FALSE);
        return RETURN_ERROR;
    }

    if (!(NarratorIO =
              CreateIORequest(NarratorPort, sizeof(struct narrator_rb)))) {
        displayError(STRING_ERROR_NARRATOR_IO_REQUEST);
        configSetSpeechEnabled(FALSE);
        return RETURN_ERROR;
    }

    switch (speechSystem) {
    case SPEECH_SYSTEM_34:
        if (OpenDevice("AMIGAGPT:devs/speech/34/narrator.device", 0,
                       (struct IORequest *)NarratorIO, 0L) != 0) {
            displayError(STRING_ERROR_NARRATOR_34_DEVICE_OPEN);
            configSetSpeechEnabled(FALSE);
            return RETURN_ERROR;
        }
        break;
    case SPEECH_SYSTEM_37:
        if (OpenDevice("AMIGAGPT:devs/speech/37/narrator.device", 0,
                       (struct IORequest *)NarratorIO, 0L) != 0) {
            displayError(STRING_ERROR_NARRATOR_37_DEVICE_OPEN);
            configSetSpeechEnabled(FALSE);
            return RETURN_ERROR;
        }
        NarratorIO->flags = NDF_NEWIORB;
        break;
    }

    if ((TranslatorBase =
             (struct Library *)OpenLibrary("translator.library", 42)) == NULL) {
        displayError(STRING_ERROR_TRANSLATOR_LIB_OPEN);
        configSetSpeechEnabled(FALSE);
        return RETURN_ERROR;
    }
#elif defined(__AMIGAOS4__)
    /* Note: we could use a struct IOStdReq here if we didn't need any of
       the more "advanced" features of the device. */
    /* Any additional fields will be ignored by flite.device 52.1 */
    fliteMessagePort = AllocSysObject(ASOT_PORT, NULL);
    fliteRequest = AllocSysObjectTags(
        ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct FliteRequest),
        ASOIOR_ReplyPort, fliteMessagePort, TAG_END);

    if (fliteRequest) {
        /* minimum version/revision */
        /* This information will unfortunately be ignored by 52.1, */
        /* so an additional check after OpenDevice() may be in order. */
        fliteRequest->fr_Version = 53;
        fliteRequest->fr_Revision = 1;

        if (OpenDevice("flite.device", 0, (struct IORequest *)fliteRequest,
                       0) == IOERR_SUCCESS) {
            FliteBase = fliteRequest->fr_Std.io_Device;
            /* "main" interface is only available in 53.1 and higher versions */
            IFlite = (struct FliteIFace *)GetInterface(
                (struct Library *)FliteBase, "main", 1, NULL);
            if (!IFlite) {
                displayError(STRING_ERROR_FLITE_INTERFACE_OPEN);
                configSetSpeechEnabled(FALSE);
                return RETURN_ERROR;
            }

            /* flite.device supports only one output per request. Keep a
               second request ready so speech can be written as RIFF-WAVE and
               then played normally through AHI. */
            fliteFileMessagePort = AllocSysObject(ASOT_PORT, NULL);
            if (fliteFileMessagePort != NULL) {
                fliteFileRequest = AllocSysObjectTags(
                    ASOT_IOREQUEST, ASOIOR_Size, sizeof(struct FliteRequest),
                    ASOIOR_ReplyPort, fliteFileMessagePort, TAG_END);
            }
            if (fliteFileRequest != NULL) {
                fliteFileRequest->fr_Version = 53;
                fliteFileRequest->fr_Revision = 1;
                if (OpenDevice("flite.device", 0,
                               (struct IORequest *)fliteFileRequest,
                               0) != IOERR_SUCCESS) {
                    FreeSysObject(ASOT_IOREQUEST, fliteFileRequest);
                    fliteFileRequest = NULL;
                }
            }
            if (fliteFileRequest == NULL && fliteFileMessagePort != NULL) {
                FreeSysObject(ASOT_PORT, fliteFileMessagePort);
                fliteFileMessagePort = NULL;
            }
        } else {
            displayError(STRING_ERROR_FLITE_DEVICE_OPEN);
            configSetSpeechEnabled(FALSE);
            return RETURN_ERROR;
        }
    } else {
        PrintFault(ERROR_NO_FREE_STORE, STRING_APP_NAME);
        configSetSpeechEnabled(FALSE);
        return RETURN_ERROR;
    }
#endif

    return RETURN_OK;
}

/**
 * Lazily initialize the speech system for a specific request, without
 * disturbing the user's "speech enabled" preference: initSpeech() disables
 * that preference on failure, which is correct when it's called at startup
 * or from the settings requester, but not when it's opportunistically
 * triggered by a single speak request (e.g. an ARexx SPEAKTEXT call for a
 * profile that isn't the app's configured default).
 **/
static LONG lazyInitSpeech(SpeechSystem speechSystem) {
    ULONG wasEnabled = configGetSpeechEnabled();
    LONG result = initSpeech(speechSystem);
    configSetSpeechEnabled(wasEnabled);
    return result;
}

/**
 * Close the speech system
 **/
void closeSpeech() {
    closePlayTimer();
    if (ahiRequest) {
        finishAHIPlayback(TRUE);
        CloseDevice((struct IORequest *)ahiRequest);
        DeleteIORequest((struct IORequest *)ahiRequest);
        DeleteMsgPort(AHImp);
    }
    ahiRequest = NULL;
    AHImp = NULL;
    ahiIOInFlight = FALSE;
    freePlaybackBuffer();
#ifdef __AMIGAOS3__
    waitForNarratorCapture(TRUE);
    if (TranslatorBase) {
        CloseLibrary(TranslatorBase);
        Forbid();
        RemLibrary(TranslatorBase);
        Permit();
        TranslatorBase = NULL;
    }
    if (NarratorIO) {
        if (CheckIO((struct IORequest *)NarratorIO) == 0) {
            AbortIO((struct IORequest *)NarratorIO);
            WaitIO((struct IORequest *)NarratorIO);
        }
        narratorIOInFlight = FALSE;
        if (((struct IORequest *)NarratorIO)->io_Device != NULL) {
            CloseDevice((struct IORequest *)NarratorIO);
            Forbid();
            RemDevice(
                (struct Device *)((struct IORequest *)NarratorIO)->io_Device);
            Permit();
        }

        DeleteIORequest((struct IORequest *)NarratorIO);
        NarratorIO = NULL;
    }

    if (NarratorPort) {
        DeleteMsgPort(NarratorPort);
        NarratorPort = NULL;
    }

    if (translationBuffer) {
        FreeVec(translationBuffer);
        translationBuffer = NULL;
    }
#elif defined(__AMIGAOS4__)
    finishFliteRequests(TRUE);
    if (IFlite && voice) {
        CloseVoice(voice);
        voice = NULL;
    }
    if (IFlite) {
        DropInterface((struct Interface *)IFlite);
        IFlite = NULL;
    }
    if (fliteFileRequest) {
        CloseDevice((struct IORequest *)fliteFileRequest);
        FreeSysObject(ASOT_IOREQUEST, fliteFileRequest);
        fliteFileRequest = NULL;
    }
    if (fliteFileMessagePort) {
        FreeSysObject(ASOT_PORT, fliteFileMessagePort);
        fliteFileMessagePort = NULL;
    }
    if (fliteRequest) {
        CloseDevice((struct IORequest *)fliteRequest);
        FreeSysObject(ASOT_IOREQUEST, fliteRequest);
        fliteRequest = NULL;
    }
    if (fliteMessagePort) {
        FreeSysObject(ASOT_PORT, fliteMessagePort);
        fliteMessagePort = NULL;
    }
#endif
}

/**
 * Speak the given text aloud
 * @param text the text to speak
 * @param output the output file to save the OpenAI audio to. If NULL, the
 * audio will be played through AHI.
 * @param audioFormat the audio format to save the audio to
 **/
void speakText(STRPTR text, CONST_STRPTR output, AudioFormat *audioFormat) {
    struct SpeechRequestSettings settings;
    configGetSpeechRequestSettings(&settings);
    speakTextWithSettings(text, output, audioFormat, &settings);
    configFreeSpeechRequestSettings(&settings);
}

BOOL speakTextWithSettings(STRPTR text, CONST_STRPTR output,
                           AudioFormat *audioFormat,
                           const struct SpeechRequestSettings *settings) {
    if (settings == NULL)
        return FALSE;

    SpeechSystem speechSystem = settings->speechSystem;
    BOOL saveToFile = output != NULL && strlen(output) > 0;
    AudioFormat requestedFormat =
        audioFormat != NULL ? *audioFormat : AUDIO_FORMAT_WAV;

    if (saveToFile && requestedFormat != AUDIO_FORMAT_WAV &&
        requestedFormat != AUDIO_FORMAT_PCM)
        return FALSE;

    if (speechSystem == SPEECH_SYSTEM_OPENAI ||
        speechSystem == SPEECH_SYSTEM_ELEVENLABS ||
        speechSystem == SPEECH_SYSTEM_XAI ||
        speechSystem == SPEECH_SYSTEM_OPENVOX) {
        if (!saveToFile && ahiRequest == NULL) {
            if (lazyInitSpeech(speechSystem) == RETURN_ERROR)
                return FALSE; /* initSpeech already displayed a specific error */
            if (ahiRequest == NULL) {
                displayError(STRING_ERROR_SPEECH_NOT_INITIALIZED);
                return FALSE;
            }
        }

        if (speechSystem == SPEECH_SYSTEM_OPENAI) {
            if (settings->authorizationType != AUTHORIZATION_TYPE_NONE &&
                (settings->openAiApiKey == NULL ||
                 strlen(settings->openAiApiKey) == 0)) {
                displayError(STRING_ERROR_NO_API_KEY);
                return FALSE;
            }
        } else if (speechSystem == SPEECH_SYSTEM_ELEVENLABS) {
            if (settings->authorizationType != AUTHORIZATION_TYPE_NONE &&
                (settings->elevenLabsApiKey == NULL ||
                 strlen(settings->elevenLabsApiKey) == 0)) {
                displayError(STRING_ERROR_NO_API_KEY);
                return FALSE;
            }
        } else if (speechSystem == SPEECH_SYSTEM_XAI) {
            if (settings->authorizationType != AUTHORIZATION_TYPE_NONE &&
                (settings->xaiApiKey == NULL ||
                 strlen(settings->xaiApiKey) == 0)) {
                displayError(STRING_ERROR_NO_API_KEY);
                return FALSE;
            }
        } else if (speechSystem == SPEECH_SYSTEM_OPENVOX) {
            if (settings->authorizationType != AUTHORIZATION_TYPE_NONE &&
                (settings->openVoxApiKey == NULL ||
                 strlen(settings->openVoxApiKey) == 0)) {
                displayError(STRING_ERROR_NO_API_KEY);
                return FALSE;
            }
        }

        AudioFormat requestFormat =
            saveToFile ? requestedFormat : AUDIO_FORMAT_PCM;
        audioFormat = &requestFormat;

        finishAHIPlayback(TRUE);
        stopPlayTimer();
        freePlaybackBuffer();

        ULONG audioLength = 0;
        ULONG playbackFrequency = 24000;

        if (speechSystem == SPEECH_SYSTEM_OPENAI) {
            audioBuffer = postTextToSpeechRequestToOpenAI(
                text, settings->openAiTtsModelId, settings->openAiTtsVoice,
                settings->openAiVoiceInstructions, settings->host,
                settings->port, settings->useSSL, settings->apiEndpointUrl,
                settings->authorizationType, settings->openAiApiKey,
                &audioLength, configGetProxyEnabled(), configGetProxyHost(),
                configGetProxyPort(), configGetProxyUsesSSL(),
                configGetProxyRequiresAuth(), configGetProxyUsername(),
                configGetProxyPassword(), audioFormat);
        } else if (speechSystem == SPEECH_SYSTEM_ELEVENLABS) {
            audioBuffer = postTextToSpeechRequestToElevenLabs(
                text, settings->elevenLabsVoiceID, settings->elevenLabsModel,
                settings->host, settings->port, settings->useSSL,
                settings->apiEndpointUrl, settings->authorizationType,
                settings->elevenLabsApiKey, &audioLength,
                configGetProxyEnabled(), configGetProxyHost(),
                configGetProxyPort(), configGetProxyUsesSSL(),
                configGetProxyRequiresAuth(), configGetProxyUsername(),
                configGetProxyPassword(), audioFormat);
        } else if (speechSystem == SPEECH_SYSTEM_XAI) {
            CONST_STRPTR voiceId = settings->xaiVoiceId;
            if (voiceId == NULL || strlen(voiceId) == 0) {
                XAITTSVoice v = settings->xaiVoice;
                if (v >= 0 && XAI_TTS_VOICE_NAMES[v] != NULL)
                    voiceId = XAI_TTS_VOICE_NAMES[v];
                else
                    voiceId = XAI_TTS_VOICE_NAMES[XAI_TTS_VOICE_EVE];
            }
            audioBuffer = postTextToSpeechRequestToXAI(
                text, voiceId, settings->xaiLanguage, settings->host,
                settings->port, settings->useSSL, settings->apiEndpointUrl,
                settings->authorizationType, settings->xaiApiKey, &audioLength,
                configGetProxyEnabled(), configGetProxyHost(),
                configGetProxyPort(), configGetProxyUsesSSL(),
                configGetProxyRequiresAuth(), configGetProxyUsername(),
                configGetProxyPassword(), audioFormat);
        } else if (speechSystem == SPEECH_SYSTEM_OPENVOX) {
            audioBuffer = postTextToSpeechRequestToOpenVox(
                text, settings->openVoxModel, settings->openVoxVoice,
                settings->openVoxLanguage, settings->host, settings->port,
                settings->useSSL, settings->apiEndpointUrl,
                settings->authorizationType, settings->openVoxApiKey,
                &audioLength, &playbackFrequency, configGetProxyEnabled(),
                configGetProxyHost(), configGetProxyPort(),
                configGetProxyUsesSSL(), configGetProxyRequiresAuth(),
                configGetProxyUsername(), configGetProxyPassword(),
                audioFormat);
        }

        if (!audioBuffer) {
            return FALSE;
        }

        if (saveToFile) {
            BOOL saved;
            if (requestedFormat == AUDIO_FORMAT_WAV) {
                saved = bufferLooksLikeWav(audioBuffer, audioLength)
                            ? saveRawAudio(output, audioBuffer, audioLength)
                            : savePcmAsWav(output, audioBuffer, audioLength,
                                           playbackFrequency, 16);
            } else {
                saved = saveRawAudio(output, audioBuffer, audioLength);
            }
            FreeVec(audioBuffer);
            audioBuffer = NULL;
            resetPlaybackMeta();
            if (!saved)
                displayError(STRING_ERROR_FILE_OPEN);
            return saved;
        }

        if (requestFormat == AUDIO_FORMAT_PCM && audioLength >= 2) {
// Convert to big endian
#ifdef __AMIGAOS3__
            __asm__ __volatile__(
                "lea %a1, %%a0\n"   // Load buffer address into A0
                "move.l %0, %%d1\n" // Load fileSize into D1
                "lsr.l #1, %%d1\n"  // fileSize / 2, since we're processing 2
                                    // bytes at a time

                "1:\n"
                "move.w (%%a0), %%d0\n"  // Load the word from the buffer
                                         // into D0
                "rol.w #8, %%d0\n"       // Rotate left by 8 bits to swap the
                                         // bytes
                "move.w %%d0, (%%a0)+\n" // Store the swapped word back and
                                         // increment address
                "subq.l #1, %%d1\n"      // Decrement counter
                "bne.b 1b\n"             // Repeat if not done

                :                                    // No output operands
                : "d"(audioLength), "a"(audioBuffer) // Input operands
                : "d0", "d1", "a0", "memory"         // Clobber list
            );
#else
            for (ULONG i = 0; i + 1 < audioLength; i += 2) {
                UBYTE temp = audioBuffer[i];
                audioBuffer[i] = audioBuffer[i + 1];
                audioBuffer[i + 1] = temp;
            }
#endif
        }

        // Add 2s of silence to the audio buffer to make sure AHI plays the
        // entire audio buffer. Unsure if this is an AHI bug or an emulator
        // bug.
        {
            ULONG padBytes =
                playbackFrequency * 4; /* 2s, 16-bit mono */
            UBYTE *padded =
                AllocVec(audioLength + padBytes, MEMF_ANY | MEMF_CLEAR);
            if (padded) {
                memcpy(padded, audioBuffer, audioLength);
                FreeVec(audioBuffer);
                audioBuffer = padded;
                audioLength += padBytes;
            } else {
                displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
                FreeVec(audioBuffer);
                audioBuffer = NULL;
                resetPlaybackMeta();
                return FALSE;
            }
        }

        startAHIWrite(audioBuffer, audioLength, playbackFrequency,
                      AHIST_M16S);
        playbackIsFile = FALSE;
        playbackPaused = FALSE;

        return TRUE;
    }
#ifdef __AMIGAOS3__
    if (speechSystem == SPEECH_SYSTEM_34 || speechSystem == SPEECH_SYSTEM_37) {
        BOOL capturePrepared = FALSE;

        if (NarratorIO == NULL) {
            if (lazyInitSpeech(speechSystem) == RETURN_ERROR)
                return FALSE; /* initSpeech already displayed a specific error */
            if (NarratorIO == NULL) {
                displayError(STRING_ERROR_SPEECH_NOT_INITIALIZED);
                return FALSE;
            }
        }

        waitForNarratorCapture(TRUE);
        if (CheckIO((struct IORequest *)NarratorIO) == 0) {
            AbortIO((struct IORequest *)NarratorIO);
            WaitIO((struct IORequest *)NarratorIO);
        }

        STRPTR accent = settings->accentPath;
        if (accent == NULL || strlen(accent) == 0)
            accent = "american.accent";
        LoadAccent(accent);
        SetAccent(accent);
        Translate(text, strlen(text), translationBuffer,
                  TRANSLATION_BUFFER_SIZE - 1);
        NarratorIO->ch_masks = audioChannels;
        NarratorIO->nm_masks = sizeof(audioChannels);
        NarratorIO->rate = settings->narratorRate;
        NarratorIO->pitch = settings->narratorPitch;
        NarratorIO->mode =
            settings->narratorMode ? 1 : 0; /* 0 natural, 1 robotic */
        NarratorIO->sex = settings->narratorSex ? 1 : 0; /* 0 male, 1 female */
        NarratorIO->message.io_Command = CMD_WRITE;
        NarratorIO->message.io_Data = translationBuffer;
        NarratorIO->message.io_Length = strlen(translationBuffer);
        if (saveToFile) {
            capturePrepared = prepareNarratorCapture(
                text, NarratorIO->rate, NarratorIO->sampfreq, output,
                requestedFormat);
            if (!capturePrepared)
                return FALSE;
        }
        SendIO((struct IORequest *)NarratorIO);
        if (capturePrepared) {
            startNarratorCaptureProcess();
            waitForNarratorCapture(FALSE);
            return narratorCaptureSucceeded;
        }
        narratorIOInFlight = TRUE;
        return TRUE;
    }
#elif defined(__AMIGAOS4__)
    if (speechSystem == SPEECH_SYSTEM_FLITE) {
        if (fliteRequest == NULL) {
            if (lazyInitSpeech(speechSystem) == RETURN_ERROR)
                return FALSE; /* initSpeech already displayed a specific error */
            if (fliteRequest == NULL) {
                displayError(STRING_ERROR_SPEECH_NOT_INITIALIZED);
                return FALSE;
            }
        }

        finishFliteRequests(TRUE);
        if (IFlite && voice)
            CloseVoice(voice);
        voice = NULL;
        UBYTE voiceName[32];
        snprintf(voiceName, 32, "%s.voice\0",
                 SPEECH_FLITE_VOICE_NAMES[settings->fliteVoice]);
        voice = OpenVoice(voiceName);
        if (!voice) {
            displayError(STRING_ERROR_VOICE_OPEN);
            return FALSE;
        }

        if (saveToFile && fliteFileRequest == NULL) {
            displayError(STRING_ERROR_FLITE_DEVICE_OPEN);
            return FALSE;
        }

        if (fliteFileRequest != NULL) {
            ULONG textLength = strlen(text) + 1;
            fliteTextBuffer = AllocVec(textLength, MEMF_SHARED);
            if (fliteTextBuffer != NULL)
                memcpy(fliteTextBuffer, text, textLength);
            else
                displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
        }

        if (saveToFile && fliteFileRequest != NULL &&
            fliteTextBuffer != NULL) {
            CONST_STRPTR fliteOutput = output;
            CONST_STRPTR temporaryWav = "T:AmigaGPT-flite-output.wav";
            if (requestedFormat == AUDIO_FORMAT_PCM)
                fliteOutput = temporaryWav;
            fliteFileRequest->fr_Std.io_Command = CMD_WRITE;
            fliteFileRequest->fr_Std.io_Data = (APTR)fliteTextBuffer;
            fliteFileRequest->fr_Std.io_Length = ~0;
            fliteFileRequest->fr_Input = FLITE_INPUT_TEXT;
            fliteFileRequest->fr_Output = FLITE_OUTPUT_FILE;
            fliteFileRequest->fr_Lock = GetCurrentDir();
            fliteFileRequest->fr_Filename = fliteOutput;
            fliteFileRequest->fr_Voice = voice;
            SendIO((struct IORequest *)fliteFileRequest);
            fliteFileRequestPending = TRUE;
            finishFliteRequest(fliteFileRequest, &fliteFileRequestPending,
                               FALSE);
            BOOL saved = fliteFileRequest->fr_Std.io_Error == 0;
            if (saved && requestedFormat == AUDIO_FORMAT_PCM)
                saved = saveWavPayloadAsRaw(temporaryWav, output);
            if (requestedFormat == AUDIO_FORMAT_PCM)
                Delete(temporaryWav);
            FreeVec(fliteTextBuffer);
            fliteTextBuffer = NULL;
            return saved;
        }

        fliteRequest->fr_Std.io_Command = CMD_WRITE;
        fliteRequest->fr_Std.io_Data =
            (APTR)(fliteTextBuffer != NULL ? fliteTextBuffer : text);
        fliteRequest->fr_Std.io_Length = ~0; /* io_Data is NULL-terminated */
        fliteRequest->fr_Input = FLITE_INPUT_TEXT;
        fliteRequest->fr_Output = FLITE_OUTPUT_AHI;
        fliteRequest->fr_Voice = voice;
        SendIO((struct IORequest *)fliteRequest);
        fliteRequestPending = TRUE;
        return TRUE;
    }
#endif
    return FALSE;
}

/**
 * Load an audio file into memory
 * @param filename the name of the file to load
 * @param size the size of the file
 * @return a pointer to the loaded audio data, or NULL on failure. Free the
 * buffer with FreeVec() when done.
 */
static APTR loadAudioFile(CONST_STRPTR filename, ULONG *size) {
    BPTR fileHandle;
    APTR buffer = NULL;
    LONG fileSize;

    // Attempt to open the audio file
    fileHandle = Open(filename, MODE_OLDFILE);
    if (!fileHandle) {
        displayError(STRING_ERROR_FILE_OPEN);
        return NULL;
    }

#ifdef __AMIGAOS3__
    Seek(fileHandle, 0, OFFSET_END);
    fileSize = Seek(fileHandle, 0, OFFSET_BEGINNING);
#elif defined(__AMIGAOS4__)
    fileSize = (LONG)GetFileSize(fileHandle);
#else
    {
        struct FileInfoBlock fib;
        ExamineFH64(fileHandle, &fib, NULL);
        fileSize = (LONG)fib.fib_Size;
    }
#endif

    if (fileSize > 0) {
        // Allocate buffer for audio data
        buffer = AllocVec(fileSize, MEMF_PUBLIC | MEMF_CLEAR);
        if (!buffer) {
            displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
        } else {
            // Read file content into buffer
            LONG bytesRead = Read(fileHandle, buffer, fileSize);
            if (bytesRead <= 0) {
                displayError(STRING_ERROR_FILE_READ);
                FreeVec(buffer);
                buffer = NULL;
            } else {
                *size = (ULONG)bytesRead;
            }
        }
    }

    // Close the file
    Close(fileHandle);

    return buffer;
}

void stopSpeech() {
#ifdef __AMIGAOS3__
    waitForNarratorCapture(TRUE);
    if (NarratorIO != NULL &&
        ((struct IORequest *)NarratorIO)->io_Device != NULL &&
        CheckIO((struct IORequest *)NarratorIO) == 0) {
        AbortIO((struct IORequest *)NarratorIO);
        WaitIO((struct IORequest *)NarratorIO);
    }
    narratorIOInFlight = FALSE;
#elif defined(__AMIGAOS4__)
    finishFliteRequests(TRUE);
#endif
    stopPlayTimer();
    finishAHIPlayback(TRUE);
    if (playbackIsFile) {
        playbackOffset = 0;
        playbackStartOffset = 0;
        playbackPaused = FALSE;
    } else {
        freePlaybackBuffer();
    }
}

BOOL isSpeechPlaying(void) {
    if (ahiIOInFlight) {
        if (CheckIO((struct IORequest *)ahiRequest) == 0)
            return TRUE;
        finishAHIPlayback(FALSE);
        stopPlayTimer();
        if (playbackIsFile) {
            playbackOffset = playbackDataLength;
            playbackPaused = FALSE;
        } else if (audioBuffer != NULL) {
            FreeVec(audioBuffer);
            audioBuffer = NULL;
            resetPlaybackMeta();
        }
    }
#ifdef __AMIGAOS3__
    if (narratorIOInFlight && NarratorIO != NULL &&
        ((struct IORequest *)NarratorIO)->io_Device != NULL) {
        if (CheckIO((struct IORequest *)NarratorIO) == 0)
            return TRUE;
        WaitIO((struct IORequest *)NarratorIO);
        narratorIOInFlight = FALSE;
    }
#elif defined(__AMIGAOS4__)
    if (fliteRequestPending && fliteRequest != NULL) {
        if (CheckIO((struct IORequest *)fliteRequest) == 0)
            return TRUE;
        finishFliteRequest(fliteRequest, &fliteRequestPending, FALSE);
        if (fliteTextBuffer != NULL && !fliteFileRequestPending) {
            FreeVec(fliteTextBuffer);
            fliteTextBuffer = NULL;
        }
    }
#endif
    return FALSE;
}

void speechServicePlayback(void) {
    servicePlayTimer();
    isSpeechPlaying();
}

BOOL isSpeechPaused(void) {
    return playbackIsFile && playbackPaused && audioBuffer != NULL;
}

ULONG speechPlaybackPositionMs(void) {
    return msFromBytes(currentPlaybackBytes());
}

ULONG speechPlaybackDurationMs(void) {
    return msFromBytes(playbackDataLength);
}

BOOL speechPlaybackHasAudio(void) {
    return playbackIsFile && audioBuffer != NULL && playbackDataLength > 0;
}

const UBYTE *speechPlaybackSamples(void) {
    return audioBuffer;
}

ULONG speechPlaybackSampleBytes(void) {
    return playbackDataLength;
}

ULONG speechPlaybackSampleRate(void) {
    return playbackFrequency;
}

UWORD speechPlaybackChannelCount(void) {
    return (playbackType == AHIST_S8S || playbackType == AHIST_S16S) ? 2 : 1;
}

UWORD speechPlaybackBitsPerSample(void) {
    return (playbackType == AHIST_M8S || playbackType == AHIST_S8S) ? 8 : 16;
}

void pauseSpeech(void) {
    if (!playbackIsFile || !ahiIOInFlight)
        return;
    playbackOffset = currentPlaybackBytes();
    stopPlayTimer();
    finishAHIPlayback(TRUE);
    playbackPaused = TRUE;
}

BOOL seekSpeech(ULONG positionMs) {
    BOOL wasPlaying;
    ULONG offset;

    if (!playbackIsFile || audioBuffer == NULL)
        return FALSE;
    wasPlaying = ahiIOInFlight;
    offset = bytesFromMs(positionMs);
    stopPlayTimer();
    finishAHIPlayback(TRUE);
    playbackOffset = offset;
    playbackPaused = !wasPlaying;
    if (wasPlaying)
        return startPlaybackFromOffset(offset);
    return TRUE;
}

void rewindSpeech(void) {
    BOOL wasPlaying = ahiIOInFlight;

    if (!playbackIsFile || audioBuffer == NULL) {
        stopSpeech();
        return;
    }
    stopPlayTimer();
    finishAHIPlayback(TRUE);
    playbackOffset = 0;
    playbackPaused = !wasPlaying;
    if (wasPlaying)
        startPlaybackFromOffset(0);
}

BOOL startSpeechPlayback(void) {
    if (!playbackIsFile || audioBuffer == NULL)
        return FALSE;
    if (ahiIOInFlight)
        return TRUE;
    if (playbackOffset >= playbackDataLength)
        playbackOffset = 0;
    return startPlaybackFromOffset(playbackOffset);
}

ULONG speechPlaybackSignalMask(void) {
    ULONG mask = 0;
    if (ahiIOInFlight && AHImp != NULL)
        mask |= (1UL << AHImp->mp_SigBit);
    if (playTimerPending && playTimerPort != NULL)
        mask |= (1UL << playTimerPort->mp_SigBit);
#ifdef __AMIGAOS3__
    if (narratorIOInFlight && NarratorPort != NULL)
        mask |= (1UL << NarratorPort->mp_SigBit);
#elif defined(__AMIGAOS4__)
    if (fliteRequestPending && fliteMessagePort != NULL)
        mask |= (1UL << fliteMessagePort->mp_SigBit);
#endif
    return mask;
}

void unloadSpeechPlayback(void) {
    stopPlayTimer();
    finishAHIPlayback(TRUE);
    freePlaybackBuffer();
}

BOOL loadSpeechPlayback(CONST_STRPTR filename) {
    ULONG wavLength = 0;
    UBYTE *wav;
    ULONG riff = 0;
    ULONG offset;
    ULONG sampleRate = 0;
    ULONG dataLength = 0;
    UBYTE *data = NULL;
    UWORD encoding = 0;
    UWORD channels = 1;
    UWORD bitsPerSample = 16;
    ULONG srcFrameSize;
    ULONG dstFrameSize;
    ULONG playLength;
    ULONG padBytes;
    ULONG type;
    ULONG convertedLength = 0;
    BOOL parsedWav = FALSE;

    if (filename == NULL || filename[0] == '\0')
        return FALSE;

    stopPlayTimer();
    finishAHIPlayback(TRUE);
    freePlaybackBuffer();

    wav = loadAudioFile(filename, &wavLength);
    if (wav == NULL)
        return FALSE;

    while (riff + 12 <= wavLength &&
           (memcmp(wav + riff, "RIFF", 4) != 0 ||
            memcmp(wav + riff + 8, "WAVE", 4) != 0))
        riff++;
    if (riff + 12 <= wavLength) {
        offset = riff + 12;
        while (offset + 8 <= wavLength) {
            ULONG chunkLength = readLittleEndian32(wav + offset + 4);
            ULONG chunkData = offset + 8;
            if (chunkData > wavLength)
                break;
            if (chunkLength > wavLength - chunkData)
                chunkLength = wavLength - chunkData;
            if (memcmp(wav + offset, "fmt ", 4) == 0 && chunkLength >= 16) {
                encoding = readLittleEndian16(wav + chunkData);
                channels = readLittleEndian16(wav + chunkData + 2);
                sampleRate = readLittleEndian32(wav + chunkData + 4);
                bitsPerSample = readLittleEndian16(wav + chunkData + 14);
            } else if (memcmp(wav + offset, "data", 4) == 0) {
                data = wav + chunkData;
                dataLength = chunkLength;
            }
            offset = chunkData + chunkLength + (chunkLength & 1);
        }
        if (encoding == 0xFFFE)
            encoding = 1;
        parsedWav = (encoding == 1 && (channels == 1 || channels == 2) &&
                     sampleRate != 0 && data != NULL && dataLength != 0 &&
                     (bitsPerSample == 8 || bitsPerSample == 16 ||
                      bitsPerSample == 24));
    }

    if (!parsedWav) {
        data = wav;
        dataLength = wavLength;
        sampleRate = 24000;
        channels = 1;
        bitsPerSample = 16;
    }
    srcFrameSize = (ULONG)((bitsPerSample + 7) / 8) * channels;
    if (srcFrameSize == 0) {
        FreeVec(wav);
        return FALSE;
    }
    dataLength -= dataLength % srcFrameSize;
    if (dataLength == 0) {
        FreeVec(wav);
        return FALSE;
    }
    dstFrameSize = (ULONG)(bitsPerSample == 8 ? 1 : 2) * channels;
    padBytes = sampleRate * dstFrameSize * 2;
    playLength = (bitsPerSample == 24 ? (dataLength / 3) * 2 : dataLength) +
                 padBytes;
    audioBuffer = AllocVec(playLength, MEMF_ANY | MEMF_CLEAR);
    if (audioBuffer == NULL) {
        displayError(STRING_ERROR_AUDIO_BUFFER_MEMORY);
        FreeVec(wav);
        return FALSE;
    }
    if (bitsPerSample == 8) {
        ULONG i;
        for (i = 0; i < dataLength; i++)
            audioBuffer[i] = data[i] ^ 0x80;
        convertedLength = dataLength;
        type = channels == 1 ? AHIST_M8S : AHIST_S8S;
    } else if (bitsPerSample == 24) {
        ULONG in = 0;
        ULONG out = 0;
        while (in + 2 < dataLength) {
            audioBuffer[out++] = data[in + 2];
            audioBuffer[out++] = data[in + 1];
            in += 3;
        }
        convertedLength = out;
        type = channels == 1 ? AHIST_M16S : AHIST_S16S;
    } else {
        ULONG i;
        for (i = 0; i + 1 < dataLength; i += 2) {
            audioBuffer[i] = data[i + 1];
            audioBuffer[i + 1] = data[i];
        }
        convertedLength = dataLength;
        type = channels == 1 ? AHIST_M16S : AHIST_S16S;
    }

    playbackDataLength = convertedLength;
    playbackAllocLength = playLength;
    playbackFrequency = sampleRate;
    playbackType = type;
    playbackFrameSize = dstFrameSize;
    playbackOffset = 0;
    playbackStartOffset = 0;
    playbackPaused = FALSE;
    playbackIsFile = TRUE;
    strncpy(playbackPath, filename, sizeof(playbackPath) - 1);
    playbackPath[sizeof(playbackPath) - 1] = '\0';
    FreeVec(wav);
    return TRUE;
}

BOOL playSpeechFile(CONST_STRPTR filename) {
    if (!loadSpeechPlayback(filename))
        return FALSE;
    return startPlaybackFromOffset(0);
}
