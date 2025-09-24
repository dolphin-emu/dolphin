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
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/ShaderGenCommon.h"

class CustomMeshAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "custom_mesh";
  static std::unique_ptr<CustomMeshAction>
  Create(const picojson::value& json_data,
         std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  explicit CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

  struct SerializableData
  {
    VideoCommon::CustomAssetLibrary::AssetID asset_id;
    Common::Matrix44 transform;
    bool ignore_mesh_transform;
  };

  CustomMeshAction(std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
                   SerializableData serializable_data);
  ~CustomMeshAction();
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

  void DrawImGui() override;
  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

  Common::Matrix44* GetTransform() override;

private:
  std::shared_ptr<VideoCommon::CustomAssetLibrary> m_library;
  VideoCommon::CachedAsset<VideoCommon::MeshAsset> m_cached_mesh_asset;

  SerializableData m_serializable_data;
  Common::Vec3 m_original_mesh_center = Common::Vec3{0, 0, 0};
  bool m_mesh_asset_changed = false;
  bool m_recalculate_original_mesh_center = true;
};
