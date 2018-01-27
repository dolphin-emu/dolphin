// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/FramebufferManager.h"

#include <algorithm>
#include <cstddef>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

#include "Core/HW/Memmap.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/TextureConverter.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
// Maximum number of pixels poked in one batch * 6
#if defined(_MSC_VER) && _MSC_VER <= 1800
enum
{
  MAX_POKE_VERTICES = 8192,
  POKE_VERTEX_BUFFER_SIZE = 8 * 1024 * 1024
};
#else
constexpr size_t MAX_POKE_VERTICES = 8192;
constexpr size_t POKE_VERTEX_BUFFER_SIZE = 8 * 1024 * 1024;
#endif

FramebufferManager::FramebufferManager()
{
}

FramebufferManager::~FramebufferManager()
{
  DestroyEFBFramebuffer();
  DestroyEFBRenderPass();

  DestroyConversionShaders();

  DestroyReadbackFramebuffer();
  DestroyReadbackTextures();
  DestroyReadbackShaders();
  DestroyReadbackRenderPasses();

  DestroyPokeVertexBuffer();
  DestroyPokeShaders();
}

FramebufferManager* FramebufferManager::GetInstance()
{
  return static_cast<FramebufferManager*>(g_framebuffer_manager.get());
}

u32 FramebufferManager::GetEFBWidth() const
{
  return m_efb_color_texture->GetWidth();
}

u32 FramebufferManager::GetEFBHeight() const
{
  return m_efb_color_texture->GetHeight();
}

u32 FramebufferManager::GetEFBLayers() const
{
  return m_efb_color_texture->GetLayers();
}

VkSampleCountFlagBits FramebufferManager::GetEFBSamples() const
{
  return m_efb_color_texture->GetSamples();
}

MultisamplingState FramebufferManager::GetEFBMultisamplingState() const
{
  MultisamplingState ms = {};
  ms.per_sample_shading = g_ActiveConfig.MultisamplingEnabled() && g_ActiveConfig.bSSAA;
  ms.samples = static_cast<u32>(GetEFBSamples());
  return ms;
}

std::pair<u32, u32> FramebufferManager::GetTargetSize() const
{
  return std::make_pair(GetEFBWidth(), GetEFBHeight());
}

bool FramebufferManager::Initialize()
{
  if (!CreateEFBRenderPass())
  {
    PanicAlert("Failed to create EFB render pass");
    return false;
  }
  if (!CreateEFBFramebuffer())
  {
    PanicAlert("Failed to create EFB textures");
    return false;
  }

  if (!CompileConversionShaders())
  {
    PanicAlert("Failed to compile EFB shaders");
    return false;
  }

  if (!CreateReadbackRenderPasses())
  {
    PanicAlert("Failed to create readback render passes");
    return false;
  }
  if (!CompileReadbackShaders())
  {
    PanicAlert("Failed to compile readback shaders");
    return false;
  }
  if (!CreateReadbackTextures())
  {
    PanicAlert("Failed to create readback textures");
    return false;
  }
  if (!CreateReadbackFramebuffer())
  {
    PanicAlert("Failed to create readback framebuffer");
    return false;
  }

  CreatePokeVertexFormat();
  if (!CreatePokeVertexBuffer())
  {
    PanicAlert("Failed to create poke vertex buffer");
    return false;
  }

  if (!CompilePokeShaders())
  {
    PanicAlert("Failed to compile poke shaders");
    return false;
  }

  return true;
}

bool FramebufferManager::CreateEFBRenderPass()
{
  VkSampleCountFlagBits samples = static_cast<VkSampleCountFlagBits>(g_ActiveConfig.iMultisamples);

  // render pass for rendering to the efb
  VkAttachmentDescription attachments[] = {
      {0, EFB_COLOR_TEXTURE_FORMAT, samples, VK_ATTACHMENT_LOAD_OP_LOAD,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
      {0, EFB_DEPTH_TEXTURE_FORMAT, samples, VK_ATTACHMENT_LOAD_OP_LOAD,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference depth_attachment_reference = {
      1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

  VkSubpassDescription subpass_description = {
      0,       VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, color_attachment_references,
      nullptr, &depth_attachment_reference,     0, nullptr};

  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      static_cast<u32>(ArraySize(attachments)),
                                      attachments,
                                      1,
                                      &subpass_description,
                                      0,
                                      nullptr};

  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &pass_info, nullptr,
                                    &m_efb_load_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (EFB) failed: ");
    return false;
  }

  // render pass for clearing color/depth on load, as opposed to loading it
  attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &pass_info, nullptr,
                           &m_efb_clear_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (EFB) failed: ");
    return false;
  }

  // render pass for resolving depth, since we can't do it with vkCmdResolveImage
  if (g_ActiveConfig.MultisamplingEnabled())
  {
    VkAttachmentDescription resolve_attachment = {0,
                                                  EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT,
                                                  VK_SAMPLE_COUNT_1_BIT,
                                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_STORE,
                                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    subpass_description.pDepthStencilAttachment = nullptr;
    pass_info.pAttachments = &resolve_attachment;
    pass_info.attachmentCount = 1;
    res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &pass_info, nullptr,
                             &m_depth_resolve_render_pass);

    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateRenderPass (EFB depth resolve) failed: ");
      return false;
    }
  }

  return true;
}

void FramebufferManager::DestroyEFBRenderPass()
{
  if (m_efb_load_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_efb_load_render_pass, nullptr);
    m_efb_load_render_pass = VK_NULL_HANDLE;
  }

  if (m_efb_clear_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_efb_clear_render_pass, nullptr);
    m_efb_clear_render_pass = VK_NULL_HANDLE;
  }

  if (m_depth_resolve_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_depth_resolve_render_pass, nullptr);
    m_depth_resolve_render_pass = VK_NULL_HANDLE;
  }
}

