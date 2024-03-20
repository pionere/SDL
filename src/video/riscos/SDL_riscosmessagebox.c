/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_RISCOS

#include "SDL_messagebox.h"
#include "SDL_riscosmessagebox.h"

#include <kernel.h>
#include <swis.h>

int RISCOS_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonid)
{
    _kernel_swi_regs regs;
    _kernel_oserror error;
    char buttonstring[1024];
    int i, numbtns;

    error.errnum = 0;
    SDL_strlcpy(error.errmess, messageboxdata->message, 252);
    regs.r[0] = (unsigned int)&error;

    if (messageboxdata->flags & SDL_MESSAGEBOX_ERROR) {
        i = 0;
    } else if (messageboxdata->flags & SDL_MESSAGEBOX_INFORMATION) {
        i = 1;
    } else if (messageboxdata->flags & SDL_MESSAGEBOX_WARNING) {
        i = 2;
    } else {
        i = 7;
    }
    regs.r[1] = (1 << 8) | (1 << 4) | (i << 9);

    regs.r[2] = (unsigned int)messageboxdata->title;
    regs.r[3] = 0;
    regs.r[4] = 0;

    buttonstring[0] = '\0';
    numbtns = messageboxdata->numbuttons;
    for (i = 0; i < numbtns; i++) {
        int btnIdx;
        if (!(messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT)) {
            btnIdx = i;
        } else {
            btnIdx = numbtns - 1 - i;
        }
        SDL_strlcat(buttonstring, messageboxdata->buttons[btnIdx].text, 1024);
        if (i + 1 < numbtns) {
            SDL_strlcat(buttonstring, ",", 1024);
        }
    }
    regs.r[5] = (unsigned int)buttonstring;

    _kernel_swi(Wimp_ReportError, &regs, &regs);

    i = -1;
    if (regs.r[1] != 0) {
        int btnIdx;
        i = regs.r[1] - 3;
        if (!(messageboxdata->flags & SDL_MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT)) {
            btnIdx = i;
        } else {
            btnIdx = numbtns - 1 - i;
        }
        i = messageboxdata->buttons[btnIdx].buttonid;
    }
    *buttonid = i;
    return 0;
}

#endif /* SDL_VIDEO_DRIVER_RISCOS */

/* vi: set ts=4 sw=4 expandtab: */
