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

#include "SDL_video.h"

#include "SDL_blit.h"
#include "SDL_surface_c.h"

#ifdef __SSE2__
#define HAVE_SSE2_INTRINSICS
#endif

#ifdef __ARM_NEON
#define HAVE_NEON_INTRINSICS 1
#define CAST_uint8x8_t       (uint8x8_t)
#define CAST_uint32x2_t      (uint32x2_t)
#endif

#if defined(__WINRT__) || defined(_MSC_VER)
#ifdef HAVE_NEON_INTRINSICS
#undef CAST_uint8x8_t
#undef CAST_uint32x2_t
#define CAST_uint8x8_t
#define CAST_uint32x2_t
#endif
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

/* Set to zero if platform is guaranteed to use a SIMD codepath here. */
#ifndef HAVE_SSE2_SUPPORT
#define HAVE_SSE2_SUPPORT 0
#endif
#ifndef HAVE_NEON_SUPPORT
#define HAVE_NEON_SUPPORT 0
#endif

#define SDL_STRETCH_LIMIT SDL_MAX_UINT16

static void SDL_LowerSoftStretchNearest(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
static void SDL_LowerSoftStretchLinear(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect);
static int SDL_UpperSoftStretch(SDL_Surface *src, const SDL_Rect *srcrect, SDL_Surface *dst, const SDL_Rect *dstrect, SDL_ScaleMode scaleMode);

/* Function pointer set to a CPU-specific implementation. */
static void (*SDL_Scale_linear)(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch, Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip) = NULL;

int SDL_SoftStretch(SDL_Surface *src, const SDL_Rect *srcrect,
                    SDL_Surface *dst, const SDL_Rect *dstrect)
{
    return SDL_UpperSoftStretch(src, srcrect, dst, dstrect, SDL_ScaleModeNearest);
}

int SDL_SoftStretchLinear(SDL_Surface *src, const SDL_Rect *srcrect,
                          SDL_Surface *dst, const SDL_Rect *dstrect)
{
    return SDL_UpperSoftStretch(src, srcrect, dst, dstrect, SDL_ScaleModeLinear);
}

static int SDL_UpperSoftStretch(SDL_Surface *src, const SDL_Rect *srcrect,
                                SDL_Surface *dst, const SDL_Rect *dstrect, SDL_ScaleMode scaleMode)
{
    Uint32 format = src->format->format;
    int src_locked;
    int dst_locked;
    SDL_Rect full_src;
    SDL_Rect full_dst;

    if (format != dst->format->format) {
        return SDL_SetError("Only works with same format surfaces");
    }

    if (scaleMode != SDL_ScaleModeNearest) {
        if (src->format->BytesPerPixel != 4 || format == SDL_PIXELFORMAT_ARGB2101010) {
            return SDL_SetError("Wrong format");
        }
    }

    /* Verify the blit rectangles */
    if (srcrect) {
        if ((srcrect->x < 0) || (srcrect->y < 0) ||
            ((srcrect->x + srcrect->w) > src->w) ||
            ((srcrect->y + srcrect->h) > src->h)) {
            return SDL_SetError("Invalid source blit rectangle");
        }
    } else {
        full_src.x = 0;
        full_src.y = 0;
        full_src.w = src->w;
        full_src.h = src->h;
        srcrect = &full_src;
    }
    if (dstrect) {
        if ((dstrect->x < 0) || (dstrect->y < 0) ||
            ((dstrect->x + dstrect->w) > dst->w) ||
            ((dstrect->y + dstrect->h) > dst->h)) {
            return SDL_SetError("Invalid destination blit rectangle");
        }
    } else {
        full_dst.x = 0;
        full_dst.y = 0;
        full_dst.w = dst->w;
        full_dst.h = dst->h;
        dstrect = &full_dst;
    }

    // if (SDL_RectEmpty(srcrect) || SDL_RectEmpty(dstrect)) {
    if ((srcrect->w <= 0 || srcrect->h <= 0) || (dstrect->w <= 0 || dstrect->h <= 0)) {
        /* No-op */
        return 0;
    }

    if (srcrect->w > SDL_STRETCH_LIMIT || srcrect->h > SDL_STRETCH_LIMIT ||
        dstrect->w > SDL_STRETCH_LIMIT || dstrect->h > SDL_STRETCH_LIMIT) {
        return SDL_SetError("Size too large for scaling");
    }

    /* Lock the destination if it's in hardware */
    dst_locked = 0;
    if (SDL_MUSTLOCK(dst)) {
        if (SDL_LockSurface(dst) < 0) {
            return SDL_SetError("Unable to lock destination surface");
        }
        dst_locked = 1;
    }
    /* Lock the source if it's in hardware */
    src_locked = 0;
    if (SDL_MUSTLOCK(src)) {
        if (SDL_LockSurface(src) < 0) {
            if (dst_locked) {
                SDL_UnlockSurface(dst);
            }
            return SDL_SetError("Unable to lock source surface");
        }
        src_locked = 1;
    }

    if (scaleMode == SDL_ScaleModeNearest) {
        SDL_LowerSoftStretchNearest(src, srcrect, dst, dstrect);
    } else {
        SDL_LowerSoftStretchLinear(src, srcrect, dst, dstrect);
    }

    /* We need to unlock the surfaces if they're locked */
    if (dst_locked) {
        SDL_UnlockSurface(dst);
    }
    if (src_locked) {
        SDL_UnlockSurface(src);
    }

    return 0;
}

/* bilinear interpolation precision must be < 8
   Because with SSE: add-multiply: _mm_madd_epi16 works with signed int
   so pixels 0xb1...... are negatives and false the result
   same in NEON probably */
#define PRECISION 7

#define FIXED_POINT(i) ((Uint32)(i) << 16)
#define SRC_INDEX(fp)  ((Uint32)(fp) >> 16)
#define INTEGER(fp)    ((Uint32)(fp) >> PRECISION)
#define FRAC(fp)       ((Uint32)(fp >> (16 - PRECISION)) & ((1 << PRECISION) - 1))
#define FRAC_ZERO      0
#define FRAC_ONE       (1 << PRECISION)
#define FP_ONE         FIXED_POINT(1)

SDL_COMPILE_TIME_ASSERT(stretch_limit, SRC_INDEX(FIXED_POINT(SDL_STRETCH_LIMIT)) == SDL_STRETCH_LIMIT);
SDL_COMPILE_TIME_ASSERT(stretch_limit_round, FP_ONE > SDL_STRETCH_LIMIT);
#define BILINEAR___START             \
    Uint32 *dst = (Uint32 *)dst_ptr; \
    Uint32 posx, posy;               \
    Uint32 incx, incy;               \
    Uint32 columns;                  \
    Uint32 rows = dst_h;             \
    incx = dst_w > 1 ? FIXED_POINT(src_w - 1) / (dst_w - 1) : 0; \
    incy = dst_h > 1 ? FIXED_POINT(src_h - 1) / (dst_h - 1) : 0; \
    incx += src_w > 1 && src_w < SDL_STRETCH_LIMIT ? 1 : 0; \
    incy += src_h > 1 && src_h < SDL_STRETCH_LIMIT ? 1 : 0; \
    posy = 0;

#define BILINEAR___HEIGHT                                         \
    int frac_h0;                                                  \
    const Uint32 *src_h0, *src_h1;                                \
    int incr_h0;                                                  \
    frac_h0 = FRAC(posy);                                         \
    incr_h0 = SRC_INDEX(posy) * src_pitch;                        \
    src_h0 = (const Uint32 *)(src_ptr + incr_h0);                 \
    src_h1 = (const Uint32 *)((const Uint8 *)src_h0 + src_pitch); \
    posy += incy;                                                 \
    posx = 0;                                                     \
    columns = dst_w;

typedef struct color_t
{
    Uint8 a;
    Uint8 b;
    Uint8 c;
    Uint8 d;
} color_t;

/* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */

static SDL_INLINE void INTERPOL(const Uint32 *src_x0, const Uint32 *src_x1, int frac0, Uint32 *dst)
{
    const color_t *c0 = (const color_t *)src_x0;
    const color_t *c1 = (const color_t *)src_x1;
    color_t *cx = (color_t *)dst;
#if 1
    cx->a = c0->a + INTEGER(frac0 * (c1->a - c0->a));
    cx->b = c0->b + INTEGER(frac0 * (c1->b - c0->b));
    cx->c = c0->c + INTEGER(frac0 * (c1->c - c0->c));
    cx->d = c0->d + INTEGER(frac0 * (c1->d - c0->d));
#else
    unsigned int frac1 = FRAC_ONE - frac0;
    cx->a = INTEGER(frac1 * c0->a + frac0 * c1->a);
    cx->b = INTEGER(frac1 * c0->b + frac0 * c1->b);
    cx->c = INTEGER(frac1 * c0->c + frac0 * c1->c);
    cx->d = INTEGER(frac1 * c0->d + frac0 * c1->d);
#endif
}

static SDL_INLINE void INTERPOL_BILINEAR(const Uint32 *s0, const Uint32 *s1, int frac_w, int frac_h, Uint32 *dst)
{
    Uint32 tmp[2];

    /* Vertical first, store to 'tmp' */
    INTERPOL(s0, s1, frac_h, tmp);
    INTERPOL(s0 + 1, s1 + 1, frac_h, tmp + 1);

    /* Horizontal, store to 'dst' */
    INTERPOL(tmp, tmp + 1, frac_w, dst);
}

static void scale_mat(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch, Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    BILINEAR___START

    // SDL_assert(rows != 0 && columns != 0);

    while (1) {

        BILINEAR___HEIGHT

        if (--rows) {
            while (1) {
                const Uint32 *s_00_01;
                const Uint32 *s_10_11;
                int index_w = 4 * SRC_INDEX(posx);
                int frac_w = FRAC(posx);
                posx += incx;

                s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
                s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);

                if (--columns) {
                    /*
                        x00 ... x0_ ..... x01
                        .       .         .
                        .       x         .
                        .       .         .
                        .       .         .
                        x10 ... x1_ ..... x11
                    */
                    INTERPOL_BILINEAR(s_00_01, s_10_11, frac_w, frac_h0, dst);
                    dst += 1;
                } else {
                    /* last column
                        x00
                        .
                        x
                        .
                        .
                        x10
                    */
                    INTERPOL(&s_00_01[0], &s_10_11[0], frac_h0, dst);
                    dst += 1;
                    break;
                }
            }
            dst = (Uint32 *)((Uint8 *)dst + dst_skip);
            continue;
        }
        /* last row
                        x00 ... x ..... x01
        */
        while (1) {
            const Uint32 *s_00_01;
            int index_w = 4 * SRC_INDEX(posx);
            int frac_w = FRAC(posx);
            posx += incx;

            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);

            if (--columns) {
                INTERPOL(&s_00_01[0], &s_00_01[1], frac_w, dst);
                dst += 1;
            } else {
                /* last column and row
                        x00
                */
                *dst = s_00_01[0];
                break;
            }
        }
        break;
    }
}