bool FramebufferManager::CreateEFBFramebuffer()
{
  u32 efb_width = static_cast<u32>(std::max(g_renderer->GetTargetWidth(), 1));
  u32 efb_height = static_cast<u32>(std::max(g_renderer->GetTargetHeight(), 1));
  u32 efb_layers = (g_ActiveConfig.iStereoMode != STEREO_OFF) ? 2 : 1;
  VkSampleCountFlagBits efb_samples =
      static_cast<VkSampleCountFlagBits>(g_ActiveConfig.iMultisamples);
  INFO_LOG(VIDEO, "EFB size: %ux%ux%u", efb_width, efb_height, efb_layers);

  // Update the static variable in the base class. Why does this even exist?
  FramebufferManagerBase::m_EFBLayers = g_ActiveConfig.iMultisamples;

  // Allocate EFB render targets
  m_efb_color_texture =
      Texture2D::Create(efb_width, efb_height, 1, efb_layers, EFB_COLOR_TEXTURE_FORMAT, efb_samples,
                        VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  // We need a second texture to swap with for changing pixel formats
  m_efb_convert_color_texture =
      Texture2D::Create(efb_width, efb_height, 1, efb_layers, EFB_COLOR_TEXTURE_FORMAT, efb_samples,
                        VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  m_efb_depth_texture = Texture2D::Create(
      efb_width, efb_height, 1, efb_layers, EFB_DEPTH_TEXTURE_FORMAT, efb_samples,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  if (!m_efb_color_texture || !m_efb_convert_color_texture || !m_efb_depth_texture)
    return false;

  // Create resolved textures if MSAA is on
  if (g_ActiveConfig.MultisamplingEnabled())
  {
    m_efb_resolve_color_texture = Texture2D::Create(
        efb_width, efb_height, 1, efb_layers, EFB_COLOR_TEXTURE_FORMAT, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    m_efb_resolve_depth_texture = Texture2D::Create(
        efb_width, efb_height, 1, efb_layers, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT,
        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    if (!m_efb_resolve_color_texture || !m_efb_resolve_depth_texture)
      return false;

    VkImageView attachment = m_efb_resolve_depth_texture->GetView();
    VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                                nullptr,
                                                0,
                                                m_depth_resolve_render_pass,
                                                1,
                                                &attachment,
                                                efb_width,
                                                efb_height,
                                                efb_layers};

    VkResult res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                                       &m_depth_resolve_framebuffer);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
      return false;
    }
  }

  VkImageView framebuffer_attachments[] = {
      m_efb_color_texture->GetView(), m_efb_depth_texture->GetView(),
  };

  VkFramebufferCreateInfo framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                              nullptr,
                                              0,
                                              m_efb_load_render_pass,
                                              static_cast<u32>(ArraySize(framebuffer_attachments)),
                                              framebuffer_attachments,
                                              efb_width,
                                              efb_height,
                                              efb_layers};

  VkResult res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                                     &m_efb_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  // Create second framebuffer for format conversions
  framebuffer_attachments[0] = m_efb_convert_color_texture->GetView();
  res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                            &m_efb_convert_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  // Transition to state that can be used to clear
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  m_efb_convert_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Clear the contents of the buffers.
  static const VkClearColorValue clear_color = {{0.0f, 0.0f, 0.0f, 0.0f}};
  static const VkClearDepthStencilValue clear_depth = {0.0f, 0};
  VkImageSubresourceRange clear_color_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, efb_layers};
  VkImageSubresourceRange clear_depth_range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, efb_layers};
  vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                       m_efb_color_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &clear_color, 1, &clear_color_range);
  vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                       m_efb_convert_color_texture->GetImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1, &clear_color_range);
  vkCmdClearDepthStencilImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                              m_efb_depth_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              &clear_depth, 1, &clear_depth_range);

  // Transition to color attachment state ready for rendering.
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  return true;
}

void FramebufferManager::DestroyEFBFramebuffer()
{
  if (m_efb_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_efb_framebuffer, nullptr);
    m_efb_framebuffer = VK_NULL_HANDLE;
  }

  if (m_efb_convert_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_efb_convert_framebuffer, nullptr);
    m_efb_convert_framebuffer = VK_NULL_HANDLE;
  }

  if (m_depth_resolve_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_depth_resolve_framebuffer, nullptr);
    m_depth_resolve_framebuffer = VK_NULL_HANDLE;
  }

  m_efb_color_texture.reset();
  m_efb_convert_color_texture.reset();
  m_efb_depth_texture.reset();
  m_efb_resolve_color_texture.reset();
  m_efb_resolve_depth_texture.reset();
}

void FramebufferManager::ResizeEFBTextures()
{
  DestroyEFBFramebuffer();
  if (!CreateEFBFramebuffer())
    PanicAlert("Failed to create EFB textures");
}

void FramebufferManager::RecreateRenderPass()
{
  DestroyEFBRenderPass();

  if (!CreateEFBRenderPass())
    PanicAlert("Failed to create EFB render pass");
}

void FramebufferManager::RecompileShaders()
{
  DestroyConversionShaders();

  if (!CompileConversionShaders())
    PanicAlert("Failed to compile EFB shaders");

  DestroyReadbackShaders();
  if (!CompileReadbackShaders())
    PanicAlert("Failed to compile readback shaders");
}

