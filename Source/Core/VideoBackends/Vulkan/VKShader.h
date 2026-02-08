// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <string>
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
  VKShader(ShaderStage stage, std::vector<u32> spv, VkShaderModule mod, std::string_view name);
  VKShader(std::vector<u32> spv, VkPipeline compute_pipeline, std::string_view name);
  ~VKShader() override;

  VkShaderModule GetShaderModule() const { return m_module; }
  VkPipeline GetComputePipeline() const { return m_compute_pipeline; }
  BinaryData GetBinary() const override;

  static std::unique_ptr<VKShader> CreateFromSource(ShaderStage stage, std::string_view source,
                                                    std::string_view name);
  static std::unique_ptr<VKShader> CreateFromBinary(ShaderStage stage, const void* data,
                                                    size_t length, std::string_view name);

private:
  std::vector<u32> m_spv;
  VkShaderModule m_module;
  VkPipeline m_compute_pipeline;
  std::string m_name;
};

}  // namespace Vulkan