#ifdef HAVE_SSE2_INTRINSICS

/* Interpolate between colors at s0 and s1 using frac_w and store the result to dst */
static SDL_INLINE void INTERPOL_SSE2(const Uint32 *s0, const Uint32 *s1, const __m128i v_frac_w0, const __m128i v_frac_w1, Uint32 *dst, const __m128i zero)
{
    __m128i x_00_01, x_10_11; /* Pixels in 4*uint8 in row */
    __m128i k0, l0, d0, e0;

    x_00_01 = _mm_cvtsi32_si128(*s0); /* Load x00 and x01 */
    x_10_11 = _mm_cvtsi32_si128(*s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */

    /* Interpolation */
    k0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_00_01, zero), v_frac_w1);
    l0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_10_11, zero), v_frac_w0);
    k0 = _mm_add_epi16(k0, l0);

    /* FRAC to INTEGER */
    d0 = _mm_srli_epi16(k0, PRECISION);

    /* Store 1 pixel */
    e0 = _mm_packus_epi16(d0, d0);
    *dst = _mm_cvtsi128_si32(e0);
}

/* Interpolate between color pairs at s0 and s1 using frac_w0 and frac_w1 and store the result to dst */
static SDL_INLINE void INTERPOLx2_SSE2(const Uint32 *s0, const Uint32 *s1, int frac_w0, int frac_w1, Uint32 *dst, const __m128i zero)
{
    __m128i x_00_01, x_10_11; /* (Pixels in 4*uint8 in row) * 2 */
    __m128i v_frac_w0, v_frac_w1, k0, l0, d0, e0;

    int f00, f01, f10, f11;
    f00 = frac_w0;
    f01 = FRAC_ONE - frac_w0;

    f10 = frac_w1;
    f11 = FRAC_ONE - frac_w1;

    v_frac_w0 = _mm_set_epi16(f10, f10, f10, f10, f00, f00, f00, f00);
    v_frac_w1 = _mm_set_epi16(f11, f11, f11, f11, f01, f01, f01, f01);

    x_00_01 = _mm_loadl_epi64((const __m128i *)s0); /* Load s0[0],s0[1] and s1[0],s1[1] */
    x_10_11 = _mm_loadl_epi64((const __m128i *)s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */

    /* Interpolation */
    k0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_00_01, zero), v_frac_w1);
    l0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_10_11, zero), v_frac_w0);
    k0 = _mm_add_epi16(k0, l0);

    /* FRAC to INTEGER */
    d0 = _mm_srli_epi16(k0, PRECISION);

    /* Store 2 pixels */
    e0 = _mm_packus_epi16(d0, d0);
    _mm_storeu_si64((Uint64 *)dst, e0);
}