void FramebufferManager::ReinterpretPixelData(int convtype)
{
  VkShaderModule pixel_shader = VK_NULL_HANDLE;
  if (convtype == 0)
  {
    pixel_shader = m_ps_rgb8_to_rgba6;
  }
  else if (convtype == 2)
  {
    pixel_shader = m_ps_rgba6_to_rgb8;
  }
  else
  {
    ERROR_LOG(VIDEO, "Unhandled reinterpret pixel data %d", convtype);
    return;
  }

  // Transition EFB color buffer to shader resource, and the convert buffer to color attachment.
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  m_efb_convert_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                         m_efb_load_render_pass, g_shader_cache->GetScreenQuadVertexShader(),
                         g_shader_cache->GetScreenQuadGeometryShader(), pixel_shader);

  VkRect2D region = {{0, 0}, {GetEFBWidth(), GetEFBHeight()}};
  draw.SetMultisamplingState(GetEFBMultisamplingState());
  draw.BeginRenderPass(m_efb_convert_framebuffer, region);
  draw.SetPSSampler(0, m_efb_color_texture->GetView(), g_object_cache->GetPointSampler());
  draw.SetViewportAndScissor(0, 0, GetEFBWidth(), GetEFBHeight());
  draw.DrawWithoutVertexBuffer(4);
  draw.EndRenderPass();

  // Swap EFB texture pointers
  std::swap(m_efb_color_texture, m_efb_convert_color_texture);
  std::swap(m_efb_framebuffer, m_efb_convert_framebuffer);
}

Texture2D* FramebufferManager::ResolveEFBColorTexture(const VkRect2D& region)
{
  // Return the normal EFB texture if multisampling is off.
  if (GetEFBSamples() == VK_SAMPLE_COUNT_1_BIT)
    return m_efb_color_texture.get();

  // Can't resolve within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  // It's not valid to resolve out-of-bounds coordinates.
  // Ensuring the region is within the image is the caller's responsibility.
  _assert_(region.offset.x >= 0 && region.offset.y >= 0 &&
           (static_cast<u32>(region.offset.x) + region.extent.width) <= GetEFBWidth() &&
           (static_cast<u32>(region.offset.y) + region.extent.height) <= GetEFBHeight());

  // Resolving is considered to be a transfer operation.
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_efb_resolve_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Resolve to our already-created texture.
  VkImageResolve resolve = {
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, GetEFBLayers()},  // VkImageSubresourceLayers srcSubresource
      {region.offset.x, region.offset.y, 0},              // VkOffset3D                  srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, GetEFBLayers()},  // VkImageSubresourceLayers dstSubresource
      {region.offset.x, region.offset.y, 0},              // VkOffset3D                  dstOffset
      {region.extent.width, region.extent.height, GetEFBLayers()}  // VkExtent3D extent
  };
  vkCmdResolveImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                    m_efb_color_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    m_efb_resolve_color_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &resolve);

  // Restore MSAA texture ready for rendering again
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  return m_efb_resolve_color_texture.get();
}

Texture2D* FramebufferManager::ResolveEFBDepthTexture(const VkRect2D& region)
{
  // Return the normal EFB texture if multisampling is off.
  if (GetEFBSamples() == VK_SAMPLE_COUNT_1_BIT)
    return m_efb_depth_texture.get();

  // Can't resolve within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Draw using resolve shader to write the minimum depth of all samples to the resolve texture.
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                         m_depth_resolve_render_pass, g_shader_cache->GetScreenQuadVertexShader(),
                         g_shader_cache->GetScreenQuadGeometryShader(), m_ps_depth_resolve);
  draw.BeginRenderPass(m_depth_resolve_framebuffer, region);
  draw.SetPSSampler(0, m_efb_depth_texture->GetView(), g_object_cache->GetPointSampler());
  draw.SetViewportAndScissor(region.offset.x, region.offset.y, region.extent.width,
                             region.extent.height);
  draw.DrawWithoutVertexBuffer(4);
  draw.EndRenderPass();

  // Restore MSAA texture ready for rendering again
  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  // Render pass transitions to shader resource.
  m_efb_resolve_depth_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return m_efb_resolve_depth_texture.get();
}

bool FramebufferManager::CompileConversionShaders()
{
  static const char RGB8_TO_RGBA6_SHADER_SOURCE[] = R"(
    #if MSAA_ENABLED
      SAMPLER_BINDING(0) uniform sampler2DMSArray samp0;
    #else
      SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    #endif
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      int layer = 0;
      #if EFB_LAYERS > 1
        layer = int(uv0.z);
      #endif

      ivec3 coords = ivec3(gl_FragCoord.xy, layer);

      vec4 val;
      #if !MSAA_ENABLED
        // No MSAA - just load the first (and only) sample
        val = texelFetch(samp0, coords, 0);
      #elif SSAA_ENABLED
        // Sample shading, shader runs once per sample
        val = texelFetch(samp0, coords, gl_SampleID);
      #else
        // MSAA without sample shading, average out all samples.
        val = vec4(0, 0, 0, 0);
        for (int i = 0; i < MSAA_SAMPLES; i++)
          val += texelFetch(samp0, coords, i);
        val /= float(MSAA_SAMPLES);
      #endif

      ivec4 src8 = ivec4(round(val * 255.f));
      ivec4 dst6;
      dst6.r = src8.r >> 2;
      dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);
      dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);
      dst6.a = src8.b & 0x3F;

      ocol0 = float4(dst6) / 63.f;
    }
  )";

  static const char RGBA6_TO_RGB8_SHADER_SOURCE[] = R"(
    #if MSAA_ENABLED
      SAMPLER_BINDING(0) uniform sampler2DMSArray samp0;
    #else
      SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    #endif
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;

    void main()
    {
      int layer = 0;
      #if EFB_LAYERS > 1
        layer = int(uv0.z);
      #endif

      ivec3 coords = ivec3(gl_FragCoord.xy, layer);

      vec4 val;
      #if !MSAA_ENABLED
        // No MSAA - just load the first (and only) sample
        val = texelFetch(samp0, coords, 0);
      #elif SSAA_ENABLED
        // Sample shading, shader runs once per sample
        val = texelFetch(samp0, coords, gl_SampleID);
      #else
        // MSAA without sample shading, average out all samples.
        val = vec4(0, 0, 0, 0);
        for (int i = 0; i < MSAA_SAMPLES; i++)
          val += texelFetch(samp0, coords, i);
        val /= float(MSAA_SAMPLES);
      #endif

      ivec4 src6 = ivec4(round(val * 63.f));
      ivec4 dst8;
      dst8.r = (src6.r << 2) | (src6.g >> 4);
      dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);
      dst8.b = ((src6.b & 0x3) << 6) | src6.a;
      dst8.a = 255;

      ocol0 = float4(dst8) / 255.f;
    }
  )";

  static const char DEPTH_RESOLVE_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DMSArray samp0;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out float ocol0;

    void main()
    {
      int layer = 0;
      #if EFB_LAYERS > 1
        layer = int(uv0.z);
      #endif

      // gl_FragCoord is in window coordinates, and we're rendering to
      // the same rectangle in the resolve texture.
      ivec3 coords = ivec3(gl_FragCoord.xy, layer);

      // Take the minimum of all depth samples.
      ocol0 = texelFetch(samp0, coords, 0).r;
      for (int i = 1; i < MSAA_SAMPLES; i++)
        ocol0 = min(ocol0, texelFetch(samp0, coords, i).r);
    }
  )";

  std::string header = g_shader_cache->GetUtilityShaderHeader();
  DestroyConversionShaders();

  m_ps_rgb8_to_rgba6 = Util::CompileAndCreateFragmentShader(header + RGB8_TO_RGBA6_SHADER_SOURCE);
  m_ps_rgba6_to_rgb8 = Util::CompileAndCreateFragmentShader(header + RGBA6_TO_RGB8_SHADER_SOURCE);
  if (GetEFBSamples() != VK_SAMPLE_COUNT_1_BIT)
    m_ps_depth_resolve = Util::CompileAndCreateFragmentShader(header + DEPTH_RESOLVE_SHADER_SOURCE);

  return (m_ps_rgba6_to_rgb8 != VK_NULL_HANDLE && m_ps_rgb8_to_rgba6 != VK_NULL_HANDLE &&
          (GetEFBSamples() == VK_SAMPLE_COUNT_1_BIT || m_ps_depth_resolve != VK_NULL_HANDLE));
}

