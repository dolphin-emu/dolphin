// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace Common
{

// These return U+FFFD "REPLACEMENT CHARACTER" on failure.
char32_t DecodeSingleByteShiftJIS(u8 code_unit);
char32_t DecodeDoubleByteShiftJIS(u8 first_code_unit, u8 second_code_unit);

// Returns u16(-1) on failure.
u16 EncodeShiftJIS(char32_t unicode);

}  // namespace Common
