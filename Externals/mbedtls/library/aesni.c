/*
 *  AES-NI support functions
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/*
 * [AES-WP] http://software.intel.com/en-us/articles/intel-advanced-encryption-standard-aes-instructions-set
 * [CLMUL-WP] https://www.intel.com/content/dam/develop/external/us/en/documents/clmul-wp-rev-2-02-2014-04-20.pdf
 */

#include "common.h"

#if defined(MBEDTLS_AESNI_C)

#include "mbedtls/aesni.h"

#include <string.h>

#if defined(MBEDTLS_HAVE_X86_64)

#ifdef _MSC_VER
#include <intrin.h>
#define ATTRIBUTE_TARGET_AES
#else
#include <cpuid.h>
#define ATTRIBUTE_TARGET_AES __attribute__((target("aes")))
#endif

#include <emmintrin.h>
#include <immintrin.h>

/*
 * AES-NI support detection routine
 */
int mbedtls_aesni_has_support( unsigned int what )
{
    static int done = 0;
    static unsigned int c = 0;

    if( ! done )
    {
#if defined(_MSC_VER)
        // gets filled with [eax, ebx, ecx, edx]
        int info[4];
        __cpuid(info, 1);
        c = (unsigned int)info[2];
#else
        unsigned int eax, ebx, ecx, edx;
        __cpuid(1, eax, ebx, ecx, edx);
        c = ecx;
#endif

        done = 1;
    }

    return( ( c & what ) != 0 );
}

#define PCLMULQDQ   ".byte 0x66,0x0F,0x3A,0x44,"

#define xmm0_xmm0   "0xC0"
#define xmm0_xmm1   "0xC8"
#define xmm0_xmm2   "0xD0"
#define xmm0_xmm3   "0xD8"
#define xmm0_xmm4   "0xE0"
#define xmm1_xmm0   "0xC1"
#define xmm1_xmm2   "0xD1"

/*
 * AES-NI AES-ECB block en(de)cryption
 */
ATTRIBUTE_TARGET_AES
int mbedtls_aesni_crypt_ecb( mbedtls_aes_context *ctx,
                     int mode,
                     const unsigned char input[16],
                     unsigned char output[16] )
{
    __m128i block = _mm_loadu_si128((const __m128i *)input);
    __m128i *rk = (__m128i *)ctx->rk;

    // round 0
    block = _mm_xor_si128(block, _mm_loadu_si128(rk++));

    if (mode == MBEDTLS_AES_DECRYPT) {
        for (int i = ctx->nr - 1; i > 0; --i) {
            block = _mm_aesdec_si128(block, _mm_loadu_si128(rk++));
        }
        block = _mm_aesdeclast_si128(block, _mm_loadu_si128(rk));
    } else {
        for (int i = ctx->nr - 1; i > 0; --i) {
            block = _mm_aesenc_si128(block, _mm_loadu_si128(rk++));
        }
        block = _mm_aesenclast_si128(block, _mm_loadu_si128(rk));
    }

    _mm_storeu_si128((__m128i *)output, block);

    return( 0 );
}

/*
 * GCM multiplication: c = a times b in GF(2^128)
 * Based on [CLMUL-WP] algorithms 1 (with equation 27) and 5.
 */
