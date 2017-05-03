/*
 * Copyright © 2014 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(CUBEB_PANNER)
#define CUBEB_PANNER

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Pan an integer or an float stereo buffer according to a cos/sin pan law
 * @param buf the buffer to pan
 * @param frames the number of frames in `buf`
 * @param pan a float in [-1.0; 1.0]
 */
void cubeb_pan_stereo_buffer_float(float * buf, uint32_t frames, float pan);
void cubeb_pan_stereo_buffer_int(short* buf, uint32_t frames, float pan);

#if defined(__cplusplus)
}
#endif

#endif
