// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class FileInfo;
struct Partition;
class Volume;

u64 ReadFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
             u8* buffer, u64 max_buffer_size, u64 offset_in_file = 0);
bool ExportData(const Volume& volume, const Partition& partition, u64 offset, u64 size,
                const std::string& export_filename);
bool ExportFile(const Volume& volume, const Partition& partition, const FileInfo* file_info,
                const std::string& export_filename);
bool ExportApploader(const Volume& volume, const Partition& partition,
                     const std::string& export_folder);
std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition);
std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset);
bool ExportDOL(const Volume& volume, const Partition& partition, const std::string& export_folder);

}  // namespace DiscIO
