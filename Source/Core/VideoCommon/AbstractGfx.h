// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/HookableEvent.h"
#include "Common/MathUtil.h"

#include "VideoCommon/RenderState.h"

#include <array>
#include <memory>

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;
class AbstractStagingTexture;
class NativeVertexFormat;
struct ComputePipelineConfig;
struct AbstractPipelineConfig;
struct PortableVertexDeclaration;
struct TextureConfig;
enum class AbstractTextureFormat : u32;
enum class ShaderStage;
enum class StagingTextureType;

struct SurfaceInfo
{
  u32 width = 0;
  u32 height = 0;
  float scale = 0.0f;
  AbstractTextureFormat format = {};
};

namespace VideoCommon
{
class AsyncShaderCompiler;
}

using ClearColor = std::array<float, 4>;

// AbstractGfx is the root of Dolphin's Graphics API abstraction layer.
//
// Abstract knows nothing about the internals of the GameCube/Wii, that is all handled elsewhere in
// VideoCommon.

class AbstractGfx
{
public:
  AbstractGfx();
  virtual ~AbstractGfx() = default;

  virtual bool IsHeadless() const = 0;

  // Does the backend support drawing a UI or doing post-processing
  virtual bool SupportsUtilityDrawing() const { return true; }

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

  virtual void ClearRegion(const MathUtil::Rectangle<int>& target_rc, bool colorEnable,
                           bool alphaEnable, bool zEnable, u32 color, u32 z);

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

  virtual void Flush() {}
  virtual void WaitForGPUIdle() {}

  // For opengl's glDrawBuffer
  virtual void SelectLeftBuffer() {}
  virtual void SelectRightBuffer() {}
  virtual void SelectMainBuffer() {}

  // A simple presentation fallback, only used by video software
  virtual void ShowImage(const AbstractTexture* source_texture,
                         const MathUtil::Rectangle<int>& source_rc)
  {
  }

  virtual std::unique_ptr<VideoCommon::AsyncShaderCompiler> CreateAsyncShaderCompiler();

  // Called when the configuration changes, and backend structures need to be updated.
  virtual void OnConfigChanged(u32 changed_bits);

  // Returns true if a layer-expanding geometry shader should be used when rendering the user
  // interface and final XFB.
  bool UseGeometryShaderForUI() const;

  // Returns info about the main surface (aka backbuffer)
  virtual SurfaceInfo GetSurfaceInfo() const = 0;

protected:
  AbstractFramebuffer* m_current_framebuffer = nullptr;
  const AbstractPipeline* m_current_pipeline = nullptr;

private:
  Common::EventHook m_config_changed;
};

extern std::unique_ptr<AbstractGfx> g_gfx;
