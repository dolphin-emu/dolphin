// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Crypto/SHA1.h"

static constexpr std::string_view ACHIEVEMENT_APPROVED_LIST_FILENAME = "ApprovedInis.json";
// After building tests, find the new hash with:
// ./Binaries/Tests/tests --gtest_filter=PatchAllowlist.VerifyHashes
static const inline Common::SHA1::Digest ACHIEVEMENT_APPROVED_LIST_HASH = {
    0x6E, 0x38, 0x6C, 0x15, 0x5F, 0x6B, 0x83, 0x72, 0xB0, 0x35,
    0xDA, 0xA8, 0x70, 0x43, 0xCB, 0x4D, 0xE0, 0xD7, 0x9C, 0x98};
