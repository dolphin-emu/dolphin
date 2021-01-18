// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/IniFile.h"

namespace ControllerEmu
{
class EmulatedController;
}

namespace InputCommon
{
class InputProfile
{
public:
  explicit InputProfile(const std::string& profile_path, const std::string& root_directory);
  ~InputProfile();
  std::string GetRelativePathToRoot() const;
  const std::string& GetAbsolutePath() const;
  void Apply(ControllerEmu::EmulatedController* controller);
  IniFile::Section* GetProfileData();

private:
  std::string m_file_path;
  std::string m_root_directory;
  IniFile m_ini;
};
}  // namespace InputCommon
