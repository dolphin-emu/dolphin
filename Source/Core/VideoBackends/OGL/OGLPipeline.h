// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/GL/GLUtil.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/RenderState.h"

namespace OGL
{
class OGLPipeline final : public AbstractPipeline
{
public:
  explicit OGLPipeline(const AbstractPipelineConfig& config, const GLVertexFormat* vertex_format,
                       const RasterizationState& rasterization_state, const DepthState& depth_state,
                       const BlendingState& blending_state, PipelineProgram* program,
                       GLenum gl_primitive);
  ~OGLPipeline() override;

  const GLVertexFormat* GetVertexFormat() const { return m_vertex_format; }
  const RasterizationState& GetRasterizationState() const { return m_rasterization_state; }
  const DepthState& GetDepthState() const { return m_depth_state; }
  const BlendingState& GetBlendingState() const { return m_blending_state; }
  const PipelineProgram* GetProgram() const { return m_program; }
  bool HasVertexInput() const { return m_vertex_format != nullptr; }
  GLenum GetGLPrimitive() const { return m_gl_primitive; }
  CacheData GetCacheData() const override;
  static std::unique_ptr<OGLPipeline> Create(const AbstractPipelineConfig& config,
                                             const void* cache_data, size_t cache_data_size);

private:
  const GLVertexFormat* m_vertex_format;
  RasterizationState m_rasterization_state;
  DepthState m_depth_state;
  BlendingState m_blending_state;
  PipelineProgram* m_program;
  GLenum m_gl_primitive;
};

}  // namespace OGL
