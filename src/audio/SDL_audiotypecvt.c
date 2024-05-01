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
#include "../SDL_internal.h"

#include "SDL_audio.h"
#include "SDL_cpuinfo.h"

#include "SDL_audio_c.h"
#include "SDL_audiotypecvt.h"

#ifdef __ARM_NEON
#define HAVE_NEON_INTRINSICS 1
#endif

#ifdef __SSE2__
#define HAVE_SSE2_INTRINSICS
#endif

#if defined(__x86_64__) && defined(HAVE_SSE2_INTRINSICS)
#define HAVE_SSE2_SUPPORT 1  /* x86_64 guarantees SSE2. */
#elif defined(__MACOSX__) && defined(HAVE_SSE2_INTRINSICS)
#define HAVE_SSE2_SUPPORT 1  /* Mac OS X/Intel guarantees SSE2. */
#elif defined(__ARM_ARCH) && (__ARM_ARCH >= 8) && defined(HAVE_NEON_INTRINSICS)
#define HAVE_NEON_SUPPORT 1 /* ARMv8+ promise NEON. */
#elif defined(__APPLE__) && defined(__ARM_ARCH) && (__ARM_ARCH >= 7) && defined(HAVE_NEON_INTRINSICS)
#define HAVE_NEON_SUPPORT 1 /* All Apple ARMv7 chips promise NEON support. */
#endif

#ifndef HAVE_SSE2_SUPPORT
#define HAVE_SSE2_SUPPORT 0
#endif
#ifndef HAVE_NEON_SUPPORT
#define HAVE_NEON_SUPPORT 0
#endif

/* Function pointers set to a CPU-specific implementation. */
SDL_AudioFilter SDL_Convert_Byteswap16 = NULL;
SDL_AudioFilter SDL_Convert_Byteswap32 = NULL;

SDL_AudioFilter SDL_Convert_S8_to_F32 = NULL;
SDL_AudioFilter SDL_Convert_U8_to_F32 = NULL;
SDL_AudioFilter SDL_Convert_S16_to_F32 = NULL;
SDL_AudioFilter SDL_Convert_U16_to_F32 = NULL;
SDL_AudioFilter SDL_Convert_S32_to_F32 = NULL;
SDL_AudioFilter SDL_Convert_F32_to_S8 = NULL;
SDL_AudioFilter SDL_Convert_F32_to_U8 = NULL;
SDL_AudioFilter SDL_Convert_F32_to_S16 = NULL;
SDL_AudioFilter SDL_Convert_F32_to_U16 = NULL;
SDL_AudioFilter SDL_Convert_F32_to_S32 = NULL;

#define DIVBY128     0.0078125f
#define DIVBY32768   0.000030517578125f
#define DIVBY8388607 0.00000011920930376163766f
#define DIVBY2147483648 0.0000000004656612873077392578125f /* 0x1p-31f */

/* This code requires that floats are in the IEEE-754 binary32 format */
SDL_COMPILE_TIME_ASSERT(float_bits, sizeof(float) == sizeof(Uint32));

union float_bits {
    Uint32 u32;
    float f32;
};

/* Create a bit-mask based on the sign-bit. Should optimize to a single arithmetic-shift-right */
#define SIGNMASK(x) (Uint32)(0u - ((Uint32)(x) >> 31))

static void SDLCALL SDL_Convert_Byteswap16_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Uint16);
    Uint16 *ptr = (Uint16 *)cvt->buf;
    int i = num_samples;
#if DEBUG_CONVERT
    SDL_Log("SDL_AUDIO_CONVERT: Converting byte order (16)\n");
#endif
    for ( ; i; --i, ++ptr) {
        ptr[0] = SDL_Swap16(ptr[0]);
    }
}

static void SDLCALL SDL_Convert_Byteswap32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Uint32);
    Uint32 *ptr = (Uint32 *)cvt->buf;
    int i = num_samples;
#if DEBUG_CONVERT
    SDL_Log("SDL_AUDIO_CONVERT: Converting byte order (32)\n");
#endif
    for ( ; i; --i, ++ptr) {
        ptr[0] = SDL_Swap32(ptr[0]);
    }
}


