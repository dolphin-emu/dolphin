// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

class ControllerInterface;

namespace ciface
{

namespace Core
{
class Device;
}

class InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);

  virtual ~InputBackend();

  virtual void PopulateDevices() = 0;
  // Do NOT directly add/remove devices within here,
  // just add them to the removal list if necessary.
  virtual void UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove);

  ControllerInterface& GetControllerInterface();

private:
  ControllerInterface& m_controller_interface;
};

}  // namespace ciface
