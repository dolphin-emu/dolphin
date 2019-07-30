// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/LinearDiskCache.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GXPipelineTypes.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConversionShader.h"
#include "VideoCommon/TextureConverterShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

class NativeVertexFormat;
enum class AbstractTextureFormat : u32;
enum class TextureFormat;
enum class TLUTFormat;

namespace VideoCommon
{
class ShaderCache final
{
public:
  ShaderCache();
  ~ShaderCache();

  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();
  void Shutdown();

  // Compiles/loads cached shaders.
  void InitializeShaderCache();

  // Changes the shader host config. Shaders should be reloaded afterwards.
  void SetHostConfig(const ShaderHostConfig& host_config) { m_host_config = host_config; }

  // Reloads/recreates all shaders and pipelines.
  void Reload();

  // Retrieves all pending shaders/pipelines from the async compiler.
  void RetrieveAsyncShaders();

  // Accesses ShaderGen shader caches
  const AbstractPipeline* GetPipelineForUid(const GXPipelineUid& uid);

  // Accesses ShaderGen shader caches asynchronously.
  // The optional will be empty if this pipeline is now background compiling.
  std::optional<const AbstractPipeline*> GetPipelineForUidAsync(const GXPipelineUid& uid);

  // Shared shaders
  const AbstractShader* GetScreenQuadVertexShader() const
  {
    return m_screen_quad_vertex_shader.get();
  }
  const AbstractShader* GetTextureCopyVertexShader() const
  {
    return m_texture_copy_vertex_shader.get();
  }
  const AbstractShader* GetEFBCopyVertexShader() const { return m_efb_copy_vertex_shader.get(); }
  const AbstractShader* GetTextureCopyPixelShader() const
  {
    return m_texture_copy_pixel_shader.get();
  }
  const AbstractShader* GetColorPixelShader() const { return m_color_pixel_shader.get(); }

  // EFB copy to RAM/VRAM pipelines
  const AbstractPipeline*
  GetEFBCopyToVRAMPipeline(const TextureConversionShaderGen::TCShaderUid& uid);
  const AbstractPipeline* GetEFBCopyToRAMPipeline(const EFBCopyParams& uid);

  // RGBA8 framebuffer copy pipelines
  const AbstractPipeline* GetRGBA8CopyPipeline() const { return m_copy_rgba8_pipeline.get(); }

  // Palette texture conversion pipelines
  const AbstractPipeline* GetPaletteConversionPipeline(TLUTFormat format);

  // Texture reinterpret pipelines
  const AbstractPipeline* GetTextureReinterpretPipeline(TextureFormat from_format,
                                                        TextureFormat to_format);

  // Texture decoding compute shaders
  const AbstractShader* GetTextureDecodingShader(TextureFormat format, TLUTFormat palette_format);

private:
  static constexpr size_t NUM_PALETTE_CONVERSION_SHADERS = 3;

  void WaitForAsyncCompiler();
  void LoadCaches();
  void ClearCaches();
  void LoadPipelineUIDCache();
  void ClosePipelineUIDCache();
  void CompileMissingPipelines();
  bool CompileSharedPipelines();

  // GX shader compiler methods
  std::unique_ptr<AbstractShader> CompileVertexShader(const VertexShaderUid& uid) const;
  std::unique_ptr<AbstractShader> CompilePixelShader(const PixelShaderUid& uid) const;
  const AbstractShader* InsertVertexShader(const VertexShaderUid& uid,
                                           std::unique_ptr<AbstractShader> shader);
  const AbstractShader* InsertPixelShader(const PixelShaderUid& uid,
                                          std::unique_ptr<AbstractShader> shader);
  const AbstractShader* CreateGeometryShader(const GeometryShaderUid& uid);
  bool NeedsGeometryShader(const GeometryShaderUid& uid) const;

  // GX pipeline compiler methods
  AbstractPipelineConfig
  GetGXPipelineConfig(const NativeVertexFormat* vertex_format, const AbstractShader* vertex_shader,
                      const AbstractShader* geometry_shader, const AbstractShader* pixel_shader,
                      const RasterizationState& rasterization_state, const DepthState& depth_state,
                      const BlendingState& blending_state);
  std::optional<AbstractPipelineConfig> GetGXPipelineConfig(const GXPipelineUid& uid);
  const AbstractPipeline* InsertGXPipeline(const GXPipelineUid& config,
                                           std::unique_ptr<AbstractPipeline> pipeline);
  void AddSerializedGXPipelineUID(const SerializedGXPipelineUid& uid);
  void AppendGXPipelineUID(const GXPipelineUid& config);

