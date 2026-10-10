// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

/*
Copyright (c) 2003-2010, Mark Borgerding

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted
provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
this list of conditions
and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
this list of
conditions and the following disclaimer in the documentation and/or other
materials provided with
the distribution.
 * Neither the author nor the names of any contributors may be used to
endorse or promote
products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF
THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once
#include "AudioCommon/SurroundDecoder/KissFFT.h"

struct kiss_fft_state
{
  int nfft;
  int inverse;
  int factors[64];
  kiss_fft_cpx twiddles[1];
};

/*
  Explanation of macros dealing with complex math:

   C_MUL(m,a,b)         : m = a*b
   C_FIXDIV( c , div )  : if a fixed point impl., c /= div. noop otherwise
   C_SUB( res, a,b)     : res = a - b
   C_SUBFROM( res , a)  : res -= a
   C_ADDTO( res , a)    : res += a
 * */
#define S_MUL(a, b) ((a) * (b))
#define C_MUL(m, a, b)                                                                             \
  (m).r = (a).r * (b).r - (a).i * (b).i;                                                           \
  (m).i = (a).r * (b).i + (a).i * (b).r;

#define C_FIXDIV(c, div) /* NOOP */
#define C_MULBYSCALAR(c, s)                                                                        \
  (c).r *= (s);                                                                                    \
  (c).i *= (s);

#define CHECK_OVERFLOW_OP(a, op, b) /* noop */

#define C_ADD(res, a, b)                                                                           \
  CHECK_OVERFLOW_OP((a).r, +, (b).r)                                                               \
  CHECK_OVERFLOW_OP((a).i, +, (b).i)                                                               \
  (res).r = (a).r + (b).r;                                                                         \
  (res).i = (a).i + (b).i;

#define C_SUB(res, a, b)                                                                           \
  CHECK_OVERFLOW_OP((a).r, -, (b).r)                                                               \
  CHECK_OVERFLOW_OP((a).i, -, (b).i)                                                               \
  (res).r = (a).r - (b).r;                                                                         \
  (res).i = (a).i - (b).i;

#define C_ADDTO(res, a)                                                                            \
  CHECK_OVERFLOW_OP((res).r, +, (a).r)                                                             \
  CHECK_OVERFLOW_OP((res).i, +, (a).i)                                                             \
  (res).r += (a).r;                                                                                \
  (res).i += (a).i;

#define HALF_OF(x) ((x) * .5)

#define kf_cexp(x, phase)                                                                          \
  (x)->r = cos(phase);                                                                             \
  (x)->i = sin(phase);

/* a debugging function */
#define pcpx(c) fprintf(stderr, "%g + %gi\n", (double)((c)->r), (double)((c)->i))

#define KISS_FFT_TMP_ALLOC(nbytes) KISS_FFT_MALLOC(nbytes)
#define KISS_FFT_TMP_FREE(ptr) KISS_FFT_FREE(ptr)
