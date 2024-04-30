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

#ifndef SDL_RESAMPLER_DISABLED

#include "SDL_audio.h"
#include "SDL_cpuinfo.h"

#include "SDL_audio_c.h"
#include "SDL_audioresampler.h"

#ifdef __SSE__
#define HAVE_SSE_INTRINSICS
#endif

/* Function pointers set to a CPU-specific implementation. */
SDL_AudioResampler SDL_Resampler_Mono = NULL;
SDL_AudioResampler SDL_Resampler_Stereo = NULL;
SDL_AudioResampler SDL_Resampler_Generic = NULL;

/* SDL's resampler uses a "bandlimited interpolation" algorithm:
     https://ccrma.stanford.edu/~jos/resample/ */

#include "SDL_audio_resampler_filter.h"

/*
 * @param channels: the number of channels in the audio data
 * @param inrate: the current frequency of the the audio data
 * @param outrate: the result frequency of the the audio data
 * @param inbuffer: the audio data (must have ResamplePadding(inrate, outrate) * framelen bytes long paddings on each side)
 * @param inframes: the number of samples (per channel) in the input audio data (without the paddings)
 * @param outbuffer: the output audio data (must be at least 'outframes * framelen' bytes long)
 * @param outframes: the number of samples (per channel) in the output audio data
 */
static int SDL_Resampler_Generic_Scalar(const Uint8 channels, const int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes)
{
    /* This function uses integer arithmetics to avoid precision loss caused
     * by large floating point numbers. For some operations, Sint32 or Sint64
     * are needed for the large number multiplications. The input integers are
     * assumed to be non-negative so that division rounds by truncation and
     * modulo is always non-negative. Note that the operator order is important
     * for these integer divisions. */
    const int chans = channels;
    float *dst = outbuffer;
    int i, chan, j;

    for (i = 0; i < outframes; i++) {
        float scales[2 * RESAMPLER_ZERO_CROSSINGS];
        int filt_idx;
        /* Calculating the following way avoids subtraction or modulo of large
         * floats which have low result precision.
         *   interpolation1
         * = (i / outrate * inrate) - floor(i / outrate * inrate)
         * = mod(i / outrate * inrate, 1)
         * = mod(i * inrate, outrate) / outrate */
        const Sint64 pos = (Sint64)i * inrate;
        const int srcindex = (int)(pos / outrate);
        const int srcfraction = (int)(pos % outrate);
        const float interpolation1 = ((float)srcfraction) / ((float)outrate);
        const int filterindex1 = (srcfraction * RESAMPLER_SAMPLES_PER_ZERO_CROSSING / outrate) * RESAMPLER_ZERO_CROSSINGS; // [0.. RESAMPLER_SAMPLES_PER_ZERO_CROSSING) * RESAMPLER_ZERO_CROSSINGS
        const float interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (RESAMPLER_SAMPLES_PER_ZERO_CROSSING - 1) * RESAMPLER_ZERO_CROSSINGS - filterindex1; // [0.. RESAMPLER_SAMPLES_PER_ZERO_CROSSING) * RESAMPLER_ZERO_CROSSINGS
        const float *src = &inbuffer[srcindex * chans];
        // shift to the first sample
        const int filterindex0 = filterindex1 + (RESAMPLER_ZERO_CROSSINGS - 1);
        src -= (RESAMPLER_ZERO_CROSSINGS - 1) * chans;

        /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
        for (j = 0, filt_idx = filterindex0; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx--) {
            scales[j] = ResamplerFilter[filt_idx] + (interpolation1 * ResamplerFilterDifference[filt_idx]);
        }

        /* Do the right wing! */
        for (j = 0, filt_idx = filterindex2; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx++) {
            scales[j + RESAMPLER_ZERO_CROSSINGS] = ResamplerFilter[filt_idx] + (interpolation2 * ResamplerFilterDifference[filt_idx]);
        }

        for (chan = 0; chan < chans; chan++) {
            float outsample = 0.0f;
            // int filt_idx;
            const float *s = src;

            /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
//            for (j = 0, filt_idx = filterindex0; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx--) {
//                const float insample = *s;
//                outsample += (float) (insample * (ResamplerFilter[filt_idx] + (interpolation1 * ResamplerFilterDifference[filt_idx])));
//                s += chans;
//            }

            /* Do the right wing! */
//            for (j = 0, filt_idx = filterindex2; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx++) {
//                const float insample = *s;
//                outsample += (float) (insample * (ResamplerFilter[filt_idx] + (interpolation2 * ResamplerFilterDifference[filt_idx])));
//                s += chans;
//            }

            for (j = 0; j < 2 * RESAMPLER_ZERO_CROSSINGS; j++) {
                const float insample = *s;
                outsample += insample * scales[j];
                s += chans;
            }

            *(dst++) = outsample;
            src++;
        }
    }

    return outframes * chans * sizeof(float);
}

