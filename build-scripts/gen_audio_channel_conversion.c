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

#include <stdio.h>

/*

Built with:

gcc -o genchancvt build-scripts/gen_audio_channel_conversion.c -lm && ./genchancvt > src/audio/SDL_audio_channel_converters.h

*/

#define NUM_CHANNELS 8

static const char *layout_names[NUM_CHANNELS] = {
    "Mono", "Stereo", "2.1", "Quad", "4.1", "5.1", "6.1", "7.1"
};

static const char *channel_names[NUM_CHANNELS][NUM_CHANNELS] = {
    /* mono */   { "FC" },
    /* stereo */ { "FL", "FR" },
    /* 2.1 */    { "FL", "FR", "LFE" },
    /* quad */   { "FL", "FR", "BL", "BR" },
    /* 4.1 */    { "FL", "FR", "LFE", "BL", "BR" },
    /* 5.1 */    { "FL", "FR", "FC", "LFE", "BL", "BR" },
    /* 6.1 */    { "FL", "FR", "FC", "LFE", "BC", "SL", "SR" },
    /* 7.1 */    { "FL", "FR", "FC", "LFE", "BL", "BR", "SL", "SR" },
};


/*
 * This table is from FAudio:
 *
 *  https://raw.githubusercontent.com/FNA-XNA/FAudio/master/src/matrix_defaults.inl
 */
