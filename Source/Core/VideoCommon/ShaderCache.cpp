// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/ShaderCache.h"

#include <fmt/format.h>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/FramebufferShaderGen.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

#include <imgui.h>

std::unique_ptr<VideoCommon::ShaderCache> g_shader_cache;

namespace VideoCommon
{
ShaderCache::ShaderCache() : m_api_type{APIType::Nothing}
{
}

ShaderCache::~ShaderCache()
{
  ClearCaches();
}

bool ShaderCache::Initialize()
{
  m_api_type = g_ActiveConfig.backend_info.api_type;
  m_host_config.bits = ShaderHostConfig::GetCurrent().bits;

  if (!CompileSharedPipelines())
    return false;

  m_async_shader_compiler = g_gfx->CreateAsyncShaderCompiler();
  m_frame_end_handler = AfterFrameEvent::Register([this](Core::System&) { RetrieveAsyncShaders(); },
                                                  "RetrieveAsyncShaders");
  return true;
}

void ShaderCache::InitializeShaderCache()
{
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderPrecompilerThreads());

  // Load shader and UID caches.
  if (g_ActiveConfig.bShaderCache && m_api_type != APIType::Nothing)
  {
    LoadCaches();
    LoadPipelineUIDCache();
  }

  // Queue ubershader precompiling if required.
  if (g_ActiveConfig.UsingUberShaders())
    QueueUberShaderPipelines();

  // Compile all known UIDs.
  CompileMissingPipelines();
  if (g_ActiveConfig.bWaitForShadersBeforeStarting)
    WaitForAsyncCompiler();

  // Switch to the runtime shader compiler thread configuration.
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderCompilerThreads());
}

void ShaderCache::Reload()
{
  WaitForAsyncCompiler();
  ClosePipelineUIDCache();
  ClearCaches();

  if (!CompileSharedPipelines())
    PanicAlertFmt("Failed to compile shared pipelines after reload.");

  if (g_ActiveConfig.bShaderCache)
    LoadCaches();

  // Switch to the precompiling shader configuration while we rebuild.
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderPrecompilerThreads());

  // We don't need to explicitly recompile the individual ubershaders here, as the pipelines
  // UIDs are still be in the map. Therefore, when these are rebuilt, the shaders will also
  // be recompiled.
  CompileMissingPipelines();
  if (g_ActiveConfig.bWaitForShadersBeforeStarting)
    WaitForAsyncCompiler();
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderCompilerThreads());
}

void ShaderCache::RetrieveAsyncShaders()
{
  m_async_shader_compiler->RetrieveWorkItems();
}

void ShaderCache::Shutdown()
{
  // This may leave shaders uncommitted to the cache, but it's better than blocking shutdown
  // until everything has finished compiling.
  if (m_async_shader_compiler)
    m_async_shader_compiler->StopWorkerThreads();

  ClosePipelineUIDCache();
}

const AbstractPipeline* ShaderCache::GetPipelineForUid(const GXPipelineUid& uid)
{
  auto it = m_gx_pipeline_cache.find(uid);
  if (it != m_gx_pipeline_cache.end() && !it->second.second)
    return it->second.first.get();

  const bool exists_in_cache = it != m_gx_pipeline_cache.end();
  std::unique_ptr<AbstractPipeline> pipeline;
  std::optional<AbstractPipelineConfig> pipeline_config = GetGXPipelineConfig(uid);
  if (pipeline_config)
    pipeline = g_gfx->CreatePipeline(*pipeline_config);
  if (g_ActiveConfig.bShaderCache && !exists_in_cache)
    AppendGXPipelineUID(uid);
  return InsertGXPipeline(uid, std::move(pipeline));
}

std::optional<const AbstractPipeline*> ShaderCache::GetPipelineForUidAsync(const GXPipelineUid& uid)
{
  auto it = m_gx_pipeline_cache.find(uid);
  if (it != m_gx_pipeline_cache.end())
  {
    // .second is the pending flag, i.e. compiling in the background.
    if (!it->second.second)
      return it->second.first.get();
    else
      return {};
  }

  AppendGXPipelineUID(uid);
  QueuePipelineCompile(uid, COMPILE_PRIORITY_ONDEMAND_PIPELINE);
  return {};
}

const AbstractPipeline* ShaderCache::GetUberPipelineForUid(const GXUberPipelineUid& uid)
{
  auto it = m_gx_uber_pipeline_cache.find(uid);
  if (it != m_gx_uber_pipeline_cache.end() && !it->second.second)
    return it->second.first.get();

  std::unique_ptr<AbstractPipeline> pipeline;
  std::optional<AbstractPipelineConfig> pipeline_config = GetGXPipelineConfig(uid);
  if (pipeline_config)
    pipeline = g_gfx->CreatePipeline(*pipeline_config);
  return InsertGXUberPipeline(uid, std::move(pipeline));
}

