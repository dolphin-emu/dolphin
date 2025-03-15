// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <concepts>
#include <string>
#include <vector>

#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModEditor
{
std::string GetDrawCallName(const EditorState& editor_state,
                            GraphicsModSystem::DrawCallID draw_call);
std::string GetActionName(GraphicsModAction* action);

template <std::derived_from<GraphicsModAction> ActionType>
std::vector<ActionType*> GetActionsForDrawCall(const EditorState& editor_state,
                                               GraphicsModSystem::DrawCallID draw_call)
{
  std::vector<ActionType*> result;
  auto& draw_call_id_to_actions = editor_state.m_user_data.m_draw_call_id_to_actions;
  if (const auto actions_iter = draw_call_id_to_actions.find(draw_call);
      actions_iter != draw_call_id_to_actions.end())
  {
    for (GraphicsModAction* action : actions_iter->second)
    {
      if (action->GetFactoryName() == ActionType::factory_name)
        result.push_back(static_cast<ActionType*>(action));
    }
  }
  return result;
}
}  // namespace GraphicsModEditor
