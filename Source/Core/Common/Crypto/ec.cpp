// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string.h>

#include "Common/Common.h"
#include "Common/Crypto/bn.h"
#include "Common/Crypto/ec.h"

// y**2 + x*y = x**3 + x + b
UNUSED static const u8 ec_b[30] =
	{0x00,0x66,0x64,0x7e,0xde,0x6c,0x33,0x2c,0x7f,0x8c,0x09,0x23,0xbb,0x58,0x21
	,0x3b,0x33,0x3b,0x20,0xe9,0xce,0x42,0x81,0xfe,0x11,0x5f,0x7d,0x8f,0x90,0xad};

// order of the addition group of points
static const u8 ec_N[30] =
	{0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	,0x13,0xe9,0x74,0xe7,0x2f,0x8a,0x69,0x22,0x03,0x1d,0x26,0x03,0xcf,0xe0,0xd7};

// base point
static const u8 ec_G[60] =
	{0x00,0xfa,0xc9,0xdf,0xcb,0xac,0x83,0x13,0xbb,0x21,0x39,0xf1,0xbb,0x75,0x5f
	,0xef,0x65,0xbc,0x39,0x1f,0x8b,0x36,0xf8,0xf8,0xeb,0x73,0x71,0xfd,0x55,0x8b
	,0x01,0x00,0x6a,0x08,0xa4,0x19,0x03,0x35,0x06,0x78,0xe5,0x85,0x28,0xbe,0xbf
	,0x8a,0x0b,0xef,0xf8,0x67,0xa7,0xca,0x36,0x71,0x6f,0x7e,0x01,0xf8,0x10,0x52};

static void elt_copy(u8 *d, const u8 *a)
{
	memcpy(d, a, 30);
}

static void elt_zero(u8 *d)
{
	memset(d, 0, 30);
}

static int elt_is_zero(const u8 *d)
{
	u32 i;

	for (i = 0; i < 30; i++)
		if (d[i] != 0)
			return 0;

	return 1;
}

static void elt_add(u8 *d, const u8 *a, const u8 *b)
{
	u32 i;

	for (i = 0; i < 30; i++)
		d[i] = a[i] ^ b[i];
}

static void elt_mul_x(u8 *d, const u8 *a)
{
	u8 carry, x, y;
	u32 i;

	carry = a[0] & 1;

	x = 0;
	for (i = 0; i < 29; i++) {
		y = a[i + 1];
		d[i] = x ^ (y >> 7);
		x = y << 1;
	}
	d[29] = x ^ carry;

	d[20] ^= carry << 2;
}

static void elt_mul(u8 *d, const u8 *a, const u8 *b)
{
	u32 i, n;
	u8 mask;

	elt_zero(d);

	i = 0;
	mask = 1;
	for (n = 0; n < 233; n++) {
		elt_mul_x(d, d);

		if ((a[i] & mask) != 0)
			elt_add(d, d, b);

		mask >>= 1;
		if (mask == 0) {
			mask = 0x80;
			i++;
		}
	}
}

static const u8 square[16] =
	{0x00,0x01,0x04,0x05,0x10,0x11,0x14,0x15,0x40,0x41,0x44,0x45,0x50,0x51,0x54,0x55};

static void elt_square_to_wide(u8 *d, const u8 *a)
{
	u32 i;

	for (i = 0; i < 30; i++) {
		d[2*i] = square[a[i] >> 4];
		d[2*i + 1] = square[a[i] & 15];
	}
}

static void wide_reduce(u8 *d)
{
	u32 i;
	u8 x;

	for (i = 0; i < 30; i++) {
		x = d[i];

		d[i + 19] ^= x >> 7;
		d[i + 20] ^= x << 1;

		d[i + 29] ^= x >> 1;
		d[i + 30] ^= x << 7;
	}

	x = d[30] & ~1;

	d[49] ^= x >> 7;
	d[50] ^= x << 1;

	d[59] ^= x >> 1;

	d[30] &= 1;
}

static void elt_square(u8 *d, const u8 *a)
{
	u8 wide[60];

	elt_square_to_wide(wide, a);
	wide_reduce(wide);

	elt_copy(d, wide + 30);
}

static void itoh_tsujii(u8 *d, const u8 *a, const u8 *b, u32 j)
{
	u8 t[30];

	elt_copy(t, a);
	while (j--) {
		elt_square(d, t);
		elt_copy(t, d);
	}

	elt_mul(d, t, b);
}

static void elt_inv(u8 *d, const u8 *a)
{
	u8 t[30];
	u8 s[30];

	itoh_tsujii(t, a, a, 1);
	itoh_tsujii(s, t, a, 1);
	itoh_tsujii(t, s, s, 3);
	itoh_tsujii(s, t, a, 1);
	itoh_tsujii(t, s, s, 7);
	itoh_tsujii(s, t, t, 14);
	itoh_tsujii(t, s, a, 1);
	itoh_tsujii(s, t, t, 29);
	itoh_tsujii(t, s, s, 58);
	itoh_tsujii(s, t, t, 116);
	elt_square(d, s);
}

