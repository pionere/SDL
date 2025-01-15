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

#if defined(SDL_VIDEO_DRIVER_WINDOWS) && !defined(__XBOXONE__) && !defined(__XBOXSERIES__)

// disable IME if locale is disabled (not necessary, but reasonable)
#if !defined(SDL_DISABLE_WINDOWS_IME) && defined(SDL_LOCALE_DISABLED)
#define SDL_DISABLE_WINDOWS_IME 1
#endif

#include "SDL_windowsvideo.h"
#include "SDL_hints.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../events/scancodes_windows.h"

#ifndef SDL_DISABLE_WINDOWS_IME
#if defined(_MSC_VER) && (_MSC_VER >= 1500)
#include <msctf.h>
#else
#include "SDL_msctf.h"
#endif

#include <imm.h>

#define MAX_CANDLIST   10
#define MAX_CANDLENGTH 256
#define MAX_CANDSIZE   (sizeof(WCHAR) * MAX_CANDLIST * MAX_CANDLENGTH)

typedef struct
{
    void **lpVtbl;
    int refcount;
} TSFSink;

/* Definition from Win98DDK version of IMM.H */
typedef struct tagINPUTCONTEXT2
{
    HWND hWnd;
    BOOL fOpen;
    POINT ptStatusWndPos;
    POINT ptSoftKbdPos;
    DWORD fdwConversion;
    DWORD fdwSentence;
    union
    {
        LOGFONTA A;
        LOGFONTW W;
    } lfFont;
    COMPOSITIONFORM cfCompForm;
    CANDIDATEFORM cfCandForm[4];
    HIMCC hCompStr;
    HIMCC hCandInfo;
    HIMCC hGuideLine;
    HIMCC hPrivate;
    DWORD dwNumMsgBuf;
    HIMCC hMsgBuf;
    DWORD fdwInit;
    DWORD dwReserve[3];
} INPUTCONTEXT2, *PINPUTCONTEXT2, NEAR *NPINPUTCONTEXT2, FAR *LPINPUTCONTEXT2;

typedef struct WIN_ImeData
{
    SDL_bool ime_com_initialized;
    struct ITfThreadMgr *ime_threadmgr;
    SDL_bool ime_initialized;
    SDL_bool ime_enabled;
    SDL_bool ime_available;
    HWND ime_hwnd_main;
    HWND ime_hwnd_current;
    SDL_bool ime_suppress_endcomposition_event;
    HIMC ime_himc;

    WCHAR *ime_composition;
    int ime_composition_length;
    WCHAR ime_readingstring[16];
    int ime_cursor;
#ifdef IME_DRAW_UI
    SDL_bool ime_candlist;
    WCHAR *ime_candidates;
    DWORD ime_candcount;
    DWORD ime_candref;
    DWORD ime_candsel;
    UINT ime_candpgsize;
    int ime_candlistindexbase;
    SDL_bool ime_candhorizontal;

    SDL_bool ime_dirty;
    SDL_Rect ime_rect;
    SDL_Rect ime_candlistrect;
    int ime_winwidth;
    int ime_winheight;
#endif
    HKL ime_hkl;
    DWORD ime_ids[2];
    void *ime_hime;
    UINT (WINAPI *GetReadingString)(HIMC himc, UINT uReadingBufLen, LPWSTR lpwReadingBuf, PINT pnErrorIndex, BOOL *pfIsVertical, PUINT puMaxReadingLen);
    void *ime_himm32;
    LPINPUTCONTEXT2 (WINAPI *ImmLockIMC)(HIMC himc);
    BOOL (WINAPI *ImmUnlockIMC)(HIMC himc);
    LPVOID (WINAPI *ImmLockIMCC)(HIMCC himcc);
    BOOL (WINAPI *ImmUnlockIMCC)(HIMCC himcc);
#ifdef IME_DRAW_UI
    SDL_bool ime_sdl_ui;
#endif
#ifdef IME_USE_TF_API
    struct ITfThreadMgrEx *ime_threadmgrex;
    DWORD ime_uielemsinkcookie;
    DWORD ime_alpnsinkcookie;
    DWORD ime_openmodesinkcookie;
    DWORD ime_convmodesinkcookie;
    TSFSink *ime_uielemsink;
    TSFSink *ime_ippasink;
#endif
    SDL_bool ime_uicontext;
} WIN_ImeData;

void WIN_UpdateKeymap(SDL_bool send_event);
static void IME_Init(HWND hwnd);
static void IME_Enable(void);
static void IME_Disable(void);
static void IME_Quit(void);
static SDL_bool IME_IsTextInputShown(void);

static WIN_ImeData winImeData;
#endif /* !SDL_DISABLE_WINDOWS_IME */

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif
#ifndef MAPVK_VSC_TO_VK
#define MAPVK_VSC_TO_VK 1
#endif
#ifndef MAPVK_VK_TO_CHAR
#define MAPVK_VK_TO_CHAR 2
#endif

/* Alphabetic scancodes for PC keyboards */
void WIN_InitKeyboard(void)
{
#ifndef SDL_DISABLE_WINDOWS_IME
    WIN_ImeData *data = &winImeData;

    SDL_zerop(data);
#if 0
    data->ime_com_initialized = SDL_FALSE;
    data->ime_threadmgr = NULL;
    data->ime_initialized = SDL_FALSE;
    data->ime_enabled = SDL_FALSE;
    data->ime_available = SDL_FALSE;
    data->ime_hwnd_main = 0;
    data->ime_hwnd_current = 0;
    data->ime_suppress_endcomposition_event = SDL_FALE;
    data->ime_himc = 0;
#endif
    data->ime_composition_length = 32 * sizeof(WCHAR);
    data->ime_composition = (WCHAR *)SDL_calloc(1, data->ime_composition_length + sizeof(WCHAR));
    if (data->ime_composition == NULL) {
        data->ime_composition_length = 0;
    }
#if 0
    data->ime_composition[0] = 0;
    data->ime_readingstring[0] = 0;
    data->ime_cursor = 0;

    data->ime_candidates = NULL;
    data->ime_candcount = 0;
    data->ime_candref = 0;
    data->ime_candsel = 0;
    data->ime_candpgsize = 0;
    data->ime_candlistindexbase = 0;
    data->ime_candhorizontal = SDL_FALSE;

    data->ime_dirty = SDL_FALSE;
    SDL_memset(&data->ime_rect, 0, sizeof(data->ime_rect));
    SDL_memset(&data->ime_candlistrect, 0, sizeof(data->ime_candlistrect));
    data->ime_winwidth = 0;
    data->ime_winheight = 0;

    data->ime_hkl = 0;
    SDL_memset(data->ime_ids, 0, sizeof(data->ime_ids));
    data->ime_hime = NULL;
    data->GetReadingString = NULL;
    data->ime_himm32 = NULL;
    data->ImmLockIMC = NULL;
    data->ImmUnlockIMC = NULL;
    data->ImmLockIMCC = NULL;
    data->ImmUnlockIMCC = NULL;
    data->ime_sdl_ui = SDL_FALSE;
    data->ime_threadmgrex = NULL;
#endif
#ifdef IME_USE_TF_API
    data->ime_uielemsinkcookie = TF_INVALID_COOKIE;
    data->ime_alpnsinkcookie = TF_INVALID_COOKIE;
    data->ime_openmodesinkcookie = TF_INVALID_COOKIE;
    data->ime_convmodesinkcookie = TF_INVALID_COOKIE;
#endif
#if 0
    data->ime_uielemsink = NULL;
    data->ime_ippasink = NULL;
    data->ime_uicontext = SDL_FALSE;
#endif
#endif /* !SDL_DISABLE_WINDOWS_IME */

    WIN_UpdateKeymap(SDL_FALSE);

    SDL_SetScancodeName(SDL_SCANCODE_APPLICATION, "Menu");
    SDL_SetScancodeName(SDL_SCANCODE_LGUI, "Left Windows");
    SDL_SetScancodeName(SDL_SCANCODE_RGUI, "Right Windows");

    /* Are system caps/num/scroll lock active? Set our state to match. */
    SDL_ToggleModState(KMOD_CAPS, (GetKeyState(VK_CAPITAL) & 0x0001) ? SDL_TRUE : SDL_FALSE);
    SDL_ToggleModState(KMOD_NUM, (GetKeyState(VK_NUMLOCK) & 0x0001) ? SDL_TRUE : SDL_FALSE);
    SDL_ToggleModState(KMOD_SCROLL, (GetKeyState(VK_SCROLL) & 0x0001) ? SDL_TRUE : SDL_FALSE);
}

