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

#include "FreeSurround/_KissFFTGuts.h"
#include <random>
#include <vector>

/* The guts header contains all the multiplication and addition macros
   that are defined for fixed or floating point complex numbers.
   It also declares the kf_ internal functions.
 */

static void kf_bfly2(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_cfg st, int m) {
  const kiss_fft_cpx *tw1 = st->twiddles;
  kiss_fft_cpx *Fout2 = Fout + m;
  do {
    kiss_fft_cpx t;
    C_FIXDIV(*Fout, 2);
    C_FIXDIV(*Fout2, 2);

    C_MUL(t, *Fout2, *tw1);
    tw1 += fstride;
    C_SUB(*Fout2, *Fout, t);
    C_ADDTO(*Fout, t);
    ++Fout2;
    ++Fout;
  } while (--m);
}

static void kf_bfly4(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_cfg st, const size_t m) {
  kiss_fft_cpx *tw1, *tw2, *tw3;
  size_t k = m;
  const size_t m2 = 2 * m, m3 = 3 * m;

  tw3 = tw2 = tw1 = st->twiddles;

  do {
    kiss_fft_cpx scratch[6];
    C_FIXDIV(*Fout, 4);
    C_FIXDIV(Fout[m], 4);
    C_FIXDIV(Fout[m2], 4);
    C_FIXDIV(Fout[m3], 4);

    C_MUL(scratch[0], Fout[m], *tw1);
    C_MUL(scratch[1], Fout[m2], *tw2);
    C_MUL(scratch[2], Fout[m3], *tw3);

    C_SUB(scratch[5], *Fout, scratch[1]);
    C_ADDTO(*Fout, scratch[1]);
    C_ADD(scratch[3], scratch[0], scratch[2]);
    C_SUB(scratch[4], scratch[0], scratch[2]);
    C_SUB(Fout[m2], *Fout, scratch[3]);
    tw1 += fstride;
    tw2 += fstride * 2;
    tw3 += fstride * 3;
    C_ADDTO(*Fout, scratch[3]);

    if (st->inverse) {
      Fout[m].r = scratch[5].r - scratch[4].i;
      Fout[m].i = scratch[5].i + scratch[4].r;
      Fout[m3].r = scratch[5].r + scratch[4].i;
      Fout[m3].i = scratch[5].i - scratch[4].r;
    } else {
      Fout[m].r = scratch[5].r + scratch[4].i;
      Fout[m].i = scratch[5].i - scratch[4].r;
      Fout[m3].r = scratch[5].r - scratch[4].i;
      Fout[m3].i = scratch[5].i + scratch[4].r;
    }
    ++Fout;
  } while (--k);
}

static void kf_bfly3(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_cfg st, const size_t m) {
  size_t k = m;
  const size_t m2 = 2 * m;
  kiss_fft_cpx *tw1, *tw2;
  const auto [r, i] = st->twiddles[fstride * m];

  tw1 = tw2 = st->twiddles;

  do {
    kiss_fft_cpx scratch[5];
    C_FIXDIV(*Fout, 3);
    C_FIXDIV(Fout[m], 3);
    C_FIXDIV(Fout[m2], 3);

    C_MUL(scratch[1], Fout[m], *tw1);
    C_MUL(scratch[2], Fout[m2], *tw2);

    C_ADD(scratch[3], scratch[1], scratch[2]);
    C_SUB(scratch[0], scratch[1], scratch[2]);
    tw1 += fstride;
    tw2 += fstride * 2;

    Fout[m].r = Fout->r - HALF_OF(scratch[3].r);
    Fout[m].i = Fout->i - HALF_OF(scratch[3].i);

    C_MULBYSCALAR(scratch[0], i);

    C_ADDTO(*Fout, scratch[3]);

    Fout[m2].r = Fout[m].r + scratch[0].i;
    Fout[m2].i = Fout[m].i - scratch[0].r;

    Fout[m].r -= scratch[0].i;
    Fout[m].i += scratch[0].r;

    ++Fout;
  } while (--k);
}

