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

/* Functions for audio drivers to perform runtime conversion of audio format */

#include "SDL_audio.h"

#include "SDL_loadso.h"
#include "../SDL_dataqueue.h"
#include "SDL_cpuinfo.h"

#include "SDL_audio_c.h"
#include "SDL_audioresampler.h"
#include "SDL_audiotypecvt.h"

#define DEBUG_AUDIOSTREAM 0

/*
 * CHANNEL LAYOUTS AS SDL EXPECTS THEM:
 *
 * (Even if the platform expects something else later, that
 * SDL will swizzle between the app and the platform).
 *
 * Abbreviations:
 * - FRONT=single mono speaker
 * - FL=front left speaker
 * - FR=front right speaker
 * - FC=front center speaker
 * - BL=back left speaker
 * - BR=back right speaker
 * - SR=surround right speaker
 * - SL=surround left speaker
 * - BC=back center speaker
 * - LFE=low-frequency speaker
 *
 * These are listed in the order they are laid out in
 * memory, so "FL+FR" means "the front left speaker is
 * layed out in memory first, then the front right, then
 * it repeats for the next audio frame".
 *
 * 1 channel (mono) layout: FRONT
 * 2 channels (stereo) layout: FL+FR
 * 3 channels (2.1) layout: FL+FR+LFE
 * 4 channels (quad) layout: FL+FR+BL+BR
 * 5 channels (4.1) layout: FL+FR+LFE+BL+BR
 * 6 channels (5.1) layout: FL+FR+FC+LFE+BL+BR
 * 7 channels (6.1) layout: FL+FR+FC+LFE+BC+SL+SR
 * 8 channels (7.1) layout: FL+FR+FC+LFE+BL+BR+SL+SR
 */

#ifdef SDL_SSE_INTRINSICS
/* Convert from stereo to mono. Average left and right. */
static void SDLCALL SDL_ConvertStereoToMono_SSE(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt / (sizeof(float) * 2u);
    const __m128 divby2 = _mm_set1_ps(0.5f);
    float *dst = (float *)cvt->buf;
    const float *src = dst;
    int i = num_samples;

    LOG_DEBUG_CONVERT("stereo", "mono (using SSE)");

    cvt->len_cvt /= 2u;

    /* Do SSE blocks as long as we have 16 bytes available.
       Just use unaligned load/stores, if the memory at runtime is
       aligned it'll be just as fast on modern processors */
    while (i >= 4) { /* 4 * float32 */
        const __m128 src0 = _mm_loadu_ps(&src[0]);
        const __m128 src1 = _mm_loadu_ps(&src[4]);
#if 0
        const __m128 val = _mm_hadd_ps(src0, src1);
#else
        const __m128 val0 = _mm_shuffle_ps(src0, src1, _MM_SHUFFLE(2, 0, 2, 0));
        const __m128 val1 = _mm_shuffle_ps(src0, src1, _MM_SHUFFLE(3, 1, 3, 1));
        const __m128 val  = _mm_add_ps(val0, val1);
#endif
        _mm_storeu_ps(dst, _mm_mul_ps(val, divby2));

        i -= 4;
        src += 8;
        dst += 4;
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; i--, src += 2, dst++) {
        dst[0] = (src[0] + src[1]) * 0.5f;
    }
}

/* Convert from mono to stereo. Duplicate to stereo left and right. */
static void SDLCALL SDL_ConvertMonoToStereo_SSE(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt / (unsigned)sizeof(float);
    float *dst = (float *)(cvt->buf + (cvt->len_cvt * 2));
    const float *src = (const float *)(cvt->buf + cvt->len_cvt);
    int i = num_samples;

    LOG_DEBUG_CONVERT("mono", "stereo (using SSE)");

    cvt->len_cvt *= 2;

    /* Do SSE blocks as long as we have 16 bytes available.
       Just use unaligned load/stores, if the memory at runtime is
       aligned it'll be just as fast on modern processors */
    /* convert backwards, since output is growing in-place. */
    while (i >= 4) {                                           /* 4 * float32 */
        src -= 4;
        dst -= 8;
        {
        const __m128 input = _mm_loadu_ps(&src[0]);            /* A B C D */
        _mm_storeu_ps(&dst[0], _mm_unpacklo_ps(input, input)); /* A A B B */
        _mm_storeu_ps(&dst[4], _mm_unpackhi_ps(input, input)); /* C C D D */
        }
        i -= 4;
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; i--) {
        src--;
        dst -= 2;
        {
        const float srcFC = src[0];
        dst[1] /* FR */ = srcFC;
        dst[0] /* FL */ = srcFC;
        }
    }
}
#endif // SDL_SSE_INTRINSICS

#ifdef SDL_NEON_INTRINSICS
/* Convert from stereo to mono. Average left and right. */
static void SDLCALL SDL_ConvertStereoToMono_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt / (sizeof(float) * 2u);
    float *dst = (float *)cvt->buf;
    const float *src = dst;
    int i = num_samples;

    LOG_DEBUG_CONVERT("stereo", "mono (using NEON)");

    cvt->len_cvt /= 2u;

    if (!((size_t)src & 15)) {
        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        const float32x4_t divby2 = vdupq_n_f32(0.5f);
        while (i >= 4) { /* 4 * float32 */
            const float32x4_t src0 = vld1q_f32(&src[0]);
            const float32x4_t src1 = vld1q_f32(&src[4]);
#if 0
            const float32x4_t val = vpaddq_f32(src0, src1);
#else
            const float32x2_t val0 = vpadd_f32(vget_low_f32(src0), vget_high_f32(src0));
            const float32x2_t val1 = vpadd_f32(vget_low_f32(src1), vget_high_f32(src1));
            const float32x4_t val = vcombine_f32(val0, val1);
#endif
            vst1q_f32(dst, vmulq_f32(val, divby2));

            i -= 4;
            src += 8;
            dst += 4;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; i--, src += 2, dst++) {
        dst[0] = (src[0] + src[1]) * 0.5f;
    }
}

/* Convert from mono to stereo. Duplicate to stereo left and right. */
static void SDLCALL SDL_ConvertMonoToStereo_NEON(SDL_AudioCVT *cvt)
{
    const int num_samples = cvt->len_cvt / (unsigned)sizeof(float);
    float *dst = (float *)(cvt->buf + (cvt->len_cvt * 2));
    const float *src = (const float *)(cvt->buf + cvt->len_cvt);
    int i = num_samples;

    LOG_DEBUG_CONVERT("mono", "stereo (using NEON)");

    cvt->len_cvt *= 2;

    if (!((size_t)cvt->buf & 15)) {
        /* Get dst aligned to 16 bytes */
        for ( ; i && ((size_t)dst & 15); i--) {
            src--;
            dst -= 2;
            {
            const float srcFC = src[0];
            dst[1] /* FR */ = srcFC;
            dst[0] /* FL */ = srcFC;
            }
        }

        SDL_assert(!i || !((size_t)dst & 15));
        SDL_assert(!i || !((size_t)src & 15));

        /* Aligned! Do NEON blocks as long as we have 16 bytes available. */
        while (i >= 4) {                                           /* 4 * float32 */
            src -= 4;
            dst -= 8;
            {
            const float32x4_t input = vld1q_f32(&src[0]);
            const float32x4x2_t value = vzipq_f32(input, input);

            vst1q_f32(&dst[0], value.val[0]);
            vst1q_f32(&dst[4], value.val[1]);
            }
            i -= 4;
        }
    }

    /* Finish off any leftovers with scalar operations. */
    for ( ; i; i--) {
        src--;
        dst -= 2;
        {
        const float srcFC = src[0];
        dst[1] /* FR */ = srcFC;
        dst[0] /* FL */ = srcFC;
        }
    }
}
#endif // SDL_NEON_INTRINSICS