void ShaderCache::WaitForAsyncCompiler()
{
  bool running = true;

  constexpr auto update_ui_progress = [](size_t completed, size_t total) {
    const float center_x = ImGui::GetIO().DisplaySize.x * 0.5f;
    const float center_y = ImGui::GetIO().DisplaySize.y * 0.5f;
    const float scale = ImGui::GetIO().DisplayFramebufferScale.x;

    ImGui::SetNextWindowSize(ImVec2(400.0f * scale, 50.0f * scale), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(center_x, center_y), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin(Common::GetStringT("Compiling Shaders").c_str(), nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
    {
      ImGui::Text("Compiling shaders: %zu/%zu", completed, total);
      ImGui::ProgressBar(static_cast<float>(completed) /
                             static_cast<float>(std::max(total, static_cast<size_t>(1))),
                         ImVec2(-1.0f, 0.0f), "");
    }
    ImGui::End();

    g_presenter->Present();
  };

  while (running &&
         (m_async_shader_compiler->HasPendingWork() || m_async_shader_compiler->HasCompletedWork()))
  {
    running = m_async_shader_compiler->WaitUntilCompletion(update_ui_progress);

    m_async_shader_compiler->RetrieveWorkItems();
  }

  // An extra Present to clear the screen
  g_presenter->Present();
}

template <typename SerializedUidType, typename UidType>
static void SerializePipelineUid(const UidType& uid, SerializedUidType& serialized_uid)
{
  // Convert to disk format. Ensure all padding bytes are zero.
  std::memset(reinterpret_cast<u8*>(&serialized_uid), 0, sizeof(serialized_uid));
  serialized_uid.vertex_decl = uid.vertex_format->GetVertexDeclaration();
  serialized_uid.vs_uid = uid.vs_uid;
  serialized_uid.gs_uid = uid.gs_uid;
  serialized_uid.ps_uid = uid.ps_uid;
  serialized_uid.rasterization_state_bits = uid.rasterization_state.hex;
  serialized_uid.depth_state_bits = uid.depth_state.hex;
  serialized_uid.blending_state_bits = uid.blending_state.hex;
}

template <typename UidType, typename SerializedUidType>
static void UnserializePipelineUid(const SerializedUidType& uid, UidType& real_uid)
{
  real_uid.vertex_format = VertexLoaderManager::GetOrCreateMatchingFormat(uid.vertex_decl);
  real_uid.vs_uid = uid.vs_uid;
  real_uid.gs_uid = uid.gs_uid;
  real_uid.ps_uid = uid.ps_uid;
  real_uid.rasterization_state.hex = uid.rasterization_state_bits;
  real_uid.depth_state.hex = uid.depth_state_bits;
  real_uid.blending_state.hex = uid.blending_state_bits;
}

template <ShaderStage stage, typename K, typename T>
void ShaderCache::LoadShaderCache(T& cache, APIType api_type, const char* type, bool include_gameid)
{
  class CacheReader : public Common::LinearDiskCacheReader<K, u8>
  {
  public:
    CacheReader(T& cache_) : cache(cache_) {}
    void Read(const K& key, const u8* value, u32 value_size) override
    {
      auto shader = g_gfx->CreateShaderFromBinary(stage, value, value_size);
      if (shader)
      {
        auto& entry = cache.shader_map[key];
        entry.shader = std::move(shader);
        entry.pending = false;

        switch (stage)
        {
        case ShaderStage::Vertex:
          INCSTAT(g_stats.num_vertex_shaders_created);
          INCSTAT(g_stats.num_vertex_shaders_alive);
          break;
        case ShaderStage::Pixel:
          INCSTAT(g_stats.num_pixel_shaders_created);
          INCSTAT(g_stats.num_pixel_shaders_alive);
          break;
        default:
          break;
        }
      }
    }

  private:
    T& cache;
  };

  std::string filename = GetDiskShaderCacheFileName(api_type, type, include_gameid, true);
  CacheReader reader(cache);
  u32 count = cache.disk_cache.OpenAndRead(filename, reader);
  INFO_LOG_FMT(VIDEO, "Loaded {} cached shaders from {}", count, filename);
}

template <typename T>
void ShaderCache::ClearShaderCache(T& cache)
{
  cache.disk_cache.Sync();
  cache.disk_cache.Close();
  cache.shader_map.clear();
}

template <typename KeyType, typename DiskKeyType, typename T>
void ShaderCache::LoadPipelineCache(T& cache, Common::LinearDiskCache<DiskKeyType, u8>& disk_cache,
                                    APIType api_type, const char* type, bool include_gameid)
{
  class CacheReader : public Common::LinearDiskCacheReader<DiskKeyType, u8>
  {
  public:
    CacheReader(ShaderCache* this_ptr_, T& cache_) : this_ptr(this_ptr_), cache(cache_) {}
    bool AnyFailed() const { return failed; }
    void Read(const DiskKeyType& key, const u8* value, u32 value_size) override
    {
      KeyType real_uid;
      UnserializePipelineUid(key, real_uid);

      // Skip those which are already compiled.
      if (failed || cache.find(real_uid) != cache.end())
        return;

      auto config = this_ptr->GetGXPipelineConfig(real_uid);
      if (!config)
        return;

      auto pipeline = g_gfx->CreatePipeline(*config, value, value_size);
      if (!pipeline)
      {
        // If any of the pipelines fail to create, consider the cache stale.
        failed = true;
        return;
      }

      auto& entry = cache[real_uid];
      entry.first = std::move(pipeline);
      entry.second = false;
    }

  private:
    ShaderCache* this_ptr;
    T& cache;
    bool failed = false;
  };

  std::string filename = GetDiskShaderCacheFileName(api_type, type, include_gameid, true);
  CacheReader reader(this, cache);
  const u32 count = disk_cache.OpenAndRead(filename, reader);
  INFO_LOG_FMT(VIDEO, "Loaded {} cached pipelines from {}", count, filename);

  // If any of the pipelines in the cache failed to create, it's likely because of a change of
  // driver version, or system configuration. In this case, when the UID cache picks up the pipeline
  // later on, we'll write a duplicate entry to the pipeline cache. There's also no point in keeping
  // the old cache data around, so discard and recreate the disk cache.
  if (reader.AnyFailed())
  {
    WARN_LOG_FMT(VIDEO, "Failed to load one or more pipelines from cache '{}'. Discarding.",
                 filename);
    disk_cache.Close();
    File::Delete(filename);
    disk_cache.OpenAndRead(filename, reader);
  }
}

template <typename T, typename Y>
void ShaderCache::ClearPipelineCache(T& cache, Y& disk_cache)
{
  disk_cache.Sync();
  disk_cache.Close();

  // Set the pending flag to false, and destroy the pipeline.
  for (auto& it : cache)
  {
    it.second.first.reset();
    it.second.second = false;
  }
}

void ShaderCache::LoadCaches()
{
  // Ubershader caches, if present.
  if (g_ActiveConfig.backend_info.bSupportsShaderBinaries)
  {
    LoadShaderCache<ShaderStage::Vertex, UberShader::VertexShaderUid>(m_uber_vs_cache, m_api_type,
                                                                      "uber-vs", false);
    LoadShaderCache<ShaderStage::Pixel, UberShader::PixelShaderUid>(m_uber_ps_cache, m_api_type,
                                                                    "uber-ps", false);

    // We also share geometry shaders, as there aren't many variants.
    if (m_host_config.backend_geometry_shaders)
      LoadShaderCache<ShaderStage::Geometry, GeometryShaderUid>(m_gs_cache, m_api_type, "gs",
                                                                false);

    // Specialized shaders, gameid-specific.
    LoadShaderCache<ShaderStage::Vertex, VertexShaderUid>(m_vs_cache, m_api_type, "specialized-vs",
                                                          true);
    LoadShaderCache<ShaderStage::Pixel, PixelShaderUid>(m_ps_cache, m_api_type, "specialized-ps",
                                                        true);
  }

  if (g_ActiveConfig.backend_info.bSupportsPipelineCacheData)
  {
    LoadPipelineCache<GXPipelineUid, SerializedGXPipelineUid>(
        m_gx_pipeline_cache, m_gx_pipeline_disk_cache, m_api_type, "specialized-pipeline", true);
    LoadPipelineCache<GXUberPipelineUid, SerializedGXUberPipelineUid>(
        m_gx_uber_pipeline_cache, m_gx_uber_pipeline_disk_cache, m_api_type, "uber-pipeline",
        false);
  }
}

void ShaderCache::ClearCaches()
{
  ClearPipelineCache(m_gx_pipeline_cache, m_gx_pipeline_disk_cache);
  ClearShaderCache(m_vs_cache);
  ClearShaderCache(m_gs_cache);
  ClearShaderCache(m_ps_cache);

  ClearPipelineCache(m_gx_uber_pipeline_cache, m_gx_uber_pipeline_disk_cache);
  ClearShaderCache(m_uber_vs_cache);
  ClearShaderCache(m_uber_ps_cache);

  m_screen_quad_vertex_shader.reset();
  m_texture_copy_vertex_shader.reset();
  m_efb_copy_vertex_shader.reset();
  m_texcoord_geometry_shader.reset();
  m_color_geometry_shader.reset();
  m_texture_copy_pixel_shader.reset();
  m_color_pixel_shader.reset();

  m_efb_copy_to_vram_pipelines.clear();
  m_efb_copy_to_ram_pipelines.clear();
  m_copy_rgba8_pipeline.reset();
  m_rgba8_stereo_copy_pipeline.reset();
  for (auto& pipeline : m_palette_conversion_pipelines)
    pipeline.reset();
  m_texture_reinterpret_pipelines.clear();
  m_texture_decoding_shaders.clear();

  SETSTAT(g_stats.num_pixel_shaders_created, 0);
  SETSTAT(g_stats.num_pixel_shaders_alive, 0);
  SETSTAT(g_stats.num_vertex_shaders_created, 0);
  SETSTAT(g_stats.num_vertex_shaders_alive, 0);
}

void ShaderCache::CompileMissingPipelines()
{
  // Queue all uids with a null pipeline for compilation.
  for (auto& it : m_gx_pipeline_cache)
  {
    if (!it.second.first)
      QueuePipelineCompile(it.first, COMPILE_PRIORITY_SHADERCACHE_PIPELINE);
  }
  for (auto& it : m_gx_uber_pipeline_cache)
  {
    if (!it.second.first)
      QueueUberPipelineCompile(it.first, COMPILE_PRIORITY_UBERSHADER_PIPELINE);
  }
}

std::unique_ptr<AbstractShader> ShaderCache::CompileVertexShader(const VertexShaderUid& uid) const
{
  const ShaderCode source_code =
      GenerateVertexShaderCode(m_api_type, m_host_config, uid.GetUidData());
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer());
}

std::unique_ptr<AbstractShader>
ShaderCache::CompileVertexUberShader(const UberShader::VertexShaderUid& uid) const
{
  const ShaderCode source_code =
      UberShader::GenVertexShader(m_api_type, m_host_config, uid.GetUidData());
  return g_gfx->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer(),
                                       fmt::to_string(*uid.GetUidData()));
}

