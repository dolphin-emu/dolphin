// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/FilesystemWatcher.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"

namespace VideoCommon
{
class WatchableFilesystemAssetLibrary : public CustomAssetLibrary, public Common::FilesystemWatcher
{
};
}  // namespace VideoCommon
