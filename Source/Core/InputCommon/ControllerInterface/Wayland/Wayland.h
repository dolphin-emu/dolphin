// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

class ControllerInterface;

namespace ciface
{
class InputBackend;
}

namespace ciface::Wayland
{
// Keyboard and mouse input for native Wayland windows.
//
// Wayland only sends input to the surfaces of the client that has focus, so unlike with XInput2
// there is no input while no Dolphin window has focus (this is the same for every Wayland
// application, and is deliberate). The devices expose the same controls as the XInput2 ones.
std::unique_ptr<ciface::InputBackend> CreateInputBackend(ControllerInterface* controller_interface);
}  // namespace ciface::Wayland