  // ASync Compiler Methods
  void QueueVertexShaderCompile(const VertexShaderUid& uid, u32 priority);
  void QueuePixelShaderCompile(const PixelShaderUid& uid, u32 priority);
  void QueuePipelineCompile(const GXPipelineUid& uid, u32 priority);

  // Populating various caches.
  template <ShaderStage stage, typename K, typename T>
  void LoadShaderCache(T& cache, APIType api_type, const char* type, bool include_gameid);
  template <typename T>
  void ClearShaderCache(T& cache);
  template <typename KeyType, typename DiskKeyType, typename T>
  void LoadPipelineCache(T& cache, LinearDiskCache<DiskKeyType, u8>& disk_cache, APIType api_type,
                         const char* type, bool include_gameid);
  template <typename T, typename Y>
  void ClearPipelineCache(T& cache, Y& disk_cache);

  // Priorities for compiling. The lower the value, the sooner the pipeline is compiled.
  // The shader cache is compiled last, as it is the least likely to be required. On demand
  // shaders are always compiled before pending ubershaders, as we want to use the ubershader
  // for as few frames as possible, otherwise we risk framerate drops.
  enum : u32
  {
    COMPILE_PRIORITY_ONDEMAND_PIPELINE = 100,
    COMPILE_PRIORITY_SHADERCACHE_PIPELINE = 300
  };

  // Configuration bits.
  APIType m_api_type = APIType::Nothing;
  ShaderHostConfig m_host_config = {};
  std::unique_ptr<AsyncShaderCompiler> m_async_shader_compiler;

  // Shared shaders
  std::unique_ptr<AbstractShader> m_screen_quad_vertex_shader;
  std::unique_ptr<AbstractShader> m_texture_copy_vertex_shader;
  std::unique_ptr<AbstractShader> m_efb_copy_vertex_shader;
  std::unique_ptr<AbstractShader> m_texture_copy_pixel_shader;
  std::unique_ptr<AbstractShader> m_color_pixel_shader;

  // GX Shader Caches
  template <typename Uid>
  struct ShaderModuleCache
  {
    struct Shader
    {
      std::unique_ptr<AbstractShader> shader;
      bool pending;
    };
    std::map<Uid, Shader> shader_map;
    LinearDiskCache<Uid, u8> disk_cache;
  };
  ShaderModuleCache<VertexShaderUid> m_vs_cache;
  ShaderModuleCache<GeometryShaderUid> m_gs_cache;
  ShaderModuleCache<PixelShaderUid> m_ps_cache;

  // GX Pipeline Caches - .first - pipeline, .second - pending
  std::map<GXPipelineUid, std::pair<std::unique_ptr<AbstractPipeline>, bool>> m_gx_pipeline_cache;
  File::IOFile m_gx_pipeline_uid_cache_file;
  LinearDiskCache<SerializedGXPipelineUid, u8> m_gx_pipeline_disk_cache;

  // EFB copy to VRAM/RAM pipelines
  std::map<TextureConversionShaderGen::TCShaderUid, std::unique_ptr<AbstractPipeline>>
      m_efb_copy_to_vram_pipelines;
  std::map<EFBCopyParams, std::unique_ptr<AbstractPipeline>> m_efb_copy_to_ram_pipelines;

  // Copy pipeline for RGBA8 textures
  std::unique_ptr<AbstractPipeline> m_copy_rgba8_pipeline;

  // Palette conversion pipelines
  std::array<std::unique_ptr<AbstractPipeline>, NUM_PALETTE_CONVERSION_SHADERS>
      m_palette_conversion_pipelines;

  // Texture reinterpreting pipeline
  std::map<std::pair<TextureFormat, TextureFormat>, std::unique_ptr<AbstractPipeline>>
      m_texture_reinterpret_pipelines;

  // Texture decoding shaders
  std::map<std::pair<u32, u32>, std::unique_ptr<AbstractShader>> m_texture_decoding_shaders;
};

}  // namespace VideoCommon

extern std::unique_ptr<VideoCommon::ShaderCache> g_shader_cache;
