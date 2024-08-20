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
static const char s_empty_pixel_shader[] = "void main() { SetOutput(Sample()); }\n";
static const char s_default_pixel_shader_name[] = "default_pre_post_process";
// Keep the highest quality possible to avoid losing quality on subtle gamma conversions.
// RGBA16F should have enough quality even if we store colors in gamma space on it.
static const AbstractTextureFormat s_intermediary_buffer_format = AbstractTextureFormat::RGBA16F;

static bool LoadShaderFromFile(const std::string& shader, const std::string& sub_dir,
                               std::string& out_code)
{
  std::string path = File::GetUserPath(D_SHADERS_IDX) + sub_dir + shader + ".glsl";

  if (!File::Exists(path))
  {
    // Fallback to shared user dir
    path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + sub_dir + shader + ".glsl";
  }

  if (!File::ReadFileToString(path, out_code))
  {
    out_code = "";
    ERROR_LOG_FMT(VIDEO, "Post-processing shader not found: {}", path);
    return false;
  }

  return true;
}

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

  std::string code;
  if (!LoadShaderFromFile(shader, sub_dir, code))
  {
    LoadDefaultShader();
    return;
  }

  LoadOptions(code);
  // Note that this will build the shaders with the custom options values users
  // might have set in the settings
  LoadOptionsConfiguration();
  m_current_shader_code = code;
}

void PostProcessingConfiguration::LoadDefaultShader()
{
  m_options.clear();
  m_any_options_dirty = false;
  m_current_shader = "";
  m_current_shader_code = s_empty_pixel_shader;
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

  // We already expect all the options to be marked as "dirty" when we reach here
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
      {
        auto integer_values = it.second.m_integer_values;
        if (TryParseVector(value, &integer_values))
        {
          it.second.m_integer_values = integer_values;
        }
      }
    }
    break;
    case ConfigurationOption::OptionType::Float:
    {
      std::string value;
      ini.GetOrCreateSection(section)->Get(it.second.m_option_name, &value);
      if (!value.empty())
      {
        auto float_values = it.second.m_float_values;
        if (TryParseVector(value, &float_values))
        {
          it.second.m_float_values = float_values;
        }
      }
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
    if (name == s_default_pixel_shader_name)
      continue;
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
  // CompilePixelShader() must be run first if configuration options are used.
  // Otherwise the UBO has a different member list between vertex and pixel
  // shaders, which is a link error on some backends.
  if (!CompilePixelShader() || !CompileVertexShader() || !CompilePipeline())
    return false;

  return true;
}

void PostProcessing::RecompileShader()
{
  // Note: for simplicity we already recompile all the shaders
  // and pipelines even if there might not be need to.

  m_default_pipeline.reset();
  m_pipeline.reset();
  m_default_pixel_shader.reset();
  m_pixel_shader.reset();
  m_default_vertex_shader.reset();
  m_vertex_shader.reset();
  if (!CompilePixelShader())
    return;
  if (!CompileVertexShader())
    return;

  CompilePipeline();
}

void PostProcessing::RecompilePipeline()
{
  m_default_pipeline.reset();
  m_pipeline.reset();
  CompilePipeline();
}

bool PostProcessing::IsColorCorrectionActive() const
{
  // We can skip the color correction pass if none of these settings are on
  // (it might have still helped with gamma correct sampling, but it's not worth running it).
  return g_ActiveConfig.color_correction.bCorrectColorSpace ||
         g_ActiveConfig.color_correction.bCorrectGamma ||
         m_framebuffer_format == AbstractTextureFormat::RGBA16F;
}