void WIN_UpdateKeymap(SDL_bool send_event)
{
    int i;
    SDL_Scancode scancode;
    SDL_Keycode keymap[SDL_NUM_SCANCODES];

    SDL_GetDefaultKeymap(keymap);

    for (i = 0; i < SDL_arraysize(windows_scancode_table); i++) {
        int vk;
        /* Make sure this scancode is a valid character scancode */
        scancode = windows_scancode_table[i];
        if (scancode == SDL_SCANCODE_UNKNOWN) {
            continue;
        }

        /* If this key is one of the non-mappable keys, ignore it */
        /* Uncomment the second part re-enable the behavior of not mapping the "`"(grave) key to the users actual keyboard layout */
        if ((keymap[scancode] & SDLK_SCANCODE_MASK) /*|| scancode == SDL_SCANCODE_GRAVE*/) {
            continue;
        }

        vk = MapVirtualKey(i, MAPVK_VSC_TO_VK);
        if (vk) {
            int ch = (MapVirtualKey(vk, MAPVK_VK_TO_CHAR) & 0x7FFF);
            if (ch) {
                if (ch >= 'A' && ch <= 'Z') {
                    keymap[scancode] = SDLK_a + (ch - 'A');
                } else {
                    keymap[scancode] = ch;
                }
            }
        }
    }

    SDL_SetKeymap(0, keymap, SDL_NUM_SCANCODES, send_event);
}

void WIN_QuitKeyboard(void)
{
#ifndef SDL_DISABLE_WINDOWS_IME
    WIN_ImeData *data = &winImeData;

    IME_Quit();

    SDL_free(data->ime_composition);
    data->ime_composition = NULL;
#endif
}

void WIN_ResetDeadKeys(void)
{
#if 0 // commented out till someone proves this does anything useful...
    /*
    if a deadkey has been typed, but not the next character (which the deadkey might modify),
    this tries to undo the effect pressing the deadkey.
    see: http://archives.miloush.net/michkap/archive/2006/09/10/748775.html
    */
    BYTE keyboardState[256];
    WCHAR buffer[16];
    int keycode, scancode, result, i;

    GetKeyboardState(keyboardState);

    keycode = VK_SPACE;
    scancode = MapVirtualKey(keycode, MAPVK_VK_TO_VSC);
    if (scancode == 0) {
        /* the keyboard doesn't have this key */
        return;
    }

    for (i = 0; i < 5; i++) {
        result = ToUnicode(keycode, scancode, keyboardState, (LPWSTR)buffer, 16, 0);
        if (result > 0) {
            /* success */
            return;
        }
    }
#endif
}

void WIN_StartTextInput(void)
{
#ifndef SDL_DISABLE_WINDOWS_IME
    SDL_Window *window;
#endif

    WIN_ResetDeadKeys();

#ifndef SDL_DISABLE_WINDOWS_IME
    window = SDL_GetKeyboardFocus();
    if (window) {
        HWND hwnd = ((SDL_WindowData *)window->driverdata)->hwnd;
#ifdef IME_DRAW_UI
        WIN_ImeData *videodata = &winImeData;
        SDL_GetWindowSize(window, &videodata->ime_winwidth, &videodata->ime_winheight);
#endif
        IME_Init(hwnd);
        IME_Enable();
    }
#endif /* !SDL_DISABLE_WINDOWS_IME */
}

void WIN_StopTextInput(void)
{
#ifndef SDL_DISABLE_WINDOWS_IME
    SDL_Window *window;
#endif

    WIN_ResetDeadKeys();

#ifndef SDL_DISABLE_WINDOWS_IME
    window = SDL_GetKeyboardFocus();
    if (window) {
        HWND hwnd = ((SDL_WindowData *)window->driverdata)->hwnd;
        IME_Init(hwnd);
        IME_Disable();
    }
#endif /* !SDL_DISABLE_WINDOWS_IME */
}

void WIN_SetTextInputRect(const SDL_Rect *rect)
{
#ifndef SDL_DISABLE_WINDOWS_IME
    WIN_ImeData *videodata = &winImeData;
    HIMC himc;

#ifdef IME_DRAW_UI
    videodata->ime_rect = *rect;
#endif
    himc = ImmGetContext(videodata->ime_hwnd_current);
    if (himc) {
        COMPOSITIONFORM cof;
        CANDIDATEFORM caf;

        cof.dwStyle = CFS_RECT;
        cof.ptCurrentPos.x = rect->x;
        cof.ptCurrentPos.y = rect->y;
        cof.rcArea.left = rect->x;
        cof.rcArea.right = (LONG)rect->x + rect->w;
        cof.rcArea.top = rect->y;
        cof.rcArea.bottom = (LONG)rect->y + rect->h;
        ImmSetCompositionWindow(himc, &cof);

        caf.dwIndex = 0;
        caf.dwStyle = CFS_EXCLUDE;
        caf.ptCurrentPos.x = rect->x;
        caf.ptCurrentPos.y = rect->y;
        caf.rcArea.left = rect->x;
        caf.rcArea.right = (LONG)rect->x + rect->w;
        caf.rcArea.top = rect->y;
        caf.rcArea.bottom = (LONG)rect->y + rect->h;
        ImmSetCandidateWindow(himc, &caf);

        ImmReleaseContext(videodata->ime_hwnd_current, himc);
    }
#endif /* !SDL_DISABLE_WINDOWS_IME */
}

#ifdef SDL_DISABLE_WINDOWS_IME

void WIN_ClearComposition(void)
{
}

SDL_bool WIN_IsTextInputShown(void)
{
    return SDL_FALSE;
}

SDL_bool IME_HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM *lParam)
{
    return SDL_FALSE;
}

void IME_Present(void)
{
}

#else

#ifdef SDL_msctf_h_
#define USE_INIT_GUID
#elif defined(__GNUC__)
#define USE_INIT_GUID
#endif
#ifdef USE_INIT_GUID
#undef DEFINE_GUID
#define DEFINE_GUID(n, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) static const GUID n = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
DEFINE_GUID(CLSID_TF_ThreadMgr, 0x529A9E6B, 0x6587, 0x4F23, 0xAB, 0x9E, 0x9C, 0x7D, 0x68, 0x3E, 0x3C, 0x50);
DEFINE_GUID(IID_ITfThreadMgr, 0xAA80E801, 0x2021, 0x11D2, 0x93, 0xE0, 0x00, 0x60, 0xB0, 0x67, 0xB8, 0x6E);
#ifdef IME_USE_TF_API
DEFINE_GUID(IID_ITfThreadMgrEx, 0x3E90ADE3, 0x7594, 0x4CB0, 0xBB, 0x58, 0x69, 0x62, 0x8F, 0x5F, 0x45, 0x8C);
DEFINE_GUID(IID_ITfInputProcessorProfileActivationSink, 0x71C6E74E, 0x0F28, 0x11D8, 0xA8, 0x2A, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C);
DEFINE_GUID(IID_ITfUIElementSink, 0xEA1EA136, 0x19DF, 0x11D7, 0xA6, 0xD2, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C);
DEFINE_GUID(GUID_TFCAT_TIP_KEYBOARD, 0x34745C63, 0xB2F0, 0x4784, 0x8B, 0x67, 0x5E, 0x12, 0xC8, 0x70, 0x1A, 0x31);
DEFINE_GUID(IID_ITfSource, 0x4EA48A35, 0x60AE, 0x446F, 0x8F, 0xD6, 0xE6, 0xA8, 0xD8, 0x24, 0x59, 0xF7);
DEFINE_GUID(IID_ITfUIElementMgr, 0xEA1EA135, 0x19DF, 0x11D7, 0xA6, 0xD2, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C);
DEFINE_GUID(IID_ITfCandidateListUIElement, 0xEA1EA138, 0x19DF, 0x11D7, 0xA6, 0xD2, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C);
DEFINE_GUID(IID_ITfReadingInformationUIElement, 0xEA1EA139, 0x19DF, 0x11D7, 0xA6, 0xD2, 0x00, 0x06, 0x5B, 0x84, 0x43, 0x5C);
#endif // IME_USE_TF_API
#endif // USE_INIT_GUID

#define LANG_CHT MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL)
#define LANG_CHS MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)

#define MAKEIMEVERSION(major, minor) ((DWORD)(((BYTE)(major) << 24) | ((BYTE)(minor) << 16)))
#define IMEID_VER(id)                ((id)&0xffff0000)
#define IMEID_LANG(id)               ((id)&0x0000ffff)

#define CHT_HKL_DAYI          ((HKL)(UINT_PTR)0xE0060404)
#define CHT_HKL_NEW_PHONETIC  ((HKL)(UINT_PTR)0xE0080404)
#define CHT_HKL_NEW_CHANG_JIE ((HKL)(UINT_PTR)0xE0090404)
#define CHT_HKL_NEW_QUICK     ((HKL)(UINT_PTR)0xE00A0404)
#define CHT_HKL_HK_CANTONESE  ((HKL)(UINT_PTR)0xE00B0404)
#define CHT_IMEFILENAME1      "TINTLGNT.IME"
#define CHT_IMEFILENAME2      "CINTLGNT.IME"
#define CHT_IMEFILENAME3      "MSTCIPHA.IME"
#define IMEID_CHT_VER42       (LANG_CHT | MAKEIMEVERSION(4, 2))
#define IMEID_CHT_VER43       (LANG_CHT | MAKEIMEVERSION(4, 3))
#define IMEID_CHT_VER44       (LANG_CHT | MAKEIMEVERSION(4, 4))
#define IMEID_CHT_VER50       (LANG_CHT | MAKEIMEVERSION(5, 0))
#define IMEID_CHT_VER51       (LANG_CHT | MAKEIMEVERSION(5, 1))
#define IMEID_CHT_VER52       (LANG_CHT | MAKEIMEVERSION(5, 2))
#define IMEID_CHT_VER60       (LANG_CHT | MAKEIMEVERSION(6, 0))
#define IMEID_CHT_VER_VISTA   (LANG_CHT | MAKEIMEVERSION(7, 0))

