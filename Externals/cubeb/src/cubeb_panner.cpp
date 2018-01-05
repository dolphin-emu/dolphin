/*
 * Copyright © 2014 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>

#include "cubeb_panner.h"

#ifndef M_PI
#define M_PI 3.14159263
#endif

/**
 * We use a cos/sin law.
 */

namespace {
template<typename T>
void cubeb_pan_stereo_buffer(T * buf, uint32_t frames, float pan)
{
  if (pan == 0.0) {
    return;
  }
  /* rescale in [0; 1] */
  pan += 1;
  pan /= 2;
  float left_gain  = float(cos(pan * M_PI * 0.5));
  float right_gain = float(sin(pan * M_PI * 0.5));

  /* In we are panning on the left, pan the right channel into the left one and
   * vice-versa. */
  if (pan < 0.5) {
    for (uint32_t i = 0; i < frames * 2; i+=2) {
      buf[i]     = T(buf[i] + buf[i + 1] * left_gain);
      buf[i + 1] = T(buf[i + 1] * right_gain);
    }
  } else {
    for (uint32_t i = 0; i < frames * 2; i+=2) {
      buf[i]     = T(buf[i] * left_gain);
      buf[i + 1] = T(buf[i + 1] + buf[i] * right_gain);
    }
  }
}
}

void cubeb_pan_stereo_buffer_float(float * buf, uint32_t frames, float pan)
{
  cubeb_pan_stereo_buffer(buf, frames, pan);
}

void cubeb_pan_stereo_buffer_int(short * buf, uint32_t frames, float pan)
{
  cubeb_pan_stereo_buffer(buf, frames, pan);
}

