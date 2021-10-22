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

constexpr u32 DISCHEADER_ADDRESS = 0;
constexpr u32 DISCHEADER_SIZE = 0x440;
constexpr u32 BI2_ADDRESS = 0x440;
constexpr u32 BI2_SIZE = 0x2000;
constexpr u32 APPLOADER_ADDRESS = 0x2440;

constexpr u32 WII_PARTITION_TICKET_ADDRESS = 0;
constexpr u32 WII_PARTITION_TICKET_SIZE = 0x2a4;
constexpr u32 WII_PARTITION_TMD_SIZE_ADDRESS = 0x2a4;
constexpr u32 WII_PARTITION_TMD_OFFSET_ADDRESS = 0x2a8;
constexpr u32 WII_PARTITION_CERT_CHAIN_SIZE_ADDRESS = 0x2ac;
constexpr u32 WII_PARTITION_CERT_CHAIN_OFFSET_ADDRESS = 0x2b0;
constexpr u32 WII_PARTITION_H3_OFFSET_ADDRESS = 0x2b4;
constexpr u32 WII_PARTITION_H3_SIZE = 0x18000;

constexpr u32 WII_NONPARTITION_DISCHEADER_ADDRESS = 0;
constexpr u32 WII_NONPARTITION_DISCHEADER_SIZE = 0x100;
constexpr u32 WII_REGION_DATA_ADDRESS = 0x4E000;
constexpr u32 WII_REGION_DATA_SIZE = 0x20;

std::string NameForPartitionType(u32 partition_type, bool include_prefix);

std::optional<u64> GetApploaderSize(const Volume& volume, const Partition& partition);
std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition);
std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset);
std::optional<u64> GetFSTOffset(const Volume& volume, const Partition& partition);
std::optional<u64> GetFSTSize(const Volume& volume, const Partition& partition);

u64 GetBiggestReferencedOffset(const Volume& volume);
u64 GetBiggestReferencedOffset(const Volume& volume, const std::vector<Partition>& partitions);
}  // namespace DiscIO
