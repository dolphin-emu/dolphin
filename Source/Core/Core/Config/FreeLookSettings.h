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

// FreeLook.Controller1
extern const Info<FreeLook::ControlType> FL1_CONTROL_TYPE;
extern const Info<std::string> FL1_UDP_ADDRESS;
extern const Info<u16> FL1_UDP_PORT;

}  // namespace Config
