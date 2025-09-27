// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/XFMemory.h"

namespace GraphicsModEditor
{
struct DrawCallData
{
  GraphicsModSystem::DrawCallID id;
  GraphicsModSystem::MaterialID material_id;
  GraphicsModSystem::MeshID mesh_id;
  std::chrono::steady_clock::time_point create_time;
  std::chrono::steady_clock::time_point last_update_time;

  GraphicsModSystem::DrawData draw_data;
};

struct TextureCacheData
{
  GraphicsModSystem::TextureCacheID id;
  std::chrono::steady_clock::time_point create_time;
  std::chrono::steady_clock::time_point last_load_time;
  bool active = false;

  GraphicsModSystem::TextureView texture;
};

struct LightData
{
  GraphicsModSystem::LightID m_id;
  std::chrono::steady_clock::time_point m_create_time;
  std::chrono::steady_clock::time_point m_last_update_time;

  int4 m_color;
  float4 m_cosatt;
  float4 m_distatt;
  float4 m_pos;
  float4 m_dir;
};

struct DrawCallUserData
{
  std::string m_friendly_name;
  std::vector<std::string> m_tag_names;
};

struct TextureCacheUserData
{
  std::string m_friendly_name;
  std::vector<std::string> m_tag_names;
};

struct LightUserData
{
  std::string m_friendly_name;
  std::vector<std::string> m_tag_names;
};

struct GpuSkinningData
{
  struct GpuSkinningForDrawCall
  {
    std::vector<u16> indices;
    std::vector<Common::Vec3> positions;
    std::vector<u32> skinning_index_per_vertex;
    std::vector<Common::Matrix44> view_to_object_transforms;
  };
  std::map<GraphicsModSystem::DrawCallID, GpuSkinningForDrawCall> m_draw_call_to_gpu_skinning;
};

using EditorAssetData = std::variant<
    std::unique_ptr<VideoCommon::MaterialData>, std::unique_ptr<VideoCommon::TextureAndSamplerData>,
    std::unique_ptr<VideoCommon::MeshData>, std::unique_ptr<VideoCommon::RasterSurfaceShaderData>,
    std::unique_ptr<VideoCommon::RenderTargetData>>;
enum AssetDataType
{
  Material,
  Mesh,
  PixelShader,
  RenderTarget,
  Shader,
  Texture
};
struct EditorAsset
{
  VideoCommon::CustomAssetLibrary::AssetID m_asset_id;
  std::filesystem::path m_asset_path;
  EditorAssetData m_data;
  AssetDataType m_data_type;
  VideoCommon::CustomAsset::TimeType m_last_data_write;
  VideoCommon::Assets::AssetMap m_asset_map;
  bool m_valid = false;
};

using SelectableType =
    std::variant<GraphicsModSystem::DrawCallID, GraphicsModSystem::TextureCacheID,
                 GraphicsModSystem::LightID, GraphicsModAction*, EditorAsset*>;

class EditorAction final : public GraphicsModAction
{
public:
  explicit EditorAction(std::unique_ptr<GraphicsModAction> action) : m_action(std::move(action)) {}
  void OnDrawStarted(GraphicsModActionData::DrawStarted* draw) override
  {
    if (m_active)
      m_action->OnDrawStarted(draw);
  }
  void OnEFB(GraphicsModActionData::EFB* efb) override
  {
    if (m_active)
      m_action->OnEFB(efb);
  }
  void OnXFB() override
  {
    if (m_active)
      m_action->OnXFB();
  }
  void OnProjection(GraphicsModActionData::Projection* projection) override
  {
    if (m_active)
      m_action->OnProjection(projection);
  }
  void OnProjectionAndTexture(GraphicsModActionData::Projection* projection) override
  {
    if (m_active)
      m_action->OnProjectionAndTexture(projection);
  }
  void OnTextureLoad(GraphicsModActionData::TextureLoad* texture_load) override
  {
    if (m_active)
      m_action->OnTextureLoad(texture_load);
  }
  void OnTextureCreate(GraphicsModActionData::TextureCreate* texture_create) override
  {
    if (m_active)
      m_action->OnTextureCreate(texture_create);
  }
  void OnLight(GraphicsModActionData::Light* light) override
  {
    if (m_active)
      m_action->OnLight(light);
  }
  void OnFrameEnd() override
  {
    if (m_active)
      m_action->OnFrameEnd();
  }

  void SetAllDrawCall(GraphicsModSystem::DrawCallID draw_call)
  {
    m_draw_call = draw_call;
    m_action->SetDrawCall(draw_call);
  }

  void DrawImGui() override
  {
    ImGui::Checkbox("##EmptyCheckbox", &m_active);
    m_action->DrawImGui();
  }

  void SerializeToConfig(picojson::object* obj) override
  {
    if (!obj) [[unlikely]]
      return;

    auto& json_obj = *obj;
    json_obj["active"] = picojson::value{m_active};
    m_action->SerializeToConfig(&json_obj);
  }

  std::string GetFactoryName() const override { return m_action->GetFactoryName(); }

  Common::Matrix44* GetTransform() override { return m_action->GetTransform(); }

  void SetActive(bool active) { m_active = active; }

private:
  bool m_active = true;
  std::unique_ptr<GraphicsModAction> m_action;
};
}  // namespace GraphicsModEditor
