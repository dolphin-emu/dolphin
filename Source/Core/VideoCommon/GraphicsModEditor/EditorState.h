// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAsset.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/GraphicsModEditor/EditorAssetSource.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModEditor/SceneDumper.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/GraphicsModSystem/Types.h"

struct GraphicsModConfig;

namespace GraphicsModEditor
{
struct OperationAndDrawCallID
{
  enum class Operation
  {
    Projection2D,
    Projection3D,
    TextureCreate,
    TextureLoad,
    Draw
  };
  Operation m_operation;

  GraphicsMods::DrawCallID m_draw_call_id;

  auto operator<=>(const OperationAndDrawCallID&) const = default;
};
struct EditorData
{
  // References of draw actions defined by the editor
  // (ex: highlight)
  std::map<OperationAndDrawCallID, std::vector<GraphicsModAction*>>
      m_operation_and_draw_call_id_to_actions;

  // References of efb actions defined by the editor
  // (ex: highlight)
  std::map<FBInfo, std::vector<GraphicsModAction*>> m_fb_call_id_to_actions;

  std::map<GraphicsMods::LightID, std::vector<GraphicsModAction*>> m_light_id_to_actions;

  // An action used by the editor to highlight selected draw calls
  std::unique_ptr<GraphicsModAction> m_highlight_action;

  // An action used by the editor to highlight selected lights
  std::unique_ptr<GraphicsModAction> m_highlight_light_action;

  // A map of editor specific textures accessible by name
  std::map<std::string, std::unique_ptr<AbstractTexture>> m_name_to_texture;

  // A map of editor specific templates accessible by name
  std::map<std::string, std::string> m_name_to_template;

  // Editor specific assets
  std::vector<std::shared_ptr<VideoCommon::CustomAsset>> m_assets;

  // User data that is waiting on the editor to generate
  // a preview
  std::map<VideoCommon::CustomAssetLibrary::AssetID, std::shared_ptr<VideoCommon::CustomAsset>>
      m_assets_waiting_for_preview;

  std::shared_ptr<VideoCommon::DirectFilesystemAssetLibrary> m_asset_library;
};

struct UserData
{
  std::string m_title;
  std::string m_author;
  std::string m_description;
  std::filesystem::path m_current_mod_path;

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<OperationAndDrawCallID, std::vector<GraphicsModAction*>>
      m_operation_and_draw_call_id_to_actions;

  // The data stored for the above and in a more easily accessible structure
  // for usage in the editor
  std::map<GraphicsMods::DrawCallID, std::vector<std::unique_ptr<EditorAction>>>
      m_draw_call_id_to_actions;

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<FBInfo, std::vector<GraphicsModAction*>> m_fb_call_id_to_reference_actions;

  // The data stored for the above and in a more easily accessible structure
  // for usage in the editor
  std::map<FBInfo, std::vector<std::unique_ptr<EditorAction>>> m_fb_call_id_to_actions;

  // References of actions defined by the user to provide to the graphics mod interface
  std::map<GraphicsMods::LightID, std::vector<GraphicsModAction*>> m_light_id_to_reference_actions;

  // The data stored for the above and in a more easily accessible structure
  // for usage in the editor
  std::map<GraphicsMods::LightID, std::vector<std::unique_ptr<EditorAction>>> m_light_id_to_actions;

  // Mapping the draw call id to any user data for the target
  std::map<GraphicsMods::DrawCallID, DrawCallUserData> m_draw_call_id_to_user_data;

  // Mapping the fbinfo to any user data for the target
  std::map<FBInfo, FBCallUserData> m_fb_call_id_to_user_data;

  // Mapping the light id to any user data for the target
  std::map<GraphicsMods::LightID, LightUserData> m_light_id_to_user_data;

  // The asset library provided to all user actions
  std::shared_ptr<EditorAssetSource> m_asset_library;
};
void WriteToGraphicsMod(const UserData& user_data, GraphicsModConfig* config);
void ReadFromGraphicsMod(UserData* user_data, const GraphicsModConfig& config);

struct RuntimeState
{
  // Mapping the draw call id to the frame's data
  std::map<GraphicsMods::DrawCallID, DrawCallData> m_draw_call_id_to_data;

  // Mapping the draw call id to the frame's data
  std::map<FBInfo, FBCallData> m_fb_call_id_to_data;

  // Mapping the draw call id to the frame's data
  std::map<GraphicsMods::LightID, LightData> m_light_id_to_data;
};

struct EditorState
{
  EditorData m_editor_data;
  UserData m_user_data;
  RuntimeState m_runtime_data;
  SceneDumper m_scene_dumper;
};
}  // namespace GraphicsModEditor
