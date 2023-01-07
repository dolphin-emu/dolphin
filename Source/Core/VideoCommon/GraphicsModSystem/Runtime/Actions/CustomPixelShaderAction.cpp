// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPixelShaderAction.h"

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/GraphicsModSystem/Runtime/CustomTextureData.h"
#include "VideoCommon/ShaderGenCommon.h"
#include "VideoCommon/TextureCacheBase.h"

namespace
{
bool IsQualifier(std::string_view value)
{
  static std::array<std::string_view, 7> qualifiers = {"attribute", "const",   "highp",  "lowp",
                                                       "mediump",   "uniform", "varying"};
  return std::find(qualifiers.begin(), qualifiers.end(), value) != qualifiers.end();
}

bool IsBuiltInMacro(std::string_view value)
{
  static std::array<std::string_view, 5> built_in = {"__LINE__", "__FILE__", "__VERSION__",
                                                     "GL_core_profile", "GL_compatibility_profile"};
  return std::find(built_in.begin(), built_in.end(), value) != built_in.end();
}

std::vector<std::string> GlobalConflicts(std::string_view source)
{
  std::string_view last_identifier = "";
  std::vector<std::string> global_result;
  u32 scope = 0;
  for (u32 i = 0; i < source.size(); i++)
  {
    // If we're out of global scope, we don't care
    // about any of the details
    if (scope > 0)
    {
      if (source[i] == '{')
      {
        scope++;
      }
      else if (source[i] == '}')
      {
        scope--;
      }
      continue;
    }

    const auto parse_identifier = [&]() {
      const u32 start = i;
      for (; i < source.size(); i++)
      {
        if (!IsAlpha(source[i]) && source[i] != '_' && !std::isdigit(source[i]))
          break;
      }
      u32 end = i;
      i--;  // unwind
      return source.substr(start, end - start);
    };

    if (IsAlpha(source[i]) || source[i] == '_')
    {
      const std::string_view identifier = parse_identifier();
      if (IsQualifier(identifier))
        continue;
      if (IsBuiltInMacro(identifier))
        continue;
      last_identifier = identifier;
    }
    else if (source[i] == '#')
    {
      const auto parse_until_end_of_preprocessor = [&]() {
        bool continue_until_next_newline = false;
        for (; i < source.size(); i++)
        {
          if (source[i] == '\n')
          {
            if (continue_until_next_newline)
              continue_until_next_newline = false;
            else
              break;
          }
          else if (source[i] == '\\')
          {
            continue_until_next_newline = true;
          }
        }
      };
      i++;
      const std::string_view identifier = parse_identifier();
      if (identifier == "define")
      {
        i++;
        // skip whitespace
        while (source[i] == ' ')
        {
          i++;
        }
        global_result.push_back(std::string{parse_identifier()});
        parse_until_end_of_preprocessor();
      }
      else
      {
        parse_until_end_of_preprocessor();
      }
    }
    else if (source[i] == '{')
    {
      scope++;
    }
    else if (source[i] == '(')
    {
      // Unlikely the user will be using layouts but...
      if (last_identifier == "layout")
        continue;

      // Since we handle equality, we can assume the identifier
      // before '(' is a function definition
      global_result.push_back(std::string{last_identifier});
    }
    else if (source[i] == '=')
    {
      global_result.push_back(std::string{last_identifier});
      i++;
      for (; i < source.size(); i++)
      {
        if (source[i] == ';')
          break;
      }
    }
    else if (source[i] == '/')
    {
      if ((i + 1) >= source.size())
        continue;

      if (source[i + 1] == '/')
      {
        // Go to end of line...
        for (; i < source.size(); i++)
        {
          if (source[i] == '\n')
            break;
        }
      }
      else if (source[i + 1] == '*')
      {
        // Multiline, look for first '*/'
        for (; i < source.size(); i++)
        {
          if (source[i] == '/' && source[i - 1] == '*')
            break;
        }
      }
    }
  }

  // Sort the conflicts from largest to smallest string
  // this way we can ensure smaller strings that are a substring
  // of the larger string are able to be replaced appropriately
  std::sort(global_result.begin(), global_result.end(),
            [](const std::string& first, const std::string& second) {
              return first.size() > second.size();
            });
  return global_result;
}

void WriteDefines(
    ShaderCode* out,
    const std::vector<CustomPixelShaderAction::TextureAllocationData>& texture_allocations,
    u32 texture_unit)
{
  for (int i = 0; i < texture_allocations.size(); i++)
  {
    const auto& texture_data = texture_allocations[i];
    out->Write("#define {}_UNIT_{{0}} {}\n", texture_data.code_name, texture_unit);
    out->Write(
        "#define {0}_COORD_{{0}} float3(data.texcoord[data.texmap_to_texcoord_index[{1}]].xy, "
        "{2})\n",
        texture_data.code_name, texture_unit, i + 1);
  }
}

bool TestShader(
    const std::string& source,
    const std::vector<CustomPixelShaderAction::TextureAllocationData>& texture_allocations)
{
  ShaderCode out;

  // Header details
  out.Write("layout(location = 0) out vec4 FragColor;\n\n");
  out.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp[8];\n");
  out.Write("int4 " I_MATERIALS "[4];\n");
  out.Write("uint time_ms;\n");
  WriteCustomShaderStructDef(&out, 0);

  ShaderCode intermediate;
  WriteDefines(&intermediate, texture_allocations, 0);
  intermediate.Write("{}", source);

  // Replace defines and source with unique identifier
  out.Write("{}", fmt::format(fmt::runtime(intermediate.GetBuffer()), 0));

  out.Write("\n\n");
  out.Write("void main()\n");
  out.Write("{{\n");
  pixel_shader_uid_data ps_uid_data = {};
  ps_uid_data.genMode_numtexgens = 1;
  // Mock out some fake data
  out.Write("\tvec3 tex0 = vec3(0, 0, 0);\n");
  // Fill out our custom struct
  WriteCustomShaderStructImpl(&out, 0, false, &ps_uid_data);
  out.Write("\tcustom_data.final_color = vec4(0, 0, 0, 0);\n");
  // Call actual user function so it isn't optimized out
  out.Write("\tFragColor = {}_0(custom_data);\n", CUSTOM_PIXELSHADER_COLOR_FUNC);
  out.Write("}}\n");
  auto shader = g_gfx->CreateShaderFromSource(ShaderStage::Pixel, out.GetBuffer(), "Temp");
  return shader != nullptr;
}
}  // namespace

