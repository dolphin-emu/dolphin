// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <optional>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractStagingTexture.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"

class NativeVertexFormat;

enum class EFBReinterpretType
{
  RGB8ToRGB565 = 0,
  RGB8ToRGBA6 = 1,
  RGBA6ToRGB8 = 2,
  RGBA6ToRGB565 = 3,
  RGB565ToRGB8 = 4,
  RGB565ToRGBA6 = 5
};
constexpr u32 NUM_EFB_REINTERPRET_TYPES = 6;

inline bool AddressRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
  return !((aLower >= bUpper) || (bLower >= aUpper));
}

class FramebufferManager final
{
public:
  FramebufferManager();
  virtual ~FramebufferManager();

  // Does not require the framebuffer to be created. Slower than direct queries.
  static AbstractTextureFormat GetEFBColorFormat();
  static AbstractTextureFormat GetEFBDepthFormat();
  static TextureConfig GetEFBColorTextureConfig();
  static TextureConfig GetEFBDepthTextureConfig();

  // Accessors.
  AbstractTexture* GetEFBColorTexture() const { return m_efb_color_texture.get(); }
  AbstractTexture* GetEFBDepthTexture() const { return m_efb_depth_texture.get(); }
  AbstractFramebuffer* GetEFBFramebuffer() const { return m_efb_framebuffer.get(); }
  u32 GetEFBWidth() const { return m_efb_color_texture->GetWidth(); }
  u32 GetEFBHeight() const { return m_efb_color_texture->GetHeight(); }
  u32 GetEFBLayers() const { return m_efb_color_texture->GetLayers(); }
  u32 GetEFBSamples() const { return m_efb_color_texture->GetSamples(); }
  bool IsEFBMultisampled() const { return m_efb_color_texture->IsMultisampled(); }
  FramebufferState GetEFBFramebufferState() const;

  // First-time setup.
  bool Initialize();

  // Recreate EFB framebuffers, call when the EFB size (IR) changes.
  void RecreateEFBFramebuffer();

  // Recompile shaders, use when MSAA mode changes.
  void RecompileShaders();

  // This is virtual, because D3D has both normalized and integer framebuffers.
  void BindEFBFramebuffer();

  // Resolve color/depth textures to a non-msaa texture, and return it.
  AbstractTexture* ResolveEFBColorTexture(const MathUtil::Rectangle<int>& region);
  AbstractTexture* ResolveEFBDepthTexture(const MathUtil::Rectangle<int>& region);

  // Reinterpret pixel format of EFB color texture.
  // Assumes no render pass is currently in progress.
  // Swaps EFB framebuffers, so re-bind afterwards.
  bool ReinterpretPixelData(EFBReinterpretType convtype);

  // Clears the EFB using shaders.
  void ClearEFB(const MathUtil::Rectangle<int>& rc, bool clear_color, bool clear_alpha,
                bool clear_z, u32 color, u32 z);

  // Reads a framebuffer value back from the GPU. This may block if the cache is not current.
  u32 PeekEFBColor(u32 x, u32 y);
  float PeekEFBDepth(u32 x, u32 y);
  void SetEFBCacheTileSize(u32 size);
  void InvalidatePeekCache(bool forced = true);
  void FlagPeekCacheAsOutOfDate();

  // Writes a value to the framebuffer. This will never block, and writes will be batched.
  void PokeEFBColor(u32 x, u32 y, u32 color);
  void PokeEFBDepth(u32 x, u32 y, float depth);
  void FlushEFBPokes();

protected:
  struct EFBPokeVertex
  {
    float position[4];
    u32 color;
  };
  static_assert(std::is_standard_layout<EFBPokeVertex>::value, "EFBPokeVertex is standard-layout");

  // EFB cache - for CPU EFB access
  // Tiles are ordered left-to-right, then top-to-bottom
  struct EFBCacheData
  {
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
    std::unique_ptr<AbstractStagingTexture> readback_texture;
    std::unique_ptr<AbstractPipeline> copy_pipeline;
    std::vector<bool> tiles;
    bool out_of_date;
    bool valid;
  };

  bool CreateEFBFramebuffer();
  void DestroyEFBFramebuffer();

  bool CompileConversionPipelines();
  void DestroyConversionPipelines();

  bool CompileReadbackPipelines();
  void DestroyReadbackPipelines();

  bool CreateReadbackFramebuffer();
  void DestroyReadbackFramebuffer();

  bool CompileClearPipelines();
  void DestroyClearPipelines();

  bool CompilePokePipelines();
  void DestroyPokePipelines();

  bool IsUsingTiledEFBCache() const;
  bool IsEFBCacheTilePresent(bool depth, u32 x, u32 y, u32* tile_index) const;
  MathUtil::Rectangle<int> GetEFBCacheTileRect(u32 tile_index) const;
  void PopulateEFBCache(bool depth, u32 tile_index);

  void CreatePokeVertices(std::vector<EFBPokeVertex>* destination_list, u32 x, u32 y, float z,
                          u32 color);

  void DrawPokeVertices(const EFBPokeVertex* vertices, u32 vertex_count,
                        const AbstractPipeline* pipeline);

  std::unique_ptr<AbstractTexture> m_efb_color_texture;
  std::unique_ptr<AbstractTexture> m_efb_convert_color_texture;
  std::unique_ptr<AbstractTexture> m_efb_depth_texture;
  std::unique_ptr<AbstractTexture> m_efb_resolve_color_texture;
  std::unique_ptr<AbstractTexture> m_efb_depth_resolve_texture;

  std::unique_ptr<AbstractFramebuffer> m_efb_framebuffer;
  std::unique_ptr<AbstractFramebuffer> m_efb_convert_framebuffer;
  std::unique_ptr<AbstractFramebuffer> m_efb_depth_resolve_framebuffer;
  std::unique_ptr<AbstractPipeline> m_efb_depth_resolve_pipeline;

  // Format conversion shaders
  std::array<std::unique_ptr<AbstractPipeline>, 6> m_format_conversion_pipelines;

  // EFB cache - for CPU EFB access
  u32 m_efb_cache_tile_size = 0;
  u32 m_efb_cache_tiles_wide = 0;
  EFBCacheData m_efb_color_cache = {};
  EFBCacheData m_efb_depth_cache = {};

  // EFB clear pipelines
  // Indexed by [color_write_enabled][alpha_write_enabled][depth_write_enabled]
  std::array<std::array<std::array<std::unique_ptr<AbstractPipeline>, 2>, 2>, 2>
      m_efb_clear_pipelines;

  // EFB poke drawing setup
  std::unique_ptr<NativeVertexFormat> m_poke_vertex_format;
  std::unique_ptr<AbstractPipeline> m_color_poke_pipeline;
  std::unique_ptr<AbstractPipeline> m_depth_poke_pipeline;
  std::vector<EFBPokeVertex> m_color_poke_vertices;
  std::vector<EFBPokeVertex> m_depth_poke_vertices;
};

extern std::unique_ptr<FramebufferManager> g_framebuffer_manager;
