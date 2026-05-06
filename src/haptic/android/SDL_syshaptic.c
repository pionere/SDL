/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_HAPTIC_ANDROID

#include "SDL_timer.h"
#include "SDL_syshaptic_c.h"
#include "../SDL_syshaptic.h"
#include "SDL_haptic.h"
#include "../../core/android/SDL_android.h"


typedef struct SDL_hapticlist_item
{
    int device_id;
    char *name;
    SDL_Haptic *haptic;
    struct SDL_hapticlist_item *next;
} SDL_hapticlist_item;

static SDL_hapticlist_item *SDL_hapticlist = NULL;
static SDL_hapticlist_item *SDL_hapticlist_tail = NULL;
static int numhaptics = 0;

int SDL_SYS_HapticInit(void)
{
    Android_JNI_HapticSubscribe();
    return 0;
}

int SDL_SYS_NumHaptics(void)
{
    return numhaptics;
}

static SDL_hapticlist_item *HapticByOrder(int index)
{
    SDL_hapticlist_item *item = SDL_hapticlist;
    if ((index < 0) || (index >= numhaptics)) {
        return NULL;
    }
    while (index > 0) {
        SDL_assert(item != NULL);
        --index;
        item = item->next;
    }
    return item;
}

const char *SDL_SYS_HapticName(int index)
{
    SDL_hapticlist_item *item = HapticByOrder(index);
    return item->name;
}

static int OpenHaptic(SDL_Haptic *haptic, SDL_hapticlist_item *item)
{
    const int numEffects = 1;
    if (!item) {
        return SDL_SetError("No such device");
    }
    if (item->haptic) {
        return SDL_SetError("Haptic already opened");
    }

    haptic->hwdata = (struct haptic_hwdata *)item;
    item->haptic = haptic;

    haptic->supported = SDL_HAPTIC_LEFTRIGHT;
    haptic->neffects = numEffects;
    haptic->nplaying = numEffects;
    haptic->effects = (struct haptic_effect *)SDL_calloc(numEffects, sizeof(struct haptic_effect));
    if (!haptic->effects) {
        return SDL_OutOfMemory();
    }
    return 0;
}

int SDL_SYS_HapticOpen(SDL_Haptic *haptic)
{
    return OpenHaptic(haptic, HapticByOrder(haptic->index));
}

int SDL_SYS_HapticMouse(void)
{
    return -1;
}

int SDL_SYS_JoystickIsHaptic(SDL_Joystick *joystick)
{
    return 0;
}

int SDL_SYS_HapticOpenFromJoystick(SDL_Haptic *haptic, SDL_Joystick *joystick)
{
    return SDL_Unsupported();
}

int SDL_SYS_JoystickSameHaptic(SDL_Haptic *haptic, SDL_Joystick *joystick)
{
    return 0;
}

void SDL_SYS_HapticClose(SDL_Haptic *haptic)
{
    SDL_assert(haptic->hwdata != NULL);
    /* Free Effects. */
    SDL_free(haptic->effects);
    // haptic->effects = NULL;
    // haptic->neffects = 0;

    /* unlink */
    ((SDL_hapticlist_item *)haptic->hwdata)->haptic = NULL;
    // haptic->hwdata = NULL;
}

void SDL_SYS_HapticQuit(void)
{
    SDL_hapticlist_item *item;
    SDL_hapticlist_item *next;

    Android_JNI_HapticUnsubscribe();

    for (item = SDL_hapticlist; item; item = next) {
        next = item->next;
        SDL_free(item);
    }

    SDL_hapticlist = SDL_hapticlist_tail = NULL;
    numhaptics = 0;
}

int SDL_SYS_HapticNewEffect(SDL_Haptic *haptic,
                            struct haptic_effect *effect, SDL_HapticEffect *base)
{
    return 0;
}

int SDL_SYS_HapticUpdateEffect(SDL_Haptic *haptic,
                               struct haptic_effect *effect,
                               SDL_HapticEffect *data)
{
    return 0;
}

int SDL_SYS_HapticRunEffect(SDL_Haptic *haptic, struct haptic_effect *effect,
                            Uint32 iterations)
{
    /* Rumble (Android expects 0-65535, so multiply by 2) */
    Uint16 low_frequency_intensity = effect->effect.leftright.large_magnitude * 2;
    Uint16 high_frequency_intensity = effect->effect.leftright.small_magnitude * 2;

    Android_JNI_HapticRun(((SDL_hapticlist_item *)haptic->hwdata)->device_id, low_frequency_intensity, high_frequency_intensity, effect->effect.leftright.length);
    return 0;
}

int SDL_SYS_HapticStopEffect(SDL_Haptic *haptic, struct haptic_effect *effect)
{
    Android_JNI_HapticStop(((SDL_hapticlist_item *)haptic->hwdata)->device_id);
    return 0;
}

void SDL_SYS_HapticDestroyEffect(SDL_Haptic *haptic, struct haptic_effect *effect)
{
}

int SDL_SYS_HapticGetEffectStatus(SDL_Haptic *haptic, struct haptic_effect *effect)
{
    return 0;
}

int SDL_SYS_HapticSetGain(SDL_Haptic *haptic, int gain)
{
    return 0;
}

int SDL_SYS_HapticSetAutocenter(SDL_Haptic *haptic, int autocenter)
{
    return 0;
}

int SDL_SYS_HapticPause(SDL_Haptic *haptic)
{
    return 0;
}

int SDL_SYS_HapticUnpause(SDL_Haptic *haptic)
{
    return 0;
}

int SDL_SYS_HapticStopAll(SDL_Haptic *haptic)
{
    return 0;
}

void Android_AddHaptic(int device_id, const char *name)
{
    SDL_hapticlist_item *item;
    item = (SDL_hapticlist_item *)SDL_calloc(1, sizeof(SDL_hapticlist_item));
    if (!item) {
        return;
    }

    item->device_id = device_id;
    item->name = SDL_strdup(name);
    if (!item->name) {
        SDL_free(item);
        return;
    }

    if (!SDL_hapticlist_tail) {
        SDL_hapticlist = SDL_hapticlist_tail = item;
    } else {
        SDL_hapticlist_tail->next = item;
        SDL_hapticlist_tail = item;
    }

    ++numhaptics;
}

void Android_RemoveHaptic(int device_id)
{
    SDL_hapticlist_item *item;
    SDL_hapticlist_item *prev = NULL;

    for (item = SDL_hapticlist; item; item = item->next) {
        /* found it, remove it. */
        if (device_id == item->device_id) {
            if (prev) {
                prev->next = item->next;
            } else {
                SDL_assert(SDL_hapticlist == item);
                SDL_hapticlist = item->next;
            }
            if (item == SDL_hapticlist_tail) {
                SDL_hapticlist_tail = prev;
            }

            /* Need to decrement the haptic count */
            --numhaptics;
            /* !!! TODO: Send a haptic remove event? */

            SDL_free(item->name);
            SDL_free(item);
        }
        prev = item;
    }
}

#endif /* SDL_HAPTIC_ANDROID */

/* vi: set ts=4 sw=4 expandtab: */
