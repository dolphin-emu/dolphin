// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "InputCommon/DynamicInputTextures/DITData.h"
#include "InputCommon/ImageOperations.h"

namespace Common
{
class IniFile;
}

namespace InputCommon::DynamicInputTextures
{
// Output folder name to image name to image data
using OutputDetails = std::map<std::string, std::map<std::string, ImagePixelData>>;
class Configuration
{
public:
  explicit Configuration(const std::string& json_path);
  ~Configuration();
  void GenerateTextures(const Common::IniFile& file,
                        const std::vector<std::string>& controller_names,
                        OutputDetails* output) const;

private:
  void GenerateTexture(const Common::IniFile& file,
                       const std::vector<std::string>& controller_names, const Data& texture_data,
                       OutputDetails* output) const;

  std::vector<Data> m_dynamic_input_textures;
  std::string m_base_path;
  bool m_valid = true;
};
}  // namespace InputCommon::DynamicInputTextures