static SDL_INLINE void INTERPOL_BILINEAR_SSE2(const Uint32 *s0, const Uint32 *s1, int frac_w, const __m128i v_frac_h0, const __m128i v_frac_h1, Uint32 *dst, const __m128i zero)
{
    __m128i x_00_01, x_10_11; /* Pixels in 4*uint8 in row */
    __m128i v_frac_w0, k0, l0, d0, e0;

    int f, f2;
    f = frac_w;
    f2 = FRAC_ONE - frac_w;
    v_frac_w0 = _mm_set_epi16(f, f2, f, f2, f, f2, f, f2);

    x_00_01 = _mm_loadl_epi64((const __m128i *)s0); /* Load x00 and x01 */
    x_10_11 = _mm_loadl_epi64((const __m128i *)s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */

    /* Interpolation vertical */
    k0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_00_01, zero), v_frac_h1);
    l0 = _mm_mullo_epi16(_mm_unpacklo_epi8(x_10_11, zero), v_frac_h0);
    k0 = _mm_add_epi16(k0, l0);

    /* For perfect match, clear the factionnal part eventually. */
    /*
    k0 = _mm_srli_epi16(k0, PRECISION);
    k0 = _mm_slli_epi16(k0, PRECISION);
    */

    /* Interpolation horizontal */
    l0 = _mm_unpacklo_epi64(/* unused */ l0, k0);
    k0 = _mm_madd_epi16(_mm_unpackhi_epi16(l0, k0), v_frac_w0);

    /* FRAC to INTEGER */
    d0 = _mm_srli_epi32(k0, PRECISION * 2);

    /* Store 1 pixel */
    e0 = _mm_packs_epi32(d0, d0);
    e0 = _mm_packus_epi16(e0, e0);
    *dst = _mm_cvtsi128_si32(e0);
}

