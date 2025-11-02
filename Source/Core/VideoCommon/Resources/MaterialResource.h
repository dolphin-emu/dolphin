// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <string_view>
#include <variant>
#include <vector>

#include "Common/Buffer.h"
#include "Common/SmallVector.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/TextureSamplerValue.h"
#include "VideoCommon/Constants.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/Resources/Resource.h"
#include "VideoCommon/Resources/ShaderResource.h"
#include "VideoCommon/Resources/TextureAndSamplerResource.h"

namespace VideoCommon
{
class MaterialResource final : public Resource
{
public:
  MaterialResource(Resource::ResourceContext resource_context, const GXPipelineUid& pipeline_uid);

  struct TextureLikeReference
  {
    SamplerState sampler;
    u32 sampler_index;
    TextureSamplerValue::SamplerOrigin sampler_origin;
    std::string_view texture_hash;
    AbstractTexture* texture;
  };

  class Data
  {
  public:
    AbstractPipeline* GetPipeline() const { return m_pipeline.get(); }
    std::span<const u8> GetUniforms() const { return m_uniform_data; }
    std::span<const TextureLikeReference> GetTextures() const { return m_texture_like_references; }
    MaterialResource* GetNextMaterial() const { return m_next_material; }

  private:
    friend class MaterialResource;
    std::unique_ptr<AbstractPipeline> m_pipeline = nullptr;
    Common::UniqueBuffer<u8> m_uniform_data;
    std::shared_ptr<MaterialData> m_material_data = nullptr;
    ShaderResource* m_shader_resource = nullptr;

    using TextureLikeResource = Resource*;
    Common::SmallVector<TextureLikeResource, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS>
        m_texture_like_resources;

    // Variant for future expansion...
    using TextureLikeData = std::variant<std::shared_ptr<TextureAndSamplerResource::Data>>;
    Common::SmallVector<TextureLikeData, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS>
        m_texture_like_data;

    Common::SmallVector<TextureLikeReference, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS>
        m_texture_like_references;

    MaterialResource* m_next_material = nullptr;
    std::size_t m_shader_key;
    std::string m_preprocessor_settings;
    std::atomic_bool m_processing_finished;
  };

  const std::shared_ptr<Data>& GetData() const { return m_current_data; }
  void MarkAsActive() override;
  void MarkAsPending() override;

private:
  void ResetData() override;
  Resource::TaskComplete CollectPrimaryData() override;
  Resource::TaskComplete CollectDependencyData() override;
  Resource::TaskComplete ProcessData() override;

  static void CreateTextureData(Data* data);
  static void SetShaderKey(Data* data, GXPipelineUid* uid);
  static void WriteUniforms(Data* data);

  std::shared_ptr<Data> m_current_data;

  std::shared_ptr<Data> m_load_data;
  bool m_processing_load_data = false;

  // Note: asset cache owns the asset, we access as a reference
  MaterialAsset* m_material_asset = nullptr;

  GXPipelineUid m_uid;
  std::unique_ptr<NativeVertexFormat> m_uid_vertex_format_copy;
};
}  // namespace VideoCommon
