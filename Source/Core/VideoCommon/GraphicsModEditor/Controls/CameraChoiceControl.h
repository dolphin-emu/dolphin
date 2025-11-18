// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor::Controls
{
bool CameraChoiceControl(std::string_view popup_name, EditorState* editor_state,
                         GraphicsModSystem::DrawCallID* draw_call_chosen);
}  // namespace GraphicsModEditor::Controls
