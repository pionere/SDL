/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "../../SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_AAUDIO

#include "SDL_audio.h"
#include "SDL_loadso.h"
#include "../SDL_audio_c.h"
#include "../../core/android/SDL_android.h"
#include "SDL_aaudio.h"

/* Debug */
#if 0
#define LOGI(...) SDL_Log(__VA_ARGS__);
#else
#define LOGI(...)
#endif

typedef struct AAUDIO_Data
{
    AAudioStreamBuilder *builder;
    void *handle;
#define SDL_PROC(ret, func, params) ret (*func) params;
#include "SDL_aaudiofuncs.h"
#undef SDL_PROC
} AAUDIO_Data;
static AAUDIO_Data ctx;

static int AAUDIO_SetErrorFromResult(const char *prefix, aaudio_result_t res)
{
#ifndef SDL_VERBOSE_ERROR_DISABLED
    return SDL_SetError("%s (%s)", prefix, ctx.AAudio_convertResultToText(res));
#else
    return -1;
#endif
}

static int AAUDIO_LoadFunctions(AAUDIO_Data *data)
{
#define SDL_PROC(ret, func, params)                                        \
    do {                                                                   \
        data->func = SDL_LoadFunction(data->handle, #func);                \
        if (!data->func) {                                                 \
            /* Don't call SDL_SetError(): SDL_LoadFunction already did. */ \
            return -1;                                                     \
        }                                                                  \
    } while (0);
#include "SDL_aaudiofuncs.h"
#undef SDL_PROC
    return 0;
}

static void AAUDIO_errorCallback(AAudioStream *stream, void *userData, aaudio_result_t error)
{
    LOGI("SDL AAUDIO_errorCallback: %d - %s", error, ctx.AAudio_convertResultToText(error));
}

#define LIB_AAUDIO_SO "libaaudio.so"

static int AAUDIO_OpenDevice(SDL_AudioDevice *device, const char *devname)
{
    struct SDL_PrivateAudioData *hidden;
    SDL_bool iscapture = device->iscapture;
    aaudio_result_t res;
    LOGI(__func__);

    if (iscapture) {
        if (!Android_JNI_RequestPermission("android.permission.RECORD_AUDIO")) {
            LOGI("This app doesn't have RECORD_AUDIO permission");
            return SDL_SetError("This app doesn't have RECORD_AUDIO permission");
        }
    }

    hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!hidden) {
        return SDL_OutOfMemory();
    }
    device->hidden = hidden;

    ctx.AAudioStreamBuilder_setSampleRate(ctx.builder, device->spec.freq);
    ctx.AAudioStreamBuilder_setChannelCount(ctx.builder, device->spec.channels);
    if (devname) {
        hidden->devid = SDL_atoi(devname);
        LOGI("Opening device id %d", hidden->devid);
        ctx.AAudioStreamBuilder_setDeviceId(ctx.builder, hidden->devid);
    }
    {
        const aaudio_direction_t direction = (iscapture ? AAUDIO_DIRECTION_INPUT : AAUDIO_DIRECTION_OUTPUT);
        ctx.AAudioStreamBuilder_setDirection(ctx.builder, direction);
    }
    {
        const aaudio_format_t format = (device->spec.format == AUDIO_S16SYS) ? AAUDIO_FORMAT_PCM_I16 : AAUDIO_FORMAT_PCM_FLOAT;
        ctx.AAudioStreamBuilder_setFormat(ctx.builder, format);
    }

    ctx.AAudioStreamBuilder_setErrorCallback(ctx.builder, AAUDIO_errorCallback, hidden);

    LOGI("AAudio Try to open %u hz %u bit chan %u %s samples %u",
         device->spec.freq, SDL_AUDIO_BITSIZE(device->spec.format),
         device->spec.channels, (device->spec.format & 0x1000) ? "BE" : "LE", device->spec.samples);

    res = ctx.AAudioStreamBuilder_openStream(ctx.builder, &hidden->stream);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudioStreamBuilder_openStream %d", res);
        return AAUDIO_SetErrorFromResult("AAudioStreamBuilder_openStream failed", res);
    }

    device->spec.freq = ctx.AAudioStream_getSampleRate(hidden->stream);
    device->spec.channels = ctx.AAudioStream_getChannelCount(hidden->stream);
    {
        aaudio_format_t fmt = ctx.AAudioStream_getFormat(hidden->stream);
        if (fmt == AAUDIO_FORMAT_PCM_I16) {
            device->spec.format = AUDIO_S16SYS;
        } else if (fmt == AAUDIO_FORMAT_PCM_FLOAT) {
            device->spec.format = AUDIO_F32SYS;
        }
    }

    LOGI("AAudio Try to open %u hz %u bit chan %u %s samples %u",
         device->spec.freq, SDL_AUDIO_BITSIZE(device->spec.format),
         device->spec.channels, (device->spec.format & 0x1000) ? "BE" : "LE", device->spec.samples);

    SDL_CalculateAudioSpec(&device->spec);

    /* Allocate mixing buffer */
    if (!iscapture) {
        hidden->mixlen = device->spec.size;
        hidden->mixbuf = (Uint8 *)SDL_malloc(device->spec.size);
        if (!hidden->mixbuf) {
            return SDL_OutOfMemory();
        }
        SDL_memset(hidden->mixbuf, device->spec.silence, device->spec.size);
    }

    hidden->frame_size = device->spec.channels * (SDL_AUDIO_BITSIZE(device->spec.format) / 8);

    res = ctx.AAudioStream_requestStart(hidden->stream);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudioStream_requestStart %d iscapture:%d", res, iscapture);
        return AAUDIO_SetErrorFromResult("AAudioStream_requestStart failed", res);
    }

    LOGI("SDL AAudioStream_requestStart OK");
    return 0;
}

