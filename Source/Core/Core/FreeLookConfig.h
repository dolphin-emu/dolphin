// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// IMPORTANT: UI etc should modify the value returned by FreeLook::GetConfig().
// Free Look code should read from the value returned by FreeLook::GetActiveConfig().
// The reason for this is to get rid of race conditions etc when the
// configuration changes in the middle of a frame.

#pragma once

#include <string>

namespace FreeLook
{
enum class ControlType : int
{
  SixAxis,
  FPS,
  Orbital
};

struct CameraConfig
{
  ControlType control_type;
};

struct CameraConfig2D
{
  std::string layers;
};

// NEVER inherit from this class.
struct Config final
{
  Config();
  void Refresh();

  CameraConfig camera_config;
  CameraConfig2D camera_config_2d;
  bool enabled;
  bool enabled_2d;
};

Config& GetConfig();
const Config& GetActiveConfig();

// Called every frame.
void UpdateActiveConfig();
}  // namespace FreeLook