static void kf_bfly5(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_cfg st, const int m) {
  kiss_fft_cpx *Fout0 = Fout, *Fout1 = Fout0 + m, *Fout2 = Fout0 + 2 * m, *Fout3 = Fout0 + 3 * m,
      *Fout4 = Fout0 + 4 * m;
  kiss_fft_cpx scratch[13];
  const kiss_fft_cpx *twiddles = st->twiddles, *tw = st->twiddles, ya = twiddles[fstride * m],
      yb = twiddles[fstride * 2 * m];

  for (int u = 0; u < m; ++u) {
    C_FIXDIV(*Fout0, 5);
    C_FIXDIV(*Fout1, 5);
    C_FIXDIV(*Fout2, 5);
    C_FIXDIV(*Fout3, 5);
    C_FIXDIV(*Fout4, 5);
    scratch[0] = *Fout0;

    C_MUL(scratch[1], *Fout1, tw[u * fstride]);
    C_MUL(scratch[2], *Fout2, tw[2 * u * fstride]);
    C_MUL(scratch[3], *Fout3, tw[3 * u * fstride]);
    C_MUL(scratch[4], *Fout4, tw[4 * u * fstride]);

    C_ADD(scratch[7], scratch[1], scratch[4]);
    C_SUB(scratch[10], scratch[1], scratch[4]);
    C_ADD(scratch[8], scratch[2], scratch[3]);
    C_SUB(scratch[9], scratch[2], scratch[3]);

    Fout0->r += scratch[7].r + scratch[8].r;
    Fout0->i += scratch[7].i + scratch[8].i;

    scratch[5].r = scratch[0].r + S_MUL(scratch[7].r, ya.r) + S_MUL(scratch[8].r, yb.r);
    scratch[5].i = scratch[0].i + S_MUL(scratch[7].i, ya.r) + S_MUL(scratch[8].i, yb.r);

    scratch[6].r = S_MUL(scratch[10].i, ya.i) + S_MUL(scratch[9].i, yb.i);
    scratch[6].i = -S_MUL(scratch[10].r, ya.i) - S_MUL(scratch[9].r, yb.i);

    C_SUB(*Fout1, scratch[5], scratch[6]);
    C_ADD(*Fout4, scratch[5], scratch[6]);

    scratch[11].r = scratch[0].r + S_MUL(scratch[7].r, yb.r) + S_MUL(scratch[8].r, ya.r);
    scratch[11].i = scratch[0].i + S_MUL(scratch[7].i, yb.r) + S_MUL(scratch[8].i, ya.r);
    scratch[12].r = -S_MUL(scratch[10].i, yb.i) + S_MUL(scratch[9].i, ya.i);
    scratch[12].i = S_MUL(scratch[10].r, yb.i) - S_MUL(scratch[9].r, ya.i);

    C_ADD(*Fout2, scratch[11], scratch[12]);
    C_SUB(*Fout3, scratch[11], scratch[12]);

    ++Fout0;
    ++Fout1;
    ++Fout2;
    ++Fout3;
    ++Fout4;
  }
}

/* perform the butterfly for one stage of a mixed radix FFT */
static void kf_bfly_generic(kiss_fft_cpx *Fout, const size_t fstride, const kiss_fft_cfg st, const int m, const int p) {
  const kiss_fft_cpx *twiddles = st->twiddles;
  const int Norig = st->nfft;

  const auto scratch = static_cast<kiss_fft_cpx *>(KISS_FFT_TMP_ALLOC(sizeof(kiss_fft_cpx) * p));

  for (int u = 0; u < m; ++u) {
    for (int q1 = 0, i = u; q1 < p; ++q1, i += m) {
      scratch[q1] = Fout[i];
      C_FIXDIV(scratch[q1], p);
    }

    for (int q1 = 0, j = u; q1 < p; ++q1, j += m) {
      int twidx = 0;
      Fout[j] = scratch[0];
      for (int q = 1; q < p; ++q) {
        kiss_fft_cpx t;
        twidx += static_cast<int>(fstride) * j;
        if (twidx >= Norig)
          twidx -= Norig;
        C_MUL(t, scratch[q], twiddles[twidx]);
        C_ADDTO(Fout[j], t);
      }
    }
  }
  KISS_FFT_TMP_FREE(scratch);
}

