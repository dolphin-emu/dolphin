// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Crypto/SHA1.h"

// After building tests, find the new hash with:
// ./Binaries/Tests/tests --gtest_filter=PatchAllowlist.VerifyHashes
static const inline Common::SHA1::Digest APPROVED_LIST_HASH = {
    0x7D, 0x96, 0x3E, 0x00, 0x30, 0x1C, 0x7D, 0x0E, 0x72, 0x09,
    0xE1, 0xF0, 0xF3, 0x51, 0x6D, 0x5C, 0xB3, 0x68, 0xAD, 0x79};