std::unique_ptr<AbstractShader> ShaderCache::CompilePixelShader(const PixelShaderUid& uid) const
{
  const ShaderCode source_code =
      GeneratePixelShaderCode(m_api_type, m_host_config, uid.GetUidData(), {});
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer());
}

std::unique_ptr<AbstractShader>
ShaderCache::CompilePixelUberShader(const UberShader::PixelShaderUid& uid) const
{
  const ShaderCode source_code =
      UberShader::GenPixelShader(m_api_type, m_host_config, uid.GetUidData(), {});
  return g_gfx->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer(),
                                       fmt::to_string(*uid.GetUidData()));
}

const AbstractShader* ShaderCache::InsertVertexShader(const VertexShaderUid& uid,
                                                      std::unique_ptr<AbstractShader> shader)
{
  auto& entry = m_vs_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && g_ActiveConfig.backend_info.bSupportsShaderBinaries)
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_vs_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(g_stats.num_vertex_shaders_created);
    INCSTAT(g_stats.num_vertex_shaders_alive);
    entry.shader = std::move(shader);
  }

  return entry.shader.get();
}

const AbstractShader* ShaderCache::InsertVertexUberShader(const UberShader::VertexShaderUid& uid,
                                                          std::unique_ptr<AbstractShader> shader)
{
  auto& entry = m_uber_vs_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && g_ActiveConfig.backend_info.bSupportsShaderBinaries)
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_uber_vs_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(g_stats.num_vertex_shaders_created);
    INCSTAT(g_stats.num_vertex_shaders_alive);
    entry.shader = std::move(shader);
  }

  return entry.shader.get();
}

const AbstractShader* ShaderCache::InsertPixelShader(const PixelShaderUid& uid,
                                                     std::unique_ptr<AbstractShader> shader)
{
  auto& entry = m_ps_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && g_ActiveConfig.backend_info.bSupportsShaderBinaries)
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_ps_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(g_stats.num_pixel_shaders_created);
    INCSTAT(g_stats.num_pixel_shaders_alive);
    entry.shader = std::move(shader);
  }

  return entry.shader.get();
}

const AbstractShader* ShaderCache::InsertPixelUberShader(const UberShader::PixelShaderUid& uid,
                                                         std::unique_ptr<AbstractShader> shader)
{
  auto& entry = m_uber_ps_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && g_ActiveConfig.backend_info.bSupportsShaderBinaries)
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_uber_ps_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(g_stats.num_pixel_shaders_created);
    INCSTAT(g_stats.num_pixel_shaders_alive);
    entry.shader = std::move(shader);
  }

  return entry.shader.get();
}

const AbstractShader* ShaderCache::CreateGeometryShader(const GeometryShaderUid& uid)
{
  const ShaderCode source_code =
      GenerateGeometryShaderCode(m_api_type, m_host_config, uid.GetUidData());
  std::unique_ptr<AbstractShader> shader =
      g_gfx->CreateShaderFromSource(ShaderStage::Geometry, source_code.GetBuffer(),
                                    fmt::format("Geometry shader: {}", *uid.GetUidData()));

  auto& entry = m_gs_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && g_ActiveConfig.backend_info.bSupportsShaderBinaries)
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_gs_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    entry.shader = std::move(shader);
  }

  return entry.shader.get();
}

bool ShaderCache::NeedsGeometryShader(const GeometryShaderUid& uid) const
{
  return m_host_config.backend_geometry_shaders && !uid.GetUidData()->IsPassthrough();
}

bool ShaderCache::UseGeometryShaderForEFBCopies() const
{
  return m_host_config.backend_geometry_shaders && m_host_config.stereo;
}

AbstractPipelineConfig ShaderCache::GetGXPipelineConfig(
    const NativeVertexFormat* vertex_format, const AbstractShader* vertex_shader,
    const AbstractShader* geometry_shader, const AbstractShader* pixel_shader,
    const RasterizationState& rasterization_state, const DepthState& depth_state,
    const BlendingState& blending_state, AbstractPipelineUsage usage)
{
  AbstractPipelineConfig config = {};
  config.usage = usage;
  config.vertex_format = vertex_format;
  config.vertex_shader = vertex_shader;
  config.geometry_shader = geometry_shader;
  config.pixel_shader = pixel_shader;
  config.rasterization_state = rasterization_state;
  config.depth_state = depth_state;
  config.blending_state = blending_state;
  config.framebuffer_state = g_framebuffer_manager->GetEFBFramebufferState();
  return config;
}

