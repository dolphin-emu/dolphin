// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonFuncs.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/VKPixelShader.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
VKPixelShader::VKPixelShader(const std::string& shader_source) : AbstractPixelShader(shader_source)
{
  // TODO: it's possible this should come from a pool, maybe on the pipeline?
  if (!CreateRenderPass())
  {
    // TODO: Properly return..
  }

  m_shader = Util::CompileAndCreateFragmentShader(shader_source);
  if (m_shader == VK_NULL_HANDLE)
  {
    // PanicAlert("Failed to compile texture shader.");
  }
}

VKPixelShader::~VKPixelShader()
{
  if (m_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), m_render_pass, nullptr);

  if (m_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_shader, nullptr);
}

bool VKPixelShader::CreateRenderPass()
{
  VkAttachmentDescription attachments[] = {
      {0, TEXTURE_FORMAT, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpass_descriptions[] = {{0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1,
                                                  color_attachment_references, nullptr, nullptr, 0,
                                                  nullptr}};

  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      static_cast<u32>(ArraySize(attachments)),
                                      attachments,
                                      static_cast<u32>(ArraySize(subpass_descriptions)),
                                      subpass_descriptions,
                                      0,
                                      nullptr};

  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &pass_info, nullptr,
                                    &m_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "CreateRenderPass failed: ");
    return false;
  }

  return true;
}

void VKPixelShader::ApplyTo(AbstractTexture* texture)
{
  VKTexture* vulkan_texture = static_cast<VKTexture*>(texture);
  Texture2D* src_texture = vulkan_texture->GetRawTexIdentifier();
  VkImageLayout original_layout = src_texture->GetLayout();

  // Transition to shader resource before reading.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  DrawTexture(src_texture, vulkan_texture->GetFramebuffer(), vulkan_texture->GetConfig().width, vulkan_texture->GetConfig().height);

  // Transition back to original state
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(), original_layout);
}

void VKPixelShader::DrawTexture(Texture2D* vulkan_texture, const VkFramebuffer& frame_buffer, int width, int height)
{
  // Can't do our own draw within a render pass.
  StateTracker::GetInstance()->EndRenderPass();

  vulkan_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
    g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_PUSH_CONSTANT),
    m_render_pass, g_object_cache->GetScreenQuadVertexShader(),
    VK_NULL_HANDLE, m_shader);

  // Uniform - int4[2] of left,top,--,scale,native-width,native-height
  auto position_uniform = GetUniform(0);
  draw.SetPushConstants(position_uniform.data(), sizeof(position_uniform));

  // We also linear filtering for both box filtering and downsampling higher resolutions to 1x
  // TODO: This only produces perfect downsampling for 1.5x and 2x IR, other resolution will
  //       need more complex down filtering to average all pixels and produce the correct result.
  bool linear_filter = g_ActiveConfig.iEFBScale != SCALE_1X;
  draw.SetPSSampler(0, vulkan_texture->GetView(), linear_filter ? g_object_cache->GetLinearSampler() :
    g_object_cache->GetPointSampler());

  Util::SetViewportAndScissor(g_command_buffer_mgr->GetCurrentCommandBuffer(), 0, 0, width, height);

  VkRect2D render_region = { { 0, 0 },{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) } };
  draw.BeginRenderPass(frame_buffer, render_region);
  draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
  draw.EndRenderPass();

  // Transition the image before copying
  vulkan_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
}

} // namespace Vulkan