static const float channel_conversion_matrix[NUM_CHANNELS][NUM_CHANNELS][64] = {
{
	/* 1 x 1 */
	{ 1.000000000f },
	/* 1 x 2 */
	{ 1.000000000f, 1.000000000f },
	/* 1 x 3 */
	{ 1.000000000f, 1.000000000f, 0.000000000f },
	/* 1 x 4 */
	{ 1.000000000f, 1.000000000f, 0.000000000f, 0.000000000f },
	/* 1 x 5 */
	{ 1.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 1 x 6 */
	{ 1.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 1 x 7 */
	{ 1.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 1 x 8 */
	{ 1.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f }
},
{
	/* 2 x 1 */
	{ 0.500000000f, 0.500000000f },
	/* 2 x 2 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 2 x 3 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f },
	/* 2 x 4 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 2 x 5 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 2 x 6 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 2 x 7 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 2 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f }
},
{
	/* 3 x 1 */
	{ 0.333333343f, 0.333333343f, 0.333333343f },
	/* 3 x 2 */
	{ 0.800000012f, 0.000000000f, 0.200000003f, 0.000000000f, 0.800000012f, 0.200000003f },
	/* 3 x 3 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 3 x 4 */
	{ 0.888888896f, 0.000000000f, 0.111111112f, 0.000000000f, 0.888888896f, 0.111111112f, 0.000000000f, 0.000000000f, 0.111111112f, 0.000000000f, 0.000000000f, 0.111111112f },
	/* 3 x 5 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 3 x 6 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 3 x 7 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 3 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f }
},
{
	/* 4 x 1 */
	{ 0.250000000f, 0.250000000f, 0.250000000f, 0.250000000f },
	/* 4 x 2 */
	{ 0.421000004f, 0.000000000f, 0.358999997f, 0.219999999f, 0.000000000f, 0.421000004f, 0.219999999f, 0.358999997f },
	/* 4 x 3 */
	{ 0.421000004f, 0.000000000f, 0.358999997f, 0.219999999f, 0.000000000f, 0.421000004f, 0.219999999f, 0.358999997f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 4 x 4 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 4 x 5 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 4 x 6 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 4 x 7 */
	{ 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.500000000f, 0.500000000f, 0.000000000f, 0.000000000f, 0.796000004f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.796000004f },
	/* 4 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f }
},
{
	/* 5 x 1 */
	{ 0.200000003f, 0.200000003f, 0.200000003f, 0.200000003f, 0.200000003f },
	/* 5 x 2 */
	{ 0.374222219f, 0.000000000f, 0.111111112f, 0.319111109f, 0.195555553f, 0.000000000f, 0.374222219f, 0.111111112f, 0.195555553f, 0.319111109f },
	/* 5 x 3 */
	{ 0.421000004f, 0.000000000f, 0.000000000f, 0.358999997f, 0.219999999f, 0.000000000f, 0.421000004f, 0.000000000f, 0.219999999f, 0.358999997f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f },
	/* 5 x 4 */
	{ 0.941176474f, 0.000000000f, 0.058823530f, 0.000000000f, 0.000000000f, 0.000000000f, 0.941176474f, 0.058823530f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.058823530f, 0.941176474f, 0.000000000f, 0.000000000f, 0.000000000f, 0.058823530f, 0.000000000f, 0.941176474f },
	/* 5 x 5 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 5 x 6 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 5 x 7 */
	{ 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.500000000f, 0.500000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.796000004f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.796000004f },
	/* 5 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f }
},
{
	/* 6 x 1 */
	{ 0.166666672f, 0.166666672f, 0.166666672f, 0.166666672f, 0.166666672f, 0.166666672f },
	/* 6 x 2 */
	{ 0.294545442f, 0.000000000f, 0.208181813f, 0.090909094f, 0.251818180f, 0.154545456f, 0.000000000f, 0.294545442f, 0.208181813f, 0.090909094f, 0.154545456f, 0.251818180f },
	/* 6 x 3 */
	{ 0.324000001f, 0.000000000f, 0.229000002f, 0.000000000f, 0.277000010f, 0.170000002f, 0.000000000f, 0.324000001f, 0.229000002f, 0.000000000f, 0.170000002f, 0.277000010f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f },
	/* 6 x 4 */
	{ 0.558095276f, 0.000000000f, 0.394285709f, 0.047619049f, 0.000000000f, 0.000000000f, 0.000000000f, 0.558095276f, 0.394285709f, 0.047619049f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.047619049f, 0.558095276f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.047619049f, 0.000000000f, 0.558095276f },
	/* 6 x 5 */
	{ 0.586000025f, 0.000000000f, 0.414000005f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.586000025f, 0.414000005f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.586000025f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.586000025f },
	/* 6 x 6 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 6 x 7 */
	{ 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.939999998f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.500000000f, 0.500000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.796000004f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.796000004f },
	/* 6 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f }
},
{
	/* 7 x 1 */
	{ 0.143142849f, 0.143142849f, 0.143142849f, 0.142857149f, 0.143142849f, 0.143142849f, 0.143142849f },
	/* 7 x 2 */
	{ 0.247384623f, 0.000000000f, 0.174461529f, 0.076923080f, 0.174461529f, 0.226153851f, 0.100615382f, 0.000000000f, 0.247384623f, 0.174461529f, 0.076923080f, 0.174461529f, 0.100615382f, 0.226153851f },
	/* 7 x 3 */
	{ 0.268000007f, 0.000000000f, 0.188999996f, 0.000000000f, 0.188999996f, 0.245000005f, 0.108999997f, 0.000000000f, 0.268000007f, 0.188999996f, 0.000000000f, 0.188999996f, 0.108999997f, 0.245000005f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 7 x 4 */
	{ 0.463679999f, 0.000000000f, 0.327360004f, 0.040000003f, 0.000000000f, 0.168960005f, 0.000000000f, 0.000000000f, 0.463679999f, 0.327360004f, 0.040000003f, 0.000000000f, 0.000000000f, 0.168960005f, 0.000000000f, 0.000000000f, 0.000000000f, 0.040000003f, 0.327360004f, 0.431039989f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.040000003f, 0.327360004f, 0.000000000f, 0.431039989f },
	/* 7 x 5 */
	{ 0.483000010f, 0.000000000f, 0.340999991f, 0.000000000f, 0.000000000f, 0.175999999f, 0.000000000f, 0.000000000f, 0.483000010f, 0.340999991f, 0.000000000f, 0.000000000f, 0.000000000f, 0.175999999f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.340999991f, 0.449000001f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.340999991f, 0.000000000f, 0.449000001f },
	/* 7 x 6 */
	{ 0.611000001f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.223000005f, 0.000000000f, 0.000000000f, 0.611000001f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.223000005f, 0.000000000f, 0.000000000f, 0.611000001f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.432000011f, 0.568000019f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.432000011f, 0.000000000f, 0.568000019f },
	/* 7 x 7 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f },
	/* 7 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.707000017f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.707000017f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f }
},
{
	/* 8 x 1 */
	{ 0.125125006f, 0.125125006f, 0.125125006f, 0.125000000f, 0.125125006f, 0.125125006f, 0.125125006f, 0.125125006f },
	/* 8 x 2 */
	{ 0.211866662f, 0.000000000f, 0.150266662f, 0.066666670f, 0.181066677f, 0.111066669f, 0.194133341f, 0.085866667f, 0.000000000f, 0.211866662f, 0.150266662f, 0.066666670f, 0.111066669f, 0.181066677f, 0.085866667f, 0.194133341f },
	/* 8 x 3 */
	{ 0.226999998f, 0.000000000f, 0.160999998f, 0.000000000f, 0.194000006f, 0.119000003f, 0.208000004f, 0.092000000f, 0.000000000f, 0.226999998f, 0.160999998f, 0.000000000f, 0.119000003f, 0.194000006f, 0.092000000f, 0.208000004f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f },
	/* 8 x 4 */
	{ 0.466344833f, 0.000000000f, 0.329241365f, 0.034482758f, 0.000000000f, 0.000000000f, 0.169931039f, 0.000000000f, 0.000000000f, 0.466344833f, 0.329241365f, 0.034482758f, 0.000000000f, 0.000000000f, 0.000000000f, 0.169931039f, 0.000000000f, 0.000000000f, 0.000000000f, 0.034482758f, 0.466344833f, 0.000000000f, 0.433517247f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.034482758f, 0.000000000f, 0.466344833f, 0.000000000f, 0.433517247f },
	/* 8 x 5 */
	{ 0.483000010f, 0.000000000f, 0.340999991f, 0.000000000f, 0.000000000f, 0.000000000f, 0.175999999f, 0.000000000f, 0.000000000f, 0.483000010f, 0.340999991f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.175999999f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.483000010f, 0.000000000f, 0.449000001f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.483000010f, 0.000000000f, 0.449000001f },
	/* 8 x 6 */
	{ 0.518000007f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.188999996f, 0.000000000f, 0.000000000f, 0.518000007f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.188999996f, 0.000000000f, 0.000000000f, 0.518000007f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.518000007f, 0.000000000f, 0.481999993f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.518000007f, 0.000000000f, 0.481999993f },
	/* 8 x 7 */
	{ 0.541000009f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.541000009f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.541000009f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.287999988f, 0.287999988f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.458999991f, 0.000000000f, 0.541000009f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.458999991f, 0.000000000f, 0.541000009f },
	/* 8 x 8 */
	{ 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f }
}
};

static char *remove_dots(const char *str)  /* this is NOT robust. */
{
    static char retval1[32];
    static char retval2[32];
    static int idx = 0;
    char *retval = (idx++ & 1) ? retval1 : retval2;
    char *ptr = retval;
    while (*str) {
        if (*str != '.') {
            *(ptr++) = *str;
        }
        str++;
    }
    *ptr = '\0';
    return retval;
}

static char *lowercase(const char *str)  /* this is NOT robust. */
{
    static char retval1[32];
    static char retval2[32];
    static int idx = 0;
    char *retval = (idx++ & 1) ? retval1 : retval2;
    char *ptr = retval;
    while (*str) {
        const char ch = *(str++);
        *(ptr++) = ((ch >= 'A') && (ch <= 'Z')) ? (ch - ('A' - 'a')) : ch;
    }
    *ptr = '\0';
    return retval;
}

static int stringlen(const char *str)
{
    int i;
    for (i = 0; str[i]; i++) {}
    return i;
}

static void write_converter(const int fromchans, const int tochans)
{
    const char *fromstr = layout_names[fromchans-1];
    const char *tostr = layout_names[tochans-1];
    const float *cvtmatrix = channel_conversion_matrix[fromchans-1][tochans-1];
    const float *fptr;
    const int convert_backwards = (tochans > fromchans);
    int input_channel_used[NUM_CHANNELS];
    int maxdstchlen = 0;
    int i, j;

    // if (tochans == fromchans) {
    //    return;  /* nothing to convert, don't generate a converter. */
    // }

    for (i = 0; i < fromchans; i++) {
        input_channel_used[i] = 0;
    }

    fptr = cvtmatrix;
    for (j = 0; j < tochans; j++) {
        for (i = 0; i < fromchans; i++) {
            #if 0
            printf("to=%d, from=%d, coeff=%f\n", j, i, *fptr);
            #endif
            if (*(fptr++) != 0.0f) {
                input_channel_used[i]++;
            }
        }
        const char* name = channel_names[tochans-1][j];
        i = stringlen(name);
        if (i > maxdstchlen) {
            maxdstchlen = i;
        }
    }

    printf("static void SDLCALL SDL_Convert%sTo%s(SDL_AudioCVT *cvt)\n"
           "{\n", remove_dots(fromstr), remove_dots(tostr));

    if (fromchans == 1) {
        printf("    const int num_samples = cvt->len_cvt / (unsigned)sizeof(float);\n");
    } else { // if ((fromchans & (fromchans - 1)) == 0) { // -- fromchans is power of two -> use unsigned division which is converted to a shift
        printf("    const int num_samples = cvt->len_cvt / (sizeof(float) * %du);\n", fromchans);
    // } else {
    //    printf("    const int num_samples = cvt->len_cvt / (sizeof(float) * %d);\n", fromchans);
    }
    if (convert_backwards) {  /* must convert backwards when growing the output in-place. */
        /*if (fromchans == 1) {
            printf("    float *dst = ((float *)(cvt->buf + (cvt->len_cvt * %d)));\n", tochans);
        } else */ if ((tochans % fromchans) == 0) {
            printf("    float *dst = (float *)(cvt->buf + (cvt->len_cvt * %d));\n", tochans / fromchans);
        } else { // if ((fromchans & (fromchans - 1)) == 0) { // -- fromchans is power of two -> use unsigned division which is converted to a shift
            printf("    float *dst = (float *)(cvt->buf + ((cvt->len_cvt / %du) * %d));\n", fromchans, tochans);
        // } else {
        //    printf("    float *dst = ((float *)(cvt->buf + ((cvt->len_cvt / %d) * %d)));\n", fromchans, tochans);
        }
        printf("    const float *src = (const float *)(cvt->buf + cvt->len_cvt);\n");
    } else {
        printf("    float *dst = (float *)cvt->buf;\n");
        printf("    const float *src = dst;\n");
    }

    printf("    int i = num_samples;\n"
           "\n"
           "    LOG_DEBUG_CONVERT(\"%s\", \"%s\");\n"
           "\n", lowercase(fromstr), lowercase(tostr));

    if ((fromchans > 1) && (tochans > 1)) {
        if (fromchans > tochans && (fromchans % tochans) == 0) { // -- fromchans is multiple of tochans -> use single division
            // if (((fromchans / tochans) & ((fromchans / tochans) - 1)) == 0) {
                printf("    cvt->len_cvt = cvt->len_cvt / %du;\n", (fromchans / tochans));
            // } else {
            //    printf("    cvt->len_cvt = (cvt->len_cvt / %d);\n", (fromchans / tochans));
            // }
        } else if (tochans > fromchans && (tochans % fromchans) == 0) { // -- tochans is multiple of fromchans -> use single multiplication
            printf("    cvt->len_cvt = cvt->len_cvt * %d;\n", (tochans / fromchans));
        } else { // if ((fromchans & (fromchans - 1)) == 0) { // -- fromchans is power of two -> use unsigned division which is converted to a shift
            printf("    cvt->len_cvt = (cvt->len_cvt / %du) * %d;\n", fromchans, tochans);
        // } else {
        //    printf("    cvt->len_cvt = (cvt->len_cvt / %d) * %d;\n", fromchans, tochans);
        }
    } else if (tochans == 1) {
        // if ((fromchans & (fromchans - 1)) == 0) {
            printf("    cvt->len_cvt = cvt->len_cvt / %du;\n", fromchans);
        // } else {
        //    printf("    cvt->len_cvt = cvt->len_cvt / %d;\n", fromchans);
        // }
    } else /* if (fromchans == 1) */ {
        printf("    cvt->len_cvt = cvt->len_cvt * %d;\n", tochans);
    }
    printf("\n");

    if (convert_backwards) {
        printf("    /* convert backwards, since output is growing in-place. */\n");
        printf("    for ( ; i; i--) {\n");
        printf("        src -= %d;\n", fromchans);
        printf("        dst -= %d;\n", tochans);
        printf("        {\n");
        fptr = cvtmatrix;
        for (i = 0; i < fromchans; i++) {
            if (input_channel_used[i] > 1) {  /* don't read it from src more than once. */
                printf("        const float src%s = src[%d];\n", channel_names[fromchans-1][i], i);
            }
        }

        for (j = tochans - 1; j >= 0; j--) {
            float coeff[NUM_CHANNELS];
            int groups[NUM_CHANNELS][NUM_CHANNELS] = { 0 };
            int num_groups = 0;
            const char* name = channel_names[tochans-1][j];
            fptr = cvtmatrix + (fromchans * j);
            printf("        dst[%d] /* %s", j, name);
            for (i = maxdstchlen - stringlen(name); i > 0; i--)
                printf(" ");
            printf(" */ =");
            for (i = fromchans - 1; i >= 0; i--) {
                const float coefficient = fptr[i];
                int idx, n;
                if (coefficient == 0.0f) {
                    continue;
                }
                for (idx = 0; idx < num_groups; idx++) {
                    if (coeff[idx] == coefficient) {
                        break;
                    }
                }
                for (n = 0; groups[idx][n] != 0; n++) { }
                groups[idx][n] = i + 1;
                if (idx == num_groups) {
                    coeff[idx] = coefficient;
                    num_groups++;
                }
            }

            if (num_groups != 0) {
                int idx, n;
                for (idx = 0; idx < num_groups; idx++) {
                    int multi = groups[idx][1] != 0;
                    int has_input = 0;

                    if (idx != 0) {
                        printf(" +");
                    }

                    for (n = 0; groups[idx][n] != 0; n++) {
                        i = groups[idx][n] - 1;
                        const float coefficient = fptr[i];
                        char srcname[32];
                        if (input_channel_used[i] > 1) {
                            snprintf(srcname, sizeof (srcname), "src%s", channel_names[fromchans-1][i]);
                        } else {
                            snprintf(srcname, sizeof (srcname), "src[%d]", i);
                        }

                        if (has_input) {
                            printf(" +");
                        } else if (multi) {
                            printf(" (");
                        }

                        has_input = 1;

                        if (coefficient == 1.0f || multi) {
                            printf(" %s", srcname);
                        } else {
                            printf(" (%s * %.9ff)", srcname, coefficient);
                        }
                    }

                    if (multi) {
                        printf(" ) * %.9ff", coeff[idx]);
                    }
                }
            } else {
                printf(" 0.0f");
            }

            printf(";\n");
        }
        printf("        }\n");

        printf("    }\n");
    } else {
        printf("    for ( ; i; i--, src += %d, dst += %d) {\n", fromchans, tochans);

        fptr = cvtmatrix;
        for (i = 0; i < fromchans; i++) {
            if (input_channel_used[i] > 1) {  /* don't read it from src more than once. */
                printf("        const float src%s = src[%d];\n", channel_names[fromchans-1][i], i);
            }
        }

        for (j = 0; j < tochans; j++) {
            float coeff[NUM_CHANNELS];
            int groups[NUM_CHANNELS][NUM_CHANNELS] = { 0 };
            int num_groups = 0;
            const char* name = channel_names[tochans-1][j];
            fptr = cvtmatrix + (fromchans * j);
            printf("        dst[%d] /* %s", j, name);
            for (i = maxdstchlen - stringlen(name); i > 0; i--)
                printf(" ");
            printf(" */ =");
            for (i = 0; i < fromchans; i++) {
                const float coefficient = fptr[i];
                int idx, n;
                if (coefficient == 0.0f) {
                    continue;
                }
                for (idx = 0; idx < num_groups; idx++) {
                    if (coeff[idx] == coefficient) {
                        break;
                    }
                }
                for (n = 0; groups[idx][n] != 0; n++) { }
                groups[idx][n] = i + 1;
                if (idx == num_groups) {
                    coeff[idx] = coefficient;
                    num_groups++;
                }
            }

            if (num_groups != 0) {
                int idx, n;
                for (idx = 0; idx < num_groups; idx++) {
                    int multi = groups[idx][1] != 0;
                    int has_input = 0;

                    if (idx != 0) {
                        printf(" +");
                    }

                    for (n = 0; groups[idx][n] != 0; n++) {
                        i = groups[idx][n] - 1;
                        const float coefficient = fptr[i];
                        char srcname[32];
                        if (input_channel_used[i] > 1) {
                            snprintf(srcname, sizeof (srcname), "src%s", channel_names[fromchans-1][i]);
                        } else {
                            snprintf(srcname, sizeof (srcname), "src[%d]", i);
                        }

                        if (has_input) {
                            printf(" +");
                        } else if (multi) {
                            printf(" (");
                        }

                        has_input = 1;

                        if (coefficient == 1.0f || multi) {
                            printf(" %s", srcname);
                        } else {
                            printf(" (%s * %.9ff)", srcname, coefficient);
                        }
                    }

                    if (multi) {
                        printf(" ) * %.9ff", coeff[idx]);
                    }
                }
            } else {
                printf(" 0.0f");
            }

            printf(";\n");
        }
        printf("    }\n");
    }

    printf("}\n");
}

int main(void)
{
    int i, ini, outi;

    printf(
        "/*\n"
        "  Simple DirectMedia Layer\n"
        "  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>\n"
        "\n"
        "  This software is provided 'as-is', without any express or implied\n"
        "  warranty.  In no event will the authors be held liable for any damages\n"
        "  arising from the use of this software.\n"
        "\n"
        "  Permission is granted to anyone to use this software for any purpose,\n"
        "  including commercial applications, and to alter it and redistribute it\n"
        "  freely, subject to the following restrictions:\n"
        "\n"
        "  1. The origin of this software must not be misrepresented; you must not\n"
        "     claim that you wrote the original software. If you use this software\n"
        "     in a product, an acknowledgment in the product documentation would be\n"
        "     appreciated but is not required.\n"
        "  2. Altered source versions must be plainly marked as such, and must not be\n"
        "     misrepresented as being the original software.\n"
        "  3. This notice may not be removed or altered from any source distribution.\n"
        "*/\n"
        "\n"
        "/* DO NOT EDIT, THIS FILE WAS GENERATED BY build-scripts/gen_audio_channel_conversion.c */\n"
        "\n"
    );

    printf("#define NUM_CHANNELS %d\n\n", NUM_CHANNELS);

    for (ini = 1; ini <= NUM_CHANNELS; ini++) {
        for (outi = 1; outi <= NUM_CHANNELS; outi++) {
            if (ini == outi)
                continue;
            if (ini <= 2 && outi <= 2) {
                printf("#if !SDL_HAVE_NEON_SUPPORT && !SDL_HAVE_SSE_SUPPORT\n");
            }
            write_converter(ini, outi);
            if (ini <= 2 && outi <= 2) {
                printf("#endif // !SDL_HAVE_NEON_SUPPORT && !SDL_HAVE_SSE_SUPPORT\n");
            }
            printf("\n");
        }
    }

    for (i = 0; i < 3; i++) {
        switch (i) {
        case 0:
            printf("#if SDL_HAVE_NEON_SUPPORT\n");
            break;
        case 1:
            printf("#if SDL_HAVE_SSE_SUPPORT\n");
            break;
        case 2:
            printf("#if !SDL_HAVE_NEON_SUPPORT && !SDL_HAVE_SSE_SUPPORT\n");
            break;
        }
        printf("static SDL_AudioFilter channel_converters[NUM_CHANNELS][NUM_CHANNELS] = { /* [from][to] */\n", NUM_CHANNELS, NUM_CHANNELS);
        for (ini = 1; ini <= NUM_CHANNELS; ini++) {
            printf("    {");
            for (outi = 1; outi <= NUM_CHANNELS; outi++) {
                const char *fromstr = layout_names[ini - 1];
                const char *tostr = layout_names[outi - 1];
                int len;
                if (ini == outi || (i < 2 && ini <= 2 && outi <= 2)) {
                    len = printf(" NULL");
                } else {
                    len = printf(" SDL_Convert%sTo%s", remove_dots(fromstr), remove_dots(tostr));
                }
                printf(",");
                while (len < 24) {
                    len++;
                    printf(" ");
                }
            }
            printf(" }%s\n", (ini == NUM_CHANNELS) ? "" : ",");
        }
        switch (i) {
        case 0:
            printf("#endif // SDL_HAVE_NEON_SUPPORT\n");
            break;
        case 1:
            printf("#endif // SDL_HAVE_SSE_SUPPORT\n");
            break;
        case 2:
            printf("#endif // !SDL_HAVE_NEON_SUPPORT && !SDL_HAVE_SSE_SUPPORT\n");
            break;
        }
    }

    printf("};\n\n");
    printf("/* vi: set ts=4 sw=4 expandtab: */\n");

    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
