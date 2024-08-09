// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomPipeline.h"

#include <algorithm>
#include <array>
#include <variant>

#include "Common/Logging/Log.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/Assets/CustomAssetLoader.h"
#include "VideoCommon/VideoConfig.h"

namespace
{
bool IsQualifier(std::string_view value)
{
  static constexpr std::array<std::string_view, 7> qualifiers = {
      "attribute", "const", "highp", "lowp", "mediump", "uniform", "varying",
  };
  return std::find(qualifiers.begin(), qualifiers.end(), value) != qualifiers.end();
}

bool IsBuiltInMacro(std::string_view value)
{
  static constexpr std::array<std::string_view, 5> built_in = {
      "__LINE__", "__FILE__", "__VERSION__", "GL_core_profile", "GL_compatibility_profile",
  };
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
        global_result.emplace_back(parse_identifier());
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
      global_result.emplace_back(last_identifier);
    }
    else if (source[i] == '=')
    {
      global_result.emplace_back(last_identifier);
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

}  // namespace

void CustomPipeline::UpdatePixelData(
    VideoCommon::CustomAssetLoader& loader,
    std::shared_ptr<VideoCommon::CustomAssetLibrary> library,
    std::shared_ptr<VideoCommon::CustomTextureCache> texture_cache,
    Common::SmallVector<GraphicsModSystem::TextureView, 8> textures,
    std::array<SamplerState, 8> samplers,
    const VideoCommon::CustomAssetLibrary::AssetID& material_to_load)
{
  if (!m_pixel_material.m_asset || material_to_load != m_pixel_material.m_asset->GetAssetId())
  {
    m_pixel_material.m_asset = loader.LoadMaterial(material_to_load, library);
  }
  else
  {
    loader.AssetReferenced(m_pixel_material.m_asset->GetSessionId());
  }

  const auto material_data = m_pixel_material.m_asset->GetData();
  if (!material_data)
  {
    return;
  }

  std::size_t max_material_data_size = 0;
  if (m_pixel_material.m_asset->GetLastLoadedTime() > m_pixel_material.m_cached_write_time)
  {
    m_last_generated_material_code = ShaderCode{};
    m_pixel_material.m_cached_write_time = m_pixel_material.m_asset->GetLastLoadedTime();
    std::size_t texture_count = 0;
    for (const auto& property : material_data->properties)
    {
      max_material_data_size += VideoCommon::MaterialProperty::GetMemorySize(property);
      VideoCommon::MaterialProperty::WriteAsShaderCode(m_last_generated_material_code, property);
      if (std::holds_alternative<VideoCommon::TextureSamplerValue>(property.m_value))
      {
        texture_count++;
      }
    }
    m_material_data.resize(max_material_data_size);
    m_game_textures.resize(texture_count);
  }

  if (!m_pixel_shader.m_asset ||
      m_pixel_shader.m_asset->GetLastLoadedTime() > m_pixel_shader.m_cached_write_time ||
      material_data->shader_asset != m_pixel_shader.m_asset->GetAssetId())
  {
    m_pixel_shader.m_asset = loader.LoadPixelShader(material_data->shader_asset, library);
    m_pixel_shader.m_cached_write_time = m_pixel_shader.m_asset->GetLastLoadedTime();

    m_last_generated_shader_code = ShaderCode{};
  }
  else
  {
    loader.AssetReferenced(m_pixel_shader.m_asset->GetSessionId());
  }

  const auto shader_data = m_pixel_shader.m_asset->GetData();
  if (!shader_data)
  {
    return;
  }

  if (shader_data->m_properties.size() != material_data->properties.size())
  {
    return;
  }

  u8* material_buffer = m_material_data.data();
  u32 sampler_index = 8;
  for (std::size_t index = 0; index < material_data->properties.size(); index++)
  {
    auto& property = material_data->properties[index];
    const auto shader_it = shader_data->m_properties.find(property.m_code_name);
    if (shader_it == shader_data->m_properties.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Custom pipeline, has material asset '{}' that uses a "
                    "code name of '{}' but that can't be found on shader asset '{}'!",
                    m_pixel_material.m_asset->GetAssetId(), property.m_code_name,
                    m_pixel_shader.m_asset->GetAssetId());
      return;
    }

    if (auto* texture_sampler_value =
            std::get_if<VideoCommon::TextureSamplerValue>(&property.m_value))
    {
      if (texture_sampler_value->asset != "")
      {
        AbstractTextureType texture_type = AbstractTextureType::Texture_2DArray;
        if (std::holds_alternative<VideoCommon::ShaderProperty::SamplerCube>(
                shader_it->second.m_default))
        {
          texture_type = AbstractTextureType::Texture_CubeMap;
        }
        else if (std::holds_alternative<VideoCommon::ShaderProperty::Sampler2D>(
                     shader_it->second.m_default))
        {
          texture_type = AbstractTextureType::Texture_2D;
        }
        auto& texture_data = m_game_textures[index];
        if (!texture_data)
        {
          texture_data = CustomPipeline::TextureData{};
          if (std::holds_alternative<VideoCommon::ShaderProperty::Sampler2D>(
                  shader_it->second.m_default))
          {
            texture_data->m_sampler_code =
                fmt::format("SAMPLER_BINDING({}) uniform sampler2D samp_{};\n", sampler_index,
                            property.m_code_name);
            texture_data->m_define_code = fmt::format("#define HAS_{} 1\n", property.m_code_name);
          }
          else if (std::holds_alternative<VideoCommon::ShaderProperty::Sampler2DArray>(
                       shader_it->second.m_default))
          {
            texture_data->m_sampler_code =
                fmt::format("SAMPLER_BINDING({}) uniform sampler2DArray samp_{};\n", sampler_index,
                            property.m_code_name);
            texture_data->m_define_code = fmt::format("#define HAS_{} 1\n", property.m_code_name);
          }
          else if (std::holds_alternative<VideoCommon::ShaderProperty::SamplerCube>(
                       shader_it->second.m_default))
          {
            texture_data->m_sampler_code =
                fmt::format("SAMPLER_BINDING({}) uniform samplerCube samp_{};\n", sampler_index,
                            property.m_code_name);
            texture_data->m_define_code = fmt::format("#define HAS_{} 1\n", property.m_code_name);
          }
        }
        const auto texture_result = texture_cache->GetTextureAsset(
            loader, library, texture_sampler_value->asset, texture_type);

        if (texture_result)
        {
          SamplerState state;
          if (texture_sampler_value->sampler_origin ==
              VideoCommon::TextureSamplerValue::SamplerOrigin::Asset)
          {
            state = texture_result->data->m_sampler;
            if (g_ActiveConfig.iMaxAnisotropy != 0 && !(state.tm0.min_filter == FilterMode::Near &&
                                                        state.tm0.mag_filter == FilterMode::Near))
            {
              state.tm0.min_filter = FilterMode::Linear;
              state.tm0.mag_filter = FilterMode::Linear;
              if (!texture_result->data->m_texture.m_slices.empty() &&
                  !texture_result->data->m_texture.m_slices[0].m_levels.empty())
              {
                state.tm0.mipmap_filter = FilterMode::Linear;
              }
              state.tm0.anisotropic_filtering = true;
            }
            else
            {
              state.tm0.anisotropic_filtering = false;
            }
          }
          else
          {
            for (const auto& texture : textures)
            {
              if (texture.hash_name == texture_sampler_value->texture_hash)
              {
                state = samplers[texture.unit];
                break;
              }
            }
          }
          g_gfx->SetTexture(sampler_index, texture_result->texture);
          g_gfx->SetSamplerState(sampler_index, state);
        }

        sampler_index++;
      }
    }
    else
    {
      VideoCommon::MaterialProperty::WriteToMemory(material_buffer, property);
    }
  }

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

    for (const auto& game_texture : m_game_textures)
    {
      if (!game_texture)
        continue;

      m_last_generated_shader_code.Write("{}", game_texture->m_sampler_code);
      m_last_generated_shader_code.Write("{}", game_texture->m_define_code);
    }

    for (std::size_t i = 0; i < textures.size(); i++)
    {
      const auto& texture = textures[i];
      m_last_generated_shader_code.Write(
          "#define TEX_COORD{} data.texcoord[data.texmap_to_texcoord_index[{}]].xy\n", i,
          texture.unit);
    }
    m_last_generated_shader_code.Write("{}", color_shader_data);
  }
}