#define CHS_HKL          ((HKL)(UINT_PTR)0xE00E0804)
#define CHS_IMEFILENAME1 "PINTLGNT.IME"
#define CHS_IMEFILENAME2 "MSSCIPYA.IME"
#define IMEID_CHS_VER41  (LANG_CHS | MAKEIMEVERSION(4, 1))
#define IMEID_CHS_VER42  (LANG_CHS | MAKEIMEVERSION(4, 2))
#define IMEID_CHS_VER53  (LANG_CHS | MAKEIMEVERSION(5, 3))

#define LANG()         LOWORD((videodata->ime_hkl))
#define PRIMLANG()     ((WORD)PRIMARYLANGID(LANG()))
#define SUBLANG()      SUBLANGID(LANG())

static SDL_bool IME_UpdateInputLocale(void);
static void IME_ClearComposition(void);
static void IME_SetWindow(HWND hwnd);
static void IME_SendEditingEvent(void);
#ifdef IME_DRAW_UI
static int IME_InitCandidateList(void);
static void IME_DestroyTextures(void);
#endif
#ifdef IME_USE_TF_API
static SDL_bool UILess_SetupSinks(void);
static void UILess_ReleaseSinks(void);
static void UILess_EnableUIUpdates(void);
static void UILess_DisableUIUpdates(void);
#endif
static void IME_Init(HWND hwnd)
{
    HRESULT hResult = S_OK;
    WIN_ImeData *videodata = &winImeData;

    if (videodata->ime_initialized) {
        return;
    }

    videodata->ime_initialized = SDL_TRUE;
    videodata->ime_hwnd_main = hwnd;
    if (SUCCEEDED(WIN_CoInitialize())) {
        videodata->ime_com_initialized = SDL_TRUE;
        hResult = CoCreateInstance(&CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, &IID_ITfThreadMgr, (LPVOID *)&videodata->ime_threadmgr);
        if (hResult != S_OK) {
            // videodata->ime_available = SDL_FALSE;
            WIN_SetErrorFromHRESULT("CoCreateInstance() failed", hResult);
            return;
        }
    }
    videodata->ime_himm32 = SDL_LoadObject("imm32.dll");
    if (!videodata->ime_himm32) {
        // videodata->ime_available = SDL_FALSE;
        SDL_ClearError();
        return;
    }
    videodata->ImmLockIMC = (LPINPUTCONTEXT2 (WINAPI *)(HIMC))SDL_LoadFunction(videodata->ime_himm32, "ImmLockIMC");
    videodata->ImmUnlockIMC = (BOOL (WINAPI *)(HIMC))SDL_LoadFunction(videodata->ime_himm32, "ImmUnlockIMC");
    videodata->ImmLockIMCC = (LPVOID (WINAPI *)(HIMCC))SDL_LoadFunction(videodata->ime_himm32, "ImmLockIMCC");
    videodata->ImmUnlockIMCC = (BOOL (WINAPI *)(HIMCC))SDL_LoadFunction(videodata->ime_himm32, "ImmUnlockIMCC");

    IME_SetWindow(hwnd);
    videodata->ime_himc = ImmGetContext(hwnd);
    if (!videodata->ime_himc) {
        // videodata->ime_available = SDL_FALSE;
        // IME_Disable();
        return;
    }
    videodata->ime_available = SDL_TRUE;
    ImmReleaseContext(hwnd, videodata->ime_himc);
#ifdef IME_DRAW_UI
    videodata->ime_sdl_ui = SDL_GetHintBoolean(SDL_HINT_IME_SHOW_UI, SDL_FALSE);
#endif
#ifdef IME_USE_TF_API
    UILess_SetupSinks();
#endif
    IME_UpdateInputLocale();
    IME_Disable();
}

static void IME_Enable(void)
{
    WIN_ImeData *videodata = &winImeData;
    HWND hwnd = videodata->ime_hwnd_current;
    if (!hwnd) {
        return;
    }
    // SDL_assert(videodata->ime_initialized);

    if (!videodata->ime_available) {
        // IME_Disable();
        return;
    }
    if (hwnd == videodata->ime_hwnd_main) {
        ImmAssociateContext(hwnd, videodata->ime_himc);
    }

    videodata->ime_enabled = SDL_TRUE;
    IME_UpdateInputLocale();
#ifdef IME_USE_TF_API
    UILess_EnableUIUpdates();
#endif
}

static void IME_Disable(void)
{
    WIN_ImeData *videodata = &winImeData;
    HWND hwnd = videodata->ime_hwnd_current;
    if (!hwnd) {
        return;
    }
    // SDL_assert(videodata->ime_initialized);

    IME_ClearComposition();
    if (hwnd == videodata->ime_hwnd_main) {
        ImmAssociateContext(hwnd, (HIMC)0);
    }

    videodata->ime_enabled = SDL_FALSE;
#ifdef IME_USE_TF_API
    UILess_DisableUIUpdates();
#endif
}

static void IME_Quit(void)
{
    WIN_ImeData *videodata = &winImeData;

    if (!videodata->ime_initialized) {
        return;
    }
#ifdef IME_USE_TF_API
    UILess_ReleaseSinks();
#endif
    if (videodata->ime_hwnd_main) {
        ImmAssociateContext(videodata->ime_hwnd_main, videodata->ime_himc);
        videodata->ime_hwnd_main = 0;
    }

    if (videodata->ime_himm32) {
        SDL_UnloadObject(videodata->ime_himm32);
        videodata->ime_himm32 = NULL;
    }
    if (videodata->ime_hime) {
        SDL_UnloadObject(videodata->ime_hime);
        videodata->ime_hime = NULL;
    }
    if (videodata->ime_threadmgr) {
        videodata->ime_threadmgr->lpVtbl->Release(videodata->ime_threadmgr);
        videodata->ime_threadmgr = NULL;
    }
    if (videodata->ime_com_initialized) {
        WIN_CoUninitialize();
        videodata->ime_com_initialized = SDL_FALSE;
    }
#ifdef IME_DRAW_UI
    IME_DestroyTextures();
#endif
    videodata->ime_initialized = SDL_FALSE;
}

static void IME_GetReadingString(HWND hwnd)
{
    WIN_ImeData *videodata = &winImeData;
    HIMC himc;

    videodata->ime_readingstring[0] = 0;

    himc = ImmGetContext(hwnd);
    if (!himc) {
        return;
    }

    if (videodata->GetReadingString) {
        INT err = 0;
        BOOL vertical = FALSE;
        UINT maxuilen = 0;
        videodata->GetReadingString(himc, SDL_arraysize(videodata->ime_readingstring), videodata->ime_readingstring, &err, &vertical, &maxuilen);
    } else {
        LPINPUTCONTEXT2 lpimc = videodata->ImmLockIMC(himc);
        LPBYTE p;
        WCHAR *s = NULL;
        DWORD len = 0;
        switch (videodata->ime_ids[0]) {
        case IMEID_CHT_VER42:
        case IMEID_CHT_VER43:
        case IMEID_CHT_VER44:
            p = *(LPBYTE *)((LPBYTE)videodata->ImmLockIMCC(lpimc->hPrivate) + 24);
            if (!p) {
                break;
            }

            len = *(DWORD *)(p + 7 * 4 + 32 * 4);
            s = (WCHAR *)(p + 56);
            break;
        case IMEID_CHT_VER51:
        case IMEID_CHT_VER52:
        case IMEID_CHS_VER53:
            p = *(LPBYTE *)((LPBYTE)videodata->ImmLockIMCC(lpimc->hPrivate) + 4);
            if (!p) {
                break;
            }

            p = *(LPBYTE *)(p + 1 * 4 + 5 * 4);
            if (!p) {
                break;
            }

            len = *(DWORD *)(p + 1 * 4 + (16 * 2 + 2 * 4) + 5 * 4 + 16 * 2);
            s = (WCHAR *)(p + 1 * 4 + (16 * 2 + 2 * 4) + 5 * 4);
            break;
        case IMEID_CHS_VER41:
        {
            int offset = (videodata->ime_ids[1] >= 0x00000002) ? 8 : 7;
            p = *(LPBYTE *)((LPBYTE)videodata->ImmLockIMCC(lpimc->hPrivate) + offset * 4);
            if (!p) {
                break;
            }

            len = *(DWORD *)(p + 7 * 4 + 16 * 2 * 4);
            s = (WCHAR *)(p + 6 * 4 + 16 * 2 * 1);
        } break;
        case IMEID_CHS_VER42:
            p = *(LPBYTE *)((LPBYTE)videodata->ImmLockIMCC(lpimc->hPrivate) + 1 * 4 + 1 * 4 + 6 * 4);
            if (!p) {
                break;
            }

            len = *(DWORD *)(p + 1 * 4 + (16 * 2 + 2 * 4) + 5 * 4 + 16 * 2);
            s = (WCHAR *)(p + 1 * 4 + (16 * 2 + 2 * 4) + 5 * 4);
            break;
        }
        if (s) {
            size_t size = SDL_min((size_t)(len + 1), SDL_arraysize(videodata->ime_readingstring));
            SDL_memcpy(videodata->ime_readingstring, s, size * sizeof(WCHAR));
        }

        videodata->ImmUnlockIMCC(lpimc->hPrivate);
        videodata->ImmUnlockIMC(himc);
    }
    ImmReleaseContext(hwnd, himc);
    IME_SendEditingEvent();
}

