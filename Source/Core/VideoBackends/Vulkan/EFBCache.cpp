// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VideoConfig.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/EFBCache.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Texture2D.h"

namespace Vulkan
{
EFBCache::EFBCache(FramebufferManager* framebuffer_mgr) : m_framebuffer_mgr(framebuffer_mgr)
{
}

EFBCache::~EFBCache()
{
  if (m_color_readback_texture && m_color_readback_texture->IsMapped())
    m_color_readback_texture->Unmap();
  if (m_depth_readback_texture && m_depth_readback_texture->IsMapped())
    m_depth_readback_texture->Unmap();
}

bool EFBCache::Initialize()
{
  if (!CreateRenderPasses())
    return false;

  if (!CompileShaders())
    return false;

  if (!CreateTextures())
    return false;

  return true;
}

u32 EFBCache::PeekEFBColor(StateTracker* state_tracker, u32 x, u32 y)
{
  if (!m_color_readback_texture_valid && !PopulateColorReadbackTexture(state_tracker))
    return 0;

  u32 value;
  m_color_readback_texture->ReadTexel(x, y, &value, sizeof(value));
  return value;
}

float EFBCache::PeekEFBDepth(StateTracker* state_tracker, u32 x, u32 y)
{
  if (!m_depth_readback_texture_valid && !PopulateDepthReadbackTexture(state_tracker))
    return 0.0f;

  float value;
  m_depth_readback_texture->ReadTexel(x, y, &value, sizeof(value));
  return value;
}

void EFBCache::InvalidatePeekCache()
{
  m_color_readback_texture_valid = false;
  m_depth_readback_texture_valid = false;
}

void EFBCache::PokeEFBColor(u32 x, u32 y, u32 color)
{
  ERROR_LOG(VIDEO, "EFBCache::PokeEFBColor not implemented");
}

void EFBCache::PokeEFBDepth(u32 x, u32 y, float depth)
{
  ERROR_LOG(VIDEO, "EFBCache::PokeEFBDepth not implemented");
}

void EFBCache::FlushEFBPokes()
{
}

bool EFBCache::CreateRenderPasses()
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

  VkResult res = vkCreateRenderPass(g_object_cache->GetDevice(), &copy_pass, nullptr,
                                    &m_copy_color_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  // Depth is similar to copy, just a different format.
  copy_attachment.format = EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT;
  res = vkCreateRenderPass(g_object_cache->GetDevice(), &copy_pass, nullptr,
                           &m_copy_color_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  return true;
}

bool EFBCache::CompileShaders()
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

  source = g_object_cache->GetUtilityShaderHeader() + COPY_COLOR_SHADER_SOURCE;
  m_copy_color_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);
  source = g_object_cache->GetUtilityShaderHeader() + COPY_DEPTH_SHADER_SOURCE;
  m_copy_depth_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  if (m_copy_color_shader == VK_NULL_HANDLE || m_copy_depth_shader == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Failed to compile one or more shaders");
    return false;
  }

  return true;
}

bool EFBCache::CreateTextures()
{
  m_color_copy_texture = Texture2D::Create(
      EFB_WIDTH, EFB_HEIGHT, 1, 1, EFB_COLOR_TEXTURE_FORMAT, VK_IMAGE_VIEW_TYPE_2D,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  m_color_readback_texture =
      StagingTexture2D::Create(EFB_WIDTH, EFB_HEIGHT, EFB_COLOR_TEXTURE_FORMAT);
  if (!m_color_copy_texture || !m_color_readback_texture)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB color readback texture");
    return false;
  }

  m_depth_copy_texture = Texture2D::Create(
      EFB_WIDTH, EFB_HEIGHT, 1, 1, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT, VK_IMAGE_VIEW_TYPE_2D,
      VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  // We can't copy to/from color<->depth formats, so using a linear texture is not an option here.
  // TODO: Investigate if vkCmdBlitImage can be used. The documentation isn't that clear.
  m_depth_readback_texture =
      // StagingTexture2D::Create(EFB_WIDTH, EFB_HEIGHT, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT);
      StagingTexture2DBuffer::Create(EFB_WIDTH, EFB_HEIGHT, EFB_DEPTH_TEXTURE_FORMAT);
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

bool EFBCache::PopulateColorReadbackTexture(StateTracker* state_tracker)
{
  // Can't be in our normal render pass.
  state_tracker->EndRenderPass();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  Texture2D* src_texture = m_framebuffer_mgr->GetEFBColorTexture();
  VkImageAspectFlags src_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  if (m_framebuffer_mgr->GetEFBWidth() != EFB_WIDTH ||
      m_framebuffer_mgr->GetEFBHeight() != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
  {
    UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           g_object_cache->GetStandardPipelineLayout(), m_copy_color_render_pass,
                           g_object_cache->GetSharedShaderCache().GetScreenQuadVertexShader(),
                           VK_NULL_HANDLE, m_copy_color_shader);

    VkRect2D rect = {{0, 0}, {EFB_WIDTH, EFB_HEIGHT}};
    draw.BeginRenderPass(m_color_copy_framebuffer, rect);

    // Transition EFB to shader read before drawing.
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
    draw.SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
    draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
    draw.EndRenderPass();

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
  if (src_texture == m_framebuffer_mgr->GetEFBColorTexture())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  // Wait until the copy is complete.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  state_tracker->InvalidateDescriptorSets();
  state_tracker->SetPendingRebind();

  // Map to host memory.
  if (!m_color_readback_texture->IsMapped() && !m_color_readback_texture->Map())
    return false;

  m_color_readback_texture_valid = true;
  return true;
}

bool EFBCache::PopulateDepthReadbackTexture(StateTracker* state_tracker)
{
  // Can't be in our normal render pass.
  state_tracker->EndRenderPass();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  Texture2D* src_texture = m_framebuffer_mgr->GetEFBDepthTexture();
  VkImageAspectFlags src_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (m_framebuffer_mgr->GetEFBWidth() != EFB_WIDTH ||
      m_framebuffer_mgr->GetEFBHeight() != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
  {
    UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           g_object_cache->GetStandardPipelineLayout(), m_copy_depth_render_pass,
                           g_object_cache->GetSharedShaderCache().GetScreenQuadVertexShader(),
                           VK_NULL_HANDLE, m_copy_depth_shader);

    VkRect2D rect = {{0, 0}, {EFB_WIDTH, EFB_HEIGHT}};
    draw.BeginRenderPass(m_depth_copy_framebuffer, rect);

    // Transition EFB to shader read before drawing.
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
    draw.SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
    draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
    draw.EndRenderPass();

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
  if (src_texture == m_framebuffer_mgr->GetEFBDepthTexture())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }

  // Wait until the copy is complete.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  state_tracker->InvalidateDescriptorSets();
  state_tracker->SetPendingRebind();

  // Map to host memory.
  if (!m_depth_readback_texture->IsMapped() && !m_depth_readback_texture->Map())
    return false;

  m_depth_readback_texture_valid = true;
  return true;
}

}  // namespace Vulkan
