// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/EditorAssetSource.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModEditor/SceneDumper.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModHashPolicy.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModTag.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureCache.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

namespace GraphicsModSystem::Config
{
struct GraphicsMod;
}

namespace GraphicsModEditor
{
struct EditorData
{
  // An action used by the editor to highlight selected draw calls
  std::unique_ptr<GraphicsModAction> m_highlight_action;

  // An action used by the editor to highlight selected lights
  std::unique_ptr<GraphicsModAction> m_highlight_light_action;

  // An action used by the editor to display lighting
  std::unique_ptr<GraphicsModAction> m_simple_light_visualization_action;

  // A map of editor specific textures accessible by name
  std::map<std::string, std::unique_ptr<AbstractTexture>> m_name_to_texture;

  // A map of editor specific templates accessible by name
  std::map<std::string, std::string> m_name_to_template;

  // Editor specific assets
  std::vector<std::shared_ptr<VideoCommon::CustomAsset>> m_assets;

  // Editor specific tags
  std::map<std::string, GraphicsModSystem::Config::GraphicsModTag> m_tags;

  // User data that is waiting on the editor to generate
  // a preview
  std::map<VideoCommon::CustomAssetLibrary::AssetID, std::shared_ptr<VideoCommon::CustomAsset>>
      m_assets_waiting_for_preview;

  std::shared_ptr<VideoCommon::DirectFilesystemAssetLibrary> m_asset_library;

  bool m_disable_all_actions = false;

  bool m_view_lighting = false;

  u64 m_next_action_id = 1;
};

struct UserData
{
  std::string m_title;
  std::string m_author;
  std::string m_description;
  std::filesystem::path m_current_mod_path;

  GraphicsModSystem::Config::HashPolicy m_hash_policy =
      GraphicsModSystem::Config::GetDefaultHashPolicy();

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<GraphicsModSystem::DrawCallID, std::vector<GraphicsModAction*>>
      m_draw_call_id_to_actions;

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<GraphicsModSystem::TextureCacheID, std::vector<GraphicsModAction*>>
      m_texture_cache_id_to_actions;

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<GraphicsModSystem::LightID, std::vector<GraphicsModAction*>>
      m_light_id_to_reference_actions;

  std::vector<std::unique_ptr<EditorAction>> m_actions;

  std::vector<GraphicsModSystem::Config::GraphicsModTag> m_tags;

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<std::string, std::vector<GraphicsModAction*>> m_tag_name_to_actions;

  // Mapping the draw call id to any user data for the target
  std::map<GraphicsModSystem::DrawCallID, DrawCallUserData> m_draw_call_id_to_user_data;

  // Mapping the texture cache id to any user data for the target
  std::map<GraphicsModSystem::TextureCacheID, TextureCacheUserData> m_texture_cache_id_to_user_data;

  // Mapping the light id to any user data for the target
  std::map<GraphicsModSystem::LightID, LightUserData> m_light_id_to_user_data;

  // The asset library provided to all user actions
  std::shared_ptr<EditorAssetSource> m_asset_library;
};

struct RuntimeState
{
  struct XFBData
  {
    struct DrawCallWithTime
    {
      GraphicsModSystem::DrawCallID draw_call_id;
      std::chrono::steady_clock::time_point sort_time;
    };

    struct DrawCallWithTimeCompare
    {
      bool operator()(const DrawCallWithTime& lhs, const DrawCallWithTime& rhs) const
      {
        return lhs.sort_time < rhs.sort_time;
      }
    };

    // The draw call ids this frame
    std::set<DrawCallWithTime, DrawCallWithTimeCompare> m_draw_call_ids;

    // The EFB/XFB calls triggered this frame
    std::set<GraphicsModSystem::TextureCacheID> m_texture_cache_ids;

    // The game lights triggered this frame
    std::set<GraphicsModSystem::LightID> m_light_ids;
  };
  std::map<std::string, XFBData, std::less<>> m_xfb_to_data;
  std::vector<std::string> m_xfbs_presented;

  XFBData m_current_xfb;

  // Mapping the draw call id to the last known data
  std::map<GraphicsModSystem::DrawCallID, DrawCallData> m_draw_call_id_to_data;

  // Mapping the EFB/XFB calls to the last known data
  std::map<GraphicsModSystem::TextureCacheID, TextureCacheData, std::less<>>
      m_texture_cache_id_to_data;

  // Mapping the game light id to the last known data
  std::map<GraphicsModSystem::LightID, LightData> m_light_id_to_data;

  // Texture cache management
  std::shared_ptr<VideoCommon::CustomTextureCache> m_texture_cache;
};

struct EditorState
{
  EditorData m_editor_data;
  UserData m_user_data;
  RuntimeState m_runtime_data;
  SceneDumper m_scene_dumper;
};

void WriteToGraphicsMod(const UserData& user_data, GraphicsModSystem::Config::GraphicsMod* config);
void ReadFromGraphicsMod(UserData* user_data, EditorData* editor_data,
                         const RuntimeState& runtime_state,
                         const GraphicsModSystem::Config::GraphicsMod& config,
                         const std::string& mod_root);

}  // namespace GraphicsModEditor