static void scale_mat_SSE2(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch, Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    BILINEAR___START

    // SDL_assert(rows != 0 && columns != 0);

    while (1) {
        int f, f2;
        __m128i v_frac_h0;
        __m128i v_frac_h1;
        const __m128i zero = _mm_setzero_si128();

        BILINEAR___HEIGHT

        if (--rows) {
            f = frac_h0;
            f2 = FRAC_ONE - frac_h0;
            v_frac_h0 = _mm_set1_epi16(f);
            v_frac_h1 = _mm_set1_epi16(f2);

            while (1) {
                const Uint32 *s_00_01;
                const Uint32 *s_10_11;
                int index_w = 4 * SRC_INDEX(posx);
                int frac_w = FRAC(posx);
                posx += incx;

                s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
                s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);

                if (--columns) {
                    /*
                        x00 ... x0_ ..... x01
                        .       .         .
                        .       x         .
                        .       .         .
                        .       .         .
                        x10 ... x1_ ..... x11
                    */
                    INTERPOL_BILINEAR_SSE2(s_00_01, s_10_11, frac_w, v_frac_h0, v_frac_h1, dst, zero);
                    dst += 1;
                } else {
                    /* last column
                        x00
                        .
                        x
                        .
                        .
                        x10
                    */
                    INTERPOL_SSE2(&s_00_01[0], &s_10_11[0], v_frac_h0, v_frac_h1, dst, zero);
                    dst += 1;
                    break;
                }
            }

            dst = (Uint32 *)((Uint8 *)dst + dst_skip);
        } else {
            /* last row
                        x00 ... x ..... x01
            */
            int nb_block2 = (columns - 1) >> 1;
            columns -= nb_block2 * 2;

            while (nb_block2--) {
                const Uint32 *s0_00_01, *s1_00_01;
                int frac_w0, frac_w1;
                int index_w;

                index_w = 4 * SRC_INDEX(posx);
                frac_w0 = FRAC(posx);
                posx += incx;

                s0_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);

                index_w = 4 * SRC_INDEX(posx);
                frac_w1 = FRAC(posx);
                posx += incx;

                s1_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);

                INTERPOLx2_SSE2(&s0_00_01[0], &s1_00_01[0], frac_w0, frac_w1, dst, zero);
                dst += 2;
            }

            while (1) {
                const Uint32 *s_00_01;
                int index_w = 4 * SRC_INDEX(posx);
                int frac_w = FRAC(posx);
                __m128i v_frac_w0, v_frac_w1;
                posx += incx;

                s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);

                v_frac_w0 = _mm_set1_epi16(frac_w);
                v_frac_w1 = _mm_set1_epi16(FRAC_ONE - frac_w);

                if (--columns) {
                    INTERPOL_SSE2(&s_00_01[0], &s_00_01[1], v_frac_w0, v_frac_w1, dst, zero);
                    dst += 1;
                } else {
                    /* last column and row
                            x00
                    */
                    *dst = s_00_01[0];
                    break;
                }
            }
            break;
        }
    }
}
#endif // HAVE_SSE2_INTRINSICS

