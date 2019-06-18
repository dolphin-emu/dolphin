// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
class IOSC;
}  // namespace IOS::HLE

namespace WiiSave
{
class Storage;
struct StorageDeleter
{
  void operator()(Storage* p) const;
};

using StoragePointer = std::unique_ptr<Storage, StorageDeleter>;
StoragePointer MakeNandStorage(IOS::HLE::FS::FileSystem* fs, u64 tid);
StoragePointer MakeDataBinStorage(IOS::HLE::IOSC* iosc, const std::string& path, const char* mode);

bool Copy(Storage* source, Storage* destination);

/// Import a save into the NAND from a .bin file.
bool Import(const std::string& data_bin_path, std::function<bool()> can_overwrite);
/// Export a save to a .bin file.
bool Export(u64 tid, const std::string& export_path);
/// Export all saves that are in the NAND. Returns the number of exported saves.
size_t ExportAll(const std::string& export_path);
}  // namespace WiiSave