bool PostProcessing::NeedsIntermediaryBuffer() const
{
  // If we have no user selected post process shader,
  // there's no point in having an intermediary buffer doing nothing.
  return !m_config.GetShader().empty();
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

  // By default all source layers will be copied into the respective target layers
  const bool copy_all_layers = src_layer < 0;
  src_layer = std::max(src_layer, 0);

  MathUtil::Rectangle<int> src_rect = src;
  g_gfx->SetSamplerState(0, RenderState::GetLinearSamplerState());
  g_gfx->SetSamplerState(1, RenderState::GetPointSamplerState());
  g_gfx->SetTexture(0, src_tex);
  g_gfx->SetTexture(1, src_tex);

  const bool needs_color_correction = IsColorCorrectionActive();
  // Rely on the default (bi)linear sampler with the default mode
  // (it might not be gamma corrected).
  const bool needs_resampling =
      g_ActiveConfig.output_resampling_mode > OutputResamplingMode::Default;
  const bool needs_intermediary_buffer = NeedsIntermediaryBuffer();
  const bool needs_default_pipeline = needs_color_correction || needs_resampling;
  const AbstractPipeline* final_pipeline = m_pipeline.get();
  std::vector<u8>* uniform_staging_buffer = &m_default_uniform_staging_buffer;
  bool default_uniform_staging_buffer = true;
  const MathUtil::Rectangle<int> present_rect = g_presenter->GetTargetRectangle();

  // Intermediary pass.
  // We draw to a high quality intermediary texture for a couple reasons:
  // -Consistently do high quality gamma corrected resampling (upscaling/downscaling)
  // -Keep quality for gamma and gamut conversions, and HDR output
  //  (low bit depths lose too much quality with gamma conversions)
  // -Keep the post process phase in linear space, to better operate with colors
  if (m_default_pipeline && needs_default_pipeline && needs_intermediary_buffer)
  {
    AbstractFramebuffer* const previous_framebuffer = g_gfx->GetCurrentFramebuffer();

    // We keep the min number of layers as the render target,
    // as in case of OpenGL, the source FBX will have two layers,
    // but we will render onto two separate frame buffers (one by one),
    // so it would be a waste to allocate two layers (see "bUsesExplictQuadBuffering").
    const u32 target_layers = copy_all_layers ? src_tex->GetLayers() : 1;

    const u32 target_width =
        needs_resampling ? present_rect.GetWidth() : static_cast<u32>(src_rect.GetWidth());
    const u32 target_height =
        needs_resampling ? present_rect.GetHeight() : static_cast<u32>(src_rect.GetHeight());

    if (!m_intermediary_frame_buffer || !m_intermediary_color_texture ||
        m_intermediary_color_texture->GetWidth() != target_width ||
        m_intermediary_color_texture->GetHeight() != target_height ||
        m_intermediary_color_texture->GetLayers() != target_layers)
    {
      const TextureConfig intermediary_color_texture_config(
          target_width, target_height, 1, target_layers, src_tex->GetSamples(),
          s_intermediary_buffer_format, AbstractTextureFlag_RenderTarget,
          AbstractTextureType::Texture_2DArray);
      m_intermediary_color_texture = g_gfx->CreateTexture(intermediary_color_texture_config,
                                                          "Intermediary post process texture");

      m_intermediary_frame_buffer =
          g_gfx->CreateFramebuffer(m_intermediary_color_texture.get(), nullptr);
    }

    g_gfx->SetFramebuffer(m_intermediary_frame_buffer.get());

    FillUniformBuffer(src_rect, src_tex, src_layer, g_gfx->GetCurrentFramebuffer()->GetRect(),
                      present_rect, uniform_staging_buffer->data(), !default_uniform_staging_buffer,
                      true);
    g_vertex_manager->UploadUtilityUniforms(uniform_staging_buffer->data(),
                                            static_cast<u32>(uniform_staging_buffer->size()));

    g_gfx->SetViewportAndScissor(g_gfx->ConvertFramebufferRectangle(
        m_intermediary_color_texture->GetRect(), m_intermediary_frame_buffer.get()));
    g_gfx->SetPipeline(m_default_pipeline.get());
    g_gfx->Draw(0, 3);

    g_gfx->SetFramebuffer(previous_framebuffer);
    src_rect = m_intermediary_color_texture->GetRect();
    src_tex = m_intermediary_color_texture.get();
    g_gfx->SetTexture(0, src_tex);
    g_gfx->SetTexture(1, src_tex);
    // The "m_intermediary_color_texture" has already copied
    // from the specified source layer onto its first one.
    // If we query for a layer that the source texture doesn't have,
    // it will fall back on the first one anyway.
    src_layer = 0;
    uniform_staging_buffer = &m_uniform_staging_buffer;
    default_uniform_staging_buffer = false;
  }
  else
  {
    // If we have no custom user shader selected, and color correction
    // is active, directly run the fixed pipeline shader instead of
    // doing two passes, with the second one doing nothing useful.
    if (m_default_pipeline && needs_default_pipeline)
    {
      final_pipeline = m_default_pipeline.get();
    }
    else
    {
      uniform_staging_buffer = &m_uniform_staging_buffer;
      default_uniform_staging_buffer = false;
    }

    m_intermediary_frame_buffer.reset();
    m_intermediary_color_texture.reset();
  }

  // TODO: ideally we'd do the user selected post process pass in the intermediary buffer in linear
  // space (instead of gamma space), so the shaders could act more accurately (and sample in linear
  // space), though that would break the look of some of current post processes we have, and thus is
  // better avoided for now.

  // Final pass, either a user selected shader or the default (fixed) shader.
  if (final_pipeline)
  {
    FillUniformBuffer(src_rect, src_tex, src_layer, g_gfx->GetCurrentFramebuffer()->GetRect(),
                      present_rect, uniform_staging_buffer->data(), !default_uniform_staging_buffer,
                      false);
    g_vertex_manager->UploadUtilityUniforms(uniform_staging_buffer->data(),
                                            static_cast<u32>(uniform_staging_buffer->size()));

    g_gfx->SetViewportAndScissor(
        g_gfx->ConvertFramebufferRectangle(dst, g_gfx->GetCurrentFramebuffer()));
    g_gfx->SetPipeline(final_pipeline);
    g_gfx->Draw(0, 3);
  }
}

