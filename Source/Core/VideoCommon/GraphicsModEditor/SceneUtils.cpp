// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/SceneUtils.h"

#include <fmt/format.h>

#include "Common/EnumUtils.h"

namespace GraphicsModEditor
{
std::string GetDrawCallName(const EditorState& editor_state,
                            GraphicsModSystem::DrawCallID draw_call)
{
  if (draw_call == GraphicsModSystem::DrawCallID::INVALID)
    return "";

  std::string draw_call_name = std::to_string(Common::ToUnderlying(draw_call));
  auto& draw_call_to_user_data = editor_state.m_user_data.m_draw_call_id_to_user_data;
  if (const auto user_data_iter = draw_call_to_user_data.find(draw_call);
      user_data_iter != draw_call_to_user_data.end())
  {
    if (!user_data_iter->second.m_friendly_name.empty())
      draw_call_name = user_data_iter->second.m_friendly_name;
  }
  return draw_call_name;
}

std::string GetActionName(GraphicsModAction* action)
{
  if (!action) [[unlikely]]
    return "";
  return fmt::format("{}-{}", action->GetFactoryName(), action->GetID());
}
}  // namespace GraphicsModEditor
