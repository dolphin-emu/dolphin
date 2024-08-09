// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <picojson.h>

#include "Common/Matrix.h"
#include "Common/SmallVector.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/MeshAsset.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomPipeline.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon
{
class CustomTextureCache;
}

class CustomMeshAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "custom_mesh";
  static std::unique_ptr<CustomMeshAction>
  Create(const picojson::value& json_data, std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
         std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache);
  explicit CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                            std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache);
  CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                   std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache,
                   Common::Vec3 rotation, Common::Vec3 scale, Common::Vec3 translation,
                   VideoCommon::CustomAssetLibrary::AssetID mesh_asset_id);
  ~CustomMeshAction();
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

  void DrawImGui() override;
  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

private:
  std::shared_ptr<VideoCommon::CustomAssetLibrary> m_library;
  std::shared_ptr<VideoCommon::CustomTextureCache> m_texture_cache;
  VideoCommon::CustomAssetLibrary::AssetID m_mesh_asset_id;
  VideoCommon::CachedAsset<VideoCommon::MeshAsset> m_cached_mesh_asset;

  Common::Vec3 m_scale = Common::Vec3{1, 1, 1};
  Common::Vec3 m_rotation = Common::Vec3{0, 0, 0};
  Common::Vec3 m_translation = Common::Vec3{0, 0, 0};
  Common::Vec3 m_original_mesh_center = Common::Vec3{0, 0, 0};
  bool m_transform_changed = false;
  bool m_mesh_asset_changed = false;
  bool m_recalculate_original_mesh_center = true;
  bool m_ignore_mesh_transform = false;
  bool m_use_game_material = false;

  struct RenderChunk
  {
    CustomPipeline m_custom_pipeline;
    GraphicsModActionData::MeshChunk m_mesh_chunk;
    std::unique_ptr<NativeVertexFormat> m_native_vertex_format;
    Common::SmallVector<GraphicsModSystem::TextureView, 8> m_textures;
  };
  std::vector<RenderChunk> m_render_chunks;
  std::shared_ptr<VideoCommon::MeshData> m_mesh_data;
};
