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

class CustomMeshAction final : public GraphicsModAction
{
public:
  static std::unique_ptr<CustomMeshAction>
  Create(const picojson::value& json_data,
         std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  explicit CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library, Common::Vec3 rotation,
                   Common::Vec3 scale, Common::Vec3 translation,
                   VideoCommon::CustomAssetLibrary::AssetID mesh_asset_id);
  ~CustomMeshAction();
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

  void DrawImGui() override;
  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

private:
  std::shared_ptr<VideoCommon::CustomAssetLibrary> m_library;
  VideoCommon::CustomAssetLibrary::AssetID m_mesh_asset_id;
  VideoCommon::CachedAsset<VideoCommon::MeshAsset> m_cached_mesh_asset;

  Common::Vec3 m_scale = Common::Vec3{1, 1, 1};
  Common::Vec3 m_rotation = Common::Vec3{0, 0, 0};
  Common::Vec3 m_translation = Common::Vec3{0, 0, 0};
  Common::Vec3 m_original_mesh_center = Common::Vec3{0, 0, 0};
  bool m_transform_changed = false;
  bool m_mesh_asset_changed = false;
  bool m_recalculate_original_mesh_center = true;

  struct RenderChunk
  {
    CustomPipeline m_custom_pipeline;
    GraphicsModActionData::MeshChunk m_mesh_chunk;
    std::unique_ptr<NativeVertexFormat> m_native_vertex_format;
    Common::SmallVector<u32, 8> m_tex_units;
  };
  std::vector<RenderChunk> m_render_chunks;
  std::shared_ptr<VideoCommon::MeshData> m_mesh_data;
};
