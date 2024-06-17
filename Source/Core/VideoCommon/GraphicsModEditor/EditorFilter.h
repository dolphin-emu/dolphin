// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor
{
struct EditorState;

struct DrawCallFilterContext
{
  std::string text;

  enum class TextureFilter
  {
    Any,
    EFBOnly,
    TextureOnly,
    Both,
    None
  };
  TextureFilter texture_filter = TextureFilter::Any;

  enum class ProjectionFilter
  {
    Any,
    Orthographic,
    Perspective
  };
  ProjectionFilter projection_filter = ProjectionFilter::Any;
};

bool DoesDrawCallMatchFilter(const DrawCallFilterContext& context, const EditorState& state,
                             GraphicsModSystem::DrawCallID draw_call_id);
}  // namespace GraphicsModEditor
