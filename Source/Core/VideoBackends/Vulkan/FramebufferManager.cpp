// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>

#include "Common/CommonFuncs.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/Helpers.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"

#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
FramebufferManager::FramebufferManager()
{
  if (!CreateEFBRenderPass())
    PanicAlert("Failed to create EFB render pass");
  if (!CompileShaders())
    PanicAlert("Failed to compile EFB shaders");
  if (!CreateEFBFramebuffer())
    PanicAlert("Failed to create EFB textures");
}

FramebufferManager::~FramebufferManager()
{
  DestroyEFBFramebuffer();
  DestroyEFBRenderPass();
  DestroyShaders();
}

void FramebufferManager::GetTargetSize(unsigned int* width, unsigned int* height)
{
  *width = m_efb_width;
  *height = m_efb_height;
}

bool FramebufferManager::CreateEFBRenderPass()
{
  m_efb_samples = static_cast<VkSampleCountFlagBits>(g_ActiveConfig.iMultisamples);

  // render pass for rendering to the efb
  VkAttachmentDescription attachments[] = {
      {0, EFB_COLOR_TEXTURE_FORMAT, m_efb_samples, VK_ATTACHMENT_LOAD_OP_LOAD,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
      {0, EFB_DEPTH_TEXTURE_FORMAT, m_efb_samples, VK_ATTACHMENT_LOAD_OP_LOAD,
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

  VkResult res =
      vkCreateRenderPass(g_object_cache->GetDevice(), &pass_info, nullptr, &m_efb_render_pass);

  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass (EFB) failed: ");
    return false;
  }

  // render pass for resolving depth, since we can't do it with vkCmdResolveImage
  if (m_efb_samples != VK_SAMPLE_COUNT_1_BIT)
  {
    VkAttachmentDescription resolve_attachment = {0,
                                                  EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT,
                                                  VK_SAMPLE_COUNT_1_BIT,
                                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_STORE,
                                                  VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                  VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                  VK_IMAGE_LAYOUT_UNDEFINED,
                                                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    // Ensure all reads have finished from the resolved texture before overwriting it.
    VkSubpassDependency dependancies[] = {
        {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_DEPENDENCY_BY_REGION_BIT},
        {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT}};
    subpass_description.pDepthStencilAttachment = nullptr;
    pass_info.pAttachments = &resolve_attachment;
    pass_info.attachmentCount = 1;
    pass_info.dependencyCount = static_cast<u32>(ArraySize(dependancies));
    pass_info.pDependencies = dependancies;
    res = vkCreateRenderPass(g_object_cache->GetDevice(), &pass_info, nullptr,
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
  if (m_efb_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_efb_render_pass, nullptr);
    m_efb_render_pass = VK_NULL_HANDLE;
  }

  if (m_depth_resolve_render_pass != VK_NULL_HANDLE)
  {
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_depth_resolve_render_pass, nullptr);
    m_depth_resolve_render_pass = VK_NULL_HANDLE;
  }
}

bool FramebufferManager::CreateEFBFramebuffer()
{
  m_efb_width = static_cast<u32>(std::max(Renderer::GetTargetWidth(), 1));
  m_efb_height = static_cast<u32>(std::max(Renderer::GetTargetHeight(), 1));
  m_efb_layers = (g_ActiveConfig.iStereoMode != STEREO_OFF) ? 2 : 1;
  INFO_LOG(VIDEO, "EFB size: %ux%ux%u", m_efb_width, m_efb_height, m_efb_layers);

  // Update the static variable in the base class. Why does this even exist?
  FramebufferManagerBase::m_EFBLayers = m_efb_layers;

  // Allocate EFB render targets
  m_efb_color_texture =
      Texture2D::Create(m_efb_width, m_efb_height, 1, m_efb_layers, EFB_COLOR_TEXTURE_FORMAT,
                        m_efb_samples, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  // We need a second texture to swap with for changing pixel formats
  m_efb_convert_color_texture =
      Texture2D::Create(m_efb_width, m_efb_height, 1, m_efb_layers, EFB_COLOR_TEXTURE_FORMAT,
                        m_efb_samples, VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  m_efb_depth_texture = Texture2D::Create(
      m_efb_width, m_efb_height, 1, m_efb_layers, EFB_DEPTH_TEXTURE_FORMAT, m_efb_samples,
      VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

  if (!m_efb_color_texture || !m_efb_convert_color_texture || !m_efb_depth_texture)
    return false;

  // Create resolved textures if MSAA is on
  if (m_efb_samples != VK_SAMPLE_COUNT_1_BIT)
  {
    m_efb_resolve_color_texture = Texture2D::Create(
        m_efb_width, m_efb_height, 1, m_efb_layers, EFB_COLOR_TEXTURE_FORMAT, VK_SAMPLE_COUNT_1_BIT,
        VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT);

    m_efb_resolve_depth_texture = Texture2D::Create(
        m_efb_width, m_efb_height, 1, m_efb_layers, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT,
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
                                                m_efb_width,
                                                m_efb_height,
                                                m_efb_layers};

    VkResult res = vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr,
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
                                              m_efb_render_pass,
                                              static_cast<u32>(ArraySize(framebuffer_attachments)),
                                              framebuffer_attachments,
                                              m_efb_width,
                                              m_efb_height,
                                              m_efb_layers};

  VkResult res = vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr,
                                     &m_efb_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  // Create second framebuffer for format conversions
  framebuffer_attachments[0] = m_efb_convert_color_texture->GetView();
  res = vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr,
                            &m_efb_convert_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  // Transition to state that can be used to clear
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Clear the contents of the buffers.
  // TODO: On a resize, this should really be copying the old contents in.
  static const VkClearColorValue clear_color = {{0.0f, 0.0f, 0.0f, 1.0f}};
  static const VkClearDepthStencilValue clear_depth = {0.0f, 0};
  VkImageSubresourceRange clear_color_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, m_efb_layers};
  VkImageSubresourceRange clear_depth_range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, m_efb_layers};
  vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                       m_efb_color_texture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       &clear_color, 1, &clear_color_range);
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
    vkDestroyFramebuffer(g_object_cache->GetDevice(), m_efb_framebuffer, nullptr);
    m_efb_framebuffer = VK_NULL_HANDLE;
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
  DestroyShaders();

  if (!CompileShaders())
    PanicAlert("Failed to compile EFB shaders");
}

void FramebufferManager::ReinterpretPixelData(int convtype)
{
  // TODO: Constants for these?
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
                         g_object_cache->GetStandardPipelineLayout(), m_efb_render_pass,
                         g_object_cache->GetScreenQuadVertexShader(),
                         g_object_cache->GetScreenQuadGeometryShader(), pixel_shader);

  RasterizationState rs_state = Util::GetNoCullRasterizationState();
  rs_state.samples = m_efb_samples;
  rs_state.per_sample_shading = g_ActiveConfig.bSSAA ? VK_TRUE : VK_FALSE;
  draw.SetRasterizationState(rs_state);

  VkRect2D region = {{0, 0}, {m_efb_width, m_efb_height}};
  draw.BeginRenderPass(m_efb_convert_framebuffer, region);
  draw.SetPSSampler(0, m_efb_color_texture->GetView(), g_object_cache->GetPointSampler());
  draw.SetViewportAndScissor(0, 0, m_efb_width, m_efb_height);
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();

  // Swap EFB texture pointers
  std::swap(m_efb_color_texture, m_efb_convert_color_texture);
  std::swap(m_efb_framebuffer, m_efb_convert_framebuffer);
}

Texture2D* FramebufferManager::ResolveEFBColorTexture(StateTracker* state_tracker,
                                                      const VkRect2D& region)
{
  // Return the normal EFB texture if multisampling is off.
  if (m_efb_samples == VK_SAMPLE_COUNT_1_BIT)
    return m_efb_color_texture.get();

  // Can't resolve within a render pass.
  state_tracker->EndRenderPass();

  // Resolving is considered to be a transfer operation.
  m_efb_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_efb_resolve_color_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  // Resolve to our already-created texture.
  VkImageResolve resolve = {
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, m_efb_layers},  // VkImageSubresourceLayers srcSubresource
      {region.offset.x, region.offset.y, 0},            // VkOffset3D                  srcOffset
      {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, m_efb_layers},  // VkImageSubresourceLayers dstSubresource
      {region.offset.x, region.offset.y, 0},            // VkOffset3D                  dstOffset
      {region.extent.width, region.extent.height, m_efb_layers}  // VkExtent3D extent
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

Texture2D* FramebufferManager::ResolveEFBDepthTexture(StateTracker* state_tracker,
                                                      const VkRect2D& region)
{
  // Return the normal EFB texture if multisampling is off.
  if (m_efb_samples == VK_SAMPLE_COUNT_1_BIT)
    return m_efb_depth_texture.get();

  // Can't resolve within a render pass.
  state_tracker->EndRenderPass();

  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  // Draw using resolve shader to write the minimum depth of all samples to the resolve texture.
  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                         g_object_cache->GetStandardPipelineLayout(), m_depth_resolve_render_pass,
                         g_object_cache->GetScreenQuadVertexShader(),
                         g_object_cache->GetScreenQuadGeometryShader(), m_ps_depth_resolve);
  draw.BeginRenderPass(m_depth_resolve_framebuffer, region);
  draw.SetPSSampler(0, m_efb_depth_texture->GetView(), g_object_cache->GetPointSampler());
  draw.SetViewportAndScissor(region.offset.x, region.offset.y, region.extent.width,
                             region.extent.height);
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();

  // Restore MSAA texture ready for rendering again
  m_efb_depth_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

  // Render pass transitions to shader resource.
  m_efb_resolve_depth_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  return m_efb_resolve_depth_texture.get();
}

bool FramebufferManager::CompileShaders()
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

  std::string header = g_object_cache->GetUtilityShaderHeader();
  PixelShaderCache& ps_cache = g_object_cache->GetPixelShaderCache();
  DestroyShaders();

  m_ps_rgb8_to_rgba6 = ps_cache.CompileAndCreateShader(header + RGB8_TO_RGBA6_SHADER_SOURCE);
  m_ps_rgba6_to_rgb8 = ps_cache.CompileAndCreateShader(header + RGBA6_TO_RGB8_SHADER_SOURCE);
  if (m_efb_samples != VK_SAMPLE_COUNT_1_BIT)
    m_ps_depth_resolve = ps_cache.CompileAndCreateShader(header + DEPTH_RESOLVE_SHADER_SOURCE);

  return (m_ps_rgba6_to_rgb8 != VK_NULL_HANDLE && m_ps_rgb8_to_rgba6 != VK_NULL_HANDLE &&
          (m_efb_samples == VK_SAMPLE_COUNT_1_BIT || m_ps_depth_resolve != VK_NULL_HANDLE));
}

void FramebufferManager::DestroyShaders()
{
  auto DestroyShader = [this](VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE)
    {
      vkDestroyShaderModule(g_object_cache->GetDevice(), shader, nullptr);
      shader = VK_NULL_HANDLE;
    }
  };

  DestroyShader(m_ps_rgb8_to_rgba6);
  DestroyShader(m_ps_rgba6_to_rgb8);
  DestroyShader(m_ps_depth_resolve);
}
}  // namespace Vulkan
