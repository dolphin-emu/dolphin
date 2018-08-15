// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"

#include "VideoBackends/OGL/OGLPipeline.h"
#include "VideoBackends/OGL/OGLShader.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/VertexManager.h"
#include "Common/GL/GLUtil.h"

namespace OGL
{
OGLPipeline::OGLPipeline(const GLVertexFormat* vertex_format,
                         const RasterizationState& rasterization_state,
                         const DepthState& depth_state, const BlendingState& blending_state,
                         const PipelineProgram* program, GLuint gl_primitive)
    : m_vertex_format(vertex_format), m_rasterization_state(rasterization_state),
      m_depth_state(depth_state), m_blending_state(blending_state), m_program(program),
      m_gl_primitive(gl_primitive)
{
}

OGLPipeline::~OGLPipeline()
{
  // We don't want to destroy the shaders.
  ProgramShaderCache::ReleasePipelineProgram(m_program);
}

std::unique_ptr<OGLPipeline> OGLPipeline::Create(const AbstractPipelineConfig& config)
{
  const PipelineProgram* program = ProgramShaderCache::GetPipelineProgram(
      static_cast<const GLVertexFormat*>(config.vertex_format),
      static_cast<const OGLShader*>(config.vertex_shader),
      static_cast<const OGLShader*>(config.geometry_shader),
      static_cast<const OGLShader*>(config.pixel_shader));
  if (!program)
    return nullptr;

  const GLVertexFormat* vertex_format = static_cast<const GLVertexFormat*>(config.vertex_format);
  GLenum gl_primitive = GLUtil::MapToGLPrimitive(config.rasterization_state.primitive);
  return std::make_unique<OGLPipeline>(vertex_format, config.rasterization_state,
                                       config.depth_state, config.blending_state, program,
                                       gl_primitive);
}
}  // namespace OGL
