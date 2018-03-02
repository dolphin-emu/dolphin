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
#include "Common/LinearDiskCache.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/NativeVertexFormat.h"

#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureConverterShaderGen.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexShaderGen.h"

class NativeVertexFormat;

namespace VideoCommon
{
struct GXPipelineConfig
{
  const NativeVertexFormat* vertex_format;
  VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  PixelShaderUid ps_uid;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;

  // We use memcmp() for comparing pipelines as std::tie generates a large number of instructions,
  // and this map lookup can happen every draw call. However, as using memcmp() will also compare
  // any padding bytes, we have to ensure these are zeroed out.
  GXPipelineConfig() { std::memset(this, 0, sizeof(*this)); }
  GXPipelineConfig(const GXPipelineConfig& rhs) { std::memcpy(this, &rhs, sizeof(*this)); }
  GXPipelineConfig& operator=(const GXPipelineConfig& rhs)
  {
    std::memcpy(this, &rhs, sizeof(*this));
    return *this;
  }
  bool operator<(const GXPipelineConfig& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) < 0;
  }
  bool operator==(const GXPipelineConfig& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) == 0;
  }
  bool operator!=(const GXPipelineConfig& rhs) const { return !operator==(rhs); }
};
struct GXUberPipelineConfig
{
  const NativeVertexFormat* vertex_format;
  UberShader::VertexShaderUid vs_uid;
  GeometryShaderUid gs_uid;
  UberShader::PixelShaderUid ps_uid;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;

  GXUberPipelineConfig() { std::memset(this, 0, sizeof(*this)); }
  GXUberPipelineConfig(const GXUberPipelineConfig& rhs) { std::memcpy(this, &rhs, sizeof(*this)); }
  GXUberPipelineConfig& operator=(const GXUberPipelineConfig& rhs)
  {
    std::memcpy(this, &rhs, sizeof(*this));
    return *this;
  }
  bool operator<(const GXUberPipelineConfig& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) < 0;
  }
  bool operator==(const GXUberPipelineConfig& rhs) const
  {
    return std::memcmp(this, &rhs, sizeof(*this)) == 0;
  }
  bool operator!=(const GXUberPipelineConfig& rhs) const { return !operator==(rhs); }
};
struct EFBClearPipelineConfig
{
  EFBClearPipelineConfig(bool color_enable_, bool alpha_enable_, bool depth_enable_)
      : color_enable(color_enable_), alpha_enable(alpha_enable_), depth_enable(depth_enable_)
  {
  }

  bool color_enable;
  bool alpha_enable;
  bool depth_enable;

  bool operator<(const EFBClearPipelineConfig& rhs) const
  {
    return std::tie(color_enable, alpha_enable, depth_enable) <
           std::tie(rhs.color_enable, rhs.alpha_enable, rhs.depth_enable);
  }
};
struct BlitPipelineConfig
{
  BlitPipelineConfig(AbstractTextureFormat dst_format_, bool stereo_ = false)
      : dst_format(dst_format_), stereo(stereo_)
  {
  }

  AbstractTextureFormat dst_format;
  bool stereo;

  bool operator<(const BlitPipelineConfig& rhs) const
  {
    return std::tie(dst_format, stereo) < std::tie(rhs.dst_format, rhs.stereo);
  }
};

struct UtilityVertex
{
  float position[4] = {};
  float uv[3] = {};
  u32 color = 0;

  UtilityVertex() = default;
  void SetPosition(float x = 0.0f, float y = 0.0f, float z = 0.0f, float w = 1.0f)
  {
    position[0] = x;
    position[1] = y;
    position[2] = z;
    position[3] = w;
  }
  void SetTextureCoordinates(float u = 0.0f, float v = 0.0f, float layer = 0.0f)
  {
    uv[0] = u;
    uv[1] = v;
    uv[2] = layer;
  }
  void SetColor(u32 color_) { color = color_; }
  void SetColorRGBA(u32 r = 0, u32 g = 0, u32 b = 0, u32 a = 255)
  {
    color = (r) | (g << 8) | (b << 16) | (a << 24);
  }
};

