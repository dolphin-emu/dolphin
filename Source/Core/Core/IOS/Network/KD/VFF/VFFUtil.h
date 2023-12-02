// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24
{
constexpr u16 SECTOR_SIZE = 512;
constexpr u16 VF_LITTLE_ENDIAN = 0xFFFE;
constexpr u16 VF_BIG_ENDIAN = 0xFEFF;
ErrorCode WriteToVFF(const std::string& path, const std::string& filename,
                     const std::shared_ptr<FS::FileSystem>& fs, const std::vector<u8>& data);
ErrorCode ReadFromVFF(const std::string& path, const std::string& filename,
                      const std::shared_ptr<FS::FileSystem>& fs, std::vector<u8>& out);
ErrorCode DeleteFileFromVFF(const std::string& path, const std::string& filename,
                            const std::shared_ptr<FS::FileSystem>& fs);

#pragma pack(push, 1)
struct VFFHeader final
{
  u8 magic[4];
  u16 endianness;
  u16 unknown_marker;
  u32 volume_size;
  u16 cluster_size;
  u16 empty;
  u16 unknown;
  u8 padding[14];
};
static_assert(sizeof(VFFHeader) == 32);
#pragma pack(pop)
}  // namespace NWC24
}  // namespace IOS::HLE
