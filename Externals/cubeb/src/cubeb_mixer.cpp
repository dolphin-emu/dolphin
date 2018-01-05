/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#include <cassert>
#include "cubeb-internal.h"
#include "cubeb_mixer.h"

// DUAL_MONO(_LFE) is same as STEREO(_LFE).
#define MASK_MONO         (1 << CHANNEL_MONO)
#define MASK_MONO_LFE     (MASK_MONO | (1 << CHANNEL_LFE))
#define MASK_STEREO       ((1 << CHANNEL_LEFT) | (1 << CHANNEL_RIGHT))
#define MASK_STEREO_LFE   (MASK_STEREO | (1 << CHANNEL_LFE))
#define MASK_3F           (MASK_STEREO | (1 << CHANNEL_CENTER))
#define MASK_3F_LFE       (MASK_3F | (1 << CHANNEL_LFE))
#define MASK_2F1          (MASK_STEREO | (1 << CHANNEL_RCENTER))
#define MASK_2F1_LFE      (MASK_2F1 | (1 << CHANNEL_LFE))
#define MASK_3F1          (MASK_3F | (1 << CHANNEL_RCENTER))
#define MASK_3F1_LFE      (MASK_3F1 | (1 << CHANNEL_LFE))
#define MASK_2F2          (MASK_STEREO | (1 << CHANNEL_LS) | (1 << CHANNEL_RS))
#define MASK_2F2_LFE      (MASK_2F2 | (1 << CHANNEL_LFE))
#define MASK_3F2          (MASK_2F2 | (1 << CHANNEL_CENTER))
#define MASK_3F2_LFE      (MASK_3F2 | (1 << CHANNEL_LFE))
#define MASK_3F3R_LFE     (MASK_3F2_LFE | (1 << CHANNEL_RCENTER))
#define MASK_3F4_LFE      (MASK_3F2_LFE | (1 << CHANNEL_RLS) | (1 << CHANNEL_RRS))

cubeb_channel_layout cubeb_channel_map_to_layout(cubeb_channel_map const * channel_map)
{
  uint32_t channel_mask = 0;
  for (uint8_t i = 0 ; i < channel_map->channels ; ++i) {
    if (channel_map->map[i] == CHANNEL_INVALID ||
        channel_map->map[i] == CHANNEL_UNMAPPED) {
      return CUBEB_LAYOUT_UNDEFINED;
    }
    channel_mask |= 1 << channel_map->map[i];
  }

  switch(channel_mask) {
    case MASK_MONO: return CUBEB_LAYOUT_MONO;
    case MASK_MONO_LFE: return CUBEB_LAYOUT_MONO_LFE;
    case MASK_STEREO: return CUBEB_LAYOUT_STEREO;
    case MASK_STEREO_LFE: return CUBEB_LAYOUT_STEREO_LFE;
    case MASK_3F: return CUBEB_LAYOUT_3F;
    case MASK_3F_LFE: return CUBEB_LAYOUT_3F_LFE;
    case MASK_2F1: return CUBEB_LAYOUT_2F1;
    case MASK_2F1_LFE: return CUBEB_LAYOUT_2F1_LFE;
    case MASK_3F1: return CUBEB_LAYOUT_3F1;
    case MASK_3F1_LFE: return CUBEB_LAYOUT_3F1_LFE;
    case MASK_2F2: return CUBEB_LAYOUT_2F2;
    case MASK_2F2_LFE: return CUBEB_LAYOUT_2F2_LFE;
    case MASK_3F2: return CUBEB_LAYOUT_3F2;
    case MASK_3F2_LFE: return CUBEB_LAYOUT_3F2_LFE;
    case MASK_3F3R_LFE: return CUBEB_LAYOUT_3F3R_LFE;
    case MASK_3F4_LFE: return CUBEB_LAYOUT_3F4_LFE;
    default: return CUBEB_LAYOUT_UNDEFINED;
  }
}