/// Edits the UID based on driver bugs and other special configurations
static GXPipelineUid ApplyDriverBugs(const GXPipelineUid& in)
{
  GXPipelineUid out;
  // TODO: static_assert(std::is_trivially_copyable_v<GXPipelineUid>);
  // GXPipelineUid is not trivially copyable because RasterizationState and BlendingState aren't
  // either, but we can pretend it is for now. This will be improved after PR #10848 is finished.
  memcpy(static_cast<void*>(&out), static_cast<const void*>(&in), sizeof(out));  // copy padding
  pixel_shader_uid_data* ps = out.ps_uid.GetUidData();
  BlendingState& blend = out.blending_state;

  if (ps->ztest == EmulatedZ::ForcedEarly && !out.depth_state.updateenable)
  {
    // No need to force early depth test if you're not writing z
    ps->ztest = EmulatedZ::Early;
  }

  // If framebuffer fetch is available, we can emulate logic ops in the fragment shader
  // and don't need the below blend approximation
  if (blend.logicopenable && !g_ActiveConfig.backend_info.bSupportsLogicOp &&
      !g_ActiveConfig.backend_info.bSupportsFramebufferFetch)
  {
    if (!blend.LogicOpApproximationIsExact())
      WARN_LOG_FMT(VIDEO,
                   "Approximating logic op with blending, this will produce incorrect rendering.");
    if (blend.LogicOpApproximationWantsShaderHelp())
    {
      ps->emulate_logic_op_with_blend = true;
      ps->logic_op_mode = static_cast<u32>(blend.logicmode.Value());
    }
    blend.ApproximateLogicOpWithBlending();
  }

  const bool benefits_from_ps_dual_source_off =
      (!g_ActiveConfig.backend_info.bSupportsDualSourceBlend &&
       g_ActiveConfig.backend_info.bSupportsFramebufferFetch) ||
      DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DUAL_SOURCE_BLENDING);
  if (benefits_from_ps_dual_source_off && !blend.RequiresDualSrc())
  {
    // Only use dual-source blending when required on drivers that don't support it very well.
    ps->no_dual_src = true;
    blend.usedualsrc = false;
  }

  if (g_ActiveConfig.backend_info.bSupportsFramebufferFetch)
  {
    bool fbfetch_blend = false;
    if ((DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DISCARD_WITH_EARLY_Z) ||
         !g_ActiveConfig.backend_info.bSupportsEarlyZ) &&
        ps->ztest == EmulatedZ::ForcedEarly)
    {
      ps->ztest = EmulatedZ::EarlyWithFBFetch;
      fbfetch_blend |= static_cast<bool>(out.blending_state.blendenable);
      ps->no_dual_src = true;
    }
    fbfetch_blend |= blend.logicopenable && !g_ActiveConfig.backend_info.bSupportsLogicOp;
    fbfetch_blend |= blend.usedualsrc && !g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
    if (fbfetch_blend)
    {
      ps->no_dual_src = true;
      if (blend.logicopenable)
      {
        ps->logic_op_enable = true;
        ps->logic_op_mode = static_cast<u32>(blend.logicmode.Value());
        blend.logicopenable = false;
      }
      if (blend.blendenable)
      {
        ps->blend_enable = true;
        ps->blend_src_factor = blend.srcfactor;
        ps->blend_src_factor_alpha = blend.srcfactoralpha;
        ps->blend_dst_factor = blend.dstfactor;
        ps->blend_dst_factor_alpha = blend.dstfactoralpha;
        ps->blend_subtract = blend.subtract;
        ps->blend_subtract_alpha = blend.subtractAlpha;
        blend.blendenable = false;
      }
    }
  }

  // force dual src off if we can't support it
  if (!g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
  {
    ps->no_dual_src = true;
    blend.usedualsrc = false;
  }

  if (ps->ztest == EmulatedZ::ForcedEarly && !g_ActiveConfig.backend_info.bSupportsEarlyZ)
  {
    // These things should be false
    ASSERT(!ps->zfreeze);
    // ZCOMPLOC HACK:
    // The only way to emulate alpha test + early-z is to force early-z in the shader.
    // As this isn't available on all drivers and as we can't emulate this feature otherwise,
    // we are only able to choose which one we want to respect more.
    // Tests seem to have proven that writing depth even when the alpha test fails is more
    // important that a reliable alpha test, so we just force the alpha test to always succeed.
    // At least this seems to be less buggy.
    ps->ztest = EmulatedZ::EarlyWithZComplocHack;
  }

  if (g_ActiveConfig.UseVSForLinePointExpand() &&
      (out.rasterization_state.primitive == PrimitiveType::Points ||
       out.rasterization_state.primitive == PrimitiveType::Lines))
  {
    // All primitives are expanded to triangles in the vertex shader
    vertex_shader_uid_data* vs = out.vs_uid.GetUidData();
    const PortableVertexDeclaration& decl = out.vertex_format->GetVertexDeclaration();
    vs->position_has_3_elems = decl.position.components >= 3;
    vs->texcoord_elem_count = 0;
    for (int i = 0; i < 8; i++)
    {
      if (decl.texcoords[i].enable)
      {
        ASSERT(decl.texcoords[i].components <= 3);
        vs->texcoord_elem_count |= decl.texcoords[i].components << (i * 2);
      }
    }
    out.vertex_format = nullptr;
    if (out.rasterization_state.primitive == PrimitiveType::Points)
      vs->vs_expand = VSExpand::Point;
    else
      vs->vs_expand = VSExpand::Line;
    PrimitiveType prim = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ?
                             PrimitiveType::TriangleStrip :
                             PrimitiveType::Triangles;
    out.rasterization_state.primitive = prim;
    out.gs_uid.GetUidData()->primitive_type = static_cast<u32>(prim);
  }

  return out;
}

std::optional<AbstractPipelineConfig>
ShaderCache::GetGXPipelineConfig(const GXPipelineUid& config_in)
{
  GXPipelineUid config = ApplyDriverBugs(config_in);
  const AbstractShader* vs;
  auto vs_iter = m_vs_cache.shader_map.find(config.vs_uid);
  if (vs_iter != m_vs_cache.shader_map.end() && !vs_iter->second.pending)
    vs = vs_iter->second.shader.get();
  else
    vs = InsertVertexShader(config.vs_uid, CompileVertexShader(config.vs_uid));

  PixelShaderUid ps_uid = config.ps_uid;
  ClearUnusedPixelShaderUidBits(m_api_type, m_host_config, &ps_uid);

  const AbstractShader* ps;
  auto ps_iter = m_ps_cache.shader_map.find(ps_uid);
  if (ps_iter != m_ps_cache.shader_map.end() && !ps_iter->second.pending)
    ps = ps_iter->second.shader.get();
  else
    ps = InsertPixelShader(ps_uid, CompilePixelShader(ps_uid));

  if (!vs || !ps)
    return {};

  const AbstractShader* gs = nullptr;
  if (NeedsGeometryShader(config.gs_uid))
  {
    auto gs_iter = m_gs_cache.shader_map.find(config.gs_uid);
    if (gs_iter != m_gs_cache.shader_map.end() && !gs_iter->second.pending)
      gs = gs_iter->second.shader.get();
    else
      gs = CreateGeometryShader(config.gs_uid);
    if (!gs)
      return {};
  }

  return GetGXPipelineConfig(config.vertex_format, vs, gs, ps, config.rasterization_state,
                             config.depth_state, config.blending_state, AbstractPipelineUsage::GX);
}

/// Edits the UID based on driver bugs and other special configurations
static GXUberPipelineUid ApplyDriverBugs(const GXUberPipelineUid& in)
{
  GXUberPipelineUid out;
  // TODO: static_assert(std::is_trivially_copyable_v<GXUberPipelineUid>);
  // GXUberPipelineUid is not trivially copyable because RasterizationState and BlendingState aren't
  // either, but we can pretend it is for now. This will be improved after PR #10848 is finished.
  memcpy(static_cast<void*>(&out), static_cast<const void*>(&in), sizeof(out));  // Copy padding
  if (g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader)
    out.vertex_format = nullptr;

  // If framebuffer fetch is available, we can emulate logic ops in the fragment shader
  // and don't need the below blend approximation
  if (out.blending_state.logicopenable && !g_ActiveConfig.backend_info.bSupportsLogicOp &&
      !g_ActiveConfig.backend_info.bSupportsFramebufferFetch)
  {
    if (!out.blending_state.LogicOpApproximationIsExact())
      WARN_LOG_FMT(VIDEO,
                   "Approximating logic op with blending, this will produce incorrect rendering.");
    out.blending_state.ApproximateLogicOpWithBlending();
  }

  if (g_ActiveConfig.backend_info.bSupportsFramebufferFetch)
  {
    // Always blend in shader
    out.blending_state.hex = 0;
    out.blending_state.colorupdate = in.blending_state.colorupdate.Value();
    out.blending_state.alphaupdate = in.blending_state.alphaupdate.Value();
    out.ps_uid.GetUidData()->no_dual_src = true;
  }
  else if (!g_ActiveConfig.backend_info.bSupportsDualSourceBlend ||
           (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DUAL_SOURCE_BLENDING) &&
            !out.blending_state.RequiresDualSrc()))
  {
    out.blending_state.usedualsrc = false;
    out.ps_uid.GetUidData()->no_dual_src = true;
  }

  if (g_ActiveConfig.UseVSForLinePointExpand())
  {
    // All primitives are expanded to triangles in the vertex shader
    PrimitiveType prim = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ?
                             PrimitiveType::TriangleStrip :
                             PrimitiveType::Triangles;
    out.rasterization_state.primitive = prim;
    out.gs_uid.GetUidData()->primitive_type = static_cast<u32>(prim);
  }

  return out;
}

