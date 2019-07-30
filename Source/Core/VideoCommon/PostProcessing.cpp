// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/PostProcessing.h"

#include <sstream>
#include <string>

#include "Common/Assert.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoCommon/AbstractFramebuffer.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoConfig.h"

namespace VideoCommon
{
static const char s_default_shader[] = "void main() { SetOutput(Sample()); }\n";

PostProcessingConfiguration::PostProcessingConfiguration() = default;

PostProcessingConfiguration::~PostProcessingConfiguration() = default;

void PostProcessingConfiguration::LoadShader(const std::string& shader)
{
  // Load the shader from the configuration if there isn't one sent to us.
  m_current_shader = shader;
  if (shader.empty())
  {
    LoadDefaultShader();
    return;
  }

  // loading shader code
  std::string code;
  std::string path = File::GetUserPath(D_SHADERS_IDX) + shader + ".glsl";

  if (!File::Exists(path))
  {
    // Fallback to shared user dir
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + shader + ".glsl";
  }

  if (!File::ReadFileToString(path, code))
  {
    ERROR_LOG(VIDEO, "Post-processing shader not found: %s", path.c_str());
    LoadDefaultShader();
    return;
  }

  LoadOptions(code);
  LoadOptionsConfiguration();
  m_current_shader_code = code;
}

void PostProcessingConfiguration::LoadDefaultShader()
{
  m_options.clear();
  m_current_shader_code = s_default_shader;
}

void PostProcessingConfiguration::LoadOptions(const std::string& code)
{
  const std::string config_start_delimiter = "[configuration]";
  const std::string config_end_delimiter = "[/configuration]";
  size_t configuration_start = code.find(config_start_delimiter);
  size_t configuration_end = code.find(config_end_delimiter);

  m_options.clear();

  if (configuration_start == std::string::npos || configuration_end == std::string::npos)
  {
    // Issue loading configuration or there isn't one.
    return;
  }

  std::string configuration_string =
      code.substr(configuration_start + config_start_delimiter.size(),
                  configuration_end - configuration_start - config_start_delimiter.size());

  std::istringstream in(configuration_string);

  struct GLSLStringOption
  {
    std::string m_type;
    std::vector<std::pair<std::string, std::string>> m_options;
  };

  std::vector<GLSLStringOption> option_strings;
  GLSLStringOption* current_strings = nullptr;
  while (!in.eof())
  {
    std::string line;

    if (std::getline(in, line))
    {
#ifndef _WIN32
      // Check for CRLF eol and convert it to LF
      if (!line.empty() && line.at(line.size() - 1) == '\r')
      {
        line.erase(line.size() - 1);
      }
#endif

      if (!line.empty())
      {
        if (line[0] == '[')
        {
          size_t endpos = line.find("]");

          if (endpos != std::string::npos)
          {
            // New section!
            std::string sub = line.substr(1, endpos - 1);
            option_strings.push_back({sub});
            current_strings = &option_strings.back();
          }
        }
        else
        {
          if (current_strings)
          {
            std::string key, value;
            IniFile::ParseLine(line, &key, &value);

            if (!(key.empty() && value.empty()))
              current_strings->m_options.emplace_back(key, value);
          }
        }
      }
    }
  }

  for (const auto& it : option_strings)
  {
    ConfigurationOption option;

    if (it.m_type == "OptionBool")
      option.m_type = ConfigurationOption::OptionType::OPTION_BOOL;
    else if (it.m_type == "OptionRangeFloat")
      option.m_type = ConfigurationOption::OptionType::OPTION_FLOAT;
    else if (it.m_type == "OptionRangeInteger")
      option.m_type = ConfigurationOption::OptionType::OPTION_INTEGER;

    for (const auto& string_option : it.m_options)
    {
      if (string_option.first == "GUIName")
      {
        option.m_gui_name = string_option.second;
      }
      else if (string_option.first == "OptionName")
      {
        option.m_option_name = string_option.second;
      }
      else if (string_option.first == "DependentOption")
      {
        option.m_dependent_option = string_option.second;
      }
      else if (string_option.first == "MinValue" || string_option.first == "MaxValue" ||
               string_option.first == "DefaultValue" || string_option.first == "StepAmount")
      {
        std::vector<s32>* output_integer = nullptr;
        std::vector<float>* output_float = nullptr;

        if (string_option.first == "MinValue")
        {
          output_integer = &option.m_integer_min_values;
          output_float = &option.m_float_min_values;
        }
        else if (string_option.first == "MaxValue")
        {
          output_integer = &option.m_integer_max_values;
          output_float = &option.m_float_max_values;
        }
        else if (string_option.first == "DefaultValue")
        {
          output_integer = &option.m_integer_values;
          output_float = &option.m_float_values;
        }
        else if (string_option.first == "StepAmount")
        {
          output_integer = &option.m_integer_step_values;
          output_float = &option.m_float_step_values;
        }

        if (option.m_type == ConfigurationOption::OptionType::OPTION_BOOL)
        {
          TryParse(string_option.second, &option.m_bool_value);
        }
        else if (option.m_type == ConfigurationOption::OptionType::OPTION_INTEGER)
        {
          TryParseVector(string_option.second, output_integer);
          if (output_integer->size() > 4)
            output_integer->erase(output_integer->begin() + 4, output_integer->end());
        }
        else if (option.m_type == ConfigurationOption::OptionType::OPTION_FLOAT)
        {
          TryParseVector(string_option.second, output_float);
          if (output_float->size() > 4)
            output_float->erase(output_float->begin() + 4, output_float->end());
        }
      }
    }
    m_options[option.m_option_name] = option;
  }
}

void PostProcessingConfiguration::LoadOptionsConfiguration()
{
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  std::string section = m_current_shader + "-options";

  for (auto& it : m_options)
  {
    switch (it.second.m_type)
    {
    case ConfigurationOption::OptionType::OPTION_BOOL:
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &it.second.m_bool_value,
                                           it.second.m_bool_value);
      break;
    case ConfigurationOption::OptionType::OPTION_INTEGER:
    {
      std::string value;
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
      if (!value.empty())
        TryParseVector(value, &it.second.m_integer_values);
    }
    break;
    case ConfigurationOption::OptionType::OPTION_FLOAT:
    {
      std::string value;
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
      if (!value.empty())
        TryParseVector(value, &it.second.m_float_values);
    }
    break;
    }
  }
}

