// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

namespace FreeLook
{
enum class ControlType : int;
}

namespace Config
{
// Configuration Information

extern const Info<bool> FREE_LOOK_ENABLED;
extern const Info<bool> FREE_LOOK_BACKGROUND_INPUT;

// FreeLook.Controller1
extern const Info<FreeLook::ControlType> FL1_CONTROL_TYPE;

}  // namespace Config