static int SDL_Resampler_Mono_Scalar(const Uint8 channels, const int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes)
{
    /* This function uses integer arithmetics to avoid precision loss caused
     * by large floating point numbers. For some operations, Sint32 or Sint64
     * are needed for the large number multiplications. The input integers are
     * assumed to be non-negative so that division rounds by truncation and
     * modulo is always non-negative. Note that the operator order is important
     * for these integer divisions. */
    const int chans = 1;
    float *dst = outbuffer;
    int i, j;

    for (i = 0; i < outframes; i++) {
        int filt_idx;
        /* Calculating the following way avoids subtraction or modulo of large
         * floats which have low result precision.
         *   interpolation1
         * = (i / outrate * inrate) - floor(i / outrate * inrate)
         * = mod(i / outrate * inrate, 1)
         * = mod(i * inrate, outrate) / outrate */
        const Sint64 pos = (Sint64)i * inrate;
        const int srcindex = (int)(pos / outrate);
        const int srcfraction = (int)(pos % outrate);
        const float interpolation1 = ((float)srcfraction) / ((float)outrate);
        const int filterindex1 = (srcfraction * RESAMPLER_SAMPLES_PER_ZERO_CROSSING / outrate) * RESAMPLER_ZERO_CROSSINGS;
        const float interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (RESAMPLER_SAMPLES_PER_ZERO_CROSSING - 1) * RESAMPLER_ZERO_CROSSINGS - filterindex1;
        const float *src = &inbuffer[srcindex * chans];
        // shift to the first sample
        const int filterindex0 = filterindex1 + (RESAMPLER_ZERO_CROSSINGS - 1);
        src -= (RESAMPLER_ZERO_CROSSINGS - 1) * chans;

        {
            float outsample = 0.0f;
            // int filt_idx;
            const float *s = src;

            /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
            for (j = 0, filt_idx = filterindex0; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx--) {
                const float insample = *s;
                outsample += (float) (insample * (ResamplerFilter[filt_idx] + (interpolation1 * ResamplerFilterDifference[filt_idx])));
                s += chans;
            }

            /* Do the right wing! */
            for (j = 0, filt_idx = filterindex2; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx++) {
                const float insample = *s;
                outsample += (float) (insample * (ResamplerFilter[filt_idx] + (interpolation2 * ResamplerFilterDifference[filt_idx])));
                s += chans;
            }

            *(dst++) = outsample;
            src++;
        }
    }

    return outframes * chans * sizeof(float);
}

