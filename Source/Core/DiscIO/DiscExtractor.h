// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
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

// update_progress is called once for each child (file or directory).
// If update_progress returns true, the extraction gets cancelled.
// filesystem_path is supposed to be the path corresponding to the directory argument.
void ExportDirectory(const DiscIO::Volume& volume, const DiscIO::Partition partition,
                     const DiscIO::FileInfo& directory, bool recursive,
                     const std::string& filesystem_path, const std::string& export_folder,
                     const std::function<bool(const std::string& path)>& update_progress);

bool ExportApploader(const Volume& volume, const Partition& partition,
                     const std::string& export_filename);
std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition);
std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset);
bool ExportDOL(const Volume& volume, const Partition& partition,
               const std::string& export_filename);

}  // namespace DiscIO
