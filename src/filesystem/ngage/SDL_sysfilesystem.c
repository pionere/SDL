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

#ifdef SDL_FILESYSTEM_NGAGE

extern void NGAGE_GetAppPath(char* path);

char *SDL_GetBasePath(void)
{
    char app_path[512];
    NGAGE_GetAppPath(app_path);
    char *base_path = SDL_strdup(app_path);
    return base_path;
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
    const char *envr = "C:/System/Apps/";
    char *retval;
    size_t len;

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!org) {
        org = "";
    }

    len = sizeof(envr) - 1; // SDL_strlen(envr);

    len += SDL_strlen(org) + SDL_strlen(app) + 3;
    retval = (char *)SDL_malloc(len);
    if (!retval) {
        SDL_OutOfMemory();
        return NULL;
    }

    if (*org) {
        SDL_snprintf(retval, len, "%s%s/%s/", envr, org, app);
    } else {
        SDL_snprintf(retval, len, "%s%s/", envr, app);
    }

    return retval;
}

#endif /* SDL_FILESYSTEM_NGAGE */

/* vi: set ts=4 sw=4 expandtab: */