std::unique_ptr<CustomPixelShaderAction>
CustomPixelShaderAction::Create(const picojson::value& json_data, std::string_view path)
{
  std::string color_shader_path;
  const auto& color_shader_path_json = json_data.get("color_shader_path");
  if (color_shader_path_json.is<std::string>())
  {
    color_shader_path = color_shader_path_json.get<std::string>();
  }

  std::string color_shader_data;
  std::string full_color_shader_path;
  if (!color_shader_path.empty())
  {
    full_color_shader_path = fmt::format("{}/{}", path, color_shader_path);

    if (!File::ReadFileToString(full_color_shader_path, color_shader_data))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load color shader path '{}' for custom shader action",
                    full_color_shader_path);
      return nullptr;
    }
    color_shader_data = ReplaceAll(color_shader_data, "custom_main", CUSTOM_PIXELSHADER_COLOR_FUNC);
    const auto global_conflicts = GlobalConflicts(color_shader_data);
    color_shader_data = ReplaceAll(color_shader_data, "\r\n", "\n");
    color_shader_data = ReplaceAll(color_shader_data, "{", "{{");
    color_shader_data = ReplaceAll(color_shader_data, "}", "}}");
    // First replace global conflicts with dummy strings
    // This avoids the problem where a shorter word
    // is in a longer word, ex two functions:  'execute' and 'execute_fast'
    for (std::size_t i = 0; i < global_conflicts.size(); i++)
    {
      const std::string& identifier = global_conflicts[i];
      color_shader_data =
          ReplaceAll(color_shader_data, identifier, fmt::format("_{0}_DOLPHIN_TEMP_{0}_", i));
    }
    // Now replace the temporaries with the actual value
    for (std::size_t i = 0; i < global_conflicts.size(); i++)
    {
      const std::string& identifier = global_conflicts[i];
      color_shader_data = ReplaceAll(color_shader_data, fmt::format("_{0}_DOLPHIN_TEMP_{0}_", i),
                                     fmt::format("{}_{{0}}", identifier));
    }
  }

  std::vector<TextureAllocationData> texture_allocations;
  const auto& inputs_json = json_data.get("inputs");
  if (inputs_json.is<picojson::array>())
  {
    for (const auto& inputs_json_val : inputs_json.get<picojson::array>())
    {
      if (!inputs_json_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load custom shader action, 'inputs' has an array value that "
                      "is not an object!");
        return nullptr;
      }

      auto input = inputs_json_val.get<picojson::object>();
      if (!input.contains("texture_path"))
      {
        ERROR_LOG_FMT(VIDEO, "Failed to load custom shader action, 'inputs' value missing required "
                             "field 'texture_path'");
        return nullptr;
      }

      auto texture_path_json = input["texture_path"];
      if (!texture_path_json.is<std::string>())
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Failed to load custom shader action, 'inputs' field 'texture_path' is not a string!");
        return nullptr;
      }
      const std::string texture_path = texture_path_json.to_str();
      const std::string full_texture_path = fmt::format("{}/{}", path, texture_path);
      TextureAllocationData data;
      SplitPath(full_texture_path, nullptr, &data.file_name, nullptr);
      data.raw_data.m_levels.emplace_back();
      if (!VideoCommon::LoadPNGTexture(&data.raw_data, full_texture_path))
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Failed to load custom shader action, 'inputs' field texture '{}' failed to be loaded",
            texture_path);
        return nullptr;
      }

      auto code_name_json = input["code_name"];
      if (!code_name_json.is<std::string>())
      {
        ERROR_LOG_FMT(
            VIDEO,
            "Failed to load custom shader action, 'inputs' field 'code_name' is not a string!");
        return nullptr;
      }
      data.code_name = code_name_json.to_str();
      Common::ToUpper(&data.code_name);

      if (!color_shader_data.empty())
      {
        color_shader_data = ReplaceAll(color_shader_data, fmt::format("{}_COORD", data.code_name),
                                       fmt::format("{}_COORD_{{0}}", data.code_name));
        color_shader_data = ReplaceAll(color_shader_data, fmt::format("{}_UNIT", data.code_name),
                                       fmt::format("{}_UNIT_{{0}}", data.code_name));
      }

      texture_allocations.push_back(std::move(data));
    }
  }

  if (!color_shader_data.empty())
  {
    if (!TestShader(color_shader_data, texture_allocations))
    {
      ERROR_LOG_FMT(VIDEO, "Failed to parse color shader with path '{}' for custom shader action",
                    full_color_shader_path);
      return nullptr;
    }
  }

  return std::make_unique<CustomPixelShaderAction>(std::move(color_shader_data),
                                                   std::move(texture_allocations));
}

