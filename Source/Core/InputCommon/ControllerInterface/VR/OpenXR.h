// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace ciface::VR
{
void RegisterInputOverrider(int controller_index);
void UnregisterInputOverrider(int controller_index);

void UpdateInput(int controller_index);
}  // namespace ciface::VR
