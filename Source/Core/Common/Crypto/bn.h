// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>

#include "Common/CommonTypes.h"

// bignum arithmetic

int bn_compare(const u8* a, const u8* b, size_t n);
void bn_sub_modulus(u8* a, const u8* N, size_t n);
void bn_add(u8* d, const u8* a, const u8* b, const u8* N, size_t n);
void bn_mul(u8* d, const u8* a, const u8* b, const u8* N, size_t n);
void bn_inv(u8* d, const u8* a, const u8* N, size_t n);  // only for prime N
void bn_exp(u8* d, const u8* a, const u8* N, size_t n, const u8* e, size_t en);
