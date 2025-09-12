// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/OGLShader.h"

#include <algorithm>
#include <cctype>

#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/ShaderCompileUtils.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
namespace
{
std::string ResolveIncludeStatements(glslang::TShader::Includer* shader_includer,
                                     std::string_view source, const char* includer_name = "",
                                     std::size_t depth = 1)
{
  if (!shader_includer)
  {
    return std::string{source};
  }

  std::string source_str(source);
  std::istringstream iss(source_str);

  ShaderCode output;
  std::string line;
  while (std::getline(iss, line))
  {
    if (line.empty())
    {
      output.Write("{}\n", line);
      continue;
    }

    std::string_view include_preprocessor = "#include";
    if (!line.starts_with(include_preprocessor))
    {
      output.Write("{}\n", line);
      continue;
    }

    const std::string after_include = line.substr(include_preprocessor.size());

    bool local_include = true;
    std::string_view filename;
    // First non-whitespace character after include
    const std::string::const_iterator non_whitespace_iter = std::find_if_not(
        after_include.begin(), after_include.end(), [](char ch) { return std::isspace(ch); });
    if (*non_whitespace_iter == '<')
    {
      const auto after_less = std::next(non_whitespace_iter);
      if (after_less == after_include.end())
      {
        // Found less-than at the end, malformed
        output.Write("{}\n", line);
        continue;
      }
      const auto end_iter = std::find(after_less, after_include.end(), '>');
      if (end_iter == after_include.end())
      {
        // Include spans multiple lines or is malformed, just pass it along
        output.Write("{}\n", line);
        continue;
      }
      filename = std::string_view(after_less, end_iter);
      local_include = false;
    }
    else if (*non_whitespace_iter == '"')
    {
      const auto after_quote = std::next(non_whitespace_iter);
      if (after_quote == after_include.end())
      {
        // Found quote at the end, malformed
        output.Write("{}\n", line);
        continue;
      }
      const auto end_iter = std::find(after_quote, after_include.end(), '"');
      if (end_iter == after_include.end())
      {
        // Include spans multiple lines or is malformed, just pass it along
        output.Write("{}\n", line);
        continue;
      }
      filename = std::string_view(after_quote, end_iter);
    }

    const std::string header_path = std::string{filename};
    auto include_result =
        local_include ? shader_includer->includeLocal(header_path.c_str(), includer_name, depth) :
                        shader_includer->includeSystem(header_path.c_str(), includer_name, depth);
    if (!include_result)
    {
      output.Write("{}\n", line);
      continue;
    }

    output.Write("{}", ResolveIncludeStatements(shader_includer, include_result->headerData,
                                                header_path.c_str(), ++depth));
  }

  return output.GetBuffer();
}
}  // namespace
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
  if (!m_name.empty() && g_backend_info.bSupportsSettingObjectNames)
  {
    glObjectLabel(GL_SHADER, m_gl_id, (GLsizei)m_name.size(), m_name.c_str());
  }
}

OGLShader::OGLShader(GLuint gl_compute_program_id, std::string source, std::string name)
    : AbstractShader(ShaderStage::Compute), m_id(ProgramShaderCache::GenerateShaderID()),
      m_type(GL_COMPUTE_SHADER), m_gl_compute_program_id(gl_compute_program_id),
      m_source(std::move(source)), m_name(std::move(name))
{
  if (!m_name.empty() && g_backend_info.bSupportsSettingObjectNames)
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
                                                       VideoCommon::ShaderIncluder* shader_includer,
                                                       std::string_view name)
{
  // Note: while the source will all be available, any errors will not
  // reference the correct paths
  std::string source_str = ResolveIncludeStatements(shader_includer, source);
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