#ifdef HAVE_NEON_INTRINSICS

static SDL_INLINE void INTERPOL_NEON(const Uint32 *s0, const Uint32 *s1, uint8x8_t v_frac_w0, uint8x8_t v_frac_w1, Uint32 *dst)
{
    uint8x8_t x_00_01, x_10_11; /* Pixels in 4*uint8 in row */
    uint16x8_t k0;
    uint8x8_t e0;

    x_00_01 = CAST_uint8x8_t vld1_u32(s0); /* Load 2 pixels */
    x_10_11 = CAST_uint8x8_t vld1_u32(s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */
    k0 = vmull_u8(x_00_01, v_frac_w1);     /* k0 := x0 * (1 - frac)    */
    k0 = vmlal_u8(k0, x_10_11, v_frac_w0); /* k0 += x1 * frac          */

    /* FRAC to INTEGER and narrow */
    e0 = vshrn_n_u16(k0, PRECISION);

    /* Store 1 pixel */
    *dst = vget_lane_u32(CAST_uint32x2_t e0, 0);
}

static SDL_INLINE void INTERPOL_BILINEAR_NEON(const Uint32 *s0, const Uint32 *s1, int frac_w, uint8x8_t v_frac_h0, uint8x8_t v_frac_h1, Uint32 *dst)
{
    uint8x8_t x_00_01, x_10_11; /* Pixels in 4*uint8 in row */
    uint16x8_t k0;
    uint32x4_t l0;
    uint16x8_t d0;
    uint8x8_t e0;

    x_00_01 = CAST_uint8x8_t vld1_u32(s0); /* Load 2 pixels */
    x_10_11 = CAST_uint8x8_t vld1_u32(s1);

    /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */
    k0 = vmull_u8(x_00_01, v_frac_h1);     /* k0 := x0 * (1 - frac)    */
    k0 = vmlal_u8(k0, x_10_11, v_frac_h0); /* k0 += x1 * frac          */

    /* k0 now contains 2 interpolated pixels { j0, j1 } */
    l0 = vshll_n_u16(vget_low_u16(k0), PRECISION);
    l0 = vmlsl_n_u16(l0, vget_low_u16(k0), frac_w);
    l0 = vmlal_n_u16(l0, vget_high_u16(k0), frac_w);

    /* Shift and narrow */
    d0 = vcombine_u16(
        /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION),
        /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION));

    /* Narrow again */
    e0 = vmovn_u16(d0);

    /* Store 1 pixel */
    *dst = vget_lane_u32(CAST_uint32x2_t e0, 0);
}

