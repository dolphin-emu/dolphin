// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

// bignum arithmetic

int bn_compare(const u8* a, const u8* b, int n);
void bn_sub_modulus(u8* a, const u8* N, int n);
void bn_add(u8* d, const u8* a, const u8* b, const u8* N, int n);
void bn_mul(u8* d, const u8* a, const u8* b, const u8* N, int n);
void bn_inv(u8* d, const u8* a, const u8* N, int n);  // only for prime N
void bn_exp(u8* d, const u8* a, const u8* N, int n, const u8* e, int en);
