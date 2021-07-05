// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class FileInfo;
struct Partition;
class Volume;

constexpr u64 MINI_DVD_SIZE = 1459978240;  // GameCube
constexpr u64 SL_DVD_SIZE = 4699979776;    // Wii retail
constexpr u64 SL_DVD_R_SIZE = 4707319808;  // Wii RVT-R
constexpr u64 DL_DVD_SIZE = 8511160320;    // Wii retail
constexpr u64 DL_DVD_R_SIZE = 8543666176;  // Wii RVT-R

constexpr u32 GAMECUBE_DISC_MAGIC = 0xC2339F3D;
constexpr u32 WII_DISC_MAGIC = 0x5D1C9EA3;

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

u64 GetBiggestReferencedOffset(const Volume& volume);
u64 GetBiggestReferencedOffset(const Volume& volume, const std::vector<Partition>& partitions);
}  // namespace DiscIO
