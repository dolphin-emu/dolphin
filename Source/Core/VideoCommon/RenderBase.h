// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/MathUtil.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/FrameDump.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;
class AbstractStagingTexture;
class NativeVertexFormat;
struct TextureConfig;
struct AbstractPipelineConfig;
struct PortableVertexDeclaration;
enum class ShaderStage;
enum class EFBAccessType;
enum class EFBReinterpretType;
enum class StagingTextureType;

namespace VideoCommon
{
class RasterFont;
class PostProcessing;
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
  virtual std::unique_ptr<AbstractTexture> CreateTexture(const TextureConfig& config) = 0;
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
  virtual void DispatchComputeShader(const AbstractShader* shader, u32 groups_x, u32 groups_y,
                                     u32 groups_z)
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
                                                                 std::string_view source) = 0;
  virtual std::unique_ptr<AbstractShader>
  CreateShaderFromBinary(ShaderStage stage, const void* data, size_t length) = 0;
  virtual std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) = 0;
  virtual std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config,
                                                           const void* cache_data = nullptr,
                                                           size_t cache_data_length = 0) = 0;

  AbstractFramebuffer* GetCurrentFramebuffer() const { return m_current_framebuffer; }

  // Ideal internal resolution - multiple of the native EFB resolution
  int GetTargetWidth() const { return m_target_width; }
  int GetTargetHeight() const { return m_target_height; }
  // Display resolution
  int GetBackbufferWidth() const { return m_backbuffer_width; }
  int GetBackbufferHeight() const { return m_backbuffer_height; }
  float GetBackbufferScale() const { return m_backbuffer_scale; }
  void SetWindowSize(int width, int height);

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
                                                       u32 fb_width, u32 fb_height);
  MathUtil::Rectangle<int> ConvertFramebufferRectangle(const MathUtil::Rectangle<int>& rect,
                                                       const AbstractFramebuffer* framebuffer);

  // EFB coordinate conversion functions
  // Use this to convert a whole native EFB rect to backbuffer coordinates
  MathUtil::Rectangle<int> ConvertEFBRectangle(const MathUtil::Rectangle<int>& rc);
  float CalculateDrawAspectRatio() const;

  // Crops the target rectangle to the framebuffer dimensions, reducing the size of the source
  // rectangle if it is greater. Works even if the source and target rectangles don't have a
  // 1:1 pixel mapping, scaling as appropriate.
  void AdjustRectanglesToFitBounds(MathUtil::Rectangle<int>& target_rect,
                                   MathUtil::Rectangle<int>& source_rect);

  std::tuple<float, float> ScaleToDisplayAspectRatio(int width, int height) const;
  void UpdateDrawRectangle();

  bool IsScaledEFB() const;
  float GetEFBScale() const;

  // Use this to upscale native EFB coordinates to IDEAL internal resolution
  int EFBToScaledX(int x) const;
  int EFBToScaledY(int y) const;

  // Floating point versions of the above - only use them if really necessary
  float EFBToScaledXf(float x) const;
  float EFBToScaledYf(float y) const;

  // Random utilities
  void SaveScreenshot(const std::string& filename, bool wait_for_completion);
  void DrawDebugText();
  void UpdateDebugTitle(const std::string& title) { m_debug_title_text = std::move(title); }
  void RenderText(const std::string& text, int left, int top, u32 color);

  virtual void ClearScreen(const MathUtil::Rectangle<int>& rc, bool colorEnable, bool alphaEnable,
                           bool zEnable, u32 color, u32 z);
  virtual void ReinterpretPixelData(EFBReinterpretType convtype);
  void RenderToXFB(u32 xfbAddr, const MathUtil::Rectangle<int>& sourceRc, u32 fbStride,
                   u32 fbHeight, float Gamma = 1.0f);

  virtual u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data);
  virtual void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points);

  virtual u16 BBoxRead(int index) = 0;
  virtual void BBoxWrite(int index, u16 value) = 0;
  virtual void BBoxFlush() {}

  virtual void Flush() {}
  virtual void WaitForGPUIdle() {}

  // Finish up the current frame, print some stats
  void Swap(u32 xfb_addr, u32 fb_width, u32 fb_stride, u32 fb_height, u64 ticks);

  // Draws the specified XFB buffer to the screen, performing any post-processing.
  // Assumes that the backbuffer has already been bound and cleared.
  virtual void RenderXFBToScreen(const MathUtil::Rectangle<int>& target_rc,
                                 const AbstractTexture* source_texture,
                                 const MathUtil::Rectangle<int>& source_rc);

  // Called when the configuration changes, and backend structures need to be updated.
  virtual void OnConfigChanged(u32 bits) {}

  PEControl::PixelFormat GetPrevPixelFormat() const { return m_prev_efb_format; }
  void StorePixelFormat(PEControl::PixelFormat new_format) { m_prev_efb_format = new_format; }
  bool EFBHasAlphaChannel() const;
  VideoCommon::PostProcessing* GetPostProcessor() const { return m_post_processor.get(); }
  // Final surface changing
  // This is called when the surface is resized (WX) or the window changes (Android).
  void ChangeSurface(void* new_surface_handle);
  void ResizeSurface();
  bool UseVertexDepthRange() const;

  virtual std::unique_ptr<VideoCommon::AsyncShaderCompiler> CreateAsyncShaderCompiler();

