// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoBackends/Vulkan/VulkanLoader.h"
#include "VideoCommon/AbstractShader.h"

namespace Vulkan
{
class VKShader final : public AbstractShader
{
public:
  VKShader(ShaderStage stage, std::vector<u32> spv, VkShaderModule mod);
  VKShader(std::vector<u32> spv, VkPipeline compute_pipeline);
  ~VKShader() override;

  VkShaderModule GetShaderModule() const { return m_module; }
  VkPipeline GetComputePipeline() const { return m_compute_pipeline; }
  BinaryData GetBinary() const override;

  static std::unique_ptr<VKShader> CreateFromSource(ShaderStage stage, std::string_view source);
  static std::unique_ptr<VKShader> CreateFromBinary(ShaderStage stage, const void* data,
                                                    size_t length);

private:
  std::vector<u32> m_spv;
  VkShaderModule m_module;
  VkPipeline m_compute_pipeline;
};

}  // namespace Vulkan