void PostProcessingConfiguration::SaveOptionsConfiguration()
{
  IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  std::string section = m_current_shader + "-options";

  for (auto& it : m_options)
  {
    switch (it.second.m_type)
    {
    case ConfigurationOption::OptionType::OPTION_BOOL:
    {
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, it.second.m_bool_value);
    }
    break;
    case ConfigurationOption::OptionType::OPTION_INTEGER:
    {
      std::string value;
      for (size_t i = 0; i < it.second.m_integer_values.size(); ++i)
        value += StringFromFormat("%d%s", it.second.m_integer_values[i],
                                  i == (it.second.m_integer_values.size() - 1) ? "" : ", ");
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value);
    }
    break;
    case ConfigurationOption::OptionType::OPTION_FLOAT:
    {
      std::ostringstream value;
      value.imbue(std::locale("C"));

      for (size_t i = 0; i < it.second.m_float_values.size(); ++i)
      {
        value << it.second.m_float_values[i];
        if (i != (it.second.m_float_values.size() - 1))
          value << ", ";
      }
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value.str());
    }
    break;
    }
  }
  ini.Save(File::GetUserPath(F_DOLPHINCONFIG_IDX));
}

void PostProcessingConfiguration::SetOptionf(const std::string& option, int index, float value)
{
  auto it = m_options.find(option);

  it->second.m_float_values[index] = value;
}

void PostProcessingConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
  auto it = m_options.find(option);

  it->second.m_integer_values[index] = value;
}