protected:
  // Bitmask containing information about which configuration has changed for the backend.
  enum ConfigChangeBits : u32
  {
    CONFIG_CHANGE_BIT_HOST_CONFIG = (1 << 0),
    CONFIG_CHANGE_BIT_MULTISAMPLES = (1 << 1),
    // CONFIG_CHANGE_BIT_STEREO_MODE = (1 << 2),
    CONFIG_CHANGE_BIT_TARGET_SIZE = (1 << 3),
    CONFIG_CHANGE_BIT_ANISOTROPY = (1 << 4),
    CONFIG_CHANGE_BIT_FORCE_TEXTURE_FILTERING = (1 << 5),
    CONFIG_CHANGE_BIT_VSYNC = (1 << 6),
    CONFIG_CHANGE_BIT_BBOX = (1 << 7)
  };

  std::tuple<int, int> CalculateTargetScale(int x, int y) const;
  bool CalculateTargetSize();

  void CheckForConfigChanges();

  void CheckFifoRecording();
  void RecordVideoMemory();

  AbstractFramebuffer* m_current_framebuffer = nullptr;
  const AbstractPipeline* m_current_pipeline = nullptr;

  Common::Flag m_screenshot_request;
  Common::Event m_screenshot_completed;
  std::mutex m_screenshot_lock;
  std::string m_screenshot_name;
  bool m_aspect_wide = false;

  // The framebuffer size
  int m_target_width = 1;
  int m_target_height = 1;

  // Backbuffer (window) size and render area
  int m_backbuffer_width = 0;
  int m_backbuffer_height = 0;
  float m_backbuffer_scale = 1.0f;
  AbstractTextureFormat m_backbuffer_format = AbstractTextureFormat::Undefined;
  MathUtil::Rectangle<int> m_target_rectangle = {};
  int m_frame_count = 0;

  std::unique_ptr<VideoCommon::PostProcessing> m_post_processor;

  void* m_new_surface_handle = nullptr;
  Common::Flag m_surface_changed;
  Common::Flag m_surface_resized;
  std::mutex m_swap_mutex;

private:
  void RunFrameDumps();
  std::tuple<int, int> CalculateOutputDimensions(int width, int height);

  PEControl::PixelFormat m_prev_efb_format = PEControl::INVALID_FMT;
  unsigned int m_efb_scale = 1;

  // These will be set on the first call to SetWindowSize.
  int m_last_window_request_width = 0;
  int m_last_window_request_height = 0;

  // frame dumping
  std::thread m_frame_dump_thread;
  Common::Event m_frame_dump_start;
  Common::Event m_frame_dump_done;
  Common::Flag m_frame_dump_thread_running;
  u32 m_frame_dump_image_counter = 0;
  bool m_frame_dump_frame_running = false;
  struct FrameDumpConfig
  {
    const u8* data;
    int width;
    int height;
    int stride;
    FrameDump::Frame state;
  } m_frame_dump_config;

  // Texture used for screenshot/frame dumping
  std::unique_ptr<AbstractTexture> m_frame_dump_render_texture;
  std::unique_ptr<AbstractFramebuffer> m_frame_dump_render_framebuffer;
  std::array<std::unique_ptr<AbstractStagingTexture>, 2> m_frame_dump_readback_textures;
  FrameDump::Frame m_last_frame_state;
  bool m_last_frame_exported = false;

  // Tracking of XFB textures so we don't render duplicate frames.
  u64 m_last_xfb_id = std::numeric_limits<u64>::max();

  // Note: Only used for auto-ir
  u32 m_last_xfb_width = 0;
  u32 m_last_xfb_height = 0;

  // fps text
  std::unique_ptr<VideoCommon::RasterFont> m_raster_font;
  std::string m_debug_title_text;

  // NOTE: The methods below are called on the framedumping thread.
  bool StartFrameDumpToFFMPEG(const FrameDumpConfig& config);
  void DumpFrameToFFMPEG(const FrameDumpConfig& config);
  void StopFrameDumpToFFMPEG();
  std::string GetFrameDumpNextImageFileName() const;
  bool StartFrameDumpToImage(const FrameDumpConfig& config);
  void DumpFrameToImage(const FrameDumpConfig& config);
  void ShutdownFrameDumping();

  bool IsFrameDumping();

  // Checks that the frame dump render texture exists and is the correct size.
  bool CheckFrameDumpRenderTexture(u32 target_width, u32 target_height);

  // Checks that the frame dump readback texture exists and is the correct size.
  bool CheckFrameDumpReadbackTexture(u32 target_width, u32 target_height);

  // Fills the frame dump staging texture with the current XFB texture.
  void DumpCurrentFrame(const AbstractTexture* src_texture,
                        const MathUtil::Rectangle<int>& src_rect, u64 ticks);

  // Asynchronously encodes the specified pointer of frame data to the frame dump.
  void DumpFrameData(const u8* data, int w, int h, int stride, const FrameDump::Frame& state);

  // Ensures all rendered frames are queued for encoding.
  void FlushFrameDump();

  // Ensures all encoded frames have been written to the output file.
  void FinishFrameData();
};

extern std::unique_ptr<Renderer> g_renderer;