cubeb_layout_map const CUBEB_CHANNEL_LAYOUT_MAPS[CUBEB_LAYOUT_MAX] = {
  { "undefined",      0,  CUBEB_LAYOUT_UNDEFINED },
  { "dual mono",      2,  CUBEB_LAYOUT_DUAL_MONO },
  { "dual mono lfe",  3,  CUBEB_LAYOUT_DUAL_MONO_LFE },
  { "mono",           1,  CUBEB_LAYOUT_MONO },
  { "mono lfe",       2,  CUBEB_LAYOUT_MONO_LFE },
  { "stereo",         2,  CUBEB_LAYOUT_STEREO },
  { "stereo lfe",     3,  CUBEB_LAYOUT_STEREO_LFE },
  { "3f",             3,  CUBEB_LAYOUT_3F },
  { "3f lfe",         4,  CUBEB_LAYOUT_3F_LFE },
  { "2f1",            3,  CUBEB_LAYOUT_2F1 },
  { "2f1 lfe",        4,  CUBEB_LAYOUT_2F1_LFE },
  { "3f1",            4,  CUBEB_LAYOUT_3F1 },
  { "3f1 lfe",        5,  CUBEB_LAYOUT_3F1_LFE },
  { "2f2",            4,  CUBEB_LAYOUT_2F2 },
  { "2f2 lfe",        5,  CUBEB_LAYOUT_2F2_LFE },
  { "3f2",            5,  CUBEB_LAYOUT_3F2 },
  { "3f2 lfe",        6,  CUBEB_LAYOUT_3F2_LFE },
  { "3f3r lfe",       7,  CUBEB_LAYOUT_3F3R_LFE },
  { "3f4 lfe",        8,  CUBEB_LAYOUT_3F4_LFE }
};

static int const CHANNEL_ORDER_TO_INDEX[CUBEB_LAYOUT_MAX][CHANNEL_MAX] = {
//  M | L | R | C | LS | RS | RLS | RC | RRS | LFE
  { -1, -1, -1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // UNDEFINED
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // DUAL_MONO
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,   2 }, // DUAL_MONO_LFE
  {  0, -1, -1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // MONO
  {  0, -1, -1, -1,  -1,  -1,   -1,  -1,   -1,   1 }, // MONO_LFE
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,  -1 }, // STEREO
  { -1,  0,  1, -1,  -1,  -1,   -1,  -1,   -1,   2 }, // STEREO_LFE
  { -1,  0,  1,  2,  -1,  -1,   -1,  -1,   -1,  -1 }, // 3F
  { -1,  0,  1,  2,  -1,  -1,   -1,  -1,   -1,   3 }, // 3F_LFE
  { -1,  0,  1, -1,  -1,  -1,   -1,   2,   -1,  -1 }, // 2F1
  { -1,  0,  1, -1,  -1,  -1,   -1,   3,   -1,   2 }, // 2F1_LFE
  { -1,  0,  1,  2,  -1,  -1,   -1,   3,   -1,  -1 }, // 3F1
  { -1,  0,  1,  2,  -1,  -1,   -1,   4,   -1,   3 }, // 3F1_LFE
  { -1,  0,  1, -1,   2,   3,   -1,  -1,   -1,  -1 }, // 2F2
  { -1,  0,  1, -1,   3,   4,   -1,  -1,   -1,   2 }, // 2F2_LFE
  { -1,  0,  1,  2,   3,   4,   -1,  -1,   -1,  -1 }, // 3F2
  { -1,  0,  1,  2,   4,   5,   -1,  -1,   -1,   3 }, // 3F2_LFE
  { -1,  0,  1,  2,   5,   6,   -1,   4,   -1,   3 }, // 3F3R_LFE
  { -1,  0,  1,  2,   6,   7,    4,  -1,    5,   3 }, // 3F4_LFE
};

// The downmix matrix from TABLE 2 in the ITU-R BS.775-3[1] defines a way to
// convert 3F2 input data to 1F, 2F, 3F, 2F1, 3F1, 2F2 output data. We extend it
// to convert 3F2-LFE input data to 1F, 2F, 3F, 2F1, 3F1, 2F2 and their LFEs
// output data.
// [1] https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.775-3-201208-I!!PDF-E.pdf