void FramebufferManager::DestroyConversionShaders()
{
  auto DestroyShader = [this](VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE)
    {
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), shader, nullptr);
      shader = VK_NULL_HANDLE;
    }
  };

  DestroyShader(m_ps_rgb8_to_rgba6);
  DestroyShader(m_ps_rgba6_to_rgb8);
  DestroyShader(m_ps_depth_resolve);
}

u32 FramebufferManager::PeekEFBColor(u32 x, u32 y)
{
  if (!m_color_readback_texture_valid && !PopulateColorReadbackTexture())
    return 0;

  u32 value;
  m_color_readback_texture->ReadTexel(x, y, &value, sizeof(value));
  return value;
}

bool FramebufferManager::PopulateColorReadbackTexture()
{
  // Can't be in our normal render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->OnReadback();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  VkRect2D src_region = {{0, 0}, {GetEFBWidth(), GetEFBHeight()}};
  Texture2D* src_texture = m_efb_color_texture.get();
  VkImageAspectFlags src_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  if (GetEFBSamples() > 1)
    src_texture = ResolveEFBColorTexture(src_region);

  if (GetEFBWidth() != EFB_WIDTH || GetEFBHeight() != EFB_HEIGHT)
  {
    m_color_copy_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                           m_copy_color_render_pass, g_shader_cache->GetScreenQuadVertexShader(),
                           VK_NULL_HANDLE, m_copy_color_shader);

    VkRect2D rect = {{0, 0}, {EFB_WIDTH, EFB_HEIGHT}};
    draw.BeginRenderPass(m_color_copy_framebuffer, rect);

    // Transition EFB to shader read before drawing.
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
    draw.SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
    draw.DrawWithoutVertexBuffer(4);
    draw.EndRenderPass();

    // Restore EFB to color attachment, since we're done with it.
    if (src_texture == m_efb_color_texture.get())
    {
      src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    // Use this as a source texture now.
    src_texture = m_color_copy_texture.get();
  }

  // Copy from EFB or copy texture to staging texture.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_color_readback_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          src_texture->GetImage(), src_aspect, 0, 0, EFB_WIDTH,
                                          EFB_HEIGHT, 0, 0);

  // Restore original layout if we used the EFB as a source.
  if (src_texture == m_efb_color_texture.get())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  // Wait until the copy is complete.
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Map to host memory.
  if (!m_color_readback_texture->IsMapped() && !m_color_readback_texture->Map())
    return false;

  m_color_readback_texture_valid = true;
  return true;
}

float FramebufferManager::PeekEFBDepth(u32 x, u32 y)
{
  if (!m_depth_readback_texture_valid && !PopulateDepthReadbackTexture())
    return 0.0f;

  float value;
  m_depth_readback_texture->ReadTexel(x, y, &value, sizeof(value));
  return value;
}

bool FramebufferManager::PopulateDepthReadbackTexture()
{
  // Can't be in our normal render pass.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->OnReadback();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  VkRect2D src_region = {{0, 0}, {GetEFBWidth(), GetEFBHeight()}};
  Texture2D* src_texture = m_efb_depth_texture.get();
  VkImageAspectFlags src_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (GetEFBSamples() > 1)
  {
    // EFB depth resolves are written out as color textures
    src_texture = ResolveEFBDepthTexture(src_region);
    src_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  if (GetEFBWidth() != EFB_WIDTH || GetEFBHeight() != EFB_HEIGHT)
  {
    m_depth_copy_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD),
                           m_copy_depth_render_pass, g_shader_cache->GetScreenQuadVertexShader(),
                           VK_NULL_HANDLE, m_copy_depth_shader);

    VkRect2D rect = {{0, 0}, {EFB_WIDTH, EFB_HEIGHT}};
    draw.BeginRenderPass(m_depth_copy_framebuffer, rect);

    // Transition EFB to shader read before drawing.
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
    draw.SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
    draw.DrawWithoutVertexBuffer(4);
    draw.EndRenderPass();

    // Restore EFB to depth attachment, since we're done with it.
    if (src_texture == m_efb_depth_texture.get())
    {
      src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                      VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    // Use this as a source texture now.
    src_texture = m_depth_copy_texture.get();
    src_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  // Copy from EFB or copy texture to staging texture.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_depth_readback_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          src_texture->GetImage(), src_aspect, 0, 0, EFB_WIDTH,
                                          EFB_HEIGHT, 0, 0);

  // Restore original layout if we used the EFB as a source.
  if (src_texture == m_efb_depth_texture.get())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }

  // Wait until the copy is complete.
  Util::ExecuteCurrentCommandsAndRestoreState(false, true);

  // Map to host memory.
  if (!m_depth_readback_texture->IsMapped() && !m_depth_readback_texture->Map())
    return false;

  m_depth_readback_texture_valid = true;
  return true;
}

