// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class ControllerInterface;

namespace ciface
{
class InputBackend
{
public:
  InputBackend(ControllerInterface* controller_interface);

  virtual ~InputBackend();

  virtual void PopulateDevices() = 0;
  virtual void UpdateInput();

  ControllerInterface& GetControllerInterface();

private:
  ControllerInterface& m_controller_interface;
};

}  // namespace ciface
