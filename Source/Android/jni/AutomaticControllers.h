// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace AutomaticControllers
{
// Whether generated mappings are currently applied to the emulated GameCube controllers.
// While they are, the controllers do not hold what the user configured, so saving them would
// write the generated mappings over it.
bool IsActive();
}  // namespace AutomaticControllers
