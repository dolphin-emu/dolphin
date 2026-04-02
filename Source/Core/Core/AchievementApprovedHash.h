// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Crypto/SHA1.h"

static constexpr std::string_view ACHIEVEMENT_APPROVED_LIST_FILENAME = "ApprovedInis.json";
// After building tests, find the new hash with:
// ./Binaries/Tests/tests --gtest_filter=PatchAllowlist.VerifyHashes
static const inline Common::SHA1::Digest ACHIEVEMENT_APPROVED_LIST_HASH = {
    0x75, 0x4E, 0xFB, 0x44, 0xBB, 0x97, 0xD0, 0xF5, 0x62, 0xA0,
    0xCF, 0xEA, 0xC3, 0x0E, 0x93, 0xA3, 0x87, 0xF3, 0x7F, 0xB5};