void PostProcessingConfiguration::SetOptionb(const std::string& option, bool value)
{
  auto it = m_options.find(option);

  it->second.m_bool_value = value;
}

PostProcessing::PostProcessing()
{
  m_timer.Start();
}

PostProcessing::~PostProcessing()
{
  m_timer.Stop();
}

static std::vector<std::string> GetShaders(const std::string& sub_dir = "")
{
  std::vector<std::string> paths =
      Common::DoFileSearch({File::GetUserPath(D_SHADERS_IDX) + sub_dir,
                            File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir},
                           {".glsl"});
  std::vector<std::string> result;
  for (std::string path : paths)
  {
    std::string name;
    SplitPath(path, nullptr, &name, nullptr);
    result.push_back(name);
  }
  return result;
}

std::vector<std::string> PostProcessing::GetShaderList()
{
  return GetShaders();
}

bool PostProcessing::Initialize(AbstractTextureFormat format)
{
  m_framebuffer_format = format;
  return CompileVertexShader() && CompilePixelShader() && CompilePipeline();
}

void PostProcessing::RecompileShader()
{
  m_pipeline.reset();
  m_pixel_shader.reset();
  if (!CompilePixelShader())
    return;

  CompilePipeline();
}

void PostProcessing::RecompilePipeline()
{
  m_pipeline.reset();
  CompilePipeline();
}

void PostProcessing::BlitFromTexture(const MathUtil::Rectangle<int>& dst,
                                     const MathUtil::Rectangle<int>& src,
                                     const AbstractTexture* src_tex, int src_layer)
{
  if (g_renderer->GetCurrentFramebuffer()->GetColorFormat() != m_framebuffer_format)
  {
    m_framebuffer_format = g_renderer->GetCurrentFramebuffer()->GetColorFormat();
    RecompilePipeline();
  }

  if (!m_pipeline)
    return;

  FillUniformBuffer(src, src_tex, src_layer);
  g_vertex_manager->UploadUtilityUniforms(m_uniform_staging_buffer.data(),
                                          static_cast<u32>(m_uniform_staging_buffer.size()));

  g_renderer->SetViewportAndScissor(
      g_renderer->ConvertFramebufferRectangle(dst, g_renderer->GetCurrentFramebuffer()));
  g_renderer->SetPipeline(m_pipeline.get());
  g_renderer->SetTexture(0, src_tex);
  g_renderer->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_renderer->Draw(0, 3);
}

std::string PostProcessing::GetUniformBufferHeader() const
{
  std::stringstream ss;
  u32 unused_counter = 1;
  if (g_ActiveConfig.backend_info.api_type == APIType::D3D)
    ss << "cbuffer PSBlock : register(b0) {\n";
  else
    ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";

  // Builtin uniforms
  ss << "  float4 resolution;\n";
  ss << "  float4 src_rect;\n";
  ss << "  uint time;\n";
  ss << "  int layer;\n";
  for (u32 i = 0; i < 2; i++)
    ss << "  uint ubo_align_" << unused_counter++ << "_;\n";
  ss << "\n";

  // Custom options/uniforms
  for (const auto& it : m_config.GetOptions())
  {
    if (it.second.m_type ==
        PostProcessingConfiguration::ConfigurationOption::OptionType::OPTION_BOOL)
    {
      ss << StringFromFormat("  int %s;\n", it.first.c_str());
      for (u32 i = 0; i < 3; i++)
        ss << "  int ubo_align_" << unused_counter++ << "_;\n";
    }
    else if (it.second.m_type ==
             PostProcessingConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER)
    {
      u32 count = static_cast<u32>(it.second.m_integer_values.size());
      if (count == 1)
        ss << StringFromFormat("  int %s;\n", it.first.c_str());
      else
        ss << StringFromFormat("  int%u %s;\n", count, it.first.c_str());

      for (u32 i = count; i < 4; i++)
        ss << "  int ubo_align_" << unused_counter++ << "_;\n";
    }
    else if (it.second.m_type ==
             PostProcessingConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT)
    {
      u32 count = static_cast<u32>(it.second.m_float_values.size());
      if (count == 1)
        ss << StringFromFormat("  float %s;\n", it.first.c_str());
      else
        ss << StringFromFormat("  float%u %s;\n", count, it.first.c_str());

      for (u32 i = count; i < 4; i++)
        ss << "  float ubo_align_" << unused_counter++ << "_;\n";
    }
  }

  ss << "};\n\n";
  return ss.str();
}