ATTRIBUTE_TARGET_AES
void mbedtls_aesni_gcm_mult( unsigned char c[16],
                     const unsigned char a[16],
                     const unsigned char b[16] )
{
    (void)a;
    (void)b;
    (void)c;
//    unsigned char aa[16], bb[16], cc[16];
//    size_t i;
//
//    /* The inputs are in big-endian order, so byte-reverse them */
//    for( i = 0; i < 16; i++ )
//    {
//        aa[i] = a[15 - i];
//        bb[i] = b[15 - i];
//    }
//
//    asm( "movdqu (%0), %%xmm0               \n\t" // a1:a0
//         "movdqu (%1), %%xmm1               \n\t" // b1:b0
//
//         /*
//          * Caryless multiplication xmm2:xmm1 = xmm0 * xmm1
//          * using [CLMUL-WP] algorithm 1 (p. 13).
//          */
//         "movdqa %%xmm1, %%xmm2             \n\t" // copy of b1:b0
//         "movdqa %%xmm1, %%xmm3             \n\t" // same
//         "movdqa %%xmm1, %%xmm4             \n\t" // same
//         PCLMULQDQ xmm0_xmm1 ",0x00         \n\t" // a0*b0 = c1:c0
//         PCLMULQDQ xmm0_xmm2 ",0x11         \n\t" // a1*b1 = d1:d0
//         PCLMULQDQ xmm0_xmm3 ",0x10         \n\t" // a0*b1 = e1:e0
//         PCLMULQDQ xmm0_xmm4 ",0x01         \n\t" // a1*b0 = f1:f0
//         "pxor %%xmm3, %%xmm4               \n\t" // e1+f1:e0+f0
//         "movdqa %%xmm4, %%xmm3             \n\t" // same
//         "psrldq $8, %%xmm4                 \n\t" // 0:e1+f1
//         "pslldq $8, %%xmm3                 \n\t" // e0+f0:0
//         "pxor %%xmm4, %%xmm2               \n\t" // d1:d0+e1+f1
//         "pxor %%xmm3, %%xmm1               \n\t" // c1+e0+f1:c0
//
//         /*
//          * Now shift the result one bit to the left,
//          * taking advantage of [CLMUL-WP] eq 27 (p. 20)
//          */
//         "movdqa %%xmm1, %%xmm3             \n\t" // r1:r0
//         "movdqa %%xmm2, %%xmm4             \n\t" // r3:r2
//         "psllq $1, %%xmm1                  \n\t" // r1<<1:r0<<1
//         "psllq $1, %%xmm2                  \n\t" // r3<<1:r2<<1
//         "psrlq $63, %%xmm3                 \n\t" // r1>>63:r0>>63
//         "psrlq $63, %%xmm4                 \n\t" // r3>>63:r2>>63
//         "movdqa %%xmm3, %%xmm5             \n\t" // r1>>63:r0>>63
//         "pslldq $8, %%xmm3                 \n\t" // r0>>63:0
//         "pslldq $8, %%xmm4                 \n\t" // r2>>63:0
//         "psrldq $8, %%xmm5                 \n\t" // 0:r1>>63
//         "por %%xmm3, %%xmm1                \n\t" // r1<<1|r0>>63:r0<<1
//         "por %%xmm4, %%xmm2                \n\t" // r3<<1|r2>>62:r2<<1
//         "por %%xmm5, %%xmm2                \n\t" // r3<<1|r2>>62:r2<<1|r1>>63
//
//         /*
//          * Now reduce modulo the GCM polynomial x^128 + x^7 + x^2 + x + 1
//          * using [CLMUL-WP] algorithm 5 (p. 20).
//          * Currently xmm2:xmm1 holds x3:x2:x1:x0 (already shifted).
//          */
//         /* Step 2 (1) */
//         "movdqa %%xmm1, %%xmm3             \n\t" // x1:x0
//         "movdqa %%xmm1, %%xmm4             \n\t" // same
//         "movdqa %%xmm1, %%xmm5             \n\t" // same
//         "psllq $63, %%xmm3                 \n\t" // x1<<63:x0<<63 = stuff:a
//         "psllq $62, %%xmm4                 \n\t" // x1<<62:x0<<62 = stuff:b
//         "psllq $57, %%xmm5                 \n\t" // x1<<57:x0<<57 = stuff:c
//
//         /* Step 2 (2) */
//         "pxor %%xmm4, %%xmm3               \n\t" // stuff:a+b
//         "pxor %%xmm5, %%xmm3               \n\t" // stuff:a+b+c
//         "pslldq $8, %%xmm3                 \n\t" // a+b+c:0
//         "pxor %%xmm3, %%xmm1               \n\t" // x1+a+b+c:x0 = d:x0
//
//         /* Steps 3 and 4 */
//         "movdqa %%xmm1,%%xmm0              \n\t" // d:x0
//         "movdqa %%xmm1,%%xmm4              \n\t" // same
//         "movdqa %%xmm1,%%xmm5              \n\t" // same
//         "psrlq $1, %%xmm0                  \n\t" // e1:x0>>1 = e1:e0'
//         "psrlq $2, %%xmm4                  \n\t" // f1:x0>>2 = f1:f0'
//         "psrlq $7, %%xmm5                  \n\t" // g1:x0>>7 = g1:g0'
//         "pxor %%xmm4, %%xmm0               \n\t" // e1+f1:e0'+f0'
//         "pxor %%xmm5, %%xmm0               \n\t" // e1+f1+g1:e0'+f0'+g0'
//         // e0'+f0'+g0' is almost e0+f0+g0, ex\tcept for some missing
//         // bits carried from d. Now get those\t bits back in.
//         "movdqa %%xmm1,%%xmm3              \n\t" // d:x0
//         "movdqa %%xmm1,%%xmm4              \n\t" // same
//         "movdqa %%xmm1,%%xmm5              \n\t" // same
//         "psllq $63, %%xmm3                 \n\t" // d<<63:stuff
//         "psllq $62, %%xmm4                 \n\t" // d<<62:stuff
//         "psllq $57, %%xmm5                 \n\t" // d<<57:stuff
//         "pxor %%xmm4, %%xmm3               \n\t" // d<<63+d<<62:stuff
//         "pxor %%xmm5, %%xmm3               \n\t" // missing bits of d:stuff
//         "psrldq $8, %%xmm3                 \n\t" // 0:missing bits of d
//         "pxor %%xmm3, %%xmm0               \n\t" // e1+f1+g1:e0+f0+g0
//         "pxor %%xmm1, %%xmm0               \n\t" // h1:h0
//         "pxor %%xmm2, %%xmm0               \n\t" // x3+h1:x2+h0
//
//         "movdqu %%xmm0, (%2)               \n\t" // done
//         :
//         : "r" (aa), "r" (bb), "r" (cc)
//         : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5" );
//
//    /* Now byte-reverse the outputs */
//    for( i = 0; i < 16; i++ )
//        c[i] = cc[15 - i];

    return;
}