/* Include the autogenerated channel converters... */
#include "SDL_audio_channel_converters.h"

void SDL_ChooseAudioChannelConverters(void)
{
#ifdef SDL_NEON_INTRINSICS
#if SDL_HAVE_NEON_SUPPORT
    SDL_assert(SDL_HasNEON());
    SDL_assert(channel_converters[1][0] == NULL);
    SDL_assert(channel_converters[0][1] == NULL);
    if (1) {
#else
    SDL_assert(channel_converters[1][0] == SDL_ConvertStereoToMono);
    SDL_assert(channel_converters[0][1] == SDL_ConvertMonoToStereo);
    if (SDL_HasNEON()) {
#endif // SDL_HAVE_NEON_SUPPORT
        channel_converters[1][0] = SDL_ConvertStereoToMono_NEON;
        channel_converters[0][1] = SDL_ConvertMonoToStereo_NEON;
    }
#endif
#ifdef SDL_SSE_INTRINSICS
#if SDL_HAVE_SSE_SUPPORT
#ifdef SDL_NEON_INTRINSICS
    SDL_assert(channel_converters[1][0] == NULL || channel_converters[1][0] == SDL_ConvertStereoToMono_NEON);
    SDL_assert(channel_converters[0][1] == NULL || channel_converters[0][1] == SDL_ConvertMonoToStereo_NEON);
#else
    SDL_assert(channel_converters[1][0] == NULL);
    SDL_assert(channel_converters[0][1] == NULL);
#endif
    SDL_assert(SDL_HasSSE());
    if (1) {
#else
#ifdef SDL_NEON_INTRINSICS
    SDL_assert(channel_converters[1][0] == SDL_ConvertStereoToMono || channel_converters[1][0] == SDL_ConvertStereoToMono_NEON);
    SDL_assert(channel_converters[0][1] == SDL_ConvertMonoToStereo || channel_converters[0][1] == SDL_ConvertMonoToStereo_NEON);
#else
    SDL_assert(channel_converters[1][0] == SDL_ConvertStereoToMono);
    SDL_assert(channel_converters[0][1] == SDL_ConvertMonoToStereo);
#endif
    if (SDL_HasSSE()) {
#endif // SDL_HAVE_SSE_SUPPORT
        channel_converters[1][0] = SDL_ConvertStereoToMono_SSE;
        channel_converters[0][1] = SDL_ConvertMonoToStereo_SSE;
    }
#endif // SDL_SSE_INTRINSICS
}

static void SDL_PrivateConvertAudio(SDL_AudioCVT *cvt)
{
    int i;
    /* !!! FIXME: (cvt) should be const; stack-copy it here. */
    /* !!! FIXME: (actually, we can't...len_cvt needs to be updated. Grr.) */

    /* Make sure there's data to convert */
    SDL_assert(cvt->buf != NULL);

    /* Set up the conversion and go! */
    cvt->len_cvt = cvt->len;
    for (i = 0; ; i++) {
        if (cvt->filters[i] != NULL) {
            cvt->filters[i](cvt);
            continue;
        }
        break;
    }
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    /* !!! FIXME: (cvt) should be const; stack-copy it here. */
    /* !!! FIXME: (actually, we can't...len_cvt needs to be updated. Grr.) */

    /* Make sure there's data to convert */
    if (cvt->buf) {
        SDL_PrivateConvertAudio(cvt);
        return 0;
    } else {
        return SDL_SetError("No buffer allocated for conversion");
    }
}

static int SDL_AddAudioCVTFilter(SDL_AudioCVT *cvt, SDL_AudioFilter filter)
{
    SDL_assert(cvt->needed < SDL_AUDIOCVT_MAX_FILTERS);

    SDL_assert(filter != NULL);
    cvt->filters[cvt->needed++] = filter;
    return 0;
}

static int SDL_BuildAudioTypeCVTSwap(SDL_AudioCVT *cvt, const SDL_AudioFormat format)
{
    SDL_AudioFilter filter;

    switch (SDL_AUDIO_BITSIZE(format)) {
    case 16: filter = SDL_Convert_Byteswap16; break;
    case 32: filter = SDL_Convert_Byteswap32; break;
    default:
        SDL_assume(!"unhandled byteswap datatype!");
        return SDL_SetError("unhandled byteswap datatype!");
    }

    return SDL_AddAudioCVTFilter(cvt, filter);
}

static int SDL_BuildAudioTypeCVTToFloat(SDL_AudioCVT *cvt /*, const SDL_AudioFormat src_fmt*/)
{
    const SDL_AudioFormat src_fmt = cvt->src_format;
    int retval = 0; /* 0 == no conversion necessary. */

    if ((SDL_AUDIO_ISBIGENDIAN(src_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN) && SDL_AUDIO_BITSIZE(src_fmt) > 8) {
        if (SDL_BuildAudioTypeCVTSwap(cvt, src_fmt) < 0) {
            return -1; /* shouldn't happen, but just in case... */
        }
        // retval = 1; /* added a converter. */
    }

    if (!SDL_AUDIO_ISFLOAT(src_fmt)) {
        // const Uint16 src_bitsize = SDL_AUDIO_BITSIZE(src_fmt);
        // const Uint16 dst_bitsize = 32;
        SDL_AudioFilter filter;
        uint8_t len_shift;

        switch (src_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
        case AUDIO_S8:
            filter = SDL_Convert_S8_to_F32;
            len_shift = 2;
            break;
        case AUDIO_U8:
            filter = SDL_Convert_U8_to_F32;
            len_shift = 2;
            break;
        case AUDIO_S16:
            filter = SDL_Convert_S16_to_F32;
            len_shift = 1;
            break;
        case AUDIO_U16:
            filter = SDL_Convert_U16_to_F32;
            len_shift = 1;
            break;
        case AUDIO_S32:
            filter = SDL_Convert_S32_to_F32;
            len_shift = 0;
            break;
        default:
            SDL_assume(!"Unexpected audio format!");
            filter = NULL;
            len_shift = 0;
            break;
        }

        SDL_assume(filter != NULL);
        if (!filter) {
            return SDL_SetError("No conversion from source format to float available");
        }

        if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
            return -1; /* shouldn't happen, but just in case... */
        }
        cvt->len_mult <<= len_shift;
        cvt->len_num <<= len_shift;
        //if (src_bitsize < dst_bitsize) {
        //    const int mult = (dst_bitsize / src_bitsize);
        //    cvt->len_mult *= mult;
        //    cvt->len_ratio *= mult;
        // } else if (src_bitsize > dst_bitsize) {
        //    const int div = (src_bitsize / dst_bitsize);
        //    cvt->len_ratio /= div;
        // }

        // retval = 1; /* added a converter. */
    }

    return retval;
}

static int SDL_BuildAudioTypeCVTFromFloat(SDL_AudioCVT *cvt)
{
    const SDL_AudioFormat dst_fmt = cvt->dst_format;
    int retval = 0; /* 0 == no conversion necessary. */

    if (!SDL_AUDIO_ISFLOAT(dst_fmt)) {
        // const Uint16 dst_bitsize = SDL_AUDIO_BITSIZE(dst_fmt);
        // const Uint16 src_bitsize = 32;
        SDL_AudioFilter filter;
        uint8_t len_shift;
        switch (dst_fmt & ~SDL_AUDIO_MASK_ENDIAN) {
        case AUDIO_S8:
            filter = SDL_Convert_F32_to_S8;
            len_shift = 2;
            break;
        case AUDIO_U8:
            filter = SDL_Convert_F32_to_U8;
            len_shift = 2;
            break;
        case AUDIO_S16:
            filter = SDL_Convert_F32_to_S16;
            len_shift = 1;
            break;
        case AUDIO_U16:
            filter = SDL_Convert_F32_to_U16;
            len_shift = 1;
            break;
        case AUDIO_S32:
            filter = SDL_Convert_F32_to_S32;
            len_shift = 0;
            break;
        default:
            SDL_assume(!"Unexpected audio format!");
            filter = NULL;
            len_shift = 0;
            break;
        }

        SDL_assume(filter != NULL);
        if (!filter) {
            return SDL_SetError("No conversion from float to format 0x%.4x available", dst_fmt);
        }

        if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
            return -1; /* shouldn't happen, but just in case... */
        }
        cvt->len_denom <<= len_shift;
        // if (src_bitsize < dst_bitsize) {
        //     const int mult = (dst_bitsize / src_bitsize);
        //     cvt->len_mult *= mult;
        //     cvt->len_ratio *= mult;
        // } else if (src_bitsize > dst_bitsize) {
        //     const int div = (src_bitsize / dst_bitsize);
        //     cvt->len_ratio /= div;
        // }
        // retval = 1; /* added a converter. */
    }

    if ((SDL_AUDIO_ISBIGENDIAN(dst_fmt) != 0) == (SDL_BYTEORDER == SDL_LIL_ENDIAN) && SDL_AUDIO_BITSIZE(dst_fmt) > 8) {
        if (SDL_BuildAudioTypeCVTSwap(cvt, dst_fmt) < 0) {
            return -1; /* shouldn't happen, but just in case... */
        }
        // retval = 1; /* added a converter. */
    }

    return retval;
}
#ifndef SDL_RESAMPLER_DISABLED
#ifdef HAVE_LIBSAMPLERATE_H

static void SDLCALL SDL_ResampleCVT_SRC(SDL_AudioCVT *cvt)
{
    const int chans = cvt->dst_channels;
    const float *src = (const float *)cvt->buf;
    const int srclen = cvt->len_cvt;
    float *dst = (float *)(cvt->buf + srclen);
    const int dstlen = (cvt->len * cvt->len_mult) - srclen;
    const int framelen = sizeof(float) * chans;
    int result = 0;
    SRC_DATA data;

    SDL_zero(data);

    data.data_in = (float *)src; /* Older versions of libsamplerate had a non-const pointer, but didn't write to it */
    data.input_frames = srclen / framelen;

    data.data_out = dst;
    data.output_frames = dstlen / framelen;

    data.src_ratio = cvt->rate_incr;

    result = SRC_src_simple(&data, SRC_converter, chans); /* Simple API converts the whole buffer at once.  No need for initialization. */
/* !!! FIXME: Handle library failures? */
#if DEBUG_CONVERT
    if (result != 0) {
        SDL_Log("src_simple() failed: %s", SRC_src_strerror(result));
    }
#else
    (void)result;
#endif

    cvt->len_cvt = data.output_frames_gen * framelen;

    SDL_memmove(cvt->buf, dst, cvt->len_cvt);
}

#endif /* HAVE_LIBSAMPLERATE_H */

static void SDLCALL SDL_ResampleCVT(SDL_AudioCVT *cvt)
{
    /* !!! FIXME in 2.1: there are ten slots in the filter list, and the theoretical maximum we use is six (seven with NULL terminator).
       !!! FIXME in 2.1:   We need to store data for this resampler, because the cvt structure doesn't store the original sample rates,
       !!! FIXME in 2.1:   so we steal the ninth and tenth slot.  :( */
    const int inrate = (int)(size_t)cvt->filters[SDL_AUDIOCVT_MAX_FILTERS - 1];
    const int outrate = (int)(size_t)cvt->filters[SDL_AUDIOCVT_MAX_FILTERS];
#if 0
    const float *src;
    int paddingsamples, srclen, dstlen;
    float *padding, *lpaddingend, *dst;
    const int padding_steps = ResamplerPadding(inrate, outrate);
    const Uint8 chans = cvt->dst_channels;
    size_t padding_len;

    SDL_assert(padding_steps <= SDL_INT_MAX / chans);

    paddingsamples = padding_steps * chans;

    SDL_assert(paddingsamples <= SDL_SIZE_MAX / sizeof(float));
    padding_len = paddingsamples * sizeof(float);
    /* we keep no streaming state here, so pad with silence on both ends. */
    SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
    padding = (float *)SDL_calloc(1, padding_len);
    SDL_expect(padding,
        SDL_OutOfMemory();
        return;
    );

    lpaddingend = (float *)((Uint8 *)padding + padding_len);
    src = (const float *)cvt->buf;
    srclen = cvt->len_cvt;
    /*dst = (float *) cvt->buf;
    dstlen = (cvt->len * cvt->len_mult);*/
    /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
    dst = (float *)(cvt->buf + srclen);
    // dstlen = (cvt->len * cvt->len_mult) - srclen;

    cvt->len_cvt = SDL_ResampleAudio(chans, inrate, outrate, lpaddingend, padding, src, srclen, dst);

    SDL_free(padding);

    SDL_memmove(cvt->buf, dst, cvt->len_cvt); /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
#endif
    const int padding_steps = ResamplerPadding(inrate, outrate);
    const Uint8 chans = cvt->dst_channels;
    Uint8 *workbuf, *tmp = NULL;
    int paddingsamples, padding_len, indatalen, dstlen;
    Uint8 *dst = cvt->buf;
    const int srclen = cvt->len_cvt;
    const int buflen = cvt->len * cvt->len_mult;
    const int framelen = chans * sizeof(float);
    const int inframes = srclen / framelen;
    const int outframes = (int)((Sint64)inframes * outrate / inrate);
    const int reslen = outframes * framelen;
    SDL_AudioResampler resampler_impl;

    SDL_assert(outrate <= SDL_MAX_SINT64 / inframes);
    SDL_assert(((Sint64)inframes * outrate / inrate) <= SDL_INT_MAX);
    SDL_assert(inrate <= SDL_MAX_SINT64 / outframes);
    SDL_assert(((Sint64)outframes * inrate / outrate) <= SDL_INT_MAX);
    // SDL_assert(outbuflen >= outframes * framelen);

    SDL_assert(padding_steps <= SDL_INT_MAX / chans);

    paddingsamples = padding_steps * chans;

    SDL_assert(paddingsamples <= SDL_INT_MAX / sizeof(float));
    padding_len = paddingsamples * sizeof(float);

    /* we keep no streaming state here, so pad with silence on both ends. */
    indatalen = 2 * padding_len + srclen;
    if (buflen >= indatalen + reslen) {
        /* enough buffer for the two paddings (should be the case most of the time) -> prepare space at the end of the buffer */
        dstlen = buflen - indatalen;
        workbuf = dst + dstlen;
        workbuf += padding_len;
        SDL_memmove(workbuf, dst, srclen); // move the data to the designated area
        SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
        SDL_memset(workbuf - padding_len, '\0', padding_len);
        SDL_memset(workbuf + srclen, '\0', padding_len);
    } else {
        /* not enough buffer for the paddings -> alloc ahead... */
        // dstlen = buflen;
        SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
        tmp = (Uint8 *)SDL_calloc(1, indatalen);
        SDL_expect(tmp,
            SDL_OutOfMemory();
            return;
        );
        workbuf = tmp + padding_len;
        SDL_memcpy(workbuf, dst, srclen); // copy the data to the designated area
    }

    resampler_impl = SDL_Resampler_Generic;
    if (chans == 1) {
        resampler_impl = SDL_Resampler_Mono;
    } else if (chans == 2) {
        resampler_impl = SDL_Resampler_Stereo;
    }
    cvt->len_cvt = resampler_impl(chans, inrate, outrate, (const float *)workbuf, inframes, (float *)dst, outframes);

    SDL_assert(cvt->len_cvt == reslen);

    SDL_free(tmp);
}

static SDL_AudioFilter ChooseCVTResampler(void)
{
#ifdef HAVE_LIBSAMPLERATE_H
    if (SRC_available) {
        return SDL_ResampleCVT_SRC;
    }
#endif /* HAVE_LIBSAMPLERATE_H */

    return SDL_ResampleCVT;
}

static int SDL_BuildAudioResampleCVT(SDL_AudioCVT *cvt, /*const int dst_channels, */
                                     const int src_rate, const int dst_rate)
{
    SDL_AudioFilter filter;

    if (src_rate == dst_rate) {
        return 0; /* no conversion necessary. */
    }

    filter = ChooseCVTResampler(/* dst_channels */);
    SDL_assume(filter != NULL);
    if (!filter) {
        return SDL_SetError("No conversion available for these rates");
    }

    /* Update (cvt) with filter details... */
    if (SDL_AddAudioCVTFilter(cvt, filter) < 0) {
        return -1; /* shouldn't happen, but just in case... */
    }

    /* !!! FIXME in 2.1: there are ten slots in the filter list, and the theoretical maximum we use is six (seven with NULL terminator).
       !!! FIXME in 2.1:   We need to store data for this resampler, because the cvt structure doesn't store the original sample rates,
       !!! FIXME in 2.1:   so we steal the ninth and tenth slot.  :( */
    SDL_assert(cvt->needed < (SDL_AUDIOCVT_MAX_FILTERS - 2));

    cvt->filters[SDL_AUDIOCVT_MAX_FILTERS - 1] = (SDL_AudioFilter)(uintptr_t)src_rate;
    cvt->filters[SDL_AUDIOCVT_MAX_FILTERS] = (SDL_AudioFilter)(uintptr_t)dst_rate;

    if (src_rate < dst_rate) {
        const int mult = (dst_rate + src_rate - 1) / src_rate;
        cvt->len_mult *= mult;
        // cvt->len_mult *= (int)SDL_ceil(mult);
        // cvt->len_ratio *= ((double)dst_rate) / ((double)src_rate);
    } else {
        // cvt->len_ratio /= ((double)src_rate) / ((double)dst_rate);
    }

    /* !!! FIXME: remove this if we can get the resampler to work in-place again. */
    /* the buffer is big enough to hold the destination now, but
       we need it large enough to hold a separate scratch buffer. */
    cvt->len_mult *= 2;

    // return 1; /* added a converter. */
    return 0;
}
#else
static int SDL_BuildAudioResampleCVT(SDL_AudioCVT *cvt, /*const int dst_channels, */
                                     const int src_rate, const int dst_rate)
{
    if (src_rate == dst_rate) {
        return 0; /* no conversion necessary. */
    }

    return SDL_Unsupported();
}
#endif /* !SDL_RESAMPLER_DISABLED */
static int SDL_BuildAudioChannelCVT(SDL_AudioCVT *cvt/*, const int src_channels, const int dst_channels*/)
{
    const Uint8 src_channels = cvt->src_channels;
    const Uint8 dst_channels = cvt->dst_channels;
    SDL_AudioFilter channel_converter;

    /* SDL_SupportedChannelCount should have caught these asserts, or we added a new format and forgot to update the table. */
    SDL_assert(src_channels <= SDL_arraysize(channel_converters));
    SDL_assert(dst_channels <= SDL_arraysize(channel_converters[0]));

    if (src_channels != dst_channels) {
        channel_converter = channel_converters[src_channels - 1][dst_channels - 1];
        if (!channel_converter) {
            /* All combinations of supported channel counts should have been handled by now, but let's be defensive */
            SDL_assume(!"Invalid channel combination");
            return SDL_SetError("Invalid channel combination");
        }

        if (SDL_AddAudioCVTFilter(cvt, channel_converter) < 0) {
            return -1; /* shouldn't happen, but just in case... */
        }

        if (src_channels < dst_channels) {
            cvt->len_mult = ((cvt->len_mult * dst_channels) + (src_channels - 1)) / src_channels;
        }

        // cvt->len_ratio = (cvt->len_ratio * dst_channels) / src_channels;
        cvt->len_num *= dst_channels;
        cvt->len_denom *= src_channels;
    }
    return 0;
}

static SDL_bool SDL_SupportedAudioFormat(const SDL_AudioFormat fmt)
{
    switch (fmt) {
    case AUDIO_U8:
    case AUDIO_S8:
    case AUDIO_U16LSB:
    case AUDIO_S16LSB:
    case AUDIO_U16MSB:
    case AUDIO_S16MSB:
    case AUDIO_S32LSB:
    case AUDIO_S32MSB:
    case AUDIO_F32LSB:
    case AUDIO_F32MSB:
        return SDL_TRUE; /* supported. */

    default:
        break;
    }

    return SDL_FALSE; /* unsupported. */
}

static SDL_bool SDL_SupportedChannelCount(const int channels)
{
    return ((channels >= 1) && (channels <= NUM_CHANNELS)) ? SDL_TRUE : SDL_FALSE;
}

/* Creates a set of audio filters to convert from one format to another.
   Returns 0 if no conversion is needed, 1 if the audio filter is set up,
   or -1 if an error like invalid parameter, unsupported format, etc. occurred.
*/

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt,
                      SDL_AudioFormat src_format, Uint8 src_channels, int src_rate,
                      SDL_AudioFormat dst_format, Uint8 dst_channels, int dst_rate)
{
    /* Sanity check target pointer */
    if (!cvt) {
        return SDL_InvalidParamError("cvt");
    }

    /* Make sure we zero out the audio conversion before error checking */
    SDL_zerop(cvt);

    if (!SDL_SupportedAudioFormat(src_format)) {
        return SDL_InvalidParamError("src_format");
    }
    if (!SDL_SupportedAudioFormat(dst_format)) {
        return SDL_InvalidParamError("dst_format");
    }
    if (!SDL_SupportedChannelCount(src_channels)) {
        return SDL_InvalidParamError("src_channels");
    }
    if (!SDL_SupportedChannelCount(dst_channels)) {
        return SDL_InvalidParamError("dst_channels");
    }
    if (src_rate <= 0) {
        return SDL_InvalidParamError("src_rate");
    }
    if (dst_rate <= 0) {
        return SDL_InvalidParamError("dst_rate");
    }
#if DEBUG_CONVERT
    SDL_Log("SDL_AUDIO_CONVERT: Build format %04x->%04x, channels %u->%u, rate %d->%d\n",
            src_format, dst_format, src_channels, dst_channels, src_rate, dst_rate);
#endif

    /* Start off with no conversion necessary */
    // cvt->needed = 0;
    cvt->src_channels = src_channels;
    cvt->dst_channels = dst_channels;
    cvt->src_format = src_format;
    cvt->dst_format = dst_format;
    // cvt->filter_index = 0;
    // SDL_zeroa(cvt->filters);
    cvt->len_mult = 1;
    cvt->len_ratio = 1.0;
    cvt->rate_incr = ((double)dst_rate) / ((double)src_rate);

    /* Make sure we've chosen audio conversion functions (SIMD, scalar, etc.) */
    SDL_ChooseAudioConverters();

    /* Type conversion goes like this now:
        - byteswap to CPU native format first if necessary.
        - convert to native Float32 if necessary.
        - resample and change channel count if necessary.
        - convert to final data format.
        - byteswap back to foreign format if necessary.

       The expectation is we can process data faster in float32
       (possibly with SIMD), and making several passes over the same
       buffer is likely to be CPU cache-friendly, avoiding the
       biggest performance hit in modern times. Previously we had
       (script-generated) custom converters for every data type and
       it was a bloat on SDL compile times and final library size. */

    /* see if we can skip float conversion entirely. */
    if (src_rate == dst_rate && src_channels == dst_channels) {
        if (src_format == dst_format) {
            return 0;
        }

        /* just a byteswap needed? */
        if ((src_format ^ dst_format) == SDL_AUDIO_MASK_ENDIAN) {
            SDL_assert(SDL_AUDIO_BITSIZE(src_format) > 8);
            if (SDL_BuildAudioTypeCVTSwap(cvt, src_format) < 0) {
                return -1; /* shouldn't happen, but just in case... */
            }
            SDL_assert(cvt->needed == 1);
            // cvt->needed = 1;
            return 1;
        }
    }
    cvt->len_num = dst_rate;
    cvt->len_denom = src_rate;

    /* Convert data types, if necessary. Updates (cvt). */
    if (SDL_BuildAudioTypeCVTToFloat(cvt/*, src_format*/) < 0) {
        return -1; /* shouldn't happen, but just in case... */
    }

    /* Channel conversion */
    if (SDL_BuildAudioChannelCVT(cvt/*, src_channels, dst_channels*/) < 0) {
        return -1; /* shouldn't happen, but just in case... */
    }

    /* Do rate conversion, if necessary. Updates (cvt). */
    if (SDL_BuildAudioResampleCVT(cvt, /*dst_channels, */src_rate, dst_rate) < 0) {
        return -1; /* shouldn't happen, but just in case... */
    }

    /* Move to final data type. */
    if (SDL_BuildAudioTypeCVTFromFloat(cvt/*, dst_format*/) < 0) {
        return -1; /* shouldn't happen, but just in case... */
    }

    cvt->len_ratio = (double)cvt->len_num / (double)cvt->len_denom;
    cvt->needed = (cvt->needed != 0) ? 1 : 0;
    return cvt->needed;
}

