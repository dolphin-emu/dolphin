// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLShader.h"

#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/VideoConfig.h"

namespace OGL
{
static GLenum GetGLShaderTypeForStage(ShaderStage stage)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
    return GL_VERTEX_SHADER;
  case ShaderStage::Geometry:
    return GL_GEOMETRY_SHADER;
  case ShaderStage::Pixel:
    return GL_FRAGMENT_SHADER;
  case ShaderStage::Compute:
    return GL_COMPUTE_SHADER;
  default:
    return 0;
  }
}

OGLShader::OGLShader(ShaderStage stage, GLenum gl_type, GLuint gl_id, std::string source,
                     std::string name)
    : AbstractShader(stage), m_id(ProgramShaderCache::GenerateShaderID()), m_type(gl_type),
      m_gl_id(gl_id), m_source(std::move(source)), m_name(std::move(name))
{
  if (!m_name.empty() && g_ActiveConfig.backend_info.bSupportsSettingObjectNames)
  {
    glObjectLabel(GL_SHADER, m_gl_id, (GLsizei)m_name.size(), m_name.c_str());
  }
}

OGLShader::OGLShader(GLuint gl_compute_program_id, std::string source, std::string name)
    : AbstractShader(ShaderStage::Compute), m_id(ProgramShaderCache::GenerateShaderID()),
      m_type(GL_COMPUTE_SHADER), m_gl_compute_program_id(gl_compute_program_id),
      m_source(std::move(source)), m_name(std::move(name))
{
  if (!m_name.empty() && g_ActiveConfig.backend_info.bSupportsSettingObjectNames)
  {
    glObjectLabel(GL_PROGRAM, m_gl_compute_program_id, (GLsizei)m_name.size(), m_name.c_str());
  }
}

OGLShader::~OGLShader()
{
  if (m_stage != ShaderStage::Compute)
    glDeleteShader(m_gl_id);
  else
    glDeleteProgram(m_gl_compute_program_id);
}

std::unique_ptr<OGLShader> OGLShader::CreateFromSource(ShaderStage stage, std::string_view source,
                                                       std::string_view name)
{
  std::string source_str(source);
  std::string name_str(name);
  if (stage != ShaderStage::Compute)
  {
    GLenum shader_type = GetGLShaderTypeForStage(stage);
    GLuint shader_id = ProgramShaderCache::CompileSingleShader(shader_type, source_str);
    if (!shader_id)
      return nullptr;

    return std::make_unique<OGLShader>(stage, shader_type, shader_id, std::move(source_str),
                                       std::move(name_str));
  }

  // Compute shaders.
  SHADER prog;
  if (!ProgramShaderCache::CompileComputeShader(prog, source_str))
    return nullptr;
  return std::make_unique<OGLShader>(prog.glprogid, std::move(source_str), std::move(name_str));
}

}  // namespace OGL
