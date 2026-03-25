// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Virtual forwarder channel system: allows disc images (RVZ, ISO, etc.) to appear
// as launchable channel tiles on the Wii Menu.

#pragma once

#include <map>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace WiiForwarder
{
// Title type prefix for forwarder channels (same as normal channels).
constexpr u32 FORWARDER_TITLE_TYPE = 0x00010001;

// Magic XOR constant used to derive forwarder title IDs from disc game IDs.
constexpr u32 FORWARDER_MAGIC = 0x44465744;  // "DFWD"

// Generate a deterministic forwarder title ID from a disc's game ID string.
// The game ID (e.g. "RSBE01") is hashed to produce a unique lower-32 of the title ID.
u64 GenerateForwarderTitleID(const std::string& game_id);

// Check if a given title ID is a forwarder title by looking up the mapping file.
bool IsForwarderTitle(u64 title_id);

// Install a virtual forwarder channel for a disc image to the emulated NAND.
// The disc image can be any format Dolphin supports (RVZ, ISO, GCZ, etc.).
// When silent is true, errors are logged but no popup dialogs are shown.
// Returns true on success.
bool InstallForwarder(const std::string& disc_image_path, bool silent = false);

// Uninstall a forwarder channel from NAND and remove the mapping entry.
bool UninstallForwarder(u64 forwarder_title_id);

// Get the disc image path associated with a forwarder title ID.
std::optional<std::string> GetDiscImagePath(u64 forwarder_title_id);

// Check if a forwarder is already installed for a given disc image path.
bool IsForwarderInstalled(const std::string& disc_image_path);

// Get all installed forwarders as {title_id -> disc_image_path}.
std::map<u64, std::string> GetInstalledForwarders();

// Get the path to the forwarder mapping file on the host filesystem.
std::string GetForwarderMappingFilePath();

// Set a pending forwarder boot path. Called from ES when a forwarder channel is launched.
// The host (DolphinQt) should check this after stopping emulation and reboot with the disc.
void SetPendingForwarderBoot(const std::string& disc_image_path);

// Get and clear the pending forwarder boot path. Returns empty string if none pending.
std::string ConsumePendingForwarderBoot();

// Check if there is a pending forwarder boot.
bool HasPendingForwarderBoot();

}  // namespace WiiForwarder