static void SDLCALL SDL_Convert_S8_to_F32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt;
    const Sint8 *src = (const Sint8 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 4);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_S8", "AUDIO_F32");

    cvt->len_cvt *= 4;

    for ( ; i; --i) {
        src--;
        dst--;
        /* 1) Construct a float in the range [65536.0, 65538.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        {
        union float_bits x;
        x.u32 = (Uint8)src[0] ^ 0x47800080u;
        dst[0] = x.f32 - 65537.0f;
        }
    }
}

static void SDLCALL SDL_Convert_U8_to_F32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt;
    const Uint8 *src = (const Uint8 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 4);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_U8", "AUDIO_F32");

    cvt->len_cvt *= 4;

    for ( ; i; --i) {
        src--;
        dst--;
        /* 1) Construct a float in the range [65536.0, 65538.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        {
        union float_bits x;
        x.u32 = src[0] ^ 0x47800000u;
        dst[0] = x.f32 - 65537.0f;
        }
    }
}

static void SDLCALL SDL_Convert_S16_to_F32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Sint16);
    const Sint16 *src = (const Sint16 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 2);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_S16", "AUDIO_F32");

    cvt->len_cvt *= 2;

    for ( ; i; --i) {
        src--;
        dst--;
        /* 1) Construct a float in the range [256.0, 258.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        {
        union float_bits x;
        x.u32 = (Uint16)src[0] ^ 0x43808000u;
        dst[0] = x.f32 - 257.0f;
        }
    }
}

static void SDLCALL SDL_Convert_U16_to_F32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Uint16);
    const Uint16 *src = (const Uint16 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 2);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_U16", "AUDIO_F32");

    cvt->len_cvt *= 2;

    for ( ; i; --i) {
        src--;
        dst--;
        dst[0] = (((float)src[0]) * DIVBY32768) - 1.0f;
    }
}

static void SDLCALL SDL_Convert_S32_to_F32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Sint32);
    const Sint32 *src = (const Sint32 *)cvt->buf;
    float *dst = (float *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_S32", "AUDIO_F32");

    for ( ; i; --i, ++src, ++dst) {
        dst[0] = (float)src[0] * DIVBY2147483648;
    }
}

static void SDLCALL SDL_Convert_F32_to_S8_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint8 *dst = (Sint8 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S8");

    cvt->len_cvt /= 4u;

    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
         * 2) Shift the integer range from [0x47BFFF80, 0x47C00080] to [-128, 128]
         * 3) Clamp the value to [-128, 127] */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0] + 98304.0f;

        y = x.u32 - 0x47C00000u;
        z = 0x7Fu - (y ^ SIGNMASK(y));
        y = y ^ (z & SIGNMASK(z));

        dst[0] = (Sint8)(y & 0xFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_U8_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Uint8 *dst = (Uint8 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U8");

    cvt->len_cvt /= 4u;

    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
         * 2) Shift the integer range from [0x47BFFF80, 0x47C00080] to [-128, 128]
         * 3) Clamp the value to [-128, 127]
         * 4) Shift the integer range from [-128, 127] to [0, 255] */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0] + 98304.0f;

        y = x.u32 - 0x47C00000u;
        z = 0x7Fu - (y ^ SIGNMASK(y));
        y = (y ^ 0x80u) ^ (z & SIGNMASK(z));

        dst[0] = (Uint8)(y & 0xFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_S16_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint16 *dst = (Sint16 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S16");

    cvt->len_cvt /= 2u;

    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [383.0, 385.0]
         * 2) Shift the integer range from [0x43BF8000, 0x43C08000] to [-32768, 32768]
         * 3) Clamp values outside the [-32768, 32767] range */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0] + 384.0f;

        y = x.u32 - 0x43C00000u;
        z = 0x7FFFu - (y ^ SIGNMASK(y));
        y = y ^ (z & SIGNMASK(z));

        dst[0] = (Sint16)(y & 0xFFFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_U16_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Uint16 *dst = (Uint16 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U16");

    cvt->len_cvt /= 2u;

    for ( ; i; --i, ++src, ++dst) {
        const float sample = src[0];
        if (sample >= 1.0f) {
            dst[0] = 65535;
        } else if (sample <= -1.0f) {
            dst[0] = 0;
        } else {
            dst[0] = (Uint16)((sample + 1.0f) * 32767.0f);
        }
    }
}

static void SDLCALL SDL_Convert_F32_to_S32_Scalar(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint32 *dst = (Sint32 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S32");

    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [-2147483648.0, 2147483648.0]
         * 2) Set values outside the [-2147483648.0, 2147483647.0] range to -2147483648.0
         * 3) Convert the float to an integer, and fixup values outside the valid range */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0];

        y = x.u32 + 0x0F800000u;
        z = y - 0xCF000000u;
        z &= SIGNMASK(y ^ z);
        x.u32 = y - z;

        dst[0] = (Sint32)x.f32 ^ (Sint32)SIGNMASK(z);
    }
}

#ifdef HAVE_SSE2_INTRINSICS
static void SDLCALL SDL_Convert_S8_to_F32_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt;
    const Sint8 *src = (const Sint8 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 4);
    int i = num_samples;

    /* 1) Flip the sign bit to convert from S8 to U8 format
     * 2) Construct a float in the range [65536.0, 65538.0)
     * 3) Shift the float range to [-1.0, 1.0)
     * dst[i] = i2f((src[i] ^ 0x80) | 0x47800000) - 65537.0 */
    const __m128i zero = _mm_setzero_si128();
    const __m128i flipper = _mm_set1_epi8(-0x80);
    const __m128i caster = _mm_set1_epi16(0x4780 /* 0x47800000 = f2i(65536.0) */);
    const __m128 offset = _mm_set1_ps(-65537.0);

    LOG_DEBUG_CONVERT("AUDIO_S8", "AUDIO_F32 (using SSE2)");

    cvt->len_cvt *= 4;

    while (i >= 16) {
        i -= 16;
        src -= 16;
        dst -= 16;

        {
        const __m128i bytes = _mm_xor_si128(_mm_loadu_si128((const __m128i *)&src[0]), flipper);

        const __m128i shorts1 = _mm_unpacklo_epi8(bytes, zero);
        const __m128i shorts2 = _mm_unpackhi_epi8(bytes, zero);

        const __m128 floats1 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts1, caster)), offset);
        const __m128 floats2 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts1, caster)), offset);
        const __m128 floats3 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts2, caster)), offset);
        const __m128 floats4 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts2, caster)), offset);

        _mm_storeu_ps(&dst[0], floats1);
        _mm_storeu_ps(&dst[4], floats2);
        _mm_storeu_ps(&dst[8], floats3);
        _mm_storeu_ps(&dst[12], floats4);
        }
    }

    for ( ; i; i--) {
        src--;
        dst--;
        _mm_store_ss(&dst[0], _mm_add_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((Uint8)src[0] ^ 0x47800080u)), offset));
    }
}