/*
 * Compute decryption round keys from encryption round keys
 */
ATTRIBUTE_TARGET_AES
void mbedtls_aesni_inverse_key( unsigned char *invkey,
                        const unsigned char *fwdkey, int nr )
{
    __m128i *ik = (__m128i *)invkey;
    const __m128i *fwdkey_start = (const __m128i *)fwdkey;
    const __m128i *fk = fwdkey_start + nr;

    _mm_storeu_si128(ik, _mm_loadu_si128(fk));

    for( fk--, ik++; fk > fwdkey_start; fk--, ik++ )
        _mm_storeu_si128(ik, _mm_aesimc_si128(_mm_loadu_si128(fk)));

    _mm_storeu_si128(ik, _mm_loadu_si128(fk));
}

static inline __m128i aeskeygenassist_finish_128( __m128i *rk,
                                                  __m128i key,
                                                  __m128i kga )
{
    __m128i tmp = _mm_shuffle_epi32(kga, _MM_SHUFFLE(3, 3, 3, 3));
    tmp = _mm_xor_si128(tmp, key);

    key = _mm_slli_si128(key, 4);
    tmp = _mm_xor_si128(tmp, key);
    key = _mm_slli_si128(key, 4);
    tmp = _mm_xor_si128(tmp, key);
    key = _mm_slli_si128(key, 4);
    tmp = _mm_xor_si128(tmp, key);

    _mm_storeu_si128(rk, tmp);
    return tmp;
}

