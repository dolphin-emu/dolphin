// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/OGLShader.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"

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

OGLShader::OGLShader(ShaderStage stage, GLenum gl_type, GLuint gl_id)
    : AbstractShader(stage), m_id(ProgramShaderCache::GenerateShaderID()), m_type(gl_type),
      m_gl_id(gl_id)
{
}

OGLShader::OGLShader(GLuint gl_compute_program_id)
    : AbstractShader(ShaderStage::Compute), m_id(ProgramShaderCache::GenerateShaderID()),
      m_type(GL_COMPUTE_SHADER), m_gl_compute_program_id(gl_compute_program_id)
{
}

OGLShader::~OGLShader()
{
  if (m_stage != ShaderStage::Compute)
    glDeleteShader(m_gl_id);
  else
    glDeleteProgram(m_gl_compute_program_id);
}

bool OGLShader::HasBinary() const
{
  // NOTE: GL shaders do not have binaries, programs do.
  return false;
}

AbstractShader::BinaryData OGLShader::GetBinary() const
{
  return {};
}

std::unique_ptr<OGLShader> OGLShader::CreateFromSource(ShaderStage stage, const char* source,
                                                       size_t length)
{
  if (stage != ShaderStage::Compute)
  {
    GLenum shader_type = GetGLShaderTypeForStage(stage);
    GLuint shader_id =
        ProgramShaderCache::CompileSingleShader(shader_type, std::string(source, length));
    if (!shader_id)
      return nullptr;

    return std::make_unique<OGLShader>(stage, shader_type, shader_id);
  }

  // Compute shaders.
  SHADER prog;
  if (!ProgramShaderCache::CompileComputeShader(prog, std::string(source, length)))
    return nullptr;
  return std::make_unique<OGLShader>(prog.glprogid);
}

}  // namespace OGL
