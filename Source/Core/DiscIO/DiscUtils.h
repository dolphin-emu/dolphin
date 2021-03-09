// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct Partition;
class Volume;

constexpr u32 PARTITION_DATA = 0;
constexpr u32 PARTITION_UPDATE = 1;
constexpr u32 PARTITION_CHANNEL = 2;  // Mario Kart Wii, Wii Fit, Wii Fit Plus, Rabbids Go Home
constexpr u32 PARTITION_INSTALL = 3;  // Dragon Quest X only

std::string NameForPartitionType(u32 partition_type, bool include_prefix);

std::optional<u64> GetApploaderSize(const Volume& volume, const Partition& partition);
std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition);
std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset);
std::optional<u64> GetFSTOffset(const Volume& volume, const Partition& partition);
std::optional<u64> GetFSTSize(const Volume& volume, const Partition& partition);
}
