// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <span>

#include "Common/CommonTypes.h"

namespace DiscIO
{
// Descrambles one main data block in place. disc_key is std::nullopt for a standard DVD
void DescrambleDVDSector(std::span<u8, 2048> main_data, u32 psn, std::optional<u8> disc_key);

// DVD-ROM Error Detection Code
u32 CalculateDVDEDC(std::span<const u8> data);
}  // namespace DiscIO