typedef int (*SDL_ResampleAudioStreamFunc)(SDL_AudioStream *stream, const void *inbuf, const int inbuflen, void *outbuf, const int outbuflen);
typedef void (*SDL_ResetAudioStreamResamplerFunc)(SDL_AudioStream *stream);
typedef void (*SDL_CleanupAudioStreamResamplerFunc)(SDL_AudioStream *stream);

#define AUDIO_PACKET_LEN 4096 /* !!! FIXME: good enough for now. */

struct _SDL_AudioStream
{
#ifndef SDL_RESAMPLER_DISABLED
    SDL_AudioCVT cvt_before_resampling;
#endif
    SDL_AudioCVT cvt_after_resampling;
    SDL_DataQueue *queue;
    Uint8 *work_buffer_base; /* maybe unaligned pointer from SDL_realloc(). */
    Uint8 *work_buffer_ptr;  /* the aligned pointer from SDL_realloc(). */
    int work_buffer_len;
    int src_sample_frame_size;
    int dst_sample_frame_size;
    // int packetlen;
#ifndef SDL_RESAMPLER_DISABLED
    int src_rate;
    int dst_rate;
#ifdef HAVE_LIBSAMPLERATE_H
    double rate_incr;
#endif
    Uint8 *staging_buffer;
    int staging_buffer_len;
    int staging_buffer_fill_len;
    SDL_boolean first_run;
    SDL_boolean resampling_needed;
    Uint8 pre_resample_channels;
    int resampler_padding_len;
    float *resampler_padding;
    void *resampler_state;
    SDL_ResampleAudioStreamFunc resampler_func;
    SDL_ResetAudioStreamResamplerFunc reset_resampler_func;
    SDL_CleanupAudioStreamResamplerFunc cleanup_resampler_func;
    SDL_AudioResampler resampler_impl;
#endif
};

