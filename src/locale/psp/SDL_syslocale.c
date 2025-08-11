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
#include "../SDL_syslocale.h"

#include <psputility.h>

void SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    int current_locale_int = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;
    const char *locale;

    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &current_locale_int);
    switch(current_locale_int) {
        case PSP_SYSTEMPARAM_LANGUAGE_JAPANESE:
            locale = "ja_JP";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_ENGLISH:
            locale = "en_US";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_FRENCH:
            locale = "fr_FR";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_SPANISH:
            locale = "es_ES";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_GERMAN:
            locale = "de_DE";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_ITALIAN:
            locale = "it_IT";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_DUTCH:
            locale = "nl_NL";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_PORTUGUESE:
            locale = "pt_PT";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_RUSSIAN:
            locale = "ru_RU";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_KOREAN:
            locale = "ko_KR";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_TRADITIONAL:
            locale = "zh_TW";
            break;
        case PSP_SYSTEMPARAM_LANGUAGE_CHINESE_SIMPLIFIED:
            locale = "zh_CN";
            break;
        default:
            locale = "en_US";
            break;
    }
    SDL_assert(SDL_strlen(locale) + 1 == 6);
    SDL_assert(buflen >= 6);
    SDL_memcpy(buf, locale, 6);
}

/* vi: set ts=4 sw=4 expandtab: */