CustomPixelShaderAction::CustomPixelShaderAction(
    std::string color_shader_data,

    std::vector<TextureAllocationData> texture_allocations)
    : m_color_shader_data(std::move(color_shader_data)),
      m_texture_data(std::move(texture_allocations))
{
}

CustomPixelShaderAction::~CustomPixelShaderAction() = default;

void CustomPixelShaderAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->custom_pixel_shader) [[unlikely]]
    return;

  if (!m_valid) [[unlikely]]
    return;

  m_last_generated_shader_code = ShaderCode{};
  WriteDefines(&m_last_generated_shader_code, m_texture_data, draw_started->texture_unit);

  if (!m_color_shader_data.empty())
  {
    m_last_generated_shader_code.Write("{}", m_color_shader_data);
    m_last_generated_shader_code.Write("\n\n\n");
  }
  else
  {
    m_last_generated_shader_code.Write("float4 {}_{{0}}( in CustomShaderData data )\n",
                                       CUSTOM_PIXELSHADER_COLOR_FUNC);
    m_last_generated_shader_code.Write("{{{{\n");
    m_last_generated_shader_code.Write("\treturn data.final_color;\n");
    m_last_generated_shader_code.Write("}}}}\n");
    m_last_generated_shader_code.Write("\n\n\n");
  }

  CustomPixelShader custom_pixel_shader;
  custom_pixel_shader.custom_shader = m_last_generated_shader_code.GetBuffer();
  *draw_started->custom_pixel_shader = custom_pixel_shader;
}

void CustomPixelShaderAction::OnTextureLoad(GraphicsModActionData::TextureLoad* load)
{
  for (auto& data : m_texture_data)
  {
    if (load->texture_width != data.raw_data.m_levels[0].width ||
        load->texture_height != data.raw_data.m_levels[0].height)
    {
      ERROR_LOG_FMT(VIDEO,
                    "Custom pixel shader for texture '{}' has additional inputs that do not match "
                    "the width/height of the texture loaded.  Width [{} vs {}], Height [{} vs {}]",
                    load->texture_name, load->texture_width, data.raw_data.m_levels[0].width,
                    load->texture_height, data.raw_data.m_levels[0].height);
      m_valid = false;
      return;
    }
  }

  for (auto& data : m_texture_data)
  {
    load->custom_textures->push_back(&data.raw_data);
  }
}