static Uint8 *EnsureStreamBufferSize(SDL_AudioStream *stream, int len)
{
    Uint8 *retval;

    if (stream->work_buffer_len >= len) {
        retval = stream->work_buffer_ptr;
    } else {
        const size_t alignment_1 = SDL_SIMDGetAlignment() - 1;
        const size_t padding = (alignment_1 + 1 - (len & alignment_1)) & alignment_1;
        len += padding;
        retval = (Uint8 *)SDL_realloc(stream->work_buffer_base, alignment_1 + len);
        if (retval) {
            stream->work_buffer_base = retval;
            stream->work_buffer_len = len;
            /* Make sure we're aligned to 16 bytes for SIMD code. */
            retval = (Uint8 *)((size_t)(retval + alignment_1) & ~alignment_1);
            stream->work_buffer_ptr = retval;
        } else {
            SDL_OutOfMemory();
        }
    }

    return retval;
}
#ifndef SDL_RESAMPLER_DISABLED
#ifdef HAVE_LIBSAMPLERATE_H
static int SDL_ResampleAudioStream_SRC(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const float *inbuf = (const float *)_inbuf;
    float *outbuf = (float *)_outbuf;
    const int framelen = sizeof(float) * stream->pre_resample_channels;
    SRC_STATE *state = (SRC_STATE *)stream->resampler_state;
    SRC_DATA data;
    int result;

    SDL_assert(inbuf != ((const float *)outbuf)); /* SDL_AudioStreamPut() shouldn't allow in-place resamples. */

    data.data_in = (float *)inbuf; /* Older versions of libsamplerate had a non-const pointer, but didn't write to it */
    data.input_frames = inbuflen / framelen;
    data.input_frames_used = 0;

    data.data_out = outbuf;
    data.output_frames = outbuflen / framelen;

    data.end_of_input = 0;
    data.src_ratio = stream->rate_incr;

    result = SRC_src_process(state, &data);
    if (result != 0) {
        SDL_SetError("src_process() failed: %s", SRC_src_strerror(result));
        return 0;
    }

    /* If this fails, we need to store them off somewhere */
    SDL_assert(data.input_frames_used == data.input_frames);

    return data.output_frames_gen * (sizeof(float) * stream->pre_resample_channels);
}

