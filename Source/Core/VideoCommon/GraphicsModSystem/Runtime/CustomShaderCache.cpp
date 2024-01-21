// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomShaderCache.h"
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/VideoConfig.h"

CustomShaderCache::CustomShaderCache()
{
  m_api_type = g_ActiveConfig.backend_info.api_type;
  m_host_config.bits = ShaderHostConfig::GetCurrent().bits;

  m_async_shader_compiler = g_gfx->CreateAsyncShaderCompiler();
  m_async_shader_compiler->StartWorkerThreads(1);  // TODO

  m_async_uber_shader_compiler = g_gfx->CreateAsyncShaderCompiler();
  m_async_uber_shader_compiler->StartWorkerThreads(1);  // TODO

  m_frame_end_handler = AfterFrameEvent::Register([this](Core::System&) { RetrieveAsyncShaders(); },
                                                  "RetrieveAsyncShaders");

  m_mesh_cache.Initialize(true);
}

CustomShaderCache::~CustomShaderCache()
{
  if (m_async_shader_compiler)
    m_async_shader_compiler->StopWorkerThreads();

  if (m_async_uber_shader_compiler)
    m_async_uber_shader_compiler->StopWorkerThreads();

  m_mesh_cache.Shutdown();
}

void CustomShaderCache::RetrieveAsyncShaders()
{
  m_async_shader_compiler->RetrieveWorkItems();
  m_async_uber_shader_compiler->RetrieveWorkItems();

  m_mesh_cache.RetrieveAsyncShaders();
}

void CustomShaderCache::Reload()
{
  while (m_async_shader_compiler->HasPendingWork() || m_async_shader_compiler->HasCompletedWork())
  {
    m_async_shader_compiler->RetrieveWorkItems();
  }

  while (m_async_uber_shader_compiler->HasPendingWork() ||
         m_async_uber_shader_compiler->HasCompletedWork())
  {
    m_async_uber_shader_compiler->RetrieveWorkItems();
  }

  m_ps_cache = {};
  m_uber_ps_cache = {};
  m_pipeline_cache = {};
  m_uber_pipeline_cache = {};

  m_mesh_cache.Reload();
}

std::optional<const AbstractPipeline*>
CustomShaderCache::GetPipelineAsync(const VideoCommon::GXPipelineUid& uid,
                                    const CustomShaderInstance& custom_shaders,
                                    const AbstractPipelineConfig& pipeline_config)
{
  if (auto holder = m_pipeline_cache.GetHolder(uid, custom_shaders))
  {
    if (holder->pending)
      return std::nullopt;
    return holder->value.get();
  }
  AsyncCreatePipeline(uid, custom_shaders, pipeline_config);
  return std::nullopt;
}

std::optional<const AbstractPipeline*>
CustomShaderCache::GetPipelineAsync(const VideoCommon::GXUberPipelineUid& uid,
                                    const CustomShaderInstance& custom_shaders,
                                    const AbstractPipelineConfig& pipeline_config)
{
  if (auto holder = m_uber_pipeline_cache.GetHolder(uid, custom_shaders))
  {
    if (holder->pending)
      return std::nullopt;
    return holder->value.get();
  }
  AsyncCreatePipeline(uid, custom_shaders, pipeline_config);
  return std::nullopt;
}

const AbstractPipeline* CustomShaderCache::GetPipelineForUid(const VideoCommon::GXPipelineUid& uid)
{
  return m_mesh_cache.GetPipelineForUid(uid);
}

const AbstractPipeline*
CustomShaderCache::GetUberPipelineForUid(const VideoCommon::GXUberPipelineUid& uid)
{
  return m_mesh_cache.GetUberPipelineForUid(uid);
}

std::optional<const AbstractPipeline*>
CustomShaderCache::GetPipelineForUidAsync(const VideoCommon::GXPipelineUid& uid)
{
  return m_mesh_cache.GetPipelineForUidAsync(uid);
}