static void kf_work(kiss_fft_cpx *Fout, const kiss_fft_cpx *f, const size_t fstride, int in_stride, int *factors,
                    const kiss_fft_cfg st) {
  kiss_fft_cpx *Fout_beg = Fout;
  const int p = *factors++; /* the radix  */
  const int m = *factors++; /* stage's fft length/p */
  const kiss_fft_cpx *Fout_end = Fout + p * m;

#ifdef _OPENMP
  // use openmp extensions at the
  // top-level (not recursive)
  if (fstride == 1 && p <= 5) {
    int k;

// execute the p different work units in different threads
#pragma omp parallel for
    for (k = 0; k < p; ++k)
      kf_work(Fout + k * m, f + fstride * in_stride * k, fstride * p, in_stride, factors, st);
    // all threads have joined by this point

    switch (p) {
      case 2:
        kf_bfly2(Fout, fstride, st, m);
        break;
      case 3:
        kf_bfly3(Fout, fstride, st, m);
        break;
      case 4:
        kf_bfly4(Fout, fstride, st, m);
        break;
      case 5:
        kf_bfly5(Fout, fstride, st, m);
        break;
      default:
        kf_bfly_generic(Fout, fstride, st, m, p);
        break;
    }
    return;
  }
#endif

  if (m == 1) {
    do {
      *Fout = *f;
      f += fstride * in_stride;
    } while (++Fout != Fout_end);
  } else {
    do {
      // recursive call:
      // DFT of size m*p performed by doing
      // p instances of smaller DFTs of size m,
      // each one takes a decimated version of the input
      kf_work(Fout, f, fstride * p, in_stride, factors, st);
      f += fstride * in_stride;
    } while ((Fout += m) != Fout_end);
  }

  Fout = Fout_beg;

  // recombine the p smaller DFTs
  switch (p) {
    case 2:
      kf_bfly2(Fout, fstride, st, m);
      break;
    case 3:
      kf_bfly3(Fout, fstride, st, m);
      break;
    case 4:
      kf_bfly4(Fout, fstride, st, m);
      break;
    case 5:
      kf_bfly5(Fout, fstride, st, m);
      break;
    default:
      kf_bfly_generic(Fout, fstride, st, m, p);
      break;
  }
}

/**
 * @brief Implements Pollard's Rho algorithm to find a non-trivial factor of n.
 *
 * This function uses Pollard's Rho algorithm, a probabilistic integer factorization method.
 * The algorithm generates a sequence of numbers based on a quadratic recurrence and uses the
 * "tortoise and hare" technique to detect cycles. If a cycle is detected, the GCD of the
 * difference between two sequence values and n is computed, which yields a factor of n.
 *
 * If n is even, the function immediately returns 2. Otherwise, the algorithm iterates
 * until it finds a non-trivial factor or determines that no factor exists.
 *
 * @param n The integer to factor. The function will return a non-trivial factor of n.
 *
 * @return A non-trivial factor of n if found, otherwise 0 if no factor is found.
 */
int pollards_rho(const int n) {
  if (n % 2 == 0)
    return 2;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution dist(1, n - 1);

  int64_t x = dist(gen);
  int64_t y = x;
  const int64_t c = dist(gen);
  int64_t d = 1;

  while (d == 1) {
    x = (x * x + c) % n;
    y = (y * y + c) % n;
    y = (y * y + c) % n;
    d = std::gcd(std::llabs(x - y), static_cast<int64_t>(n));
  }

  return d == n ? 0 : static_cast<int>(d);
}

/**
 * @brief Factorizes a number using Pollard's Rho algorithm.
 *
 * This function attempts to factor the given number `n` by repeatedly calling the Pollard's Rho algorithm.
 * It continuously divides `n` by each found factor and stores the factors in the provided `facbuf` array.
 * This method returns the prime factors of `n` and updates the array with factor pairs:
 * each factor and the quotient of `n` after dividing by the factor.
 *
 * The factorization process continues until all factors of `n` are found,
 * or until Pollard's Rho fails to find further factors.
 *
 * @param n The integer to factor. It will be reduced during the process.
 * @param facbuf A pointer to an array where the factors will be stored.
 * The factors are stored as pairs: each factor and the quotient of n after dividing by the factor.
 */