static void SDLCALL SDL_Convert_U8_to_F32_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt;
    const Sint8 *src = (const Sint8 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 4);
    int i = num_samples;

    /* 1) Construct a float in the range [65536.0, 65538.0)
     * 2) Shift the float range to [-1.0, 1.0)
     * dst[i] = i2f(src[i] | 0x47800000) - 65537.0 */
    const __m128i zero = _mm_setzero_si128();
    const __m128i caster = _mm_set1_epi16(0x4780 /* 0x47800000 = f2i(65536.0) */);
    const __m128 offset = _mm_set1_ps(-65537.0);

    LOG_DEBUG_CONVERT("AUDIO_U8", "AUDIO_F32 (using SSE2)");

    cvt->len_cvt *= 4;

    while (i >= 16) {
        i -= 16;
        src -= 16;
        dst -= 16;

        {
        const __m128i bytes = _mm_loadu_si128((const __m128i *)&src[0]);

        const __m128i shorts1 = _mm_unpacklo_epi8(bytes, zero);
        const __m128i shorts2 = _mm_unpackhi_epi8(bytes, zero);

        const __m128 floats1 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts1, caster)), offset);
        const __m128 floats2 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts1, caster)), offset);
        const __m128 floats3 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts2, caster)), offset);
        const __m128 floats4 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts2, caster)), offset);

        _mm_storeu_ps(&dst[0], floats1);
        _mm_storeu_ps(&dst[4], floats2);
        _mm_storeu_ps(&dst[8], floats3);
        _mm_storeu_ps(&dst[12], floats4);
        }
    }

    for ( ; i; i--) {
        src--;
        dst--;
        _mm_store_ss(&dst[0], _mm_add_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((Uint8)src[0] ^ 0x47800000u)), offset));
    }
}

static void SDLCALL SDL_Convert_S16_to_F32_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Sint16);
    const Sint16 *src = (const Sint16 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 2);
    int i = num_samples;

    /* 1) Flip the sign bit to convert from S16 to U16 format
     * 2) Construct a float in the range [256.0, 258.0)
     * 3) Shift the float range to [-1.0, 1.0)
     * dst[i] = i2f((src[i] ^ 0x8000) | 0x43800000) - 257.0 */
    const __m128i flipper = _mm_set1_epi16(-0x8000);
    const __m128i caster = _mm_set1_epi16(0x4380 /* 0x43800000 = f2i(256.0) */);
    const __m128 offset = _mm_set1_ps(-257.0f);

    LOG_DEBUG_CONVERT("AUDIO_S16", "AUDIO_F32 (using SSE2)");

    cvt->len_cvt *= 2;

    while (i >= 16) {
        i -= 16;
        src -= 16;
        dst -= 16;

        {
        const __m128i shorts1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)&src[0]), flipper);
        const __m128i shorts2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)&src[8]), flipper);

        const __m128 floats1 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts1, caster)), offset);
        const __m128 floats2 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts1, caster)), offset);
        const __m128 floats3 = _mm_add_ps(_mm_castsi128_ps(_mm_unpacklo_epi16(shorts2, caster)), offset);
        const __m128 floats4 = _mm_add_ps(_mm_castsi128_ps(_mm_unpackhi_epi16(shorts2, caster)), offset);

        _mm_storeu_ps(&dst[0], floats1);
        _mm_storeu_ps(&dst[4], floats2);
        _mm_storeu_ps(&dst[8], floats3);
        _mm_storeu_ps(&dst[12], floats4);
        }
    }

    for ( ; i; i--) {
        src--;
        dst--;
        _mm_store_ss(&dst[0], _mm_add_ss(_mm_castsi128_ps(_mm_cvtsi32_si128((Uint16)src[0] ^ 0x43808000u)), offset));
    }
}

static void SDLCALL SDL_Convert_U16_to_F32_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Uint16);
    const Uint16 *src = (const Uint16 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 2);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_U16", "AUDIO_F32 (using SSE2)");

    cvt->len_cvt *= 2;

    {
        const __m128 divby32768 = _mm_set1_ps(DIVBY32768);
        const __m128 minus1 = _mm_set1_ps(-1.0f);
        while (i >= 8) {                                               /* 8 * 16-bit */
            src -= 8;
            dst -= 8;
            {
            const __m128i ints = _mm_loadu_si128((__m128i const *)&src[0]); /* get 8 sint16 into an XMM register. */
            /* treat as int32, shift left to clear every other sint16, then back right with zero-extend. Now sint32. */
            const __m128i a = _mm_srli_epi32(_mm_slli_epi32(ints, 16), 16);
            /* right-shift-sign-extend gets us sint32 with the other set of values. */
            const __m128i b = _mm_srli_epi32(ints, 16);
            /* Interleave these back into the right order, convert to float, multiply, store. */
            _mm_storeu_ps(&dst[0], _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_unpacklo_epi32(a, b)), divby32768), minus1));
            _mm_storeu_ps(&dst[4], _mm_add_ps(_mm_mul_ps(_mm_cvtepi32_ps(_mm_unpackhi_epi32(a, b)), divby32768), minus1));
            }
            i -= 8;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; i--) {
        src--;
        dst--;
        dst[0] = (((float)src[0]) * DIVBY32768) - 1.0f;
    }
}

static void SDLCALL SDL_Convert_S32_to_F32_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Sint32);
    const Sint32 *src = (const Sint32 *)cvt->buf;
    float *dst = (float *)cvt->buf;
    int i = num_samples;

    /* dst[i] = f32(src[i]) / f32(0x80000000) */
    const __m128 scaler = _mm_set1_ps(DIVBY2147483648);

    LOG_DEBUG_CONVERT("AUDIO_S32", "AUDIO_F32 (using SSE2)");

    while (i >= 16) {
        i -= 16;

        {
        const __m128i ints1 = _mm_loadu_si128((const __m128i *)&src[0]);
        const __m128i ints2 = _mm_loadu_si128((const __m128i *)&src[4]);
        const __m128i ints3 = _mm_loadu_si128((const __m128i *)&src[8]);
        const __m128i ints4 = _mm_loadu_si128((const __m128i *)&src[12]);

        const __m128 floats1 = _mm_mul_ps(_mm_cvtepi32_ps(ints1), scaler);
        const __m128 floats2 = _mm_mul_ps(_mm_cvtepi32_ps(ints2), scaler);
        const __m128 floats3 = _mm_mul_ps(_mm_cvtepi32_ps(ints3), scaler);
        const __m128 floats4 = _mm_mul_ps(_mm_cvtepi32_ps(ints4), scaler);

        _mm_storeu_ps(&dst[0], floats1);
        _mm_storeu_ps(&dst[4], floats2);
        _mm_storeu_ps(&dst[8], floats3);
        _mm_storeu_ps(&dst[12], floats4);
        }
        src += 16;
        dst += 16;
    }

    for ( ; i; i--, src++, dst++) {
        _mm_store_ss(&dst[0], _mm_mul_ss(_mm_cvt_si2ss(_mm_setzero_ps(), src[0]), scaler));
    }
}