void CustomShaderCache::AsyncCreatePipeline(const VideoCommon::GXPipelineUid& uid,

                                            const CustomShaderInstance& custom_shaders,
                                            const AbstractPipelineConfig& pipeline_config)
{
  class PipelineWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PipelineWorkItem(CustomShaderCache* shader_cache, const VideoCommon::GXPipelineUid& uid,
                     const CustomShaderInstance& custom_shaders, PipelineIterator iterator,
                     const AbstractPipelineConfig& pipeline_config)
        : m_shader_cache(shader_cache), m_uid(uid), m_iterator(iterator), m_config(pipeline_config),
          m_custom_shaders(custom_shaders)
    {
      SetStagesReady();
    }

    void SetStagesReady()
    {
      m_stages_ready = true;

      PixelShaderUid ps_uid = m_uid.ps_uid;
      ClearUnusedPixelShaderUidBits(m_shader_cache->m_api_type, m_shader_cache->m_host_config,
                                    &ps_uid);

      if (auto holder = m_shader_cache->m_ps_cache.GetHolder(ps_uid, m_custom_shaders))
      {
        // If the pixel shader is no longer pending compilation
        // and the shader compilation succeeded, set
        // the pipeline to use the new pixel shader.
        // Otherwise, use the existing shader.
        if (!holder->pending && holder->value.get())
        {
          m_config.pixel_shader = holder->value.get();
        }
        m_stages_ready &= !holder->pending;
      }
      else
      {
        m_stages_ready &= false;
        m_shader_cache->QueuePixelShaderCompile(ps_uid, m_custom_shaders);
      }
    }

    bool Compile() override
    {
      if (m_stages_ready)
      {
        m_pipeline = g_gfx->CreatePipeline(m_config);
      }
      return true;
    }

    void Retrieve() override
    {
      if (m_stages_ready)
      {
        m_shader_cache->NotifyPipelineFinished(m_iterator, std::move(m_pipeline));
      }
      else
      {
        // Re-queue for next frame.
        auto wi = m_shader_cache->m_async_shader_compiler->CreateWorkItem<PipelineWorkItem>(
            m_shader_cache, m_uid, m_custom_shaders, m_iterator, m_config);
        m_shader_cache->m_async_shader_compiler->QueueWorkItem(std::move(wi), 0);
      }
    }

  private:
    CustomShaderCache* m_shader_cache;
    std::unique_ptr<AbstractPipeline> m_pipeline;
    VideoCommon::GXPipelineUid m_uid;
    PipelineIterator m_iterator;
    AbstractPipelineConfig m_config;
    CustomShaderInstance m_custom_shaders;
    bool m_stages_ready;
  };

  auto list_iter = m_pipeline_cache.InsertElement(uid, custom_shaders);
  auto work_item = m_async_shader_compiler->CreateWorkItem<PipelineWorkItem>(
      this, uid, custom_shaders, list_iter, pipeline_config);
  m_async_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

void CustomShaderCache::AsyncCreatePipeline(const VideoCommon::GXUberPipelineUid& uid,

                                            const CustomShaderInstance& custom_shaders,
                                            const AbstractPipelineConfig& pipeline_config)
{
  class PipelineWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PipelineWorkItem(CustomShaderCache* shader_cache, const VideoCommon::GXUberPipelineUid& uid,
                     const CustomShaderInstance& custom_shaders, UberPipelineIterator iterator,
                     const AbstractPipelineConfig& pipeline_config)
        : m_shader_cache(shader_cache), m_uid(uid), m_iterator(iterator), m_config(pipeline_config),
          m_custom_shaders(custom_shaders)
    {
      SetStagesReady();
    }

    void SetStagesReady()
    {
      m_stages_ready = true;

      UberShader::PixelShaderUid ps_uid = m_uid.ps_uid;
      ClearUnusedPixelShaderUidBits(m_shader_cache->m_api_type, m_shader_cache->m_host_config,
                                    &ps_uid);

      if (auto holder = m_shader_cache->m_uber_ps_cache.GetHolder(ps_uid, m_custom_shaders))
      {
        if (!holder->pending && holder->value.get())
        {
          m_config.pixel_shader = holder->value.get();
        }
        m_stages_ready &= !holder->pending;
      }
      else
      {
        m_stages_ready &= false;
        m_shader_cache->QueuePixelShaderCompile(ps_uid, m_custom_shaders);
      }
    }

    bool Compile() override
    {
      if (m_stages_ready)
      {
        if (m_config.pixel_shader == nullptr || m_config.vertex_shader == nullptr)
          return false;

        m_pipeline = g_gfx->CreatePipeline(m_config);
      }
      return true;
    }

    void Retrieve() override
    {
      if (m_stages_ready)
      {
        m_shader_cache->NotifyPipelineFinished(m_iterator, std::move(m_pipeline));
      }
      else
      {
        // Re-queue for next frame.
        auto wi = m_shader_cache->m_async_uber_shader_compiler->CreateWorkItem<PipelineWorkItem>(
            m_shader_cache, m_uid, m_custom_shaders, m_iterator, m_config);
        m_shader_cache->m_async_uber_shader_compiler->QueueWorkItem(std::move(wi), 0);
      }
    }

  private:
    CustomShaderCache* m_shader_cache;
    std::unique_ptr<AbstractPipeline> m_pipeline;
    VideoCommon::GXUberPipelineUid m_uid;
    UberPipelineIterator m_iterator;
    AbstractPipelineConfig m_config;
    CustomShaderInstance m_custom_shaders;
    bool m_stages_ready;
  };

  auto list_iter = m_uber_pipeline_cache.InsertElement(uid, custom_shaders);
  auto work_item = m_async_uber_shader_compiler->CreateWorkItem<PipelineWorkItem>(
      this, uid, custom_shaders, list_iter, pipeline_config);
  m_async_uber_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

