// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// ---------------------------------------------------------------------------------------------
// GC graphics pipeline
// ---------------------------------------------------------------------------------------------
// 3d commands are issued through the fifo. The GPU draws to the 2MB EFB.
// The efb can be copied back into ram in two forms: as textures or as XFB.
// The XFB is the region in RAM that the VI chip scans out to the television.
// So, after all rendering to EFB is done, the image is copied into one of two XFBs in RAM.
// Next frame, that one is scanned out and the other one gets the copy. = double buffering.
// ---------------------------------------------------------------------------------------------

#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Flag.h"
#include "Common/MathUtil.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModManager.h"
#include "VideoCommon/RenderState.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;
class AbstractStagingTexture;
class BoundingBox;
class NativeVertexFormat;
class PixelShaderManager;
class PointerWrap;
struct ComputePipelineConfig;
struct AbstractPipelineConfig;
struct PortableVertexDeclaration;
struct TextureConfig;
enum class AbstractTextureFormat : u32;
enum class ShaderStage;
enum class EFBAccessType;
enum class EFBReinterpretType;
enum class StagingTextureType;

namespace VideoCommon
{
class AsyncShaderCompiler;
}

struct EfbPokeData
{
  u16 x, y;
  u32 data;
};

// Renderer really isn't a very good name for this class - it's more like "Misc".
// The long term goal is to get rid of this class and replace it with others that make
// more sense.
class Renderer
{
public:
  Renderer(int backbuffer_width, int backbuffer_height, float backbuffer_scale,
           AbstractTextureFormat backbuffer_format);
  virtual ~Renderer();

  using ClearColor = std::array<float, 4>;

  virtual bool IsHeadless() const = 0;

  virtual bool Initialize();
  virtual void Shutdown();

  virtual void SetPipeline(const AbstractPipeline* pipeline) {}
  virtual void SetScissorRect(const MathUtil::Rectangle<int>& rc) {}
  virtual void SetTexture(u32 index, const AbstractTexture* texture) {}
  virtual void SetSamplerState(u32 index, const SamplerState& state) {}
  virtual void SetComputeImageTexture(AbstractTexture* texture, bool read, bool write) {}
  virtual void UnbindTexture(const AbstractTexture* texture) {}
  virtual void SetViewport(float x, float y, float width, float height, float near_depth,
                           float far_depth)
  {
  }
  virtual void SetFullscreen(bool enable_fullscreen) {}
  virtual bool IsFullscreen() const { return false; }
  virtual void BeginUtilityDrawing();
  virtual void EndUtilityDrawing();
  virtual std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config,
                                                         std::string_view name = "") = 0;
  virtual std::unique_ptr<AbstractStagingTexture>
  CreateStagingTexture(StagingTextureType type, const TextureConfig& config) = 0;
  virtual std::unique_ptr<AbstractFramebuffer>
  CreateFramebuffer(AbstractTexture* color_attachment, AbstractTexture* depth_attachment) = 0;

  // Framebuffer operations.
  virtual void SetFramebuffer(AbstractFramebuffer* framebuffer);
  virtual void SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer);
  virtual void SetAndClearFramebuffer(AbstractFramebuffer* framebuffer,
                                      const ClearColor& color_value = {}, float depth_value = 0.0f);

  // Drawing with currently-bound pipeline state.
  virtual void Draw(u32 base_vertex, u32 num_vertices) {}
  virtual void DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex) {}

  // Dispatching compute shaders with currently-bound state.
  virtual void DispatchComputeShader(const AbstractShader* shader, u32 groupsize_x, u32 groupsize_y,
                                     u32 groupsize_z, u32 groups_x, u32 groups_y, u32 groups_z)
  {
  }

  // Binds the backbuffer for rendering. The buffer will be cleared immediately after binding.
  // This is where any window size changes are detected, therefore m_backbuffer_width and/or
  // m_backbuffer_height may change after this function returns.
  virtual void BindBackbuffer(const ClearColor& clear_color = {}) {}

  // Presents the backbuffer to the window system, or "swaps buffers".
  virtual void PresentBackbuffer() {}

  // Shader modules/objects.
  virtual std::unique_ptr<AbstractShader> CreateShaderFromSource(ShaderStage stage,
                                                                 std::string_view source,
                                                                 std::string_view name = "") = 0;
  virtual std::unique_ptr<AbstractShader> CreateShaderFromBinary(ShaderStage stage,
                                                                 const void* data, size_t length,
                                                                 std::string_view name = "") = 0;
  virtual std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) = 0;
  virtual std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config,
                                                           const void* cache_data = nullptr,
                                                           size_t cache_data_length = 0) = 0;

  AbstractFramebuffer* GetCurrentFramebuffer() const { return m_current_framebuffer; }

  // Ideal internal resolution - multiple of the native EFB resolution
  int GetTargetWidth() const { return m_target_width; }
  int GetTargetHeight() const { return m_target_height; }

  // Sets viewport and scissor to the specified rectangle. rect is assumed to be in framebuffer
  // coordinates, i.e. lower-left origin in OpenGL.
  void SetViewportAndScissor(const MathUtil::Rectangle<int>& rect, float min_depth = 0.0f,
                             float max_depth = 1.0f);

  // Scales a GPU texture using a copy shader.
  virtual void ScaleTexture(AbstractFramebuffer* dst_framebuffer,
                            const MathUtil::Rectangle<int>& dst_rect,
                            const AbstractTexture* src_texture,
                            const MathUtil::Rectangle<int>& src_rect);

  // Converts an upper-left to lower-left if required by the backend, optionally
  // clamping to the framebuffer size.
  MathUtil::Rectangle<int> ConvertFramebufferRectangle(const MathUtil::Rectangle<int>& rect,
                                                       u32 fb_width, u32 fb_height) const;
  MathUtil::Rectangle<int>
  ConvertFramebufferRectangle(const MathUtil::Rectangle<int>& rect,
                              const AbstractFramebuffer* framebuffer) const;

  // EFB coordinate conversion functions
  // Use this to convert a whole native EFB rect to backbuffer coordinates
  MathUtil::Rectangle<int> ConvertEFBRectangle(const MathUtil::Rectangle<int>& rc) const;

  unsigned int GetEFBScale() const;

  // Use this to upscale native EFB coordinates to IDEAL internal resolution
  int EFBToScaledX(int x) const;
  int EFBToScaledY(int y) const;

  // Floating point versions of the above - only use them if really necessary
  float EFBToScaledXf(float x) const;
  float EFBToScaledYf(float y) const;

  virtual void ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                           bool zEnable, u32 color, u32 z);
  virtual void ReinterpretPixelData(EFBReinterpretType convtype);
  void RenderToXFB(u32 xfbAddr, const MathUtil::Rectangle<int>& sourceRc, u32 fbStride,
                   u32 fbHeight, float Gamma = 1.0f);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points);

  bool IsBBoxEnabled() const;
  void BBoxEnable(PixelShaderManager& pixel_shader_manager);
  void BBoxDisable(PixelShaderManager& pixel_shader_manager);
  u16 BBoxRead(u32 index);
  void BBoxWrite(u32 index, u16 value);
  void BBoxFlush();

  virtual void Flush() {}
  virtual void WaitForGPUIdle() {}

  // Finish up the current frame, print some stats
  void Swap(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks);

  void UpdateWidescreenHeuristic();
  bool IsGameWidescreen() const { return m_is_game_widescreen; }

  // A simple presentation fallback, only used by video software
  virtual void ShowImage(const AbstractTexture* source_texture,
                         const MathUtil::Rectangle<int>& source_rc)
  {
  }

  // For opengl's glDrawBuffer
  virtual void SelectLeftBuffer() {}
  virtual void SelectRightBuffer() {}
  virtual void SelectMainBuffer() {}

  // Called when the configuration changes, and backend structures need to be updated.
  virtual void OnConfigChanged(u32 bits) {}

  PixelFormat GetPrevPixelFormat() const { return m_prev_efb_format; }
  void StorePixelFormat(PixelFormat new_format) { m_prev_efb_format = new_format; }
  bool EFBHasAlphaChannel() const;

  bool UseVertexDepthRange() const;
  void DoState(PointerWrap& p);

  virtual std::unique_ptr<VideoCommon::AsyncShaderCompiler> CreateAsyncShaderCompiler();

  // Returns true if a layer-expanding geometry shader should be used when rendering the user
  // interface and final XFB.
  bool UseGeometryShaderForUI() const;

  // Will forcibly reload all textures on the next swap
  void ForceReloadTextures();

  const GraphicsModManager& GetGraphicsModManager() const;

  // Bitmask containing information about which configuration has changed for the backend.
  enum ConfigChangeBits : u32
  {
    CONFIG_CHANGE_BIT_HOST_CONFIG = (1 << 0),
    CONFIG_CHANGE_BIT_MULTISAMPLES = (1 << 1),
    CONFIG_CHANGE_BIT_STEREO_MODE = (1 << 2),
    CONFIG_CHANGE_BIT_TARGET_SIZE = (1 << 3),
    CONFIG_CHANGE_BIT_ANISOTROPY = (1 << 4),
    CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING = (1 << 5),
    CONFIG_CHANGE_BIT_VSYNC = (1 << 6),
    CONFIG_CHANGE_BIT_BBOX = (1 << 7)
  };

