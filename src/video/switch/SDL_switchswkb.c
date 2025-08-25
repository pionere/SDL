//
// Created by cpasjuste on 22/04/2020.
//

#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_SWITCH

#include <switch.h>
#include "SDL_switchswkb.h"

static SwkbdInline kbd;
static SwkbdAppearArg kbdAppearArg;
static SDL_bool kbdInited = SDL_FALSE;
static SDL_bool kbdShown = SDL_FALSE;

void SWITCH_InitSwkb()
{
}

void SWITCH_PollSwkb(void)
{
    if (kbdInited) {
        if (kbdShown) {
            swkbdInlineUpdate(&kbd, NULL);
        } else if (SDL_IsTextInputActive()) {
            SDL_StopTextInput();
        }
    }
}

void SWITCH_QuitSwkb()
{
    if (kbdInited) {
        swkbdInlineClose(&kbd);
        kbdInited = SDL_FALSE;
    }
}

SDL_bool SWITCH_HasScreenKeyboardSupport()
{
    return SDL_TRUE;
}

SDL_bool SWITCH_IsScreenKeyboardShown(SDL_Window *window)
{
    return kbdShown;
}

static void SWITCH_EnterCb(const char *str, SwkbdDecidedEnterArg* arg)
{
    if (arg->stringLen > 0) {
        SDL_SendKeyboardText(str);
    }

    kbdShown = SDL_FALSE;
}

static void SWITCH_CancelCb(void)
{
    SDL_StopTextInput();
}

void SWITCH_StartTextInput()
{
    Result rc;

    if (!kbdInited) {
        rc = swkbdInlineCreate(&kbd);
        if (R_SUCCEEDED(rc)) {
            rc = swkbdInlineLaunchForLibraryApplet(&kbd, SwkbdInlineMode_AppletDisplay, 0);
            if (R_SUCCEEDED(rc)) {
                swkbdInlineSetDecidedEnterCallback(&kbd, SWITCH_EnterCb);
                swkbdInlineSetDecidedCancelCallback(&kbd, SWITCH_CancelCb);
                swkbdInlineMakeAppearArg(&kbdAppearArg, SwkbdType_Normal);
                swkbdInlineAppearArgSetOkButtonText(&kbdAppearArg, "Submit");
                kbdAppearArg.dicFlag = 1;
                kbdAppearArg.returnButtonFlag = 1;
                kbdInited = SDL_TRUE;
            }
        }
    }

    if (kbdInited) {
        swkbdInlineSetInputText(&kbd, "");
        swkbdInlineSetCursorPos(&kbd, 0);
        swkbdInlineUpdate(&kbd, NULL);
        swkbdInlineAppear(&kbd, &kbdAppearArg);
        kbdShown = SDL_TRUE;
    }
}

void SWITCH_StopTextInput()
{
    if (kbdInited) {
        swkbdInlineDisappear(&kbd);
    }

    kbdShown = SDL_FALSE;
}

#endif // SDL_VIDEO_DRIVER_SWITCH