void CustomShaderCache::NotifyPipelineFinished(PipelineIterator iterator,
                                               std::unique_ptr<AbstractPipeline> pipeline)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(pipeline);
}

void CustomShaderCache::NotifyPipelineFinished(UberPipelineIterator iterator,
                                               std::unique_ptr<AbstractPipeline> pipeline)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(pipeline);
}

void CustomShaderCache::QueuePixelShaderCompile(const PixelShaderUid& uid,

                                                const CustomShaderInstance& custom_shaders)
{
  class PixelShaderWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PixelShaderWorkItem(CustomShaderCache* shader_cache, const PixelShaderUid& uid,
                        const CustomShaderInstance& custom_shaders, PixelShaderIterator iter)
        : m_shader_cache(shader_cache), m_uid(uid), m_custom_shaders(custom_shaders), m_iter(iter)
    {
    }

    bool Compile() override
    {
      m_shader = m_shader_cache->CompilePixelShader(m_uid, m_custom_shaders);
      return true;
    }

    void Retrieve() override
    {
      m_shader_cache->NotifyPixelShaderFinished(m_iter, std::move(m_shader));
    }

  private:
    CustomShaderCache* m_shader_cache;
    std::unique_ptr<AbstractShader> m_shader;
    PixelShaderUid m_uid;
    CustomShaderInstance m_custom_shaders;
    PixelShaderIterator m_iter;
  };

  auto list_iter = m_ps_cache.InsertElement(uid, custom_shaders);
  auto work_item = m_async_shader_compiler->CreateWorkItem<PixelShaderWorkItem>(
      this, uid, custom_shaders, list_iter);
  m_async_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

void CustomShaderCache::QueuePixelShaderCompile(const UberShader::PixelShaderUid& uid,

                                                const CustomShaderInstance& custom_shaders)
{
  class PixelShaderWorkItem final : public VideoCommon::AsyncShaderCompiler::WorkItem
  {
  public:
    PixelShaderWorkItem(CustomShaderCache* shader_cache, const UberShader::PixelShaderUid& uid,
                        const CustomShaderInstance& custom_shaders, UberPixelShaderIterator iter)
        : m_shader_cache(shader_cache), m_uid(uid), m_custom_shaders(custom_shaders), m_iter(iter)
    {
    }

    bool Compile() override
    {
      m_shader = m_shader_cache->CompilePixelShader(m_uid, m_custom_shaders);
      return true;
    }

    void Retrieve() override
    {
      m_shader_cache->NotifyPixelShaderFinished(m_iter, std::move(m_shader));
    }

  private:
    CustomShaderCache* m_shader_cache;
    std::unique_ptr<AbstractShader> m_shader;
    UberShader::PixelShaderUid m_uid;
    CustomShaderInstance m_custom_shaders;
    UberPixelShaderIterator m_iter;
  };

  auto list_iter = m_uber_ps_cache.InsertElement(uid, custom_shaders);
  auto work_item = m_async_uber_shader_compiler->CreateWorkItem<PixelShaderWorkItem>(
      this, uid, custom_shaders, list_iter);
  m_async_uber_shader_compiler->QueueWorkItem(std::move(work_item), 0);
}

std::unique_ptr<AbstractShader>
CustomShaderCache::CompilePixelShader(const PixelShaderUid& uid,
                                      const CustomShaderInstance& custom_shaders) const
{
  const ShaderCode source_code = GeneratePixelShaderCode(
      m_api_type, m_host_config, uid.GetUidData(), custom_shaders.pixel_contents);
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(),
                                       "Custom Pixel Shader");
}

std::unique_ptr<AbstractShader>
CustomShaderCache::CompilePixelShader(const UberShader::PixelShaderUid& uid,
                                      const CustomShaderInstance& custom_shaders) const
{
  const ShaderCode source_code =
      GenPixelShader(m_api_type, m_host_config, uid.GetUidData(), custom_shaders.pixel_contents);
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(),
                                       "Custom Uber Pixel Shader");
}

void CustomShaderCache::NotifyPixelShaderFinished(PixelShaderIterator iterator,
                                                  std::unique_ptr<AbstractShader> shader)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(shader);
}

void CustomShaderCache::NotifyPixelShaderFinished(UberPixelShaderIterator iterator,
                                                  std::unique_ptr<AbstractShader> shader)
{
  iterator->second.pending = false;
  iterator->second.value = std::move(shader);
}
