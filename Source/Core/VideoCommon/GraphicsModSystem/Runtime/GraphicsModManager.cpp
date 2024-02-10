// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"

#include <string>
#include <string_view>
#include <variant>

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"

#include "Core/ConfigManager.h"

#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsMod.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAsset.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModGroup.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModActionFactory.h"
#include "VideoCommon/TextureInfo.h"
#include "VideoCommon/VideoConfig.h"

std::unique_ptr<GraphicsModManager> g_graphics_mod_manager;

class GraphicsModManager::DecoratedAction final : public GraphicsModAction
{
public:
  DecoratedAction(std::unique_ptr<GraphicsModAction> action, GraphicsModConfig mod)
      : m_action_impl(std::move(action)), m_mod(std::move(mod))
  {
  }
  void OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started) override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnDrawStarted(draw_started);
  }
  void OnEFB(GraphicsModActionData::EFB* efb) override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnEFB(efb);
  }
  void OnProjection(GraphicsModActionData::Projection* projection) override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnProjection(projection);
  }
  void OnProjectionAndTexture(GraphicsModActionData::Projection* projection) override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnProjectionAndTexture(projection);
  }
  void OnTextureLoad(GraphicsModActionData::TextureLoad* texture_load) override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnTextureLoad(texture_load);
  }
  void OnTextureCreate(GraphicsModActionData::TextureCreate* texture_create) override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnTextureCreate(texture_create);
  }
  void OnFrameEnd() override
  {
    if (!m_mod.m_enabled)
      return;
    m_action_impl->OnFrameEnd();
  }

private:
  std::unique_ptr<GraphicsModAction> m_action_impl;
  GraphicsModConfig m_mod;
};

bool GraphicsModManager::Initialize()
{
  if (g_ActiveConfig.bGraphicMods)
  {
    // If a config change occurred in a previous session,
    // remember the old change count value.  By setting
    // our current change count to the old value, we
    // avoid loading the stale data when we
    // check for config changes.
    const u32 old_game_mod_changes = g_ActiveConfig.graphics_mod_config ?
                                         g_ActiveConfig.graphics_mod_config->GetChangeCount() :
                                         0;
    g_ActiveConfig.graphics_mod_config = GraphicsModGroupConfig(SConfig::GetInstance().GetGameID());
    g_ActiveConfig.graphics_mod_config->Load();
    g_ActiveConfig.graphics_mod_config->SetChangeCount(old_game_mod_changes);
    g_graphics_mod_manager->Load(*g_ActiveConfig.graphics_mod_config);

    m_end_of_frame_event =
        AfterFrameEvent::Register([this](Core::System&) { EndOfFrame(); }, "ModManager");
  }

  return true;
}

