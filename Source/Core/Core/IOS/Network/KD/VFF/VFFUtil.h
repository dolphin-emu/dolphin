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
ErrorCode OpenVFF(const std::string& path, const std::string& filename,
                  const std::shared_ptr<FS::FileSystem>& fs, const std::vector<u8>& data);
}
}  // namespace IOS::HLE

#pragma pack(push, 1)
struct Header final
{
  u8 magic[4];
  u16 endianness;
  u16 unknown_marker;
  u32 volumeSize;
  u16 clusterSize;
  u16 _empty;
  u16 _unknown;
  u8 padding[14];
};
#pragma pack(pop)
