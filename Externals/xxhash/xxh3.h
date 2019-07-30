/*
   xxHash - Extremely Fast Hash algorithm
   Development source file for `xxh3`
   Copyright (C) 2019-present, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

       * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
       * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   You can contact the author at :
   - xxHash source repository : https://github.com/Cyan4973/xxHash
*/

/* Note :
   This file is separated for development purposes.
   It will be integrated into `xxhash.c` when development phase is complete.
*/

#ifndef XXH3_H
#define XXH3_H


/* ===   Dependencies   === */

#undef XXH_INLINE_ALL   /* in case it's already defined */
#define XXH_INLINE_ALL
#include "xxhash.h"

#ifndef NDEBUG
#  define NDEBUG
#  define UNDEF_NDEBUG
#  include <assert.h>
#endif


/* ===   Compiler versions   === */

#if !(defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)   /* C99+ */
#  define restrict   /* disable */
#endif

#if defined(__GNUC__)
#  if defined(__AVX2__)
#    include <immintrin.h>
#  elif defined(__SSE2__)
#    include <emmintrin.h>
#  elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#    define inline __inline__ /* clang bug */
#    include <arm_neon.h>
#    undef inline
#  endif
#  define ALIGN(n)      __attribute__ ((aligned(n)))
#elif defined(_MSC_VER)
#  include <intrin.h>
#  define ALIGN(n)      __declspec(align(n))
#else
#  define ALIGN(n)   /* disabled */
#endif



/* ==========================================
 * Vectorization detection
 * ========================================== */
#define XXH_SCALAR 0
#define XXH_SSE2   1
#define XXH_AVX2   2
#define XXH_NEON   3
#define XXH_VSX    4

#ifndef XXH_VECTOR    /* can be defined on command line */
#  if defined(__AVX2__)
#    define XXH_VECTOR XXH_AVX2
#  elif defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64)
#    define XXH_VECTOR XXH_SSE2
#  elif defined(__GNUC__) /* msvc support maybe later */ \
  && (defined(__ARM_NEON__) || defined(__ARM_NEON)) \
  && defined(__LITTLE_ENDIAN__) /* ARM big endian is a thing */
#    define XXH_VECTOR XXH_NEON
#  elif defined(__PPC64__) && defined(__VSX__) && defined(__GNUC__)
#    define XXH_VECTOR XXH_VSX
#  else
#    define XXH_VECTOR XXH_SCALAR
#  endif
#endif

/* U64 XXH_mult32to64(U32 a, U64 b) { return (U64)a * (U64)b; } */
#ifdef _MSC_VER
#   include <intrin.h>
    /* MSVC doesn't do a good job with the mull detection. */
#   define XXH_mult32to64 __emulu
#else
#   define XXH_mult32to64(x, y) ((U64)((x) & 0xFFFFFFFF) * (U64)((y) & 0xFFFFFFFF))
#endif

