// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "Common/CommonTypes.h"

// Small utility functions for common Wii related tasks.

namespace WiiUtils
{
bool InstallWAD(const std::string& wad_path);

enum class UpdateResult
{
  Succeeded,
  Failed,
  AlreadyUpToDate,
};

using UpdateCallback = std::function<void(size_t processed, size_t total, u64 title_id)>;
// Download and install the latest version of all titles (if missing) from NUS.
UpdateResult DoOnlineUpdate(UpdateCallback update_callback = {});
}
