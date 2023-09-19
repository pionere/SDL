/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_FILESYSTEM_WINDOWS

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* System dependent filesystem routines                                */

#include "../../core/windows/SDL_windows.h"
#include <shlobj.h>

#include "SDL_error.h"
#include "SDL_stdinc.h"
#include "SDL_filesystem.h"

char *SDL_GetBasePath(void)
{
    char *retval;
    WCHAR path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (len == 0) {
        WIN_SetError("Couldn't locate our .exe");
        return NULL;
    }
    /* chop off filename. */
    for (len--; len > 0; len--) {
        if (path[len] == '\\') {
            break;
        }
    }
    path[len + 1] = '\0';

    retval = WIN_StringToUTF8W(path);
    return retval;
}

static SDL_bool TryAppendDir(const char *dir, SDL_iconv_t cd, WCHAR **cursor, size_t *wleft)
{
    size_t convres;
    size_t dirLen = SDL_strlen(dir);

    **cursor = L'\\';
    ++*cursor;
    if (*wleft-- == 0) {
        return SDL_FALSE;
    }
    convres = SDL_iconv(cd, &dir, &dirLen, (char **)cursor, wleft);
    switch (convres) {
    case SDL_ICONV_E2BIG:
    case SDL_ICONV_EILSEQ:
    case SDL_ICONV_EINVAL:
    case SDL_ICONV_ERROR:
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
    /*
     * Vista and later has a new API for this, but SHGetFolderPath works there,
     *  and apparently just wraps the new API. This is the new way to do it:
     *
     *     SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
     *                          NULL, &wszPath);
     */

    WCHAR path[MAX_PATH];
    size_t len, wleft;
    WCHAR *cursor;
    char *retval;
    BOOL api_result = FALSE;
    SDL_iconv_t cd;

    if (app == NULL) {
        SDL_InvalidParamError("app");
        return NULL;
    }

    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, path))) {
        WIN_SetError("Couldn't locate our prefpath");
        return NULL;
    }

    cd = SDL_iconv_open("UTF-16LE", "UTF-8"); // WIN_UTF8ToStringW
    if (cd == (SDL_iconv_t)-1) {
        SDL_OutOfMemory();
        return NULL;
    }

    len = SDL_wcslen(path);
    cursor = &path[len];
    wleft = MAX_PATH - (len + 1);
    if (org != NULL && *org && !TryAppendDir(org, cd, &cursor, &wleft)) {
        goto errlong;
    }
    if (*app && !TryAppendDir(app, cd, &cursor, &wleft)) {
        goto errlong;
    }
    if (wleft == 0) {
        goto errlong;
    }
    *cursor = L'\\';
    cursor++;
    *cursor = L'\0';

    for (cursor = &path[len + 1]; *cursor; cursor++) {
        if (*cursor == L'\\') {
            *cursor = L'\0';
            api_result = CreateDirectoryW(path, NULL);

            if (api_result == FALSE && GetLastError() != ERROR_ALREADY_EXISTS) {
                WIN_SetError("Couldn't create a prefpath.");
                return NULL;
            }
            *cursor = L'\\';
        }
    }

    SDL_iconv_close(cd);

    retval = WIN_StringToUTF8W(path);

    return retval;
errlong:
    WIN_SetError("Path too long.");
    return NULL;
}

#endif /* SDL_FILESYSTEM_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
