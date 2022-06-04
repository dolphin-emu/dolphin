// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"

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

// 128 KiB (0x20000) is the default block size for GCZ/RVZ images
constexpr int GCZ_RVZ_PREFERRED_BLOCK_SIZE = 0x20000;

// 32 KiB (0x8000) was picked because DVD timings are emulated as if we can't read less than
// an entire ECC block at once. Therefore, little reason to choose a smaller block size.
constexpr int PREFERRED_MIN_BLOCK_SIZE = 0x8000;

// 2 MiB (0x200000) was picked because it is the smallest block size supported by WIA.
// For performance reasons, blocks shouldn't be too large.
constexpr int PREFERRED_MAX_BLOCK_SIZE = 0x200000;

// If we didn't find a good GCZ block size, pick the block size which was hardcoded
// in legacy versions. That way, at least we're not worse than older versions.
// 16 KiB (0x4000) for supporting GCZs in versions of Dolphin prior to 5.0-11893
constexpr int GCZ_FALLBACK_BLOCK_SIZE = 0x4000;

// 2 MiB (0x200000) is the smallest block size supported by WIA.
constexpr int WIA_MIN_BLOCK_SIZE = 0x200000;

// 32 KiB (0x8000) is the smallest block size supported by RVZ.
constexpr int RVZ_MIN_BLOCK_SIZE = 0x8000;

// 2 MiB (0x200000): for RVZ, block sizes larger than 2 MiB must be an integer multiple of 2 MiB.
constexpr int RVZ_BIG_BLOCK_SIZE_LCM = 0x200000;

std::string NameForPartitionType(u32 partition_type, bool include_prefix);

std::optional<u64> GetApploaderSize(const Volume& volume, const Partition& partition);
std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition);
std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset);
std::optional<u64> GetFSTOffset(const Volume& volume, const Partition& partition);
std::optional<u64> GetFSTSize(const Volume& volume, const Partition& partition);

u64 GetBiggestReferencedOffset(const Volume& volume);
u64 GetBiggestReferencedOffset(const Volume& volume, const std::vector<Partition>& partitions);

bool IsGCZBlockSizeLegacyCompatible(int block_size, u64 file_size);
bool IsDiscImageBlockSizeValid(int block_size, DiscIO::BlobType format);
}  // namespace DiscIO