void FramebufferManager::InvalidatePeekCache()
{
  m_color_readback_texture_valid = false;
  m_depth_readback_texture_valid = false;
}

bool FramebufferManager::CreateReadbackRenderPasses()
{
  VkAttachmentDescription copy_attachment = {
      0,                                         // VkAttachmentDescriptionFlags    flags
      EFB_COLOR_TEXTURE_FORMAT,                  // VkFormat                        format
      VK_SAMPLE_COUNT_1_BIT,                     // VkSampleCountFlagBits           samples
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,           // VkAttachmentLoadOp              loadOp
      VK_ATTACHMENT_STORE_OP_STORE,              // VkAttachmentStoreOp             storeOp
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,           // VkAttachmentLoadOp              stencilLoadOp
      VK_ATTACHMENT_STORE_OP_DONT_CARE,          // VkAttachmentStoreOp             stencilStoreOp
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // VkImageLayout                   initialLayout
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL   // VkImageLayout                   finalLayout
  };
  VkAttachmentReference copy_attachment_ref = {
      0,                                        // uint32_t         attachment
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout    layout
  };
  VkSubpassDescription copy_subpass = {
      0,                                // VkSubpassDescriptionFlags       flags
      VK_PIPELINE_BIND_POINT_GRAPHICS,  // VkPipelineBindPoint             pipelineBindPoint
      0,                                // uint32_t                        inputAttachmentCount
      nullptr,                          // const VkAttachmentReference*    pInputAttachments
      1,                                // uint32_t                        colorAttachmentCount
      &copy_attachment_ref,             // const VkAttachmentReference*    pColorAttachments
      nullptr,                          // const VkAttachmentReference*    pResolveAttachments
      nullptr,                          // const VkAttachmentReference*    pDepthStencilAttachment
      0,                                // uint32_t                        preserveAttachmentCount
      nullptr                           // const uint32_t*                 pPreserveAttachments
  };
  VkRenderPassCreateInfo copy_pass = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,  // VkStructureType                   sType
      nullptr,                                    // const void*                       pNext
      0,                                          // VkRenderPassCreateFlags           flags
      1,                 // uint32_t                          attachmentCount
      &copy_attachment,  // const VkAttachmentDescription*    pAttachments
      1,                 // uint32_t                          subpassCount
      &copy_subpass,     // const VkSubpassDescription*       pSubpasses
      0,                 // uint32_t                          dependencyCount
      nullptr            // const VkSubpassDependency*        pDependencies
  };

  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &copy_pass, nullptr,
                                    &m_copy_color_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  // Depth is similar to copy, just a different format.
  copy_attachment.format = EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT;
  res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &copy_pass, nullptr,
                           &m_copy_depth_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  // Some devices don't support point sizes >1 (e.g. Adreno).
  // If we can't use a point size above our maximum IR, use triangles instead.
  // This means a 6x increase in the size of the vertices, though.
  if (!g_vulkan_context->GetDeviceFeatures().largePoints ||
      g_vulkan_context->GetDeviceLimits().pointSizeGranularity > 1 ||
      g_vulkan_context->GetDeviceLimits().pointSizeRange[0] > 1 ||
      g_vulkan_context->GetDeviceLimits().pointSizeRange[1] < 16)
  {
    m_poke_primitive = PrimitiveType::TriangleStrip;
  }
  else
  {
    // Points should be okay.
    m_poke_primitive = PrimitiveType::Points;
  }

  return true;
}

void FramebufferManager::DestroyReadbackRenderPasses()
{
  if (m_copy_color_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_copy_color_render_pass, nullptr);
    m_copy_color_render_pass = VK_NULL_HANDLE;
  }
  if (m_copy_depth_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_copy_depth_render_pass, nullptr);
    m_copy_depth_render_pass = VK_NULL_HANDLE;
  }
}

bool FramebufferManager::CompileReadbackShaders()
{
  std::string source;

  // TODO: Use input attachment here instead?
  // TODO: MSAA resolve in shader.
  static const char COPY_COLOR_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;
    void main()
    {
      ocol0 = texture(samp0, uv0);
    }
  )";

  static const char COPY_DEPTH_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out float ocol0;
    void main()
    {
      ocol0 = texture(samp0, uv0).r;
    }
  )";

  source = g_shader_cache->GetUtilityShaderHeader() + COPY_COLOR_SHADER_SOURCE;
  m_copy_color_shader = Util::CompileAndCreateFragmentShader(source);

  source = g_shader_cache->GetUtilityShaderHeader() + COPY_DEPTH_SHADER_SOURCE;
  m_copy_depth_shader = Util::CompileAndCreateFragmentShader(source);

  return m_copy_color_shader != VK_NULL_HANDLE && m_copy_depth_shader != VK_NULL_HANDLE;
}

void FramebufferManager::DestroyReadbackShaders()
{
  if (m_copy_color_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_copy_color_shader, nullptr);
    m_copy_color_shader = VK_NULL_HANDLE;
  }
  if (m_copy_depth_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_copy_depth_shader, nullptr);
    m_copy_depth_shader = VK_NULL_HANDLE;
  }
}