static void IME_InputLangChanged(void)
{
    if (IME_UpdateInputLocale()) {
        IME_ClearComposition();
    }
}

static void IME_SetupAPI(void)
{
    WIN_ImeData *videodata = &winImeData;
    char ime_file[MAX_PATH];
    void *hime;
    HKL hkl;
    BOOL (WINAPI *ShowReadingWindow)(HIMC himc, BOOL bShow);
    DWORD dwVerSize;
    DWORD dwVerHandle = 0;
    LPVOID lpVerBuffer;
    LPVOID lpVerData = 0;
    UINT cbVerData = 0;
    DWORD dwLang;
    // reset previous values
    if (videodata->ime_hime) {
        SDL_UnloadObject(videodata->ime_hime);
        videodata->ime_hime = NULL;
        videodata->GetReadingString = NULL;
    }
    videodata->ime_ids[0] = 0;

    hkl = videodata->ime_hkl;
    if (!ImmGetIMEFileNameA(hkl, ime_file, sizeof(ime_file))) {
        return;
    }
    // load api functions
    hime = SDL_LoadObject(ime_file);
    if (hime) {
        videodata->GetReadingString = (UINT (WINAPI *)(HIMC, UINT, LPWSTR, PINT, BOOL*, PUINT))
            SDL_LoadFunction(hime, "GetReadingString");
        ShowReadingWindow = (BOOL (WINAPI *)(HIMC, BOOL))
            SDL_LoadFunction(hime, "ShowReadingWindow");
        videodata->ime_hime = hime;

        if (ShowReadingWindow) {
            HWND hwnd = videodata->ime_hwnd_current;
            HIMC himc = ImmGetContext(hwnd);
            if (himc) {
                ShowReadingWindow(himc, FALSE);
                ImmReleaseContext(hwnd, himc);
            }
        }
    }
    // set ime-ids
    dwLang = ((DWORD_PTR)hkl & 0xffff);
#define LCID_INVARIANT MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT)
    if (CompareStringA(LCID_INVARIANT, NORM_IGNORECASE, ime_file, -1, CHT_IMEFILENAME1, -1) != 2 && CompareStringA(LCID_INVARIANT, NORM_IGNORECASE, ime_file, -1, CHT_IMEFILENAME2, -1) != 2 && CompareStringA(LCID_INVARIANT, NORM_IGNORECASE, ime_file, -1, CHT_IMEFILENAME3, -1) != 2 && CompareStringA(LCID_INVARIANT, NORM_IGNORECASE, ime_file, -1, CHS_IMEFILENAME1, -1) != 2 && CompareStringA(LCID_INVARIANT, NORM_IGNORECASE, ime_file, -1, CHS_IMEFILENAME2, -1) != 2) {
        return;
    }
#undef LCID_INVARIANT
    dwVerSize = GetFileVersionInfoSizeA(ime_file, &dwVerHandle);
    if (dwVerSize) {
        lpVerBuffer = SDL_malloc(dwVerSize);
        if (lpVerBuffer) {
            if (GetFileVersionInfoA(ime_file, dwVerHandle, dwVerSize, lpVerBuffer)) {
                if (VerQueryValueA(lpVerBuffer, "\\", &lpVerData, &cbVerData)) {
#define pVerFixedInfo ((VS_FIXEDFILEINFO FAR *)lpVerData)
                    DWORD dwVer = pVerFixedInfo->dwFileVersionMS;
                    dwVer = (dwVer & 0x00ff0000) << 8 | (dwVer & 0x000000ff) << 16;
                    videodata->ime_ids[0] = dwVer | dwLang;
                    videodata->ime_ids[1] = pVerFixedInfo->dwFileVersionLS;
#undef pVerFixedInfo
                }
            }
            SDL_free(lpVerBuffer);
        }
    }
}

static void IME_SetWindow(HWND hwnd)
{
    WIN_ImeData *videodata = &winImeData;

    videodata->ime_hwnd_current = hwnd;
    if (videodata->ime_threadmgr) {
        struct ITfDocumentMgr *document_mgr = 0;
        if (SUCCEEDED(videodata->ime_threadmgr->lpVtbl->AssociateFocus(videodata->ime_threadmgr, hwnd, NULL, &document_mgr))) {
            if (document_mgr) {
                document_mgr->lpVtbl->Release(document_mgr);
            }
        }
    }
}

static SDL_bool IME_UpdateInputLocale(void)
{
    WIN_ImeData *videodata = &winImeData;
    HKL hklnext = GetKeyboardLayout(0);

    if (hklnext == videodata->ime_hkl) {
        return SDL_FALSE;
    }

    videodata->ime_hkl = hklnext;
#ifdef IME_DRAW_UI
    videodata->ime_candlistindexbase = (videodata->ime_hkl == CHT_HKL_DAYI) ? 0 : 1;
    videodata->ime_candhorizontal = (PRIMLANG() == LANG_KOREAN || LANG() == LANG_CHS) ? SDL_TRUE : SDL_FALSE;
#endif
    IME_SetupAPI();

    return SDL_TRUE;
}

static void IME_ClearComposition(void)
{
    WIN_ImeData *videodata = &winImeData;
    HIMC himc;
    if (!videodata->ime_hwnd_current) {
        return;
    }
    // SDL_assert(videodata->initialized);
    himc = ImmGetContext(videodata->ime_hwnd_current);
    if (!himc) {
        return;
    }

    ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    // if (videodata->ime_sdl_ui) { -- why would this be necessary?
    //     ImmSetCompositionString(himc, SCS_SETSTR, TEXT(""), sizeof(TCHAR), TEXT(""), sizeof(TCHAR));
    // }

    ImmNotifyIME(himc, NI_CLOSECANDIDATE, 0, 0);
    ImmReleaseContext(videodata->ime_hwnd_current, himc);
    SDL_SendEditingText("", 0, 0);
}

static SDL_bool IME_IsTextInputShown(void)
{
    WIN_ImeData *videodata = &winImeData;

    if (!videodata->ime_enabled) {
        return SDL_FALSE;
    }
    // SDL_assert(videodata->ime_initialized && videodata->ime_available);
    return videodata->ime_uicontext;
}

static void IME_GetCompositionString(HIMC himc, DWORD string)
{
    WIN_ImeData *videodata = &winImeData;
    LONG length;
    DWORD dwLang = ((DWORD_PTR)videodata->ime_hkl & 0xffff);

    length = ImmGetCompositionStringW(himc, string, NULL, 0);
    if (length > 0 && videodata->ime_composition_length < length) {
        void *composition = SDL_realloc(videodata->ime_composition, length + sizeof(WCHAR));
        if (composition != NULL) {
            videodata->ime_composition = (WCHAR *)composition;
            videodata->ime_composition_length = length;
        }
    }

    length = ImmGetCompositionStringW(
        himc,
        string,
        videodata->ime_composition,
        videodata->ime_composition_length);

    if (length < 0) {
        length = 0;
    }

    length /= sizeof(WCHAR);
    videodata->ime_cursor = LOWORD(ImmGetCompositionStringW(himc, GCS_CURSORPOS, 0, 0));
    if ((dwLang == LANG_CHT || dwLang == LANG_CHS) &&
        videodata->ime_cursor > 0 &&
        videodata->ime_cursor < (int)(videodata->ime_composition_length / (unsigned)sizeof(WCHAR)) &&
        (videodata->ime_composition[0] == 0x3000 || videodata->ime_composition[0] == 0x0020)) {
        // Traditional Chinese IMEs add a placeholder U+3000
        // Simplified Chinese IMEs seem to add a placeholder U+0020 sometimes
        int i;
        for (i = videodata->ime_cursor + 1; i < length; ++i) {
            videodata->ime_composition[i - 1] = videodata->ime_composition[i];
        }

        --length;
    }

    videodata->ime_composition[length] = 0;

    // Get the correct caret position if we've selected a candidate from the candidate window
    if (videodata->ime_cursor == 0 && length > 0) {
        int start = 0;
        int end = 0;

        length = ImmGetCompositionStringW(himc, GCS_COMPATTR, NULL, 0);
        if (length > 0) {
            Uint8 *attributes = (Uint8 *)SDL_malloc(length);
            if (attributes) {
                length = ImmGetCompositionStringW(himc, GCS_COMPATTR, attributes, length);

                for (start = 0; start < length; ++start) {
                    if (attributes[start] == ATTR_TARGET_CONVERTED || attributes[start] == ATTR_TARGET_NOTCONVERTED) {
                        break;
                    }
                }

                for (end = start; end < length; ++end) {
                    if (attributes[end] != ATTR_TARGET_CONVERTED && attributes[end] != ATTR_TARGET_NOTCONVERTED) {
                        break;
                    }
                }

                videodata->ime_cursor = end;

                SDL_free(attributes);
            }
        }
    }
}

