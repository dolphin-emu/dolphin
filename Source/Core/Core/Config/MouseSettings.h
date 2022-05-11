// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Config/Config.h"

namespace Config
{
extern const Info<double> MOUSE_SENSITIVITY;
static constexpr double scaling_factor_to_make_mouse_sensitivity_one_for_fullscreen = 2.0;
extern const Info<double> MOUSE_SNAPPING_DISTANCE;
extern const Info<bool> MOUSE_OCTAGONAL_JAIL_ENABLED;
}  // namespace Config