std::string PostProcessing::GetUniformBufferHeader(bool user_post_process) const
{
  std::ostringstream ss;
  ss << "UBO_BINDING(std140, 1) uniform PSBlock {\n";

  // Builtin uniforms:

  ss << "  float4 resolution;\n";  // Source resolution
  ss << "  float4 target_resolution;\n";
  ss << "  float4 window_resolution;\n";
  // How many horizontal and vertical stereo views do we have? (set to 1 when we use layers instead)
  ss << "  int2 stereo_views;\n";
  ss << "  float4 src_rect;\n";
  // The first (but not necessarily only) source layer we target
  ss << "  int src_layer;\n";
  ss << "  uint time;\n";
  ss << "  int graphics_api;\n";
  // If true, it's an intermediary buffer (including the first), if false, it's the final one
  ss << "  int intermediary_buffer;\n";

  ss << "  int resampling_method;\n";
  ss << "  int correct_color_space;\n";
  ss << "  int game_color_space;\n";
  ss << "  int correct_gamma;\n";
  ss << "  float game_gamma;\n";
  ss << "  int sdr_display_gamma_sRGB;\n";
  ss << "  float sdr_display_custom_gamma;\n";
  ss << "  int linear_space_output;\n";
  ss << "  int hdr_output;\n";
  ss << "  float hdr_paper_white_nits;\n";
  ss << "  float hdr_sdr_white_nits;\n";

  if (user_post_process)
  {
    u32 unused_counter = 1;
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
  }

  ss << "};\n\n";
  return ss.str();
}

std::string PostProcessing::GetHeader(bool user_post_process) const
{
  std::ostringstream ss;
  ss << GetUniformBufferHeader(user_post_process);
  ss << "SAMPLER_BINDING(0) uniform sampler2DArray samp0;\n";
  ss << "SAMPLER_BINDING(1) uniform sampler2DArray samp1;\n";

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

float2 GetTargetResolution()
{
  return target_resolution.xy;
}

float2 GetInvTargetResolution()
{
  return target_resolution.zw;
}

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
#define OptionDisabled(x) ((x) == 0)

)";
  return ss.str();
}

