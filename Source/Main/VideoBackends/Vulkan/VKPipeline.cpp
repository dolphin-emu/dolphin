// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKPipeline.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
VKPipeline::VKPipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                       AbstractPipelineUsage usage)
    : m_pipeline(pipeline), m_pipeline_layout(pipeline_layout), m_usage(usage)
{
}

VKPipeline::~VKPipeline()
{
  vkDestroyPipeline(g_vulkan_context->GetDevice(), m_pipeline, nullptr);
}

std::unique_ptr<VKPipeline> VKPipeline::Create(const AbstractPipelineConfig& config)
{
  DEBUG_ASSERT(config.vertex_shader && config.pixel_shader);

  // Get render pass for config.
  VkRenderPass render_pass = g_object_cache->GetRenderPass(
      Util::GetVkFormatForHostTextureFormat(config.framebuffer_state.color_texture_format),
      Util::GetVkFormatForHostTextureFormat(config.framebuffer_state.depth_texture_format),
      config.framebuffer_state.samples, VK_ATTACHMENT_LOAD_OP_LOAD);

  // Get pipeline layout.
  VkPipelineLayout pipeline_layout;
  switch (config.usage)
  {
  case AbstractPipelineUsage::GX:
    pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD);
    break;
  case AbstractPipelineUsage::Utility:
    pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_UTILITY);
    break;
  default:
    PanicAlert("Unknown pipeline layout.");
    return nullptr;
  }

  // TODO: Move ShaderCache stuff to here.
  PipelineInfo pinfo;
  pinfo.vertex_format = static_cast<const VertexFormat*>(config.vertex_format);
  pinfo.pipeline_layout = pipeline_layout;
  pinfo.vs = static_cast<const VKShader*>(config.vertex_shader)->GetShaderModule();
  pinfo.ps = static_cast<const VKShader*>(config.pixel_shader)->GetShaderModule();
  pinfo.gs = config.geometry_shader ?
                 static_cast<const VKShader*>(config.geometry_shader)->GetShaderModule() :
                 VK_NULL_HANDLE;
  pinfo.render_pass = render_pass;
  pinfo.rasterization_state.hex = config.rasterization_state.hex;
  pinfo.depth_state.hex = config.depth_state.hex;
  pinfo.blend_state.hex = config.blending_state.hex;
  pinfo.multisampling_state.hex = 0;
  pinfo.multisampling_state.samples = config.framebuffer_state.samples;
  pinfo.multisampling_state.per_sample_shading = config.framebuffer_state.per_sample_shading;

  VkPipeline pipeline = g_shader_cache->CreatePipeline(pinfo);
  if (pipeline == VK_NULL_HANDLE)
    return nullptr;

  return std::make_unique<VKPipeline>(pipeline, pipeline_layout, config.usage);
}
}  // namespace Vulkan