static void SDLCALL SDL_Convert_F32_to_S8_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint8 *dst = (Sint8 *)cvt->buf;
    int i = num_samples;

    /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
     * 2) Extract the lowest 16 bits and clamp to [-128, 127]
     * Overflow is correctly handled for inputs between roughly [-255.0, 255.0]
     * dst[i] = clamp(i16(f2i(src[i] + 98304.0) & 0xFFFF), -128, 127) */
    const __m128 offset = _mm_set1_ps(98304.0f);
    const __m128i mask = _mm_set1_epi16(0xFF);

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S8 (using SSE2)");

    cvt->len_cvt /= (unsigned)sizeof(float);

    while (i >= 16) {
        const __m128 floats1 = _mm_loadu_ps(&src[0]);
        const __m128 floats2 = _mm_loadu_ps(&src[4]);
        const __m128 floats3 = _mm_loadu_ps(&src[8]);
        const __m128 floats4 = _mm_loadu_ps(&src[12]);

        const __m128i ints1 = _mm_castps_si128(_mm_add_ps(floats1, offset));
        const __m128i ints2 = _mm_castps_si128(_mm_add_ps(floats2, offset));
        const __m128i ints3 = _mm_castps_si128(_mm_add_ps(floats3, offset));
        const __m128i ints4 = _mm_castps_si128(_mm_add_ps(floats4, offset));

        const __m128i shorts1 = _mm_and_si128(_mm_packs_epi16(ints1, ints2), mask);
        const __m128i shorts2 = _mm_and_si128(_mm_packs_epi16(ints3, ints4), mask);

        const __m128i bytes = _mm_packus_epi16(shorts1, shorts2);

        _mm_storeu_si128((__m128i*)&dst[0], bytes);

        i -= 16;
        src += 16;
        dst += 16;
    }

    for ( ; i; --i, ++src, ++dst) {
        const __m128i ints = _mm_castps_si128(_mm_add_ss(_mm_load_ss(&src[0]), offset));
        dst[0] = (Sint8)(_mm_cvtsi128_si32(_mm_packs_epi16(ints, ints)) & 0xFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_U8_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Uint8 *dst = cvt->buf;
    int i = num_samples;

    /* 1) Shift the float range from [-1.0, 1.0] to [98304.0, 98306.0]
     * 2) Extract the lowest 16 bits and clamp to [0, 255]
     * Overflow is correctly handled for inputs between roughly [-254.0, 254.0]
     * dst[i] = clamp(i16(f2i(src[i] + 98305.0) & 0xFFFF), 0, 255) */
    const __m128 offset = _mm_set1_ps(98305.0f);
    const __m128i mask = _mm_set1_epi16(0xFF);

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U8 (using SSE2)");

    cvt->len_cvt /= (unsigned)sizeof(float);

    while (i >= 16) {
        const __m128 floats1 = _mm_loadu_ps(&src[0]);
        const __m128 floats2 = _mm_loadu_ps(&src[4]);
        const __m128 floats3 = _mm_loadu_ps(&src[8]);
        const __m128 floats4 = _mm_loadu_ps(&src[12]);

        const __m128i ints1 = _mm_castps_si128(_mm_add_ps(floats1, offset));
        const __m128i ints2 = _mm_castps_si128(_mm_add_ps(floats2, offset));
        const __m128i ints3 = _mm_castps_si128(_mm_add_ps(floats3, offset));
        const __m128i ints4 = _mm_castps_si128(_mm_add_ps(floats4, offset));

        const __m128i shorts1 = _mm_and_si128(_mm_packus_epi16(ints1, ints2), mask);
        const __m128i shorts2 = _mm_and_si128(_mm_packus_epi16(ints3, ints4), mask);

        const __m128i bytes = _mm_packus_epi16(shorts1, shorts2);

        _mm_storeu_si128((__m128i*)&dst[0], bytes);

        i -= 16;
        src += 16;
        dst += 16;
    }

    for ( ; i; --i, ++src, ++dst) {
        const __m128i ints = _mm_castps_si128(_mm_add_ss(_mm_load_ss(&src[0]), offset));
        dst[0] = (Uint8)(_mm_cvtsi128_si32(_mm_packus_epi16(ints, ints)) & 0xFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_S16_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint16 *dst = (Sint16 *)cvt->buf;
    int i = num_samples;

    /* 1) Shift the float range from [-1.0, 1.0] to [256.0, 258.0]
     * 2) Shift the int range from [0x43800000, 0x43810000] to [-32768,32768]
     * 3) Clamp to range [-32768,32767]
     * Overflow is correctly handled for inputs between roughly [-257.0, +inf)
     * dst[i] = clamp(f2i(src[i] + 257.0) - 0x43808000, -32768, 32767) */
    const __m128 offset = _mm_set1_ps(257.0f);

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S16 (using SSE2)");

    cvt->len_cvt /= 2u;

    while (i >= 16) {
        const __m128 floats1 = _mm_loadu_ps(&src[0]);
        const __m128 floats2 = _mm_loadu_ps(&src[4]);
        const __m128 floats3 = _mm_loadu_ps(&src[8]);
        const __m128 floats4 = _mm_loadu_ps(&src[12]);

        const __m128i ints1 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats1, offset)), _mm_castps_si128(offset));
        const __m128i ints2 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats2, offset)), _mm_castps_si128(offset));
        const __m128i ints3 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats3, offset)), _mm_castps_si128(offset));
        const __m128i ints4 = _mm_sub_epi32(_mm_castps_si128(_mm_add_ps(floats4, offset)), _mm_castps_si128(offset));

        const __m128i shorts1 = _mm_packs_epi32(ints1, ints2);
        const __m128i shorts2 = _mm_packs_epi32(ints3, ints4);

        _mm_storeu_si128((__m128i*)&dst[0], shorts1);
        _mm_storeu_si128((__m128i*)&dst[8], shorts2);

        i -= 16;
        src += 16;
        dst += 16;
    }

    for ( ; i; --i, ++src, ++dst) {
        const __m128i ints = _mm_sub_epi32(_mm_castps_si128(_mm_add_ss(_mm_load_ss(&src[0]), offset)), _mm_castps_si128(offset));
        dst[0] = (Sint16)(_mm_cvtsi128_si32(_mm_packs_epi32(ints, ints)) & 0xFFFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_U16_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Uint16 *dst = (Uint16 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U16 (using SSE2)");

    cvt->len_cvt /= 2u;

    {
        /* Aligned! Do SSE blocks as long as we have 16 bytes available. */
        /* This calculates differently than the scalar path because SSE2 can't
           pack int32 data down to unsigned int16. _mm_packs_epi32 does signed
           saturation, so that would corrupt our data. _mm_packus_epi32 exists,
           but not before SSE 4.1. So we convert from float to sint16, packing
           that down with legit signed saturation, and then xor the top bit
           against 1. This results in the correct unsigned 16-bit value, even
           though it looks like dark magic. */
        const __m128 mulby32767 = _mm_set1_ps(32767.0f);
        const __m128i topbit = _mm_set1_epi16(-32768);
        const __m128 one = _mm_set1_ps(1.0f);
        const __m128 negone = _mm_set1_ps(-1.0f);
        while (i >= 8) {                                                                                                              /* 8 * float32 */
            const __m128i ints1 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_loadu_ps(&src[0])), one), mulby32767)); /* load 4 floats, clamp, convert to sint32 */
            const __m128i ints2 = _mm_cvtps_epi32(_mm_mul_ps(_mm_min_ps(_mm_max_ps(negone, _mm_loadu_ps(&src[4])), one), mulby32767)); /* load 4 floats, clamp, convert to sint32 */
            _mm_storeu_si128((__m128i *)&dst[0], _mm_xor_si128(_mm_packs_epi32(ints1, ints2), topbit));                                /* pack to sint16, xor top bit, store out. */
            i -= 8;
            src += 8;
            dst += 8;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        const float sample = src[0];
        if (sample >= 1.0f) {
            dst[0] = 65535;
        } else if (sample <= -1.0f) {
            dst[0] = 0;
        } else {
            dst[0] = (Uint16)((sample + 1.0f) * 32767.0f);
        }
    }
}

