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
                         PipelineProgram* program, GLuint gl_primitive)
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

AbstractPipeline::CacheData OGLPipeline::GetCacheData() const
{
  // More than one pipeline can share the same shaders. To avoid bloating the cache with multiple
  // copies of the same program combination, we set a flag on the program object so that it can't
  // be retrieved again. When booting, the pipeline cache is loaded in-order, so the additional
  // pipelines which use the program combination will re-use the already-created object.
  if (!g_ActiveConfig.backend_info.bSupportsPipelineCacheData || m_program->binary_retrieved)
    return {};

  GLint program_size = 0;
  glGetProgramiv(m_program->shader.glprogid, GL_PROGRAM_BINARY_LENGTH, &program_size);
  if (program_size == 0)
    return {};

  // Clear any existing error.
  glGetError();

  // We pack the format at the start of the buffer.
  CacheData data(program_size + sizeof(u32));
  GLsizei data_size = 0;
  GLenum program_format = 0;
  glGetProgramBinary(m_program->shader.glprogid, program_size, &data_size, &program_format,
                     &data[sizeof(u32)]);
  if (glGetError() != GL_NO_ERROR || data_size == 0)
    return {};

  u32 program_format_u32 = static_cast<u32>(program_format);
  std::memcpy(&data[0], &program_format_u32, sizeof(u32));
  data.resize(data_size + sizeof(u32));
  m_program->binary_retrieved = true;
  return data;
}

std::unique_ptr<OGLPipeline> OGLPipeline::Create(const AbstractPipelineConfig& config,
                                                 const void* cache_data, size_t cache_data_size)
{
  PipelineProgram* program = ProgramShaderCache::GetPipelineProgram(
      static_cast<const GLVertexFormat*>(config.vertex_format),
      static_cast<const OGLShader*>(config.vertex_shader),
      static_cast<const OGLShader*>(config.geometry_shader),
      static_cast<const OGLShader*>(config.pixel_shader), cache_data, cache_data_size);
  if (!program)
    return nullptr;

  const GLVertexFormat* vertex_format = static_cast<const GLVertexFormat*>(config.vertex_format);
  GLenum gl_primitive = GLUtil::MapToGLPrimitive(config.rasterization_state.primitive);
  return std::make_unique<OGLPipeline>(vertex_format, config.rasterization_state,
                                       config.depth_state, config.blending_state, program,
                                       gl_primitive);
}
}  // namespace OGL
