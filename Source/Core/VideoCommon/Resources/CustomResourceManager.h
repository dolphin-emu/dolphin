// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>

#include "Common/HookableEvent.h"
#include "Common/WorkQueueThread.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/CustomAssetCache.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/Resources/MaterialResource.h"
#include "VideoCommon/Resources/ShaderResource.h"
#include "VideoCommon/Resources/TextureAndSamplerResource.h"
#include "VideoCommon/Resources/TextureDataResource.h"
#include "VideoCommon/Resources/TexturePool.h"

namespace VideoCommon
{
class CustomResourceManager
{
public:
  void Initialize();
  void Shutdown();

  void Reset();

  // Request that an asset be reloaded
  void MarkAssetDirty(const CustomAssetLibrary::AssetID& asset_id);

  void XFBTriggered();
  void SetHostConfig(const ShaderHostConfig& host_config);

  TextureDataResource*
  GetTextureDataFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                          std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  MaterialResource* GetMaterialFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                         const GXPipelineUid& pipeline_uid,
                                         std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

  ShaderResource* GetShaderFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                     std::size_t shader_key, const GXPipelineUid& pipeline_uid,
                                     const std::string& preprocessor_settings,
                                     std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  TextureAndSamplerResource*
  GetTextureAndSamplerFromAsset(const CustomAssetLibrary::AssetID& asset_id,
                                std::shared_ptr<VideoCommon::CustomAssetLibrary> library);

private:
  Resource::ResourceContext
  CreateResourceContext(const CustomAssetLibrary::AssetID& asset_id,
                        std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  CustomAssetCache m_asset_cache;
  TexturePool m_texture_pool;
  Common::AsyncWorkThreadSP m_worker_thread;
  std::unique_ptr<AsyncShaderCompiler> m_async_shader_compiler;

  using PipelineIdToMaterial = std::map<std::size_t, std::unique_ptr<MaterialResource>>;
  std::map<CustomAssetLibrary::AssetID, PipelineIdToMaterial> m_material_resources;

  using ShaderKeyToShader = std::map<std::size_t, std::unique_ptr<ShaderResource>>;
  std::map<CustomAssetLibrary::AssetID, ShaderKeyToShader> m_shader_resources;

  std::map<CustomAssetLibrary::AssetID, std::unique_ptr<TextureDataResource>>
      m_texture_data_resources;

  std::map<CustomAssetLibrary::AssetID, std::unique_ptr<TextureAndSamplerResource>>
      m_texture_sampler_resources;

  ShaderHostConfig m_host_config;

  Common::EventHook m_xfb_event;

  std::unique_ptr<AbstractTexture> m_invalid_transparent_texture;
  std::unique_ptr<AbstractTexture> m_invalid_color_texture;
  std::unique_ptr<AbstractTexture> m_invalid_cubemap_texture;
  std::unique_ptr<AbstractTexture> m_invalid_array_texture;
};
}  // namespace VideoCommon