/* VSX stuff */
#if XXH_VECTOR == XXH_VSX
#  include <altivec.h>
#  undef vector
typedef __vector unsigned long long U64x2;
typedef __vector unsigned U32x4;
/* Adapted from https://github.com/google/highwayhash/blob/master/highwayhash/hh_vsx.h. */
XXH_FORCE_INLINE U64x2 XXH_vsxMultOdd(U32x4 a, U32x4 b) {
    U64x2 result;
    __asm__("vmulouw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
    return result;
}
XXH_FORCE_INLINE U64x2 XXH_vsxMultEven(U32x4 a, U32x4 b) {
    U64x2 result;
    __asm__("vmuleuw %0, %1, %2" : "=v" (result) : "v" (a), "v" (b));
    return result;
}
#endif


/* ==========================================
 * XXH3 default settings
 * ========================================== */

#define KEYSET_DEFAULT_SIZE 48   /* minimum 32 */


ALIGN(64) static const U32 kKey[KEYSET_DEFAULT_SIZE] = {
    0xb8fe6c39,0x23a44bbe,0x7c01812c,0xf721ad1c,
    0xded46de9,0x839097db,0x7240a4a4,0xb7b3671f,
    0xcb79e64e,0xccc0e578,0x825ad07d,0xccff7221,
    0xb8084674,0xf743248e,0xe03590e6,0x813a264c,
    0x3c2852bb,0x91c300cb,0x88d0658b,0x1b532ea3,
    0x71644897,0xa20df94e,0x3819ef46,0xa9deacd8,
    0xa8fa763f,0xe39c343f,0xf9dcbbc7,0xc70b4f1d,
    0x8a51e04b,0xcdb45931,0xc89f7ec9,0xd9787364,

    0xeac5ac83,0x34d3ebc3,0xc581a0ff,0xfa1363eb,
    0x170ddd51,0xb7f0da49,0xd3165526,0x29d4689e,
    0x2b16be58,0x7d47a1fc,0x8ff8b8d1,0x7ad031ce,
    0x45cb3a8f,0x95160428,0xafd7fbca,0xbb4b407e,
};


#if defined(__GNUC__) && defined(__i386__)
/* GCC is stupid and tries to vectorize this.
 * This tells GCC that it is wrong. */
__attribute__((__target__("no-sse")))
#endif
static U64
XXH3_mul128_fold64(U64 ll1, U64 ll2)
{
#if defined(__SIZEOF_INT128__) || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)

    __uint128_t lll = (__uint128_t)ll1 * ll2;
    return (U64)lll ^ (U64)(lll >> 64);

#elif defined(_M_X64) || defined(_M_IA64)

#ifndef _MSC_VER
#   pragma intrinsic(_umul128)
#endif
    U64 llhigh;
    U64 const lllow = _umul128(ll1, ll2, &llhigh);
    return lllow ^ llhigh;

    /* We have to do it out manually on 32-bit.
     * This is a modified, unrolled, widened, and optimized version of the
     * mulqdu routine from Hacker's Delight.
     *
     *   https://www.hackersdelight.org/hdcodetxt/mulqdu.c.txt
     *
     * This was modified to use U32->U64 multiplication instead
     * of U16->U32, to add the high and low values in the end,
     * be endian-independent, and I added a partial assembly
     * implementation for ARM. */

    /* An easy 128-bit folding multiply on ARMv6T2 and ARMv7-A/R can be done with
     * the mighty umaal (Unsigned Multiply Accumulate Accumulate Long) which takes 4 cycles
     * or less, doing a long multiply and adding two 32-bit integers:
     *
     *     void umaal(U32 *RdLo, U32 *RdHi, U32 Rn, U32 Rm)
     *     {
     *         U64 prodAcc = (U64)Rn * (U64)Rm;
     *         prodAcc += *RdLo;
     *         prodAcc += *RdHi;
     *         *RdLo = prodAcc & 0xFFFFFFFF;
     *         *RdHi = prodAcc >> 32;
     *     }
     *
     * This is compared to umlal which adds to a single 64-bit integer:
     *
     *     void umlal(U32 *RdLo, U32 *RdHi, U32 Rn, U32 Rm)
     *     {
     *         U64 prodAcc = (U64)Rn * (U64)Rm;
     *         prodAcc += (*RdLo | ((U64)*RdHi << 32);
     *         *RdLo = prodAcc & 0xFFFFFFFF;
     *         *RdHi = prodAcc >> 32;
     *     }
     *
     * Getting the compiler to emit them is like pulling teeth, and checking
     * for it is annoying because ARMv7-M lacks this instruction. However, it
     * is worth it, because this is an otherwise expensive operation. */

     /* GCC-compatible, ARMv6t2 or ARMv7+, non-M variant, and 32-bit */
#elif defined(__GNUC__) /* GCC-compatible */ \
    && defined(__ARM_ARCH) && !defined(__aarch64__) && !defined(__arm64__) /* 32-bit ARM */\
    && !defined(__ARM_ARCH_7M__) /* <- Not ARMv7-M  vv*/ \
        && !(defined(__TARGET_ARCH_ARM) && __TARGET_ARCH_ARM == 0 && __TARGET_ARCH_THUMB == 4) \
    && (defined(__ARM_ARCH_6T2__) || __ARM_ARCH > 6) /* ARMv6T2 or later */

    U32 w[4] = { 0 };
    U32 u[2] = { (U32)(ll1 >> 32), (U32)ll1 };
    U32 v[2] = { (U32)(ll2 >> 32), (U32)ll2 };
    U32 k;

    /* U64 t = (U64)u[1] * (U64)v[1];
     * w[3] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umull %0, %1, %2, %3"
            : "=r" (w[3]), "=r" (k)
            : "r" (u[1]), "r" (v[1]));

    /* t = (U64)u[0] * (U64)v[1] + w[2] + k;
     * w[2] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umaal %0, %1, %2, %3"
            : "+r" (w[2]), "+r" (k)
            : "r" (u[0]), "r" (v[1]));
    w[1] = k;
    k = 0;

    /* t = (U64)u[1] * (U64)v[0] + w[2] + k;
     * w[2] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umaal %0, %1, %2, %3"
            : "+r" (w[2]), "+r" (k)
            : "r" (u[1]), "r" (v[0]));

    /* t = (U64)u[0] * (U64)v[0] + w[1] + k;
     * w[1] = t & 0xFFFFFFFF;
     * k = t >> 32; */
    __asm__("umaal %0, %1, %2, %3"
            : "+r" (w[1]), "+r" (k)
            : "r" (u[0]), "r" (v[0]));
    w[0] = k;

    return (w[1] | ((U64)w[0] << 32)) ^ (w[3] | ((U64)w[2] << 32));

#else /* Portable scalar version */

    /* emulate 64x64->128b multiplication, using four 32x32->64 */
    U32 const h1 = (U32)(ll1 >> 32);
    U32 const h2 = (U32)(ll2 >> 32);
    U32 const l1 = (U32)ll1;
    U32 const l2 = (U32)ll2;

    U64 const llh  = XXH_mult32to64(h1, h2);
    U64 const llm1 = XXH_mult32to64(l1, h2);
    U64 const llm2 = XXH_mult32to64(h1, l2);
    U64 const lll  = XXH_mult32to64(l1, l2);

    U64 const t = lll + (llm1 << 32);
    U64 const carry1 = t < lll;

    U64 const lllow = t + (llm2 << 32);
    U64 const carry2 = lllow < t;
    U64 const llhigh = llh + (llm1 >> 32) + (llm2 >> 32) + carry1 + carry2;

    return llhigh ^ lllow;

#endif
}


static XXH64_hash_t XXH3_avalanche(U64 h64)
{
    h64 ^= h64 >> 37;
    h64 *= PRIME64_3;
    h64 ^= h64 >> 32;
    return h64;
}


/* ==========================================
 * Short keys
 * ========================================== */

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_1to3_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len > 0 && len <= 3);
    assert(keyPtr != NULL);
    {   const U32* const key32 = (const U32*) keyPtr;
        BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const l1 = (U32)(c1) + ((U32)(c2) << 8);
        U32  const l2 = (U32)(len) + ((U32)(c3) << 2);
        U64  const ll11 = XXH_mult32to64(l1 + (U32)seed + key32[0],
                                         l2 + (U32)(seed >> 32) + key32[1]);
        return XXH3_avalanche(ll11);
    }
}

