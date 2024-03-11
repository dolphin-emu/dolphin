// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/Quartz/Quartz.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/ControllerInterface/Quartz/QuartzKeyboardAndMouse.h"

namespace ciface::Quartz
{
std::string GetSourceName()
{
  return "Quartz";
}

class InputBackend final : public ciface::InputBackend
{
public:
  using ciface::InputBackend::InputBackend;
  void PopulateDevices() override;
  void HandleWindowChange() override;
};

std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface)
{
  return std::make_unique<InputBackend>(controller_interface);
}

void InputBackend::HandleWindowChange()
{
  const std::string source_name = GetSourceName();
  GetControllerInterface().RemoveDevice(
      [&](const auto* dev) { return dev->GetSource() == source_name; }, true);

  PopulateDevices();
}

void InputBackend::PopulateDevices()
{
  const WindowSystemInfo wsi = GetControllerInterface().GetWindowSystemInfo();
  if (wsi.type != WindowSystemType::MacOS)
    return;

  GetControllerInterface().AddDevice(std::make_shared<KeyboardAndMouse>(wsi.render_window));
}

}  // namespace ciface::Quartz
