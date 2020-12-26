// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "InputCommon/ImageOperations.h"

namespace InputCommon
{
class DynamicInputTextureConfiguration
{
public:
  explicit DynamicInputTextureConfiguration(const std::string& json_file);
  ~DynamicInputTextureConfiguration();
  void GenerateTextures(const IniFile::Section* sec, const std::string& controller_name) const;

private:
  struct DynamicInputTextureData
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

  bool GenerateTexture(const IniFile::Section* sec, const std::string& controller_name,
                       const DynamicInputTextureData& texture_data) const;

  std::vector<DynamicInputTextureData> m_dynamic_input_textures;
  std::string m_base_path;
  bool m_valid = true;
};
}  // namespace InputCommon