UNUSED static int point_is_on_curve(u8 *p)
{
	u8 s[30], t[30];
	u8 *x, *y;

	x = p;
	y = p + 30;

	elt_square(t, x);
	elt_mul(s, t, x);

	elt_add(s, s, t);

	elt_square(t, y);
	elt_add(s, s, t);

	elt_mul(t, x, y);
	elt_add(s, s, t);

	elt_add(s, s, ec_b);

	return elt_is_zero(s);
}

static int point_is_zero(const u8 *p)
{
	return elt_is_zero(p) && elt_is_zero(p + 30);
}

static void point_double(u8 *r, const u8 *p)
{
	u8 s[30], t[30];
	const u8 *px, *py;
	u8 *rx, *ry;

	px = p;
	py = p + 30;
	rx = r;
	ry = r + 30;

	if (elt_is_zero(px)) {
		elt_zero(rx);
		elt_zero(ry);

		return;
	}

	elt_inv(t, px);
	elt_mul(s, py, t);
	elt_add(s, s, px);

	elt_square(t, px);

	elt_square(rx, s);
	elt_add(rx, rx, s);
	rx[29] ^= 1;

	elt_mul(ry, s, rx);
	elt_add(ry, ry, rx);
	elt_add(ry, ry, t);
}

static void point_add(u8 *r, const u8 *p, const u8 *q)
{
	u8 s[30], t[30], u[30];
	const u8 *px, *py, *qx, *qy;
	u8 *rx, *ry;

	px = p;
	py = p + 30;
	qx = q;
	qy = q + 30;
	rx = r;
	ry = r + 30;

	if (point_is_zero(p)) {
		elt_copy(rx, qx);
		elt_copy(ry, qy);
		return;
	}

	if (point_is_zero(q)) {
		elt_copy(rx, px);
		elt_copy(ry, py);
		return;
	}

	elt_add(u, px, qx);

	if (elt_is_zero(u)) {
		elt_add(u, py, qy);
		if (elt_is_zero(u))
			point_double(r, p);
		else {
			elt_zero(rx);
			elt_zero(ry);
		}

		return;
	}

	elt_inv(t, u);
	elt_add(u, py, qy);
	elt_mul(s, t, u);

	elt_square(t, s);
	elt_add(t, t, s);
	elt_add(t, t, qx);
	t[29] ^= 1;

	elt_mul(u, s, t);
	elt_add(s, u, py);
	elt_add(rx, t, px);
	elt_add(ry, s, rx);
}

static void point_mul(u8 *d, const u8 *a, const u8 *b) // a is bignum
{
	u32 i;
	u8 mask;

	elt_zero(d);
	elt_zero(d + 30);

	for (i = 0; i < 30; i++)
		for (mask = 0x80; mask != 0; mask >>= 1) {
			point_double(d, d);
			if ((a[i] & mask) != 0)
				point_add(d, d, b);
		}
}

static void silly_random(u8 * rndArea, u8 count)
{
	u16 i;
	srand((unsigned) (time(nullptr)));

	for (i=0;i<count;i++)
	{
		rndArea[i]=rand();
	}
}

void generate_ecdsa(u8 *R, u8 *S, const u8 *k, const u8 *hash)
{
	u8 e[30];
	u8 kk[30];
	u8 m[30];
	u8 minv[30];
	u8 mG[60];
	//FILE *fp;

	elt_zero(e);
	memcpy(e + 10, hash, 20);

	//Changing random number generator to a lame one...
	silly_random(m, sizeof(m));
	//fp = fopen("/dev/random", "rb");
	//if (fread(m, sizeof m, 1, fp) != 1)
	//	fatal("reading random");
	//fclose(fp);
	m[0] = 0;

	//	R = (mG).x

	point_mul(mG, m, ec_G);
	elt_copy(R, mG);
	if (bn_compare(R, ec_N, 30) >= 0)
		bn_sub_modulus(R, ec_N, 30);

	//	S = m**-1*(e + Rk) (mod N)

	elt_copy(kk, k);
	if (bn_compare(kk, ec_N, 30) >= 0)
		bn_sub_modulus(kk, ec_N, 30);
	bn_mul(S, R, kk, ec_N, 30);
	bn_add(kk, S, e, ec_N, 30);
	bn_inv(minv, m, ec_N, 30);
	bn_mul(S, minv, kk, ec_N, 30);
}

UNUSED static int check_ecdsa(u8 *Q, u8 *R, u8 *S, const u8 *hash)
{
	u8 Sinv[30];
	u8 e[30];
	u8 w1[30], w2[30];
	u8 r1[60], r2[60];

	bn_inv(Sinv, S, ec_N, 30);

	elt_zero(e);
	memcpy(e + 10, hash, 20);

	bn_mul(w1, e, Sinv, ec_N, 30);
	bn_mul(w2, R, Sinv, ec_N, 30);

	point_mul(r1, w1, ec_G);
	point_mul(r2, w2, Q);

	point_add(r1, r1, r2);

	if (bn_compare(r1, ec_N, 30) >= 0)
		bn_sub_modulus(r1, ec_N, 30);

	return (bn_compare(r1, R, 30) == 0);
}

void ec_priv_to_pub(const u8 *k, u8 *Q)
{
	point_mul(Q, k, ec_G);
}
