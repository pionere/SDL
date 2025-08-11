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

#ifdef SDL_AUDIO_DRIVER_NGAGE

#ifdef __cplusplus
extern "C" {
#endif

#include "SDL_ngageaudio.h"
#include "../SDL_sysaudio.h"

#ifdef __cplusplus
}
#endif

#include "SDL_ngageaudio.hpp"

CAudio::CAudio() : CActive(EPriorityStandard), iBufDes(NULL, 0) {}

CAudio *CAudio::NewL(TInt aLatency)
{
    CAudio *self = new (ELeave) CAudio();
    CleanupStack::PushL(self);
    self->ConstructL(aLatency);
    CleanupStack::Pop(self);
    return self;
}

void CAudio::ConstructL(TInt aLatency)
{
    CActiveScheduler::Add(this);
    User::LeaveIfError(iTimer.CreateLocal());
    iTimerCreated = ETrue;

    iStream = CMdaAudioOutputStream::NewL(*this);
    if (!iStream) {
        SDL_Log("Error: Failed to create audio stream");
        User::Leave(KErrNoMemory);
    }

    iLatency = aLatency;
    iLatencySamples = aLatency * 8; // 8kHz.

    // Determine minimum and maximum number of samples to write with one
    // WriteL request.
    iMinWrite = iLatencySamples / 8;
    iMaxWrite = iLatencySamples / 2;

    // Set defaults.
    iState = EStateNone;
    iTimerCreated = EFalse;
    iTimerActive = EFalse;
}

CAudio::~CAudio()
{
    if (iStream) {
        iStream->Stop();

        while (iState != EStateDone) {
            User::After(100000); // 100ms.
        }

        delete iStream;
    }
}

void CAudio::Start()
{
    if (iStream) {
        // Set to 8kHz mono audio.
        iStreamSettings.iChannels = TMdaAudioDataSettings::EChannelsMono;
        iStreamSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate8000Hz;
        iStream->Open(&iStreamSettings);
        iState = EStateOpening;
    } else {
        SDL_Log("Error: Failed to open audio stream");
    }
}

// Feeds more processed data to the audio stream.
void CAudio::Feed()
{
    // If a WriteL is already in progress, or we aren't even playing;
    // do nothing!
    if ((iState != EStateWriting) && (iState != EStatePlaying)) {
        return;
    }

    // Figure out the number of samples that really have been played
    // through the output.
    TTimeIntervalMicroSeconds pos = iStream->Position();

    TInt played = 8 * (pos.Int64() / TInt64(1000)).GetTInt(); // 8kHz.

    played += iBaseSamplesPlayed;

    // Determine the difference between the number of samples written to
    // CMdaAudioOutputStream and the number of samples it has played.
    // The difference is the amount of data in the buffers.
    if (played < 0) {
        played = 0;
    }

    TInt buffered = iSamplesWritten - played;
    if (buffered < 0) {
        buffered = 0;
    }

    if (iState == EStateWriting) {
        return;
    }

    // The trick for low latency: Do not let the buffers fill up beyond the
    // latency desired! We write as many samples as the difference between
    // the latency target (in samples) and the amount of data buffered.
    TInt samplesToWrite = iLatencySamples - buffered;

    // Do not write very small blocks. This should improve efficiency, since
    // writes to the streaming API are likely to be expensive.
    if (samplesToWrite < iMinWrite) {
        // Not enough data to write, set up a timer to fire after a while.
        // Try againwhen it expired.
        if (iTimerActive) {
            return;
        }
        iTimerActive = ETrue;
        SetActive();
        iTimer.After(iStatus, (1000 * iLatency) / 8);
        return;
    }

    // Do not write more than the set number of samples at once.
    int numSamples = samplesToWrite;
    if (numSamples > iMaxWrite) {
        numSamples = iMaxWrite;
    }

    SDL_AudioDevice *device = NGAGE_GetAudioDeviceAddr();
    if (device) {
        SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;

        iBufDes.Set(phdata->buffer, 2 * numSamples, 2 * numSamples);
        iStream->WriteL(iBufDes);
        iState = EStateWriting;

        // Keep track of the number of samples written (for latency calculations).
        iSamplesWritten += numSamples;
    } else {
        // Output device not ready yet. Let's go for another round.
        if (iTimerActive) {
            return;
        }
        iTimerActive = ETrue;
        SetActive();
        iTimer.After(iStatus, (1000 * iLatency) / 8);
    }
}