static void SDL_ResetAudioStreamResampler_SRC(SDL_AudioStream *stream)
{
    SRC_src_reset((SRC_STATE *)stream->resampler_state);
}

static void SDL_CleanupAudioStreamResampler_SRC(SDL_AudioStream *stream)
{
    SRC_STATE *state = (SRC_STATE *)stream->resampler_state;
    if (state) {
        SRC_src_delete(state);
    }
}

static SDL_bool SetupLibSampleRateResampling(SDL_AudioStream *stream)
{
    int result = 0;
    SRC_STATE *state = NULL;

    if (SRC_available) {
        state = SRC_src_new(SRC_converter, stream->pre_resample_channels, &result);
        if (!state) {
            SDL_SetError("src_new() failed: %s", SRC_src_strerror(result));
        }
    }

    if (!state) {
        return SDL_FALSE;
    }

    stream->resampler_state = state;
    stream->resampler_func = SDL_ResampleAudioStream_SRC;
    stream->reset_resampler_func = SDL_ResetAudioStreamResampler_SRC;
    stream->cleanup_resampler_func = SDL_CleanupAudioStreamResampler_SRC;

    return SDL_TRUE;
}
#endif /* HAVE_LIBSAMPLERATE_H */

static int SDL_ResampleAudioStream(SDL_AudioStream *stream, const void *_inbuf, const int inbuflen, void *_outbuf, const int outbuflen)
{
    const float *inbuf = (const float *)_inbuf;
    float *outbuf = (float *)_outbuf;
    const int padding_len = stream->resampler_padding_len;
    Uint8 chans;
    int framelen, inframes, inrate, outrate, outframes, retval;

    SDL_assert(inbuf != outbuf); /* SDL_AudioStreamPut() shouldn't allow in-place resamples. */

    SDL_memcpy((Uint8 *)inbuf - padding_len, stream->resampler_state, padding_len);

    chans = stream->pre_resample_channels;
    framelen = chans * sizeof(float);
    inframes = inbuflen / framelen;

    inrate = stream->src_rate;
    outrate = stream->dst_rate;
    SDL_assert(outrate <= SDL_MAX_SINT64 / inframes);
    SDL_assert(((Sint64)inframes * outrate / inrate) <= SDL_INT_MAX);
    SDL_assert(inrate <= SDL_MAX_SINT64 / outframes);
    SDL_assert(((Sint64)outframes * inrate / outrate) <= SDL_INT_MAX);
    // SDL_assert(outbuflen >= outframes * framelen);
    outframes = (int)((Sint64)inframes * outrate / inrate);

    retval = stream->resampler_impl(chans, inrate, outrate, inbuf, inframes, outbuf, outframes);

    /* update our left padding with end of current input, for next run. */
    SDL_memcpy(stream->resampler_state, (Uint8 *)inbuf + inbuflen - padding_len, padding_len);
    return retval;
}