std::string PostProcessing::GetHeader() const
{
  std::stringstream ss;
  ss << GetUniformBufferHeader();
  if (g_ActiveConfig.backend_info.api_type == APIType::D3D)
  {
    ss << "Texture2DArray samp0 : register(t0);\n";
    ss << "SamplerState samp0_ss : register(s0);\n";
  }
  else
  {
    ss << "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n";
    ss << "VARYING_LOCATION(0) in float3 v_tex0;\n";
    ss << "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n";
  }

  // Rename main, since we need to set up globals
  if (g_ActiveConfig.backend_info.api_type == APIType::D3D)
  {
    ss << R"(
#define main real_main
static float3 v_tex0;
static float4 ocol0;

// Wrappers for sampling functions.
#define texture(sampler, coords) sampler.Sample(sampler##_ss, coords)
#define textureOffset(sampler, coords, offset) sampler.Sample(sampler##_ss, coords, offset)
)";
  }

  ss << R"(
float4 Sample() { return texture(samp0, float3(v_tex0.xy, float(layer))); }
float4 SampleLocation(float2 location) { return texture(samp0, float3(location, float(layer))); }
float4 SampleLayer(int layer) { return texture(samp0, float3(v_tex0.xy, float(layer))); }
#define SampleOffset(offset) textureOffset(samp0, float3(v_tex0.xy, float(layer)), offset)

float2 GetResolution()
{
  return resolution.xy;
}

float2 GetInvResolution()
{
  return resolution.zw;
}

float2 GetCoordinates()
{
  return v_tex0.xy;
}

uint GetTime()
{
  return time;
}

void SetOutput(float4 color)
{
  ocol0 = color;
}

#define GetOption(x) (x)
#define OptionEnabled(x) ((x) != 0)

)";
  return ss.str();
}

std::string PostProcessing::GetFooter() const
{
  if (g_ActiveConfig.backend_info.api_type == APIType::D3D)
  {
    return R"(

#undef main
void main(in float3 v_tex0_ : TEXCOORD0, out float4 ocol0_ : SV_Target)
{
  v_tex0 = v_tex0_;
  real_main();
  ocol0_ = ocol0;
})";
  }
  else
  {
    return {};
  }
}

bool PostProcessing::CompileVertexShader()
{
  std::stringstream ss;
  ss << GetUniformBufferHeader();

  if (g_ActiveConfig.backend_info.api_type == APIType::D3D)
  {
    ss << "void main(in uint id : SV_VertexID, out float3 v_tex0 : TEXCOORD0,\n";
    ss << "          out float4 opos : SV_Position) {\n";
  }
  else
  {
    ss << "VARYING_LOCATION(0) out float3 v_tex0;\n";
    ss << "#define id gl_VertexID\n";
    ss << "#define opos gl_Position\n";
    ss << "void main() {\n";
  }

  ss << "  float2 rawpos = float2((id << 1) & 2, id & 2);\n";
  if (g_ActiveConfig.backend_info.api_type == APIType::Vulkan)
  {
    // NDC space is flipped in Vulkan
    ss << "  opos = float4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);\n";
  }
  else
  {
    ss << "  opos = float4(rawpos * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n";
  }
  ss << "  v_tex0 = float3(src_rect.xy + (src_rect.zw * rawpos), 0.0f);\n";
  ss << "}\n";

  m_vertex_shader = g_renderer->CreateShaderFromSource(ShaderStage::Vertex, ss.str());
  if (!m_vertex_shader)
  {
    PanicAlert("Failed to compile post-processing vertex shader");
    return false;
  }

  return true;
}

