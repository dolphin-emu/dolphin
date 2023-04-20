// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PostProcessing.h"

#include <sstream>
#include <string>
#include <string_view>

#include <fmt/format.h>

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
#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/FramebufferManager.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/ShaderCache.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"
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

  std::string sub_dir = "";

  if (g_Config.stereo_mode == StereoMode::Anaglyph)
  {
    sub_dir = ANAGLYPH_DIR DIR_SEP;
  }
  else if (g_Config.stereo_mode == StereoMode::Passive)
  {
    sub_dir = PASSIVE_DIR DIR_SEP;
  }

  // loading shader code
  std::string code;
  std::string path = File::GetUserPath(D_SHADERS_IDX) + sub_dir + shader + ".glsl";

  if (!File::Exists(path))
  {
    // Fallback to shared user dir
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + shader + ".glsl";
  }

  if (!File::ReadFileToString(path, code))
  {
    ERROR_LOG_FMT(VIDEO, "Post-processing shader not found: {}", path);
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
  m_any_options_dirty = false;
  m_current_shader_code = s_default_shader;
}

void PostProcessingConfiguration::LoadOptions(const std::string& code)
{
  const std::string config_start_delimiter = "[configuration]";
  const std::string config_end_delimiter = "[/configuration]";
  size_t configuration_start = code.find(config_start_delimiter);
  size_t configuration_end = code.find(config_end_delimiter);

  m_options.clear();
  m_any_options_dirty = true;

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
    std::string line_str;
    if (std::getline(in, line_str))
    {
      std::string_view line = line_str;

#ifndef _WIN32
      // Check for CRLF eol and convert it to LF
      if (!line.empty() && line.at(line.size() - 1) == '\r')
        line.remove_suffix(1);
#endif

      if (!line.empty())
      {
        if (line[0] == '[')
        {
          size_t endpos = line.find("]");

          if (endpos != std::string::npos)
          {
            // New section!
            std::string_view sub = line.substr(1, endpos - 1);
            option_strings.push_back({std::string(sub)});
            current_strings = &option_strings.back();
          }
        }
        else
        {
          if (current_strings)
          {
            std::string key, value;
            Common::IniFile::ParseLine(line, &key, &value);

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
    option.m_dirty = true;

    if (it.m_type == "OptionBool")
      option.m_type = ConfigurationOption::OptionType::Bool;
    else if (it.m_type == "OptionRangeFloat")
      option.m_type = ConfigurationOption::OptionType::Float;
    else if (it.m_type == "OptionRangeInteger")
      option.m_type = ConfigurationOption::OptionType::Integer;

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

        if (option.m_type == ConfigurationOption::OptionType::Bool)
        {
          TryParse(string_option.second, &option.m_bool_value);
        }
        else if (option.m_type == ConfigurationOption::OptionType::Integer)
        {
          TryParseVector(string_option.second, output_integer);
          if (output_integer->size() > 4)
            output_integer->erase(output_integer->begin() + 4, output_integer->end());
        }
        else if (option.m_type == ConfigurationOption::OptionType::Float)
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
  Common::IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  std::string section = m_current_shader + "-options";

  for (auto& it : m_options)
  {
    switch (it.second.m_type)
    {
    case ConfigurationOption::OptionType::Bool:
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &it.second.m_bool_value,
                                           it.second.m_bool_value);
      break;
    case ConfigurationOption::OptionType::Integer:
    {
      std::string value;
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
      if (!value.empty())
        TryParseVector(value, &it.second.m_integer_values);
    }
    break;
    case ConfigurationOption::OptionType::Float:
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
  Common::IniFile ini;
  ini.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
  std::string section = m_current_shader + "-options";

  for (auto& it : m_options)
  {
    switch (it.second.m_type)
    {
    case ConfigurationOption::OptionType::Bool:
    {
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, it.second.m_bool_value);
    }
    break;
    case ConfigurationOption::OptionType::Integer:
    {
      std::string value;
      for (size_t i = 0; i < it.second.m_integer_values.size(); ++i)
      {
        value += fmt::format("{}{}", it.second.m_integer_values[i],
                             i == (it.second.m_integer_values.size() - 1) ? "" : ", ");
      }
      ini.GetOrCreateSection(section)->Set(it.second.m_option_name, value);
    }
    break;
    case ConfigurationOption::OptionType::Float:
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
  it->second.m_dirty = true;
  m_any_options_dirty = true;
}

void PostProcessingConfiguration::SetOptioni(const std::string& option, int index, s32 value)
{
  auto it = m_options.find(option);

  it->second.m_integer_values[index] = value;
  it->second.m_dirty = true;
  m_any_options_dirty = true;
}

void PostProcessingConfiguration::SetOptionb(const std::string& option, bool value)
{
  auto it = m_options.find(option);

  it->second.m_bool_value = value;
  it->second.m_dirty = true;
  m_any_options_dirty = true;
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

std::vector<std::string> PostProcessing::GetAnaglyphShaderList()
{
  return GetShaders(ANAGLYPH_DIR DIR_SEP);
}

std::vector<std::string> PostProcessing::GetPassiveShaderList()
{
  return GetShaders(PASSIVE_DIR DIR_SEP);
}

bool PostProcessing::Initialize(AbstractTextureFormat format)
{
  m_framebuffer_format = format;
  // CompilePixelShader must be run first if configuration options are used.
  // Otherwise the UBO has a different member list between vertex and pixel
  // shaders, which is a link error.
  if (!CompilePixelShader() || !CompileVertexShader() || !CompilePipeline())
    return false;

  return true;
}

void PostProcessing::RecompileShader()
{
  m_pipeline.reset();
  m_pixel_shader.reset();
  if (!CompilePixelShader())
    return;
  if (!CompileVertexShader())
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
  if (g_gfx->GetCurrentFramebuffer()->GetColorFormat() != m_framebuffer_format)
  {
    m_framebuffer_format = g_gfx->GetCurrentFramebuffer()->GetColorFormat();
    RecompilePipeline();
  }

  if (!m_pipeline)
    return;

  FillUniformBuffer(src, src_tex, src_layer);
  g_vertex_manager->UploadUtilityUniforms(m_uniform_staging_buffer.data(),
                                          static_cast<u32>(m_uniform_staging_buffer.size()));

  g_gfx->SetViewportAndScissor(
      g_gfx->ConvertFramebufferRectangle(dst, g_gfx->GetCurrentFramebuffer()));
  g_gfx->SetPipeline(m_pipeline.get());
  g_gfx->SetTexture(0, src_tex);
  g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_gfx->Draw(0, 3);
}

std::string PostProcessing::GetUniformBufferHeader() const
{
  std::ostringstream ss;
  u32 unused_counter = 1;
  ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";

  // Builtin uniforms
  ss << "  float4 resolution;\n";
  ss << "  float4 window_resolution;\n";
  ss << "  float4 src_rect;\n";
  ss << "  int src_layer;\n";
  ss << "  uint time;\n";
  for (u32 i = 0; i < 2; i++)
    ss << "  uint ubo_align_" << unused_counter++ << "_;\n";
  ss << "\n";

  // Custom options/uniforms
  for (const auto& it : m_config.GetOptions())
  {
    if (it.second.m_type == PostProcessingConfiguration::ConfigurationOption::OptionType::Bool)
    {
      ss << fmt::format("  int {};\n", it.first);
      for (u32 i = 0; i < 3; i++)
        ss << "  int ubo_align_" << unused_counter++ << "_;\n";
    }
    else if (it.second.m_type ==
             PostProcessingConfiguration::ConfigurationOption::OptionType::Integer)
    {
      u32 count = static_cast<u32>(it.second.m_integer_values.size());
      if (count == 1)
        ss << fmt::format("  int {};\n", it.first);
      else
        ss << fmt::format("  int{} {};\n", count, it.first);

      for (u32 i = count; i < 4; i++)
        ss << "  int ubo_align_" << unused_counter++ << "_;\n";
    }
    else if (it.second.m_type ==
             PostProcessingConfiguration::ConfigurationOption::OptionType::Float)
    {
      u32 count = static_cast<u32>(it.second.m_float_values.size());
      if (count == 1)
        ss << fmt::format("  float {};\n", it.first);
      else
        ss << fmt::format("  float{} {};\n", count, it.first);

      for (u32 i = count; i < 4; i++)
        ss << "  float ubo_align_" << unused_counter++ << "_;\n";
    }
  }

  ss << "};\n\n";
  return ss.str();
}

std::string PostProcessing::GetHeader() const
{
  std::ostringstream ss;
  ss << GetUniformBufferHeader();
  ss << "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n";

  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    ss << "VARYING_LOCATION(0) in VertexData {\n";
    ss << "  float3 v_tex0;\n";
    ss << "};\n";
  }
  else
  {
    ss << "VARYING_LOCATION(0) in float3 v_tex0;\n";
  }

  ss << "FRAGMENT_OUTPUT_LOCATION(0) out float4 ocol0;\n";

  ss << R"(
float4 Sample() { return texture(samp0, v_tex0); }
float4 SampleLocation(float2 location) { return texture(samp0, float3(location, float(v_tex0.z))); }
float4 SampleLayer(int layer) { return texture(samp0, float3(v_tex0.xy, float(layer))); }
#define SampleOffset(offset) textureOffset(samp0, v_tex0, offset)

float2 GetWindowResolution()
{
  return window_resolution.xy;
}

float2 GetInvWindowResolution()
{
  return window_resolution.zw;
}

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

float GetLayer()
{
  return v_tex0.z;
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
  return {};
}

bool PostProcessing::CompileVertexShader()
{
  std::ostringstream ss;
  ss << GetUniformBufferHeader();

  if (g_ActiveConfig.backend_info.bSupportsGeometryShaders)
  {
    ss << "VARYING_LOCATION(0) out VertexData {\n";
    ss << "  float3 v_tex0;\n";
    ss << "};\n";
  }
  else
  {
    ss << "VARYING_LOCATION(0) out float3 v_tex0;\n";
  }

  ss << "#define id gl_VertexID\n";
  ss << "#define opos gl_Position\n";
  ss << "void main() {\n";
  ss << "  v_tex0 = float3(float((id << 1) & 2), float(id & 2), 0.0f);\n";
  ss << "  opos = float4(v_tex0.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n";
  ss << "  v_tex0 = float3(src_rect.xy + (src_rect.zw * v_tex0.xy), float(src_layer));\n";

  if (g_ActiveConfig.backend_info.api_type == APIType::Vulkan)
    ss << "  opos.y = -opos.y;\n";

  ss << "}\n";

  m_vertex_shader =
      g_gfx->CreateShaderFromSource(ShaderStage::Vertex, ss.str(), "Post-processing vertex shader");
  if (!m_vertex_shader)
  {
    PanicAlertFmt("Failed to compile post-processing vertex shader");
    return false;
  }

  return true;
}

struct BuiltinUniforms
{
  float resolution[4];
  float window_resolution[4];
  float src_rect[4];
  s32 src_layer;
  u32 time;
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
  const auto& window_rect = g_presenter->GetTargetRectangle();
  const float rcp_src_width = 1.0f / src_tex->GetWidth();
  const float rcp_src_height = 1.0f / src_tex->GetHeight();
  BuiltinUniforms builtin_uniforms = {
      {static_cast<float>(src_tex->GetWidth()), static_cast<float>(src_tex->GetHeight()),
       rcp_src_width, rcp_src_height},
      {static_cast<float>(window_rect.GetWidth()), static_cast<float>(window_rect.GetHeight()),
       1.0f / static_cast<float>(window_rect.GetWidth()),
       1.0f / static_cast<float>(window_rect.GetHeight())},
      {static_cast<float>(src.left) * rcp_src_width, static_cast<float>(src.top) * rcp_src_height,
       static_cast<float>(src.GetWidth()) * rcp_src_width,
       static_cast<float>(src.GetHeight()) * rcp_src_height},
      static_cast<s32>(src_layer),
      static_cast<u32>(m_timer.ElapsedMs()),
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
    case PostProcessingConfiguration::ConfigurationOption::OptionType::Bool:
      value.as_bool[0] = it.second.m_bool_value ? 1 : 0;
      break;

    case PostProcessingConfiguration::ConfigurationOption::OptionType::Integer:
      ASSERT(it.second.m_integer_values.size() < 4);
      std::copy_n(it.second.m_integer_values.begin(), it.second.m_integer_values.size(),
                  value.as_int);
      break;

    case PostProcessingConfiguration::ConfigurationOption::OptionType::Float:
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
  m_pixel_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, GetHeader() + m_config.GetShaderCode() + GetFooter(),
      fmt::format("Post-processing pixel shader: {}", m_config.GetShader()));
  if (!m_pixel_shader)
  {
    PanicAlertFmt("Failed to compile post-processing shader {}", m_config.GetShader());

    // Use default shader.
    m_config.LoadDefaultShader();
    m_pixel_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Pixel, GetHeader() + m_config.GetShaderCode() + GetFooter(),
        "Default post-processing pixel shader");
    if (!m_pixel_shader)
      return false;
  }

  m_uniform_staging_buffer.resize(CalculateUniformsSize());
  return true;
}

bool PostProcessing::CompilePipeline()
{
  if (m_framebuffer_format == AbstractTextureFormat::Undefined)
    return true;  // Not needed (some backends don't like making pipelines with no targets)

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_vertex_shader.get();
  config.geometry_shader =
      g_gfx->UseGeometryShaderForUI() ? g_shader_cache->GetTexcoordGeometryShader() : nullptr;
  config.pixel_shader = m_pixel_shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(m_framebuffer_format);
  config.usage = AbstractPipelineUsage::Utility;
  m_pipeline = g_gfx->CreatePipeline(config);
  if (!m_pipeline)
    return false;

  return true;
}
}  // namespace VideoCommon
