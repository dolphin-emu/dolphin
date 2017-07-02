// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/PostProcessing.h"

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "Core/Config/GraphicsSettings.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"
#include "VideoBackends/OGL/SamplerCache.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
static const char s_vertex_shader[] = "out vec2 uv0;\n"
                                      "uniform vec4 src_rect;\n"
                                      "void main(void) {\n"
                                      "	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
                                      "	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
                                      "	uv0 = rawpos * src_rect.zw + src_rect.xy;\n"
                                      "}\n";

OpenGLPostProcessing::OpenGLPostProcessing() : m_initialized(false)
{
  CreateHeader();
}

OpenGLPostProcessing::~OpenGLPostProcessing()
{
  m_shader.Destroy();
}

void OpenGLPostProcessing::BlitFromTexture(TargetRectangle src, TargetRectangle dst,
                                           int src_texture, int src_width, int src_height,
                                           int layer)
{
  ApplyShader();

  glViewport(dst.left, dst.bottom, dst.GetWidth(), dst.GetHeight());

  OpenGL_BindAttributelessVAO();

  m_shader.Bind();

  glUniform4f(m_uniform_resolution, (float)src_width, (float)src_height, 1.0f / (float)src_width,
              1.0f / (float)src_height);
  glUniform4f(m_uniform_src_rect, src.left / (float)src_width, src.bottom / (float)src_height,
              src.GetWidth() / (float)src_width, src.GetHeight() / (float)src_height);
  glUniform1ui(m_uniform_time, (GLuint)m_timer.GetTimeElapsed());
  glUniform1i(m_uniform_layer, layer);

  if (m_config.IsDirty())
  {
    for (auto& it : m_config.GetOptions())
    {
      if (it.second.m_dirty)
      {
        switch (it.second.m_type)
        {
        case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL:
          glUniform1i(m_uniform_bindings[it.first], it.second.m_bool_value);
          break;
        case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER:
          switch (it.second.m_integer_values.size())
          {
          case 1:
            glUniform1i(m_uniform_bindings[it.first], it.second.m_integer_values[0]);
            break;
          case 2:
            glUniform2i(m_uniform_bindings[it.first], it.second.m_integer_values[0],
                        it.second.m_integer_values[1]);
            break;
          case 3:
            glUniform3i(m_uniform_bindings[it.first], it.second.m_integer_values[0],
                        it.second.m_integer_values[1], it.second.m_integer_values[2]);
            break;
          case 4:
            glUniform4i(m_uniform_bindings[it.first], it.second.m_integer_values[0],
                        it.second.m_integer_values[1], it.second.m_integer_values[2],
                        it.second.m_integer_values[3]);
            break;
          }
          break;
        case PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT:
          switch (it.second.m_float_values.size())
          {
          case 1:
            glUniform1f(m_uniform_bindings[it.first], it.second.m_float_values[0]);
            break;
          case 2:
            glUniform2f(m_uniform_bindings[it.first], it.second.m_float_values[0],
                        it.second.m_float_values[1]);
            break;
          case 3:
            glUniform3f(m_uniform_bindings[it.first], it.second.m_float_values[0],
                        it.second.m_float_values[1], it.second.m_float_values[2]);
            break;
          case 4:
            glUniform4f(m_uniform_bindings[it.first], it.second.m_float_values[0],
                        it.second.m_float_values[1], it.second.m_float_values[2],
                        it.second.m_float_values[3]);
            break;
          }
          break;
        }
        it.second.m_dirty = false;
      }
    }
    m_config.SetDirty(false);
  }

  glActiveTexture(GL_TEXTURE9);
  glBindTexture(GL_TEXTURE_2D_ARRAY, src_texture);
  g_sampler_cache->BindLinearSampler(9);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void OpenGLPostProcessing::ApplyShader()
{
  // shader didn't changed
  if (m_initialized && m_config.GetShader() == g_ActiveConfig.sPostProcessingShader)
    return;

  m_shader.Destroy();
  m_uniform_bindings.clear();

  // load shader code
  std::string main_code = m_config.LoadShader();
  std::string options_code = LoadShaderOptions();
  std::string code = m_glsl_header + options_code + main_code;

  // and compile it
  if (!ProgramShaderCache::CompileShader(m_shader, s_vertex_shader, code))
  {
    ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s", m_config.GetShader().c_str());
    Config::SetCurrent(Config::GFX_ENHANCE_POST_SHADER, std::string(""));
    code = m_config.LoadShader();
    ProgramShaderCache::CompileShader(m_shader, s_vertex_shader, code);
  }

  // read uniform locations
  m_uniform_resolution = glGetUniformLocation(m_shader.glprogid, "resolution");
  m_uniform_time = glGetUniformLocation(m_shader.glprogid, "time");
  m_uniform_src_rect = glGetUniformLocation(m_shader.glprogid, "src_rect");
  m_uniform_layer = glGetUniformLocation(m_shader.glprogid, "layer");

  for (const auto& it : m_config.GetOptions())
  {
    std::string glsl_name = "options." + it.first;
    m_uniform_bindings[it.first] = glGetUniformLocation(m_shader.glprogid, glsl_name.c_str());
  }
  m_initialized = true;
}

void OpenGLPostProcessing::CreateHeader()
{
  m_glsl_header =
      // Required variables
      // Shouldn't be accessed directly by the PP shader
      // Texture sampler
      "SAMPLER_BINDING(8) uniform sampler2D samp8;\n"
      "SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"

      // Output variable
      "out float4 ocol0;\n"
      // Input coordinates
      "in float2 uv0;\n"
      // Resolution
      "uniform float4 resolution;\n"
      // Time
      "uniform uint time;\n"
      // Layer
      "uniform int layer;\n"

      // Interfacing functions
      "float4 Sample()\n"
      "{\n"
      "\treturn texture(samp9, float3(uv0, layer));\n"
      "}\n"

      "float4 SampleLocation(float2 location)\n"
      "{\n"
      "\treturn texture(samp9, float3(location, layer));\n"
      "}\n"

      "float4 SampleLayer(int layer)\n"
      "{\n"
      "\treturn texture(samp9, float3(uv0, layer));\n"
      "}\n"

      "#define SampleOffset(offset) textureOffset(samp9, float3(uv0, layer), offset)\n"

      "float4 SampleFontLocation(float2 location)\n"
      "{\n"
      "\treturn texture(samp8, location);\n"
      "}\n"

      "float2 GetResolution()\n"
      "{\n"
      "\treturn resolution.xy;\n"
      "}\n"

      "float2 GetInvResolution()\n"
      "{\n"
      "\treturn resolution.zw;\n"
      "}\n"

      "float2 GetCoordinates()\n"
      "{\n"
      "\treturn uv0;\n"
      "}\n"

      "uint GetTime()\n"
      "{\n"
      "\treturn time;\n"
      "}\n"

      "void SetOutput(float4 color)\n"
      "{\n"
      "\tocol0 = color;\n"
      "}\n"

      "#define GetOption(x) (options.x)\n"
      "#define OptionEnabled(x) (options.x != 0)\n";
}

std::string OpenGLPostProcessing::LoadShaderOptions()
{
  m_uniform_bindings.clear();
  if (m_config.GetOptions().empty())
    return "";

  std::string glsl_options = "struct Options\n{\n";

  for (const auto& it : m_config.GetOptions())
  {
    if (it.second.m_type ==
        PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_BOOL)
    {
      glsl_options += StringFromFormat("int     %s;\n", it.first.c_str());
    }
    else if (it.second.m_type ==
             PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
    {
      u32 count = static_cast<u32>(it.second.m_integer_values.size());
      if (count == 1)
        glsl_options += StringFromFormat("int     %s;\n", it.first.c_str());
      else
        glsl_options += StringFromFormat("int%d   %s;\n", count, it.first.c_str());
    }
    else if (it.second.m_type ==
             PostProcessingShaderConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT)
    {
      u32 count = static_cast<u32>(it.second.m_float_values.size());
      if (count == 1)
        glsl_options += StringFromFormat("float   %s;\n", it.first.c_str());
      else
        glsl_options += StringFromFormat("float%d %s;\n", count, it.first.c_str());
    }

    m_uniform_bindings[it.first] = 0;
  }

  glsl_options += "};\n";
  glsl_options += "uniform Options options;\n";

  return glsl_options;
}

}  // namespace OGL
