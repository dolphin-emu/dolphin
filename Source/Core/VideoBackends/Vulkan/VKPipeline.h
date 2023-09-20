// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoBackends/Vulkan/VulkanLoader.h"
#include "VideoCommon/AbstractPipeline.h"

namespace Vulkan
{
class VKPipeline final : public AbstractPipeline
{
public:
  explicit VKPipeline(const AbstractPipelineConfig& config, VkPipeline pipeline,
                      VkPipelineLayout pipeline_layout, AbstractPipelineUsage usage);
  ~VKPipeline() override;

  VkPipeline GetVkPipeline() const { return m_pipeline; }
  VkPipelineLayout GetVkPipelineLayout() const { return m_pipeline_layout; }
  AbstractPipelineUsage GetUsage() const { return m_usage; }
  static std::unique_ptr<VKPipeline> Create(const AbstractPipelineConfig& config);

  u64 GetVSHash() const { return m_vsHash; }
  u64 GetPSHash() const { return m_psHash; }
  u64 GetGSHash() const { return m_gsHash; }

private:
  VkPipeline m_pipeline;
  VkPipelineLayout m_pipeline_layout;
  AbstractPipelineUsage m_usage;
  u64 m_vsHash = 0;
  u64 m_psHash = 0;
  u64 m_gsHash = 0;
};

}  // namespace Vulkan