#define aesni_keygen_128(rk, key, rcon) \
    aeskeygenassist_finish_128((rk), (key), _mm_aeskeygenassist_si128((key), (rcon)))

/*
 * Key expansion, 128-bit case
 */
ATTRIBUTE_TARGET_AES
static void aesni_setkey_enc_128( unsigned char *rk,
                                  const unsigned char *key )
{
    // copy the original key as round key 0
    __m128i k = _mm_loadu_si128((const __m128i *)key);
    __m128i *prk = (__m128i *)rk;
    _mm_storeu_si128(prk++, k);

    k = aesni_keygen_128(prk++, k, 0x01);
    k = aesni_keygen_128(prk++, k, 0x02);
    k = aesni_keygen_128(prk++, k, 0x04);
    k = aesni_keygen_128(prk++, k, 0x08);
    k = aesni_keygen_128(prk++, k, 0x10);
    k = aesni_keygen_128(prk++, k, 0x20);
    k = aesni_keygen_128(prk++, k, 0x40);
    k = aesni_keygen_128(prk++, k, 0x80);
    k = aesni_keygen_128(prk++, k, 0x1B);
    aesni_keygen_128(prk, k, 0x36);
}

static inline void write_roundkey_192( unsigned char **dst,
                                       __m128i rk_lo,
                                       __m128i rk_hi )
{
    __m128i *d = (__m128i *)*dst;
    _mm_storeu_si128(d++, rk_lo);
    _mm_storeu_si64(d, rk_hi);
    *dst += 192 / 8;
}

static inline void aeskeygenassist_finish_192( unsigned char **prk,
                                               __m128i *pkl,
                                               __m128i *pkh,
                                               __m128i kga )
{
    __m128i kl = *pkl;
    __m128i kh = *pkh;
    __m128i tmp = _mm_shuffle_epi32(kga, _MM_SHUFFLE(1, 1, 1, 1));
    tmp = _mm_xor_si128(tmp, kl);

    kl = _mm_slli_si128(kl, 4);
    tmp = _mm_xor_si128(tmp, kl);
    kl = _mm_slli_si128(kl, 4);
    tmp = _mm_xor_si128(tmp, kl);
    kl = _mm_slli_si128(kl, 4);
    kl = _mm_xor_si128(tmp, kl);

    tmp = _mm_shuffle_epi32(kl, _MM_SHUFFLE(3, 3, 3, 3));
    tmp = _mm_xor_si128(tmp, kh);

    kh = _mm_slli_si128(kh, 4);
    kh = _mm_xor_si128(tmp, kh);

    write_roundkey_192(prk, kl, kh);
    *pkl = kl;
    *pkh = kh;
}

#define aesni_keygen_192(prk, pkl, pkh, rcon) \
    aeskeygenassist_finish_192((prk), (pkl), (pkh), _mm_aeskeygenassist_si128(*(pkh), (rcon)))

/*
 * Key expansion, 192-bit case
 */
ATTRIBUTE_TARGET_AES
static void aesni_setkey_enc_192( unsigned char *rk,
                                  const unsigned char *key )
{
    const __m128i *pkey = (const __m128i *)key;
    __m128i kl = _mm_loadu_si128(pkey);
    __m128i kh = _mm_loadu_si64(pkey + 1);
    unsigned char **prk = &rk;
    write_roundkey_192(prk, kl, kh);

    __m128i *pkl = &kl;
    __m128i *pkh = &kh;
    aesni_keygen_192(prk, pkl, pkh, 0x01);
    aesni_keygen_192(prk, pkl, pkh, 0x02);
    aesni_keygen_192(prk, pkl, pkh, 0x04);
    aesni_keygen_192(prk, pkl, pkh, 0x08);
    aesni_keygen_192(prk, pkl, pkh, 0x10);
    aesni_keygen_192(prk, pkl, pkh, 0x20);
    aesni_keygen_192(prk, pkl, pkh, 0x40);
    aesni_keygen_192(prk, pkl, pkh, 0x80);
}