class ShaderCache final
{
public:
  ShaderCache();
  ~ShaderCache();

  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();
  void Shutdown();

  // Changes the shader host config. Shaders will be reloaded if there are changes.
  void SetHostConfig(const ShaderHostConfig& host_config, u32 efb_multisamples);

  // Reloads/recreates all shaders and pipelines.
  void Reload();

  // Retrieves all pending shaders/pipelines from the async compiler.
  void RetrieveAsyncShaders();

  // Get utility shader header based on current config.
  std::string GetUtilityShaderHeader() const;
  const NativeVertexFormat* GetUtilityVertexFormat() const { return m_utility_vertex_format.get(); }
  const AbstractShader* GetScreenQuadVertexShader() const
  {
    return m_screen_quad_vertex_shader.get();
  }
  const AbstractShader* GetPassthroughVertexShader() const
  {
    return m_passthrough_vertex_shader.get();
  }
  const AbstractShader* GetClearPixelShader() const { return m_clear_pixel_shader.get(); }
  const AbstractShader* GetBlitPixelShader() const { return m_blit_pixel_shader.get(); }
  // Accesses ShaderGen shader caches
  const AbstractPipeline* GetPipelineForUid(const GXPipelineConfig& uid);
  const AbstractPipeline* GetUberPipelineForUid(const GXUberPipelineConfig& uid);

  // Accesses ShaderGen shader caches asynchronously.
  // The optional will be empty if this pipeline is now background compiling.
  std::optional<const AbstractPipeline*> GetPipelineForUidAsync(const GXPipelineConfig& uid);

  // Utility pipeline creation/access.
  const AbstractPipeline*
  GetEFBToTexturePipeline(const TextureConversionShaderGen::TCShaderUid& uid);
  const AbstractPipeline* GetEFBToRAMPipeline(const EFBCopyParams& uid);
  const AbstractPipeline* GetEFBClearPipeline(const EFBClearPipelineConfig& uid);
  const AbstractPipeline* GetBlitPipeline(const BlitPipelineConfig& uid);

private:
  void WaitForAsyncCompiler();
  void LoadShaderCaches();
  void ClearShaderCaches();
  void LoadPipelineUIDCache();
  void CompileMissingPipelines();
  void InvalidateCachedPipelines();
  void ClearPipelineCaches();
  void PrecompileUberShaders();
  void CreateUtilityVertexFormat();
  bool CompileUtilityShaders();

  // GX shader compiler methods
  std::unique_ptr<AbstractShader> CompileVertexShader(const VertexShaderUid& uid) const;
  std::unique_ptr<AbstractShader>
  CompileVertexUberShader(const UberShader::VertexShaderUid& uid) const;
  std::unique_ptr<AbstractShader> CompilePixelShader(const PixelShaderUid& uid) const;
  std::unique_ptr<AbstractShader>
  CompilePixelUberShader(const UberShader::PixelShaderUid& uid) const;
  const AbstractShader* InsertVertexShader(const VertexShaderUid& uid,
                                           std::unique_ptr<AbstractShader> shader);
  const AbstractShader* InsertVertexUberShader(const UberShader::VertexShaderUid& uid,
                                               std::unique_ptr<AbstractShader> shader);
  const AbstractShader* InsertPixelShader(const PixelShaderUid& uid,
                                          std::unique_ptr<AbstractShader> shader);
  const AbstractShader* InsertPixelUberShader(const UberShader::PixelShaderUid& uid,
                                              std::unique_ptr<AbstractShader> shader);
  const AbstractShader* CreateGeometryShader(const GeometryShaderUid& uid);
  bool NeedsGeometryShader(const GeometryShaderUid& uid) const;

  // GX pipeline compiler methods
  AbstractPipelineConfig
  GetGXPipelineConfig(const NativeVertexFormat* vertex_format, const AbstractShader* vertex_shader,
                      const AbstractShader* geometry_shader, const AbstractShader* pixel_shader,
                      const RasterizationState& rasterization_state, const DepthState& depth_state,
                      const BlendingState& blending_state);
  std::optional<AbstractPipelineConfig> GetGXPipelineConfig(const GXPipelineConfig& uid);
  std::optional<AbstractPipelineConfig> GetGXUberPipelineConfig(const GXUberPipelineConfig& uid);
  const AbstractPipeline* InsertGXPipeline(const GXPipelineConfig& config,
                                           std::unique_ptr<AbstractPipeline> pipeline);
  const AbstractPipeline* InsertGXUberPipeline(const GXUberPipelineConfig& config,
                                               std::unique_ptr<AbstractPipeline> pipeline);
  void AppendGXPipelineUID(const GXPipelineConfig& config);