std::optional<AbstractPipelineConfig>
ShaderCache::GetGXPipelineConfig(const GXUberPipelineUid& config_in)
{
  GXUberPipelineUid config = ApplyDriverBugs(config_in);
  const AbstractShader* vs;
  auto vs_iter = m_uber_vs_cache.shader_map.find(config.vs_uid);
  if (vs_iter != m_uber_vs_cache.shader_map.end() && !vs_iter->second.pending)
    vs = vs_iter->second.shader.get();
  else
    vs = InsertVertexUberShader(config.vs_uid, CompileVertexUberShader(config.vs_uid));

  UberShader::PixelShaderUid ps_uid = config.ps_uid;
  UberShader::ClearUnusedPixelShaderUidBits(m_api_type, m_host_config, &ps_uid);

  const AbstractShader* ps;
  auto ps_iter = m_uber_ps_cache.shader_map.find(ps_uid);
  if (ps_iter != m_uber_ps_cache.shader_map.end() && !ps_iter->second.pending)
    ps = ps_iter->second.shader.get();
  else
    ps = InsertPixelUberShader(ps_uid, CompilePixelUberShader(ps_uid));

  if (!vs || !ps)
    return {};

  const AbstractShader* gs = nullptr;
  if (NeedsGeometryShader(config.gs_uid))
  {
    auto gs_iter = m_gs_cache.shader_map.find(config.gs_uid);
    if (gs_iter != m_gs_cache.shader_map.end() && !gs_iter->second.pending)
      gs = gs_iter->second.shader.get();
    else
      gs = CreateGeometryShader(config.gs_uid);
    if (!gs)
      return {};
  }

  return GetGXPipelineConfig(config.vertex_format, vs, gs, ps, config.rasterization_state,
                             config.depth_state, config.blending_state,
                             AbstractPipelineUsage::GXUber);
}

const AbstractPipeline* ShaderCache::InsertGXPipeline(const GXPipelineUid& config,
                                                      std::unique_ptr<AbstractPipeline> pipeline)
{
  auto& entry = m_gx_pipeline_cache[config];
  entry.second = false;
  if (!entry.first && pipeline)
  {
    entry.first = std::move(pipeline);

    if (g_ActiveConfig.bShaderCache)
    {
      auto cache_data = entry.first->GetCacheData();
      if (!cache_data.empty())
      {
        SerializedGXPipelineUid disk_uid;
        SerializePipelineUid(config, disk_uid);
        m_gx_pipeline_disk_cache.Append(disk_uid, cache_data.data(),
                                        static_cast<u32>(cache_data.size()));
      }
    }
  }

  return entry.first.get();
}

const AbstractPipeline*
ShaderCache::InsertGXUberPipeline(const GXUberPipelineUid& config,
                                  std::unique_ptr<AbstractPipeline> pipeline)
{
  auto& entry = m_gx_uber_pipeline_cache[config];
  entry.second = false;
  if (!entry.first && pipeline)
  {
    entry.first = std::move(pipeline);

    if (g_ActiveConfig.bShaderCache)
    {
      auto cache_data = entry.first->GetCacheData();
      if (!cache_data.empty())
      {
        SerializedGXUberPipelineUid disk_uid;
        SerializePipelineUid(config, disk_uid);
        m_gx_uber_pipeline_disk_cache.Append(disk_uid, cache_data.data(),
                                             static_cast<u32>(cache_data.size()));
      }
    }
  }

  return entry.first.get();
}

void ShaderCache::LoadPipelineUIDCache()
{
  constexpr u32 CACHE_FILE_MAGIC = 0x44495550;  // PUID
  constexpr size_t CACHE_HEADER_SIZE = sizeof(u32) + sizeof(u32);
  std::string filename =
      File::GetUserPath(D_CACHE_IDX) + SConfig::GetInstance().GetGameID() + ".uidcache";
  if (m_gx_pipeline_uid_cache_file.Open(filename, "rb+"))
  {
    // If an existing case exists, validate the version before reading entries.
    u32 existing_magic;
    u32 existing_version;
    bool uid_file_valid = false;
    if (m_gx_pipeline_uid_cache_file.ReadBytes(&existing_magic, sizeof(existing_magic)) &&
        m_gx_pipeline_uid_cache_file.ReadBytes(&existing_version, sizeof(existing_version)) &&
        existing_magic == CACHE_FILE_MAGIC && existing_version == GX_PIPELINE_UID_VERSION)
    {
      // Ensure the expected size matches the actual size of the file. If it doesn't, it means
      // the cache file may be corrupted, and we should not proceed with loading potentially
      // garbage or invalid UIDs.
      const u64 file_size = m_gx_pipeline_uid_cache_file.GetSize();
      const size_t uid_count =
          static_cast<size_t>(file_size - CACHE_HEADER_SIZE) / sizeof(SerializedGXPipelineUid);
      const size_t expected_size = uid_count * sizeof(SerializedGXPipelineUid) + CACHE_HEADER_SIZE;
      uid_file_valid = file_size == expected_size;
      if (uid_file_valid)
      {
        for (size_t i = 0; i < uid_count; i++)
        {
          SerializedGXPipelineUid serialized_uid;
          if (m_gx_pipeline_uid_cache_file.ReadBytes(&serialized_uid, sizeof(serialized_uid)))
          {
            // This just adds the pipeline to the map, it is compiled later.
            AddSerializedGXPipelineUID(serialized_uid);
          }
          else
          {
            uid_file_valid = false;
            break;
          }
        }
      }

      // We open the file for reading and writing, so we must seek to the end before writing.
      if (uid_file_valid)
        uid_file_valid = m_gx_pipeline_uid_cache_file.Seek(expected_size, File::SeekOrigin::Begin);
    }

    // If the file is invalid, close it. We re-open and truncate it below.
    if (!uid_file_valid)
      m_gx_pipeline_uid_cache_file.Close();
  }

  // If the file is not open, it means it was either corrupted or didn't exist.
  if (!m_gx_pipeline_uid_cache_file.IsOpen())
  {
    if (m_gx_pipeline_uid_cache_file.Open(filename, "wb"))
    {
      // Write the version identifier.
      m_gx_pipeline_uid_cache_file.WriteBytes(&CACHE_FILE_MAGIC, sizeof(GX_PIPELINE_UID_VERSION));
      m_gx_pipeline_uid_cache_file.WriteBytes(&GX_PIPELINE_UID_VERSION,
                                              sizeof(GX_PIPELINE_UID_VERSION));

      // Write any current UIDs out to the file.
      // This way, if we load a UID cache where the data was incomplete (e.g. Dolphin crashed),
      // we don't lose the existing UIDs which were previously at the beginning.
      for (const auto& it : m_gx_pipeline_cache)
        AppendGXPipelineUID(it.first);
    }
  }

  INFO_LOG_FMT(VIDEO, "Read {} pipeline UIDs from {}", m_gx_pipeline_cache.size(), filename);
}

void ShaderCache::ClosePipelineUIDCache()
{
  // This is left as a method in case we need to append extra data to the file in the future.
  m_gx_pipeline_uid_cache_file.Close();
}

void ShaderCache::AddSerializedGXPipelineUID(const SerializedGXPipelineUid& uid)
{
  GXPipelineUid real_uid;
  UnserializePipelineUid(uid, real_uid);

  auto iter = m_gx_pipeline_cache.find(real_uid);
  if (iter != m_gx_pipeline_cache.end())
    return;

  // Flag it as empty with a null pipeline object, for later compilation.
  auto& entry = m_gx_pipeline_cache[real_uid];
  entry.second = false;
}

void ShaderCache::AppendGXPipelineUID(const GXPipelineUid& config)
{
  if (!m_gx_pipeline_uid_cache_file.IsOpen())
    return;

  SerializedGXPipelineUid disk_uid;
  SerializePipelineUid(config, disk_uid);
  if (!m_gx_pipeline_uid_cache_file.WriteBytes(&disk_uid, sizeof(disk_uid)))
  {
    WARN_LOG_FMT(VIDEO, "Writing pipeline UID to cache failed, closing file.");
    m_gx_pipeline_uid_cache_file.Close();
  }
}

