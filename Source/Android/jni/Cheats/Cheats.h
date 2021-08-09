// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Cheats
{
constexpr int TRY_SET_FAIL_CODE_MIXED_ENCRYPTION = -3;
constexpr int TRY_SET_FAIL_NO_CODE_LINES = -2;
constexpr int TRY_SET_FAIL_NO_NAME = -1;
constexpr int TRY_SET_SUCCESS = 0;
// Result codes greater than 0 represent an error on the corresponding code line (one-indexed)
}  // namespace Cheats
