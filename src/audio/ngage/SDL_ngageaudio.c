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

//#include "../SDL_sysaudio.h"
#include "SDL_ngageaudio.h"

static SDL_AudioDevice *devptr = NULL;

SDL_AudioDevice *NGAGE_GetAudioDeviceAddr()
{
    return devptr;
}

static int NGAGEAUDIO_OpenDevice(_THIS, const char *devname)
{
    this->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, sizeof(*this->hidden));
    if (!this->hidden) {
        return SDL_OutOfMemory();
    }

    // Since the phone can change the sample rate during a phone call,
    // we set the sample rate to 8KHz to be safe.  Even though it
    // might be possible to adjust the sample rate dynamically, it's
    // not supported by the current implementation.

    this->spec.format = AUDIO_S16LSB;
    this->spec.channels = 1;
    this->spec.freq = 8000;

    SDL_CalculateAudioSpec(&this->spec);

    this->hidden->buffer = SDL_malloc(NUM_BUFFERS * this->spec.size);
    if (!this->hidden->buffer) {
        return SDL_OutOfMemory();
    }
    devptr = this;

    return 0;
}

/*static Uint8 *NGAGEAUDIO_GetDeviceBuf(_THIS)
{
    SDL_INLINE_COMPILE_TIME_ASSERT(ngagebuf, NUM_BUFFERS == 2);
    return &this->hidden->buffer[(this->hidden->next_buffer ? 1 : 0) * this->spec.size];
}

static void NGAGEAUDIO_PlayDevice(_THIS)
{
    this->hidden->next_buffer = (this->hidden->next_buffer + 1) % NUM_BUFFERS;
}*/

static void NGAGEAUDIO_CloseDevice(_THIS)
{
    SDL_free(this->hidden->buffer);
    SDL_free(this->hidden);
}

static SDL_bool NGAGEAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    /* Set the function pointers */
    // impl->DetectDevices = xxx;
    impl->OpenDevice = NGAGEAUDIO_OpenDevice;
    // impl->ThreadInit = xxx;
    // impl->ThreadDeinit = xxx;
    // impl->WaitDevice = xxx;
    //impl->PlayDevice = NGAGEAUDIO_PlayDevice;
    //impl->GetDeviceBuf = NGAGEAUDIO_GetDeviceBuf;
    // impl->CaptureFromDevice = xxx;
    // impl->FlushCapture = xxx;
    impl->CloseDevice = NGAGEAUDIO_CloseDevice;
    // impl->LockDevice = xxx;
    // impl->UnlockDevice = xxx;
    // impl->FreeDeviceHandle = xxx;
    //impl->Deinitialize = xxx;
    //impl->GetDefaultAudioInfo = xxx;
    /* Set the driver flags */
    impl->ProvidesOwnCallbackThread = SDL_TRUE;
    // impl->HasCaptureSupport = SDL_FALSE;
    // impl->PreventSimultaneousOpens = SDL_FALSE;
    // impl->AllowsArbitraryDeviceNames = SDL_FALSE;
    // impl->SupportsNonPow2Samples = SDL_FALSE;

    return SDL_TRUE; /* this audio target is available. */
}

const AudioBootStrap NGAGEAUDIO_bootstrap = { "N-Gage", NGAGEAUDIO_Init };

#endif // SDL_AUDIO_DRIVER_NGAGE
