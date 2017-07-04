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
  AlreadyUpToDate,

  // Current region does not match disc region.
  RegionMismatch,
  // Missing update partition on disc.
  MissingUpdatePartition,
  // Missing or invalid files on disc.
  DiscReadFailed,

  // NUS errors and failures.
  ServerFailed,
  // General download failures.
  DownloadFailed,

  // Import failures.
  ImportFailed,
  // Update was cancelled.
  Cancelled,
};

// Return false to cancel the update as soon as the current title has finished updating.
using UpdateCallback = std::function<bool(size_t processed, size_t total, u64 title_id)>;
// Download and install the latest version of all titles (if missing) from NUS.
// If no region is specified, the region of the installed System Menu will be used.
// If no region is specified and no system menu is installed, the update will fail.
UpdateResult DoOnlineUpdate(UpdateCallback update_callback, const std::string& region);

// Perform a disc update with behaviour similar to the System Menu.
UpdateResult DoDiscUpdate(UpdateCallback update_callback, const std::string& image_path);
}
