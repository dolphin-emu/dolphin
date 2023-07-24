// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/EditorState.h"

#include <chrono>

#include <fmt/format.h>

#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModFeature.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsTargetGroup.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionFactory.h"

namespace GraphicsModEditor
{
void WriteToGraphicsMod(const UserData& user_data, GraphicsModConfig* config)
{
  config->m_title = user_data.m_title;
  config->m_author = user_data.m_author;
  config->m_description = user_data.m_description;

  config->m_assets = user_data.m_asset_library->GetAssets(user_data.m_current_mod_path);

  for (const auto& [draw_call_id, actions] : user_data.m_draw_call_id_to_actions)
  {
    GraphicsTargetGroupConfig group;
    std::string group_name = fmt::format("group_{}", Common::ToUnderlying(draw_call_id));
    group.m_name = group_name;
    for (const auto& [operation_and_drawcall, action_references] :
         user_data.m_operation_and_draw_call_id_to_actions)
    {
      if (operation_and_drawcall.m_draw_call_id == draw_call_id)
      {
        switch (operation_and_drawcall.m_operation)
        {
        case OperationAndDrawCallID::Operation::Projection2D:
        {
        }
        break;
        case OperationAndDrawCallID::Operation::Projection3D:
        {
        }
        break;
        case OperationAndDrawCallID::Operation::TextureCreate:
        {
        }
        break;
        case OperationAndDrawCallID::Operation::TextureLoad:
        {
        }
        break;
        case OperationAndDrawCallID::Operation::Draw:
        {
          DrawStartedTarget target;
          target.m_draw_call_id = draw_call_id;
          group.m_targets.push_back(std::move(target));
        }
        break;
        };
      }
    }
    config->m_groups.push_back(std::move(group));

    for (const auto& action : actions)
    {
      GraphicsModFeatureConfig feature;
      feature.m_group = group_name;
      feature.m_action = action->GetFactoryName();

      picojson::object serialized_data;
      action->SerializeToConfig(&serialized_data);
      feature.m_action_data = picojson::value{serialized_data};

      config->m_features.push_back(std::move(feature));
    }
  }
}
void ReadFromGraphicsMod(UserData* user_data, const GraphicsModConfig& config)
{
  user_data->m_title = config.m_title;
  user_data->m_author = config.m_author;
  user_data->m_description = config.m_description;
  user_data->m_current_mod_path =
      PathToString(std::filesystem::path{config.GetAbsolutePath()}.parent_path());

  user_data->m_asset_library->AddAssets(config.m_assets, user_data->m_current_mod_path);

  std::map<std::string, std::vector<GraphicsMods::DrawCallID>> group_to_drawcalls;
  std::map<GraphicsMods::DrawCallID, std::vector<OperationAndDrawCallID>> drawcall_to_operations;

  const auto add_draw_call_target = [&](const auto& group,
                                        OperationAndDrawCallID::Operation operation,
                                        GraphicsMods::DrawCallID draw_call) {
    const auto [operations_iter, added] =
        drawcall_to_operations.try_emplace(draw_call, std::vector<OperationAndDrawCallID>{});
    if (added)
    {
      const auto [drawcalls_iter, added2] =
          group_to_drawcalls.try_emplace(group.m_name, std::vector<GraphicsMods::DrawCallID>{});
      drawcalls_iter->second.push_back(draw_call);
    }
    OperationAndDrawCallID operation_and_drawcall;
    operation_and_drawcall.m_draw_call_id = draw_call;
    operation_and_drawcall.m_operation = operation;
    operations_iter->second.push_back(std::move(operation_and_drawcall));
  };

  std::map<std::string, std::vector<FBInfo>> group_to_fbinfo;

  for (const auto& group : config.m_groups)
  {
    for (const auto& target : group.m_targets)
    {
      std::visit(overloaded{
                     [&](const DrawStartedTarget& the_target) {
                       add_draw_call_target(group, OperationAndDrawCallID::Operation::Draw,
                                            the_target.m_draw_call_id);
                     },
                     [&](const LoadTextureTarget&) {},
                     [&](const CreateTextureTarget&) {},
                     [&](const EFBTarget& the_target) {
                       FBInfo info;
                       info.m_height = the_target.m_height;
                       info.m_width = the_target.m_width;
                       info.m_texture_format = the_target.m_texture_format;

                       const auto [it, added] =
                           group_to_fbinfo.try_emplace(group.m_name, std::vector<FBInfo>{});
                       it->second.push_back(std::move(info));
                     },
                     [&](const XFBTarget& the_target) {
                       FBInfo info;
                       info.m_height = the_target.m_height;
                       info.m_width = the_target.m_width;
                       info.m_texture_format = the_target.m_texture_format;

                       const auto [it, added] =
                           group_to_fbinfo.try_emplace(group.m_name, std::vector<FBInfo>{});
                       it->second.push_back(std::move(info));
                     },
                 },
                 target);
    }
  }

  const auto create_action =
      [&](const std::string_view& action_name,
          const picojson::value& json_data) -> std::unique_ptr<GraphicsModAction> {
    auto action =
        GraphicsModActionFactory::Create(action_name, json_data, user_data->m_asset_library);
    if (action == nullptr)
    {
      return nullptr;
    }
    return action;
  };

  for (const auto& feature : config.m_features)
  {
    if (const auto it = group_to_drawcalls.find(feature.m_group); it != group_to_drawcalls.end())
    {
      for (const auto& draw_call : it->second)
      {
        auto action = create_action(feature.m_action, feature.m_action_data);
        if (!action)
          continue;

        const auto [drawcall_actions_it, added] = user_data->m_draw_call_id_to_actions.try_emplace(
            draw_call, std::vector<std::unique_ptr<EditorAction>>{});

        auto editor_action = std::make_unique<EditorAction>(std::move(action));

        if (feature.m_action_data.contains("name"))
        {
          editor_action->SetName(feature.m_action_data.get("name").to_str());
        }
        else
        {
          editor_action->SetName("Action");
        }

        if (feature.m_action_data.contains("id"))
        {
          editor_action->SetID(feature.m_action_data.get("id").to_str());
        }
        else
        {
          editor_action->SetID(
              fmt::format("{}", std::chrono::system_clock::now().time_since_epoch().count()));
        }

        if (feature.m_action_data.contains("active"))
        {
          editor_action->SetActive(feature.m_action_data.get("active").get<bool>());
        }
        drawcall_actions_it->second.push_back(std::move(editor_action));

        if (const auto operations_it = drawcall_to_operations.find(draw_call);
            operations_it != drawcall_to_operations.end())
        {
          for (const auto& operation : operations_it->second)
          {
            const auto [to_actions_it, otdc_added] =
                user_data->m_operation_and_draw_call_id_to_actions.try_emplace(
                    operation, std::vector<GraphicsModAction*>{});
            to_actions_it->second.push_back(drawcall_actions_it->second.back().get());
          }
        }
      }
    }

    if (const auto it = group_to_fbinfo.find(feature.m_group); it != group_to_fbinfo.end())
    {
      for (const auto& fbinfo : it->second)
      {
        auto action = create_action(feature.m_action, feature.m_action_data);
        if (!action)
          continue;

        const auto [fb_actions_it, added] = user_data->m_fb_call_id_to_actions.try_emplace(
            fbinfo, std::vector<std::unique_ptr<EditorAction>>{});
        fb_actions_it->second.push_back(std::make_unique<EditorAction>(std::move(action)));

        const auto [fb_actions_ref_it, fbc_added] =
            user_data->m_fb_call_id_to_reference_actions.try_emplace(
                fbinfo, std::vector<GraphicsModAction*>{});
        fb_actions_ref_it->second.push_back(fb_actions_it->second.back().get());
      }
    }
  }
}
}  // namespace GraphicsModEditor