static void scale_mat_NEON(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch, Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    int nb4;

    BILINEAR___START

    // SDL_assert(rows != 0 && columns != 0);
    nb4 = (dst_w - 1) >> 2;

    while (1) {
        int f, f2;
        uint8x8_t v_frac_h0, v_frac_h1;

        BILINEAR___HEIGHT

        if (--rows) {
            int nb_block4 = nb4;
            columns -= nb_block4 * 4;
            f = frac_h0;
            f2 = FRAC_ONE - frac_h0;
            v_frac_h0 = vmov_n_u8(f);
            v_frac_h1 = vmov_n_u8(f2);

        while (nb_block4--) {
            int index_w_0, frac_w_0;
            int index_w_1, frac_w_1;
            int index_w_2, frac_w_2;
            int index_w_3, frac_w_3;

            const Uint32 *s_00_01, *s_02_03, *s_04_05, *s_06_07;
            const Uint32 *s_10_11, *s_12_13, *s_14_15, *s_16_17;

            uint8x8_t x_00_01, x_10_11, x_02_03, x_12_13; /* Pixels in 4*uint8 in row */
            uint8x8_t x_04_05, x_14_15, x_06_07, x_16_17;

            uint16x8_t k0, k1, k2, k3;
            uint32x4_t l0, l1, l2, l3;
            uint16x8_t d0, d1;
            uint8x8_t e0, e1;
            uint32x4_t f0;

            index_w_0 = 4 * SRC_INDEX(posx);
            frac_w_0 = FRAC(posx);
            posx += incx;
            index_w_1 = 4 * SRC_INDEX(posx);
            frac_w_1 = FRAC(posx);
            posx += incx;
            index_w_2 = 4 * SRC_INDEX(posx);
            frac_w_2 = FRAC(posx);
            posx += incx;
            index_w_3 = 4 * SRC_INDEX(posx);
            frac_w_3 = FRAC(posx);
            posx += incx;

            s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_0);
            s_02_03 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_1);
            s_04_05 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_2);
            s_06_07 = (const Uint32 *)((const Uint8 *)src_h0 + index_w_3);
            s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_0);
            s_12_13 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_1);
            s_14_15 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_2);
            s_16_17 = (const Uint32 *)((const Uint8 *)src_h1 + index_w_3);

            /* Interpolation vertical */
            x_00_01 = CAST_uint8x8_t vld1_u32(s_00_01); /* Load 2 pixels */
            x_02_03 = CAST_uint8x8_t vld1_u32(s_02_03);
            x_04_05 = CAST_uint8x8_t vld1_u32(s_04_05);
            x_06_07 = CAST_uint8x8_t vld1_u32(s_06_07);
            x_10_11 = CAST_uint8x8_t vld1_u32(s_10_11);
            x_12_13 = CAST_uint8x8_t vld1_u32(s_12_13);
            x_14_15 = CAST_uint8x8_t vld1_u32(s_14_15);
            x_16_17 = CAST_uint8x8_t vld1_u32(s_16_17);

            /* Interpolated == x0 + frac * (x1 - x0) == x0 * (1 - frac) + x1 * frac */
            k0 = vmull_u8(x_00_01, v_frac_h1);     /* k0 := x0 * (1 - frac)    */
            k0 = vmlal_u8(k0, x_10_11, v_frac_h0); /* k0 += x1 * frac          */

            k1 = vmull_u8(x_02_03, v_frac_h1);
            k1 = vmlal_u8(k1, x_12_13, v_frac_h0);

            k2 = vmull_u8(x_04_05, v_frac_h1);
            k2 = vmlal_u8(k2, x_14_15, v_frac_h0);

            k3 = vmull_u8(x_06_07, v_frac_h1);
            k3 = vmlal_u8(k3, x_16_17, v_frac_h0);

            /* k0 now contains 2 interpolated pixels { j0, j1 } */
            /* k1 now contains 2 interpolated pixels { j2, j3 } */
            /* k2 now contains 2 interpolated pixels { j4, j5 } */
            /* k3 now contains 2 interpolated pixels { j6, j7 } */

            l0 = vshll_n_u16(vget_low_u16(k0), PRECISION);
            l0 = vmlsl_n_u16(l0, vget_low_u16(k0), frac_w_0);
            l0 = vmlal_n_u16(l0, vget_high_u16(k0), frac_w_0);

            l1 = vshll_n_u16(vget_low_u16(k1), PRECISION);
            l1 = vmlsl_n_u16(l1, vget_low_u16(k1), frac_w_1);
            l1 = vmlal_n_u16(l1, vget_high_u16(k1), frac_w_1);

            l2 = vshll_n_u16(vget_low_u16(k2), PRECISION);
            l2 = vmlsl_n_u16(l2, vget_low_u16(k2), frac_w_2);
            l2 = vmlal_n_u16(l2, vget_high_u16(k2), frac_w_2);

            l3 = vshll_n_u16(vget_low_u16(k3), PRECISION);
            l3 = vmlsl_n_u16(l3, vget_low_u16(k3), frac_w_3);
            l3 = vmlal_n_u16(l3, vget_high_u16(k3), frac_w_3);

            /* shift and narrow */
            d0 = vcombine_u16(
                /* uint16x4_t */ vshrn_n_u32(l0, 2 * PRECISION),
                /* uint16x4_t */ vshrn_n_u32(l1, 2 * PRECISION));
            /* narrow again */
            e0 = vmovn_u16(d0);

            /* Shift and narrow */
            d1 = vcombine_u16(
                /* uint16x4_t */ vshrn_n_u32(l2, 2 * PRECISION),
                /* uint16x4_t */ vshrn_n_u32(l3, 2 * PRECISION));
            /* Narrow again */
            e1 = vmovn_u16(d1);

            f0 = vcombine_u32(CAST_uint32x2_t e0, CAST_uint32x2_t e1);
            /* Store 4 pixels */
            vst1q_u32(dst, f0);

            dst += 4;
        }

            while (1) {
                const Uint32 *s_00_01;
                const Uint32 *s_10_11;
                int index_w = 4 * SRC_INDEX(posx);
                int frac_w = FRAC(posx);
                posx += incx;

                s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);
                s_10_11 = (const Uint32 *)((const Uint8 *)src_h1 + index_w);

                if (--columns) {
                    /*
                        x00 ... x0_ ..... x01
                        .       .         .
                        .       x         .
                        .       .         .
                        .       .         .
                        x10 ... x1_ ..... x11
                    */
                    INTERPOL_BILINEAR_NEON(s_00_01, s_10_11, frac_w, v_frac_h0, v_frac_h1, dst);
                    dst += 1;
                } else {
                    /* last column
                        x00
                        .
                        x
                        .
                        .
                        x10
                    */
                    INTERPOL_NEON(&s_00_01[0], &s_10_11[0], v_frac_h0, v_frac_h1, dst);
                    dst += 1;
                    break;
                }
            }
            dst = (Uint32 *)((Uint8 *)dst + dst_skip);
        } else {
            /* last row
                        x00 ... x ..... x01
            */
            // TODO: INTERPOL_NEON2/4?

            while (1) {
                const Uint32 *s_00_01;
                int index_w = 4 * SRC_INDEX(posx);
                int frac_w = FRAC(posx);
                uint8x8_t v_frac_w0, v_frac_w1;
                posx += incx;

                s_00_01 = (const Uint32 *)((const Uint8 *)src_h0 + index_w);

                v_frac_w0 = vmov_n_u8(frac_w);
                v_frac_w1 = vmov_n_u8(FRAC_ONE - frac_w);

                if (--columns) {
                    INTERPOL_NEON(&s_00_01[0], &s_00_01[1], v_frac_w0, v_frac_w1, dst);
                    dst += 1;
                } else {
                    /* last column and row
                            x00
                    */
                    *dst = s_00_01[0];
                    break;
                }
            }
            break;
        }
    }
}
#endif // HAVE_NEON_INTRINSICS