protected:
  std::tuple<int, int> CalculateTargetScale(int x, int y) const;
  bool CalculateTargetSize();

  void CheckForConfigChanges();

  void CheckFifoRecording();
  void RecordVideoMemory();

  virtual std::unique_ptr<BoundingBox> CreateBoundingBox() const = 0;

  AbstractFramebuffer* m_current_framebuffer = nullptr;
  const AbstractPipeline* m_current_pipeline = nullptr;

  bool m_is_game_widescreen = false;
  bool m_was_orthographically_anamorphic = false;

  // The framebuffer size
  int m_target_width = 1;
  int m_target_height = 1;

  int m_frame_count = 0;

private:
  PixelFormat m_prev_efb_format = PixelFormat::INVALID_FMT;
  unsigned int m_efb_scale = 1;

  u64 m_last_xfb_ticks = 0;
  u32 m_last_xfb_addr = 0;
  u32 m_last_xfb_width = 0;
  u32 m_last_xfb_stride = 0;
  u32 m_last_xfb_height = 0;

  std::unique_ptr<BoundingBox> m_bounding_box;

  // Nintendo's SDK seems to write "default" bounding box values before every draw (1023 0 1023 0
  // are the only values encountered so far, which happen to be the extents allowed by the BP
  // registers) to reset the registers for comparison in the pixel engine, and presumably to detect
  // whether GX has updated the registers with real values.
  //
  // We can store these values when Bounding Box emulation is disabled and return them on read,
  // which the game will interpret as "no pixels have been drawn"
  //
  // This produces much better results than just returning garbage, which can cause games like
  // Ultimate Spider-Man to crash
  std::array<u16, 4> m_bounding_box_fallback = {};

  Common::Flag m_force_reload_textures;

  GraphicsModManager m_graphics_mod_manager;
};

extern std::unique_ptr<Renderer> g_renderer;
