// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/System.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"
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
        if (!Common::IsAlpha(source[i]) && source[i] != '_' && !std::isdigit(source[i]))
          break;
      }
      u32 end = i;
      i--;  // unwind
      return source.substr(start, end - start);
    };

    if (Common::IsAlpha(source[i]) || source[i] == '_')
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

void WriteDefines(ShaderCode* out, const std::vector<std::string>& texture_code_names,
                  u32 texture_unit)
{
  for (std::size_t i = 0; i < texture_code_names.size(); i++)
  {
    const auto& code_name = texture_code_names[i];
    out->Write("#define {}_UNIT_{{0}} {}\n", code_name, texture_unit);
    out->Write(
        "#define {0}_COORD_{{0}} float3(data.texcoord[data.texmap_to_texcoord_index[{1}]].xy, "
        "{2})\n",
        code_name, texture_unit, i + 1);
  }
}

}  // namespace

std::unique_ptr<CustomPipelineAction>
CustomPipelineAction::Create(const picojson::value& json_data,
                             std::shared_ptr<VideoCommon::CustomAssetLibrary> library)
{
  std::vector<CustomPipelineAction::PipelinePassPassDescription> pipeline_passes;

  const auto& passes_json = json_data.get("passes");
  if (passes_json.is<picojson::array>())
  {
    for (const auto& passes_json_val : passes_json.get<picojson::array>())
    {
      CustomPipelineAction::PipelinePassPassDescription pipeline_pass;
      if (!passes_json_val.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load custom pipeline action, 'passes' has an array value that "
                      "is not an object!");
        return nullptr;
      }

      auto pass = passes_json_val.get<picojson::object>();
      if (!pass.contains("pixel_material_asset"))
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load custom pipeline action, 'passes' value missing required "
                      "field 'pixel_material_asset'");
        return nullptr;
      }

      auto pixel_material_asset_json = pass["pixel_material_asset"];
      if (!pixel_material_asset_json.is<std::string>())
      {
        ERROR_LOG_FMT(VIDEO, "Failed to load custom pipeline action, 'passes' field "
                             "'pixel_material_asset' is not a string!");
        return nullptr;
      }
      pipeline_pass.m_pixel_material_asset = pixel_material_asset_json.to_str();
      pipeline_passes.push_back(std::move(pipeline_pass));
    }
  }

  if (pipeline_passes.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load custom pipeline action, must specify at least one pass");
    return nullptr;
  }

  if (pipeline_passes.size() > 1)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load custom pipeline action, multiple passes are not currently supported");
    return nullptr;
  }

  return std::make_unique<CustomPipelineAction>(std::move(library), std::move(pipeline_passes));
}

CustomPipelineAction::CustomPipelineAction(
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
    std::vector<PipelinePassPassDescription> pass_descriptions)
    : m_library(std::move(library)), m_passes_config(std::move(pass_descriptions))
{
  m_passes.resize(m_passes_config.size());
}

CustomPipelineAction::~CustomPipelineAction() = default;

void CustomPipelineAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (!draw_started->custom_pixel_shader) [[unlikely]]
    return;

  if (!m_valid)
    return;

  if (m_passes.empty()) [[unlikely]]
    return;

  // For now assume a single pass
  auto& pass = m_passes[0];

  if (!pass.m_pixel_shader.m_asset) [[unlikely]]
    return;

  const auto shader_data = pass.m_pixel_shader.m_asset->GetData();
  if (shader_data)
  {
    if (m_last_generated_shader_code.GetBuffer().empty())
    {
      // Calculate shader details
      std::string color_shader_data =
          ReplaceAll(shader_data->m_shader_source, "custom_main", CUSTOM_PIXELSHADER_COLOR_FUNC);
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

      for (const auto& texture_code_name : m_texture_code_names)
      {
        color_shader_data =
            ReplaceAll(color_shader_data, fmt::format("{}_COORD", texture_code_name),
                       fmt::format("{}_COORD_{{0}}", texture_code_name));
        color_shader_data = ReplaceAll(color_shader_data, fmt::format("{}_UNIT", texture_code_name),
                                       fmt::format("{}_UNIT_{{0}}", texture_code_name));
      }

      WriteDefines(&m_last_generated_shader_code, m_texture_code_names, draw_started->texture_unit);
      m_last_generated_shader_code.Write("{}", color_shader_data);
    }
    CustomPixelShader custom_pixel_shader;
    custom_pixel_shader.custom_shader = m_last_generated_shader_code.GetBuffer();
    *draw_started->custom_pixel_shader = custom_pixel_shader;
  }
}