// Number of converted layouts: 1F, 2F, 3F, 2F1, 3F1, 2F2 and their LFEs.
unsigned int const SUPPORTED_LAYOUT_NUM = 12;
// Number of input channel for downmix conversion.
unsigned int const INPUT_CHANNEL_NUM = 6; // 3F2-LFE
// Max number of possible output channels.
unsigned int const MAX_OUTPUT_CHANNEL_NUM = 5; // 2F2-LFE or 3F1-LFE
float const INV_SQRT_2 = 0.707106f; // 1/sqrt(2)
// Each array contains coefficients that will be multiplied with
// { L, R, C, LFE, LS, RS } channels respectively.
static float const DOWNMIX_MATRIX_3F2_LFE[SUPPORTED_LAYOUT_NUM][MAX_OUTPUT_CHANNEL_NUM][INPUT_CHANNEL_NUM] =
{
// 1F Mono
  {
    { INV_SQRT_2, INV_SQRT_2, 1, 0, 0.5, 0.5 }, // M
  },
// 1F Mono-LFE
  {
    { INV_SQRT_2, INV_SQRT_2, 1, 0, 0.5, 0.5 }, // M
    { 0, 0, 0, 1, 0, 0 }                        // LFE
  },
// 2F Stereo
  {
    { 1, 0, INV_SQRT_2, 0, INV_SQRT_2, 0 },     // L
    { 0, 1, INV_SQRT_2, 0, 0, INV_SQRT_2 }      // R
  },
// 2F Stereo-LFE
  {
    { 1, 0, INV_SQRT_2, 0, INV_SQRT_2, 0 },     // L
    { 0, 1, INV_SQRT_2, 0, 0, INV_SQRT_2 },     // R
    { 0, 0, 0, 1, 0, 0 }                        // LFE
  },
// 3F
  {
    { 1, 0, 0, 0, INV_SQRT_2, 0 },              // L
    { 0, 1, 0, 0, 0, INV_SQRT_2 },              // R
    { 0, 0, 1, 0, 0, 0 }                        // C
  },
// 3F-LFE
  {
    { 1, 0, 0, 0, INV_SQRT_2, 0 },              // L
    { 0, 1, 0, 0, 0, INV_SQRT_2 },              // R
    { 0, 0, 1, 0, 0, 0 },                       // C
    { 0, 0, 0, 1, 0, 0 }                        // LFE
  },
// 2F1
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 2F1-LFE
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 1, 0, 0 },                       // LFE
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 3F1
  {
    { 1, 0, 0, 0, 0, 0 },                       // L
    { 0, 1, 0, 0, 0, 0 },                       // R
    { 0, 0, 1, 0, 0, 0 },                       // C
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 3F1-LFE
  {
    { 1, 0, 0, 0, 0, 0 },                       // L
    { 0, 1, 0, 0, 0, 0 },                       // R
    { 0, 0, 1, 0, 0, 0 },                       // C
    { 0, 0, 0, 1, 0, 0 },                       // LFE
    { 0, 0, 0, 0, INV_SQRT_2, INV_SQRT_2 }      // S
  },
// 2F2
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 0, 1, 0 },                       // LS
    { 0, 0, 0, 0, 0, 1 }                        // RS
  },
// 2F2-LFE
  {
    { 1, 0, INV_SQRT_2, 0, 0, 0 },              // L
    { 0, 1, INV_SQRT_2, 0, 0, 0 },              // R
    { 0, 0, 0, 1, 0, 0 },                       // LFE
    { 0, 0, 0, 0, 1, 0 },                       // LS
    { 0, 0, 0, 0, 0, 1 }                        // RS
  }
};