static void AAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    LOGI(__func__);

    if (hidden->stream) {
        ctx.AAudioStream_requestStop(hidden->stream);
        ctx.AAudioStream_close(hidden->stream);
    }

    SDL_free(device->hidden->mixbuf);
    SDL_free(device->hidden);
}

static Uint8 *AAUDIO_GetDeviceBuf(SDL_AudioDevice *device)
{
    return device->hidden->mixbuf;
}

/* Try to reestablish an AAudioStream.

   This needs to get a stream with the same format as the previous one,
   even if this means AAudio needs to handle a conversion it didn't when
   we initially opened the device. If we can't get that, we are forced
   to give up here.

   (This is more robust in SDL3, which is designed to handle
   abrupt format changes.)
*/
static int RebuildAAudioStream(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    const SDL_bool iscapture = device->iscapture;
    aaudio_result_t res;

    ctx.AAudioStreamBuilder_setSampleRate(ctx.builder, device->spec.freq);
    ctx.AAudioStreamBuilder_setChannelCount(ctx.builder, device->spec.channels);
    if (hidden->devid) {
        LOGI("Reopening device id %d", hidden->devid);
        ctx.AAudioStreamBuilder_setDeviceId(ctx.builder, hidden->devid);
    }
    {
        const aaudio_direction_t direction = (iscapture ? AAUDIO_DIRECTION_INPUT : AAUDIO_DIRECTION_OUTPUT);
        ctx.AAudioStreamBuilder_setDirection(ctx.builder, direction);
    }
    {
        const aaudio_format_t format = (device->spec.format == AUDIO_S16SYS) ? AAUDIO_FORMAT_PCM_I16 : AAUDIO_FORMAT_PCM_FLOAT;
        ctx.AAudioStreamBuilder_setFormat(ctx.builder, format);
    }

    ctx.AAudioStreamBuilder_setErrorCallback(ctx.builder, AAUDIO_errorCallback, hidden);

    LOGI("AAudio Try to reopen %u hz %u bit chan %u %s samples %u",
         device->spec.freq, SDL_AUDIO_BITSIZE(device->spec.format),
         device->spec.channels, (device->spec.format & 0x1000) ? "BE" : "LE", device->spec.samples);

    res = ctx.AAudioStreamBuilder_openStream(ctx.builder, &hidden->stream);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudioStreamBuilder_openStream %d", res);
        return AAUDIO_SetErrorFromResult("AAudioStreamBuilder_openStream failed", res);
    }

    {
        const aaudio_format_t fmt = ctx.AAudioStream_getFormat(hidden->stream);
        SDL_AudioFormat sdlfmt = (SDL_AudioFormat) 0;
        if (fmt == AAUDIO_FORMAT_PCM_I16) {
            sdlfmt = AUDIO_S16SYS;
        } else if (fmt == AAUDIO_FORMAT_PCM_FLOAT) {
            sdlfmt = AUDIO_F32SYS;
        }

        /* We handle this better in SDL3, but this _needs_ to match the previous stream for SDL2. */
        if ((device->spec.freq != ctx.AAudioStream_getSampleRate(hidden->stream)) ||
            (device->spec.channels != ctx.AAudioStream_getChannelCount(hidden->stream)) ||
            (device->spec.format != sdlfmt)) {
            LOGI("Didn't get an identical spec from AAudioStream during reopen!");
            ctx.AAudioStream_close(hidden->stream);
            hidden->stream = NULL;
            return SDL_SetError("Didn't get an identical spec from AAudioStream during reopen!");
        }
    }

    res = ctx.AAudioStream_requestStart(hidden->stream);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudioStream_requestStart %d iscapture:%d", res, iscapture);
        return AAUDIO_SetErrorFromResult("AAudioStream_requestStart (restart) failed", res);
    }

    return 0;
}