bool FramebufferManager::CreateReadbackTextures()
{
  m_color_copy_texture =
      Texture2D::Create(EFB_WIDTH, EFB_HEIGHT, 1, 1, EFB_COLOR_TEXTURE_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  m_color_readback_texture = StagingTexture2D::Create(STAGING_BUFFER_TYPE_READBACK, EFB_WIDTH,
                                                      EFB_HEIGHT, EFB_COLOR_TEXTURE_FORMAT);
  if (!m_color_copy_texture || !m_color_readback_texture)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB color readback texture");
    return false;
  }

  m_depth_copy_texture =
      Texture2D::Create(EFB_WIDTH, EFB_HEIGHT, 1, 1, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  m_depth_readback_texture = StagingTexture2D::Create(STAGING_BUFFER_TYPE_READBACK, EFB_WIDTH,
                                                      EFB_HEIGHT, EFB_DEPTH_TEXTURE_FORMAT);
  if (!m_depth_copy_texture || !m_depth_readback_texture)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB depth readback texture");
    return false;
  }

  // With Vulkan, we can leave these textures mapped and use invalidate/flush calls instead.
  if (!m_color_readback_texture->Map() || !m_depth_readback_texture->Map())
  {
    ERROR_LOG(VIDEO, "Failed to map EFB readback textures");
    return false;
  }

  return true;
}

void FramebufferManager::DestroyReadbackTextures()
{
  m_color_copy_texture.reset();
  m_color_readback_texture.reset();
  m_color_readback_texture_valid = false;
  m_depth_copy_texture.reset();
  m_depth_readback_texture.reset();
  m_depth_readback_texture_valid = false;
}

bool FramebufferManager::CreateReadbackFramebuffer()
{
  VkImageView framebuffer_attachment = m_color_copy_texture->GetView();
  VkFramebufferCreateInfo framebuffer_info = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,  // VkStructureType             sType
      nullptr,                                    // const void*                 pNext
      0,                                          // VkFramebufferCreateFlags    flags
      m_copy_color_render_pass,                   // VkRenderPass                renderPass
      1,                                          // uint32_t                    attachmentCount
      &framebuffer_attachment,                    // const VkImageView*          pAttachments
      EFB_WIDTH,                                  // uint32_t                    width
      EFB_HEIGHT,                                 // uint32_t                    height
      1                                           // uint32_t                    layers
  };
  VkResult res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                                     &m_color_copy_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  // Swap for depth
  framebuffer_info.renderPass = m_copy_depth_render_pass;
  framebuffer_attachment = m_depth_copy_texture->GetView();
  res = vkCreateFramebuffer(g_vulkan_context->GetDevice(), &framebuffer_info, nullptr,
                            &m_depth_copy_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  return true;
}

void FramebufferManager::DestroyReadbackFramebuffer()
{
  if (m_color_copy_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_color_copy_framebuffer, nullptr);
    m_color_copy_framebuffer = VK_NULL_HANDLE;
  }
  if (m_depth_copy_framebuffer != VK_NULL_HANDLE)
  {
    vkDestroyFramebuffer(g_vulkan_context->GetDevice(), m_depth_copy_framebuffer, nullptr);
    m_depth_copy_framebuffer = VK_NULL_HANDLE;
  }
}

void FramebufferManager::PokeEFBColor(u32 x, u32 y, u32 color)
{
  // Flush if we exceeded the number of vertices per batch.
  if ((m_color_poke_vertices.size() + 6) > MAX_POKE_VERTICES)
    FlushEFBPokes();

  CreatePokeVertices(&m_color_poke_vertices, x, y, 0.0f, color);

  // Update the peek cache if it's valid, since we know the color of the pixel now.
  if (m_color_readback_texture_valid)
    m_color_readback_texture->WriteTexel(x, y, &color, sizeof(color));
}

void FramebufferManager::PokeEFBDepth(u32 x, u32 y, float depth)
{
  // Flush if we exceeded the number of vertices per batch.
  if ((m_color_poke_vertices.size() + 6) > MAX_POKE_VERTICES)
    FlushEFBPokes();

  CreatePokeVertices(&m_depth_poke_vertices, x, y, depth, 0);

  // Update the peek cache if it's valid, since we know the color of the pixel now.
  if (m_depth_readback_texture_valid)
    m_depth_readback_texture->WriteTexel(x, y, &depth, sizeof(depth));
}

void FramebufferManager::CreatePokeVertices(std::vector<EFBPokeVertex>* destination_list, u32 x,
                                            u32 y, float z, u32 color)
{
  if (m_poke_primitive == PrimitiveType::Points)
  {
    // GPU will expand the point to a quad.
    float cs_x = float(x) * 2.0f / EFB_WIDTH - 1.0f;
    float cs_y = float(y) * 2.0f / EFB_HEIGHT - 1.0f;
    float point_size = GetEFBWidth() / static_cast<float>(EFB_WIDTH);
    destination_list->push_back({{cs_x, cs_y, z, point_size}, color});
    return;
  }

  // Some devices don't support point sizes >1 (e.g. Adreno).
  // Generate quad from the single point (clip-space coordinates).
  float x1 = float(x) * 2.0f / EFB_WIDTH - 1.0f;
  float y1 = float(y) * 2.0f / EFB_HEIGHT - 1.0f;
  float x2 = float(x + 1) * 2.0f / EFB_WIDTH - 1.0f;
  float y2 = float(y + 1) * 2.0f / EFB_HEIGHT - 1.0f;
  destination_list->push_back({{x1, y1, z, 1.0f}, color});
  destination_list->push_back({{x2, y1, z, 1.0f}, color});
  destination_list->push_back({{x1, y2, z, 1.0f}, color});
  destination_list->push_back({{x1, y2, z, 1.0f}, color});
  destination_list->push_back({{x2, y1, z, 1.0f}, color});
  destination_list->push_back({{x2, y2, z, 1.0f}, color});
}

