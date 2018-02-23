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
  explicit VKPipeline(VkPipeline pipeline);
  ~VKPipeline() override;

  VkPipeline GetPipeline() const { return m_pipeline; }
  static std::unique_ptr<VKPipeline> Create(const AbstractPipelineConfig& config);

private:
  VkPipeline m_pipeline;
};

}  // namespace Vulkan