XXH_FORCE_INLINE U64
XXH3_readKey64(const void* ptr)
{
    assert(((size_t)ptr & 7) == 0);   /* aligned on 8-bytes boundaries */
    if (XXH_CPU_LITTLE_ENDIAN) {
        return *(const U64*)ptr;
    } else {
        const U32* const ptr32 = (const U32*)ptr;
        return (U64)ptr32[0] + (((U64)ptr32[1]) << 32);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_4to8_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(key != NULL);
    assert(len >= 4 && len <= 8);
    {   U32 const in1 = XXH_readLE32(data);
        U32 const in2 = XXH_readLE32((const BYTE*)data + len - 4);
        U64 const in64 = in1 + ((U64)in2 << 32);
        U64 const keyed = in64 ^ (XXH3_readKey64(keyPtr) + seed);
        U64 const mix64 = len + XXH3_mul128_fold64(keyed, PRIME64_1);
        return XXH3_avalanche(mix64);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_9to16_64b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(key != NULL);
    assert(len >= 9 && len <= 16);
    {   const U64* const key64 = (const U64*) keyPtr;
        U64 const ll1 = XXH_readLE64(data) ^ (XXH3_readKey64(key64) + seed);
        U64 const ll2 = XXH_readLE64((const BYTE*)data + len - 8) ^ (XXH3_readKey64(key64+1) - seed);
        U64 const acc = len + (ll1 + ll2) + XXH3_mul128_fold64(ll1, ll2);
        return XXH3_avalanche(acc);
    }
}

XXH_FORCE_INLINE XXH64_hash_t
XXH3_len_0to16_64b(const void* data, size_t len, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_64b(data, len, kKey, seed);
        if (len >= 4) return XXH3_len_4to8_64b(data, len, kKey, seed);
        if (len) return XXH3_len_1to3_64b(data, len, kKey, seed);
        return seed;
    }
}


/* ===    Long Keys    === */

#define STRIPE_LEN 64
#define STRIPE_ELTS (STRIPE_LEN / sizeof(U32))
#define ACC_NB (STRIPE_LEN / sizeof(U64))

XXH_FORCE_INLINE void
XXH3_accumulate_512(void* restrict acc, const void *restrict data, const void *restrict key)
{
#if (XXH_VECTOR == XXH_AVX2)

    assert(((size_t)acc) & 31 == 0);
    {   ALIGN(32) __m256i* const xacc  =       (__m256i *) acc;
        const     __m256i* const xdata = (const __m256i *) data;
        const     __m256i* const xkey  = (const __m256i *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i const d   = _mm256_loadu_si256 (xdata+i);
            __m256i const k   = _mm256_loadu_si256 (xkey+i);
            __m256i const dk  = _mm256_xor_si256 (d,k);                                  /* uint32 dk[8]  = {d0+k0, d1+k1, d2+k2, d3+k3, ...} */
            __m256i const res = _mm256_mul_epu32 (dk, _mm256_shuffle_epi32 (dk, 0x31));  /* uint64 res[4] = {dk0*dk1, dk2*dk3, ...} */
            __m256i const add = _mm256_add_epi64(d, xacc[i]);
            xacc[i]  = _mm256_add_epi64(res, add);
        }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    assert(((size_t)acc) & 15 == 0);
    {   ALIGN(16) __m128i* const xacc  =       (__m128i *) acc;
        const     __m128i* const xdata = (const __m128i *) data;
        const     __m128i* const xkey  = (const __m128i *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i const d   = _mm_loadu_si128 (xdata+i);
            __m128i const k   = _mm_loadu_si128 (xkey+i);
            __m128i const dk  = _mm_xor_si128 (d,k);                                 /* uint32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */
            __m128i const res = _mm_mul_epu32 (dk, _mm_shuffle_epi32 (dk, 0x31));    /* uint64 res[2] = {dk0*dk1,dk2*dk3} */
            __m128i const add = _mm_add_epi64(d, xacc[i]);
            xacc[i]  = _mm_add_epi64(res, add);
        }
    }

#elif (XXH_VECTOR == XXH_NEON)

    assert(((size_t)acc) & 15 == 0);
    {
        ALIGN(16) uint64x2_t* const xacc  =     (uint64x2_t *) acc;
        /* We don't use a uint32x4_t pointer because it causes bus errors on ARMv7. */
        uint32_t const* const xdata = (const uint32_t *) data;
        uint32_t const* const xkey  = (const uint32_t *) key;

        size_t i;
        for (i=0; i < STRIPE_LEN / sizeof(uint64x2_t); i++) {
#if !defined(__aarch64__) && !defined(__arm64__) && defined(__GNUC__) /* ARM32-specific hack */
            /* vzip on ARMv7 Clang generates a lot of vmovs (technically vorrs) without this.
             * vzip on 32-bit ARM NEON will overwrite the original register, and I think that Clang
             * assumes I don't want to destroy it and tries to make a copy. This slows down the code
             * a lot.
             * aarch64 not only uses an entirely different syntax, but it requires three
             * instructions...
             *    ext    v1.16B, v0.16B, #8    // select high bits because aarch64 can't address them directly
             *    zip1   v3.2s, v0.2s, v1.2s   // first zip
             *    zip2   v2.2s, v0.2s, v1.2s   // second zip
             * ...to do what ARM does in one:
             *    vzip.32 d0, d1               // Interleave high and low bits and overwrite. */

            /* data_vec = xdata[i]; */
            uint32x4_t const data_vec    = vld1q_u32(xdata + (i * 4));
            /* key_vec  = xkey[i];  */
            uint32x4_t const key_vec     = vld1q_u32(xkey  + (i * 4));
            /* data_key = data_vec ^ key_vec; */
            uint32x4_t       data_key;
            /* Add first to prevent register swaps */
            /* xacc[i] += data_vec; */
            xacc[i] = vaddq_u64(xacc[i], vreinterpretq_u64_u32(data_vec));

            data_key = veorq_u32(data_vec, key_vec);

            /* Here's the magic. We use the quirkiness of vzip to shuffle data_key in place.
             * shuffle: data_key[0, 1, 2, 3] = data_key[0, 2, 1, 3] */
            __asm__("vzip.32 %e0, %f0" : "+w" (data_key));
            /* xacc[i] += (uint64x2_t) data_key[0, 1] * (uint64x2_t) data_key[2, 3]; */
            xacc[i] = vmlal_u32(xacc[i], vget_low_u32(data_key), vget_high_u32(data_key));
#else
            /* On aarch64, vshrn/vmovn seems to be equivalent to, if not faster than, the vzip method. */

            /* data_vec = xdata[i]; */
            uint32x4_t const data_vec    = vld1q_u32(xdata + (i * 4));
            /* key_vec  = xkey[i];  */
            uint32x4_t const key_vec     = vld1q_u32(xkey  + (i * 4));
            /* data_key = data_vec ^ key_vec; */
            uint32x4_t const data_key    = veorq_u32(data_vec, key_vec);
            /* data_key_lo = (uint32x2_t) (data_key & 0xFFFFFFFF); */
            uint32x2_t const data_key_lo = vmovn_u64  (vreinterpretq_u64_u32(data_key));
            /* data_key_hi = (uint32x2_t) (data_key >> 32); */
            uint32x2_t const data_key_hi = vshrn_n_u64 (vreinterpretq_u64_u32(data_key), 32);
            /* xacc[i] += data_vec; */
            xacc[i] = vaddq_u64 (xacc[i], vreinterpretq_u64_u32(data_vec));
            /* xacc[i] += (uint64x2_t) data_key_lo * (uint64x2_t) data_key_hi; */
            xacc[i] = vmlal_u32 (xacc[i], data_key_lo, data_key_hi);
#endif
        }
    }

#elif XXH_VECTOR == XXH_VSX
          U64x2* const xacc =        (U64x2*) acc;
    U64x2 const* const xdata = (U64x2 const*) data;
    U64x2 const* const xkey  = (U64x2 const*) key;
    U64x2 const v32 = { 32,  32 };

    size_t i;
    for (i = 0; i < STRIPE_LEN / sizeof(U64x2); i++) {
        /* data_vec = xdata[i]; */
        /* key_vec = xkey[i]; */
#ifdef __BIG_ENDIAN__
        /* byteswap */
        U64x2 const data_vec = vec_revb(vec_vsx_ld(0, xdata + i));
        /* swap 32-bit words */
        U64x2 const key_vec = vec_rl(vec_vsx_ld(0, xkey + i), v32);
#else
        U64x2 const data_vec = vec_vsx_ld(0, xdata + i);
        U64x2 const key_vec = vec_vsx_ld(0, xkey + i);
#endif
        U64x2 data_key = data_vec ^ key_vec;
        /* shuffled = (data_key << 32) | (data_key >> 32); */
        U32x4 shuffled = (U32x4)vec_rl(data_key, v32);
        /* product = ((U64x2)data_key & 0xFFFFFFFF) * ((U64x2)shuffled & 0xFFFFFFFF); */
        U64x2 product = XXH_vsxMultOdd((U32x4)data_key, shuffled);

        xacc[i] += product;
        xacc[i] += data_vec;
    }
#else   /* scalar variant of Accumulator - universal */
    ALIGN(16) U64* const xacc  =       (U64*) acc;   /* presumed aligned */
    const U32* const xdata = (const U32*) data;
    const U32* const xkey  = (const U32*) key;
    size_t i;

    for (i=0; i < ACC_NB; i++) {
        U64 const data_val = XXH_readLE64(xdata + 2 * i);
        U64 const key_val = XXH3_readKey64(xkey + 2 * i);
        U64 const data_key  = key_val ^ data_val;
        xacc[i] += XXH_mult32to64(data_key & 0xFFFFFFFF, data_key >> 32);
        xacc[i] += data_val;
    }
#endif
}

static void XXH3_scrambleAcc(void* restrict acc, const void* restrict key)
{
#if (XXH_VECTOR == XXH_AVX2)

    assert(((size_t)acc) & 31 == 0);
    {   ALIGN(32) __m256i* const xacc = (__m256i*) acc;
        const     __m256i* const xkey  = (const __m256i *) key;
        const __m256i k1 = _mm256_set1_epi32((int)PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m256i); i++) {
            __m256i data = xacc[i];
            __m256i const shifted = _mm256_srli_epi64(data, 47);
            data = _mm256_xor_si256(data, shifted);

            {   __m256i const k   = _mm256_loadu_si256 (xkey+i);
                __m256i const dk  = _mm256_xor_si256   (data, k);          /* U32 dk[4]  = {d0+k0, d1+k1, d2+k2, d3+k3} */

                __m256i const dk1 = _mm256_mul_epu32 (dk, k1);

                __m256i const d2  = _mm256_shuffle_epi32 (dk, 0x31);
                __m256i const dk2 = _mm256_mul_epu32 (d2, k1);
                __m256i const dk2h= _mm256_slli_epi64 (dk2, 32);

                xacc[i] = _mm256_add_epi64(dk1, dk2h);
        }   }
    }

#elif (XXH_VECTOR == XXH_SSE2)

    {   ALIGN(16) __m128i* const xacc = (__m128i*) acc;
        const     __m128i* const xkey  = (const __m128i *) key;
        const __m128i k1 = _mm_set1_epi32((int)PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(__m128i); i++) {
            __m128i data = xacc[i];
            __m128i const shifted = _mm_srli_epi64(data, 47);
            data = _mm_xor_si128(data, shifted);

            {   __m128i const k   = _mm_loadu_si128 (xkey+i);
                __m128i const dk  = _mm_xor_si128   (data,k);

                __m128i const dk1 = _mm_mul_epu32 (dk,k1);

                __m128i const d2  = _mm_shuffle_epi32 (dk, 0x31);
                __m128i const dk2 = _mm_mul_epu32 (d2,k1);
                __m128i const dk2h= _mm_slli_epi64(dk2, 32);

                xacc[i] = _mm_add_epi64(dk1, dk2h);
        }   }
    }

#elif (XXH_VECTOR == XXH_NEON)

    assert(((size_t)acc) & 15 == 0);
    {
            uint64x2_t* const xacc =     (uint64x2_t*) acc;

        uint32_t const* const xkey = (uint32_t const*) key;

        uint32x2_t const prime     = vdup_n_u32 (PRIME32_1);

        size_t i;
        for (i=0; i < STRIPE_LEN/sizeof(uint64x2_t); i++) {
            /* data_vec = xacc[i] ^ (xacc[i] >> 47); */
            uint64x2_t const   acc_vec  = xacc[i];
            uint64x2_t const   shifted  = vshrq_n_u64 (acc_vec, 47);
            uint64x2_t const   data_vec = veorq_u64   (acc_vec, shifted);

            /* key_vec  = xkey[i]; */
            uint32x4_t const   key_vec  = vld1q_u32   (xkey + (i * 4));
            /* data_key = data_vec ^ key_vec; */
            uint32x4_t const   data_key = veorq_u32   (vreinterpretq_u32_u64(data_vec), key_vec);
            /* shuffled = { data_key[0, 2], data_key[1, 3] }; */
            uint32x2x2_t const shuffled = vzip_u32    (vget_low_u32(data_key), vget_high_u32(data_key));

            /* data_key *= PRIME32_1 */

            /* prod_hi = (data_key >> 32) * PRIME32_1; */
            uint64x2_t const   prod_hi = vmull_u32    (shuffled.val[1], prime);
            /* xacc[i] = prod_hi << 32; */
            xacc[i] = vshlq_n_u64(prod_hi, 32);
            /* xacc[i] += (prod_hi & 0xFFFFFFFF) * PRIME32_1; */
            xacc[i] = vmlal_u32(xacc[i], shuffled.val[0], prime);
        }
    }

#elif (XXH_VECTOR == XXH_VSX)
          U64x2* const xacc =       (U64x2*) acc;
    const U64x2* const xkey = (const U64x2*) key;
    /* constants */
    U64x2 const v32  = { 32, 32 };
    U64x2 const v47 = { 47, 47 };
    U32x4 const prime = { PRIME32_1, PRIME32_1, PRIME32_1, PRIME32_1 };
    size_t i;

    for (i = 0; i < STRIPE_LEN / sizeof(U64x2); i++) {
        U64x2 const acc_vec  = xacc[i];
        U64x2 const data_vec = acc_vec ^ (acc_vec >> v47);
        /* key_vec = xkey[i]; */
#ifdef __BIG_ENDIAN__
        /* swap 32-bit words */
        U64x2 const key_vec  = vec_rl(vec_vsx_ld(0, xkey + i), v32);
#else
        U64x2 const key_vec  = vec_vsx_ld(0, xkey + i);
#endif
        U64x2 const data_key = data_vec ^ key_vec;

        /* data_key *= PRIME32_1 */

        /* prod_lo = ((U64x2)data_key & 0xFFFFFFFF) * ((U64x2)prime & 0xFFFFFFFF);  */
        U64x2 const prod_lo  = XXH_vsxMultOdd((U32x4)data_key, prime);
        /* prod_hi = ((U64x2)data_key >> 32) * ((U64x2)prime >> 32);  */
        U64x2 const prod_hi  = XXH_vsxMultEven((U32x4)data_key, prime);
        xacc[i] = prod_lo + (prod_hi << v32);
    }
#else   /* scalar variant of Scrambler - universal */

          U64* const xacc =       (U64*) acc;
    const U32* const xkey = (const U32*) key;

    int i;
    assert(((size_t)acc) & 7 == 0);
    for (i=0; i < (int)ACC_NB; i++) {
        U64 const key64 = XXH3_readKey64(xkey + 2*i);
        U64 acc64 = xacc[i];
        acc64 ^= acc64 >> 47;
        acc64 ^= key64;
        acc64 *= PRIME32_1;
        xacc[i] = acc64;
    }

#endif
}

static void XXH3_accumulate(U64* acc, const void* restrict data, const U32* restrict key, size_t nbStripes)
{
    size_t n;
    /* Clang doesn't unroll this loop without the pragma. Unrolling can be up to 1.4x faster. */
#if defined(__clang__) && !defined(__OPTIMIZE_SIZE__) && !defined(__ARM_ARCH)
#  pragma clang loop unroll(enable)
#endif
    for (n = 0; n < nbStripes; n++ ) {
        XXH3_accumulate_512(acc,
               (const BYTE*)data + n*STRIPE_LEN,
                            key + n*2);
    }
}

static void
XXH3_hashLong(U64* acc, const void* data, size_t len)
{
    #define NB_KEYS ((KEYSET_DEFAULT_SIZE - STRIPE_ELTS) / 2)

    size_t const block_len = STRIPE_LEN * NB_KEYS;
    size_t const nb_blocks = len / block_len;

    size_t n;
    for (n = 0; n < nb_blocks; n++) {
        XXH3_accumulate(acc, (const BYTE*)data + n*block_len, kKey, NB_KEYS);
        XXH3_scrambleAcc(acc, kKey + (KEYSET_DEFAULT_SIZE - STRIPE_ELTS));
    }

    /* last partial block */
    assert(len > STRIPE_LEN);
    {   size_t const nbStripes = (len % block_len) / STRIPE_LEN;
        assert(nbStripes < NB_KEYS);
        XXH3_accumulate(acc, (const BYTE*)data + nb_blocks*block_len, kKey, nbStripes);

        /* last stripe */
        if (len & (STRIPE_LEN - 1)) {
            const BYTE* const p = (const BYTE*) data + len - STRIPE_LEN;
            XXH3_accumulate_512(acc, p, kKey + nbStripes*2);
    }   }
}


XXH_FORCE_INLINE U64 XXH3_mix2Accs(const U64* acc, const void* key)
{
    const U64* const key64 = (const U64*)key;
    return XXH3_mul128_fold64(
               acc[0] ^ XXH3_readKey64(key64),
               acc[1] ^ XXH3_readKey64(key64+1) );
}

static XXH64_hash_t XXH3_mergeAccs(const U64* acc, const U32* key, U64 start)
{
    U64 result64 = start;

    result64 += XXH3_mix2Accs(acc+0, key+0);
    result64 += XXH3_mix2Accs(acc+2, key+4);
    result64 += XXH3_mix2Accs(acc+4, key+8);
    result64 += XXH3_mix2Accs(acc+6, key+12);

    return XXH3_avalanche(result64);
}


XXH_FORCE_INLINE void XXH3_initKeySeed(U32* key, U64 seed64)
{
    U32 const seed1 = (U32)seed64;
    U32 const seed2 = (U32)(seed64 >> 32);
    int i;
    assert(KEYSET_DEFAULT_SIZE & 3 == 0);
    for (i=0; i < KEYSET_DEFAULT_SIZE; i+=4) {
        key[i+0] = kKey[i+0] + seed1;
        key[i+1] = kKey[i+1] - seed2;
        key[i+2] = kKey[i+2] + seed2;
        key[i+3] = kKey[i+3] - seed1;
    }
}

XXH_NO_INLINE XXH64_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_64b(const void* data, size_t len, XXH64_hash_t seed)
{
    ALIGN(64) U64 acc[ACC_NB] = { seed, PRIME64_1, PRIME64_2, PRIME64_3, PRIME64_4, PRIME64_5, (U64)0 - seed, 0 };
    ALIGN(64) U32 key[KEYSET_DEFAULT_SIZE];

    XXH3_initKeySeed(key, seed);

    XXH3_hashLong(acc, data, len);

    /* converge into final hash */
    assert(sizeof(acc) == 64);
    return XXH3_mergeAccs(acc, key, (U64)len * PRIME64_1);
}


XXH_FORCE_INLINE U64 XXH3_mix16B(const void* data, const void* key, U64 seed64)
{
    const U64* const key64 = (const U64*)key;
    U64 const ll1 = XXH_readLE64(data);
    U64 const ll2 = XXH_readLE64((const BYTE*)data+8);
    return XXH3_mul128_fold64(
               ll1 ^ (XXH3_readKey64(key64)   + seed64),
               ll2 ^ (XXH3_readKey64(key64+1) - seed64) ) ;
}


/* ===   Public entry point   === */

XXH_PUBLIC_API XXH64_hash_t
XXH3_64bits_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    const BYTE* const p = (const BYTE*)data;
    const char* const key = (const char*)kKey;

    if (len <= 16) return XXH3_len_0to16_64b(data, len, seed);

    {   U64 acc = len * PRIME64_1;
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    if (len > 128) return XXH3_hashLong_64b(data, len, seed);

                    acc += XXH3_mix16B(p+48, key+96, seed);
                    acc += XXH3_mix16B(p+len-64, key+112, seed);
                }

                acc += XXH3_mix16B(p+32, key+64, seed);
                acc += XXH3_mix16B(p+len-48, key+80, seed);
            }

            acc += XXH3_mix16B(p+16, key+32, seed);
            acc += XXH3_mix16B(p+len-32, key+48, seed);
        }

        acc += XXH3_mix16B(p+0, key+0, seed);
        acc += XXH3_mix16B(p+len-16, key+16, seed);

        return XXH3_avalanche(acc);
    }
}