void CustomPipelineAction::OnTextureCreate(GraphicsModActionData::TextureCreate* create)
{
  if (!create->custom_textures) [[unlikely]]
    return;

  if (!create->additional_dependencies) [[unlikely]]
    return;

  if (m_passes_config.empty()) [[unlikely]]
    return;

  if (m_passes.empty()) [[unlikely]]
    return;

  m_valid = true;
  auto& loader = Core::System::GetInstance().GetCustomAssetLoader();

  // For now assume a single pass
  const auto& pass_config = m_passes_config[0];
  auto& pass = m_passes[0];

  if (!pass.m_pixel_material.m_asset)
  {
    pass.m_pixel_material.m_asset =
        loader.LoadMaterial(pass_config.m_pixel_material_asset, m_library);
    pass.m_pixel_material.m_cached_write_time = pass.m_pixel_material.m_asset->GetLastLoadedTime();
  }
  create->additional_dependencies->push_back(VideoCommon::CachedAsset<VideoCommon::CustomAsset>{
      pass.m_pixel_material.m_asset, pass.m_pixel_material.m_asset->GetLastLoadedTime()});

  const auto material_data = pass.m_pixel_material.m_asset->GetData();
  if (!material_data)
    return;

  if (!pass.m_pixel_shader.m_asset || pass.m_pixel_material.m_asset->GetLastLoadedTime() >
                                          pass.m_pixel_material.m_cached_write_time)
  {
    m_last_generated_shader_code = ShaderCode{};
    pass.m_pixel_shader.m_asset = loader.LoadPixelShader(material_data->shader_asset, m_library);
    pass.m_pixel_shader.m_cached_write_time = pass.m_pixel_shader.m_asset->GetLastLoadedTime();
  }
  create->additional_dependencies->push_back(VideoCommon::CachedAsset<VideoCommon::CustomAsset>{
      pass.m_pixel_shader.m_asset, pass.m_pixel_shader.m_asset->GetLastLoadedTime()});

  const auto shader_data = pass.m_pixel_shader.m_asset->GetData();
  if (!shader_data)
  {
    m_valid = false;
    return;
  }

  m_texture_code_names.clear();
  std::vector<VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>> game_assets;
  for (const auto& property : material_data->properties)
  {
    const auto shader_it = shader_data->m_properties.find(property.m_code_name);
    if (shader_it == shader_data->m_properties.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Custom pipeline for texture '{}' has material asset '{}' that uses a "
                    "code name of '{}' but that can't be found on shader asset '{}'!",
                    create->texture_name, pass.m_pixel_material.m_asset->GetAssetId(),
                    property.m_code_name, pass.m_pixel_shader.m_asset->GetAssetId());
      m_valid = false;
      return;
    }

    if (auto* value = std::get_if<std::string>(&property.m_value))
    {
      auto asset = loader.LoadGameTexture(*value, m_library);
      if (asset)
      {
        const auto loaded_time = asset->GetLastLoadedTime();
        game_assets.push_back(
            VideoCommon::CachedAsset<VideoCommon::GameTextureAsset>{std::move(asset), loaded_time});
        m_texture_code_names.push_back(property.m_code_name);
      }
    }
  }
  // Note: we swap here instead of doing a clear + append of the member
  // variable so that any loaded assets from previous iterations
  // won't be let go
  std::swap(pass.m_game_textures, game_assets);

  for (auto& game_texture : pass.m_game_textures)
  {
    if (game_texture.m_asset)
    {
      auto data = game_texture.m_asset->GetData();
      if (data)
      {
        if (data->m_texture.m_slices.empty() || data->m_texture.m_slices[0].m_levels.empty())
        {
          ERROR_LOG_FMT(
              VIDEO,
              "Custom pipeline for texture '{}' has asset '{}' that does not have any texture data",
              create->texture_name, game_texture.m_asset->GetAssetId());
          m_valid = false;
        }
        else if (create->texture_width != data->m_texture.m_slices[0].m_levels[0].width ||
                 create->texture_height != data->m_texture.m_slices[0].m_levels[0].height)
        {
          ERROR_LOG_FMT(VIDEO,
                        "Custom pipeline for texture '{}' has asset '{}' that does not match "
                        "the width/height of the texture loaded.  Texture {}x{} vs asset {}x{}",
                        create->texture_name, game_texture.m_asset->GetAssetId(),
                        create->texture_width, create->texture_height,
                        data->m_texture.m_slices[0].m_levels[0].width,
                        data->m_texture.m_slices[0].m_levels[0].height);
          m_valid = false;
        }
      }
      else
      {
        m_valid = false;
      }
    }
  }

  // TODO: compare game textures and shader requirements

  create->custom_textures->insert(create->custom_textures->end(), pass.m_game_textures.begin(),
                                  pass.m_game_textures.end());
}