void ShaderCache::QueueVertexShaderCompile(const VertexShaderUid& uid, u32 priority)
{
  class VertexShaderWorkItem final : public AsyncShaderCompiler::WorkItem
  {
  public:
    VertexShaderWorkItem(ShaderCache* shader_cache_, const VertexShaderUid& uid_)
        : shader_cache(shader_cache_), uid(uid_)
    {
    }

    bool Compile() override
    {
      shader = shader_cache->CompileVertexShader(uid);
      return true;
    }

    void Retrieve() override { shader_cache->InsertVertexShader(uid, std::move(shader)); }

  private:
    ShaderCache* shader_cache;
    std::unique_ptr<AbstractShader> shader;
    VertexShaderUid uid;
  };

  m_vs_cache.shader_map[uid].pending = true;
  auto wi = m_async_shader_compiler->CreateWorkItem<VertexShaderWorkItem>(this, uid);
  m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
}

void ShaderCache::QueueVertexUberShaderCompile(const UberShader::VertexShaderUid& uid, u32 priority)
{
  class VertexUberShaderWorkItem final : public AsyncShaderCompiler::WorkItem
  {
  public:
    VertexUberShaderWorkItem(ShaderCache* shader_cache_, const UberShader::VertexShaderUid& uid_)
        : shader_cache(shader_cache_), uid(uid_)
    {
    }

    bool Compile() override
    {
      shader = shader_cache->CompileVertexUberShader(uid);
      return true;
    }

    void Retrieve() override { shader_cache->InsertVertexUberShader(uid, std::move(shader)); }

  private:
    ShaderCache* shader_cache;
    std::unique_ptr<AbstractShader> shader;
    UberShader::VertexShaderUid uid;
  };

  m_uber_vs_cache.shader_map[uid].pending = true;
  auto wi = m_async_shader_compiler->CreateWorkItem<VertexUberShaderWorkItem>(this, uid);
  m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
}

void ShaderCache::QueuePixelShaderCompile(const PixelShaderUid& uid, u32 priority)
{
  class PixelShaderWorkItem final : public AsyncShaderCompiler::WorkItem
  {
  public:
    PixelShaderWorkItem(ShaderCache* shader_cache_, const PixelShaderUid& uid_)
        : shader_cache(shader_cache_), uid(uid_)
    {
    }

    bool Compile() override
    {
      shader = shader_cache->CompilePixelShader(uid);
      return true;
    }

    void Retrieve() override { shader_cache->InsertPixelShader(uid, std::move(shader)); }

  private:
    ShaderCache* shader_cache;
    std::unique_ptr<AbstractShader> shader;
    PixelShaderUid uid;
  };

  m_ps_cache.shader_map[uid].pending = true;
  auto wi = m_async_shader_compiler->CreateWorkItem<PixelShaderWorkItem>(this, uid);
  m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
}

void ShaderCache::QueuePixelUberShaderCompile(const UberShader::PixelShaderUid& uid, u32 priority)
{
  class PixelUberShaderWorkItem final : public AsyncShaderCompiler::WorkItem
  {
  public:
    PixelUberShaderWorkItem(ShaderCache* shader_cache_, const UberShader::PixelShaderUid& uid_)
        : shader_cache(shader_cache_), uid(uid_)
    {
    }

    bool Compile() override
    {
      shader = shader_cache->CompilePixelUberShader(uid);
      return true;
    }

    void Retrieve() override { shader_cache->InsertPixelUberShader(uid, std::move(shader)); }

  private:
    ShaderCache* shader_cache;
    std::unique_ptr<AbstractShader> shader;
    UberShader::PixelShaderUid uid;
  };

  m_uber_ps_cache.shader_map[uid].pending = true;
  auto wi = m_async_shader_compiler->CreateWorkItem<PixelUberShaderWorkItem>(this, uid);
  m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
}

void ShaderCache::QueuePipelineCompile(const GXPipelineUid& uid, u32 priority)
{
  class PipelineWorkItem final : public AsyncShaderCompiler::WorkItem
  {
  public:
    PipelineWorkItem(ShaderCache* shader_cache_, const GXPipelineUid& uid_, u32 priority_)
        : shader_cache(shader_cache_), uid(uid_), priority(priority_)
    {
      // Check if all the stages required for this pipeline have been compiled.
      // If not, this work item becomes a no-op, and re-queues the pipeline for the next frame.
      if (SetStagesReady())
        config = shader_cache->GetGXPipelineConfig(uid);
    }

    bool SetStagesReady()
    {
      stages_ready = true;

      GXPipelineUid actual_uid = ApplyDriverBugs(uid);

      auto vs_it = shader_cache->m_vs_cache.shader_map.find(actual_uid.vs_uid);
      stages_ready &= vs_it != shader_cache->m_vs_cache.shader_map.end() && !vs_it->second.pending;
      if (vs_it == shader_cache->m_vs_cache.shader_map.end())
        shader_cache->QueueVertexShaderCompile(actual_uid.vs_uid, priority);

      PixelShaderUid ps_uid = actual_uid.ps_uid;
      ClearUnusedPixelShaderUidBits(shader_cache->m_api_type, shader_cache->m_host_config, &ps_uid);

      auto ps_it = shader_cache->m_ps_cache.shader_map.find(ps_uid);
      stages_ready &= ps_it != shader_cache->m_ps_cache.shader_map.end() && !ps_it->second.pending;
      if (ps_it == shader_cache->m_ps_cache.shader_map.end())
        shader_cache->QueuePixelShaderCompile(ps_uid, priority);

      return stages_ready;
    }

    bool Compile() override
    {
      if (config)
        pipeline = g_gfx->CreatePipeline(*config);
      return true;
    }

    void Retrieve() override
    {
      if (stages_ready)
      {
        shader_cache->InsertGXPipeline(uid, std::move(pipeline));
      }
      else
      {
        // Re-queue for next frame.
        auto wi = shader_cache->m_async_shader_compiler->CreateWorkItem<PipelineWorkItem>(
            shader_cache, uid, priority);
        shader_cache->m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
      }
    }

  private:
    ShaderCache* shader_cache;
    std::unique_ptr<AbstractPipeline> pipeline;
    GXPipelineUid uid;
    u32 priority;
    std::optional<AbstractPipelineConfig> config;
    bool stages_ready;
  };

  auto wi = m_async_shader_compiler->CreateWorkItem<PipelineWorkItem>(this, uid, priority);
  m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
  m_gx_pipeline_cache[uid].second = true;
}