void CAudio::RunL()
{
    iTimerActive = EFalse;
    Feed();
}

void CAudio::DoCancel()
{
    iTimerActive = EFalse;
    iTimer.Cancel();
}

void CAudio::StartThread()
{
    TInt heapMinSize = 8192;        // 8 KB initial heap size.
    TInt heapMaxSize = 1024 * 1024; // 1 MB maximum heap size.

    TInt err = iProcess.Create(_L("ProcessThread"), ProcessThreadCB, KDefaultStackSize * 2, heapMinSize, heapMaxSize, this);
    if (err == KErrNone) {
        iProcess.SetPriority(EPriorityLess);
        iProcess.Resume();
    } else {
        SDL_Log("Error: Failed to create audio processing thread: %d", err);
    }
}

void CAudio::StopThread()
{
    if (iStreamStarted) {
        iProcess.Kill(KErrNone);
        iProcess.Close();
        iStreamStarted = EFalse;
    }
}

static Uint8 *NGAGEAUDIO_GetDeviceBuf(SDL_AudioDevice *device)
{
    SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;
    // SDL_INLINE_COMPILE_TIME_ASSERT(ngagebuf, NUM_BUFFERS == 2);
    // return &phdata->buffer[(phdata->next_buffer ? 1 : 0) * device->spec.size];
    return phdata->buffer;
}

static void NGAGEAUDIO_PlayDevice(SDL_AudioDevice *device)
{
    // SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;
    // phdata->next_buffer = (phdata->next_buffer + 1) % NUM_BUFFERS;
}

static void SDL_PlaybackAudioThreadIterate(SDL_AudioDevice *device)
{
    void *udata = device->callbackspec.userdata;
    SDL_AudioCallback callback = device->callbackspec.callback;
    SDL_PrivateAudioData *phdata = (SDL_PrivateAudioData *)device->hidden;
    int data_len = 0;
    Uint8 *data;

    SDL_assert(!device->iscapture);

    /* Fill the current buffer with sound */
    if (!device->stream && SDL_AtomicGet(&device->enabled)) {
        SDL_assert(device->callbackspec.size == device->spec.size);
        data = NGAGEAUDIO_GetDeviceBuf(device);
    } else {
        /* if the device isn't enabled, we still write to the
           work_buffer, so the app's callback will fire with
           a regular frequency, in case they depend on that
           for timing or progress. They can use hotplug
           now to know if the device failed.
           Streaming playback uses work_buffer, too. */
        data = NULL;
    }

    if (data == NULL) {
        data = device->work_buffer;
    }

    data_len = device->callbackspec.size;

    /* !!! FIXME: this should be LockDevice. */
    SDL_LockMutex(device->mixer_lock);
    if (SDL_AtomicGet(&device->paused)) {
        SDL_memset(data, device->callbackspec.silence, data_len);
    } else {
        callback(udata, data, data_len);
    }
    SDL_UnlockMutex(device->mixer_lock);

    if (device->stream) {
        /* Stream available audio to device, converting/resampling. */
        /* if this fails...oh well. We'll play silence here. */
        SDL_AudioStreamPut(device->stream, data, data_len);

        if (SDL_AudioStreamAvailable(device->stream) >= ((int)device->spec.size)) {
            int got;
            data = SDL_AtomicGet(&device->enabled) ? NGAGEAUDIO_GetDeviceBuf(device) : NULL;
            got = SDL_PrivateAudioStreamGet(device->stream, data ? data : device->work_buffer, device->spec.size);
            SDL_assert(got == device->spec.size); // -- except if there is an another user. Reading from queue without lock?
        }
    } else if (data == device->work_buffer) {
        return;
    }

    NGAGEAUDIO_PlayDevice(device);
}