static void SDL_ResetAudioStreamResampler(SDL_AudioStream *stream)
{
    /* set all the padding to silence. */
    SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
    SDL_memset(stream->resampler_state, '\0', stream->resampler_padding_len);
}

static void SDL_CleanupAudioStreamResampler(SDL_AudioStream *stream)
{
    SDL_free(stream->resampler_state);
}
#endif /* !SDL_RESAMPLER_DISABLED */
SDL_AudioStream *SDL_NewAudioStream(const SDL_AudioFormat src_format,
                   const Uint8 src_channels,
                   const int src_rate,
                   const SDL_AudioFormat dst_format,
                   const Uint8 dst_channels,
                   const int dst_rate)
{
    SDL_AudioStream *retval;
#ifndef SDL_RESAMPLER_DISABLED
    Uint8 pre_resample_channels;
    if (src_channels == 0) {
        SDL_InvalidParamError("src_channels");
        return NULL;
    }

    if (dst_channels == 0) {
        SDL_InvalidParamError("dst_channels");
        return NULL;
    }
    pre_resample_channels = SDL_min(src_channels, dst_channels);
#endif
    retval = (SDL_AudioStream *)SDL_calloc(1, sizeof(SDL_AudioStream));
    if (!retval) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* If increasing channels, do it after resampling, since we'd just
       do more work to resample duplicate channels. If we're decreasing, do
       it first so we resample the interpolated data instead of interpolating
       the resampled data (!!! FIXME: decide if that works in practice, though!). */
    retval->src_sample_frame_size = (SDL_AUDIO_BITSIZE(src_format) / 8) * src_channels;
    retval->dst_sample_frame_size = (SDL_AUDIO_BITSIZE(dst_format) / 8) * dst_channels;
#ifndef SDL_RESAMPLER_DISABLED
    retval->src_rate = src_rate;
    retval->dst_rate = dst_rate;
    retval->first_run = SDL_TRUE;
    retval->pre_resample_channels = pre_resample_channels;
#endif
    // retval->packetlen = AUDIO_PACKET_LEN;

    /* Not resampling? It's an easy conversion (and maybe not even that!) */
    if (src_rate == dst_rate) {
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, src_format, src_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL; /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
    } else {
#ifndef SDL_RESAMPLER_DISABLED
        int padding_steps = ResamplerPadding(src_rate, dst_rate);
        int padding_samples;

        retval->resampling_needed = SDL_TRUE;
        SDL_assert(padding_steps <= SDL_INT_MAX / pre_resample_channels);
        padding_samples = padding_steps * pre_resample_channels;
        SDL_assert(padding_samples <= SDL_INT_MAX / sizeof(float));
        retval->resampler_padding_len = padding_samples * sizeof(float);

        SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
        retval->resampler_padding = (float *)SDL_calloc(1, retval->resampler_padding_len);
        SDL_assert(padding_steps <= SDL_INT_MAX / retval->src_sample_frame_size);
        retval->staging_buffer_len = padding_steps * retval->src_sample_frame_size;
        retval->staging_buffer = (Uint8 *)SDL_malloc(retval->staging_buffer_len);
        SDL_expect(retval->resampler_padding && retval->staging_buffer,
            SDL_FreeAudioStream(retval);
            SDL_OutOfMemory();
            return NULL;
        );

        /* Don't resample at first. Just get us to Float32 format. */
        /* !!! FIXME: convert to int32 on devices without hardware float. */
        if (SDL_BuildAudioCVT(&retval->cvt_before_resampling, src_format, src_channels, src_rate, AUDIO_F32SYS, pre_resample_channels, src_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL; /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }

#ifdef HAVE_LIBSAMPLERATE_H
        if (!SetupLibSampleRateResampling(retval)) {
#else
        if (1) {
#endif
            SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
            retval->resampler_state = SDL_calloc(1, retval->resampler_padding_len);
            SDL_expect(retval->resampler_state,
                SDL_FreeAudioStream(retval);
                SDL_OutOfMemory();
                return NULL;
            );

            retval->resampler_func = SDL_ResampleAudioStream;
            retval->reset_resampler_func = SDL_ResetAudioStreamResampler;
            retval->cleanup_resampler_func = SDL_CleanupAudioStreamResampler;
            retval->resampler_impl = SDL_Resampler_Generic;
            if (pre_resample_channels == 1) {
                retval->resampler_impl = SDL_Resampler_Mono;
            } else if (pre_resample_channels == 2) {
                retval->resampler_impl = SDL_Resampler_Stereo;
            }
        }

        /* Convert us to the final format after resampling. */
        if (SDL_BuildAudioCVT(&retval->cvt_after_resampling, AUDIO_F32SYS, pre_resample_channels, dst_rate, dst_format, dst_channels, dst_rate) < 0) {
            SDL_FreeAudioStream(retval);
            return NULL; /* SDL_BuildAudioCVT should have called SDL_SetError. */
        }
#ifdef HAVE_LIBSAMPLERATE_H
        /* calculate rate_incr only after the rates are 'validated' */
        retval->rate_incr = (double)retval->dst_rate / (double)retval->src_rate;
#endif
#else
        SDL_Unsupported();
        return NULL;
#endif /* !SDL_RESAMPLER_DISABLED */
    }

    retval->queue = SDL_NewDataQueue(AUDIO_PACKET_LEN, AUDIO_PACKET_LEN * 2);
    if (!retval->queue) {
        SDL_FreeAudioStream(retval);
        return NULL; /* SDL_NewDataQueue should have called SDL_SetError. */
    }

    return retval;
}

static int SDL_AudioStreamPutInternal(SDL_AudioStream *stream, const void *buf, int len, int *maxputbytes)
{
    int buflen = len;
    int workbuflen;
    Uint8 *workbuf;
    Uint8 *resamplebuf = NULL;
    int resamplebuflen = 0;
    int neededpaddingbytes = 0;
    int paddingbytes = 0;

    /* !!! FIXME: several converters can take advantage of SIMD, but only
       !!! FIXME:  if the data is aligned to 16 bytes. EnsureStreamBufferSize()
       !!! FIXME:  guarantees the buffer will align, but the
       !!! FIXME:  converters will iterate over the data backwards if
       !!! FIXME:  the output grows, and this means we won't align if buflen
       !!! FIXME:  isn't a multiple of 16. In these cases, we should chop off
       !!! FIXME:  a few samples at the end and convert them separately. */

    SDL_assert(buflen > 0);
#ifndef SDL_RESAMPLER_DISABLED
    SDL_assert(!stream->resampling_needed || buflen >= stream->resampler_padding_len);
#endif

    /* Make sure the work buffer can hold all the data we need at once... */
    workbuflen = buflen;
#ifndef SDL_RESAMPLER_DISABLED
    if (stream->cvt_before_resampling.needed) {
        workbuflen *= stream->cvt_before_resampling.len_mult;
    }

    if (stream->resampling_needed) {
        neededpaddingbytes = stream->resampler_padding_len;
        /* no padding prepended on first run. */
        paddingbytes = stream->first_run ? 0 : neededpaddingbytes;
        stream->first_run = SDL_FALSE;

        workbuflen += paddingbytes;       // add the last bytes from the previous run

        { /* resamples can't happen in place, so make space for second buf. */
        const int currbuflen = workbuflen - neededpaddingbytes; // no need to resample the last bytes (going to be buffered for the next run)
        const int inrate = stream->src_rate;
        const int outrate = stream->dst_rate;
        const Uint8 chans = stream->pre_resample_channels;
        const int framelen = chans * sizeof(float);
        const int inframes = currbuflen / framelen;
        const int outframes = (int)((Sint64)inframes * outrate / inrate);
        resamplebuflen = outframes * framelen;
#if DEBUG_AUDIOSTREAM
        SDL_Log("AUDIOSTREAM: will resample %d bytes to %d (ratio=%.6f)\n", workbuflen, resamplebuflen, (double)stream->dst_rate / (double)stream->src_rate);
#endif
        }
        workbuflen += resamplebuflen;
        workbuflen += neededpaddingbytes; // add space for the padding bytes stored by the resampler function
        if (stream->cvt_after_resampling.needed) {
            workbuflen = SDL_max(workbuflen, resamplebuflen * stream->cvt_after_resampling.len_mult);
        }
    } else
#endif
      if (stream->cvt_after_resampling.needed) {
        workbuflen *= stream->cvt_after_resampling.len_mult;
    }

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: Putting %d bytes of preconverted audio, need %d byte work buffer\n", buflen, workbuflen);
#endif

    workbuf = EnsureStreamBufferSize(stream, workbuflen);
    if (!workbuf) {
        return -1; /* probably out of memory. */
    }

    workbuf += neededpaddingbytes; // skip the space which was reserved for the resampler function

    resamplebuf = workbuf; /* default if not resampling. */

    SDL_memcpy(workbuf + paddingbytes, buf, buflen);
#ifndef SDL_RESAMPLER_DISABLED
    if (stream->cvt_before_resampling.needed) {
        stream->cvt_before_resampling.buf = workbuf + paddingbytes;
        stream->cvt_before_resampling.len = buflen;
        SDL_PrivateConvertAudio(&stream->cvt_before_resampling);
        buflen = stream->cvt_before_resampling.len_cvt;

#if DEBUG_AUDIOSTREAM
        SDL_Log("AUDIOSTREAM: After initial conversion we have %d bytes\n", buflen);
#endif
    }

    if (stream->resampling_needed) {
        /* save off some samples at the end; they are used for padding now so
           the resampler is coherent and then used at the start of the next
           put operation. Prepend last put operation's padding, too. */

        /* prepend prior put's padding. :P */
        if (paddingbytes) {
            SDL_memcpy(workbuf, stream->resampler_padding, paddingbytes);
            buflen += paddingbytes;
        }

        resamplebuf = workbuf + buflen; /* skip to second piece of workbuf. */

        /* save off the data at the end for the next run. */
        SDL_memcpy(stream->resampler_padding, workbuf + (buflen - neededpaddingbytes), neededpaddingbytes);
        buflen -= neededpaddingbytes;

        SDL_assert(buflen >= 0);
        if (buflen > 0) {
            buflen = stream->resampler_func(stream, workbuf, buflen, resamplebuf, resamplebuflen);
        }

#if DEBUG_AUDIOSTREAM
        SDL_Log("AUDIOSTREAM: After resampling we have %d bytes\n", buflen);
#endif
    }
    if (stream->cvt_after_resampling.needed && (buflen > 0)) {
#else
    if (stream->cvt_after_resampling.needed) {
#endif // !SDL_RESAMPLER_DISABLED
        stream->cvt_after_resampling.buf = resamplebuf;
        stream->cvt_after_resampling.len = buflen;
        SDL_PrivateConvertAudio(&stream->cvt_after_resampling);
        buflen = stream->cvt_after_resampling.len_cvt;

#if DEBUG_AUDIOSTREAM
        SDL_Log("AUDIOSTREAM: After final conversion we have %d bytes\n", buflen);
#endif
    }

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: Final output is %d bytes\n", buflen);
#endif

    if (maxputbytes) {
        const int maxbytes = *maxputbytes;
        if (buflen > maxbytes) {
            buflen = maxbytes;
        }
        *maxputbytes -= buflen;
    }

    /* resamplebuf holds the final output, even if we didn't resample. */
    SDL_assert(stream->queue != NULL);
    return buflen ? SDL_WriteToDataQueue(stream->queue, resamplebuf, buflen) : 0;
}

int SDL_AudioStreamPut(SDL_AudioStream *stream, const void *buf, int len)
{
    /* !!! FIXME: several converters can take advantage of SIMD, but only
       !!! FIXME:  if the data is aligned to 16 bytes. EnsureStreamBufferSize()
       !!! FIXME:  guarantees the buffer will align, but the
       !!! FIXME:  converters will iterate over the data backwards if
       !!! FIXME:  the output grows, and this means we won't align if buflen
       !!! FIXME:  isn't a multiple of 16. In these cases, we should chop off
       !!! FIXME:  a few samples at the end and convert them separately. */

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: wants to put %d preconverted bytes\n", buflen);
#endif

    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    if (!buf) {
        return SDL_InvalidParamError("buf");
    }
    if (len <= 0) {
        return 0; /* nothing to do. */
    }
    if ((len % stream->src_sample_frame_size) != 0) {
        return SDL_SetError("Can't add partial sample frames");
    }
#ifndef SDL_RESAMPLER_DISABLED
    if (!stream->cvt_before_resampling.needed &&
        !stream->resampling_needed &&
        !stream->cvt_after_resampling.needed) {
#else
    if (!stream->cvt_after_resampling.needed) {
#endif
#if DEBUG_AUDIOSTREAM
        SDL_Log("AUDIOSTREAM: no conversion needed at all, queueing %d bytes.\n", len);
#endif
        SDL_assert(stream->queue != NULL);
        return SDL_WriteToDataQueue(stream->queue, buf, len);
    }

    do {
#ifndef SDL_RESAMPLER_DISABLED
        int amount, retval;

        /* If we don't have a staging buffer or we're given enough data that
           we don't need to store it for later, skip the staging process.
         */
        if (!stream->staging_buffer_fill_len && len >= stream->staging_buffer_len) {
#endif
            return SDL_AudioStreamPutInternal(stream, buf, len, NULL);
#ifndef SDL_RESAMPLER_DISABLED
        }
        amount = (stream->staging_buffer_len - stream->staging_buffer_fill_len);
        /* If there's not enough data to fill the staging buffer, just save it */
        if (len < amount) {
            SDL_memcpy(stream->staging_buffer + stream->staging_buffer_fill_len, buf, len);
            stream->staging_buffer_fill_len += len;
            break;
        }

        /* Fill the staging buffer, process it, and continue */
        SDL_assert(amount > 0);
        SDL_memcpy(stream->staging_buffer + stream->staging_buffer_fill_len, buf, amount);
        stream->staging_buffer_fill_len = 0;
        retval = SDL_AudioStreamPutInternal(stream, stream->staging_buffer, stream->staging_buffer_len, NULL);
        if (retval < 0) {
            return retval;
        }
        buf = (void *)((Uint8 *)buf + amount);
        len -= amount;
#endif
    } while (len > 0);
    return 0;
}

int SDL_AudioStreamFlush(SDL_AudioStream *stream)
{
#ifndef SDL_RESAMPLER_DISABLED
    int fill_len, retval;
    SDL_boolean first_run;

    if (!stream) {
        return SDL_InvalidParamError("stream");
    }

#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: flushing! staging_buffer_fill_len=%d bytes\n", stream->staging_buffer_fill_len);
#endif

    first_run = stream->first_run;
    fill_len = stream->staging_buffer_fill_len;
    /* shouldn't use a staging buffer if we're not resampling. */
    SDL_assert(stream->resampling_needed || (fill_len == 0 && first_run));
    if (!first_run || fill_len > 0) {
        /* push the staging buffer + silence. We need to flush out not just
           the staging buffer, but the piece that the stream was saving off
           for right-side resampler padding. */
        int actual_input_steps = 0;
        if (fill_len > 0) {
            actual_input_steps += fill_len / stream->src_sample_frame_size;
        }
        if (!first_run) {
            actual_input_steps += stream->resampler_padding_len / (stream->pre_resample_channels * sizeof(float));
        }

        SDL_assert(actual_input_steps != 0);
        { // if (actual_input_steps > 0) {
            /* This is how many bytes we're expecting without silence appended. */
            const int inrate = stream->src_rate;
            const int outrate = stream->dst_rate;
            const int outframes = (int)((Sint64)actual_input_steps * outrate / inrate);
            int flush_remaining = outframes * stream->dst_sample_frame_size;

#if DEBUG_AUDIOSTREAM
            SDL_Log("AUDIOSTREAM: flushing with padding to get max %d bytes!\n", flush_remaining);
#endif
            SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
            SDL_memset(stream->staging_buffer + fill_len, '\0', stream->staging_buffer_len - fill_len);
            retval = SDL_AudioStreamPutInternal(stream, stream->staging_buffer, stream->staging_buffer_len, &flush_remaining);
            if (retval < 0) {
                return retval;
            }

            /* we have flushed out (or initially filled) the pending right-side
               resampler padding, but we need to push more silence to guarantee
               the staging buffer is fully flushed out, too. */
            SDL_assert(SDL_SilenceValueForFormat(AUDIO_F32SYS) == 0);
            SDL_memset(stream->staging_buffer, '\0', fill_len);
            retval = SDL_AudioStreamPutInternal(stream, stream->staging_buffer, stream->staging_buffer_len, &flush_remaining);
            if (retval < 0) {
                return retval;
            }
        }

        stream->staging_buffer_fill_len = 0;
        stream->first_run = SDL_TRUE;
        stream->reset_resampler_func(stream);
    }

#endif // SDL_RESAMPLER_DISABLED
    return 0;
}

/* get converted/resampled data from the stream */
int SDL_AudioStreamGet(SDL_AudioStream *stream, void *buf, int len)
{
    if (!stream) {
        return SDL_InvalidParamError("stream");
    }
    if (!buf) {
        return SDL_InvalidParamError("buf");
    }
    if (len <= 0) {
        return 0; /* nothing to do. */
    }
    if ((len % stream->dst_sample_frame_size) != 0) {
        return SDL_SetError("Can't request partial sample frames");
    }
    SDL_assert(stream->queue != NULL);
    return (int)SDL_ReadFromDataQueue(stream->queue, buf, len);
}

int SDL_PrivateAudioStreamGet(SDL_AudioStream *stream, void *buf, int len)
{
#if DEBUG_AUDIOSTREAM
    SDL_Log("AUDIOSTREAM: want to get %d converted bytes\n", len);
#endif
    SDL_assert(stream->queue != NULL);
    return (int)SDL_ReadFromDataQueue(stream->queue, buf, len);
}

/* number of converted/resampled bytes available */
int SDL_AudioStreamAvailable(SDL_AudioStream *stream)
{
    SDL_assert(stream == NULL || stream->queue != NULL);
    return stream ? (int)SDL_CountDataQueue(stream->queue) : 0;
}

void SDL_AudioStreamClear(SDL_AudioStream *stream)
{
    if (!stream) {
        SDL_InvalidParamError("stream");
    } else {
        SDL_assert(stream->queue != NULL);
        SDL_ClearDataQueue(stream->queue, AUDIO_PACKET_LEN * 2);
#ifndef SDL_RESAMPLER_DISABLED
        if (stream->reset_resampler_func) {
            stream->reset_resampler_func(stream);
        }
        stream->first_run = SDL_TRUE;
        stream->staging_buffer_fill_len = 0;
#endif
    }
}

/* dispose of a stream */
void SDL_FreeAudioStream(SDL_AudioStream *stream)
{
    if (stream) {
#ifndef SDL_RESAMPLER_DISABLED
        if (stream->cleanup_resampler_func) {
            stream->cleanup_resampler_func(stream);
        }
#endif
        SDL_FreeDataQueue(stream->queue);
#ifndef SDL_RESAMPLER_DISABLED
        SDL_free(stream->staging_buffer);
        SDL_free(stream->resampler_padding);
#endif
        SDL_free(stream->work_buffer_base);
        SDL_free(stream);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
