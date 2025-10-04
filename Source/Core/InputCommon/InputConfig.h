// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/DynamicInputTextureManager.h"

namespace Common
{
class IniFile;
}

namespace ControllerEmu
{
class EmulatedController;
}

class InputConfig
{
public:
  InputConfig(const std::string& ini_name, const std::string& gui_name,
      const std::string& profile_directory_name, const std::string& profile_key);

  ~InputConfig();

  bool LoadConfig();
  void SaveConfig();

  template <typename T, typename... Args>
  void CreateController(Args&&... args)
  {
    m_controllers.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
  }

  ControllerEmu::EmulatedController* GetController(int index) const;
  void ClearControllers();
  bool ControllersNeedToBeCreated() const;
  bool IsControllerControlledByGamepadDevice(int index) const;

  std::string GetGUIName() const { return m_gui_name; }
  std::string GetProfileKey() const { return m_profile_key; }
  std::string GetProfileDirectoryName() const { return m_profile_directory_name; }
  std::string GetUserProfileDirectoryPath() const;
  std::string GetSysProfileDirectoryPath() const;
  int GetControllerCount() const;

  // These should be used after creating all controllers and before clearing them, respectively.
  void RegisterHotplugCallback();
  void UnregisterHotplugCallback();

  void GenerateControllerTextures(const Common::IniFile& file);
  void GenerateControllerTextures();

private:
  ControllerInterface::HotplugCallbackHandle m_hotplug_callback_handle;
  std::vector<std::unique_ptr<ControllerEmu::EmulatedController>> m_controllers;
  const std::string m_ini_name;
  const std::string m_gui_name;
  const std::string m_profile_directory_name;
  const std::string m_profile_key;
  InputCommon::DynamicInputTextureManager m_dynamic_input_tex_config_manager;
};
