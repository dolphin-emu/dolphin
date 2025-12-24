// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string_view>

#include "imgui.h"

#include "VideoCommon/GraphicsModEditor/EditorState.h"

namespace GraphicsModEditor::Controls
{
static inline ImVec2 tag_size = ImVec2(175, 35);

bool TagSelectionWindow(std::string_view popup_name,
                        std::span<const GraphicsModSystem::Config::GraphicsModTag> tags,
                        std::size_t* chosen_tag);
}  // namespace GraphicsModEditor::Controls