static void SDLCALL SDL_Convert_F32_to_S32_SSE2(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint32 *dst = (Sint32 *)cvt->buf;
    int i = num_samples;

    /* 1) Scale the float range from [-1.0, 1.0] to [-2147483648.0, 2147483648.0]
     * 2) Convert to integer (values too small/large become 0x80000000 = -2147483648)
     * 3) Fixup values which were too large (0x80000000 ^ 0xFFFFFFFF = 2147483647)
     * dst[i] = i32(src[i] * 2147483648.0) ^ ((src[i] >= 2147483648.0) ? 0xFFFFFFFF : 0x00000000) */
    const __m128 limit = _mm_set1_ps(2147483648.0f);

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S32 (using SSE2)");

    while (i >= 16) {
        const __m128 floats1 = _mm_loadu_ps(&src[0]);
        const __m128 floats2 = _mm_loadu_ps(&src[4]);
        const __m128 floats3 = _mm_loadu_ps(&src[8]);
        const __m128 floats4 = _mm_loadu_ps(&src[12]);

        const __m128 values1 = _mm_mul_ps(floats1, limit);
        const __m128 values2 = _mm_mul_ps(floats2, limit);
        const __m128 values3 = _mm_mul_ps(floats3, limit);
        const __m128 values4 = _mm_mul_ps(floats4, limit);

        const __m128i ints1 = _mm_xor_si128(_mm_cvttps_epi32(values1), _mm_castps_si128(_mm_cmpge_ps(values1, limit)));
        const __m128i ints2 = _mm_xor_si128(_mm_cvttps_epi32(values2), _mm_castps_si128(_mm_cmpge_ps(values2, limit)));
        const __m128i ints3 = _mm_xor_si128(_mm_cvttps_epi32(values3), _mm_castps_si128(_mm_cmpge_ps(values3, limit)));
        const __m128i ints4 = _mm_xor_si128(_mm_cvttps_epi32(values4), _mm_castps_si128(_mm_cmpge_ps(values4, limit)));

        _mm_storeu_si128((__m128i*)&dst[0], ints1);
        _mm_storeu_si128((__m128i*)&dst[4], ints2);
        _mm_storeu_si128((__m128i*)&dst[8], ints3);
        _mm_storeu_si128((__m128i*)&dst[12], ints4);

        i -= 16;
        src += 16;
        dst += 16;
    }

    for ( ; i; --i, ++src, ++dst) {
        const __m128 floats = _mm_load_ss(&src[0]);
        const __m128 values = _mm_mul_ss(floats, limit);
        const __m128i ints = _mm_xor_si128(_mm_cvttps_epi32(values), _mm_castps_si128(_mm_cmpge_ss(values, limit)));
        dst[0] = (Sint32)_mm_cvtsi128_si32(ints);
    }
}
#endif

