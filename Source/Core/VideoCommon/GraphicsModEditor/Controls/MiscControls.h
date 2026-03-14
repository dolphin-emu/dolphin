// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <imgui.h>

namespace GraphicsModEditor::Controls
{
// Pulled from the tips and tricks cafe on 2023, credit to pthom
bool ColorButton(const char* label, ImVec2 buttonSize, ImVec4 color);
}  // namespace GraphicsModEditor::Controls
