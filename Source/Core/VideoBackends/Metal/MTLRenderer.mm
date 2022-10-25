// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLRenderer.h"

#include "VideoBackends/Metal/MTLBoundingBox.h"
#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLPipeline.h"
#include "VideoBackends/Metal/MTLStateTracker.h"
#include "VideoBackends/Metal/MTLTexture.h"
#include "VideoBackends/Metal/MTLUtil.h"
#include "VideoBackends/Metal/MTLVertexFormat.h"
#include "VideoBackends/Metal/MTLVertexManager.h"

#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/VideoBackendBase.h"

Metal::Renderer::Renderer(MRCOwned<CAMetalLayer*> layer, int width, int height, float layer_scale)
    : ::Renderer(width, height, layer_scale, Util::ToAbstract([layer pixelFormat])),
      m_layer(std::move(layer))
{
  UpdateActiveConfig();
  [m_layer setDisplaySyncEnabled:g_ActiveConfig.bVSyncActive];
}

Metal::Renderer::~Renderer() = default;

bool Metal::Renderer::IsHeadless() const
{
  return m_layer == nullptr;
}

bool Metal::Renderer::Initialize()
{
  if (!::Renderer::Initialize())
    return false;
  SetupSurface();
  g_state_tracker->FlushEncoders();
  return true;
}

// MARK: Texture Creation

std::unique_ptr<AbstractTexture> Metal::Renderer::CreateTexture(const TextureConfig& config,
                                                                std::string_view name)
{
  @autoreleasepool
  {
    MRCOwned<MTLTextureDescriptor*> desc = MRCTransfer([MTLTextureDescriptor new]);
    [desc setTextureType:config.samples > 1 ? MTLTextureType2DMultisampleArray :
                                              MTLTextureType2DArray];
    [desc setPixelFormat:Util::FromAbstract(config.format)];
    [desc setWidth:config.width];
    [desc setHeight:config.height];
    [desc setMipmapLevelCount:config.levels];
    [desc setArrayLength:config.layers];
    [desc setSampleCount:config.samples];
    [desc setStorageMode:MTLStorageModePrivate];
    MTLTextureUsage usage = MTLTextureUsageShaderRead;
    if (config.IsRenderTarget())
      usage |= MTLTextureUsageRenderTarget;
    if (config.IsComputeImage())
      usage |= MTLTextureUsageShaderWrite;
    [desc setUsage:usage];
    id<MTLTexture> texture = [g_device newTextureWithDescriptor:desc];
    if (!texture)
      return nullptr;

    if (name.empty())
      [texture setLabel:[NSString stringWithFormat:@"Texture %d", m_texture_counter++]];
    else
      [texture setLabel:MRCTransfer([[NSString alloc] initWithBytes:name.data()
                                                             length:name.size()
                                                           encoding:NSUTF8StringEncoding])];
    return std::make_unique<Texture>(MRCTransfer(texture), config);
  }
}

std::unique_ptr<AbstractStagingTexture>
Metal::Renderer::CreateStagingTexture(StagingTextureType type, const TextureConfig& config)
{
  @autoreleasepool
  {
    const size_t stride = config.GetStride();
    const size_t buffer_size = stride * static_cast<size_t>(config.height);

    MTLResourceOptions options = MTLStorageModeShared;
    if (type == StagingTextureType::Upload)
      options |= MTLResourceCPUCacheModeWriteCombined;

    id<MTLBuffer> buffer = [g_device newBufferWithLength:buffer_size options:options];
    if (!buffer)
      return nullptr;
    [buffer
        setLabel:[NSString stringWithFormat:@"Staging Texture %d", m_staging_texture_counter++]];
    return std::make_unique<StagingTexture>(MRCTransfer(buffer), type, config);
  }
}

std::unique_ptr<AbstractFramebuffer>
Metal::Renderer::CreateFramebuffer(AbstractTexture* color_attachment,
                                   AbstractTexture* depth_attachment)
{
  AbstractTexture* const either_attachment = color_attachment ? color_attachment : depth_attachment;
  return std::make_unique<Framebuffer>(
      color_attachment, depth_attachment, either_attachment->GetWidth(),
      either_attachment->GetHeight(), either_attachment->GetLayers(),
      either_attachment->GetSamples());
}

// MARK: Pipeline Creation

std::unique_ptr<AbstractShader> Metal::Renderer::CreateShaderFromSource(ShaderStage stage,
                                                                        std::string_view source,
                                                                        std::string_view name)
{
  std::optional<std::string> msl = Util::TranslateShaderToMSL(stage, source);
  if (!msl.has_value())
  {
    PanicAlertFmt("Failed to convert shader {} to MSL", name);
    return nullptr;
  }

  return CreateShaderFromMSL(stage, std::move(*msl), source, name);
}