#ifdef HAVE_NEON_INTRINSICS
static void SDLCALL SDL_Convert_S8_to_F32_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt;
    const Sint8 *src = (const Sint8 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 4);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_S8", "AUDIO_F32 (using NEON)");

    cvt->len_cvt *= 4;

    if (!((size_t)cvt->buf & 15)) {
    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for ( ; i && ((size_t)dst & 15); --i) {
        src--;
        dst--;
        {
        union float_bits x;
        x.u32 = (Uint8)src[0] ^ 0x47800080u;
        dst[0] = x.f32 - 65537.0f;
        }
    }

    SDL_assert(!i || !((size_t)dst & 15));
    SDL_assert(!i || !((size_t)src & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby128 = vdupq_n_f32(DIVBY128);
        while (i >= 16) {                                            /* 16 * 8-bit */
            src -= 16;
            dst -= 16;
            {
            const int8x16_t bytes = vld1q_s8(&src[0]);               /* get 16 sint8 into a NEON register. */
            const int16x8_t int16hi = vmovl_s8(vget_high_s8(bytes)); /* convert top 8 bytes to 8 int16 */
            const int16x8_t int16lo = vmovl_s8(vget_low_s8(bytes));  /* convert bottom 8 bytes to 8 int16 */
            /* split int16 to two int32, then convert to float, then multiply to normalize, store. */
            vst1q_f32(&dst[0], vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(int16lo))), divby128));
            vst1q_f32(&dst[4], vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(int16lo))), divby128));
            vst1q_f32(&dst[8], vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(int16hi))), divby128));
            vst1q_f32(&dst[12], vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(int16hi))), divby128));
            }
            i -= 16;
        }
    }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i) {
        src--;
        dst--;
        {
        union float_bits x;
        x.u32 = (Uint8)src[0] ^ 0x47800080u;
        dst[0] = x.f32 - 65537.0f;
        }
    }
}

static void SDLCALL SDL_Convert_U8_to_F32_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt;
    const Uint8 *src = (const Uint8 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 4);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_U8", "AUDIO_F32 (using NEON)");

    cvt->len_cvt *= 4;

    if (!((size_t)cvt->buf & 15)) {
    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for ( ; i && ((size_t)dst & 15); --i) {
        src--;
        dst--;
        {
        union float_bits x;
        x.u32 = src[0] ^ 0x47800000u;
        dst[0] = x.f32 - 65537.0f;
        }
    }

    SDL_assert(!i || !((size_t)dst & 15));
    SDL_assert(!i || !((size_t)src & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby128 = vdupq_n_f32(DIVBY128);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        while (i >= 16) {                                              /* 16 * 8-bit */
            src -= 16;
            dst -= 16;
            {
            const uint8x16_t bytes = vld1q_u8(&src[0]);                /* get 16 uint8 into a NEON register. */
            const uint16x8_t uint16hi = vmovl_u8(vget_high_u8(bytes)); /* convert top 8 bytes to 8 uint16 */
            const uint16x8_t uint16lo = vmovl_u8(vget_low_u8(bytes));  /* convert bottom 8 bytes to 8 uint16 */
            /* split uint16 to two uint32, then convert to float, then multiply to normalize, subtract to adjust for sign, store. */
            vst1q_f32(&dst[0], vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_low_u16(uint16lo))), divby128));
            vst1q_f32(&dst[4], vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_high_u16(uint16lo))), divby128));
            vst1q_f32(&dst[8], vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_low_u16(uint16hi))), divby128));
            vst1q_f32(&dst[12], vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_high_u16(uint16hi))), divby128));
            }
            i -= 16;
        }
    }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i) {
        src--;
        dst--;
        {
        union float_bits x;
        x.u32 = src[0] ^ 0x47800000u;
        dst[0] = x.f32 - 65537.0f;
        }
    }
}

static void SDLCALL SDL_Convert_S16_to_F32_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Sint16);
    const Sint16 *src = (const Sint16 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 2);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_S16", "AUDIO_F32 (using NEON)");

    cvt->len_cvt *= 2;

    if (!((size_t)cvt->buf & 15)) {
    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for ( ; i && ((size_t)dst & 15); --i) {
        src--;
        dst--;
        /* 1) Construct a float in the range [256.0, 258.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        {
        union float_bits x;
        x.u32 = (Uint16)src[0] ^ 0x43808000u;
        dst[0] = x.f32 - 257.0f;
        }
    }

    SDL_assert(!i || !((size_t)dst & 15));
    SDL_assert(!i || !((size_t)src & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby32768 = vdupq_n_f32(DIVBY32768);
        while (i >= 8) {                                            /* 8 * 16-bit */
            src -= 8;
            dst -= 8;
            {
            const int16x8_t ints = vld1q_s16((int16_t const *)&src[0]); /* get 8 sint16 into a NEON register. */
            /* split int16 to two int32, then convert to float, then multiply to normalize, store. */
            vst1q_f32(&dst[0], vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_low_s16(ints))), divby32768));
            vst1q_f32(&dst[4], vmulq_f32(vcvtq_f32_s32(vmovl_s16(vget_high_s16(ints))), divby32768));
            }
            i -= 8;
        }
    }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i) {
        src--;
        dst--;
        /* 1) Construct a float in the range [256.0, 258.0)
         * 2) Shift the float range to [-1.0, 1.0) */
        {
        union float_bits x;
        x.u32 = (Uint16)src[0] ^ 0x43808000u;
        dst[0] = x.f32 - 257.0f;
        }
    }
}

static void SDLCALL SDL_Convert_U16_to_F32_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Uint16);
    const Uint16 *src = (const Uint16 *)(cvt->buf + cvt->len_cvt);
    float *dst = (float *)(cvt->buf + cvt->len_cvt * 2);
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_U16", "AUDIO_F32 (using NEON)");

    cvt->len_cvt *= 2;

    if (!((size_t)cvt->buf & 15)) {
    /* Get dst aligned to 16 bytes (since buffer is growing, we don't have to worry about overreading from src) */
    for ( ; i && ((size_t)dst & 15); --i) {
        src--;
        dst--;
        dst[0] = (((float)src[0]) * DIVBY32768) - 1.0f;
    }

    SDL_assert(!i || !((size_t)dst & 15));
    SDL_assert(!i || !((size_t)src & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby32768 = vdupq_n_f32(DIVBY32768);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        while (i >= 8) {                                               /* 8 * 16-bit */
            src -= 8;
            dst -= 8;
            {
            const uint16x8_t uints = vld1q_u16((uint16_t const *)&src[0]); /* get 8 uint16 into a NEON register. */
            /* split uint16 to two int32, then convert to float, then multiply to normalize, subtract for sign, store. */
            vst1q_f32(&dst[0], vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_low_u16(uints))), divby32768));
            vst1q_f32(&dst[4], vmlaq_f32(negone, vcvtq_f32_u32(vmovl_u16(vget_high_u16(uints))), divby32768));
            }
            i -= 8;
        }
    }
    }
    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i) {
        src--;
        dst--;
        dst[0] = (((float)src[0]) * DIVBY32768) - 1.0f;
    }
}

