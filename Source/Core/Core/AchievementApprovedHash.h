// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Crypto/SHA1.h"

static constexpr std::string_view ACHIEVEMENT_APPROVED_LIST_FILENAME = "ApprovedInis.json";
// After building tests, find the new hash with:
// ./Binaries/Tests/tests --gtest_filter=PatchAllowlist.VerifyHashes
static const inline Common::SHA1::Digest ACHIEVEMENT_APPROVED_LIST_HASH = {
    0xEA, 0x2F, 0x74, 0xA1, 0x6C, 0xF3, 0xB5, 0xD4, 0x8A, 0xAF,
    0x03, 0x30, 0x58, 0x2A, 0xE0, 0xF7, 0x0A, 0x88, 0x86, 0xB3};
