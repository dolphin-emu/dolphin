// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>

#include "VideoCommon/Resources/Resource.h"

#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/ShaderGenCommon.h"

namespace VideoCommon
{
class ShaderResource final : public Resource
{
public:
  ShaderResource(Resource::ResourceContext resource_context, const GXPipelineUid& pipeline_uid,
                 const std::string& preprocessor_settings,
                 const ShaderHostConfig& shader_host_config);

  class Data
  {
  public:
    AbstractShader* GetVertexShader() const;
    AbstractShader* GetPixelShader() const;
    AbstractShader* GetGeometryShader() const;

    bool IsCompiled() const;
    AbstractTextureType GetTextureType(std::size_t index);

  private:
    friend class ShaderResource;
    std::unique_ptr<AbstractShader> m_vertex_shader;
    std::unique_ptr<AbstractShader> m_pixel_shader;
    std::unique_ptr<AbstractShader> m_geometry_shader;
    std::shared_ptr<RasterSurfaceShaderData> m_shader_data;
    bool m_needs_geometry_shader = false;
    std::atomic_bool m_processing_finished;
  };

  // Changes the shader host config. Shaders should be reloaded afterwards.
  void SetHostConfig(const ShaderHostConfig& host_config);
  const std::shared_ptr<Data>& GetData() const { return m_current_data; }

  void MarkAsActive() override;
  void MarkAsPending() override;

private:
  void ResetData() override;
  Resource::TaskComplete CollectPrimaryData() override;
  TaskComplete ProcessData() override;

  // Note: asset cache owns the asset, we access as a reference
  RasterSurfaceShaderAsset* m_shader_asset = nullptr;

  std::shared_ptr<Data> m_current_data;
  std::shared_ptr<Data> m_load_data;

  bool m_processing_load_data = false;

  ShaderHostConfig m_shader_host_config;
  GXPipelineUid m_uid;
  std::string m_preprocessor_settings;
};
}  // namespace VideoCommon