XXH_PUBLIC_API XXH64_hash_t XXH3_64bits(const void* data, size_t len)
{
    return XXH3_64bits_withSeed(data, len, 0);
}



/* ==========================================
 * XXH3 128 bits (=> XXH128)
 * ========================================== */

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_1to3_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len > 0 && len <= 3);
    assert(keyPtr != NULL);
    {   const U32* const key32 = (const U32*) keyPtr;
        BYTE const c1 = ((const BYTE*)data)[0];
        BYTE const c2 = ((const BYTE*)data)[len >> 1];
        BYTE const c3 = ((const BYTE*)data)[len - 1];
        U32  const l1 = (U32)(c1) + ((U32)(c2) << 8);
        U32  const l2 = (U32)(len) + ((U32)(c3) << 2);
        U64  const ll11 = XXH_mult32to64((unsigned int)(l1 + seed + key32[0]), (unsigned int)(l2 + key32[1]));
        U64  const ll12 = XXH_mult32to64((unsigned int)(l1 + key32[2]), (unsigned int)(l2 - seed + key32[3]));
        XXH128_hash_t const h128 = { XXH3_avalanche(ll11), XXH3_avalanche(ll12) };
        return h128;
    }
}


XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_4to8_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len >= 4 && len <= 8);
    {   const U32* const key32 = (const U32*) keyPtr;
        U32 const l1 = XXH_readLE32(data) + (U32)seed + key32[0];
        U32 const l2 = XXH_readLE32((const BYTE*)data + len - 4) + (U32)(seed >> 32) + key32[1];
        U64 const acc1 = len + l1 + ((U64)l2 << 32) + XXH_mult32to64(l1, l2);
        U64 const acc2 = len*PRIME64_1 + l1*PRIME64_2 + l2*PRIME64_3;
        {   XXH128_hash_t const h128 = { XXH3_avalanche(acc1), XXH3_avalanche(acc2) };
            return h128;
        }
    }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_9to16_128b(const void* data, size_t len, const void* keyPtr, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(key != NULL);
    assert(len >= 9 && len <= 16);
    {   const U64* const key64 = (const U64*) keyPtr;
        U64 acc1 = PRIME64_1 * ((U64)len + seed);
        U64 acc2 = PRIME64_2 * ((U64)len - seed);
        U64 const ll1 = XXH_readLE64(data);
        U64 const ll2 = XXH_readLE64((const BYTE*)data + len - 8);
        acc1 += XXH3_mul128_fold64(ll1 + XXH3_readKey64(key64+0), ll2 + XXH3_readKey64(key64+1));
        acc2 += XXH3_mul128_fold64(ll1 + XXH3_readKey64(key64+2), ll2 + XXH3_readKey64(key64+3));
        {   XXH128_hash_t const h128 = { XXH3_avalanche(acc1), XXH3_avalanche(acc2) };
            return h128;
        }
    }
}

