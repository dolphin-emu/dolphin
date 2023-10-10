// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include "DiscIO/Filesystem.h"

namespace Subtitles
{
const std::string BottomOSDStackName = "subtitles-bottom";
const std::string TopOSDStackName = "subtitles-top";
void Reload();
void OnFileAccess(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset);
}  // namespace Subtitles