// Convert audio data from 3F2(-LFE) to 1F, 2F, 3F, 2F1, 3F1, 2F2 and their LFEs.
//
// ITU-R BS.775-3[1] provides spec for downmixing 3F2 data to 1F, 2F, 3F, 2F1,
// 3F1, 2F2 data. We simply add LFE to its defined matrix. If both the input
// and output have LFE channel, then we pass it's data. If only input or output
// has LFE, then we either drop it or append 0 to the LFE channel.
//
// Fig. 1:
// |<-------------- 1 -------------->|<-------------- 2 -------------->|
// +----+----+----+------+-----+-----+----+----+----+------+-----+-----+
// | L0 | R0 | C0 | LFE0 | LS0 | RS0 | L1 | R1 | C1 | LFE1 | LS1 | RS1 | ...
// +----+----+----+------+-----+-----+----+----+----+------+-----+-----+
//
// Fig. 2:
// |<-- 1 -->|<-- 2 -->|
// +----+----+----+----+
// | L0 | R0 | L1 | R1 | ...
// +----+----+----+----+
//
// The figures above shows an example for downmixing from 3F2-LFE(Fig. 1) to
// to stereo(Fig. 2), where L0 = L0 + 0.707 * (C0 + LS0),
// R0 = R0 + 0.707 * (C0 + RS0), L1 = L1 + 0.707 * (C1 + LS1),
// R1 = R1 + 0.707 * (C1 + RS1), ...
//
// Nevertheless, the downmixing method is a little bit different on OSX.
// The audio rendering mechanism on OS X will drop the extra channels beyond
// the channels that audio device can provide. The trick here is that OSX allows
// us to set the layout containing other channels that the output device can
// NOT provide. For example, setting 3F2-LFE layout to a stereo device is fine.
// Therefore, OSX expects we fill the buffer for playing sound by the defined
// layout, so there are some will-be-dropped data in the buffer:
//
// +---+---+---+-----+----+----+
// | L | R | C | LFE | LS | RS | ...
// +---+---+---+-----+----+----+
//           ^    ^    ^    ^
//           The data for these four channels will be dropped!
//
// To keep all the information, we need to downmix the data before it's dropped.
// The figure below shows an example for downmixing from 3F2-LFE(Fig. 1)
// to stereo(Fig. 3) on OSX, where the LO, R0, L1, R0 are same as above.
//
// Fig. 3:
// |<---------- 1 ---------->|<---------- 2 ---------->|
// +----+----+---+---+---+---+----+----+---+---+---+---+
// | L0 | R0 | x | x | x | x | L1 | R1 | x | x | x | x | ...
// +----+----+---+---+---+---+----+----+---+---+---+---+
//           |<--  dummy  -->|         |<--  dummy  -->|
template<typename T>
bool
downmix_3f2(unsigned long inframes,
            T const * const in, unsigned long in_len,
            T * out, unsigned long out_len,
            cubeb_channel_layout in_layout, cubeb_channel_layout out_layout)
{
  if ((in_layout != CUBEB_LAYOUT_3F2 && in_layout != CUBEB_LAYOUT_3F2_LFE) ||
      out_layout < CUBEB_LAYOUT_MONO || out_layout > CUBEB_LAYOUT_2F2_LFE) {
    return false;
  }

  unsigned int in_channels = CUBEB_CHANNEL_LAYOUT_MAPS[in_layout].channels;
  unsigned int out_channels = CUBEB_CHANNEL_LAYOUT_MAPS[out_layout].channels;

  // Conversion from 3F2 to 2F2-LFE or 3F1-LFE is allowed, so we use '<=' instead of '<'.
  assert(out_channels <= in_channels);

  auto & downmix_matrix = DOWNMIX_MATRIX_3F2_LFE[out_layout - CUBEB_LAYOUT_MONO]; // The matrix is started from mono.
  unsigned long out_index = 0;
  for (unsigned long i = 0 ; i < inframes * in_channels; i += in_channels) {
    for (unsigned int j = 0; j < out_channels; ++j) {
      T sample = 0;
      for (unsigned int k = 0 ; k < INPUT_CHANNEL_NUM ; ++k) {
        // 3F2-LFE has 6 channels: L, R, C, LFE, LS, RS, while 3F2 has only 5
        // channels: L, R, C, LS, RS. Thus, we need to append 0 to LFE(index 3)
        // to simulate a 3F2-LFE data when input layout is 3F2.
        assert((in_layout == CUBEB_LAYOUT_3F2_LFE || k < 3) ? (i + k < in_len) : (k == 3) ? true : (i + k - 1 < in_len));
        T data = (in_layout == CUBEB_LAYOUT_3F2_LFE) ? in[i + k] : (k == 3) ? 0 : in[i + ((k < 3) ? k : k - 1)];
        sample += downmix_matrix[j][k] * data;
      }
      assert(out_index + j < out_len);
      out[out_index + j] = sample;
    }
#if defined(USE_AUDIOUNIT)
    out_index += in_channels;
#else
    out_index += out_channels;
#endif
  }

  return true;
}