static void SDL_ChooseStrecher(void)
{
#ifdef HAVE_SSE2_INTRINSICS
#if HAVE_SSE2_SUPPORT
    SDL_assert(SDL_HasSSE2());
    if (1) {
#else
    if (SDL_HasSSE2()) {
#endif
        SDL_Scale_linear = scale_mat_SSE2;
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
        SDL_Scale_linear = scale_mat_NEON;
        return;
    }
#endif

    SDL_Scale_linear = scale_mat;

    SDL_assert(SDL_Scale_linear != NULL);
}

void SDL_LowerSoftStretchLinear(SDL_Surface *s, const SDL_Rect *srcrect,
                                SDL_Surface *d, const SDL_Rect *dstrect)
{
    int src_w, src_h, dst_w, dst_h, src_pitch, dst_pitch, dst_skip;
    Uint8 *src, *dst;

    if (SDL_Scale_linear == NULL) {
        SDL_ChooseStrecher();
    }

    src_w = srcrect->w;
    src_h = srcrect->h;
    dst_w = dstrect->w;
    dst_h = dstrect->h;
    src_pitch = s->pitch;
    dst_pitch = d->pitch;
    src = (Uint8 *)s->pixels + srcrect->x * 4 + srcrect->y * src_pitch;
    dst = (Uint8 *)d->pixels + dstrect->x * 4 + dstrect->y * dst_pitch;
    dst_skip = dst_pitch - 4 * dst_w;

    SDL_Scale_linear(src, src_w, src_h, src_pitch, dst, dst_w, dst_h, dst_skip);
}

#define SDL_SCALE_NEAREST__START       \
    Uint8 *dst = dst_ptr;              \
    Uint32 posy, incy;                 \
    Uint32 posx, incx;                 \
    Uint32 srcy, srcx, n;              \
    const Uint32 *src_h0;              \
    incy = FIXED_POINT(src_h) / dst_h; \
    incx = FIXED_POINT(src_w) / dst_w; \
    posy = incy / 2;

#define SDL_SCALE_NEAREST__HEIGHT                                         \
    srcy = SRC_INDEX(posy);                                               \
    src_h0 = (const Uint32 *)(src_ptr + srcy * src_pitch);                \
    posy += incy;                                                         \
    posx = incx / 2;                                                      \
    n = dst_w;

static void scale_mat_nearest_1(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch,
                               Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    Uint32 bpp = 1;
    SDL_SCALE_NEAREST__START
    while (dst_h--) {
        SDL_SCALE_NEAREST__HEIGHT
        while (n--) {
            const Uint8 *src;
            srcx = bpp * SRC_INDEX(posx);
            posx += incx;
            src = (const Uint8 *)src_h0 + srcx;
            *(Uint8 *)dst = *src;
            dst += bpp;
        }
        dst += dst_skip;
    }
}

static void scale_mat_nearest_2(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch,
                               Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    Uint32 bpp = 2;
    SDL_SCALE_NEAREST__START
    while (dst_h--) {
        SDL_SCALE_NEAREST__HEIGHT
        while (n--) {
            const Uint16 *src;
            srcx = bpp * SRC_INDEX(posx);
            posx += incx;
            src = (const Uint16 *)((const Uint8 *)src_h0 + srcx);
            *(Uint16 *)dst = *src;
            dst += bpp;
        }
        dst += dst_skip;
    }
}

static void scale_mat_nearest_3(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch,
                               Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    Uint32 bpp = 3;
    SDL_SCALE_NEAREST__START
    while (dst_h--) {
        SDL_SCALE_NEAREST__HEIGHT
        while (n--) {
            const Uint8 *src;
            srcx = bpp * SRC_INDEX(posx);
            posx += incx;
            src = (const Uint8 *)src_h0 + srcx;
            ((Uint8 *)dst)[0] = src[0];
            ((Uint8 *)dst)[1] = src[1];
            ((Uint8 *)dst)[2] = src[2];
            dst += bpp;
        }
        dst += dst_skip;
    }
}

static void scale_mat_nearest_4(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch,
                               Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip)
{
    Uint32 bpp = 4;
    SDL_SCALE_NEAREST__START
    while (dst_h--) {
        SDL_SCALE_NEAREST__HEIGHT
        while (n--) {
            const Uint32 *src;
            srcx = bpp * SRC_INDEX(posx);
            posx += incx;
            src = (const Uint32 *)((const Uint8 *)src_h0 + srcx);
            *(Uint32 *)dst = *src;
            dst += bpp;
        }
        dst += dst_skip;
    }
}

void SDL_LowerSoftStretchNearest(SDL_Surface *s, const SDL_Rect *srcrect,
                                 SDL_Surface *d, const SDL_Rect *dstrect)
{
    int src_w = srcrect->w;
    int src_h = srcrect->h;
    int dst_w = dstrect->w;
    int dst_h = dstrect->h;
    int src_pitch = s->pitch;
    int dst_pitch = d->pitch;

    const int bpp = d->format->BytesPerPixel;

    Uint8 *src = (Uint8 *)s->pixels + srcrect->x * bpp + srcrect->y * src_pitch;
    Uint8 *dst = (Uint8 *)d->pixels + dstrect->x * bpp + dstrect->y * dst_pitch;
    int dst_skip = dst_pitch - bpp * dst_w;

    void (*func)(const Uint8 *src_ptr, int src_w, int src_h, int src_pitch,
                 Uint8 *dst_ptr, int dst_w, int dst_h, int dst_skip) = scale_mat_nearest_1;
    if (bpp == 4) {
        func = scale_mat_nearest_4;
    } else if (bpp == 3) {
        func = scale_mat_nearest_3;
    } else if (bpp == 2) {
        func = scale_mat_nearest_2;
    }
    func(src, src_w, src_h, src_pitch, dst, dst_w, dst_h, dst_skip);
}

/* vi: set ts=4 sw=4 expandtab: */
