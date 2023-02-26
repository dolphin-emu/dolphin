// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>

#include "Common/CommonTypes.h"

namespace Common
{
bool SyncSDFolderToSDImage(const std::function<bool()>& cancelled, bool deterministic);
bool SyncSDImageToSDFolder(const std::function<bool()>& cancelled);

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