XXH_FORCE_INLINE XXH128_hash_t
XXH3_len_0to16_128b(const void* data, size_t len, XXH64_hash_t seed)
{
    assert(data != NULL);
    assert(len <= 16);
    {   if (len > 8) return XXH3_len_9to16_128b(data, len, kKey, seed);
        if (len >= 4) return XXH3_len_4to8_128b(data, len, kKey, seed);
        if (len) return XXH3_len_1to3_128b(data, len, kKey, seed);
        {   XXH128_hash_t const h128 = { seed, (XXH64_hash_t)0 - seed };
            return h128;
        }
    }
}

XXH_NO_INLINE XXH128_hash_t    /* It's important for performance that XXH3_hashLong is not inlined. Not sure why (uop cache maybe ?), but difference is large and easily measurable */
XXH3_hashLong_128b(const void* data, size_t len, XXH64_hash_t seed)
{
    ALIGN(64) U64 acc[ACC_NB] = { seed, PRIME64_1, PRIME64_2, PRIME64_3, PRIME64_4, PRIME64_5, (U64)0 - seed, 0 };
    assert(len > 128);

    XXH3_hashLong(acc, data, len);

    /* converge into final hash */
    assert(sizeof(acc) == 64);
    {   U64 const low64 = XXH3_mergeAccs(acc, kKey, (U64)len * PRIME64_1);
        U64 const high64 = XXH3_mergeAccs(acc, kKey+16, ((U64)len+1) * PRIME64_2);
        XXH128_hash_t const h128 = { low64, high64 };
        return h128;
    }
}