static void IME_SendInputEvent(void)
{
    WIN_ImeData *videodata = &winImeData;
    char *s = WIN_StringToUTF8W(videodata->ime_composition);
    SDL_SendKeyboardText(s);
    SDL_free(s);

    videodata->ime_composition[0] = 0;
    videodata->ime_readingstring[0] = 0;
    videodata->ime_cursor = 0;
}

static void IME_SendEditingEvent(void)
{
    WIN_ImeData *videodata = &winImeData;
    char *s = NULL;
    WCHAR *buffer = NULL;
    size_t rssize;
    if (videodata->ime_readingstring[0]) {
        size_t cosize = SDL_wcslen(videodata->ime_composition) * sizeof(WCHAR);
        size_t cursize;
        char *cursor;

        buffer = (WCHAR *)SDL_malloc(cosize + sizeof(videodata->ime_readingstring) + sizeof(WCHAR));
        if (buffer == NULL) {
            SDL_OutOfMemory();
            return;
        }
        cursor = (char *)buffer;
        cursize = SDL_min(cosize, (size_t)videodata->ime_cursor * sizeof(WCHAR));
        SDL_memcpy(cursor, videodata->ime_composition, cursize);
        cursor += cursize;
        rssize = SDL_wcslen(videodata->ime_readingstring) * sizeof(WCHAR);
        SDL_memcpy(cursor, videodata->ime_readingstring, rssize);
        cursor += rssize;
        SDL_memcpy(cursor, &videodata->ime_composition[cursize / sizeof(WCHAR)], cosize - cursize + sizeof(WCHAR));
        // cursor += (cosize - cursize);
    } else {
        buffer = videodata->ime_composition;
        rssize = 0;
    }

    s = WIN_StringToUTF8W(buffer);
    if (buffer != videodata->ime_composition) {
        SDL_free(buffer);
    }
    SDL_SendEditingText(s, videodata->ime_cursor + (int)(rssize / sizeof(WCHAR)), 0);
    SDL_free(s);
}
#ifdef IME_DRAW_UI
static void IME_AddCandidate(UINT i, LPCWSTR candidate)
{
    WIN_ImeData *videodata = &winImeData;
    LPWSTR dst = &videodata->ime_candidates[i * MAX_CANDLENGTH];
    LPWSTR end = &dst[MAX_CANDLENGTH - 1];
    SDL_COMPILE_TIME_ASSERT(IME_CANDIDATE_INDEXING_REQUIRES, MAX_CANDLIST == 10);
    *dst++ = (WCHAR)(TEXT('0') + ((i + videodata->ime_candlistindexbase) % 10));
    if (!videodata->ime_candhorizontal) {
        *dst++ = TEXT(' ');
    }

    while (*candidate && dst < end) {
        *dst++ = *candidate++;
    }

    *dst = (WCHAR)'\0';
}

static void IME_GetCandidateList(HWND hwnd)
{
    WIN_ImeData *videodata = &winImeData;
    HIMC himc;
    DWORD size;
    LPCANDIDATELIST cand_list;

    if (IME_InitCandidateList() < 0) {
        return;
    }
    himc = ImmGetContext(hwnd);
    if (!himc) {
        return;
    }
    size = ImmGetCandidateListW(himc, 0, 0, 0);
    if (size != 0) {
        cand_list = (LPCANDIDATELIST)SDL_malloc(size);
        if (cand_list != NULL) {
            size = ImmGetCandidateListW(himc, 0, cand_list, size);
            if (size != 0) {
                UINT i, j;
                UINT page_start = 0;
                videodata->ime_candsel = cand_list->dwSelection;
                videodata->ime_candcount = cand_list->dwCount;

                if (LANG() == LANG_CHS && videodata->ime_ids[0]) {
                    const UINT maxcandchar = 18;
                    size_t cchars = 0;

                    for (i = 0; i < videodata->ime_candcount; ++i) {
                        size_t len = SDL_wcslen((LPWSTR)((DWORD_PTR)cand_list + cand_list->dwOffset[i])) + 1;
                        if (len + cchars > maxcandchar) {
                            if (i > cand_list->dwSelection) {
                                break;
                            }

                            page_start = i;
                            cchars = len;
                        } else {
                            cchars += len;
                        }
                    }
                    videodata->ime_candpgsize = i - page_start;
                } else {
                    videodata->ime_candpgsize = SDL_min(cand_list->dwPageSize == 0 ? MAX_CANDLIST : cand_list->dwPageSize, MAX_CANDLIST);
                    page_start = (cand_list->dwSelection / videodata->ime_candpgsize) * videodata->ime_candpgsize;
                }
                for (i = page_start, j = 0; (DWORD)i < cand_list->dwCount && j < videodata->ime_candpgsize; i++, j++) {
                    LPCWSTR candidate = (LPCWSTR)((DWORD_PTR)cand_list + cand_list->dwOffset[i]);
                    IME_AddCandidate(j, candidate);
                }
                // TODO: why was this necessary? check ime_candhorizontal instead? PRIMLANG() never equals LANG_CHT !
                // if (PRIMLANG() == LANG_KOREAN || (PRIMLANG() == LANG_CHT && !videodata->ime_ids[0]))
                //    videodata->ime_candsel = -1;
            }
            SDL_free(cand_list);
        }
    }
    ImmReleaseContext(hwnd, himc);
}

static int IME_InitCandidateList(void)
{
    WIN_ImeData *videodata = &winImeData;
    WCHAR *candidates = videodata->ime_candidates;

    videodata->ime_candcount = 0;
    if (candidates == NULL) {
        candidates = (WCHAR *)SDL_malloc(MAX_CANDSIZE);
        if (!candidates) {
            return -1;
        }
        videodata->ime_candidates = candidates;
    }

    SDL_memset(candidates, 0, MAX_CANDSIZE);

    videodata->ime_dirty = SDL_TRUE;
    IME_DestroyTextures();
    IME_SendEditingEvent();
    return 0;
}

static void IME_HideCandidateList(void)
{
    WIN_ImeData *videodata = &winImeData;
    videodata->ime_dirty = SDL_FALSE;
    IME_DestroyTextures();
    IME_SendEditingEvent();
}
#endif // IME_DRAW_UI
SDL_bool IME_HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM *lParam)
{
    WIN_ImeData *videodata = &winImeData;
    SDL_bool trap = SDL_FALSE;
    HIMC himc;
    if (!videodata->ime_enabled) {
        return SDL_FALSE;
    }
    // SDL_assert(videodata->ime_initialized && videodata->ime_available);

    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_PROCESSKEY) {
            videodata->ime_uicontext = SDL_TRUE;
            trap = SDL_TRUE;
        } else {
            videodata->ime_uicontext = SDL_FALSE;
        }
        break;
    case WM_INPUTLANGCHANGE:
        IME_InputLangChanged();
        break;
    case WM_IME_SETCONTEXT:
#ifdef IME_DRAW_UI
        if (videodata->ime_sdl_ui) {
            *lParam = 0;
        }