std::string PostProcessing::GetFooter() const
{
  return {};
}

static std::string GetVertexShaderBody()
{
  std::ostringstream ss;
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

  // Vulkan Y needs to be inverted on every pass
  if (g_ActiveConfig.backend_info.api_type == APIType::Vulkan)
  {
    ss << "  opos.y = -opos.y;\n";
  }
  // OpenGL Y needs to be inverted in all passes except the last one
  else if (g_ActiveConfig.backend_info.api_type == APIType::OpenGL)
  {
    ss << "  if (intermediary_buffer != 0)\n";
    ss << "    opos.y = -opos.y;\n";
  }

  ss << "}\n";
  return ss.str();
}

bool PostProcessing::CompileVertexShader()
{
  std::ostringstream ss_default;
  ss_default << GetUniformBufferHeader(false);
  ss_default << GetVertexShaderBody();
  m_default_vertex_shader = g_gfx->CreateShaderFromSource(ShaderStage::Vertex, ss_default.str(),
                                                          "Default post-processing vertex shader");

  std::ostringstream ss;
  ss << GetUniformBufferHeader(true);
  ss << GetVertexShaderBody();
  m_vertex_shader =
      g_gfx->CreateShaderFromSource(ShaderStage::Vertex, ss.str(), "Post-processing vertex shader");

  if (!m_default_vertex_shader || !m_vertex_shader)
  {
    PanicAlertFmt("Failed to compile post-processing vertex shader");
    m_default_vertex_shader.reset();
    m_vertex_shader.reset();
    return false;
  }

  return true;
}

struct BuiltinUniforms
{
  // bools need to be represented as "s32"

  std::array<float, 4> source_resolution;
  std::array<float, 4> target_resolution;
  std::array<float, 4> window_resolution;
  std::array<float, 4> stereo_views;
  std::array<float, 4> src_rect;
  s32 src_layer;
  u32 time;
  s32 graphics_api;
  s32 intermediary_buffer;
  s32 resampling_method;
  s32 correct_color_space;
  s32 game_color_space;
  s32 correct_gamma;
  float game_gamma;
  s32 sdr_display_gamma_sRGB;
  float sdr_display_custom_gamma;
  s32 linear_space_output;
  s32 hdr_output;
  float hdr_paper_white_nits;
  float hdr_sdr_white_nits;
};

size_t PostProcessing::CalculateUniformsSize(bool user_post_process) const
{
  // Allocate a vec4 for each uniform to simplify allocation.
  return sizeof(BuiltinUniforms) +
         (user_post_process ? m_config.GetOptions().size() : 0) * sizeof(float) * 4;
}

