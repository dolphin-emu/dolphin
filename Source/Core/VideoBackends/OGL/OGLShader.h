// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "VideoCommon/AbstractShader.h"

namespace OGL
{
class OGLShader final : public AbstractShader
{
public:
  explicit OGLShader(ShaderStage stage, GLenum gl_type, GLuint gl_id, std::string source);
  explicit OGLShader(GLuint gl_compute_program_id, std::string source);
  ~OGLShader() override;

  u64 GetID() const { return m_id; }
  GLenum GetGLShaderType() const { return m_type; }
  GLuint GetGLShaderID() const { return m_gl_id; }
  GLuint GetGLComputeProgramID() const { return m_gl_compute_program_id; }
  const std::string& GetSource() const { return m_source; }

  static std::unique_ptr<OGLShader> CreateFromSource(ShaderStage stage, std::string_view source);

private:
  u64 m_id;
  GLenum m_type;
  GLuint m_gl_id = 0;
  GLuint m_gl_compute_program_id = 0;
  std::string m_source;
};

}  // namespace OGL
