// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorFilter.h"

#include <fmt/format.h>

#include "Common/EnumUtils.h"

#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModEditor
{

bool DoesDrawCallMatchFilter(const DrawCallFilterContext& context, const EditorState& state,
                             GraphicsModSystem::DrawCallID draw_call_id)
{
  const auto runtime_iter = state.m_runtime_data.m_draw_call_id_to_data.find(draw_call_id);
  if (runtime_iter == state.m_runtime_data.m_draw_call_id_to_data.end())
    return false;

  bool filter = true;

  if (!context.text.empty())
  {
    if (const auto user_iter = state.m_user_data.m_draw_call_id_to_user_data.find(draw_call_id);
        user_iter != state.m_user_data.m_draw_call_id_to_user_data.end() &&
        !user_iter->second.m_friendly_name.empty())
    {
      filter &= user_iter->second.m_friendly_name.find(context.text) != std::string::npos;
    }
    else
    {
      const std::string id = fmt::to_string(Common::ToUnderlying(draw_call_id));
      filter &= id.find(context.text) != std::string::npos;
    }
  }

  u8 texture_count = 0;
  u8 efb_count = 0;
  for (const auto& texture : runtime_iter->second.draw_data.textures)
  {
    if (texture.texture_type == GraphicsModSystem::TextureType::Normal)
      texture_count++;

    if (texture.texture_type == GraphicsModSystem::TextureType::EFB)
      efb_count++;
  }

  switch (context.texture_filter)
  {
  case DrawCallFilterContext::TextureFilter::Any:
    // nop
    break;
  case DrawCallFilterContext::TextureFilter::TextureOnly:
    filter &= texture_count > 0 && efb_count == 0;
    break;
  case DrawCallFilterContext::TextureFilter::EFBOnly:
    filter &= texture_count == 0 && efb_count > 0;
    break;
  case DrawCallFilterContext::TextureFilter::Both:
    filter &= texture_count > 0 && efb_count > 0;
    break;
  case DrawCallFilterContext::TextureFilter::None:
    filter &= texture_count == 0 && efb_count == 0;
    break;
  };

  switch (context.projection_filter)
  {
  case DrawCallFilterContext::ProjectionFilter::Any:
    // nop
    break;
  case DrawCallFilterContext::ProjectionFilter::Orthographic:
    filter &= runtime_iter->second.draw_data.projection_type == ProjectionType::Orthographic;
    break;
  case DrawCallFilterContext::ProjectionFilter::Perspective:
    filter &= runtime_iter->second.draw_data.projection_type == ProjectionType::Perspective;
    break;
  };

  return filter;
}
}  // namespace GraphicsModEditor