void PostProcessing::FillUniformBuffer(const MathUtil::Rectangle<int>& src,
                                       const AbstractTexture* src_tex, int src_layer,
                                       const MathUtil::Rectangle<int>& dst,
                                       const MathUtil::Rectangle<int>& wnd, u8* buffer,
                                       bool user_post_process, bool intermediary_buffer)
{
  const float rcp_src_width = 1.0f / src_tex->GetWidth();
  const float rcp_src_height = 1.0f / src_tex->GetHeight();

  BuiltinUniforms builtin_uniforms;
  builtin_uniforms.source_resolution = {static_cast<float>(src_tex->GetWidth()),
                                        static_cast<float>(src_tex->GetHeight()), rcp_src_width,
                                        rcp_src_height};
  builtin_uniforms.target_resolution = {
      static_cast<float>(dst.GetWidth()), static_cast<float>(dst.GetHeight()),
      1.0f / static_cast<float>(dst.GetWidth()), 1.0f / static_cast<float>(dst.GetHeight())};
  builtin_uniforms.window_resolution = {
      static_cast<float>(wnd.GetWidth()), static_cast<float>(wnd.GetHeight()),
      1.0f / static_cast<float>(wnd.GetWidth()), 1.0f / static_cast<float>(wnd.GetHeight())};
  builtin_uniforms.src_rect = {static_cast<float>(src.left) * rcp_src_width,
                               static_cast<float>(src.top) * rcp_src_height,
                               static_cast<float>(src.GetWidth()) * rcp_src_width,
                               static_cast<float>(src.GetHeight()) * rcp_src_height};
  builtin_uniforms.src_layer = static_cast<s32>(src_layer);
  builtin_uniforms.time = static_cast<u32>(m_timer.ElapsedMs());
  builtin_uniforms.graphics_api = static_cast<s32>(g_ActiveConfig.backend_info.api_type);
  builtin_uniforms.intermediary_buffer = static_cast<s32>(intermediary_buffer);

  builtin_uniforms.resampling_method = static_cast<s32>(g_ActiveConfig.output_resampling_mode);
  // Color correction related uniforms.
  // These are mainly used by the "m_default_pixel_shader",
  // but should also be accessible to all other shaders.
  builtin_uniforms.correct_color_space = g_ActiveConfig.color_correction.bCorrectColorSpace;
  builtin_uniforms.game_color_space =
      static_cast<int>(g_ActiveConfig.color_correction.game_color_space);
  builtin_uniforms.correct_gamma = g_ActiveConfig.color_correction.bCorrectGamma;
  builtin_uniforms.game_gamma = g_ActiveConfig.color_correction.fGameGamma;
  builtin_uniforms.sdr_display_gamma_sRGB = g_ActiveConfig.color_correction.bSDRDisplayGammaSRGB;
  builtin_uniforms.sdr_display_custom_gamma =
      g_ActiveConfig.color_correction.fSDRDisplayCustomGamma;
  // scRGB (RGBA16F) expects linear values as opposed to sRGB gamma
  builtin_uniforms.linear_space_output = m_framebuffer_format == AbstractTextureFormat::RGBA16F;
  // Implies ouput values can be beyond the 0-1 range
  builtin_uniforms.hdr_output = m_framebuffer_format == AbstractTextureFormat::RGBA16F;
  builtin_uniforms.hdr_paper_white_nits = g_ActiveConfig.color_correction.fHDRPaperWhiteNits;
  // A value of 1 1 1 usually matches 80 nits in HDR
  builtin_uniforms.hdr_sdr_white_nits = 80.f;

  std::memcpy(buffer, &builtin_uniforms, sizeof(builtin_uniforms));
  buffer += sizeof(builtin_uniforms);

  // Don't include the custom pp shader options if they are not necessary,
  // having mismatching uniforms between different shaders can cause issues on some backends
  if (!user_post_process)
    return;

  for (auto& it : m_config.GetOptions())
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
      ASSERT(it.second.m_integer_values.size() <= 4);
      std::copy_n(it.second.m_integer_values.begin(), it.second.m_integer_values.size(),
                  value.as_int);
      break;

    case PostProcessingConfiguration::ConfigurationOption::OptionType::Float:
      ASSERT(it.second.m_float_values.size() <= 4);
      std::copy_n(it.second.m_float_values.begin(), it.second.m_float_values.size(),
                  value.as_float);
      break;
    }

    it.second.m_dirty = false;

    std::memcpy(buffer, &value, sizeof(value));
    buffer += sizeof(value);
  }

  m_config.SetDirty(false);
}

