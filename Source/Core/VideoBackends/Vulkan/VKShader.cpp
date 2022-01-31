// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKShader.h"

#include "Common/Align.h"
#include "Common/Assert.h"

#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/VideoConfig.h"

namespace Vulkan
{
VKShader::VKShader(ShaderStage stage, std::vector<u32> spv, VkShaderModule mod,
                   std::string_view name)
    : AbstractShader(stage), m_spv(std::move(spv)), m_module(mod),
      m_compute_pipeline(VK_NULL_HANDLE), m_name(name)
{
  if (!m_name.empty() && g_ActiveConfig.backend_info.bSupportsSettingObjectNames)
  {
    VkDebugUtilsObjectNameInfoEXT name_info = {};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
    name_info.objectHandle = reinterpret_cast<uint64_t>(m_module);
    name_info.pObjectName = m_name.data();
    vkSetDebugUtilsObjectNameEXT(g_vulkan_context->GetDevice(), &name_info);
  }
}

VKShader::VKShader(std::vector<u32> spv, VkPipeline compute_pipeline, std::string_view name)
    : AbstractShader(ShaderStage::Compute), m_spv(std::move(spv)), m_module(VK_NULL_HANDLE),
      m_compute_pipeline(compute_pipeline), m_name(name)
{
  if (!m_name.empty() && g_ActiveConfig.backend_info.bSupportsSettingObjectNames)
  {
    VkDebugUtilsObjectNameInfoEXT name_info = {};
    name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType = VK_OBJECT_TYPE_PIPELINE;
    name_info.objectHandle = reinterpret_cast<uint64_t>(m_compute_pipeline);
    name_info.pObjectName = m_name.data();
    vkSetDebugUtilsObjectNameEXT(g_vulkan_context->GetDevice(), &name_info);
  }
}

VKShader::~VKShader()
{
  if (m_stage != ShaderStage::Compute)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_module, nullptr);
  else
    vkDestroyPipeline(g_vulkan_context->GetDevice(), m_compute_pipeline, nullptr);
}

AbstractShader::BinaryData VKShader::GetBinary() const
{
  BinaryData ret(sizeof(u32) * m_spv.size());
  std::memcpy(ret.data(), m_spv.data(), sizeof(u32) * m_spv.size());
  return ret;
}

static std::unique_ptr<VKShader>
CreateShaderObject(ShaderStage stage, ShaderCompiler::SPIRVCodeVector spv, std::string_view name)
{
  VkShaderModuleCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = spv.size() * sizeof(u32);
  info.pCode = spv.data();

  VkShaderModule mod;
  VkResult res = vkCreateShaderModule(g_vulkan_context->GetDevice(), &info, nullptr, &mod);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateShaderModule failed: ");
    return VK_NULL_HANDLE;
  }

  // If it's a graphics shader, we defer pipeline creation.
  if (stage != ShaderStage::Compute)
    return std::make_unique<VKShader>(stage, std::move(spv), mod, name);

  // If it's a compute shader, we create the pipeline straight away.
  const VkComputePipelineCreateInfo pipeline_info = {
      VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      nullptr,
      0,
      {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT,
       mod, "main", nullptr},
      g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_COMPUTE),
      VK_NULL_HANDLE,
      -1};

  VkPipeline pipeline;
  res = vkCreateComputePipelines(g_vulkan_context->GetDevice(), g_object_cache->GetPipelineCache(),
                                 1, &pipeline_info, nullptr, &pipeline);

  // Shader module is no longer needed, now it is compiled to a pipeline.
  vkDestroyShaderModule(g_vulkan_context->GetDevice(), mod, nullptr);

  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateComputePipelines failed: ");
    return nullptr;
  }

  return std::make_unique<VKShader>(std::move(spv), pipeline, name);
}

std::unique_ptr<VKShader> VKShader::CreateFromSource(ShaderStage stage, std::string_view source,
                                                     std::string_view name)
{
  std::optional<ShaderCompiler::SPIRVCodeVector> spv;
  switch (stage)
  {
  case ShaderStage::Vertex:
    spv = ShaderCompiler::CompileVertexShader(source);
    break;
  case ShaderStage::Geometry:
    spv = ShaderCompiler::CompileGeometryShader(source);
    break;
  case ShaderStage::Pixel:
    spv = ShaderCompiler::CompileFragmentShader(source);
    break;
  case ShaderStage::Compute:
    spv = ShaderCompiler::CompileComputeShader(source);
    break;
  default:
    break;
  }

  if (!spv)
    return nullptr;

  return CreateShaderObject(stage, std::move(*spv), name);
}

std::unique_ptr<VKShader> VKShader::CreateFromBinary(ShaderStage stage, const void* data,
                                                     size_t length, std::string_view name)
{
  const size_t size_in_words = Common::AlignUp(length, sizeof(ShaderCompiler::SPIRVCodeType)) /
                               sizeof(ShaderCompiler::SPIRVCodeType);
  ShaderCompiler::SPIRVCodeVector spv(size_in_words);
  if (length > 0)
    std::memcpy(spv.data(), data, length);

  return CreateShaderObject(stage, std::move(spv), name);
}

}  // namespace Vulkan