/* Map the audio data by channel name. */
template<class T>
bool
mix_remap(long inframes,
          T const * const in, unsigned long in_len,
          T * out, unsigned long out_len,
          cubeb_channel_layout in_layout, cubeb_channel_layout out_layout)
{
  assert(in_layout != out_layout);

  // We might overwrite the data before we copied them to the mapped index
  // (e.g. upmixing from stereo to 2F1 or mapping [L, R] to [R, L])
  if (in == out) {
    return false;
  }

  unsigned int in_channels = CUBEB_CHANNEL_LAYOUT_MAPS[in_layout].channels;
  unsigned int out_channels = CUBEB_CHANNEL_LAYOUT_MAPS[out_layout].channels;

  uint32_t in_layout_mask = 0;
  for (unsigned int i = 0 ; i < in_channels ; ++i) {
    in_layout_mask |= 1 << CHANNEL_INDEX_TO_ORDER[in_layout][i];
  }

  uint32_t out_layout_mask = 0;
  for (unsigned int i = 0 ; i < out_channels ; ++i) {
    out_layout_mask |= 1 << CHANNEL_INDEX_TO_ORDER[out_layout][i];
  }

  // If there is no matched channel, then do nothing.
  if (!(out_layout_mask & in_layout_mask)) {
    return false;
  }

  for (unsigned long i = 0, out_index = 0; i < inframes * in_channels; i += in_channels, out_index += out_channels) {
    for (unsigned int j = 0; j < out_channels; ++j) {
      cubeb_channel channel = CHANNEL_INDEX_TO_ORDER[out_layout][j];
      uint32_t channel_mask = 1 << channel;
      int channel_index = CHANNEL_ORDER_TO_INDEX[in_layout][channel];
      assert(out_index + j < out_len);
      if (in_layout_mask & channel_mask) {
        assert(i + channel_index < in_len);
        assert(channel_index != -1);
        out[out_index + j] = in[i + channel_index];
      } else {
        assert(channel_index == -1);
        out[out_index + j] = 0;
      }
    }
  }

  return true;
}

/* Drop the extra channels beyond the provided output channels. */
template<typename T>
void
downmix_fallback(long inframes,
                 T const * const in, unsigned long in_len,
                 T * out, unsigned long out_len,
                 unsigned int in_channels, unsigned int out_channels)
{
  assert(in_channels >= out_channels);

  if (in_channels == out_channels && in == out) {
    return;
  }

  for (unsigned long i = 0, out_index = 0; i < inframes * in_channels; i += in_channels, out_index += out_channels) {
    for (unsigned int j = 0; j < out_channels; ++j) {
      assert(i + j < in_len && out_index + j < out_len);
      out[out_index + j] = in[i + j];
    }
  }
}


template<typename T>
void
cubeb_downmix(long inframes,
              T const * const in, unsigned long in_len,
              T * out, unsigned long out_len,
              cubeb_stream_params const * stream_params,
              cubeb_stream_params const * mixer_params)
{
  assert(in && out);
  assert(inframes);
  assert(stream_params->channels >= mixer_params->channels &&
         mixer_params->channels > 0);
  assert(stream_params->layout != CUBEB_LAYOUT_UNDEFINED);

  unsigned int in_channels = stream_params->channels;
  cubeb_channel_layout in_layout = stream_params->layout;

  unsigned int out_channels = mixer_params->channels;
  cubeb_channel_layout out_layout = mixer_params->layout;

  // If the channel number is different from the layout's setting,
  // then we use fallback downmix mechanism.
  if (out_channels == CUBEB_CHANNEL_LAYOUT_MAPS[out_layout].channels &&
      in_channels == CUBEB_CHANNEL_LAYOUT_MAPS[in_layout].channels) {
    if (downmix_3f2(inframes, in, in_len, out, out_len, in_layout, out_layout)) {
      return;
    }

#if defined(USE_AUDIOUNIT)
  // We only support downmix for audio 5.1 on OS X currently.
  return;
#endif

    if (mix_remap(inframes, in, in_len, out, out_len, in_layout, out_layout)) {
      return;
    }
  }

  downmix_fallback(inframes, in, in_len, out, out_len, in_channels, out_channels);
}

/* Upmix function, copies a mono channel into L and R. */
template<typename T>
void
mono_to_stereo(long insamples, T const * in, unsigned long in_len,
               T * out, unsigned long out_len, unsigned int out_channels)
{
  for (long i = 0, j = 0; i < insamples; ++i, j += out_channels) {
    assert(i < in_len && j + 1 < out_len);
    out[j] = out[j + 1] = in[i];
  }
}

