// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_set>

#include "Common/CommonTypes.h"

// Small utility functions for common Wii related tasks.

namespace DiscIO
{
class VolumeWAD;
}

namespace IOS::HLE
{
class Kernel;
}

namespace WiiUtils
{
enum class InstallType
{
  Permanent,
  Temporary,
};

bool InstallWAD(IOS::HLE::Kernel& ios, const DiscIO::VolumeWAD& wad, InstallType type);
// Same as the above, but constructs a temporary IOS and VolumeWAD instance for importing
// and does a permanent install.
bool InstallWAD(const std::string& wad_path);

bool UninstallTitle(u64 title_id);

bool IsTitleInstalled(u64 title_id);

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

// Check the emulated NAND for common issues.
struct NANDCheckResult
{
  bool bad = false;
  std::unordered_set<u64> titles_to_remove;
};
NANDCheckResult CheckNAND(IOS::HLE::Kernel& ios);
bool RepairNAND(IOS::HLE::Kernel& ios);
}  // namespace WiiUtils
