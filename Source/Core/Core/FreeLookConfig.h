// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// IMPORTANT: UI etc should modify the value returned by FreeLook::GetConfig().
// Free Look code should read from the value returned by FreeLook::GetActiveConfig().
// The reason for this is to get rid of race conditions etc when the
// configuration changes in the middle of a frame.

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace FreeLook
{
enum class ControlType : int
{
  SixAxis,
  FPS,
  Orbital,
  UDP,
};

struct UDPSettings
{
  std::string address;
  u16 port;
};

struct CameraConfig
{
  ControlType control_type;
  std::optional<UDPSettings> udp_settings;
};

// NEVER inherit from this class.
struct Config final
{
  Config();
  void Refresh();

  CameraConfig camera_config;
  bool enabled;
};

Config& GetConfig();
const Config& GetActiveConfig();

// Called every frame.
void UpdateActiveConfig();
}  // namespace FreeLook