void kf_factor(int n, int *facbuf) {
  while (n > 1) {
    const int factor = pollards_rho(n);
    if (factor == 0)
      break;

    while (n % factor == 0) {
      n /= factor;
      *facbuf++ = factor;
      *facbuf++ = n;
    }
  }
}

/*
 *
 * User-callable function to allocate all necessary storage space for the fft.
 *
 * The return value is a contiguous block of memory, allocated with malloc.  As
 * such,
 * It can be freed with free(), rather than a kiss_fft-specific function.
 * */
kiss_fft_cfg kiss_fft_alloc(const int nfft, const int inverse_fft, void *mem, size_t *lenmem) {
  kiss_fft_cfg st = nullptr;
  const size_t memneeded = sizeof(struct kiss_fft_state) + sizeof(kiss_fft_cpx) * (nfft - 1); /* twiddle factors*/

  if (lenmem == nullptr) {
    st = reinterpret_cast<kiss_fft_cfg>(new char[memneeded]);
  } else {
    if (mem != nullptr && *lenmem >= memneeded)
      st = static_cast<kiss_fft_cfg>(mem);
    *lenmem = memneeded;
  }
  if (!st) {
    return st;
  }
  st->nfft = nfft;
  st->inverse = inverse_fft;

  for (int i = 0; i < nfft; ++i) {
    double phase = -2 * pi * i / nfft;
    if (st->inverse)
      phase *= -1;
    kf_cexp(st->twiddles + i, phase);
  }

  kf_factor(nfft, st->factors);
  return st;
}

void kiss_fft_stride(const kiss_fft_cfg cfg, const kiss_fft_cpx *fin, kiss_fft_cpx *fout, const int fin_stride) {
  if (fin != fout) {
    kf_work(fout, fin, 1, fin_stride, cfg->factors, cfg);
    return;
  }
  // NOTE: this is not really an in-place FFT algorithm.
  // It just performs an out-of-place FFT into a temp buffer
  auto *tmpbuf = static_cast<kiss_fft_cpx *>(KISS_FFT_TMP_ALLOC(sizeof(kiss_fft_cpx) * cfg->nfft));
  kf_work(tmpbuf, fin, 1, fin_stride, cfg->factors, cfg);
  memcpy(fout, tmpbuf, sizeof(kiss_fft_cpx) * cfg->nfft);
  KISS_FFT_TMP_FREE(tmpbuf);
}

void kiss_fft(const kiss_fft_cfg cfg, const kiss_fft_cpx *fin, kiss_fft_cpx *fout) {
  kiss_fft_stride(cfg, fin, fout, 1);
}

/**
 * Finds the next largest integer that can be expressed as a product of
 * the prime factors 2, 3, and 5. This ensures the number is factorable
 * by these primes, making it suitable for optimized FFT computations.
 *
 * @param n The starting integer to search from.
 * @return The smallest integer greater than or equal to `n`
 *         divisible only by the primes 2, 3, and 5.
 */
int kiss_fft_next_fast_size(const int n) {
  std::vector hammingNumbers = {1}; // Start with 1 as the smallest Hamming number
  int i2 = 0, i3 = 0, i5 = 0; // Pointers for multiples of 2, 3, and 5

  while (true) {
    // Generate the next candidates by multiplying with 2, 3, and 5
    int next2 = hammingNumbers[i2] * 2;
    int next3 = hammingNumbers[i3] * 3;
    int next5 = hammingNumbers[i5] * 5;

    // Find the smallest candidate
    int nextHamming = std::min({next2, next3, next5});

    // If the candidate is >= n, return it
    if (nextHamming >= n) {
      return nextHamming;
    }

    // Add the smallest candidate to the list
    hammingNumbers.push_back(nextHamming);

    // Increment the respective pointer(s)
    if (nextHamming == next2)
      i2++;
    if (nextHamming == next3)
      i3++;
    if (nextHamming == next5)
      i5++;
  }
}