const std::vector<GraphicsModAction*>&
GraphicsModManager::GetProjectionActions(ProjectionType projection_type) const
{
  if (const auto it = m_projection_target_to_actions.find(projection_type);
      it != m_projection_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

const std::vector<GraphicsModAction*>&
GraphicsModManager::GetProjectionTextureActions(ProjectionType projection_type,
                                                const std::string& texture_name) const
{
  const auto lookup = fmt::format("{}_{}", texture_name, static_cast<int>(projection_type));
  if (const auto it = m_projection_texture_target_to_actions.find(lookup);
      it != m_projection_texture_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

const std::vector<GraphicsModAction*>&
GraphicsModManager::GetDrawStartedActions(const std::string& texture_name) const
{
  if (const auto it = m_draw_started_target_to_actions.find(texture_name);
      it != m_draw_started_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

const std::vector<GraphicsModAction*>&
GraphicsModManager::GetTextureLoadActions(const std::string& texture_name) const
{
  if (const auto it = m_load_texture_target_to_actions.find(texture_name);
      it != m_load_texture_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

const std::vector<GraphicsModAction*>&
GraphicsModManager::GetTextureCreateActions(const std::string& texture_name) const
{
  if (const auto it = m_create_texture_target_to_actions.find(texture_name);
      it != m_create_texture_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

const std::vector<GraphicsModAction*>& GraphicsModManager::GetEFBActions(const FBInfo& efb) const
{
  if (const auto it = m_efb_target_to_actions.find(efb); it != m_efb_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

const std::vector<GraphicsModAction*>& GraphicsModManager::GetXFBActions(const FBInfo& xfb) const
{
  if (const auto it = m_efb_target_to_actions.find(xfb); it != m_efb_target_to_actions.end())
  {
    return it->second;
  }

  return m_default;
}

void GraphicsModManager::Load(const GraphicsModGroupConfig& config)
{
  Reset();

  const auto& mods = config.GetMods();

  auto filesystem_library = std::make_shared<VideoCommon::DirectFilesystemAssetLibrary>();

  std::map<std::string, std::vector<GraphicsTargetConfig>> group_to_targets;
  for (const auto& mod : mods)
  {
    for (const GraphicsTargetGroupConfig& group : mod.m_groups)
    {
      if (m_groups.find(group.m_name) != m_groups.end())
      {
        WARN_LOG_FMT(
            VIDEO,
            "Specified graphics mod group '{}' for mod '{}' is already specified by another mod.",
            group.m_name, mod.m_title);
      }
      m_groups.insert(group.m_name);

      const auto internal_group = fmt::format("{}.{}", mod.m_title, group.m_name);
      for (const GraphicsTargetConfig& target : group.m_targets)
      {
        group_to_targets[group.m_name].push_back(target);
        group_to_targets[internal_group].push_back(target);
      }
    }

    std::string base_path;
    SplitPath(mod.GetAbsolutePath(), &base_path, nullptr, nullptr);
    for (const GraphicsModAssetConfig& asset : mod.m_assets)
    {
      auto asset_map = asset.m_map;
      for (auto& [k, v] : asset_map)
      {
        if (v.is_absolute())
        {
          WARN_LOG_FMT(VIDEO,
                       "Specified graphics mod asset '{}' for mod '{}' has an absolute path, you "
                       "shouldn't release this to users.",
                       asset.m_asset_id, mod.m_title);
        }
        else
        {
          v = std::filesystem::path{base_path} / v;
        }
      }

      filesystem_library->SetAssetIDMapData(asset.m_asset_id, std::move(asset_map));
    }
  }

  for (const auto& mod : mods)
  {
    for (const GraphicsModFeatureConfig& feature : mod.m_features)
    {
      const auto create_action =
          [filesystem_library](const std::string_view& action_name,
                               const picojson::value& json_data,
                               GraphicsModConfig mod_config) -> std::unique_ptr<GraphicsModAction> {
        auto action =
            GraphicsModActionFactory::Create(action_name, json_data, std::move(filesystem_library));
        if (action == nullptr)
        {
          return nullptr;
        }
        return std::make_unique<DecoratedAction>(std::move(action), std::move(mod_config));
      };

      const auto internal_group = fmt::format("{}.{}", mod.m_title, feature.m_group);

      const auto add_target = [&](const GraphicsTargetConfig& target) {
        std::visit(
            overloaded{
                [&](const DrawStartedTextureTarget& the_target) {
                  m_draw_started_target_to_actions[the_target.m_texture_info_string].push_back(
                      m_actions.back().get());
                },
                [&](const LoadTextureTarget& the_target) {
                  m_load_texture_target_to_actions[the_target.m_texture_info_string].push_back(
                      m_actions.back().get());
                },
                [&](const CreateTextureTarget& the_target) {
                  m_create_texture_target_to_actions[the_target.m_texture_info_string].push_back(
                      m_actions.back().get());
                },
                [&](const EFBTarget& the_target) {
                  FBInfo info;
                  info.m_height = the_target.m_height;
                  info.m_width = the_target.m_width;
                  info.m_texture_format = the_target.m_texture_format;
                  m_efb_target_to_actions[info].push_back(m_actions.back().get());
                },
                [&](const XFBTarget& the_target) {
                  FBInfo info;
                  info.m_height = the_target.m_height;
                  info.m_width = the_target.m_width;
                  info.m_texture_format = the_target.m_texture_format;
                  m_xfb_target_to_actions[info].push_back(m_actions.back().get());
                },
                [&](const ProjectionTarget& the_target) {
                  if (the_target.m_texture_info_string)
                  {
                    const auto lookup = fmt::format("{}_{}", *the_target.m_texture_info_string,
                                                    static_cast<int>(the_target.m_projection_type));
                    m_projection_texture_target_to_actions[lookup].push_back(
                        m_actions.back().get());
                  }
                  else
                  {
                    m_projection_target_to_actions[the_target.m_projection_type].push_back(
                        m_actions.back().get());
                  }
                },
            },
            target);
      };

      const auto add_action = [&](GraphicsModConfig mod_config) -> bool {
        auto action = create_action(feature.m_action, feature.m_action_data, std::move(mod_config));
        if (action == nullptr)
        {
          WARN_LOG_FMT(VIDEO, "Failed to create action '{}' for group '{}'.", feature.m_action,
                       feature.m_group);
          return false;
        }
        m_actions.push_back(std::move(action));
        return true;
      };

      // Prefer groups in the pack over groups from another pack
      if (const auto local_it = group_to_targets.find(internal_group);
          local_it != group_to_targets.end())
      {
        if (add_action(mod))
        {
          for (const GraphicsTargetConfig& target : local_it->second)
          {
            add_target(target);
          }
        }
      }
      else if (const auto global_it = group_to_targets.find(feature.m_group);
               global_it != group_to_targets.end())
      {
        if (add_action(mod))
        {
          for (const GraphicsTargetConfig& target : global_it->second)
          {
            add_target(target);
          }
        }
      }
      else
      {
        WARN_LOG_FMT(VIDEO, "Specified graphics mod group '{}' was not found for mod '{}'",
                     feature.m_group, mod.m_title);
      }
    }
  }
}

void GraphicsModManager::EndOfFrame()
{
  for (auto&& action : m_actions)
  {
    action->OnFrameEnd();
  }
}

void GraphicsModManager::Reset()
{
  m_actions.clear();
  m_groups.clear();
  m_projection_target_to_actions.clear();
  m_projection_texture_target_to_actions.clear();
  m_draw_started_target_to_actions.clear();
  m_load_texture_target_to_actions.clear();
  m_create_texture_target_to_actions.clear();
  m_efb_target_to_actions.clear();
  m_xfb_target_to_actions.clear();
}