std::unique_ptr<AbstractShader> Metal::Renderer::CreateShaderFromBinary(ShaderStage stage,
                                                                        const void* data,
                                                                        size_t length,
                                                                        std::string_view name)
{
  return CreateShaderFromMSL(stage, std::string(static_cast<const char*>(data), length), {}, name);
}

// clang-format off

static const char* StageFilename(ShaderStage stage)
{
  switch (stage)
  {
  case ShaderStage::Vertex:   return "vs";
  case ShaderStage::Geometry: return "gs";
  case ShaderStage::Pixel:    return "ps";
  case ShaderStage::Compute:  return "cs";
  }
}

static NSString* GenericShaderName(ShaderStage stage)
{
  switch (stage)
  {
  case ShaderStage::Vertex:   return @"Vertex shader %d";
  case ShaderStage::Geometry: return @"Geometry shader %d";
  case ShaderStage::Pixel:    return @"Pixel shader %d";
  case ShaderStage::Compute:  return @"Compute shader %d";
  }
}

// clang-format on

std::unique_ptr<AbstractShader> Metal::Renderer::CreateShaderFromMSL(ShaderStage stage,
                                                                     std::string msl,
                                                                     std::string_view glsl,
                                                                     std::string_view name)
{
  @autoreleasepool
  {
    NSError* err = nullptr;
    auto DumpBadShader = [&](std::string_view msg) {
      static int counter = 0;
      std::string filename = VideoBackendBase::BadShaderFilename(StageFilename(stage), counter++);
      std::ofstream stream(filename);
      if (stream.good())
      {
        stream << msl << std::endl;
        stream << "/*" << std::endl;
        stream << msg << std::endl;
        stream << "Error:" << std::endl;
        stream << [[err localizedDescription] UTF8String] << std::endl;
        if (!glsl.empty())
        {
          stream << "Original GLSL:" << std::endl;
          stream << glsl << std::endl;
        }
        else
        {
          stream << "Shader was created with cached MSL so no GLSL is available." << std::endl;
        }
      }

      stream << std::endl;
      stream << "Dolphin Version: " << Common::GetScmRevStr() << std::endl;
      stream << "Video Backend: " << g_video_backend->GetDisplayName() << std::endl;
      stream << "*/" << std::endl;
      stream.close();

      PanicAlertFmt("{} (written to {})\n", msg, filename);
    };

    auto lib = MRCTransfer([g_device newLibraryWithSource:[NSString stringWithUTF8String:msl.data()]
                                                  options:nil
                                                    error:&err]);
    if (err)
    {
      DumpBadShader(fmt::format("Failed to compile {}", name));
      return nullptr;
    }
    auto fn = MRCTransfer([lib newFunctionWithName:@"main0"]);
    if (!fn)
    {
      DumpBadShader(fmt::format("Shader {} is missing its main0 function", name));
      return nullptr;
    }
    if (!name.empty())
      [fn setLabel:MRCTransfer([[NSString alloc] initWithBytes:name.data()
                                                        length:name.size()
                                                      encoding:NSUTF8StringEncoding])];
    else
      [fn setLabel:[NSString stringWithFormat:GenericShaderName(stage),
                                              m_shader_counter[static_cast<u32>(stage)]++]];
    [lib setLabel:[fn label]];
    if (stage == ShaderStage::Compute)
    {
      MTLComputePipelineReflection* reflection = nullptr;
      auto desc = [MTLComputePipelineDescriptor new];
      [desc setComputeFunction:fn];
      [desc setLabel:[fn label]];
      MRCOwned<id<MTLComputePipelineState>> pipeline =
          MRCTransfer([g_device newComputePipelineStateWithDescriptor:desc
                                                              options:MTLPipelineOptionArgumentInfo
                                                           reflection:&reflection
                                                                error:&err]);
      if (err)
      {
        DumpBadShader(fmt::format("Failed to compile compute pipeline {}", name));
        return nullptr;
      }
      return std::make_unique<ComputePipeline>(stage, reflection, std::move(msl), std::move(fn),
                                               std::move(pipeline));
    }
    return std::make_unique<Shader>(stage, std::move(msl), std::move(fn));
  }
}

std::unique_ptr<NativeVertexFormat>
Metal::Renderer::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
  @autoreleasepool
  {
    return std::make_unique<VertexFormat>(vtx_decl);
  }
}

std::unique_ptr<AbstractPipeline>
Metal::Renderer::CreatePipeline(const AbstractPipelineConfig& config, const void* cache_data,
                                size_t cache_data_length)
{
  return g_object_cache->CreatePipeline(config);
}

void Metal::Renderer::Flush()
{
  @autoreleasepool
  {
    g_state_tracker->FlushEncoders();
  }
}

void Metal::Renderer::WaitForGPUIdle()
{
  @autoreleasepool
  {
    g_state_tracker->FlushEncoders();
    g_state_tracker->WaitForFlushedEncoders();
  }
}

