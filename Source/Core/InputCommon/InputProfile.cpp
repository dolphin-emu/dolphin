// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/InputProfile.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

namespace InputCommon
{
InputProfile::InputProfile(const std::string& profile_path, const std::string& root_directory)
    : m_file_path(profile_path), m_root_directory(root_directory)
{
  m_ini.Load(m_file_path);
}

InputProfile::~InputProfile() = default;

std::string InputProfile::GetRelativePathToRoot() const
{
  std::string basename;
  std::string basepath;
  SplitPath(m_file_path, &basepath, &basename, nullptr);
  return basepath.substr(m_root_directory.size() + 1) + basename;
}

const std::string& InputProfile::GetAbsolutePath() const
{
  return m_file_path;
}

void InputProfile::Apply(ControllerEmu::EmulatedController* controller)
{
  controller->LoadConfig(GetProfileData());
  controller->UpdateReferences(g_controller_interface);
}

IniFile::Section* InputProfile::GetProfileData()
{
  return m_ini.GetOrCreateSection("Profile");
}

}  // namespace InputCommon
