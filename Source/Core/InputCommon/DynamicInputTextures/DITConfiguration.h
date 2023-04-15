// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "InputCommon/DynamicInputTextures/DITData.h"

namespace Common
{
class IniFile;
}

namespace InputCommon::DynamicInputTextures
{
class Configuration
{
public:
  explicit Configuration(const std::string& json_file);
  ~Configuration();
  bool GenerateTextures(const Common::IniFile& file,
                        const std::vector<std::string>& controller_names) const;

private:
  bool GenerateTexture(const Common::IniFile& file,
                       const std::vector<std::string>& controller_names,
                       const Data& texture_data) const;

  std::vector<Data> m_dynamic_input_textures;
  std::string m_base_path;
  bool m_valid = true;
};
}  // namespace InputCommon::DynamicInputTextures