static void SDLCALL SDL_Convert_S32_to_F32_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(Sint32);
    const Sint32 *src = (const Sint32 *)cvt->buf;
    float *dst = (float *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_S32", "AUDIO_F32 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for ( ; i && ((size_t)dst & 15); --i, ++src, ++dst) {
        dst[0] = (float)src[0] * DIVBY2147483648;
    }

    SDL_assert(!i || !((size_t)dst & 15));
    SDL_assert(!i || !((size_t)src & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby8388607 = vdupq_n_f32(DIVBY8388607);
        while (i >= 4) { /* 4 * sint32 */
            /* shift out lowest bits so int fits in a float32. Small precision loss, but much faster. */
            vst1q_f32(&dst[0], vmulq_f32(vcvtq_f32_s32(vshrq_n_s32(vld1q_s32(&src[0]), 8)), divby8388607));
            i -= 4;
            src += 4;
            dst += 4;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        dst[0] = (float)src[0] * DIVBY2147483648;
    }
}

static void SDLCALL SDL_Convert_F32_to_S8_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint8 *dst = (Sint8 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S8 (using NEON)");

    cvt->len_cvt /= (unsigned)sizeof(float);

    /* Make sure src/dst are aligned to 16 bytes. */
    if (!((size_t)src & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby127 = vdupq_n_f32(127.0f);
        while (i >= 16) {                                                                                                       /* 16 * float32 */
            const int32x4_t ints1 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[0])), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints2 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[4])), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints3 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[8])), one), mulby127));  /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints4 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[12])), one), mulby127)); /* load 4 floats, clamp, convert to sint32 */
            const int8x8_t i8lo = vmovn_s16(vcombine_s16(vmovn_s32(ints1), vmovn_s32(ints2)));                                  /* narrow to sint16, combine, narrow to sint8 */
            const int8x8_t i8hi = vmovn_s16(vcombine_s16(vmovn_s32(ints3), vmovn_s32(ints4)));                                  /* narrow to sint16, combine, narrow to sint8 */
            vst1q_s8(&dst[0], vcombine_s8(i8lo, i8hi));                                                                         /* combine to int8x16_t, store out */
            i -= 16;
            src += 16;
            dst += 16;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
         * 2) Shift the integer range from [0x47BFFF80, 0x47C00080] to [-128, 128]
         * 3) Clamp the value to [-128, 127] */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0] + 98304.0f;

        y = x.u32 - 0x47C00000u;
        z = 0x7Fu - (y ^ SIGNMASK(y));
        y = y ^ (z & SIGNMASK(z));

        dst[0] = (Sint8)(y & 0xFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_U8_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Uint8 *dst = cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U8 (using NEON)");

    cvt->len_cvt /= (unsigned)sizeof(float);

    /* Make sure src/dst are aligned to 16 bytes. */
    if (!((size_t)src & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby127 = vdupq_n_f32(127.0f);
        while (i >= 16) {                                                                                                                         /* 16 * float32 */
            const uint32x4_t uints1 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[0])), one), one), mulby127));  /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints2 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[4])), one), one), mulby127));  /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints3 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[8])), one), one), mulby127));  /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints4 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[12])), one), one), mulby127)); /* load 4 floats, clamp, convert to uint32 */
            const uint8x8_t ui8lo = vmovn_u16(vcombine_u16(vmovn_u32(uints1), vmovn_u32(uints2)));                                                /* narrow to uint16, combine, narrow to uint8 */
            const uint8x8_t ui8hi = vmovn_u16(vcombine_u16(vmovn_u32(uints3), vmovn_u32(uints4)));                                                /* narrow to uint16, combine, narrow to uint8 */
            vst1q_u8(&dst[0], vcombine_u8(ui8lo, ui8hi));                                                                                         /* combine to uint8x16_t, store out */
            i -= 16;
            src += 16;
            dst += 16;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [98303.0, 98305.0]
         * 2) Shift the integer range from [0x47BFFF80, 0x47C00080] to [-128, 128]
         * 3) Clamp the value to [-128, 127]
         * 4) Shift the integer range from [-128, 127] to [0, 255] */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0] + 98304.0f;

        y = x.u32 - 0x47C00000u;
        z = 0x7Fu - (y ^ SIGNMASK(y));
        y = (y ^ 0x80u) ^ (z & SIGNMASK(z));

        dst[0] = (Uint8)(y & 0xFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_S16_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint16 *dst = (Sint16 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S16 (using NEON)");

    cvt->len_cvt /= 2u;

    /* Make sure src/dst are aligned to 16 bytes. */
    if (!((size_t)src & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby32767 = vdupq_n_f32(32767.0f);
        while (i >= 8) {                                                                                                         /* 8 * float32 */
            const int32x4_t ints1 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[0])), one), mulby32767)); /* load 4 floats, clamp, convert to sint32 */
            const int32x4_t ints2 = vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[4])), one), mulby32767)); /* load 4 floats, clamp, convert to sint32 */
            vst1q_s16(&dst[0], vcombine_s16(vmovn_s32(ints1), vmovn_s32(ints2)));                                              /* narrow to sint16, combine, store out. */
            i -= 8;
            src += 8;
            dst += 8;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [383.0, 385.0]
         * 2) Shift the integer range from [0x43BF8000, 0x43C08000] to [-32768, 32768]
         * 3) Clamp values outside the [-32768, 32767] range */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0] + 384.0f;

        y = x.u32 - 0x43C00000u;
        z = 0x7FFFu - (y ^ SIGNMASK(y));
        y = y ^ (z & SIGNMASK(z));

        dst[0] = (Sint16)(y & 0xFFFF);
    }
}

