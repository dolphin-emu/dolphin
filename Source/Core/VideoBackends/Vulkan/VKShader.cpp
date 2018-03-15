// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Align.h"
#include "Common/Assert.h"

#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
VKShader::VKShader(ShaderStage stage, std::vector<u32> spv, VkShaderModule mod)
    : AbstractShader(stage), m_spv(std::move(spv)), m_module(mod),
      m_compute_pipeline(VK_NULL_HANDLE)
{
}

VKShader::VKShader(std::vector<u32> spv, VkPipeline compute_pipeline)
    : AbstractShader(ShaderStage::Compute), m_spv(std::move(spv)), m_module(VK_NULL_HANDLE),
      m_compute_pipeline(compute_pipeline)
{
}

VKShader::~VKShader()
{
  if (m_stage != ShaderStage::Compute)
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_module, nullptr);
  else
    vkDestroyPipeline(g_vulkan_context->GetDevice(), m_compute_pipeline, nullptr);
}

bool VKShader::HasBinary() const
{
  ASSERT(!m_spv.empty());
  return true;
}

AbstractShader::BinaryData VKShader::GetBinary() const
{
  BinaryData ret(sizeof(u32) * m_spv.size());
  std::memcpy(ret.data(), m_spv.data(), sizeof(u32) * m_spv.size());
  return ret;
}

static std::unique_ptr<VKShader> CreateShaderObject(ShaderStage stage,
                                                    ShaderCompiler::SPIRVCodeVector spv)
{
  VkShaderModule mod = Util::CreateShaderModule(spv.data(), spv.size());
  if (mod == VK_NULL_HANDLE)
    return nullptr;

  // If it's a graphics shader, we defer pipeline creation.
  if (stage != ShaderStage::Compute)
    return std::make_unique<VKShader>(stage, std::move(spv), mod);

  // If it's a compute shader, we create the pipeline straight away.
  ComputePipelineInfo pinfo;
  pinfo.pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_COMPUTE);
  pinfo.cs = mod;
  VkPipeline pipeline = g_shader_cache->CreateComputePipeline(pinfo);
  if (pipeline == VK_NULL_HANDLE)
  {
    vkDestroyShaderModule(g_vulkan_context->GetDevice(), mod, nullptr);
    return nullptr;
  }

  // Shader module is no longer needed, now it is compiled to a pipeline.
  return std::make_unique<VKShader>(std::move(spv), pipeline);
}

std::unique_ptr<VKShader> VKShader::CreateFromSource(ShaderStage stage, const char* source,
                                                     size_t length)
{
  ShaderCompiler::SPIRVCodeVector spv;
  bool result;
  switch (stage)
  {
  case ShaderStage::Vertex:
    result = ShaderCompiler::CompileVertexShader(&spv, source, length);
    break;
  case ShaderStage::Geometry:
    result = ShaderCompiler::CompileGeometryShader(&spv, source, length);
    break;
  case ShaderStage::Pixel:
    result = ShaderCompiler::CompileFragmentShader(&spv, source, length);
    break;
  case ShaderStage::Compute:
    result = ShaderCompiler::CompileComputeShader(&spv, source, length);
    break;
  default:
    result = false;
    break;
  }

  if (!result)
    return nullptr;

  return CreateShaderObject(stage, std::move(spv));
}

std::unique_ptr<VKShader> VKShader::CreateFromBinary(ShaderStage stage, const void* data,
                                                     size_t length)
{
  const size_t size_in_words = Common::AlignUp(length, sizeof(ShaderCompiler::SPIRVCodeType)) /
                               sizeof(ShaderCompiler::SPIRVCodeType);
  ShaderCompiler::SPIRVCodeVector spv(size_in_words);
  if (length > 0)
    std::memcpy(spv.data(), data, length);

  return CreateShaderObject(stage, std::move(spv));
}

}  // namespace Vulkan