void Metal::Renderer::OnConfigChanged(u32 bits)
{
  if (bits & CONFIG_CHANGE_BIT_VSYNC)
    [m_layer setDisplaySyncEnabled:g_ActiveConfig.bVSyncActive];

  if (bits & CONFIG_CHANGE_BIT_ANISOTROPY)
  {
    g_object_cache->ReloadSamplers();
    g_state_tracker->ReloadSamplers();
  }
}

void Metal::Renderer::ClearScreen(const MathUtil::Rectangle<int>& rc, bool color_enable,
                                  bool alpha_enable, bool z_enable, u32 color, u32 z)
{
  MathUtil::Rectangle<int> target_rc = Renderer::ConvertEFBRectangle(rc);
  target_rc.ClampUL(0, 0, m_target_width, m_target_height);

  // All Metal render passes are fullscreen, so we can only run a fast clear if the target is too
  if (target_rc == MathUtil::Rectangle<int>(0, 0, m_target_width, m_target_height))
  {
    // Determine whether the EFB has an alpha channel. If it doesn't, we can clear the alpha
    // channel to 0xFF. This hopefully allows us to use the fast path in most cases.
    if (bpmem.zcontrol.pixel_format == PixelFormat::RGB565_Z16 ||
        bpmem.zcontrol.pixel_format == PixelFormat::RGB8_Z24 ||
        bpmem.zcontrol.pixel_format == PixelFormat::Z24)
    {
      // Force alpha writes, and clear the alpha channel. This is different from the other backends,
      // where the existing values of the alpha channel are preserved.
      alpha_enable = true;
      color &= 0x00FFFFFF;
    }

    bool c_ok = (color_enable && alpha_enable) ||
                g_state_tracker->GetCurrentFramebuffer()->GetColorFormat() ==
                    AbstractTextureFormat::Undefined;
    bool z_ok = z_enable || g_state_tracker->GetCurrentFramebuffer()->GetDepthFormat() ==
                                AbstractTextureFormat::Undefined;
    if (c_ok && z_ok)
    {
      @autoreleasepool
      {
        // clang-format off
        MTLClearColor clear_color = MTLClearColorMake(
            static_cast<double>((color >> 16) & 0xFF) / 255.0,
            static_cast<double>((color >>  8) & 0xFF) / 255.0,
            static_cast<double>((color >>  0) & 0xFF) / 255.0,
            static_cast<double>((color >> 24) & 0xFF) / 255.0);
        // clang-format on
        float z_normalized = static_cast<float>(z & 0xFFFFFF) / 16777216.0f;
        if (!g_Config.backend_info.bSupportsReversedDepthRange)
          z_normalized = 1.f - z_normalized;
        g_state_tracker->BeginClearRenderPass(clear_color, z_normalized);
        return;
      }
    }
  }

  g_state_tracker->EnableEncoderLabel(false);
  g_framebuffer_manager->ClearEFB(rc, color_enable, alpha_enable, z_enable, color, z);
  g_state_tracker->EnableEncoderLabel(true);
}

void Metal::Renderer::SetPipeline(const AbstractPipeline* pipeline)
{
  g_state_tracker->SetPipeline(static_cast<const Pipeline*>(pipeline));
}

void Metal::Renderer::SetFramebuffer(AbstractFramebuffer* framebuffer)
{
  // Shouldn't be bound as a texture.
  if (AbstractTexture* color = framebuffer->GetColorAttachment())
    g_state_tracker->UnbindTexture(static_cast<Texture*>(color)->GetMTLTexture());
  if (AbstractTexture* depth = framebuffer->GetDepthAttachment())
    g_state_tracker->UnbindTexture(static_cast<Texture*>(depth)->GetMTLTexture());

  m_current_framebuffer = framebuffer;
  g_state_tracker->SetCurrentFramebuffer(static_cast<Framebuffer*>(framebuffer));
}

void Metal::Renderer::SetAndDiscardFramebuffer(AbstractFramebuffer* framebuffer)
{
  @autoreleasepool
  {
    SetFramebuffer(framebuffer);
    g_state_tracker->BeginRenderPass(MTLLoadActionDontCare);
  }
}

void Metal::Renderer::SetAndClearFramebuffer(AbstractFramebuffer* framebuffer,
                                             const ClearColor& color_value, float depth_value)
{
  @autoreleasepool
  {
    SetFramebuffer(framebuffer);
    MTLClearColor color =
        MTLClearColorMake(color_value[0], color_value[1], color_value[2], color_value[3]);
    g_state_tracker->BeginClearRenderPass(color, depth_value);
  }
}

void Metal::Renderer::SetScissorRect(const MathUtil::Rectangle<int>& rc)
{
  g_state_tracker->SetScissor(rc);
}

