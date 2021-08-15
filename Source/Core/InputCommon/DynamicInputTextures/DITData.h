// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "InputCommon/ImageOperations.h"

namespace InputCommon::DynamicInputTextures
{
struct Data
{
  std::string m_image_name;
  std::string m_hires_texture_name;
  std::string m_generated_folder_name;

  using EmulatedKeyToRegionsMap = std::unordered_map<std::string, std::vector<Rect>>;
  std::unordered_map<std::string, EmulatedKeyToRegionsMap> m_emulated_controllers;

  using HostKeyToImagePath = std::unordered_map<std::string, std::string>;
  std::unordered_map<std::string, HostKeyToImagePath> m_host_devices;
  bool m_preserve_aspect_ratio = true;
};
}  // namespace InputCommon::DynamicInputTextures
