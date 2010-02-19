// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#ifndef _TOOLS_H
#define _TOOLS_H
#include "sha1.h"

// bignum
int bn_compare(u8 *a, u8 *b, u32 n);
void bn_sub_modulus(u8 *a, u8 *N, u32 n);
void bn_add(u8 *d, u8 *a, u8 *b, u8 *N, u32 n);
void bn_mul(u8 *d, u8 *a, u8 *b, u8 *N, u32 n);
void bn_inv(u8 *d, u8 *a, u8 *N, u32 n);	// only for prime N
void bn_exp(u8 *d, u8 *a, u8 *N, u32 n, u8 *e, u32 en);

void generate_ecdsa(u8 *R, u8 *S, u8 *k, u8 *hash);

void ec_priv_to_pub(u8 *k, u8 *Q);

#endif