static inline void write_roundkey_256( unsigned char **dst,
                                       __m128i rk_lo,
                                       __m128i rk_hi )
{
    __m128i *d = (__m128i *)*dst;
    _mm_storeu_si128(d++, rk_lo);
    _mm_storeu_si128(d, rk_hi);
    *dst += 256 / 8;
}

ATTRIBUTE_TARGET_AES
static inline void aeskeygenassist_finish_256( unsigned char **prk,
                                               __m128i *pkl,
                                               __m128i *pkh,
                                               __m128i kga )
{
    __m128i kl = *pkl;
    __m128i kh = *pkh;
    __m128i tmp = _mm_shuffle_epi32(kga, _MM_SHUFFLE(3, 3, 3, 3));
    tmp = _mm_xor_si128(tmp, kl);

    kl = _mm_slli_si128(kl, 4);
    tmp = _mm_xor_si128(tmp, kl);
    kl = _mm_slli_si128(kl, 4);
    tmp = _mm_xor_si128(tmp, kl);
    kl = _mm_slli_si128(kl, 4);
    kl = _mm_xor_si128(tmp, kl);

    tmp = _mm_aeskeygenassist_si128(kl, 0x00);
    tmp = _mm_shuffle_epi32(tmp, _MM_SHUFFLE(2, 2, 2, 2));
    tmp = _mm_xor_si128(tmp, kh);

    kh = _mm_slli_si128(kh, 4);
    tmp = _mm_xor_si128(tmp, kh);
    kh = _mm_slli_si128(kh, 4);
    tmp = _mm_xor_si128(tmp, kh);
    kh = _mm_slli_si128(kh, 4);
    kh = _mm_xor_si128(tmp, kh);

    write_roundkey_256(prk, kl, kh);
    *pkl = kl;
    *pkh = kh;
}

#define aesni_keygen_256(prk, pkl, pkh, rcon) \
    aeskeygenassist_finish_256((prk), (pkl), (pkh), _mm_aeskeygenassist_si128(*(pkh), (rcon)))

/*
 * Key expansion, 256-bit case
 */
ATTRIBUTE_TARGET_AES
static void aesni_setkey_enc_256( unsigned char *rk,
                                  const unsigned char *key )
{
    const __m128i *pkey = (const __m128i *)key;
    __m128i kl = _mm_loadu_si128(pkey);
    __m128i kh = _mm_loadu_si128(pkey + 1);
    unsigned char **prk = &rk;
    write_roundkey_256(prk, kl, kh);

    /*
     * Generates one more key than necessary,
     * see definition of mbedtls_aes_context.buf
     */
    __m128i *pkl = &kl;
    __m128i *pkh = &kh;
    aesni_keygen_256(prk, pkl, pkh, 0x01);
    aesni_keygen_256(prk, pkl, pkh, 0x02);
    aesni_keygen_256(prk, pkl, pkh, 0x04);
    aesni_keygen_256(prk, pkl, pkh, 0x08);
    aesni_keygen_256(prk, pkl, pkh, 0x10);
    aesni_keygen_256(prk, pkl, pkh, 0x20);
    aesni_keygen_256(prk, pkl, pkh, 0x40);
}

/*
 * Key expansion, wrapper
 */
int mbedtls_aesni_setkey_enc( unsigned char *rk,
                      const unsigned char *key,
                      size_t bits )
{
    switch( bits )
    {
        case 128: aesni_setkey_enc_128( rk, key ); break;
        case 192: aesni_setkey_enc_192( rk, key ); break;
        case 256: aesni_setkey_enc_256( rk, key ); break;
        default : return( MBEDTLS_ERR_AES_INVALID_KEY_LENGTH );
    }

    return( 0 );
}

#endif /* MBEDTLS_HAVE_X86_64 */

#endif /* MBEDTLS_AESNI_C */