static int RecoverAAudioDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    AAudioStream *stream = hidden->stream;

    /* attempt to build a new stream, in case there's a new default device. */
    hidden->stream = NULL;
    ctx.AAudioStream_requestStop(stream);
    ctx.AAudioStream_close(stream);

    return RebuildAAudioStream(device);
}


static void AAUDIO_PlayDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    aaudio_result_t res;
    int64_t timeoutNanoseconds = 1 * 1000 * 1000; /* 8 ms */
    res = ctx.AAudioStream_write(hidden->stream, hidden->mixbuf, hidden->mixlen / hidden->frame_size, timeoutNanoseconds);
    if (res < 0) {
        LOGI("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
        if (RecoverAAudioDevice(device) < 0) {
            return;  /* oh well, we went down hard. */
        }
    } else {
        LOGI("SDL AAudio play: %d frames, wanted:%d frames", (int)res, hidden->mixlen / hidden->frame_size);
    }

#if 0
    /* Log under-run count */
    {
        static int prev = 0;
        int32_t cnt = ctx.AAudioStream_getXRunCount(hidden->stream);
        if (cnt != prev) {
            SDL_Log("AAudio underrun: %d - total: %d", cnt - prev, cnt);
            prev = cnt;
        }
    }
#endif
}

static int AAUDIO_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    aaudio_result_t res;
    int64_t timeoutNanoseconds = 8 * 1000 * 1000; /* 8 ms */
    res = ctx.AAudioStream_read(hidden->stream, buffer, buflen / hidden->frame_size, timeoutNanoseconds);
    if (res < 0) {
        LOGI("%s : %s", __func__, ctx.AAudio_convertResultToText(res));
        return -1;
    }
    LOGI("SDL AAudio capture:%d frames, wanted:%d frames", (int)res, buflen / hidden->frame_size);
    return res * hidden->frame_size;
}

static void AAUDIO_Deinitialize(void)
{
    LOGI(__func__);
    if (ctx.handle) {
        if (ctx.builder) {
            aaudio_result_t res;
            res = ctx.AAudioStreamBuilder_delete(ctx.builder);
            if (res != AAUDIO_OK) {
                AAUDIO_SetErrorFromResult("AAudioStreamBuilder_delete failed", res);
            }
        }
        SDL_UnloadObject(ctx.handle);
    }
    SDL_zero(ctx);
    LOGI("End AAUDIO %s", SDL_GetError());
}

static SDL_bool AAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    aaudio_result_t res;
    LOGI(__func__);

    /* AAudio was introduced in Android 8.0, but has reference counting crash issues in that release,
     * so don't use it until 8.1.
     *
     * See https://github.com/google/oboe/issues/40 for more information.
     */
    if (SDL_GetAndroidSDKVersion() < 27) {
        return SDL_FALSE;
    }

    ctx.handle = SDL_LoadObject(LIB_AAUDIO_SO);
    if (!ctx.handle) {
        LOGI("SDL couldn't find " LIB_AAUDIO_SO);
        return SDL_FALSE;
    }

    if (AAUDIO_LoadFunctions(&ctx) < 0) {
        goto failure;
    }

    res = ctx.AAudio_createStreamBuilder(&ctx.builder);
    if (res != AAUDIO_OK) {
        LOGI("SDL Failed AAudio_createStreamBuilder %d", res);
        goto failure;
    }

    if (!ctx.builder) {
        LOGI("SDL Failed AAudio_createStreamBuilder - builder NULL");
        goto failure;
    }

    /* Set the function pointers */
    impl->DetectDevices = Android_DetectDevices;
    impl->OpenDevice = AAUDIO_OpenDevice;
    // impl->ThreadInit = xxx;
    // impl->ThreadDeinit = xxx;
    // impl->WaitDevice = xxx;
    impl->PlayDevice = AAUDIO_PlayDevice;
    impl->GetDeviceBuf = AAUDIO_GetDeviceBuf;
    impl->CaptureFromDevice = AAUDIO_CaptureFromDevice;
    impl->FlushCapture = SDL_AudioDriver_NoOp;
    impl->CloseDevice = AAUDIO_CloseDevice;
    // impl->LockDevice = xxx;
    // impl->UnlockDevice = xxx;
    // impl->FreeDeviceHandle = xxx;
    impl->Deinitialize = AAUDIO_Deinitialize;
    // impl->GetDefaultAudioInfo = xxx;
    /* Set the driver flags */
    // impl->ProvidesOwnCallbackThread = SDL_FALSE;
    impl->HasCaptureSupport = SDL_TRUE;
    impl->PreventSimultaneousOpens = SDL_TRUE;
    impl->AllowsArbitraryDeviceNames = SDL_TRUE;
    // impl->SupportsNonPow2Samples = SDL_FALSE;

    /* this audio target is available. */
    LOGI("SDL AAUDIO_Init OK");
    return SDL_TRUE;

