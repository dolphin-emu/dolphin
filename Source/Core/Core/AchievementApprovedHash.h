// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Crypto/SHA1.h"

static constexpr std::string_view ACHIEVEMENT_APPROVED_LIST_FILENAME = "ApprovedInis.json";
// After building tests, find the new hash with:
// ./Binaries/Tests/tests --gtest_filter=PatchAllowlist.VerifyHashes
static const inline Common::SHA1::Digest ACHIEVEMENT_APPROVED_LIST_HASH = {
    0x0B, 0x47, 0x3F, 0x5A, 0x84, 0xC0, 0xC1, 0x6D, 0x08, 0x88,
    0x05, 0x5C, 0xC6, 0x17, 0x60, 0xED, 0x02, 0x3C, 0x75, 0xF0};