struct BuiltinUniforms
{
  float resolution[4];
  float src_rect[4];
  s32 time;
  u32 layer;
  u32 padding[2];
};

size_t PostProcessing::CalculateUniformsSize() const
{
  // Allocate a vec4 for each uniform to simplify allocation.
  return sizeof(BuiltinUniforms) + m_config.GetOptions().size() * sizeof(float) * 4;
}

void PostProcessing::FillUniformBuffer(const MathUtil::Rectangle<int>& src,
                                       const AbstractTexture* src_tex, int src_layer)
{
  const float src_width = static_cast<float>(src_tex->GetWidth());
  const float src_height = static_cast<float>(src_tex->GetHeight());
  BuiltinUniforms builtin_uniforms = {
      {src_width, src_height, 1.0f / src_width, 1.0f / src_height},
      {src.left / src_width, src.top / src_height, src.GetWidth() / src_width,
       src.GetHeight() / src_height},
      static_cast<s32>(m_timer.GetTimeElapsed()),
      static_cast<u32>(src_layer),
  };

  u8* buf = m_uniform_staging_buffer.data();
  std::memcpy(buf, &builtin_uniforms, sizeof(builtin_uniforms));
  buf += sizeof(builtin_uniforms);

  for (const auto& it : m_config.GetOptions())
  {
    union
    {
      u32 as_bool[4];
      s32 as_int[4];
      float as_float[4];
    } value = {};

    switch (it.second.m_type)
    {
    case PostProcessingConfiguration::ConfigurationOption::OptionType::OPTION_BOOL:
      value.as_bool[0] = it.second.m_bool_value ? 1 : 0;
      break;

    case PostProcessingConfiguration::ConfigurationOption::OptionType::OPTION_INTEGER:
      ASSERT(it.second.m_integer_values.size() < 4);
      std::copy_n(it.second.m_integer_values.begin(), it.second.m_integer_values.size(),
                  value.as_int);
      break;

    case PostProcessingConfiguration::ConfigurationOption::OptionType::OPTION_FLOAT:
      ASSERT(it.second.m_float_values.size() < 4);
      std::copy_n(it.second.m_float_values.begin(), it.second.m_float_values.size(),
                  value.as_float);
      break;
    }

    std::memcpy(buf, &value, sizeof(value));
    buf += sizeof(value);
  }
}

bool PostProcessing::CompilePixelShader()
{
  m_pipeline.reset();
  m_pixel_shader.reset();

  // Generate GLSL and compile the new shader.
  m_config.LoadShader(g_ActiveConfig.sPostProcessingShader);
  m_pixel_shader = g_renderer->CreateShaderFromSource(
      ShaderStage::Pixel, GetHeader() + m_config.GetShaderCode() + GetFooter());
  if (!m_pixel_shader)
  {
    PanicAlert("Failed to compile post-processing shader %s", m_config.GetShader().c_str());

    // Use default shader.
    m_config.LoadDefaultShader();
    m_pixel_shader = g_renderer->CreateShaderFromSource(
        ShaderStage::Pixel, GetHeader() + m_config.GetShaderCode() + GetFooter());
    if (!m_pixel_shader)
      return false;
  }

  m_uniform_staging_buffer.resize(CalculateUniformsSize());
  return true;
}

bool PostProcessing::CompilePipeline()
{
  AbstractPipelineConfig config = {};
  config.vertex_shader = m_vertex_shader.get();
  config.geometry_shader = nullptr;
  config.pixel_shader = m_pixel_shader.get();
  config.rasterization_state =
      RenderState::GetCullBackFaceRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(m_framebuffer_format);
  config.usage = AbstractPipelineUsage::Utility;
  m_pipeline = g_renderer->CreatePipeline(config);
  if (!m_pipeline)
    return false;

  return true;
}
}  // namespace VideoCommon