failure:
    AAUDIO_Deinitialize();
    return SDL_FALSE;
}
/* "AAudio audio driver" */
const AudioBootStrap AAUDIO_bootstrap = {
    "AAudio", AAUDIO_Init
};

/* Pause this audio device by taking their mixer lock */
static void AAUDIO_PauseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden;
    AAudioStream *stream;

    SDL_LockMutex(device->mixer_lock);

    hidden = device->hidden;
    SDL_assert(hidden);
    stream = hidden->stream;

    if (stream) {
        if (!device->iscapture) {
            aaudio_result_t res = ctx.AAudioStream_requestPause(stream);
            if (res != AAUDIO_OK) {
                LOGI("SDL Failed AAudioStream_requestPause %d", res);
                AAUDIO_SetErrorFromResult("AAudioStream_requestPause failed", res);
            }
        } else {
            /* Pause() isn't implemented for 'capture', use Stop() */
            aaudio_result_t res = ctx.AAudioStream_requestStop(stream);
            if (res != AAUDIO_OK) {
                LOGI("SDL Failed AAudioStream_requestStop %d", res);
                AAUDIO_SetErrorFromResult("AAudioStream_requestStop (capture) failed", res);
            }
        }
    }
}
/* Pause (block) all non already paused audio devices */
void AAUDIO_PauseDevices(void)
{
    SDL_FindPhysicalAudioDeviceByCallback(AAUDIO_PauseDevice);
}

/* Resume (unblock) this audio device by releasing their mixer lock */
static void AAUDIO_ResumeDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    AAudioStream *stream;

    SDL_assert(hidden);

    stream = hidden->stream;
    if (stream) {
        aaudio_result_t res = ctx.AAudioStream_requestStart(stream);
        if (res != AAUDIO_OK) {
            LOGI("SDL Failed AAudioStream_requestStart %d", res);
            AAUDIO_SetErrorFromResult("AAudioStream_requestStart (resume) failed", res);
        }
    }

    SDL_UnlockMutex(device->mixer_lock);
}

/* Resume (unblock) all non already paused audio devices */
void AAUDIO_ResumeDevices(void)
{
    SDL_FindPhysicalAudioDeviceByCallback(AAUDIO_ResumeDevice);
}

/*
 We can sometimes get into a state where AAudioStream_write() will just block forever until we pause and unpause.
 None of the standard state queries indicate any problem in my testing. And the error callback doesn't actually get called.
 But, AAudioStream_getTimestamp() does return AAUDIO_ERROR_INVALID_STATE
*/
static void AAUDIO_DetectBrokenPlayStatePerDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *hidden = device->hidden;
    AAudioStream *stream;
    int64_t framePosition, timeNanoseconds;
    aaudio_result_t res;

    SDL_assert(hidden);

    stream = hidden->stream;
    if (!stream) {
        return;
    }

    res = ctx.AAudioStream_getTimestamp(stream, CLOCK_MONOTONIC, &framePosition, &timeNanoseconds);
    if (res == AAUDIO_ERROR_INVALID_STATE) {
        aaudio_stream_state_t currentState = ctx.AAudioStream_getState(stream);
        /* AAudioStream_getTimestamp() will also return AAUDIO_ERROR_INVALID_STATE while the stream is still initially starting. But we only care if it silently went invalid while playing. */
        if (currentState == AAUDIO_STREAM_STATE_STARTED) {
            LOGI("SDL AAUDIO_DetectBrokenPlayState: detected invalid audio device state: AAudioStream_getTimestamp result=%d, framePosition=%lld, timeNanoseconds=%lld, getState=%d", (int)res, (long long)framePosition, (long long)timeNanoseconds, (int)currentState);
            AAUDIO_PauseDevice(device);
            AAUDIO_ResumeDevice(device);
        }
    }
}

void AAUDIO_DetectBrokenPlayState(void)
{
    SDL_FindPhysicalAudioDeviceByCallback(AAUDIO_DetectBrokenPlayStatePerDevice);
}

#endif /* SDL_AUDIO_DRIVER_AAUDIO */

/* vi: set ts=4 sw=4 expandtab: */
