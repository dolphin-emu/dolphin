// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/FreeLookSettings.h"

#include <string>

#include "Common/Config/Config.h"
#include "Core/FreeLookConfig.h"

namespace Config
{
// Configuration Information
const Info<bool> FREE_LOOK_ENABLED{{System::FreeLook, "General", "Enabled"}, false};
const Info<bool> FREE_LOOK_BACKGROUND_INPUT{{System::FreeLook, "General", "BackgroundInput"},
                                            false};

// FreeLook.Controller1
const Info<FreeLook::ControlType> FL1_CONTROL_TYPE{{System::FreeLook, "Camera1", "ControlType"},
                                                   FreeLook::ControlType::SixAxis};

}  // namespace Config
