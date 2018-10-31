// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/ShaderCache.h"

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"

std::unique_ptr<VideoCommon::ShaderCache> g_shader_cache;

namespace VideoCommon
{
ShaderCache::ShaderCache() = default;
ShaderCache::~ShaderCache() = default;

bool ShaderCache::Initialize()
{
  m_api_type = g_ActiveConfig.backend_info.api_type;
  m_host_config = ShaderHostConfig::GetCurrent();
  m_efb_depth_format = FramebufferManagerBase::GetEFBDepthFormat();
  m_efb_multisamples = g_ActiveConfig.iMultisamples;

  // Create the async compiler, and start the worker threads.
  m_async_shader_compiler = g_renderer->CreateAsyncShaderCompiler();
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderPrecompilerThreads());

  // Load shader and UID caches.
  if (g_ActiveConfig.bShaderCache && m_api_type != APIType::Nothing)
  {
    LoadShaderCaches();
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
  return true;
}

void ShaderCache::SetHostConfig(const ShaderHostConfig& host_config, u32 efb_multisamples)
{
  if (m_host_config.bits == host_config.bits && m_efb_multisamples == efb_multisamples)
    return;

  m_host_config = host_config;
  m_efb_multisamples = efb_multisamples;
  Reload();
}

void ShaderCache::Reload()
{
  WaitForAsyncCompiler();
  ClosePipelineUIDCache();
  InvalidateCachedPipelines();
  ClearShaderCaches();

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCaches();

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
  m_async_shader_compiler->StopWorkerThreads();
  ClosePipelineUIDCache();
  ClearShaderCaches();
  ClearPipelineCaches();
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
    pipeline = g_renderer->CreatePipeline(*pipeline_config);
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
  std::optional<AbstractPipelineConfig> pipeline_config = GetGXUberPipelineConfig(uid);
  if (pipeline_config)
    pipeline = g_renderer->CreatePipeline(*pipeline_config);
  return InsertGXUberPipeline(uid, std::move(pipeline));
}

void ShaderCache::WaitForAsyncCompiler()
{
  while (m_async_shader_compiler->HasPendingWork() || m_async_shader_compiler->HasCompletedWork())
  {
    m_async_shader_compiler->WaitUntilCompletion([](size_t completed, size_t total) {
      Host_UpdateProgressDialog(GetStringT("Compiling shaders...").c_str(),
                                static_cast<int>(completed), static_cast<int>(total));
    });
    m_async_shader_compiler->RetrieveWorkItems();
  }
  Host_UpdateProgressDialog("", -1, -1);
}

template <ShaderStage stage, typename K, typename T>
static void LoadShaderCache(T& cache, APIType api_type, const char* type, bool include_gameid)
{
  class CacheReader : public LinearDiskCacheReader<K, u8>
  {
  public:
    CacheReader(T& cache_) : cache(cache_) {}
    void Read(const K& key, const u8* value, u32 value_size)
    {
      auto shader = g_renderer->CreateShaderFromBinary(stage, value, value_size);
      if (shader)
      {
        auto& entry = cache.shader_map[key];
        entry.shader = std::move(shader);
        entry.pending = false;

        switch (stage)
        {
        case ShaderStage::Vertex:
          INCSTAT(stats.numVertexShadersCreated);
          INCSTAT(stats.numVertexShadersAlive);
          break;
        case ShaderStage::Pixel:
          INCSTAT(stats.numPixelShadersCreated);
          INCSTAT(stats.numPixelShadersAlive);
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
  INFO_LOG(VIDEO, "Loaded %u cached shaders from %s", count, filename.c_str());
}

template <typename T>
static void ClearShaderCache(T& cache)
{
  cache.disk_cache.Sync();
  cache.disk_cache.Close();
  cache.shader_map.clear();
}

void ShaderCache::LoadShaderCaches()
{
  // Ubershader caches, if present.
  LoadShaderCache<ShaderStage::Vertex, UberShader::VertexShaderUid>(m_uber_vs_cache, m_api_type,
                                                                    "uber-vs", false);
  LoadShaderCache<ShaderStage::Pixel, UberShader::PixelShaderUid>(m_uber_ps_cache, m_api_type,
                                                                  "uber-ps", false);

  // We also share geometry shaders, as there aren't many variants.
  if (m_host_config.backend_geometry_shaders)
    LoadShaderCache<ShaderStage::Geometry, GeometryShaderUid>(m_gs_cache, m_api_type, "gs", false);

  // Specialized shaders, gameid-specific.
  LoadShaderCache<ShaderStage::Vertex, VertexShaderUid>(m_vs_cache, m_api_type, "specialized-vs",
                                                        true);
  LoadShaderCache<ShaderStage::Pixel, PixelShaderUid>(m_ps_cache, m_api_type, "specialized-ps",
                                                      true);
}

void ShaderCache::ClearShaderCaches()
{
  ClearShaderCache(m_vs_cache);
  ClearShaderCache(m_gs_cache);
  ClearShaderCache(m_ps_cache);

  ClearShaderCache(m_uber_vs_cache);
  ClearShaderCache(m_uber_ps_cache);

  SETSTAT(stats.numPixelShadersCreated, 0);
  SETSTAT(stats.numPixelShadersAlive, 0);
  SETSTAT(stats.numVertexShadersCreated, 0);
  SETSTAT(stats.numVertexShadersAlive, 0);
}

void ShaderCache::CompileMissingPipelines()
{
  // Queue all uids with a null pipeline for compilation.
  for (auto& it : m_gx_pipeline_cache)
  {
    if (!it.second.second)
      QueuePipelineCompile(it.first, COMPILE_PRIORITY_SHADERCACHE_PIPELINE);
  }
  for (auto& it : m_gx_uber_pipeline_cache)
  {
    if (!it.second.second)
      QueueUberPipelineCompile(it.first, COMPILE_PRIORITY_UBERSHADER_PIPELINE);
  }
}

void ShaderCache::InvalidateCachedPipelines()
{
  // Set the pending flag to false, and destroy the pipeline.
  for (auto& it : m_gx_pipeline_cache)
  {
    it.second.first.reset();
    it.second.second = false;
  }
  for (auto& it : m_gx_uber_pipeline_cache)
  {
    it.second.first.reset();
    it.second.second = false;
  }
}

void ShaderCache::ClearPipelineCaches()
{
  m_gx_pipeline_cache.clear();
  m_gx_uber_pipeline_cache.clear();
}

std::unique_ptr<AbstractShader> ShaderCache::CompileVertexShader(const VertexShaderUid& uid) const
{
  ShaderCode source_code = GenerateVertexShaderCode(m_api_type, m_host_config, uid.GetUidData());
  return g_renderer->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().size());
}

std::unique_ptr<AbstractShader>
ShaderCache::CompileVertexUberShader(const UberShader::VertexShaderUid& uid) const
{
  ShaderCode source_code = UberShader::GenVertexShader(m_api_type, m_host_config, uid.GetUidData());
  return g_renderer->CreateShaderFromSource(ShaderStage::Vertex, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().size());
}

std::unique_ptr<AbstractShader> ShaderCache::CompilePixelShader(const PixelShaderUid& uid) const
{
  ShaderCode source_code = GeneratePixelShaderCode(m_api_type, m_host_config, uid.GetUidData());
  return g_renderer->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().size());
}

std::unique_ptr<AbstractShader>
ShaderCache::CompilePixelUberShader(const UberShader::PixelShaderUid& uid) const
{
  ShaderCode source_code = UberShader::GenPixelShader(m_api_type, m_host_config, uid.GetUidData());
  return g_renderer->CreateShaderFromSource(ShaderStage::Pixel, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().size());
}

const AbstractShader* ShaderCache::InsertVertexShader(const VertexShaderUid& uid,
                                                      std::unique_ptr<AbstractShader> shader)
{
  auto& entry = m_vs_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && shader->HasBinary())
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_vs_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(stats.numVertexShadersCreated);
    INCSTAT(stats.numVertexShadersAlive);
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
    if (g_ActiveConfig.bShaderCache && shader->HasBinary())
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_uber_vs_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(stats.numVertexShadersCreated);
    INCSTAT(stats.numVertexShadersAlive);
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
    if (g_ActiveConfig.bShaderCache && shader->HasBinary())
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_ps_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(stats.numPixelShadersCreated);
    INCSTAT(stats.numPixelShadersAlive);
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
    if (g_ActiveConfig.bShaderCache && shader->HasBinary())
    {
      auto binary = shader->GetBinary();
      if (!binary.empty())
        m_uber_ps_cache.disk_cache.Append(uid, binary.data(), static_cast<u32>(binary.size()));
    }
    INCSTAT(stats.numPixelShadersCreated);
    INCSTAT(stats.numPixelShadersAlive);
    entry.shader = std::move(shader);
  }

  return entry.shader.get();
}

const AbstractShader* ShaderCache::CreateGeometryShader(const GeometryShaderUid& uid)
{
  ShaderCode source_code = GenerateGeometryShaderCode(m_api_type, m_host_config, uid.GetUidData());
  std::unique_ptr<AbstractShader> shader = g_renderer->CreateShaderFromSource(
      ShaderStage::Geometry, source_code.GetBuffer().c_str(), source_code.GetBuffer().size());

  auto& entry = m_gs_cache.shader_map[uid];
  entry.pending = false;

  if (shader && !entry.shader)
  {
    if (g_ActiveConfig.bShaderCache && shader->HasBinary())
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

AbstractPipelineConfig ShaderCache::GetGXPipelineConfig(
    const NativeVertexFormat* vertex_format, const AbstractShader* vertex_shader,
    const AbstractShader* geometry_shader, const AbstractShader* pixel_shader,
    const RasterizationState& rasterization_state, const DepthState& depth_state,
    const BlendingState& blending_state)
{
  AbstractPipelineConfig config = {};
  config.usage = AbstractPipelineUsage::GX;
  config.vertex_format = vertex_format;
  config.vertex_shader = vertex_shader;
  config.geometry_shader = geometry_shader;
  config.pixel_shader = pixel_shader;
  config.rasterization_state = rasterization_state;
  config.depth_state = depth_state;
  config.blending_state = blending_state;
  config.framebuffer_state.color_texture_format = AbstractTextureFormat::RGBA8;
  config.framebuffer_state.depth_texture_format = m_efb_depth_format;
  config.framebuffer_state.per_sample_shading = m_host_config.ssaa;
  config.framebuffer_state.samples = m_efb_multisamples;
  return config;
}

std::optional<AbstractPipelineConfig> ShaderCache::GetGXPipelineConfig(const GXPipelineUid& config)
{
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
                             config.depth_state, config.blending_state);
}

std::optional<AbstractPipelineConfig>
ShaderCache::GetGXUberPipelineConfig(const GXUberPipelineUid& config)
{
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
                             config.depth_state, config.blending_state);
}

const AbstractPipeline* ShaderCache::InsertGXPipeline(const GXPipelineUid& config,
                                                      std::unique_ptr<AbstractPipeline> pipeline)
{
  auto& entry = m_gx_pipeline_cache[config];
  entry.second = false;
  if (!entry.first && pipeline)
    entry.first = std::move(pipeline);

  return entry.first.get();
}

const AbstractPipeline*
ShaderCache::InsertGXUberPipeline(const GXUberPipelineUid& config,
                                  std::unique_ptr<AbstractPipeline> pipeline)
{
  auto& entry = m_gx_uber_pipeline_cache[config];
  entry.second = false;
  if (!entry.first && pipeline)
    entry.first = std::move(pipeline);

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
        uid_file_valid = m_gx_pipeline_uid_cache_file.Seek(expected_size, SEEK_SET);
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

  INFO_LOG(VIDEO, "Read %u pipeline UIDs from %s",
           static_cast<unsigned>(m_gx_pipeline_cache.size()), filename.c_str());
}

void ShaderCache::ClosePipelineUIDCache()
{
  // This is left as a method in case we need to append extra data to the file in the future.
  m_gx_pipeline_uid_cache_file.Close();
}

void ShaderCache::AddSerializedGXPipelineUID(const SerializedGXPipelineUid& uid)
{
  GXPipelineUid real_uid = {};
  real_uid.vertex_format = VertexLoaderManager::GetOrCreateMatchingFormat(uid.vertex_decl);
  real_uid.vs_uid = uid.vs_uid;
  real_uid.gs_uid = uid.gs_uid;
  real_uid.ps_uid = uid.ps_uid;
  real_uid.rasterization_state.hex = uid.rasterization_state_bits;
  real_uid.depth_state.hex = uid.depth_state_bits;
  real_uid.blending_state.hex = uid.blending_state_bits;

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

  // Convert to disk format. Ensure all padding bytes are zero.
  SerializedGXPipelineUid disk_uid;
  std::memset(&disk_uid, 0, sizeof(disk_uid));
  disk_uid.vertex_decl = config.vertex_format->GetVertexDeclaration();
  disk_uid.vs_uid = config.vs_uid;
  disk_uid.gs_uid = config.gs_uid;
  disk_uid.ps_uid = config.ps_uid;
  disk_uid.rasterization_state_bits = config.rasterization_state.hex;
  disk_uid.depth_state_bits = config.depth_state.hex;
  disk_uid.blending_state_bits = config.blending_state.hex;
  if (!m_gx_pipeline_uid_cache_file.WriteBytes(&disk_uid, sizeof(disk_uid)))
  {
    WARN_LOG(VIDEO, "Writing pipeline UID to cache failed, closing file.");
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

      auto vs_it = shader_cache->m_vs_cache.shader_map.find(uid.vs_uid);
      stages_ready &= vs_it != shader_cache->m_vs_cache.shader_map.end() && !vs_it->second.pending;
      if (vs_it == shader_cache->m_vs_cache.shader_map.end())
        shader_cache->QueueVertexShaderCompile(uid.vs_uid, priority);

      PixelShaderUid ps_uid = uid.ps_uid;
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
        pipeline = g_renderer->CreatePipeline(*config);
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
        config = shader_cache->GetGXUberPipelineConfig(uid);
    }

    bool SetStagesReady()
    {
      stages_ready = true;

      auto vs_it = shader_cache->m_uber_vs_cache.shader_map.find(uid.vs_uid);
      stages_ready &=
          vs_it != shader_cache->m_uber_vs_cache.shader_map.end() && !vs_it->second.pending;
      if (vs_it == shader_cache->m_uber_vs_cache.shader_map.end())
        shader_cache->QueueVertexUberShaderCompile(uid.vs_uid, priority);

      UberShader::PixelShaderUid ps_uid = uid.ps_uid;
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
        UberPipeline = g_renderer->CreatePipeline(*config);
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
  dummy_vertex_decl.position.type = VAR_FLOAT;
  dummy_vertex_decl.position.enable = true;
  dummy_vertex_decl.stride = sizeof(float) * 4;
  NativeVertexFormat* dummy_vertex_format =
      VertexLoaderManager::GetUberVertexFormat(dummy_vertex_decl);
  auto QueueDummyPipeline = [&](const UberShader::VertexShaderUid& vs_uid,
                                const GeometryShaderUid& gs_uid,
                                const UberShader::PixelShaderUid& ps_uid) {
    GXUberPipelineUid config;
    config.vertex_format = dummy_vertex_format;
    config.vs_uid = vs_uid;
    config.gs_uid = gs_uid;
    config.ps_uid = ps_uid;
    config.rasterization_state = RenderState::GetNoCullRasterizationState();
    config.depth_state = RenderState::GetNoDepthTestingDepthStencilState();
    config.blending_state = RenderState::GetNoBlendingBlendState();

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

      EnumerateGeometryShaderUids([&](const GeometryShaderUid& guid) {
        if (guid.GetUidData()->numTexGens != vuid.GetUidData()->num_texgens ||
            (!guid.GetUidData()->IsPassthrough() && !m_host_config.backend_geometry_shaders))
        {
          return;
        }
        QueueDummyPipeline(vuid, guid, puid);
      });
    });
  });
}

std::string ShaderCache::GetUtilityShaderHeader() const
{
  std::stringstream ss;

  ss << "#define API_D3D " << (m_api_type == APIType::D3D ? 1 : 0) << "\n";
  ss << "#define API_OPENGL " << (m_api_type == APIType::OpenGL ? 1 : 0) << "\n";
  ss << "#define API_VULKAN " << (m_api_type == APIType::Vulkan ? 1 : 0) << "\n";

  if (m_efb_multisamples > 1)
  {
    ss << "#define MSAA_ENABLED 1" << std::endl;
    ss << "#define MSAA_SAMPLES " << m_efb_multisamples << std::endl;
    if (m_host_config.ssaa)
      ss << "#define SSAA_ENABLED 1" << std::endl;
  }

  ss << "#define EFB_LAYERS " << (m_host_config.stereo ? 2 : 1) << std::endl;

  return ss.str();
}
}  // namespace VideoCommon
