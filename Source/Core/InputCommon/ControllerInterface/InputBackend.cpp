// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerInterface/InputBackend.h"

namespace ciface
{
InputBackend::InputBackend(ControllerInterface* controller_interface)
    : m_controller_interface(*controller_interface)
{
}

InputBackend::~InputBackend() = default;

void InputBackend::UpdateInput(std::vector<std::weak_ptr<ciface::Core::Device>>& devices_to_remove)
{
}

void InputBackend::HandleWindowChange()
{
}

ControllerInterface& InputBackend::GetControllerInterface()
{
  return m_controller_interface;
}

}  // namespace ciface