void FramebufferManager::FlushEFBPokes()
{
  if (!m_color_poke_vertices.empty())
  {
    DrawPokeVertices(m_color_poke_vertices.data(), m_color_poke_vertices.size(), true, false);
    m_color_poke_vertices.clear();
  }

  if (!m_depth_poke_vertices.empty())
  {
    DrawPokeVertices(m_depth_poke_vertices.data(), m_depth_poke_vertices.size(), false, true);
    m_depth_poke_vertices.clear();
  }
}

void FramebufferManager::DrawPokeVertices(const EFBPokeVertex* vertices, size_t vertex_count,
                                          bool write_color, bool write_depth)
{
  // Relatively simple since we don't have any bindings.
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

  // We don't use the utility shader in order to keep the vertices compact.
  PipelineInfo pipeline_info = {};
  pipeline_info.vertex_format = m_poke_vertex_format.get();
  pipeline_info.pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD);
  pipeline_info.vs = m_poke_vertex_shader;
  pipeline_info.gs = (GetEFBLayers() > 1) ? m_poke_geometry_shader : VK_NULL_HANDLE;
  pipeline_info.ps = m_poke_fragment_shader;
  pipeline_info.render_pass = m_efb_load_render_pass;
  pipeline_info.rasterization_state.hex = RenderState::GetNoCullRasterizationState().hex;
  pipeline_info.rasterization_state.primitive = m_poke_primitive;
  pipeline_info.multisampling_state.hex = GetEFBMultisamplingState().hex;
  pipeline_info.depth_state.hex = RenderState::GetNoDepthTestingDepthStencilState().hex;
  pipeline_info.blend_state.hex = RenderState::GetNoBlendingBlendState().hex;
  pipeline_info.blend_state.colorupdate = write_color;
  pipeline_info.blend_state.alphaupdate = write_color;
  if (write_depth)
  {
    pipeline_info.depth_state.testenable = true;
    pipeline_info.depth_state.updateenable = true;
    pipeline_info.depth_state.func = ZMode::ALWAYS;
  }

  VkPipeline pipeline = g_shader_cache->GetPipeline(pipeline_info);
  if (pipeline == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to get pipeline for EFB poke draw");
    return;
  }

  // Populate vertex buffer.
  size_t vertices_size = sizeof(EFBPokeVertex) * m_color_poke_vertices.size();
  if (!m_poke_vertex_stream_buffer->ReserveMemory(vertices_size, sizeof(EfbPokeData), true, true,
                                                  false))
  {
    // Kick a command buffer first.
    WARN_LOG(VIDEO, "Kicking command buffer due to no EFB poke space.");
    Util::ExecuteCurrentCommandsAndRestoreState(false);
    command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

    if (!m_poke_vertex_stream_buffer->ReserveMemory(vertices_size, sizeof(EfbPokeData), true, true,
                                                    false))
    {
      PanicAlert("Failed to get space for EFB poke vertices");
      return;
    }
  }
  VkBuffer vb_buffer = m_poke_vertex_stream_buffer->GetBuffer();
  VkDeviceSize vb_offset = m_poke_vertex_stream_buffer->GetCurrentOffset();
  memcpy(m_poke_vertex_stream_buffer->GetCurrentHostPointer(), vertices, vertices_size);
  m_poke_vertex_stream_buffer->CommitMemory(vertices_size);

  // Set up state.
  StateTracker::GetInstance()->EndClearRenderPass();
  StateTracker::GetInstance()->BeginRenderPass();
  StateTracker::GetInstance()->SetPendingRebind();
  Util::SetViewportAndScissor(command_buffer, 0, 0, GetEFBWidth(), GetEFBHeight());
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb_buffer, &vb_offset);
  vkCmdDraw(command_buffer, static_cast<u32>(vertex_count), 1, 0, 0);
}

void FramebufferManager::CreatePokeVertexFormat()
{
  PortableVertexDeclaration vtx_decl = {};
  vtx_decl.position.enable = true;
  vtx_decl.position.type = VAR_FLOAT;
  vtx_decl.position.components = 4;
  vtx_decl.position.integer = false;
  vtx_decl.position.offset = offsetof(EFBPokeVertex, position);
  vtx_decl.colors[0].enable = true;
  vtx_decl.colors[0].type = VAR_UNSIGNED_BYTE;
  vtx_decl.colors[0].components = 4;
  vtx_decl.colors[0].integer = false;
  vtx_decl.colors[0].offset = offsetof(EFBPokeVertex, color);
  vtx_decl.stride = sizeof(EFBPokeVertex);

  m_poke_vertex_format = std::make_unique<VertexFormat>(vtx_decl);
}

bool FramebufferManager::CreatePokeVertexBuffer()
{
  m_poke_vertex_stream_buffer = StreamBuffer::Create(
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, POKE_VERTEX_BUFFER_SIZE, POKE_VERTEX_BUFFER_SIZE);
  if (!m_poke_vertex_stream_buffer)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB poke vertex buffer");
    return false;
  }

  return true;
}

void FramebufferManager::DestroyPokeVertexBuffer()
{
  m_poke_vertex_stream_buffer.reset();
}

