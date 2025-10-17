// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/FreeLookSettings.h"

#include <string>

#include "Common/Config/Config.h"
#include "Core/Config/InputFocus.h"
#include "Core/FreeLookConfig.h"

namespace Config
{
// Configuration Information
const Info<bool> FREE_LOOK_ENABLED{{System::FreeLook, "General", "Enabled"}, false};
const Info<InputFocusPolicy> FREE_LOOK_FOCUS_POLICY{
    {System::FreeLook, "General", "BackgroundInput"}, InputFocusPolicy::RenderOrTASWindow};

// FreeLook.Controller1
const Info<FreeLook::ControlType> FL1_CONTROL_TYPE{{System::FreeLook, "Camera1", "ControlType"},
                                                   FreeLook::ControlType::SixAxis};

}  // namespace Config
