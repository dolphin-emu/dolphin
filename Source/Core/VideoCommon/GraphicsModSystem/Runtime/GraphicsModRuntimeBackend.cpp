// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModRuntimeBackend.h"

#include <iostream>
#include <variant>

#include "Common/Logging/Log.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionFactory.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModHash.h"

namespace GraphicsModSystem::Runtime
{
GraphicsModRuntimeBackend::GraphicsModRuntimeBackend(const Config::GraphicsModGroup& config_data)
{
  for (auto& config_mod : config_data.GetMods())
  {
    if (!config_mod.m_enabled)
      continue;

    GraphicsMod runtime_mod;

    runtime_mod.m_hash_policy = config_mod.m_mod.m_default_hash_policy;

    auto filesystem_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();
    auto texture_cache = std::make_shared<VideoCommon::CustomTextureCache>();

    for (const auto& config_asset : config_mod.m_mod.m_assets)
    {
      auto asset_map = config_asset.m_map;
      for (auto& [k, v] : asset_map)
      {
        if (v.is_absolute())
        {
          WARN_LOG_FMT(VIDEO,
                       "Specified graphics mod asset '{}' for mod '{}' has an absolute path, you "
                       "shouldn't release this to users.",
                       config_asset.m_asset_id, config_mod.m_id);
        }
        else
        {
          v = std::filesystem::path{config_mod.m_path} / v;
        }
      }

      filesystem_library->SetAssetIDMapData(config_asset.m_asset_id, std::move(asset_map));
    }

    for (const auto& config_action : config_mod.m_mod.m_actions)
    {
      runtime_mod.m_actions.push_back(GraphicsModActionFactory::Create(
          config_action.m_factory_name, config_action.m_data, filesystem_library, texture_cache));
    }

    std::map<std::string, std::vector<GraphicsModAction*>> tag_to_actions;
    for (const auto& [tag_name, action_indexes] : config_mod.m_mod.m_tag_name_to_action_indexes)
    {
      for (const auto& index : action_indexes)
      {
        tag_to_actions[tag_name].push_back(runtime_mod.m_actions[index].get());
      }
    }

    struct IDWithTags
    {
      std::variant<DrawCallID, std::string> m_id;
      const std::vector<std::string>* m_tag_names;
    };
    std::vector<IDWithTags> targets;
    for (const auto& target : config_mod.m_mod.m_targets)
    {
      std::visit(overloaded{[&](const Config::IntTarget& int_target) {
                              const DrawCallID id{int_target.m_target_id};
                              runtime_mod.m_draw_id_to_actions.insert_or_assign(
                                  id, std::vector<GraphicsModAction*>{});
                              targets.push_back(IDWithTags{id, &int_target.m_tag_names});
                            },
                            [&](const Config::StringTarget& str_target) {
                              const auto id{str_target.m_target_id};
                              runtime_mod.m_str_id_to_actions.insert_or_assign(
                                  id, std::vector<GraphicsModAction*>{});
                              targets.push_back(IDWithTags{id, &str_target.m_tag_names});
                            }},
                 target);
    }

    // Handle any specific actions set on targets
    for (std::size_t i = 0; i < targets.size(); i++)
    {
      const auto& target = targets[i];
      std::visit(
          overloaded{
              [&](DrawCallID draw_id) {
                auto& actions = runtime_mod.m_draw_id_to_actions[draw_id];

                // First add all specific actions
                if (const auto iter = config_mod.m_mod.m_target_index_to_action_indexes.find(i);
                    iter != config_mod.m_mod.m_target_index_to_action_indexes.end())
                {
                  for (const auto& action_index : iter->second)
                  {
                    actions.push_back(runtime_mod.m_actions[action_index].get());
                  }
                }

                // Then add all tag actions
                for (const auto& tag_name : *target.m_tag_names)
                {
                  auto& tag_actions = tag_to_actions[tag_name];
                  actions.insert(actions.end(), tag_actions.begin(), tag_actions.end());
                }
              },
              [&](const std::string& str_id) {
                auto& actions = runtime_mod.m_str_id_to_actions[str_id];

                // First add all specific actions
                if (const auto iter = config_mod.m_mod.m_target_index_to_action_indexes.find(i);
                    iter != config_mod.m_mod.m_target_index_to_action_indexes.end())
                {
                  for (const auto& action_index : iter->second)
                  {
                    actions.push_back(runtime_mod.m_actions[action_index].get());
                  }
                }

                // Then add all tag actions
                for (const auto& tag_name : *target.m_tag_names)
                {
                  auto& tag_actions = tag_to_actions[tag_name];
                  actions.insert(actions.end(), tag_actions.begin(), tag_actions.end());
                }
              }},
          target.m_id);
    }

    m_mods.push_back(std::move(runtime_mod));
  }
}

void GraphicsModRuntimeBackend::OnDraw(const DrawDataView& draw_data,
                                       VertexManagerBase* vertex_manager)
{
  if (m_mods.empty())
  {
    vertex_manager->DrawEmulatedMesh();
    return;
  }

  for (auto& mod : m_mods)
  {
    const auto hash_output = GetDrawDataHash(mod.m_hash_policy, draw_data);
    const DrawCallID draw_call_id =
        GetSkinnedDrawCallID(hash_output.draw_call_id, hash_output.material_id, draw_data);

    if (const auto iter = mod.m_draw_id_to_actions.find(draw_call_id);
        iter != mod.m_draw_id_to_actions.end())
    {
      CustomDraw(draw_data, vertex_manager, iter->second);
      return;
    }
  }

  vertex_manager->DrawEmulatedMesh();
}

void GraphicsModRuntimeBackend::OnTextureLoad(const TextureView& texture)
{
  for (auto& mod : m_mods)
  {
    if (const auto iter = mod.m_str_id_to_actions.find(texture.hash_name);
        iter != mod.m_str_id_to_actions.end())
    {
      GraphicsModActionData::TextureLoad load{.texture_name = texture.hash_name};
      for (const auto& action : iter->second)
      {
        action->OnTextureLoad(&load);
      }
      break;
    }
  }
}

void GraphicsModRuntimeBackend::OnTextureUnload(TextureType, std::string_view)
{
}

void GraphicsModRuntimeBackend::OnTextureCreate(const TextureView& texture)
{
  for (auto& mod : m_mods)
  {
    if (const auto iter = mod.m_str_id_to_actions.find(texture.hash_name);
        iter != mod.m_str_id_to_actions.end())
    {
      GraphicsModActionData::TextureCreate texture_create{.texture_name = texture.hash_name};
      for (const auto& action : iter->second)
      {
        action->OnTextureCreate(&texture_create);
      }
      break;
    }
  }
}

void GraphicsModRuntimeBackend::OnLight()
{
}

void GraphicsModRuntimeBackend::OnFramePresented(const PresentInfo&)
{
  for (auto& mod : m_mods)
  {
    for (const auto& action : mod.m_actions)
    {
      action->OnFrameEnd();
    }
  }
}

void GraphicsModRuntimeBackend::AddIndices(OpcodeDecoder::Primitive, u32)
{
}

void GraphicsModRuntimeBackend::ResetIndices()
{
}

}  // namespace GraphicsModSystem::Runtime