bool FramebufferManager::CompilePokeShaders()
{
  static const char POKE_VERTEX_SHADER_SOURCE[] = R"(
    layout(location = 0) in vec4 ipos;
    layout(location = 5) in vec4 icol0;

    layout(location = 0) out vec4 col0;

    void main()
    {
      gl_Position = vec4(ipos.xyz, 1.0f);
      #if USE_POINT_SIZE
        gl_PointSize = ipos.w;
      #endif
      col0 = icol0;
    }

    )";

  static const char POKE_GEOMETRY_SHADER_SOURCE[] = R"(
    layout(triangles) in;
    layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

    VARYING_LOCATION(0) in VertexData
    {
      vec4 col0;
    } in_data[];

    VARYING_LOCATION(0) out VertexData
    {
      vec4 col0;
    } out_data;

    void main()
    {
      for (int j = 0; j < EFB_LAYERS; j++)
      {
        for (int i = 0; i < 3; i++)
        {
          gl_Layer = j;
          gl_Position = gl_in[i].gl_Position;
          out_data.col0 = in_data[i].col0;
          EmitVertex();
        }
        EndPrimitive();
      }
    }
  )";

  static const char POKE_PIXEL_SHADER_SOURCE[] = R"(
    layout(location = 0) in vec4 col0;
    layout(location = 0) out vec4 ocol0;
    void main()
    {
      ocol0 = col0;
    }
  )";

  std::string source = g_shader_cache->GetUtilityShaderHeader();
  if (m_poke_primitive == PrimitiveType::Points)
    source += "#define USE_POINT_SIZE 1\n";
  source += POKE_VERTEX_SHADER_SOURCE;
  m_poke_vertex_shader = Util::CompileAndCreateVertexShader(source);
  if (m_poke_vertex_shader == VK_NULL_HANDLE)
    return false;

  if (g_vulkan_context->SupportsGeometryShaders())
  {
    source = g_shader_cache->GetUtilityShaderHeader() + POKE_GEOMETRY_SHADER_SOURCE;
    m_poke_geometry_shader = Util::CompileAndCreateGeometryShader(source);
    if (m_poke_geometry_shader == VK_NULL_HANDLE)
      return false;
  }

  source = g_shader_cache->GetUtilityShaderHeader() + POKE_PIXEL_SHADER_SOURCE;
  m_poke_fragment_shader = Util::CompileAndCreateFragmentShader(source);
  if (m_poke_fragment_shader == VK_NULL_HANDLE)
    return false;

  return true;
}

void FramebufferManager::DestroyPokeShaders()
{
  if (m_poke_vertex_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_poke_vertex_shader, nullptr);
    m_poke_vertex_shader = VK_NULL_HANDLE;
  }
  if (m_poke_geometry_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_poke_geometry_shader, nullptr);
    m_poke_geometry_shader = VK_NULL_HANDLE;
  }
  if (m_poke_fragment_shader != VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_poke_fragment_shader, nullptr);
    m_poke_vertex_shader = VK_NULL_HANDLE;
  }
}

std::unique_ptr<XFBSourceBase> FramebufferManager::CreateXFBSource(unsigned int target_width,
                                                                   unsigned int target_height,
                                                                   unsigned int layers)
{
  TextureConfig config;
  config.width = target_width;
  config.height = target_height;
  config.layers = layers;
  config.rendertarget = true;
  auto texture = TextureCache::GetInstance()->CreateTexture(config);
  if (!texture)
  {
    PanicAlert("Failed to create texture for XFB source");
    return nullptr;
  }

  return std::make_unique<XFBSource>(std::move(texture));
}

void FramebufferManager::CopyToRealXFB(u32 xfb_addr, u32 fb_stride, u32 fb_height,
                                       const EFBRectangle& source_rc, float gamma)
{
  // Pending/batched EFB pokes should be included in the copied image.
  FlushEFBPokes();

  // Schedule early command-buffer execution.
  StateTracker::GetInstance()->EndRenderPass();
  StateTracker::GetInstance()->OnReadback();

  // GPU EFB textures -> Guest memory
  u8* xfb_ptr = Memory::GetPointer(xfb_addr);
  _assert_(xfb_ptr);

  // source_rc is in native coordinates, so scale it to the internal resolution.
  TargetRectangle scaled_rc = g_renderer->ConvertEFBRectangle(source_rc);
  VkRect2D scaled_rc_vk = {
      {scaled_rc.left, scaled_rc.top},
      {static_cast<u32>(scaled_rc.GetWidth()), static_cast<u32>(scaled_rc.GetHeight())}};
  Texture2D* src_texture = ResolveEFBColorTexture(scaled_rc_vk);

  // The destination stride can differ from the copy region width, in which case the pixels
  // outside the copy region should not be written to.
  TextureCache::GetInstance()->GetTextureConverter()->EncodeTextureToMemoryYUYV(
      xfb_ptr, static_cast<u32>(source_rc.GetWidth()), fb_stride, fb_height, src_texture,
      scaled_rc);

  // If we sourced directly from the EFB framebuffer, restore it to a color attachment.
  if (src_texture == m_efb_color_texture.get())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }
}

XFBSource::XFBSource(std::unique_ptr<AbstractTexture> texture)
    : XFBSourceBase(), m_texture(std::move(texture))
{
}

XFBSource::~XFBSource()
{
}

VKTexture* XFBSource::GetTexture() const
{
  return static_cast<VKTexture*>(m_texture.get());
}

void XFBSource::DecodeToTexture(u32 xfb_addr, u32 fb_width, u32 fb_height)
{
  // Guest memory -> GPU EFB Textures
  const u8* src_ptr = Memory::GetPointer(xfb_addr);
  _assert_(src_ptr);
  TextureCache::GetInstance()->GetTextureConverter()->DecodeYUYVTextureFromMemory(
      static_cast<VKTexture*>(m_texture.get()), src_ptr, fb_width, fb_width * 2, fb_height);
}

void XFBSource::CopyEFB(float gamma)
{
  // Pending/batched EFB pokes should be included in the copied image.
  FramebufferManager::GetInstance()->FlushEFBPokes();

  // Virtual XFB, copy EFB at native resolution to m_texture
  MathUtil::Rectangle<int> rect(0, 0, static_cast<int>(texWidth), static_cast<int>(texHeight));
  VkRect2D vk_rect = {{rect.left, rect.top},
                      {static_cast<u32>(rect.GetWidth()), static_cast<u32>(rect.GetHeight())}};

  Texture2D* src_texture = FramebufferManager::GetInstance()->ResolveEFBColorTexture(vk_rect);
  static_cast<VKTexture*>(m_texture.get())->CopyRectangleFromTexture(src_texture, rect, rect);

  // If we sourced directly from the EFB framebuffer, restore it to a color attachment.
  if (src_texture == FramebufferManager::GetInstance()->GetEFBColorTexture())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }
}

}  // namespace Vulkan