void Metal::Renderer::SetTexture(u32 index, const AbstractTexture* texture)
{
  g_state_tracker->SetTexture(
      index, texture ? static_cast<const Texture*>(texture)->GetMTLTexture() : nullptr);
}

void Metal::Renderer::SetSamplerState(u32 index, const SamplerState& state)
{
  g_state_tracker->SetSampler(index, state);
}

void Metal::Renderer::SetComputeImageTexture(AbstractTexture* texture, bool read, bool write)
{
  g_state_tracker->SetComputeTexture(static_cast<const Texture*>(texture));
}

void Metal::Renderer::UnbindTexture(const AbstractTexture* texture)
{
  g_state_tracker->UnbindTexture(static_cast<const Texture*>(texture)->GetMTLTexture());
}

void Metal::Renderer::SetViewport(float x, float y, float width, float height, float near_depth,
                                  float far_depth)
{
  g_state_tracker->SetViewport(x, y, width, height, near_depth, far_depth);
}

void Metal::Renderer::Draw(u32 base_vertex, u32 num_vertices)
{
  @autoreleasepool
  {
    g_state_tracker->Draw(base_vertex, num_vertices);
  }
}

void Metal::Renderer::DrawIndexed(u32 base_index, u32 num_indices, u32 base_vertex)
{
  @autoreleasepool
  {
    g_state_tracker->DrawIndexed(base_index, num_indices, base_vertex);
  }
}

void Metal::Renderer::DispatchComputeShader(const AbstractShader* shader,  //
                                            u32 groupsize_x, u32 groupsize_y, u32 groupsize_z,
                                            u32 groups_x, u32 groups_y, u32 groups_z)
{
  @autoreleasepool
  {
    g_state_tracker->SetPipeline(static_cast<const ComputePipeline*>(shader));
    g_state_tracker->DispatchComputeShader(groupsize_x, groupsize_y, groupsize_z,  //
                                           groups_x, groups_y, groups_z);
  }
}

void Metal::Renderer::BindBackbuffer(const ClearColor& clear_color)
{
  @autoreleasepool
  {
    CheckForSurfaceChange();
    CheckForSurfaceResize();
    m_drawable = MRCRetain([m_layer nextDrawable]);
    m_bb_texture->SetMTLTexture(MRCRetain([m_drawable texture]));
    SetAndClearFramebuffer(m_backbuffer.get(), clear_color);
  }
}

void Metal::Renderer::PresentBackbuffer()
{
  @autoreleasepool
  {
    g_state_tracker->EndRenderPass();
    if (m_drawable)
    {
      // PresentDrawable refuses to allow Dolphin to present faster than the display's refresh rate
      // when windowed (or fullscreen with vsync enabled, but that's more understandable).
      // On the other hand, it helps Xcode's GPU captures start and stop on frame boundaries
      // which is convenient.  Put it here as a default-off config, which we can override in Xcode.
      if (g_ActiveConfig.bUsePresentDrawable)
        [g_state_tracker->GetRenderCmdBuf() presentDrawable:m_drawable];
      else
        [g_state_tracker->GetRenderCmdBuf()
            addScheduledHandler:[drawable = std::move(m_drawable)](id) { [drawable present]; }];
      m_bb_texture->SetMTLTexture(nullptr);
      m_drawable = nullptr;
    }
    g_state_tracker->FlushEncoders();
  }
}

std::unique_ptr<::BoundingBox> Metal::Renderer::CreateBoundingBox() const
{
  return std::make_unique<BoundingBox>();
}

void Metal::Renderer::CheckForSurfaceChange()
{
  if (!m_surface_changed.TestAndClear())
    return;
  m_layer = MRCRetain(static_cast<CAMetalLayer*>(m_new_surface_handle));
  m_new_surface_handle = nullptr;
  SetupSurface();
}

void Metal::Renderer::CheckForSurfaceResize()
{
  if (!m_surface_resized.TestAndClear())
    return;
  SetupSurface();
}

void Metal::Renderer::SetupSurface()
{
  CGSize size = [m_layer bounds].size;
  // TODO: Update m_backbuffer_scale (need to make doing that not break everything)
  const float backbuffer_scale = [m_layer contentsScale];
  size.width *= backbuffer_scale;
  size.height *= backbuffer_scale;
  [m_layer setDrawableSize:size];
  m_backbuffer_width = size.width;
  m_backbuffer_height = size.height;
  TextureConfig cfg(m_backbuffer_width, m_backbuffer_height, 1, 1, 1, m_backbuffer_format,
                    AbstractTextureFlag_RenderTarget);
  m_bb_texture = std::make_unique<Texture>(nullptr, cfg);
  m_backbuffer = std::make_unique<Framebuffer>(m_bb_texture.get(), nullptr,  //
                                               m_backbuffer_width, m_backbuffer_height, 1, 1);
}