  // ASync Compiler Methods
  void QueueVertexShaderCompile(const VertexShaderUid& uid);
  void QueueVertexUberShaderCompile(const UberShader::VertexShaderUid& uid);
  void QueuePixelShaderCompile(const PixelShaderUid& uid);
  void QueuePixelUberShaderCompile(const UberShader::PixelShaderUid& uid);
  void QueuePipelineCompile(const GXPipelineConfig& uid);
  void QueueUberPipelineCompile(const GXUberPipelineConfig& uid);

  // Utility Pipeline Methods
  std::unique_ptr<AbstractPipeline>
  CreateEFBToTexturePipeline(const TextureConversionShaderGen::TCShaderUid& uid) const;
  std::unique_ptr<AbstractPipeline> CreateEFBToRAMPipeline(const EFBCopyParams& uid) const;
  std::unique_ptr<AbstractPipeline> CreateEFBClearPipeline(const EFBClearPipelineConfig& uid) const;
  std::unique_ptr<AbstractPipeline> CreateBlitPipeline(const BlitPipelineConfig& uid) const;

  // Configuration bits.
  APIType m_api_type = APIType::Nothing;
  ShaderHostConfig m_host_config = {};
  u32 m_efb_multisamples = 1;
  std::unique_ptr<AsyncShaderCompiler> m_async_shader_compiler;

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
  ShaderModuleCache<UberShader::VertexShaderUid> m_uber_vs_cache;
  ShaderModuleCache<UberShader::PixelShaderUid> m_uber_ps_cache;

  // GX Pipeline Caches - .first - pipeline, .second - pending
  // TODO: Use unordered_map for speed.
  std::map<GXPipelineConfig, std::pair<std::unique_ptr<AbstractPipeline>, bool>>
      m_gx_pipeline_cache;
  std::map<GXUberPipelineConfig, std::pair<std::unique_ptr<AbstractPipeline>, bool>>
      m_gx_uber_pipeline_cache;

  // Disk cache of pipeline UIDs
  // We can't use the whole UID as a type
  struct GXPipelineDiskCacheUid
  {
    PortableVertexDeclaration vertex_decl;
    VertexShaderUid vs_uid;
    GeometryShaderUid gs_uid;
    PixelShaderUid ps_uid;
    u32 rasterization_state_bits;
    u32 depth_state_bits;
    u32 blending_state_bits;
  };
  LinearDiskCache<GXPipelineDiskCacheUid, u8> m_gx_pipeline_uid_disk_cache;

  // Pipeline cache for various utility shaders
  std::unique_ptr<NativeVertexFormat> m_utility_vertex_format;
  std::unique_ptr<AbstractShader> m_screen_quad_vertex_shader;
  std::unique_ptr<AbstractShader> m_passthrough_vertex_shader;
  std::unique_ptr<AbstractShader> m_stereo_expand_geometry_shader;
  std::unique_ptr<AbstractShader> m_clear_pixel_shader;
  std::unique_ptr<AbstractShader> m_blit_pixel_shader;
  std::map<TextureConversionShaderGen::TCShaderUid, std::unique_ptr<AbstractPipeline>>
      m_efb_to_texture_pipelines;
  std::map<EFBCopyParams, std::unique_ptr<AbstractPipeline>> m_efb_to_ram_pipelines;
  std::map<EFBClearPipelineConfig, std::unique_ptr<AbstractPipeline>> m_efb_clear_pipelines;
  std::map<BlitPipelineConfig, std::unique_ptr<AbstractPipeline>> m_blit_pipelines;
};

}  // namespace VideoCommon

extern std::unique_ptr<VideoCommon::ShaderCache> g_shader_cache;