void ShaderCache::QueueUberPipelineCompile(const GXUberPipelineUid& uid, u32 priority)
{
  class UberPipelineWorkItem final : public AsyncShaderCompiler::WorkItem
  {
  public:
    UberPipelineWorkItem(ShaderCache* shader_cache_, const GXUberPipelineUid& uid_, u32 priority_)
        : shader_cache(shader_cache_), uid(uid_), priority(priority_)
    {
      // Check if all the stages required for this UberPipeline have been compiled.
      // If not, this work item becomes a no-op, and re-queues the UberPipeline for the next frame.
      if (SetStagesReady())
        config = shader_cache->GetGXPipelineConfig(uid);
    }

    bool SetStagesReady()
    {
      stages_ready = true;

      GXUberPipelineUid actual_uid = ApplyDriverBugs(uid);

      auto vs_it = shader_cache->m_uber_vs_cache.shader_map.find(actual_uid.vs_uid);
      stages_ready &=
          vs_it != shader_cache->m_uber_vs_cache.shader_map.end() && !vs_it->second.pending;
      if (vs_it == shader_cache->m_uber_vs_cache.shader_map.end())
        shader_cache->QueueVertexUberShaderCompile(actual_uid.vs_uid, priority);

      UberShader::PixelShaderUid ps_uid = actual_uid.ps_uid;
      UberShader::ClearUnusedPixelShaderUidBits(shader_cache->m_api_type,
                                                shader_cache->m_host_config, &ps_uid);

      auto ps_it = shader_cache->m_uber_ps_cache.shader_map.find(ps_uid);
      stages_ready &=
          ps_it != shader_cache->m_uber_ps_cache.shader_map.end() && !ps_it->second.pending;
      if (ps_it == shader_cache->m_uber_ps_cache.shader_map.end())
        shader_cache->QueuePixelUberShaderCompile(ps_uid, priority);

      return stages_ready;
    }

    bool Compile() override
    {
      if (config)
        UberPipeline = g_gfx->CreatePipeline(*config);
      return true;
    }

    void Retrieve() override
    {
      if (stages_ready)
      {
        shader_cache->InsertGXUberPipeline(uid, std::move(UberPipeline));
      }
      else
      {
        // Re-queue for next frame.
        auto wi = shader_cache->m_async_shader_compiler->CreateWorkItem<UberPipelineWorkItem>(
            shader_cache, uid, priority);
        shader_cache->m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
      }
    }

  private:
    ShaderCache* shader_cache;
    std::unique_ptr<AbstractPipeline> UberPipeline;
    GXUberPipelineUid uid;
    u32 priority;
    std::optional<AbstractPipelineConfig> config;
    bool stages_ready;
  };

  auto wi = m_async_shader_compiler->CreateWorkItem<UberPipelineWorkItem>(this, uid, priority);
  m_async_shader_compiler->QueueWorkItem(std::move(wi), priority);
  m_gx_uber_pipeline_cache[uid].second = true;
}

void ShaderCache::QueueUberShaderPipelines()
{
  // Create a dummy vertex format with no attributes.
  // All attributes will be enabled in GetUberVertexFormat.
  PortableVertexDeclaration dummy_vertex_decl = {};
  dummy_vertex_decl.position.components = 4;
  dummy_vertex_decl.position.type = ComponentFormat::Float;
  dummy_vertex_decl.position.enable = true;
  dummy_vertex_decl.stride = sizeof(float) * 4;
  NativeVertexFormat* dummy_vertex_format =
      VertexLoaderManager::GetUberVertexFormat(dummy_vertex_decl);
  auto QueueDummyPipeline =
      [&](const UberShader::VertexShaderUid& vs_uid, const GeometryShaderUid& gs_uid,
          const UberShader::PixelShaderUid& ps_uid, const BlendingState& blend) {
        GXUberPipelineUid config;
        config.vertex_format = dummy_vertex_format;
        config.vs_uid = vs_uid;
        config.gs_uid = gs_uid;
        config.ps_uid = ps_uid;
        config.rasterization_state = RenderState::GetCullBackFaceRasterizationState(
            static_cast<PrimitiveType>(gs_uid.GetUidData()->primitive_type));
        config.depth_state = RenderState::GetNoDepthTestingDepthState();
        config.blending_state = blend;
        if (ps_uid.GetUidData()->uint_output)
        {
          // uint_output is only ever enabled when logic ops are enabled.
          config.blending_state.logicopenable = true;
          config.blending_state.logicmode = LogicOp::And;
        }

        auto iter = m_gx_uber_pipeline_cache.find(config);
        if (iter != m_gx_uber_pipeline_cache.end())
          return;

        auto& entry = m_gx_uber_pipeline_cache[config];
        entry.second = false;
      };

  // Populate the pipeline configs with empty entries, these will be compiled afterwards.
  UberShader::EnumerateVertexShaderUids([&](const UberShader::VertexShaderUid& vuid) {
    UberShader::EnumeratePixelShaderUids([&](const UberShader::PixelShaderUid& puid) {
      // UIDs must have compatible texgens, a mismatching combination will never be queried.
      if (vuid.GetUidData()->num_texgens != puid.GetUidData()->num_texgens)
        return;

      UberShader::PixelShaderUid cleared_puid = puid;
      UberShader::ClearUnusedPixelShaderUidBits(m_api_type, m_host_config, &cleared_puid);
      EnumerateGeometryShaderUids([&](const GeometryShaderUid& guid) {
        if (guid.GetUidData()->numTexGens != vuid.GetUidData()->num_texgens ||
            (!guid.GetUidData()->IsPassthrough() && !m_host_config.backend_geometry_shaders))
        {
          return;
        }
        BlendingState blend = RenderState::GetNoBlendingBlendState();
        QueueDummyPipeline(vuid, guid, cleared_puid, blend);
        if (g_ActiveConfig.backend_info.bSupportsDynamicVertexLoader)
        {
          // Not all GPUs need all the pipeline state compiled into shaders, so they tend to key
          // compiled shaders based on some subset of the pipeline state.
          // Some test results:
          //   (GPUs tested: AMD Radeon Pro 5600M, Nvidia GT 750M, Intel UHD 630,
          //    Intel Iris Pro 5200, Apple M1)
          // MacOS Metal:
          //  - AMD, Nvidia, Intel GPUs: Shaders are keyed on vertex layout and whether or not
          //    dual source blend is enabled.  That's it.
          //  - Apple GPUs: Shaders are keyed on vertex layout and all blending settings.  We use
          //    framebuffer fetch here, so the only blending settings used by ubershaders are the
          //    alphaupdate and colorupdate ones.  Also keyed on primitive type, but Metal supports
          //    setting it to "unknown" and we do for ubershaders (but MoltenVK won't).
          // Windows Vulkan:
          //  - AMD, Nvidia: Definitely keyed on dual source blend, but the others seem more random
          //    Changing a setting on one shader will require a recompile, but changing the same
          //    setting on another won't.  Compiling a copy with alphaupdate off, colorupdate off,
          //    and one with DSB on seems to get pretty good coverage though.
          // Windows D3D12:
          //  - AMD: Keyed on dual source blend and vertex layout
          //  - Nvidia Kepler: No recompiles for changes to vertex layout or blend
          blend.alphaupdate = false;
          QueueDummyPipeline(vuid, guid, cleared_puid, blend);
          blend.alphaupdate = true;
          blend.colorupdate = false;
          QueueDummyPipeline(vuid, guid, cleared_puid, blend);
          blend.colorupdate = true;
          if (!cleared_puid.GetUidData()->no_dual_src && !cleared_puid.GetUidData()->uint_output)
          {
            blend.blendenable = true;
            blend.usedualsrc = true;
            blend.srcfactor = SrcBlendFactor::SrcAlpha;
            blend.dstfactor = DstBlendFactor::InvSrcAlpha;
            QueueDummyPipeline(vuid, guid, cleared_puid, blend);
          }
        }
      });
    });
  });
}

const AbstractPipeline*
ShaderCache::GetEFBCopyToVRAMPipeline(const TextureConversionShaderGen::TCShaderUid& uid)
{
  auto iter = m_efb_copy_to_vram_pipelines.find(uid);
  if (iter != m_efb_copy_to_vram_pipelines.end())
    return iter->second.get();

  auto shader_code = TextureConversionShaderGen::GeneratePixelShader(m_api_type, uid.GetUidData());
  auto shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, shader_code.GetBuffer(),
      fmt::format("EFB copy to VRAM pixel shader: {}", *uid.GetUidData()));
  if (!shader)
  {
    m_efb_copy_to_vram_pipelines.emplace(uid, nullptr);
    return nullptr;
  }

  AbstractPipelineConfig config = {};
  config.vertex_format = nullptr;
  config.vertex_shader = m_efb_copy_vertex_shader.get();
  config.geometry_shader =
      UseGeometryShaderForEFBCopies() ? m_texcoord_geometry_shader.get() : nullptr;
  config.pixel_shader = shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetRGBA8FramebufferState();
  config.usage = AbstractPipelineUsage::Utility;
  auto iiter = m_efb_copy_to_vram_pipelines.emplace(uid, g_gfx->CreatePipeline(config));
  return iiter.first->second.get();
}

