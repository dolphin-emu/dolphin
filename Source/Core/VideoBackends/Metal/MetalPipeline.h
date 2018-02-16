// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/AbstractPipeline.h"

namespace Metal
{
class MetalPipeline final : public AbstractPipeline
{
public:
  explicit MetalPipeline(const mtlpp::RenderPipelineState& rps, const mtlpp::DepthStencilState& dss,
                         mtlpp::DepthClipMode depth_clip_mode, mtlpp::CullMode cull_mode,
                         mtlpp::PrimitiveType primitive_type);
  ~MetalPipeline() override;

  const mtlpp::RenderPipelineState& GetRenderPipelineState() const;
  const mtlpp::DepthStencilState& GetDepthStencilState() const;
  mtlpp::DepthClipMode GetDepthClipMode() const;
  mtlpp::CullMode GetCullMode() const;
  mtlpp::PrimitiveType GetPrimitiveType() const;

  static std::unique_ptr<MetalPipeline> Create(const AbstractPipelineConfig& config);

private:
  mtlpp::RenderPipelineState m_render_pipeline_state;
  mtlpp::DepthStencilState m_depth_stencil_state;
  mtlpp::DepthClipMode m_depth_clip_mode;
  mtlpp::CullMode m_cull_mode;
  mtlpp::PrimitiveType m_primitive_type;
};
}  // namespace Metal