static int SDL_Resampler_Stereo_Scalar(const Uint8 channels, const int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes)
{
    /* This function uses integer arithmetics to avoid precision loss caused
     * by large floating point numbers. For some operations, Sint32 or Sint64
     * are needed for the large number multiplications. The input integers are
     * assumed to be non-negative so that division rounds by truncation and
     * modulo is always non-negative. Note that the operator order is important
     * for these integer divisions. */
    const int chans = 2;
    float *dst = outbuffer;
    int i, j;

    for (i = 0; i < outframes; i++) {
        /* Calculating the following way avoids subtraction or modulo of large
         * floats which have low result precision.
         *   interpolation1
         * = (i / outrate * inrate) - floor(i / outrate * inrate)
         * = mod(i / outrate * inrate, 1)
         * = mod(i * inrate, outrate) / outrate */
        const Sint64 pos = (Sint64)i * inrate;
        const int srcindex = (int)(pos / outrate);
        const int srcfraction = (int)(pos % outrate);
        const float interpolation1 = ((float)srcfraction) / ((float)outrate);
        const int filterindex1 = (srcfraction * RESAMPLER_SAMPLES_PER_ZERO_CROSSING / outrate) * RESAMPLER_ZERO_CROSSINGS;
        const float interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (RESAMPLER_SAMPLES_PER_ZERO_CROSSING - 1) * RESAMPLER_ZERO_CROSSINGS - filterindex1;
        const float *src = &inbuffer[srcindex * chans];
        // shift to the first sample
        const int filterindex0 = filterindex1 + (RESAMPLER_ZERO_CROSSINGS - 1);
        src -= (RESAMPLER_ZERO_CROSSINGS - 1) * chans;

        {
            float outsample0 = 0.0f;
            float outsample1 = 0.0f;
            int filt_idx;
            const float *s = src;

            /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
            for (j = 0, filt_idx = filterindex0; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx--) {
                const float insample0 = s[0];
                const float insample1 = s[1];
                const float w = ResamplerFilter[filt_idx] + (interpolation1 * ResamplerFilterDifference[filt_idx]);
                outsample0 += insample0 * w;
                outsample1 += insample1 * w;
                s += chans;
            }

            /* Do the right wing! */
            for (j = 0, filt_idx = filterindex2; j < RESAMPLER_ZERO_CROSSINGS; j++, filt_idx++) {
                const float insample0 = s[0];
                const float insample1 = s[1];
                const float w = ResamplerFilter[filt_idx] + (interpolation2 * ResamplerFilterDifference[filt_idx]);
                outsample0 += insample0 * w;
                outsample1 += insample1 * w;
                s += chans;
            }

            *(dst++) = outsample0;
            *(dst++) = outsample1;
            src += chans;
        }
    }

    return outframes * chans * sizeof(float);
}

#ifdef HAVE_SSE_INTRINSICS
static int SDL_Resampler_Generic_SSE(const Uint8 channels, const int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes)
{
    const int chans = channels;
    float *dst = outbuffer;
    int i, chan, j;

    for (i = 0; i < outframes; i++) {
        float scales[2 * RESAMPLER_ZERO_CROSSINGS];
        const Sint64 pos = (Sint64)i * inrate;
        const int srcindex = (int)(pos / outrate);
        const int srcfraction = (int)(pos % outrate);
        const float interpolation1 = ((float)srcfraction) / ((float)outrate);
        const int filterindex1 = (srcfraction * RESAMPLER_SAMPLES_PER_ZERO_CROSSING / outrate) * RESAMPLER_ZERO_CROSSINGS;
        const float interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (RESAMPLER_SAMPLES_PER_ZERO_CROSSING - 1) * RESAMPLER_ZERO_CROSSINGS - filterindex1;
        const float *src = &inbuffer[srcindex * chans];
        // shift to the first sample
        src -= (RESAMPLER_ZERO_CROSSINGS - 1) * chans;

        { /* do this twice to calculate the sample, once for the "left wing" and then same for the right. */
            __m128 filt1 = _mm_loadu_ps(&ResamplerFilter[filterindex1]);
            __m128 filt_diff1 = _mm_loadu_ps(&ResamplerFilterDifference[filterindex1]);
            __m128 ipol1 = _mm_set_ps1(interpolation1);
            __m128 filt_diff_ipol1 = _mm_mul_ps(filt_diff1, ipol1);
            __m128 scale1_back = _mm_add_ps(filt1, filt_diff_ipol1);
            __m128 scale1 = _mm_shuffle_ps(scale1_back, scale1_back, _MM_SHUFFLE(0, 1, 2, 3));
            _mm_store_ps(&scales[0], scale1);
        }

        { /* Do the right wing! */
            __m128 filt2 = _mm_loadu_ps(&ResamplerFilter[filterindex2]);
            __m128 filt_diff2 = _mm_loadu_ps(&ResamplerFilterDifference[filterindex2]);
            __m128 ipol2 = _mm_set_ps1(interpolation2);
            __m128 filt_diff_ipol2 = _mm_mul_ps(filt_diff2, ipol2);
            __m128 scale2 = _mm_add_ps(filt2, filt_diff_ipol2);
            _mm_store_ps(&scales[RESAMPLER_ZERO_CROSSINGS], scale2);
        }

        for (chan = 0; chan < chans; chan++) {
            float outsample = 0.0f;
            const float *s = src;

            for (j = 0; j < 2 * RESAMPLER_ZERO_CROSSINGS; j++) {
                const float insample = *s;
                outsample += insample * scales[j];
                s += chans;
            }

            *(dst++) = outsample;
            src++;
        }
    }

    return outframes * chans * sizeof(float);
}

