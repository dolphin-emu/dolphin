/*
 *  AES-NI support functions
 *
 *  Copyright (C) 2013, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * [AES-WP] http://software.intel.com/en-us/articles/intel-advanced-encryption-standard-aes-instructions-set
 * [CLMUL-WP] http://software.intel.com/en-us/articles/intel-carry-less-multiplication-instruction-and-its-usage-for-computing-the-gcm-mode/
 */

#include "polarssl/config.h"

#if defined(POLARSSL_AESNI_C)

#include "polarssl/aesni.h"
#include <stdio.h>

#if defined(POLARSSL_HAVE_X86_64)

/*
 * AES-NI support detection routine
 */
int aesni_supports( unsigned int what )
{
    static int done = 0;
    static unsigned int c = 0;

    if( ! done )
    {
        asm( "movl  $1, %%eax   \n"
             "cpuid             \n"
             : "=c" (c)
             :
             : "eax", "ebx", "edx" );
        done = 1;
    }

    return( ( c & what ) != 0 );
}

/*
 * AES-NI AES-ECB block en(de)cryption
 */
int aesni_crypt_ecb( aes_context *ctx,
                     int mode,
                     const unsigned char input[16],
                     unsigned char output[16] )
{
    asm( "movdqu    (%3), %%xmm0    \n" // load input
         "movdqu    (%1), %%xmm1    \n" // load round key 0
         "pxor      %%xmm1, %%xmm0  \n" // round 0
         "addq      $16, %1         \n" // point to next round key
         "subl      $1, %0          \n" // normal rounds = nr - 1
         "test      %2, %2          \n" // mode?
         "jz        2f              \n" // 0 = decrypt

         "1:                        \n" // encryption loop
         "movdqu    (%1), %%xmm1    \n" // load round key
         "aesenc    %%xmm1, %%xmm0  \n" // do round
         "addq      $16, %1         \n" // point to next round key
         "subl      $1, %0          \n" // loop
         "jnz       1b              \n"
         "movdqu    (%1), %%xmm1    \n" // load round key
         "aesenclast %%xmm1, %%xmm0 \n" // last round
         "jmp       3f              \n"

         "2:                        \n" // decryption loop
         "movdqu    (%1), %%xmm1    \n"
         "aesdec    %%xmm1, %%xmm0  \n"
         "addq      $16, %1         \n"
         "subl      $1, %0          \n"
         "jnz       2b              \n"
         "movdqu    (%1), %%xmm1    \n" // load round key
         "aesdeclast %%xmm1, %%xmm0 \n" // last round

         "3:                        \n"
         "movdqu    %%xmm0, (%4)    \n" // export output
         :
         : "r" (ctx->nr), "r" (ctx->rk), "r" (mode), "r" (input), "r" (output)
         : "memory", "cc", "xmm0", "xmm1" );


    return( 0 );
}

/*
 * GCM multiplication: c = a times b in GF(2^128)
 * Based on [CLMUL-WP] algorithms 1 (with equation 27) and 5.
 */