const AbstractPipeline* ShaderCache::GetEFBCopyToRAMPipeline(const EFBCopyParams& uid)
{
  auto iter = m_efb_copy_to_ram_pipelines.find(uid);
  if (iter != m_efb_copy_to_ram_pipelines.end())
    return iter->second.get();

  const std::string shader_code =
      TextureConversionShaderTiled::GenerateEncodingShader(uid, m_api_type);
  const auto shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, shader_code, fmt::format("EFB copy to RAM pixel shader: {}", uid));
  if (!shader)
  {
    m_efb_copy_to_ram_pipelines.emplace(uid, nullptr);
    return nullptr;
  }

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_screen_quad_vertex_shader.get();
  config.pixel_shader = shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(AbstractTextureFormat::BGRA8);
  config.usage = AbstractPipelineUsage::Utility;
  auto iiter = m_efb_copy_to_ram_pipelines.emplace(uid, g_gfx->CreatePipeline(config));
  return iiter.first->second.get();
}

bool ShaderCache::CompileSharedPipelines()
{
  m_screen_quad_vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateScreenQuadVertexShader(),
      "Screen quad vertex shader");
  m_texture_copy_vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, FramebufferShaderGen::GenerateTextureCopyVertexShader(),
      "Texture copy vertex shader");
  m_efb_copy_vertex_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Vertex, TextureConversionShaderGen::GenerateVertexShader(m_api_type).GetBuffer(),
      "EFB copy vertex shader");
  if (!m_screen_quad_vertex_shader || !m_texture_copy_vertex_shader || !m_efb_copy_vertex_shader)
    return false;

  if (UseGeometryShaderForEFBCopies())
  {
    m_texcoord_geometry_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Geometry, FramebufferShaderGen::GeneratePassthroughGeometryShader(1, 0),
        "Texcoord passthrough geometry shader");
    m_color_geometry_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Geometry, FramebufferShaderGen::GeneratePassthroughGeometryShader(0, 1),
        "Color passthrough geometry shader");
    if (!m_texcoord_geometry_shader || !m_color_geometry_shader)
      return false;
  }

  m_texture_copy_pixel_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateTextureCopyPixelShader(),
      "Texture copy pixel shader");
  m_color_pixel_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, FramebufferShaderGen::GenerateColorPixelShader(), "Color pixel shader");
  if (!m_texture_copy_pixel_shader || !m_color_pixel_shader)
    return false;

  AbstractPipelineConfig config;
  config.vertex_format = nullptr;
  config.vertex_shader = m_texture_copy_vertex_shader.get();
  config.geometry_shader = nullptr;
  config.pixel_shader = m_texture_copy_pixel_shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetRGBA8FramebufferState();
  config.usage = AbstractPipelineUsage::Utility;
  m_copy_rgba8_pipeline = g_gfx->CreatePipeline(config);
  if (!m_copy_rgba8_pipeline)
    return false;

  if (UseGeometryShaderForEFBCopies())
  {
    config.geometry_shader = m_texcoord_geometry_shader.get();
    m_rgba8_stereo_copy_pipeline = g_gfx->CreatePipeline(config);
    if (!m_rgba8_stereo_copy_pipeline)
      return false;
  }

  if (m_host_config.backend_palette_conversion)
  {
    config.vertex_shader = m_screen_quad_vertex_shader.get();
    config.geometry_shader = nullptr;

    for (size_t i = 0; i < NUM_PALETTE_CONVERSION_SHADERS; i++)
    {
      TLUTFormat format = static_cast<TLUTFormat>(i);
      auto shader = g_gfx->CreateShaderFromSource(
          ShaderStage::Pixel,
          TextureConversionShaderTiled::GeneratePaletteConversionShader(format, m_api_type),
          fmt::format("Palette conversion pixel shader: {}", format));
      if (!shader)
        return false;

      config.pixel_shader = shader.get();
      m_palette_conversion_pipelines[i] = g_gfx->CreatePipeline(config);
      if (!m_palette_conversion_pipelines[i])
        return false;
    }
  }

  return true;
}

const AbstractPipeline* ShaderCache::GetPaletteConversionPipeline(TLUTFormat format)
{
  ASSERT(static_cast<size_t>(format) < NUM_PALETTE_CONVERSION_SHADERS);
  return m_palette_conversion_pipelines[static_cast<size_t>(format)].get();
}

const AbstractPipeline* ShaderCache::GetTextureReinterpretPipeline(TextureFormat from_format,
                                                                   TextureFormat to_format)
{
  const auto key = std::make_pair(from_format, to_format);
  auto iter = m_texture_reinterpret_pipelines.find(key);
  if (iter != m_texture_reinterpret_pipelines.end())
    return iter->second.get();

  std::string shader_source =
      FramebufferShaderGen::GenerateTextureReinterpretShader(from_format, to_format);
  if (shader_source.empty())
  {
    m_texture_reinterpret_pipelines.emplace(key, nullptr);
    return nullptr;
  }

  std::unique_ptr<AbstractShader> shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, shader_source,
      fmt::format("Texture reinterpret pixel shader: {} to {}", from_format, to_format));
  if (!shader)
  {
    m_texture_reinterpret_pipelines.emplace(key, nullptr);
    return nullptr;
  }

  AbstractPipelineConfig config;
  config.vertex_format = nullptr;
  config.vertex_shader = m_screen_quad_vertex_shader.get();
  config.geometry_shader = nullptr;
  config.pixel_shader = shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetRGBA8FramebufferState();
  config.usage = AbstractPipelineUsage::Utility;
  auto iiter = m_texture_reinterpret_pipelines.emplace(key, g_gfx->CreatePipeline(config));
  return iiter.first->second.get();
}

const AbstractShader*
ShaderCache::GetTextureDecodingShader(TextureFormat format,
                                      std::optional<TLUTFormat> palette_format)
{
  const auto key = std::make_pair(static_cast<u32>(format),
                                  static_cast<u32>(palette_format.value_or(TLUTFormat::IA8)));
  const auto iter = m_texture_decoding_shaders.find(key);
  if (iter != m_texture_decoding_shaders.end())
    return iter->second.get();

  const std::string shader_source =
      TextureConversionShaderTiled::GenerateDecodingShader(format, palette_format, APIType::OpenGL);
  if (shader_source.empty())
  {
    m_texture_decoding_shaders.emplace(key, nullptr);
    return nullptr;
  }

  const std::string name =
      palette_format.has_value() ?
          fmt::format("Texture decoding compute shader: {}, {}", format, *palette_format) :
          fmt::format("Texture decoding compute shader: {}", format);

  std::unique_ptr<AbstractShader> shader =
      g_gfx->CreateShaderFromSource(ShaderStage::Compute, shader_source, name);
  if (!shader)
  {
    m_texture_decoding_shaders.emplace(key, nullptr);
    return nullptr;
  }

  const auto iiter = m_texture_decoding_shaders.emplace(key, std::move(shader));
  return iiter.first->second.get();
}
}  // namespace VideoCommon