static int SDL_Resampler_Mono_SSE(const Uint8 channels, const int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes)
{
    /* This function uses integer arithmetics to avoid precision loss caused
     * by large floating point numbers. For some operations, Sint32 or Sint64
     * are needed for the large number multiplications. The input integers are
     * assumed to be non-negative so that division rounds by truncation and
     * modulo is always non-negative. Note that the operator order is important
     * for these integer divisions. */
    const int chans = 1;
    float *dst = outbuffer;
    int i;

    for (i = 0; i < outframes; i++) {
        /* Calculating the following way avoids subtraction or modulo of large
         * floats which have low result precision.
         *   interpolation1
         * = (i / outrate * inrate) - floor(i / outrate * inrate)
         * = mod(i / outrate * inrate, 1)
         * = mod(i * inrate, outrate) / outrate */
        const Sint64 pos = (Sint64)i * inrate;
        const int srcindex = (int)(pos / outrate);
        const int srcfraction = (int)(pos % outrate);
        const float interpolation1 = ((float)srcfraction) / ((float)outrate);
        const int filterindex1 = (srcfraction * RESAMPLER_SAMPLES_PER_ZERO_CROSSING / outrate) * RESAMPLER_ZERO_CROSSINGS;
        const float interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (RESAMPLER_SAMPLES_PER_ZERO_CROSSING - 1) * RESAMPLER_ZERO_CROSSINGS - filterindex1;
        const float *src = &inbuffer[srcindex * chans];
        // shift to the first sample
        src -= (RESAMPLER_ZERO_CROSSINGS - 1) * chans;

        {
            __m128 filt1 = _mm_loadu_ps(&ResamplerFilter[filterindex1]);
            __m128 filt_diff1 = _mm_loadu_ps(&ResamplerFilterDifference[filterindex1]);
            __m128 ipol1 = _mm_set_ps1(interpolation1);
            __m128 filt_diff_ipol1 = _mm_mul_ps(filt_diff1, ipol1);
            __m128 scale1 = _mm_add_ps(filt1, filt_diff_ipol1);

            __m128 filt2 = _mm_loadu_ps(&ResamplerFilter[filterindex2]);
            __m128 filt_diff2 = _mm_loadu_ps(&ResamplerFilterDifference[filterindex2]);
            __m128 ipol2 = _mm_set_ps1(interpolation2);
            __m128 filt_diff_ipol2 = _mm_mul_ps(filt_diff2, ipol2);
            __m128 scale2 = _mm_add_ps(filt2, filt_diff_ipol2);

            __m128 inleft_back = _mm_loadu_ps(src);
            __m128 inleft = _mm_shuffle_ps(inleft_back, inleft_back, _MM_SHUFFLE(0, 1, 2, 3));
            __m128 inright = _mm_loadu_ps(src + 4);
            __m128 outsample = _mm_add_ps(_mm_mul_ps(inleft, scale1), _mm_mul_ps(inright, scale2));
#if 0
            __m128 sums = _mm_hadd_ps(outsample, outsample);
            sums        = _mm_hadd_ps(sums, sums);
#else
            __m128 shuf = _mm_shuffle_ps(outsample, outsample, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums = _mm_add_ps(outsample, shuf);
            shuf        = _mm_movehl_ps(shuf, sums);
            sums        = _mm_add_ss(sums, shuf);
#endif

            _mm_store_ss(dst, sums);

            dst++;
            src++;
        }
    }

    return outframes * chans * sizeof(float);
}

static int SDL_Resampler_Stereo_SSE(const Uint8 channels, const int inrate, const int outrate,
                             const float *inbuffer, const int inframes,
                             float *outbuffer, const int outframes)
{
    const int chans = 2;
    float *dst = outbuffer;
    int i;

    for (i = 0; i < outframes; i++) {
        const Sint64 pos = (Sint64)i * inrate;
        const int srcindex = (int)(pos / outrate);
        const int srcfraction = (int)(pos % outrate);
        const float interpolation1 = ((float)srcfraction) / ((float)outrate);
        const int filterindex1 = (srcfraction * RESAMPLER_SAMPLES_PER_ZERO_CROSSING / outrate) * RESAMPLER_ZERO_CROSSINGS;
        const float interpolation2 = 1.0f - interpolation1;
        const int filterindex2 = (RESAMPLER_SAMPLES_PER_ZERO_CROSSING - 1) * RESAMPLER_ZERO_CROSSINGS - filterindex1;
        const float *src = &inbuffer[srcindex * chans];
        // shift to the first sample
        src -= (RESAMPLER_ZERO_CROSSINGS - 1) * chans;

        {
            __m128 filt1 = _mm_loadu_ps(&ResamplerFilter[filterindex1]);
            __m128 filt_diff1 = _mm_loadu_ps(&ResamplerFilterDifference[filterindex1]);
            __m128 ipol1 = _mm_set_ps1(interpolation1);
            __m128 filt_diff_ipol1 = _mm_mul_ps(filt_diff1, ipol1);
            __m128 scale1_back = _mm_add_ps(filt1, filt_diff_ipol1);
            __m128 scale1 = _mm_shuffle_ps(scale1_back, scale1_back, _MM_SHUFFLE(0, 1, 2, 3));

            __m128 filt2 = _mm_loadu_ps(&ResamplerFilter[filterindex2]);
            __m128 filt_diff2 = _mm_loadu_ps(&ResamplerFilterDifference[filterindex2]);
            __m128 ipol2 = _mm_set_ps1(interpolation2);
            __m128 filt_diff_ipol2 = _mm_mul_ps(filt_diff2, ipol2);
            __m128 scale2 = _mm_add_ps(filt2, filt_diff_ipol2);

            __m128 inleft00 = _mm_loadu_ps(src);
            __m128 inleft10 = _mm_loadu_ps(src + 4);
            __m128 inright00 = _mm_loadu_ps(src + 8);
            __m128 inright10 = _mm_loadu_ps(src + 12);

            __m128 inleft00_ = _mm_shuffle_ps(inleft00, inleft00, _MM_SHUFFLE(3, 1, 2, 0));
            __m128 inleft10_ = _mm_shuffle_ps(inleft10, inleft10, _MM_SHUFFLE(3, 1, 2, 0));
            __m128 inright00_ = _mm_shuffle_ps(inright00, inright00, _MM_SHUFFLE(3, 1, 2, 0));
            __m128 inright10_ = _mm_shuffle_ps(inright10, inright10, _MM_SHUFFLE(3, 1, 2, 0));

            __m128 inleft0 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(inleft00_), _mm_castps_pd(inleft10_)));
            __m128 inleft1 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(inleft00_), _mm_castps_pd(inleft10_)));
            __m128 inright0 = _mm_castpd_ps(_mm_unpacklo_pd(_mm_castps_pd(inright00_), _mm_castps_pd(inright10_)));
            __m128 inright1 = _mm_castpd_ps(_mm_unpackhi_pd(_mm_castps_pd(inright00_), _mm_castps_pd(inright10_)));

            __m128 outsample0 = _mm_add_ps(_mm_mul_ps(inleft0, scale1), _mm_mul_ps(inright0, scale2));
            __m128 outsample1 = _mm_add_ps(_mm_mul_ps(inleft1, scale1), _mm_mul_ps(inright1, scale2));
