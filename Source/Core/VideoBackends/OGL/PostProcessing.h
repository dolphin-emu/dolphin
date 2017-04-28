// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>

#include "Common/GL/GLUtil.h"

#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace OGL
{
class OpenGLPostProcessing : public PostProcessingShaderImplementation
{
public:
  OpenGLPostProcessing();
  ~OpenGLPostProcessing();

  void BlitFromTexture(TargetRectangle src, TargetRectangle dst, int src_texture, int src_width,
                       int src_height, int layer);
  void ApplyShader();

private:
  bool m_initialized;
  SHADER m_shader;
  GLuint m_uniform_resolution;
  GLuint m_uniform_src_rect;
  GLuint m_uniform_time;
  GLuint m_uniform_layer;
  std::string m_glsl_header;

  std::unordered_map<std::string, GLuint> m_uniform_bindings;

  void CreateHeader();
  std::string LoadShaderOptions();
};

}  // namespace
