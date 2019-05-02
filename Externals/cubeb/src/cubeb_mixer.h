/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_MIXER
#define CUBEB_MIXER

#include "cubeb/cubeb.h" // for cubeb_channel_layout ,CUBEB_CHANNEL_LAYOUT_MAPS and cubeb_stream_params.
#include <stdbool.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
  CHANNEL_INVALID = -1,
  CHANNEL_MONO = 0,
  CHANNEL_LEFT,
  CHANNEL_RIGHT,
  CHANNEL_CENTER,
  CHANNEL_LS,
  CHANNEL_RS,
  CHANNEL_RLS,
  CHANNEL_RCENTER,
  CHANNEL_RRS,
  CHANNEL_LFE,
  CHANNEL_UNMAPPED,
  CHANNEL_MAX = 256 // Max number of supported channels.
} cubeb_channel;

static cubeb_channel const CHANNEL_INDEX_TO_ORDER[CUBEB_LAYOUT_MAX][CHANNEL_MAX] = {
  { CHANNEL_INVALID },                                                                                            // UNDEFINED
  { CHANNEL_LEFT, CHANNEL_RIGHT },                                                                                // DUAL_MONO
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE },                                                                   // DUAL_MONO_LFE
  { CHANNEL_MONO },                                                                                               // MONO
  { CHANNEL_MONO, CHANNEL_LFE },                                                                                  // MONO_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT },                                                                                // STEREO
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE },                                                                   // STEREO_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER },                                                                // 3F
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE },                                                   // 3F_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_RCENTER },                                                               // 2F1
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE, CHANNEL_RCENTER },                                                  // 2F1_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_RCENTER },                                               // 3F1
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_RCENTER },                                  // 3F1_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LS, CHANNEL_RS },                                                        // 2F2
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_LFE, CHANNEL_LS, CHANNEL_RS },                                           // 2F2_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LS, CHANNEL_RS },                                        // 3F2
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_LS, CHANNEL_RS },                           // 3F2_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_RCENTER, CHANNEL_LS, CHANNEL_RS },          // 3F3R_LFE
  { CHANNEL_LEFT, CHANNEL_RIGHT, CHANNEL_CENTER, CHANNEL_LFE, CHANNEL_RLS, CHANNEL_RRS, CHANNEL_LS, CHANNEL_RS }  // 3F4_LFE
  // When more channels are present, the stream is considered unmapped to a
  // particular speaker set.
};

typedef struct {
  unsigned int channels;
  cubeb_channel map[CHANNEL_MAX];
} cubeb_channel_map;

cubeb_channel_layout cubeb_channel_map_to_layout(cubeb_channel_map const * channel_map);

bool cubeb_should_upmix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer);

bool cubeb_should_downmix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer);

bool cubeb_should_mix(cubeb_stream_params const * stream, cubeb_stream_params const * mixer);

typedef enum {
  CUBEB_MIXER_DIRECTION_DOWNMIX = 0x01,
  CUBEB_MIXER_DIRECTION_UPMIX   = 0x02,
} cubeb_mixer_direction;

typedef struct cubeb_mixer cubeb_mixer;
cubeb_mixer * cubeb_mixer_create(cubeb_sample_format format,
                                 unsigned char direction);
void cubeb_mixer_destroy(cubeb_mixer * mixer);
void cubeb_mixer_mix(cubeb_mixer * mixer, long frames,
                     void * input_buffer, unsigned long input_buffer_length,
                     void * output_buffer, unsigned long output_buffer_length,
                     cubeb_stream_params const * stream_params,
                     cubeb_stream_params const * mixer_params);

#if defined(__cplusplus)
}
#endif

#endif // CUBEB_MIXER
