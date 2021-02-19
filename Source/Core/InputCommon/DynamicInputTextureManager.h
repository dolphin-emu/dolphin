// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/IniFile.h"

#include <string>
#include <vector>

namespace InputCommon
{
namespace DynamicInputTextures
{
class Configuration;
}
class DynamicInputTextureManager
{
public:
  DynamicInputTextureManager();
  ~DynamicInputTextureManager();
  void Load();
  void GenerateTextures(const IniFile::Section* sec, const std::string& controller_name);

private:
  std::vector<DynamicInputTextures::Configuration> m_configuration;
  std::string m_config_type;
};
}  // namespace InputCommon
