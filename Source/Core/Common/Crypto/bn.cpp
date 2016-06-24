// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include <cstdio>
#include <string.h>

#include "Common/CommonTypes.h"
#include "Common/Crypto/bn.h"

static void bn_zero(u8* d, u32 n)
{
  memset(d, 0, n);
}

static void bn_copy(u8* d, const u8* a, u32 n)
{
  memcpy(d, a, n);
}

int bn_compare(const u8* a, const u8* b, u32 n)
{
  u32 i;

  for (i = 0; i < n; i++)
  {
    if (a[i] < b[i])
      return -1;
    if (a[i] > b[i])
      return 1;
  }

  return 0;
}

void bn_sub_modulus(u8* a, const u8* N, u32 n)
{
  u32 i;
  u32 dig;
  u8 c;

  c = 0;
  for (i = n - 1; i < n; i--)
  {
    dig = N[i] + c;
    c = (a[i] < dig);
    a[i] -= dig;
  }
}

void bn_add(u8* d, const u8* a, const u8* b, const u8* N, u32 n)
{
  u32 i;
  u32 dig;
  u8 c;

  c = 0;
  for (i = n - 1; i < n; i--)
  {
    dig = a[i] + b[i] + c;
    c = (dig >= 0x100);
    d[i] = dig;
  }

  if (c)
    bn_sub_modulus(d, N, n);

  if (bn_compare(d, N, n) >= 0)
    bn_sub_modulus(d, N, n);
}

void bn_mul(u8* d, const u8* a, const u8* b, const u8* N, u32 n)
{
  u32 i;
  u8 mask;

  bn_zero(d, n);

  for (i = 0; i < n; i++)
    for (mask = 0x80; mask != 0; mask >>= 1)
    {
      bn_add(d, d, d, N, n);
      if ((a[i] & mask) != 0)
        bn_add(d, d, b, N, n);
    }
}

void bn_exp(u8* d, const u8* a, const u8* N, u32 n, const u8* e, u32 en)
{
  u8 t[512];
  u32 i;
  u8 mask;

  bn_zero(d, n);
  d[n - 1] = 1;
  for (i = 0; i < en; i++)
    for (mask = 0x80; mask != 0; mask >>= 1)
    {
      bn_mul(t, d, d, N, n);
      if ((e[i] & mask) != 0)
        bn_mul(d, t, a, N, n);
      else
        bn_copy(d, t, n);
    }
}

// only for prime N -- stupid but lazy, see if I care
void bn_inv(u8* d, const u8* a, const u8* N, u32 n)
{
  u8 t[512], s[512];

  bn_copy(t, N, n);
  bn_zero(s, n);
  s[n - 1] = 2;
  bn_sub_modulus(t, s, n);
  bn_exp(d, a, N, n, t, n);
}