static void SDLCALL SDL_Convert_F32_to_U16_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Uint16 *dst = (Uint16 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_U16 (using NEON)");

    cvt->len_cvt /= 2u;

    /* Make sure src/dst are aligned to 16 bytes. */
    if (!((size_t)src & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby32767 = vdupq_n_f32(32767.0f);
        while (i >= 8) {                                                                                                                           /* 8 * float32 */
            const uint32x4_t uints1 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[0])), one), one), mulby32767));     /* load 4 floats, clamp, convert to uint32 */
            const uint32x4_t uints2 = vcvtq_u32_f32(vmulq_f32(vaddq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[4])), one), one), mulby32767)); /* load 4 floats, clamp, convert to uint32 */
            vst1q_u16(&dst[0], vcombine_u16(vmovn_u32(uints1), vmovn_u32(uints2)));                                                                  /* narrow to uint16, combine, store out. */
            i -= 8;
            src += 8;
            dst += 8;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        const float sample = src[0];
        if (sample >= 1.0f) {
            dst[0] = 65535;
        } else if (sample <= -1.0f) {
            dst[0] = 0;
        } else {
            dst[0] = (Uint16)((sample + 1.0f) * 32767.0f);
        }
    }
}

static void SDLCALL SDL_Convert_F32_to_S32_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = (unsigned)cvt->len_cvt / sizeof(float);
    const float *src = (const float *)cvt->buf;
    Sint32 *dst = (Sint32 *)cvt->buf;
    int i = num_samples;

    LOG_DEBUG_CONVERT("AUDIO_F32", "AUDIO_S32 (using NEON)");

    /* Get dst aligned to 16 bytes */
    for ( ; i && ((size_t)dst & 15); --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [-2147483648.0, 2147483648.0]
         * 2) Set values outside the [-2147483648.0, 2147483647.0] range to -2147483648.0
         * 3) Convert the float to an integer, and fixup values outside the valid range */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0];

        y = x.u32 + 0x0F800000u;
        z = y - 0xCF000000u;
        z &= SIGNMASK(y ^ z);
        x.u32 = y - z;

        dst[0] = (Sint32)x.f32 ^ (Sint32)SIGNMASK(z);
    }

    SDL_assert(!i || !((size_t)dst & 15));
    SDL_assert(!i || !((size_t)src & 15));

    {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t one = vdupq_n_f32(1.0f);
        const float32x4_t negone = vdupq_n_f32(-1.0f);
        const float32x4_t mulby8388607 = vdupq_n_f32(8388607.0f);
        while (i >= 4) { /* 4 * float32 */
            vst1q_s32(&dst[0], vshlq_n_s32(vcvtq_s32_f32(vmulq_f32(vminq_f32(vmaxq_f32(negone, vld1q_f32(&src[0])), one), mulby8388607)), 8));
            i -= 4;
            src += 4;
            dst += 4;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; --i, ++src, ++dst) {
        /* 1) Shift the float range from [-1.0, 1.0] to [-2147483648.0, 2147483648.0]
         * 2) Set values outside the [-2147483648.0, 2147483647.0] range to -2147483648.0
         * 3) Convert the float to an integer, and fixup values outside the valid range */
        union float_bits x;
        Uint32 y, z;
        x.f32 = src[0];

        y = x.u32 + 0x0F800000u;
        z = y - 0xCF000000u;
        z &= SIGNMASK(y ^ z);
        x.u32 = y - z;

        dst[0] = (Sint32)x.f32 ^ (Sint32)SIGNMASK(z);
    }
}
#endif

void SDL_ChooseAudioConverters(void)
{
    if (SDL_Convert_S8_to_F32) {
        return; // converters chosen
    }

    SDL_ChooseAudioChannelConverters();
    SDL_ChooseAudioResamplers();

    SDL_Convert_Byteswap16 = SDL_Convert_Byteswap16_Scalar;
    SDL_Convert_Byteswap32 = SDL_Convert_Byteswap32_Scalar;

#define SET_CONVERTER_FUNCS(fntype)                           \
    SDL_Convert_S8_to_F32 = SDL_Convert_S8_to_F32_##fntype;   \
    SDL_Convert_U8_to_F32 = SDL_Convert_U8_to_F32_##fntype;   \
    SDL_Convert_S16_to_F32 = SDL_Convert_S16_to_F32_##fntype; \
    SDL_Convert_U16_to_F32 = SDL_Convert_U16_to_F32_##fntype; \
    SDL_Convert_S32_to_F32 = SDL_Convert_S32_to_F32_##fntype; \
    SDL_Convert_F32_to_S8 = SDL_Convert_F32_to_S8_##fntype;   \
    SDL_Convert_F32_to_U8 = SDL_Convert_F32_to_U8_##fntype;   \
    SDL_Convert_F32_to_S16 = SDL_Convert_F32_to_S16_##fntype; \
    SDL_Convert_F32_to_U16 = SDL_Convert_F32_to_U16_##fntype; \
    SDL_Convert_F32_to_S32 = SDL_Convert_F32_to_S32_##fntype; \

#ifdef HAVE_SSE2_INTRINSICS
#if HAVE_SSE2_SUPPORT
    SDL_assert(SDL_HasSSE2());
    if (1) {
#else
    if (SDL_HasSSE2()) {
#endif
        SET_CONVERTER_FUNCS(SSE2);
        return;
    }
#endif

#ifdef HAVE_NEON_INTRINSICS
#if HAVE_NEON_SUPPORT
    SDL_assert(SDL_HasNEON());
    if (1) {
#else
    if (SDL_HasNEON()) {
#endif
        SET_CONVERTER_FUNCS(NEON);
        return;
    }
#endif

    SET_CONVERTER_FUNCS(Scalar);

#undef SET_CONVERTER_FUNCS

    SDL_assert(SDL_Convert_S8_to_F32 != NULL);
}

/* vi: set ts=4 sw=4 expandtab: */