void aesni_gcm_mult( unsigned char c[16],
                     const unsigned char a[16],
                     const unsigned char b[16] )
{
    unsigned char aa[16], bb[16], cc[16];
    size_t i;

    /* The inputs are in big-endian order, so byte-reverse them */
    for( i = 0; i < 16; i++ )
    {
        aa[i] = a[15 - i];
        bb[i] = b[15 - i];
    }

    asm( "movdqu (%0), %%xmm0               \n" // a1:a0
         "movdqu (%1), %%xmm1               \n" // b1:b0

         /*
          * Caryless multiplication xmm2:xmm1 = xmm0 * xmm1
          * using [CLMUL-WP] algorithm 1 (p. 13).
          */
         "movdqa %%xmm1, %%xmm2             \n" // copy of b1:b0
         "movdqa %%xmm1, %%xmm3             \n" // same
         "movdqa %%xmm1, %%xmm4             \n" // same
         "pclmulqdq $0x00, %%xmm0, %%xmm1   \n" // a0*b0 = c1:c0
         "pclmulqdq $0x11, %%xmm0, %%xmm2   \n" // a1*b1 = d1:d0
         "pclmulqdq $0x10, %%xmm0, %%xmm3   \n" // a0*b1 = e1:e0
         "pclmulqdq $0x01, %%xmm0, %%xmm4   \n" // a1*b0 = f1:f0
         "pxor %%xmm3, %%xmm4               \n" // e1+f1:e0+f0
         "movdqa %%xmm4, %%xmm3             \n" // same
         "psrldq $8, %%xmm4                 \n" // 0:e1+f1
         "pslldq $8, %%xmm3                 \n" // e0+f0:0
         "pxor %%xmm4, %%xmm2               \n" // d1:d0+e1+f1
         "pxor %%xmm3, %%xmm1               \n" // c1+e0+f1:c0

         /*
          * Now shift the result one bit to the left,
          * taking advantage of [CLMUL-WP] eq 27 (p. 20)
          */
         "movdqa %%xmm1, %%xmm3             \n" // r1:r0
         "movdqa %%xmm2, %%xmm4             \n" // r3:r2
         "psllq $1, %%xmm1                  \n" // r1<<1:r0<<1
         "psllq $1, %%xmm2                  \n" // r3<<1:r2<<1
         "psrlq $63, %%xmm3                 \n" // r1>>63:r0>>63
         "psrlq $63, %%xmm4                 \n" // r3>>63:r2>>63
         "movdqa %%xmm3, %%xmm5             \n" // r1>>63:r0>>63
         "pslldq $8, %%xmm3                 \n" // r0>>63:0
         "pslldq $8, %%xmm4                 \n" // r2>>63:0
         "psrldq $8, %%xmm5                 \n" // 0:r1>>63
         "por %%xmm3, %%xmm1                \n" // r1<<1|r0>>63:r0<<1
         "por %%xmm4, %%xmm2                \n" // r3<<1|r2>>62:r2<<1
         "por %%xmm5, %%xmm2                \n" // r3<<1|r2>>62:r2<<1|r1>>63

         /*
          * Now reduce modulo the GCM polynomial x^128 + x^7 + x^2 + x + 1
          * using [CLMUL-WP] algorithm 5 (p. 20).
          * Currently xmm2:xmm1 holds x3:x2:x1:x0 (already shifted).
          */
         /* Step 2 (1) */
         "movdqa %%xmm1, %%xmm3             \n" // x1:x0
         "movdqa %%xmm1, %%xmm4             \n" // same
         "movdqa %%xmm1, %%xmm5             \n" // same
         "psllq $63, %%xmm3                 \n" // x1<<63:x0<<63 = stuff:a
         "psllq $62, %%xmm4                 \n" // x1<<62:x0<<62 = stuff:b
         "psllq $57, %%xmm5                 \n" // x1<<57:x0<<57 = stuff:c

         /* Step 2 (2) */
         "pxor %%xmm4, %%xmm3               \n" // stuff:a+b
         "pxor %%xmm5, %%xmm3               \n" // stuff:a+b+c
         "pslldq $8, %%xmm3                 \n" // a+b+c:0
         "pxor %%xmm3, %%xmm1               \n" // x1+a+b+c:x0 = d:x0

         /* Steps 3 and 4 */
         "movdqa %%xmm1,%%xmm0              \n" // d:x0
         "movdqa %%xmm1,%%xmm4              \n" // same
         "movdqa %%xmm1,%%xmm5              \n" // same
         "psrlq $1, %%xmm0                  \n" // e1:x0>>1 = e1:e0'
         "psrlq $2, %%xmm4                  \n" // f1:x0>>2 = f1:f0'
         "psrlq $7, %%xmm5                  \n" // g1:x0>>7 = g1:g0'
         "pxor %%xmm4, %%xmm0               \n" // e1+f1:e0'+f0'
         "pxor %%xmm5, %%xmm0               \n" // e1+f1+g1:e0'+f0'+g0'
         // e0'+f0'+g0' is almost e0+f0+g0, except for some missing
         // bits carried from d. Now get those bits back in.
         "movdqa %%xmm1,%%xmm3              \n" // d:x0
         "movdqa %%xmm1,%%xmm4              \n" // same
         "movdqa %%xmm1,%%xmm5              \n" // same
         "psllq $63, %%xmm3                 \n" // d<<63:stuff
         "psllq $62, %%xmm4                 \n" // d<<62:stuff
         "psllq $57, %%xmm5                 \n" // d<<57:stuff
         "pxor %%xmm4, %%xmm3               \n" // d<<63+d<<62:stuff
         "pxor %%xmm5, %%xmm3               \n" // missing bits of d:stuff
         "psrldq $8, %%xmm3                 \n" // 0:missing bits of d
         "pxor %%xmm3, %%xmm0               \n" // e1+f1+g1:e0+f0+g0
         "pxor %%xmm1, %%xmm0               \n" // h1:h0
         "pxor %%xmm2, %%xmm0               \n" // x3+h1:x2+h0

         "movdqu %%xmm0, (%2)               \n" // done
         :
         : "r" (aa), "r" (bb), "r" (cc)
         : "memory", "cc", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5" );

    /* Now byte-reverse the outputs */
    for( i = 0; i < 16; i++ )
        c[i] = cc[15 - i];

    return;
}

/*
 * Compute decryption round keys from encryption round keys
 */
void aesni_inverse_key( unsigned char *invkey,
                        const unsigned char *fwdkey, int nr )
{
    unsigned char *ik = invkey;
    const unsigned char *fk = fwdkey + 16 * nr;

    memcpy( ik, fk, 16 );

    for( fk -= 16, ik += 16; fk > fwdkey; fk -= 16, ik += 16 )
        asm( "movdqu (%0), %%xmm0       \n"
             "aesimc %%xmm0, %%xmm0     \n"
             "movdqu %%xmm0, (%1)       \n"
             :
             : "r" (fk), "r" (ik)
             : "memory", "xmm0" );

    memcpy( ik, fk, 16 );
}

/*
 * Key expansion, 128-bit case
 */
static void aesni_setkey_enc_128( unsigned char *rk,
                                  const unsigned char *key )
{
    asm( "movdqu (%1), %%xmm0               \n" // copy the original key
         "movdqu %%xmm0, (%0)               \n" // as round key 0
         "jmp 2f                            \n" // skip auxiliary routine

         /*
          * Finish generating the next round key.
          *
          * On entry xmm0 is r3:r2:r1:r0 and xmm1 is X:stuff:stuff:stuff
          * with X = rot( sub( r3 ) ) ^ RCON.
          *
          * On exit, xmm0 is r7:r6:r5:r4
          * with r4 = X + r0, r5 = r4 + r1, r6 = r5 + r2, r7 = r6 + r3
          * and those are written to the round key buffer.
          */
         "1:                                \n"
         "pshufd $0xff, %%xmm1, %%xmm1      \n" // X:X:X:X
         "pxor %%xmm0, %%xmm1               \n" // X+r3:X+r2:X+r1:r4
         "pslldq $4, %%xmm0                 \n" // r2:r1:r0:0
         "pxor %%xmm0, %%xmm1               \n" // X+r3+r2:X+r2+r1:r5:r4
         "pslldq $4, %%xmm0                 \n" // etc
         "pxor %%xmm0, %%xmm1               \n"
         "pslldq $4, %%xmm0                 \n"
         "pxor %%xmm1, %%xmm0               \n" // update xmm0 for next time!
         "add $16, %0                       \n" // point to next round key
         "movdqu %%xmm0, (%0)               \n" // write it
         "ret                               \n"

         /* Main "loop" */
         "2:                                    \n"
         "aeskeygenassist $0x01, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x02, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x04, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x08, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x10, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x20, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x40, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x80, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x1B, %%xmm0, %%xmm1 \ncall 1b   \n"
         "aeskeygenassist $0x36, %%xmm0, %%xmm1 \ncall 1b   \n"
         :
         : "r" (rk), "r" (key)
         : "memory", "cc", "0" );
}

/*
 * Key expansion, 192-bit case
 */
static void aesni_setkey_enc_192( unsigned char *rk,
                                  const unsigned char *key )
{
    asm( "movdqu (%1), %%xmm0   \n" // copy original round key
         "movdqu %%xmm0, (%0)   \n"
         "add $16, %0           \n"
         "movq 16(%1), %%xmm1   \n"
         "movq %%xmm1, (%0)     \n"
         "add $8, %0            \n"
         "jmp 2f                \n" // skip auxiliary routine

         /*
          * Finish generating the next 6 quarter-keys.
          *
          * On entry xmm0 is r3:r2:r1:r0, xmm1 is stuff:stuff:r5:r4
          * and xmm2 is stuff:stuff:X:stuff with X = rot( sub( r3 ) ) ^ RCON.
          *
          * On exit, xmm0 is r9:r8:r7:r6 and xmm1 is stuff:stuff:r11:r10
          * and those are written to the round key buffer.
          */
         "1:                            \n"
         "pshufd $0x55, %%xmm2, %%xmm2  \n" // X:X:X:X
         "pxor %%xmm0, %%xmm2           \n" // X+r3:X+r2:X+r1:r4
         "pslldq $4, %%xmm0             \n" // etc
         "pxor %%xmm0, %%xmm2           \n"
         "pslldq $4, %%xmm0             \n"
         "pxor %%xmm0, %%xmm2           \n"
         "pslldq $4, %%xmm0             \n"
         "pxor %%xmm2, %%xmm0           \n" // update xmm0 = r9:r8:r7:r6
         "movdqu %%xmm0, (%0)           \n"
         "add $16, %0                   \n"
         "pshufd $0xff, %%xmm0, %%xmm2  \n" // r9:r9:r9:r9
         "pxor %%xmm1, %%xmm2           \n" // stuff:stuff:r9+r5:r10
         "pslldq $4, %%xmm1             \n" // r2:r1:r0:0
         "pxor %%xmm2, %%xmm1           \n" // update xmm1 = stuff:stuff:r11:r10
         "movq %%xmm1, (%0)             \n"
         "add $8, %0                    \n"
         "ret                           \n"

         "2:                                    \n"
         "aeskeygenassist $0x01, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x02, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x04, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x08, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x10, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x20, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x40, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x80, %%xmm1, %%xmm2 \ncall 1b   \n"

         :
         : "r" (rk), "r" (key)
         : "memory", "cc", "0" );
}

/*
 * Key expansion, 256-bit case
 */
static void aesni_setkey_enc_256( unsigned char *rk,
                                  const unsigned char *key )
{
    asm( "movdqu (%1), %%xmm0           \n"
         "movdqu %%xmm0, (%0)           \n"
         "add $16, %0                   \n"
         "movdqu 16(%1), %%xmm1         \n"
         "movdqu %%xmm1, (%0)           \n"
         "jmp 2f                        \n" // skip auxiliary routine

         /*
          * Finish generating the next two round keys.
          *
          * On entry xmm0 is r3:r2:r1:r0, xmm1 is r7:r6:r5:r4 and
          * xmm2 is X:stuff:stuff:stuff with X = rot( sub( r7 )) ^ RCON
          *
          * On exit, xmm0 is r11:r10:r9:r8 and xmm1 is r15:r14:r13:r12
          * and those have been written to the output buffer.
          */
         "1:                                \n"
         "pshufd $0xff, %%xmm2, %%xmm2      \n"
         "pxor %%xmm0, %%xmm2               \n"
         "pslldq $4, %%xmm0                 \n"
         "pxor %%xmm0, %%xmm2               \n"
         "pslldq $4, %%xmm0                 \n"
         "pxor %%xmm0, %%xmm2               \n"
         "pslldq $4, %%xmm0                 \n"
         "pxor %%xmm2, %%xmm0               \n"
         "add $16, %0                       \n"
         "movdqu %%xmm0, (%0)               \n"

         /* Set xmm2 to stuff:Y:stuff:stuff with Y = subword( r11 )
          * and proceed to generate next round key from there */
         "aeskeygenassist $0, %%xmm0, %%xmm2\n"
         "pshufd $0xaa, %%xmm2, %%xmm2      \n"
         "pxor %%xmm1, %%xmm2               \n"
         "pslldq $4, %%xmm1                 \n"
         "pxor %%xmm1, %%xmm2               \n"
         "pslldq $4, %%xmm1                 \n"
         "pxor %%xmm1, %%xmm2               \n"
         "pslldq $4, %%xmm1                 \n"
         "pxor %%xmm2, %%xmm1               \n"
         "add $16, %0                       \n"
         "movdqu %%xmm1, (%0)               \n"
         "ret                               \n"

         /*
          * Main "loop" - Generating one more key than necessary,
          * see definition of aes_context.buf
          */
         "2:                                    \n"
         "aeskeygenassist $0x01, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x02, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x04, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x08, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x10, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x20, %%xmm1, %%xmm2 \ncall 1b   \n"
         "aeskeygenassist $0x40, %%xmm1, %%xmm2 \ncall 1b   \n"
         :
         : "r" (rk), "r" (key)
         : "memory", "cc", "0" );
}

/*
 * Key expansion, wrapper
 */
int aesni_setkey_enc( unsigned char *rk,
                      const unsigned char *key,
                      size_t bits )
{
    switch( bits )
    {
        case 128: aesni_setkey_enc_128( rk, key ); break;
        case 192: aesni_setkey_enc_192( rk, key ); break;
        case 256: aesni_setkey_enc_256( rk, key ); break;
        default : return( POLARSSL_ERR_AES_INVALID_KEY_LENGTH );
    }

    return( 0 );
}

#endif /* POLARSSL_HAVE_X86_64 */

#endif /* POLARSSL_AESNI_C */