bool PostProcessing::CompilePixelShader()
{
  m_default_pixel_shader.reset();
  m_pixel_shader.reset();

  // Generate GLSL and compile the new shaders:

  std::string default_pixel_shader_code;
  if (LoadShaderFromFile(s_default_pixel_shader_name, "", default_pixel_shader_code))
  {
    m_default_pixel_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Pixel, GetHeader(false) + default_pixel_shader_code + GetFooter(),
        "Default post-processing pixel shader");
    // We continue even if all of this failed, it doesn't matter
    m_default_uniform_staging_buffer.resize(CalculateUniformsSize(false));
  }
  else
  {
    m_default_uniform_staging_buffer.resize(0);
  }

  m_config.LoadShader(g_ActiveConfig.sPostProcessingShader);
  m_pixel_shader = g_gfx->CreateShaderFromSource(
      ShaderStage::Pixel, GetHeader(true) + m_config.GetShaderCode() + GetFooter(),
      fmt::format("User post-processing pixel shader: {}", m_config.GetShader()));
  if (!m_pixel_shader)
  {
    PanicAlertFmt("Failed to compile user post-processing shader {}", m_config.GetShader());

    // Use default shader.
    m_config.LoadDefaultShader();
    m_pixel_shader = g_gfx->CreateShaderFromSource(
        ShaderStage::Pixel, GetHeader(true) + m_config.GetShaderCode() + GetFooter(),
        "Default user post-processing pixel shader");
    if (!m_pixel_shader)
    {
      m_uniform_staging_buffer.resize(0);
      return false;
    }
  }

  m_uniform_staging_buffer.resize(CalculateUniformsSize(true));
  return true;
}

static bool UseGeometryShaderForPostProcess(bool is_intermediary_buffer)
{
  // We only return true on stereo modes that need to copy
  // both source texture layers into the target texture layers.
  // Any other case is handled manually with multiple copies, thus
  // it doesn't need a geom shader.
  switch (g_ActiveConfig.stereo_mode)
  {
  case StereoMode::QuadBuffer:
    return !g_ActiveConfig.backend_info.bUsesExplictQuadBuffering;
  case StereoMode::Anaglyph:
  case StereoMode::Passive:
    return is_intermediary_buffer;
  case StereoMode::SBS:
  case StereoMode::TAB:
  case StereoMode::Off:
  default:
    return false;
  }
}

bool PostProcessing::CompilePipeline()
{
  // Not needed. Some backends don't like making pipelines with no targets,
  // and in any case, we don't need to render anything if that happened.
  if (m_framebuffer_format == AbstractTextureFormat::Undefined)
    return true;

  // If this is true, the "m_default_pipeline" won't be the only one that runs
  const bool needs_intermediary_buffer = NeedsIntermediaryBuffer();

  AbstractPipelineConfig config = {};
  config.vertex_shader = m_default_vertex_shader.get();
  // This geometry shader will take care of reading both layer 0 and 1 on the source texture,
  // and writing to both layer 0 and 1 on the render target.
  config.geometry_shader = UseGeometryShaderForPostProcess(needs_intermediary_buffer) ?
                               g_shader_cache->GetTexcoordGeometryShader() :
                               nullptr;
  config.pixel_shader = m_default_pixel_shader.get();
  config.rasterization_state = RenderState::GetNoCullRasterizationState(PrimitiveType::Triangles);
  config.depth_state = RenderState::GetNoDepthTestingDepthState();
  config.blending_state = RenderState::GetNoBlendingBlendState();
  config.framebuffer_state = RenderState::GetColorFramebufferState(
      needs_intermediary_buffer ? s_intermediary_buffer_format : m_framebuffer_format);
  config.usage = AbstractPipelineUsage::Utility;
  // We continue even if it failed, it will be skipped later on
  if (config.pixel_shader)
    m_default_pipeline = g_gfx->CreatePipeline(config);

  config.vertex_shader = m_vertex_shader.get();
  config.geometry_shader = UseGeometryShaderForPostProcess(false) ?
                               g_shader_cache->GetTexcoordGeometryShader() :
                               nullptr;
  config.pixel_shader = m_pixel_shader.get();
  config.framebuffer_state = RenderState::GetColorFramebufferState(m_framebuffer_format);
  m_pipeline = g_gfx->CreatePipeline(config);
  if (!m_pipeline)
    return false;

  return true;
}
}  // namespace VideoCommon
