// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/VideoEvents.h"

struct CustomShaderInstance
{
  CustomPixelShaderContents pixel_contents;

  bool operator==(const CustomShaderInstance& other) const = default;
};

class CustomShaderCache
{
public:
  CustomShaderCache();
  ~CustomShaderCache();
  CustomShaderCache(const CustomShaderCache&) = delete;
  CustomShaderCache(CustomShaderCache&&) = delete;
  CustomShaderCache& operator=(const CustomShaderCache&) = delete;
  CustomShaderCache& operator=(CustomShaderCache&&) = delete;

  // Changes the shader host config. Shaders should be reloaded afterwards.
  void SetHostConfig(const ShaderHostConfig& host_config) { m_host_config.bits = host_config.bits; }

  // Retrieves all pending shaders/pipelines from the async compiler.
  void RetrieveAsyncShaders();

  // Reloads/recreates all shaders and pipelines.
  void Reload();

  // The optional will be empty if this pipeline is now background compiling.
  std::optional<const AbstractPipeline*>
  GetPipelineAsync(const VideoCommon::GXPipelineUid& uid,
                   const CustomShaderInstance& custom_shaders,
                   const AbstractPipelineConfig& pipeline_config);
  std::optional<const AbstractPipeline*>
  GetPipelineAsync(const VideoCommon::GXUberPipelineUid& uid,
                   const CustomShaderInstance& custom_shaders,
                   const AbstractPipelineConfig& pipeline_config);

private:
  // Configuration bits.
  APIType m_api_type = APIType::Nothing;
  ShaderHostConfig m_host_config = {};
  std::unique_ptr<VideoCommon::AsyncShaderCompiler> m_async_shader_compiler;
  std::unique_ptr<VideoCommon::AsyncShaderCompiler> m_async_uber_shader_compiler;

  void AsyncCreatePipeline(const VideoCommon::GXPipelineUid& uid,
                           const CustomShaderInstance& custom_shaders,
                           const AbstractPipelineConfig& pipeline_config);
  void AsyncCreatePipeline(const VideoCommon::GXUberPipelineUid& uid,
                           const CustomShaderInstance& custom_shaders,
                           const AbstractPipelineConfig& pipeline_config);

  // Shader/Pipeline cache helper
  template <typename Uid, typename ValueType>
  struct Cache
  {
    struct CacheHolder
    {
      std::unique_ptr<ValueType> value = nullptr;
      bool pending = true;
    };
    using CacheElement = std::pair<CustomShaderInstance, CacheHolder>;
    using CacheList = std::list<CacheElement>;
    std::map<Uid, CacheList> uid_to_cachelist;

    const CacheHolder* GetHolder(const Uid& uid, const CustomShaderInstance& custom_shaders) const
    {
      if (auto uuid_it = uid_to_cachelist.find(uid); uuid_it != uid_to_cachelist.end())
      {
        for (const auto& [custom_shader_val, holder] : uuid_it->second)
        {
          if (custom_shaders == custom_shader_val)
          {
            return &holder;
          }
        }
      }

      return nullptr;
    }

    typename CacheList::iterator InsertElement(const Uid& uid,
                                               const CustomShaderInstance& custom_shaders)
    {
      CacheList& cachelist = uid_to_cachelist[uid];
      CacheElement e{custom_shaders, CacheHolder{}};
      return cachelist.emplace(cachelist.begin(), std::move(e));
    }
  };

  Cache<PixelShaderUid, AbstractShader> m_ps_cache;
  Cache<UberShader::PixelShaderUid, AbstractShader> m_uber_ps_cache;
  Cache<VideoCommon::GXPipelineUid, AbstractPipeline> m_pipeline_cache;
  Cache<VideoCommon::GXUberPipelineUid, AbstractPipeline> m_uber_pipeline_cache;

  using PipelineIterator = Cache<VideoCommon::GXPipelineUid, AbstractPipeline>::CacheList::iterator;
  using UberPipelineIterator =
      Cache<VideoCommon::GXUberPipelineUid, AbstractPipeline>::CacheList::iterator;
  using PixelShaderIterator = Cache<PixelShaderUid, AbstractShader>::CacheList::iterator;
  using UberPixelShaderIterator =
      Cache<UberShader::PixelShaderUid, AbstractShader>::CacheList::iterator;

  void NotifyPipelineFinished(PipelineIterator iterator,
                              std::unique_ptr<AbstractPipeline> pipeline);
  void NotifyPipelineFinished(UberPipelineIterator iterator,
                              std::unique_ptr<AbstractPipeline> pipeline);

  std::unique_ptr<AbstractShader>
  CompilePixelShader(const PixelShaderUid& uid, const CustomShaderInstance& custom_shaders) const;
  void NotifyPixelShaderFinished(PixelShaderIterator iterator,
                                 std::unique_ptr<AbstractShader> shader);
  std::unique_ptr<AbstractShader>
  CompilePixelShader(const UberShader::PixelShaderUid& uid,
                     const CustomShaderInstance& custom_shaders) const;
  void NotifyPixelShaderFinished(UberPixelShaderIterator iterator,
                                 std::unique_ptr<AbstractShader> shader);

  void QueuePixelShaderCompile(const PixelShaderUid& uid,
                               const CustomShaderInstance& custom_shaders);
  void QueuePixelShaderCompile(const UberShader::PixelShaderUid& uid,
                               const CustomShaderInstance& custom_shaders);

  Common::EventHook m_frame_end_handler;
};