#endif
        break;
    case WM_IME_STARTCOMPOSITION:
        videodata->ime_suppress_endcomposition_event = SDL_FALSE;
        trap = SDL_TRUE;
        break;
    case WM_IME_COMPOSITION:
        trap = SDL_TRUE;
        himc = ImmGetContext(hwnd);
        if (*lParam & GCS_RESULTSTR) {
            videodata->ime_suppress_endcomposition_event = SDL_TRUE;
            IME_GetCompositionString(himc, GCS_RESULTSTR);
            SDL_SendEditingText("", 0, 0);
            IME_SendInputEvent();
        }
        if (*lParam & GCS_COMPSTR) {
            // if (!videodata->ime_sdl_ui) { -- always reset the reading string
                videodata->ime_readingstring[0] = 0;
            // }

            IME_GetCompositionString(himc, GCS_COMPSTR);
            IME_SendEditingEvent();
        }
        ImmReleaseContext(hwnd, himc);
        break;
    case WM_IME_ENDCOMPOSITION:
        videodata->ime_uicontext = SDL_FALSE;
        videodata->ime_composition[0] = 0;
        videodata->ime_readingstring[0] = 0;
        videodata->ime_cursor = 0;
        if (videodata->ime_suppress_endcomposition_event == SDL_FALSE) {
            SDL_SendEditingText("", 0, 0);
        }
        videodata->ime_suppress_endcomposition_event = SDL_FALSE;
        break;
    case WM_IME_NOTIFY:
        switch (wParam) {
        case IMN_SETCONVERSIONMODE:
        case IMN_SETOPENSTATUS:
            IME_UpdateInputLocale();
            break;
        case IMN_OPENCANDIDATE:
        case IMN_CHANGECANDIDATE:
            trap = SDL_TRUE;
            videodata->ime_uicontext = SDL_TRUE;
#ifdef IME_DRAW_UI
            if (videodata->ime_sdl_ui) {
                IME_GetCandidateList(hwnd);
            }
#endif
            break;
        case IMN_CLOSECANDIDATE:
            trap = SDL_TRUE;
            videodata->ime_uicontext = SDL_FALSE;
#ifdef IME_DRAW_UI
            if (videodata->ime_sdl_ui) {
                IME_HideCandidateList();
            }
#endif
            break;
        case IMN_PRIVATE:
        {
            IME_GetReadingString(hwnd);
            switch (videodata->ime_ids[0]) {
            case IMEID_CHT_VER42:
            case IMEID_CHT_VER43:
            case IMEID_CHT_VER44:
            case IMEID_CHS_VER41:
            case IMEID_CHS_VER42:
                if (*lParam == 1 || *lParam == 2) {
                    trap = SDL_TRUE;
                }

                break;
            case IMEID_CHT_VER50:
            case IMEID_CHT_VER51:
            case IMEID_CHT_VER52:
            case IMEID_CHT_VER60:
            case IMEID_CHS_VER53:
                if (*lParam == 16 || *lParam == 17 || *lParam == 26 || *lParam == 27 || *lParam == 28) {
                    trap = SDL_TRUE;
                }
                break;
            }
        } break;
        default:
            trap = SDL_TRUE;
            break;
        }
        break;
    }
    return trap;
}
#ifdef IME_USE_TF_API
#ifdef IME_DRAW_UI
static void IME_CloseCandidateList(void)
{
    WIN_ImeData *videodata = &winImeData;
    IME_HideCandidateList();
    videodata->ime_candcount = 0;
    SDL_free(videodata->ime_candidates);
    videodata->ime_candidates = NULL;
}

static void UILess_GetCandidateList(ITfCandidateListUIElement *pcandlist)
{
    WIN_ImeData *videodata = &winImeData;
    UINT selection = 0;
    UINT count = 0;
    UINT page = 0;
    UINT pgcount = 0;
    DWORD pgstart = 0;
    DWORD pgsize = 0;
    UINT i, j;

    if (IME_InitCandidateList() < 0) {
        return;
    }

    pcandlist->lpVtbl->GetSelection(pcandlist, &selection);
    pcandlist->lpVtbl->GetCount(pcandlist, &count);
    pcandlist->lpVtbl->GetCurrentPage(pcandlist, &page);

    videodata->ime_candsel = selection;
    videodata->ime_candcount = count;

    pcandlist->lpVtbl->GetPageIndex(pcandlist, 0, 0, &pgcount);
    if (pgcount > 0) {
        UINT *idxlist = SDL_malloc(sizeof(UINT) * pgcount);
        if (idxlist) {
            pcandlist->lpVtbl->GetPageIndex(pcandlist, idxlist, pgcount, &pgcount);
            pgstart = idxlist[page];
            if (page < pgcount - 1) {
                pgsize = SDL_min(count, idxlist[page + 1]) - pgstart;
            } else {
                pgsize = count - pgstart;
            }

            SDL_free(idxlist);
        }
    }
    videodata->ime_candpgsize = SDL_min(pgsize, MAX_CANDLIST);
    videodata->ime_candsel = videodata->ime_candsel - pgstart;
    for (i = pgstart, j = 0; (DWORD)i < count && j < videodata->ime_candpgsize; i++, j++) {
        BSTR bstr;
        if (SUCCEEDED(pcandlist->lpVtbl->GetString(pcandlist, i, &bstr))) {
            if (bstr) {
                IME_AddCandidate(j, bstr);
                SysFreeString(bstr);
            }
        }
    }
    // TODO: why was this necessary? check ime_candhorizontal instead?
    // if (PRIMLANG() == LANG_KOREAN)
    //    videodata->ime_candsel = -1;
}
#endif // IME_DRAW_UI
STDMETHODIMP_(ULONG)
TSFSink_AddRef(TSFSink *sink)
{
    return ++sink->refcount;
}

STDMETHODIMP_(ULONG)
TSFSink_Release(TSFSink *sink)
{
    --sink->refcount;
    if (sink->refcount == 0) {
        SDL_free(sink);
        return 0;
    }
    return sink->refcount;
}

STDMETHODIMP UIElementSink_QueryInterface(TSFSink *sink, REFIID riid, PVOID *ppv)
{
    if (!ppv) {
        return E_INVALIDARG;
    }

    *ppv = 0;
    if (WIN_IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = (IUnknown *)sink;
    } else if (WIN_IsEqualIID(riid, &IID_ITfUIElementSink)) {
        *ppv = (ITfUIElementSink *)sink;
    }

    if (*ppv) {
        TSFSink_AddRef(sink);
        return S_OK;
    }
    return E_NOINTERFACE;
}

ITfUIElement *UILess_GetUIElement(DWORD dwUIElementId)
{
    WIN_ImeData *videodata = &winImeData;
    ITfUIElementMgr *puiem = 0;
    ITfUIElement *pelem = 0;
    ITfThreadMgrEx *threadmgrex = videodata->ime_threadmgrex;

    if (SUCCEEDED(threadmgrex->lpVtbl->QueryInterface(threadmgrex, &IID_ITfUIElementMgr, (LPVOID *)&puiem))) {
        puiem->lpVtbl->GetUIElement(puiem, dwUIElementId, &pelem);
        puiem->lpVtbl->Release(puiem);
    }
    return pelem;
}

STDMETHODIMP UIElementSink_BeginUIElement(TSFSink *sink, DWORD dwUIElementId, BOOL *pbShow)
{
    ITfUIElement *element = UILess_GetUIElement(dwUIElementId);
    ITfReadingInformationUIElement *preading = 0;
#ifdef IME_DRAW_UI
    ITfCandidateListUIElement *pcandlist = 0;
    WIN_ImeData *videodata = &winImeData;
#endif
    if (!element) {
        return E_INVALIDARG;
    }
#ifdef IME_DRAW_UI
    *pbShow = videodata->ime_sdl_ui;
#else
    *pbShow = TRUE;
#endif
    if (SUCCEEDED(element->lpVtbl->QueryInterface(element, &IID_ITfReadingInformationUIElement, (LPVOID *)&preading))) {
        BSTR bstr;
        if (SUCCEEDED(preading->lpVtbl->GetString(preading, &bstr)) && bstr) {
            SysFreeString(bstr);
        }
        preading->lpVtbl->Release(preading);
#ifdef IME_DRAW_UI
    } else if (SUCCEEDED(element->lpVtbl->QueryInterface(element, &IID_ITfCandidateListUIElement, (LPVOID *)&pcandlist))) {
        videodata->ime_candref++;
        UILess_GetCandidateList(pcandlist);
        pcandlist->lpVtbl->Release(pcandlist);
#endif
    }
    return S_OK;
}

STDMETHODIMP UIElementSink_UpdateUIElement(TSFSink *sink, DWORD dwUIElementId)
{
    ITfUIElement *element = UILess_GetUIElement(dwUIElementId);
    ITfReadingInformationUIElement *preading = 0;
#ifdef IME_DRAW_UI
    ITfCandidateListUIElement *pcandlist = 0;
#endif
    WIN_ImeData *videodata = &winImeData;
    if (!element) {
        return E_INVALIDARG;
    }

    if (SUCCEEDED(element->lpVtbl->QueryInterface(element, &IID_ITfReadingInformationUIElement, (LPVOID *)&preading))) {
        BSTR bstr;
        if (SUCCEEDED(preading->lpVtbl->GetString(preading, &bstr)) && bstr) {
            WCHAR *s = (WCHAR *)bstr;
            SDL_wcslcpy(videodata->ime_readingstring, s, SDL_arraysize(videodata->ime_readingstring));
            IME_SendEditingEvent();
            SysFreeString(bstr);
        }
        preading->lpVtbl->Release(preading);
#ifdef IME_DRAW_UI
    } else if (SUCCEEDED(element->lpVtbl->QueryInterface(element, &IID_ITfCandidateListUIElement, (LPVOID *)&pcandlist))) {
        UILess_GetCandidateList(pcandlist);
        pcandlist->lpVtbl->Release(pcandlist);
#endif
    }
    return S_OK;
}

