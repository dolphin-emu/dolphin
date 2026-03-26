// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace WiiForwarder
{
constexpr u32 FORWARDER_TITLE_TYPE = 0x00010001;
constexpr u32 FORWARDER_MAGIC = 0x44465744;  // "DFWD"

// Hashes the game ID to produce a deterministic title ID for the forwarder channel.
u64 GenerateForwarderTitleID(const std::string& game_id);

bool IsForwarderTitle(u64 title_id);

// When silent is true, errors are logged but no popup dialogs are shown.
bool InstallForwarder(const std::string& disc_image_path, bool silent = false);

bool UninstallForwarder(u64 forwarder_title_id);

std::optional<std::string> GetDiscImagePath(u64 forwarder_title_id);

bool IsForwarderInstalled(const std::string& disc_image_path);

std::map<u64, std::string> GetInstalledForwarders();

std::string GetForwarderMappingFilePath();

// Called from ES when a forwarder channel is launched. The host (DolphinQt)
// checks this after stopping emulation and reboots with the disc image.
void SetPendingForwarderBoot(const std::string& disc_image_path);

std::string ConsumePendingForwarderBoot();

bool HasPendingForwarderBoot();

}  // namespace WiiForwarder