#if 0
            __m128 sums0 = _mm_hadd_ps(outsample0, outsample0);
            __m128 sums1 = _mm_hadd_ps(outsample1, outsample1);
            sums0        = _mm_hadd_ps(sums0, sums0);
            sums1        = _mm_hadd_ps(sums1, sums1);
#else
            __m128 shuf0 = _mm_shuffle_ps(outsample0, outsample0, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums0 = _mm_add_ps(outsample0, shuf0);
            __m128 shuf1 = _mm_shuffle_ps(outsample1, outsample1, _MM_SHUFFLE(2, 3, 0, 1));
            __m128 sums1 = _mm_add_ps(outsample1, shuf1);
            shuf0        = _mm_movehl_ps(shuf0, sums0);
            sums0        = _mm_add_ss(sums0, shuf0);
            shuf1        = _mm_movehl_ps(shuf1, sums1);
            sums1        = _mm_add_ss(sums1, shuf1);
#endif

            _mm_store_ss(dst, sums0);
            _mm_store_ss(dst + 1, sums1);

            dst += chans;
            src += chans;
        }
    }

    return outframes * chans * sizeof(float);
}
#endif // HAVE_SSE_INTRINSICS

void SDL_ChooseAudioResamplers(void)
{
    SDL_assert(SDL_Resampler_Mono == NULL);

#define SET_RESAMPLER_FUNCS(fntype)                         \
    SDL_Resampler_Mono = SDL_Resampler_Mono_##fntype;       \
    SDL_Resampler_Stereo = SDL_Resampler_Stereo_##fntype;   \
    SDL_Resampler_Generic = SDL_Resampler_Generic_##fntype; \

#ifdef HAVE_SSE_INTRINSICS
    if (SDL_HasSSE()) {
        SET_RESAMPLER_FUNCS(SSE);
        return;
    }
#endif

    SET_RESAMPLER_FUNCS(Scalar);

#undef SET_RESAMPLER_FUNCS

    SDL_assert(SDL_Resampler_Mono != NULL);
}

int ResamplerPadding(const int inrate, const int outrate)
{
    /* This function uses integer arithmetics to avoid precision loss caused
     * by large floating point numbers. Sint32 is needed for the large number
     * multiplication. The integers are assumed to be non-negative so that
     * division rounds by truncation. */
    SDL_assert(inrate != outrate);

    if (inrate > outrate) {
        SDL_assert(inrate <= (SDL_INT_MAX - (outrate - 1)) / RESAMPLER_SAMPLES_PER_ZERO_CROSSING);
        return (RESAMPLER_SAMPLES_PER_ZERO_CROSSING * inrate + outrate - 1) / outrate;
    }
    return RESAMPLER_SAMPLES_PER_ZERO_CROSSING;
}

#endif // !SDL_RESAMPLER_DISABLED

/* vi: set ts=4 sw=4 expandtab: */
