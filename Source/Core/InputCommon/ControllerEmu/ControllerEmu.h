// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "InputCommon/ControllerInterface/Device.h"

class ControllerInterface;

#define sign(x) ((x) ? (x) < 0 ? -1 : 1 : 0)

const char* const named_directions[] = {"Up", "Down", "Left", "Right"};

namespace ControllerEmu
{
class ControlGroup;

class EmulatedController
{
public:
  virtual ~EmulatedController();
  virtual std::string GetName() const = 0;

  virtual void LoadDefaults(const ControllerInterface& ciface);

  virtual void LoadConfig(IniFile::Section* sec, const std::string& base = "");
  virtual void SaveConfig(IniFile::Section* sec, const std::string& base = "");
  void UpdateDefaultDevice();

  void UpdateReferences(const ControllerInterface& devi);

  // This returns a lock that should be held before calling State() on any control
  // references and GetState(), by extension. This prevents a race condition
  // which happens while handling a hotplug event because a control reference's State()
  // could be called before we have finished updating the reference.
  static std::unique_lock<std::recursive_mutex> GetStateLock();

  std::vector<std::unique_ptr<ControlGroup>> groups;

  ciface::Core::DeviceQualifier default_device;
};
}  // namespace ControllerEmu
