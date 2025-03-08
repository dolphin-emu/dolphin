// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/MaterialAsset.h"
#include "VideoCommon/Assets/ShaderAsset.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/GraphicsModSystem/Types.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoEvents.h"

struct CustomPipelineShader
{
  std::shared_ptr<VideoCommon::RasterShaderData> shader_data;
  const GraphicsModSystem::MaterialResource* material;
};

struct CustomPipelineMaterial
{
  CustomPipelineShader shader;

  const BlendingState* blending_state = nullptr;
  const DepthState* depth_state = nullptr;
  const CullMode* cull_mode = nullptr;

  u32 additional_color_attachment_count = 0;

  VideoCommon::CustomAssetLibrary::AssetID id;
  std::string pixel_shader_id;
  std::string vertex_shader_id;
};

class CustomShaderCache2
{
public:
  CustomShaderCache2();
  ~CustomShaderCache2();
  CustomShaderCache2(const CustomShaderCache2&) = delete;
  CustomShaderCache2(CustomShaderCache2&&) = delete;
  CustomShaderCache2& operator=(const CustomShaderCache2&) = delete;
  CustomShaderCache2& operator=(CustomShaderCache2&&) = delete;

  void Initialize();
  void Shutdown();

  // Changes the shader host config. Shaders should be reloaded afterwards.
  void SetHostConfig(const ShaderHostConfig& host_config) { m_host_config.bits = host_config.bits; }

  // Whether geometry shaders are needed
  bool NeedsGeometryShader(const GeometryShaderUid& uid) const;

  // Retrieves all pending shaders/pipelines from the async compiler.
  void RetrieveAsyncShaders();

  // Reloads/recreates all shaders and pipelines.
  void Reload();

  // The optional will be empty if this pipeline is now background compiling.
  std::optional<const AbstractPipeline*> GetPipelineAsync(const VideoCommon::GXPipelineUid& uid,
                                                          CustomPipelineMaterial custom_material);

  using Resource = std::variant<std::unique_ptr<AbstractPipeline>, std::unique_ptr<AbstractShader>>;
  void TakePipelineResource(const VideoCommon::CustomAssetLibrary::AssetID& id,
                            std::list<Resource>* resources);
  void TakeVertexShaderResource(const std::string& id, std::list<Resource>* resources);
  void TakePixelShaderResource(const std::string& id, std::list<Resource>* resources);

private:
  // Configuration bits.
  APIType m_api_type = APIType::Nothing;
  ShaderHostConfig m_host_config = {};
  std::unique_ptr<VideoCommon::AsyncShaderCompiler> m_async_shader_compiler;

  void AsyncCreatePipeline(const VideoCommon::GXPipelineUid& uid,
                           CustomPipelineMaterial custom_material);

  // Shader/Pipeline cache helper
  template <typename Uid, typename ValueType>
  struct Cache
  {
    struct CacheHolder
    {
      std::unique_ptr<ValueType> value = nullptr;
      bool pending = true;
    };
    using CacheElement = std::pair<Uid, CacheHolder>;
    using CacheList = std::list<CacheElement>;
    std::map<VideoCommon::CustomAssetLibrary::AssetID, CacheList> asset_id_to_cachelist;

    const CacheHolder* GetHolder(const Uid& uid,
                                 const VideoCommon::CustomAssetLibrary::AssetID& asset_id) const
    {
      if (auto asset_it = asset_id_to_cachelist.find(asset_id);
          asset_it != asset_id_to_cachelist.end())
      {
        for (const auto& [stored_uid, holder] : asset_it->second)
        {
          if (uid == stored_uid)
          {
            return &holder;
          }
        }
      }

      return nullptr;
    }

    typename CacheList::iterator
    InsertElement(const Uid& uid, const VideoCommon::CustomAssetLibrary::AssetID& asset_id)
    {
      CacheList& cachelist = asset_id_to_cachelist[asset_id];
      CacheElement e{uid, CacheHolder{}};
      return cachelist.emplace(cachelist.begin(), std::move(e));
    }
  };

  Cache<GeometryShaderUid, AbstractShader> m_gs_cache;
  Cache<PixelShaderUid, AbstractShader> m_ps_cache;
  Cache<VertexShaderUid, AbstractShader> m_vs_cache;
  Cache<VideoCommon::GXPipelineUid, AbstractPipeline> m_pipeline_cache;

  using GeometryShaderIterator = Cache<GeometryShaderUid, AbstractShader>::CacheList::iterator;
  using PipelineIterator = Cache<VideoCommon::GXPipelineUid, AbstractPipeline>::CacheList::iterator;
  using PixelShaderIterator = Cache<PixelShaderUid, AbstractShader>::CacheList::iterator;
  using VertexShaderIterator = Cache<VertexShaderUid, AbstractShader>::CacheList::iterator;

  void NotifyPipelineFinished(PipelineIterator iterator,
                              std::unique_ptr<AbstractPipeline> pipeline);

  std::unique_ptr<AbstractShader> CompileGeometryShader(const GeometryShaderUid& uid) const;
  void NotifyGeometryShaderFinished(GeometryShaderIterator iterator,
                                    std::unique_ptr<AbstractShader> shader);
  void QueueGeometryShaderCompile(const GeometryShaderUid& uid,
                                  const CustomPipelineMaterial& custom_material);

  std::unique_ptr<AbstractShader> CompilePixelShader(const PixelShaderUid& uid,
                                                     std::string_view custom_fragment,
                                                     std::string_view custom_uniforms) const;
  void NotifyPixelShaderFinished(PixelShaderIterator iterator,
                                 std::unique_ptr<AbstractShader> shader);
  void QueuePixelShaderCompile(const PixelShaderUid& uid,
                               const CustomPipelineMaterial& custom_material);

  std::unique_ptr<AbstractShader> CompileVertexShader(const VertexShaderUid& uid,
                                                      std::string_view custom_vertex,
                                                      std::string_view custom_uniform) const;
  void NotifyVertexShaderFinished(VertexShaderIterator iterator,
                                  std::unique_ptr<AbstractShader> shader);
  void QueueVertexShaderCompile(const VertexShaderUid& uid,
                                const CustomPipelineMaterial& custom_material);

  Common::EventHook m_frame_end_handler;
};