TInt CAudio::ProcessThreadCB(TAny *aPtr)
{
    CAudio *self = static_cast<CAudio *>(aPtr);
    SDL_AudioDevice *device = NGAGE_GetAudioDeviceAddr();

    while (self->iStreamStarted) {
        if (device) {
            SDL_PlaybackAudioThreadIterate(device);
        } else {
            device = NGAGE_GetAudioDeviceAddr();
        }
        User::After(100000); // 100ms.
    }
    return KErrNone;
}

void CAudio::MaoscOpenComplete(TInt aError)
{
    if (aError == KErrNone) {
        iStream->SetVolume(1);
        iStreamStarted = ETrue;
        StartThread();

    } else {
        SDL_Log("Error: Failed to open audio stream: %d", aError);
    }
}

void CAudio::MaoscBufferCopied(TInt aError, const TDesC8 & /*aBuffer*/)
{
    if (aError == KErrNone) {
        iState = EStatePlaying;
        Feed();
    } else if (aError == KErrAbort) {
        // The stream has been stopped.
        iState = EStateDone;
    } else {
        SDL_Log("Error: Failed to copy audio buffer: %d", aError);
    }
}

void CAudio::MaoscPlayComplete(TInt aError)
{
    // If we finish due to an underflow, we'll need to restart playback.
    // Normally KErrUnderlow is raised   at stream end, but in our case the API
    // should never see the stream end -- we are continuously feeding it more
    // data!  Many underflow errors mean that the latency target is too low.
    if (aError == KErrUnderflow) {
        // The number of samples played gets resetted to zero when we restart
        // playback after underflow.
        iBaseSamplesPlayed = iSamplesWritten;

        iStream->Stop();
        Cancel();

        iStream->SetAudioPropertiesL(TMdaAudioDataSettings::ESampleRate8000Hz, TMdaAudioDataSettings::EChannelsMono);

        iState = EStatePlaying;
        Feed();
        return;

    } else if (aError != KErrNone) {
        // Handle error.
    }

    // We shouldn't get here.
    SDL_Log("%s: %d", __FUNCTION__, aError);
}

static TBool gAudioRunning;

TBool AudioIsReady()
{
    return gAudioRunning;
}

TInt AudioThreadCB(TAny *aParams)
{
    CTrapCleanup *cleanup = CTrapCleanup::New();
    if (!cleanup) {
        return KErrNoMemory;
    }

    CActiveScheduler *scheduler = new CActiveScheduler();
    if (!scheduler) {
        delete cleanup;
        return KErrNoMemory;
    }

    CActiveScheduler::Install(scheduler);

    TRAPD(err,
          {
              TInt latency = *(TInt *)aParams;
              CAudio *audio = CAudio::NewL(latency);
              CleanupStack::PushL(audio);

              gAudioRunning = ETrue;
              audio->Start();
              TBool once = EFalse;

              while (gAudioRunning) {
                  // Allow active scheduler to process any events.
                  TInt error;
                  CActiveScheduler::RunIfReady(error, CActive::EPriorityIdle);

                  if (!once) {
                      SDL_AudioDevice *device = NGAGE_GetAudioDeviceAddr();
                      if (device) {
                          // Stream ready; start feeding audio data.
                          // After feeding it once, the callbacks will take over.
                          audio->iState = CAudio::EStatePlaying;
                          audio->Feed();
                          once = ETrue;
                      }
                  }

                  User::After(100000); // 100ms.
              }

              CleanupStack::PopAndDestroy(audio);
          });

    delete scheduler;
    delete cleanup;
    return err;
}

RThread audioThread;

void InitAudio(TInt *aLatency)
{
    _LIT(KAudioThreadName, "AudioThread");

    TInt err = audioThread.Create(KAudioThreadName, AudioThreadCB, KDefaultStackSize, 0, aLatency);
    if (err != KErrNone) {
        User::Leave(err);
    }

    audioThread.Resume();
}

void DeinitAudio()
{
    gAudioRunning = EFalse;

    TRequestStatus status;
    audioThread.Logon(status);
    User::WaitForRequest(status);

    audioThread.Close();
}

#endif // SDL_AUDIO_DRIVER_NGAGE
