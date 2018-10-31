// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/VulkanLoader.h"
#include "VideoCommon/AbstractPipeline.h"

namespace Vulkan
{
class VKPipeline final : public AbstractPipeline
{
public:
  explicit VKPipeline(VkPipeline pipeline, VkPipelineLayout pipeline_layout,
                      AbstractPipelineUsage usage);
  ~VKPipeline() override;

  VkPipeline GetVkPipeline() const { return m_pipeline; }
  VkPipelineLayout GetVkPipelineLayout() const { return m_pipeline_layout; }
  AbstractPipelineUsage GetUsage() const { return m_usage; }
  static std::unique_ptr<VKPipeline> Create(const AbstractPipelineConfig& config);

private:
  VkPipeline m_pipeline;
  VkPipelineLayout m_pipeline_layout;
  AbstractPipelineUsage m_usage;
};

}  // namespace Vulkan