template<typename T>
void
cubeb_upmix(long inframes,
            T const * const in, unsigned long in_len,
            T * out, unsigned long out_len,
            cubeb_stream_params const * stream_params,
            cubeb_stream_params const * mixer_params)
{
  assert(in && out);
  assert(inframes);
  assert(mixer_params->channels >= stream_params->channels &&
         stream_params->channels > 0);

  unsigned int in_channels = stream_params->channels;
  unsigned int out_channels = mixer_params->channels;

  /* Either way, if we have 2 or more channels, the first two are L and R. */
  /* If we are playing a mono stream over stereo speakers, copy the data over. */
  if (in_channels == 1 && out_channels >= 2) {
    mono_to_stereo(inframes, in, in_len, out, out_len, out_channels);
  } else {
    /* Copy through. */
    for (unsigned int i = 0, o = 0; i < inframes * in_channels;
        i += in_channels, o += out_channels) {
      for (unsigned int j = 0; j < in_channels; ++j) {
        assert(i + j < in_len && o + j < out_len);
        out[o + j] = in[i + j];
      }
    }
  }

  /* Check if more channels. */
  if (out_channels <= 2) {
    return;
  }

  /* Put silence in remaining channels. */
  for (long i = 0, o = 0; i < inframes; ++i, o += out_channels) {
    for (unsigned int j = 2; j < out_channels; ++j) {
      assert(o + j < out_len);
      out[o + j] = 0.0;
    }
  }
}

bool
cubeb_should_upmix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer)
{
  return mixer->channels > stream->channels;
}

bool
cubeb_should_downmix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer)
{
  if (mixer->channels > stream->channels || mixer->layout == stream->layout) {
    return false;
  }

  return mixer->channels < stream->channels ||
         // When mixer.channels == stream.channels
         mixer->layout == CUBEB_LAYOUT_UNDEFINED ||  // fallback downmix
         (stream->layout == CUBEB_LAYOUT_3F2 &&      // 3f2 downmix
          (mixer->layout == CUBEB_LAYOUT_2F2_LFE ||
           mixer->layout == CUBEB_LAYOUT_3F1_LFE));
}

bool
cubeb_should_mix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer)
{
  return cubeb_should_upmix(stream, mixer) || cubeb_should_downmix(stream, mixer);
}

struct cubeb_mixer {
  virtual void mix(long frames,
                   void * input_buffer, unsigned long input_buffer_length,
                   void * output_buffer, unsigned long output_buffer_length,
                   cubeb_stream_params const * stream_params,
                   cubeb_stream_params const * mixer_params) = 0;
  virtual ~cubeb_mixer() {};
};

template<typename T>
struct cubeb_mixer_impl : public cubeb_mixer {
  explicit cubeb_mixer_impl(unsigned int d)
    : direction(d)
  {
  }

  void mix(long frames,
           void * input_buffer, unsigned long input_buffer_length,
           void * output_buffer, unsigned long output_buffer_length,
           cubeb_stream_params const * stream_params,
           cubeb_stream_params const * mixer_params)
  {
    if (frames <= 0) {
      return;
    }

    T * in = static_cast<T*>(input_buffer);
    T * out = static_cast<T*>(output_buffer);

    if ((direction & CUBEB_MIXER_DIRECTION_DOWNMIX) &&
        cubeb_should_downmix(stream_params, mixer_params)) {
      cubeb_downmix(frames, in, input_buffer_length, out, output_buffer_length, stream_params, mixer_params);
    } else if ((direction & CUBEB_MIXER_DIRECTION_UPMIX) &&
               cubeb_should_upmix(stream_params, mixer_params)) {
      cubeb_upmix(frames, in, input_buffer_length, out, output_buffer_length, stream_params, mixer_params);
    }
  }

  ~cubeb_mixer_impl() {};

  unsigned char const direction;
};

cubeb_mixer * cubeb_mixer_create(cubeb_sample_format format,
                                 unsigned char direction)
{
  assert(direction & CUBEB_MIXER_DIRECTION_DOWNMIX ||
         direction & CUBEB_MIXER_DIRECTION_UPMIX);
  switch(format) {
    case CUBEB_SAMPLE_S16NE:
      return new cubeb_mixer_impl<short>(direction);
    case CUBEB_SAMPLE_FLOAT32NE:
      return new cubeb_mixer_impl<float>(direction);
    default:
      assert(false);
      return nullptr;
  }
}

void cubeb_mixer_destroy(cubeb_mixer * mixer)
{
  delete mixer;
}

void cubeb_mixer_mix(cubeb_mixer * mixer, long frames,
                     void * input_buffer, unsigned long input_buffer_length,
                     void * output_buffer, unsigned long outputput_buffer_length,
                     cubeb_stream_params const * stream_params,
                     cubeb_stream_params const * mixer_params)
{
  assert(mixer);
  mixer->mix(frames, input_buffer, input_buffer_length, output_buffer, outputput_buffer_length,
             stream_params, mixer_params);
}