XXH_PUBLIC_API XXH128_hash_t
XXH3_128bits_withSeed(const void* data, size_t len, XXH64_hash_t seed)
{
    if (len <= 16) return XXH3_len_0to16_128b(data, len, seed);

    {   U64 acc1 = PRIME64_1 * (len + seed);
        U64 acc2 = 0;
        const BYTE* const p = (const BYTE*)data;
        const char* const key = (const char*)kKey;
        if (len > 32) {
            if (len > 64) {
                if (len > 96) {
                    if (len > 128) return XXH3_hashLong_128b(data, len, seed);

                    acc1 += XXH3_mix16B(p+48, key+96, seed);
                    acc2 += XXH3_mix16B(p+len-64, key+112, seed);
                }

                acc1 += XXH3_mix16B(p+32, key+64, seed);
                acc2 += XXH3_mix16B(p+len-48, key+80, seed);
            }

            acc1 += XXH3_mix16B(p+16, key+32, seed);
            acc2 += XXH3_mix16B(p+len-32, key+48, seed);

        }

        acc1 += XXH3_mix16B(p+0, key+0, seed);
        acc2 += XXH3_mix16B(p+len-16, key+16, seed);

        {   U64 const part1 = acc1 + acc2;
            U64 const part2 = (acc1 * PRIME64_3) + (acc2 * PRIME64_4) + ((len - seed) * PRIME64_2);
            XXH128_hash_t const h128 = { XXH3_avalanche(part1), (XXH64_hash_t)0 - XXH3_avalanche(part2) };
            return h128;
        }
    }
}


XXH_PUBLIC_API XXH128_hash_t XXH3_128bits(const void* data, size_t len)
{
    return XXH3_128bits_withSeed(data, len, 0);
}


XXH_PUBLIC_API XXH128_hash_t XXH128(const void* data, size_t len, XXH64_hash_t seed)
{
    return XXH3_128bits_withSeed(data, len, seed);
}

#ifdef UNDEF_NDEBUG
#  undef NDEBUG
#endif

#endif  /* XXH3_H */
