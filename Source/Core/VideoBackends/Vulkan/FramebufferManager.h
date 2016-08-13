// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "VideoCommon/FramebufferManagerBase.h"

#include "VideoBackends/Vulkan/Constants.h"

namespace Vulkan
{
class StagingTexture2D;
class StateTracker;
class StreamBuffer;
class Texture2D;
class VertexFormat;

class XFBSource : public XFBSourceBase
{
  void DecodeToTexture(u32 xfb_addr, u32 fb_width, u32 fb_height) override {}
  void CopyEFB(float gamma) override {}
};

class FramebufferManager : public FramebufferManagerBase
{
public:
  FramebufferManager();
  ~FramebufferManager();

  bool Initialize();

  VkRenderPass GetEFBRenderPass() const { return m_efb_render_pass; }
  u32 GetEFBWidth() const { return m_efb_width; }
  u32 GetEFBHeight() const { return m_efb_height; }
  u32 GetEFBLayers() const { return m_efb_layers; }
  VkSampleCountFlagBits GetEFBSamples() const { return m_efb_samples; }
  Texture2D* GetEFBColorTexture() const { return m_efb_color_texture.get(); }
  Texture2D* GetEFBDepthTexture() const { return m_efb_depth_texture.get(); }
  VkFramebuffer GetEFBFramebuffer() const { return m_efb_framebuffer; }
  void GetTargetSize(unsigned int* width, unsigned int* height) override;

  std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width,
                                                 unsigned int target_height,
                                                 unsigned int layers) override
  {
    return std::make_unique<XFBSource>();
  }

  void CopyToRealXFB(u32 xfb_addr, u32 fb_stride, u32 fb_height, const EFBRectangle& source_rc,
                     float gamma = 1.0f) override
  {
  }

  void ResizeEFBTextures();

  // Recompile shaders, use when MSAA mode changes.
  void RecreateRenderPass();
  void RecompileShaders();

  // Reinterpret pixel format of EFB color texture.
  // Assumes no render pass is currently in progress.
  // Swaps EFB framebuffers, so re-bind afterwards.
  void ReinterpretPixelData(int convtype);

  // This render pass can be used for other readback operations.
  VkRenderPass GetColorCopyForReadbackRenderPass() const { return m_copy_color_render_pass; }
  // Resolve color/depth textures to a non-msaa texture, and return it.
  Texture2D* ResolveEFBColorTexture(StateTracker* state_tracker, const VkRect2D& region);
  Texture2D* ResolveEFBDepthTexture(StateTracker* state_tracker, const VkRect2D& region);

  // Reads a framebuffer value back from the GPU. This may block if the cache is not current.
  u32 PeekEFBColor(StateTracker* state_tracker, u32 x, u32 y);
  float PeekEFBDepth(StateTracker* state_tracker, u32 x, u32 y);
  void InvalidatePeekCache();

  // Writes a value to the framebuffer. This will never block, and writes will be batched.
  void PokeEFBColor(StateTracker* state_tracker, u32 x, u32 y, u32 color);
  void PokeEFBDepth(StateTracker* state_tracker, u32 x, u32 y, float depth);
  void FlushEFBPokes(StateTracker* state_tracker);

private:
  struct EFBPokeVertex
  {
    float position[4];
    u32 color;
  };

  bool CreateEFBRenderPass();
  void DestroyEFBRenderPass();
  bool CreateEFBFramebuffer();
  void DestroyEFBFramebuffer();

  bool CompileConversionShaders();
  void DestroyConversionShaders();

  bool CreateReadbackRenderPasses();
  void DestroyReadbackRenderPasses();
  bool CompileReadbackShaders();
  void DestroyReadbackShaders();
  bool CreateReadbackTextures();
  void DestroyReadbackTextures();
  bool CreateReadbackFramebuffer();
  void DestroyReadbackFramebuffer();

  void CreatePokeVertexFormat();
  bool CreatePokeVertexBuffer();
  void DestroyPokeVertexBuffer();
  bool CompilePokeShaders();
  void DestroyPokeShaders();

  bool PopulateColorReadbackTexture(StateTracker* state_tracker);
  bool PopulateDepthReadbackTexture(StateTracker* state_tracker);

  void CreatePokeVertices(std::vector<EFBPokeVertex>* destination_list, u32 x, u32 y, float z,
                          u32 color);

  void DrawPokeVertices(StateTracker* state_tracker, const EFBPokeVertex* vertices,
                        size_t vertex_count, bool write_color, bool write_depth);

  VkRenderPass m_efb_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_depth_resolve_render_pass = VK_NULL_HANDLE;

  u32 m_efb_width = 0;
  u32 m_efb_height = 0;
  u32 m_efb_layers = 1;
  VkSampleCountFlagBits m_efb_samples = VK_SAMPLE_COUNT_1_BIT;

  std::unique_ptr<Texture2D> m_efb_color_texture;
  std::unique_ptr<Texture2D> m_efb_convert_color_texture;
  std::unique_ptr<Texture2D> m_efb_depth_texture;
  std::unique_ptr<Texture2D> m_efb_resolve_color_texture;
  std::unique_ptr<Texture2D> m_efb_resolve_depth_texture;
  VkFramebuffer m_efb_framebuffer = VK_NULL_HANDLE;
  VkFramebuffer m_efb_convert_framebuffer = VK_NULL_HANDLE;
  VkFramebuffer m_depth_resolve_framebuffer = VK_NULL_HANDLE;

  // Format conversion shaders
  VkShaderModule m_ps_rgb8_to_rgba6 = VK_NULL_HANDLE;
  VkShaderModule m_ps_rgba6_to_rgb8 = VK_NULL_HANDLE;
  VkShaderModule m_ps_depth_resolve = VK_NULL_HANDLE;

  // EFB readback texture
  std::unique_ptr<Texture2D> m_color_copy_texture;
  std::unique_ptr<Texture2D> m_depth_copy_texture;
  VkFramebuffer m_color_copy_framebuffer = VK_NULL_HANDLE;
  VkFramebuffer m_depth_copy_framebuffer = VK_NULL_HANDLE;

  // CPU-side EFB readback texture
  std::unique_ptr<StagingTexture2D> m_color_readback_texture;
  std::unique_ptr<StagingTexture2D> m_depth_readback_texture;
  bool m_color_readback_texture_valid = false;
  bool m_depth_readback_texture_valid = false;

  // EFB poke drawing setup
  std::unique_ptr<VertexFormat> m_poke_vertex_format;
  std::unique_ptr<StreamBuffer> m_poke_vertex_stream_buffer;
  std::vector<EFBPokeVertex> m_color_poke_vertices;
  std::vector<EFBPokeVertex> m_depth_poke_vertices;
  VkPrimitiveTopology m_poke_primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkRenderPass m_copy_color_render_pass = VK_NULL_HANDLE;
  VkRenderPass m_copy_depth_render_pass = VK_NULL_HANDLE;
  VkShaderModule m_copy_color_shader = VK_NULL_HANDLE;
  VkShaderModule m_copy_depth_shader = VK_NULL_HANDLE;

  VkShaderModule m_poke_vertex_shader = VK_NULL_HANDLE;
  VkShaderModule m_poke_geometry_shader = VK_NULL_HANDLE;
  VkShaderModule m_poke_fragment_shader = VK_NULL_HANDLE;
};

}  // namespace Vulkan
