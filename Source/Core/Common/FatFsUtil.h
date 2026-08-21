// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <string>

#include "Common/Common.h"
#include "Common/CommonTypes.h"

namespace Common
{
static constexpr auto SD_PACK_TEXT = _trans("Pack SD Card Now");
static constexpr auto SD_UNPACK_TEXT = _trans("Unpack SD Card Now");

// Wii SD card: paths and size come from the Wii config keys.
bool SyncSDFolderToSDImage(const std::function<bool()>& cancelled, bool deterministic);
bool SyncSDImageToSDFolder(const std::function<bool()>& cancelled);

// Explicit-path variants (used for the GameCube EXI SD card). configured_size of 0 sizes the
// image automatically from the folder contents. differential unpacks by writing only files
// whose content changed and never deletes — safe for a real SD card or a drive root; without
// it the target directory is rebuilt from scratch.
bool SyncSDFolderToSDImage(const std::string& source_dir, const std::string& image_path,
                           u64 configured_size, const std::function<bool()>& cancelled,
                           bool deterministic);
bool SyncSDImageToSDFolder(const std::string& image_path, const std::string& target_dir,
                           const std::function<bool()>& cancelled, bool differential);

class FatFsCallbacks
{
public:
  FatFsCallbacks();
  virtual ~FatFsCallbacks();

  virtual u8 DiskInitialize(u8 pdrv);
  virtual u8 DiskStatus(u8 pdrv);
  virtual int DiskRead(u8 pdrv, u8* buff, u32 sector, unsigned int count) = 0;
  virtual int DiskWrite(u8 pdrv, const u8* buff, u32 sector, unsigned int count) = 0;
  virtual int DiskIOCtl(u8 pdrv, u8 cmd, void* buff) = 0;
  virtual u32 GetCurrentTimeFAT();
};

void RunInFatFsContext(FatFsCallbacks& callbacks, const std::function<void()>& function);
}  // namespace Common