STDMETHODIMP UIElementSink_EndUIElement(TSFSink *sink, DWORD dwUIElementId)
{
    ITfUIElement *element = UILess_GetUIElement(dwUIElementId);
    ITfReadingInformationUIElement *preading = 0;
#ifdef IME_DRAW_UI
    ITfCandidateListUIElement *pcandlist = 0;
#endif
    WIN_ImeData *videodata = &winImeData;
    if (!element) {
        return E_INVALIDARG;
    }

    if (SUCCEEDED(element->lpVtbl->QueryInterface(element, &IID_ITfReadingInformationUIElement, (LPVOID *)&preading))) {
        videodata->ime_readingstring[0] = 0;
        IME_SendEditingEvent();
        preading->lpVtbl->Release(preading);
#ifdef IME_DRAW_UI
    } else if (SUCCEEDED(element->lpVtbl->QueryInterface(element, &IID_ITfCandidateListUIElement, (LPVOID *)&pcandlist))) {
        videodata->ime_candref--;
        if (videodata->ime_candref == 0) {
            IME_CloseCandidateList();
        }

        pcandlist->lpVtbl->Release(pcandlist);
#endif
    }
    return S_OK;
}

STDMETHODIMP IPPASink_QueryInterface(TSFSink *sink, REFIID riid, PVOID *ppv)
{
    if (!ppv) {
        return E_INVALIDARG;
    }

    *ppv = 0;
    if (WIN_IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = (IUnknown *)sink;
    } else if (WIN_IsEqualIID(riid, &IID_ITfInputProcessorProfileActivationSink)) {
        *ppv = (ITfInputProcessorProfileActivationSink *)sink;
    }

    if (*ppv) {
        TSFSink_AddRef(sink);
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP IPPASink_OnActivated(TSFSink *sink, DWORD dwProfileType, LANGID langid, REFCLSID clsid, REFGUID catid, REFGUID guidProfile, HKL hkl, DWORD dwFlags)
{
    // static const GUID SDL_TF_PROFILE_DAYI = { 0x037B2C25, 0x480C, 0x4D7F, { 0xB0, 0x27, 0xD6, 0xCA, 0x6B, 0x69, 0x78, 0x8A } };
    // WIN_ImeData *videodata = &winImeData;
    // videodata->ime_candlistindexbase = WIN_IsEqualGUID(&SDL_TF_PROFILE_DAYI, guidProfile) ? 0 : 1;
    if (WIN_IsEqualIID(catid, &GUID_TFCAT_TIP_KEYBOARD) && (dwFlags & TF_IPSINK_FLAG_ACTIVE)) {
        IME_InputLangChanged();
    }

    IME_HideCandidateList();
    return S_OK;
}

static void *vtUIElementSink[] = {
    (void *)(UIElementSink_QueryInterface),
    (void *)(TSFSink_AddRef),
    (void *)(TSFSink_Release),
    (void *)(UIElementSink_BeginUIElement),
    (void *)(UIElementSink_UpdateUIElement),
    (void *)(UIElementSink_EndUIElement)
};

static void *vtIPPASink[] = {
    (void *)(IPPASink_QueryInterface),
    (void *)(TSFSink_AddRef),
    (void *)(TSFSink_Release),
    (void *)(IPPASink_OnActivated)
};

static void UILess_EnableUIUpdates(void)
{
    WIN_ImeData *videodata = &winImeData;
    ITfSource *source = NULL;
    ITfThreadMgrEx *threadmgrex = videodata->ime_threadmgrex;
    if (!threadmgrex || videodata->ime_uielemsinkcookie != TF_INVALID_COOKIE) {
        return;
    }

    if (SUCCEEDED(threadmgrex->lpVtbl->QueryInterface(threadmgrex, &IID_ITfSource, (LPVOID *)&source))) {
        source->lpVtbl->AdviseSink(source, &IID_ITfUIElementSink, (IUnknown *)videodata->ime_uielemsink, &videodata->ime_uielemsinkcookie);
        source->lpVtbl->Release(source);
    }
}

static void UILess_DisableUIUpdates(void)
{
    WIN_ImeData *videodata = &winImeData;
    ITfSource *source = NULL;
    ITfThreadMgrEx *threadmgrex = videodata->ime_threadmgrex;
    if (!threadmgrex || videodata->ime_uielemsinkcookie == TF_INVALID_COOKIE) {
        return;
    }

    if (SUCCEEDED(threadmgrex->lpVtbl->QueryInterface(threadmgrex, &IID_ITfSource, (LPVOID *)&source))) {
        source->lpVtbl->UnadviseSink(source, videodata->ime_uielemsinkcookie);
        videodata->ime_uielemsinkcookie = TF_INVALID_COOKIE;
        source->lpVtbl->Release(source);
    }
}

static SDL_bool UILess_SetupSinks(void)
{
    WIN_ImeData *videodata = &winImeData;
    TfClientId clientid = 0;
    SDL_bool result = SDL_FALSE;
    ITfSource *source = 0;
    if (SUCCEEDED(CoCreateInstance(&CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, &IID_ITfThreadMgrEx, (LPVOID *)&videodata->ime_threadmgrex))) {
        if (SUCCEEDED(videodata->ime_threadmgrex->lpVtbl->ActivateEx(videodata->ime_threadmgrex, &clientid, TF_TMAE_UIELEMENTENABLEDONLY))) {
            TSFSink *uielemsink = (TSFSink *)SDL_malloc(sizeof(TSFSink));
            TSFSink *ippasink = (TSFSink *)SDL_malloc(sizeof(TSFSink));

            if (uielemsink != NULL && ippasink != NULL) {
                uielemsink->lpVtbl = vtUIElementSink;
                uielemsink->refcount = 1;

                ippasink->lpVtbl = vtIPPASink;
                ippasink->refcount = 1;

                videodata->ime_uielemsink = uielemsink;
                videodata->ime_ippasink = ippasink;

                if (SUCCEEDED(videodata->ime_threadmgrex->lpVtbl->QueryInterface(videodata->ime_threadmgrex, &IID_ITfSource, (LPVOID *)&source))) {
                    if (SUCCEEDED(source->lpVtbl->AdviseSink(source, &IID_ITfUIElementSink, (IUnknown *)uielemsink, &videodata->ime_uielemsinkcookie))) {
                        if (SUCCEEDED(source->lpVtbl->AdviseSink(source, &IID_ITfInputProcessorProfileActivationSink, (IUnknown *)ippasink, &videodata->ime_alpnsinkcookie))) {
                            result = SDL_TRUE;
                        }
                    }
                    source->lpVtbl->Release(source);
                }
            } else {
                SDL_free(uielemsink);
                SDL_free(ippasink);
            }
        }
    }
    return result;
}

static void UILess_ReleaseSinks(void)
{
    WIN_ImeData *videodata = &winImeData;
    ITfSource *source = NULL;
    ITfThreadMgrEx *threadmgrex = videodata->ime_threadmgrex;
    if (threadmgrex) {
        if (SUCCEEDED(threadmgrex->lpVtbl->QueryInterface(threadmgrex, &IID_ITfSource, (LPVOID *)&source))) {
            source->lpVtbl->UnadviseSink(source, videodata->ime_uielemsinkcookie);
            source->lpVtbl->UnadviseSink(source, videodata->ime_alpnsinkcookie);
            source->lpVtbl->Release(source);
        }
        threadmgrex->lpVtbl->Deactivate(threadmgrex);
        threadmgrex->lpVtbl->Release(threadmgrex);
        videodata->ime_threadmgrex = NULL;
        if (videodata->ime_uielemsink) {
            TSFSink_Release(videodata->ime_uielemsink);
            videodata->ime_uielemsink = NULL;
        }
        if (videodata->ime_ippasink) {
            TSFSink_Release(videodata->ime_ippasink);
            videodata->ime_ippasink = NULL;
        }
    }
}
#endif // IME_USE_TF_API
#ifdef IME_DRAW_UI
static void *StartDrawToBitmap(HDC hdc, HBITMAP *hhbm, int width, int height)
{
    BITMAPINFO info;
    BITMAPINFOHEADER *infoHeader = &info.bmiHeader;
    BYTE *bits = NULL;
    if (hhbm) {
        SDL_zero(info);
        infoHeader->biSize = sizeof(BITMAPINFOHEADER);
        infoHeader->biWidth = width;
        infoHeader->biHeight = (LONG)-1 * SDL_abs(height);
        infoHeader->biPlanes = 1;
        infoHeader->biBitCount = 32;
        infoHeader->biCompression = BI_RGB;
        *hhbm = CreateDIBSection(hdc, &info, DIB_RGB_COLORS, (void **)&bits, 0, 0);
        if (*hhbm) {
            SelectObject(hdc, *hhbm);
        }
    }
    return bits;
}

static void StopDrawToBitmap(HDC hdc, HBITMAP *hhbm)
{
    if (hhbm && *hhbm) {
        DeleteObject(*hhbm);
        *hhbm = NULL;
    }
}

/* This draws only within the specified area and fills the entire region. */
static void DrawRect(HDC hdc, int left, int top, int right, int bottom, int pensize)
{
    /* The case of no pen (PenSize = 0) is automatically taken care of. */
    const int penadjust = (int)SDL_floor(pensize / 2.0f - 0.5f);
    left += pensize / 2u;
    top += pensize / 2u;
    right -= penadjust;
    bottom -= penadjust;
    Rectangle(hdc, left, top, right, bottom);
}

static void IME_DestroyTextures(void)
{
}

static void IME_PositionCandidateList(SIZE size)
{
    WIN_ImeData *videodata = &winImeData;
    int left, top, right, bottom;
    SDL_bool ok = SDL_FALSE;
    int winw = videodata->ime_winwidth;
    int winh = videodata->ime_winheight;

    /* Bottom */
    left = videodata->ime_rect.x;
    top = videodata->ime_rect.y + videodata->ime_rect.h;
    right = left + size.cx;
    bottom = top + size.cy;
    if (right >= winw) {
        left -= right - winw;
        right = winw;
    }
    if (bottom < winh) {
        ok = SDL_TRUE;
    }

    /* Top */
    if (!ok) {
        left = videodata->ime_rect.x;
        top = videodata->ime_rect.y - size.cy;
        right = left + size.cx;
        bottom = videodata->ime_rect.y;
        if (right >= winw) {
            left -= right - winw;
            right = winw;
        }
        if (top >= 0) {
            ok = SDL_TRUE;
        }
    }

    /* Right */
    if (!ok) {
        left = videodata->ime_rect.x + size.cx;
        top = 0;
        right = left + size.cx;
        bottom = size.cy;
        if (right < winw) {
            ok = SDL_TRUE;
        }
    }

    /* Left */
    if (!ok) {
        left = videodata->ime_rect.x - size.cx;
        top = 0;
        right = videodata->ime_rect.x;
        bottom = size.cy;
        if (right >= 0) {
            ok = SDL_TRUE;
        }
    }

    /* Window too small, show at (0,0) */
    if (!ok) {
        left = 0;
        top = 0;
        right = size.cx;
        bottom = size.cy;
    }

    videodata->ime_candlistrect.x = left;
    videodata->ime_candlistrect.y = top;
    videodata->ime_candlistrect.w = right - left;
    videodata->ime_candlistrect.h = bottom - top;
}

static void IME_RenderCandidateList(HDC hdc)
{
    WIN_ImeData *videodata = &winImeData;
    int i, j;
    SIZE size = { 0 };
    SIZE candsizes[MAX_CANDLIST];
    SIZE maxcandsize = { 0 };
    HBITMAP hbm = NULL;
    int candcount = SDL_min(SDL_min(MAX_CANDLIST, videodata->ime_candcount), videodata->ime_candpgsize);
    SDL_bool horizontal = videodata->ime_candhorizontal;

    const int listborder = 1;
    const int listpadding = 0;
    const int listbordercolor = RGB(0xB4, 0xC7, 0xAA);
    const int listfillcolor = RGB(255, 255, 255);

    const int candborder = 1;
    const int candpadding = 0;
    const int candmargin = 1;
    const COLORREF candbordercolor = RGB(255, 255, 255);
    const COLORREF candfillcolor = RGB(255, 255, 255);
    const COLORREF candtextcolor = RGB(0, 0, 0);
    const COLORREF selbordercolor = RGB(0x84, 0xAC, 0xDD);
    const COLORREF selfillcolor = RGB(0xD2, 0xE6, 0xFF);
    const COLORREF seltextcolor = RGB(0, 0, 0);
    const int horzcandspacing = 5;

    HPEN listpen = listborder != 0 ? CreatePen(PS_SOLID, listborder, listbordercolor) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH listbrush = CreateSolidBrush(listfillcolor);
    HPEN candpen = candborder != 0 ? CreatePen(PS_SOLID, candborder, candbordercolor) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH candbrush = CreateSolidBrush(candfillcolor);
    HPEN selpen = candborder != 0 ? CreatePen(PS_DOT, candborder, selbordercolor) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH selbrush = CreateSolidBrush(selfillcolor);
    HFONT font = CreateFont((int)(1 + videodata->ime_rect.h * 0.75f), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_CHARACTER_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, VARIABLE_PITCH | FF_SWISS, TEXT("Microsoft Sans Serif"));

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, font);

    for (i = 0; i < candcount; ++i) {
        const WCHAR *s = &videodata->ime_candidates[i * MAX_CANDLENGTH];
        if (!*s) {
            candcount = i;
            break;
        }

        GetTextExtentPoint32W(hdc, s, (int)SDL_wcslen(s), &candsizes[i]);
        maxcandsize.cx = SDL_max(maxcandsize.cx, candsizes[i].cx);
        maxcandsize.cy = SDL_max(maxcandsize.cy, candsizes[i].cy);
    }
    if (!horizontal) {
        size.cx =
            (listborder * 2) +
            (listpadding * 2) +
            (candmargin * 2) +
            (candborder * 2) +
            (candpadding * 2) +
            (maxcandsize.cx);
        size.cy =
            (listborder * 2) +
            (listpadding * 2) +
            ((candcount + 1) * candmargin) +
            (candcount * candborder * 2) +
            (candcount * candpadding * 2) +
            (candcount * maxcandsize.cy);
    } else {
        size.cx =
            (LONG)(listborder * 2) +
            (listpadding * 2) +
            ((candcount + 1) * candmargin) +
            (candcount * candborder * 2) +
            (candcount * candpadding * 2) +
            ((candcount - 1) * horzcandspacing);

        for (i = 0; i < candcount; ++i) {
            size.cx += candsizes[i].cx;
        }

        size.cy =
            (listborder * 2) +
            (listpadding * 2) +
            (candmargin * 2) +
            (candborder * 2) +
            (candpadding * 2) +
            (maxcandsize.cy);
    }

    StartDrawToBitmap(hdc, &hbm, size.cx, size.cy);

    SelectObject(hdc, listpen);
    SelectObject(hdc, listbrush);
    DrawRect(hdc, 0, 0, size.cx, size.cy, listborder);

    SelectObject(hdc, candpen);
    SelectObject(hdc, candbrush);
    SetTextColor(hdc, candtextcolor);
    SetBkMode(hdc, TRANSPARENT);

    for (i = 0; i < candcount; ++i) {
        const WCHAR *s = &videodata->ime_candidates[i * MAX_CANDLENGTH];
        int left, top, right, bottom;

        if (!horizontal) {
            left = listborder + listpadding + candmargin;
            top = listborder + listpadding + (i * candborder * 2) + (i * candpadding * 2) + ((i + 1) * candmargin) + (i * maxcandsize.cy);
            right = size.cx - listborder - listpadding - candmargin;
            bottom = top + maxcandsize.cy + (candpadding * 2) + (candborder * 2);
        } else {
            left = listborder + listpadding + (i * candborder * 2) + (i * candpadding * 2) + ((i + 1) * candmargin) + (i * horzcandspacing);

            for (j = 0; j < i; ++j) {
                left += candsizes[j].cx;
            }

            top = listborder + listpadding + candmargin;
            right = left + candsizes[i].cx + (candpadding * 2) + (candborder * 2);
            bottom = size.cy - listborder - listpadding - candmargin;
        }

        if (i == videodata->ime_candsel) {
            SelectObject(hdc, selpen);
            SelectObject(hdc, selbrush);
            SetTextColor(hdc, seltextcolor);
        } else {
            SelectObject(hdc, candpen);
            SelectObject(hdc, candbrush);
            SetTextColor(hdc, candtextcolor);
        }

        DrawRect(hdc, left, top, right, bottom, candborder);
        ExtTextOutW(hdc, left + candborder + candpadding, top + candborder + candpadding, 0, NULL, s, (int)SDL_wcslen(s), NULL);
    }
    StopDrawToBitmap(hdc, &hbm);

    DeleteObject(listpen);
    DeleteObject(listbrush);
    DeleteObject(candpen);
    DeleteObject(candbrush);
    DeleteObject(selpen);
    DeleteObject(selbrush);
    DeleteObject(font);

    IME_PositionCandidateList(size);
}

static void IME_Render(void)
{
    HDC hdc = CreateCompatibleDC(NULL);

    IME_RenderCandidateList(hdc);

    DeleteDC(hdc);
}

void IME_Present(void)
{
    WIN_ImeData *videodata = &winImeData;
    if (videodata->ime_dirty) {
        videodata->ime_dirty = SDL_FALSE;
        IME_Render();
    }

    /* FIXME: Need to show the IME bitmap */
}
#endif // IME_DRAW_UI
SDL_bool WIN_IsTextInputShown(void)
{
    return IME_IsTextInputShown();
}

void WIN_ClearComposition(void)
{
    IME_ClearComposition();
}

#endif /* SDL_DISABLE_WINDOWS_IME */

#endif /* SDL_VIDEO_DRIVER_WINDOWS */

/* vi: set ts=4 sw=4 expandtab: */
