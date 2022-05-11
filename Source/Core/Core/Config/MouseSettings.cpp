// Copyright 20122 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/MouseSettings.h"

namespace Config
{
const Info<double> MOUSE_SENSITIVITY{{System::Main, "Input", "MouseSensitivity"}, 1.0};
const Info<double> MOUSE_SNAPPING_DISTANCE{{System::Main, "Input", "SnappingDistance"}, 0.0};
const Info<bool> MOUSE_OCTAGONAL_JAIL_ENABLED{{System::Main, "Input", "OctagonalMouseJailEnabled"},
                                              false};
}  // namespace Config
