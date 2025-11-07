// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Crypto/SHA1.h"

static constexpr std::string_view ACHIEVEMENT_APPROVED_LIST_FILENAME = "ApprovedInis.json";
// After building tests, find the new hash with:
// ./Binaries/Tests/tests --gtest_filter=PatchAllowlist.VerifyHashes
static const inline Common::SHA1::Digest ACHIEVEMENT_APPROVED_LIST_HASH = {
    0xAF, 0xF0, 0xE3, 0x45, 0xF0, 0xCD, 0x2C, 0x58, 0xA9, 0xC9,
    0x5C, 0x9F, 0x8C, 0x71, 0x8A, 0xB6, 0x28, 0x7F, 0x17, 0x89};
