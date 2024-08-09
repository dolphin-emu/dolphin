// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModEditor/MaterialGeneration.h"

#include <filesystem>
#include <optional>
#include <vector>

#include <fmt/format.h>
#include <fmt/std.h>
#include <picojson.h>

#include "Common/FileSearch.h"
#include "Common/JsonUtil.h"
#include "Common/StringUtil.h"

#include "VideoCommon/Assets/TextureAsset.h"
#include "VideoCommon/Assets/TextureSamplerValue.h"
#include "VideoCommon/GraphicsModEditor/EditorState.h"
#include "VideoCommon/GraphicsModEditor/EditorTypes.h"
#include "VideoCommon/GraphicsModSystem/Runtime/Actions/CustomPipelineAction.h"

namespace GraphicsModEditor
{
namespace
{
struct TextureLookup
{
  std::optional<GraphicsModSystem::DrawCallID> draw_call_id;
  std::string output_name;
  std::vector<std::string> filenames;
};
std::vector<TextureLookup> GenerateLookups(const std::string& filename, std::string* error)
{
  std::vector<TextureLookup> result;
  picojson::value root;
  if (!JsonFromFile(filename, &root, error))
  {
    return {};
  }

  if (!root.is<picojson::array>())
  {
    *error = fmt::format("Failed to load '{}', expected root to contain an array", filename);
    return {};
  }

  const auto values = root.get<picojson::array>();
  for (const auto& value : values)
  {
    if (!value.is<picojson::object>())
    {
      *error = fmt::format("Failed to load '{}', value in array is not a json object", filename);
      return {};
    }
    TextureLookup lookup;
    const auto& obj = value.get<picojson::object>();
    if (const auto draw_call_id_json = ReadStringFromJson(obj, "draw_call_id"))
    {
      u64 draw_call_id = 0;
      if (!TryParse(*draw_call_id_json, &draw_call_id))
      {
        *error = fmt::format("Failed to load '{}', draw call id '{}' is not a number", filename,
                             *draw_call_id_json);
        return {};
      }
      lookup.draw_call_id = GraphicsModSystem::DrawCallID{draw_call_id};
    }
    const auto output_name = ReadStringFromJson(obj, "output_name");
    if (!output_name)
    {
      *error = fmt::format("Failed to load '{}', output_name not provided", filename);
      return {};
    }
    lookup.output_name = *output_name;

    const auto& texture_names_iter = obj.find("texture_names");
    if (texture_names_iter == obj.end())
    {
      *error = fmt::format("Failed to load '{}', texture_names not found", filename);
      return {};
    }

    if (!texture_names_iter->second.is<picojson::array>())
    {
      *error = fmt::format("Failed to load '{}', texture_names not an array", filename);
      return {};
    }
    const auto texture_names_json = texture_names_iter->second.get<picojson::array>();
    for (const auto& texture_name_json : texture_names_json)
    {
      if (texture_name_json.is<std::string>())
      {
        lookup.filenames.push_back(texture_name_json.to_str());
      }
    }

    result.push_back(std::move(lookup));
  }
  return result;
}
}  // namespace
void GenerateMaterials(MaterialGenerationContext* generation_context, std::string* error)
{
  const std::vector<TextureLookup> texture_lookups =
      GenerateLookups(generation_context->lookup_path, error);
  if (!error->empty())
  {
    return;
  }

  if (texture_lookups.empty())
  {
    return;
  }

  const auto output_dir = std::filesystem::path{generation_context->output_path};

  std::error_code ec;
  std::filesystem::create_directories(output_dir, ec);
  if (ec)
  {
    *error = fmt::format("Failed to create output directory '{}', error was {}", output_dir, ec);
    return;
  }

  std::map<std::string, VideoCommon::CustomAssetLibrary::AssetID> filename_to_asset_id;
  const auto files =
      Common::DoFileSearch({std::string{generation_context->input_path}}, {".png", ".dds"},
                           generation_context->search_input_recursively);
  for (const auto& filename : files)
  {
    const std::filesystem::path filepath{filename};

    generation_context->state->m_user_data.m_asset_library->AddAsset(filepath);
    const auto texture_asset =
        generation_context->state->m_user_data.m_asset_library->GetAssetFromPath(filepath);
    if (!texture_asset)
    {
      *error = fmt::format("Failed to create texture asset from path '{}'", PathToString(filepath));
      return;
    }

    std::string basename;
    SplitPath(filename, nullptr, &basename, nullptr);
    filename_to_asset_id[basename] = texture_asset->m_asset_id;
  }

  for (const auto& lookup : texture_lookups)
  {
    VideoCommon::MaterialData output_material;
    output_material = generation_context->material_template_data;

    std::size_t texture_properties_skipped = 0;
    for (const auto& [index, filter] :
         generation_context->material_property_index_to_texture_filter)
    {
      bool property_processed = false;
      auto& output_material_tex_sampler_val = output_material.properties[index].m_value;
      if (const auto prop =
              std::get_if<VideoCommon::TextureSamplerValue>(&output_material_tex_sampler_val))
      {
        std::string image_name = filter;
        for (std::size_t i = 0; i < lookup.filenames.size(); i++)
        {
          image_name =
              ReplaceAll(image_name, fmt::format("{{IMAGE_{}}}", i + 1), lookup.filenames[i]);
        }

        if (const auto asset_id_iter = filename_to_asset_id.find(image_name);
            asset_id_iter != filename_to_asset_id.end())
        {
          prop->asset = asset_id_iter->second;
          if (prop->sampler_origin == VideoCommon::TextureSamplerValue::SamplerOrigin::TextureHash)
          {
            prop->texture_hash = lookup.filenames[0];
          }
          property_processed = true;
        }

        if (!property_processed)
          texture_properties_skipped++;
      }
    }

    // If we couldn't find any assets, skip this material
    if (texture_properties_skipped ==
        generation_context->material_property_index_to_texture_filter.size())
    {
      continue;
    }

    picojson::object data;
    VideoCommon::MaterialData::ToJson(&data, output_material);
    const auto material_path = PathToString(output_dir / lookup.output_name) + ".material";
    if (!JsonToFile(material_path, picojson::value{data}, true))
    {
      *error = fmt::format("Failed to create json file '{}'", material_path);
      return;
    }
    generation_context->state->m_user_data.m_asset_library->AddAsset(material_path);
    const auto material_asset =
        generation_context->state->m_user_data.m_asset_library->GetAssetFromPath(material_path);
    if (!material_asset)
    {
      *error = fmt::format("Failed to get asset from path '{}'", material_path);
      return;
    }

    if (lookup.draw_call_id)
    {
      std::vector<CustomPipelineAction::PipelinePassPassDescription> pass_descriptions;
      CustomPipelineAction::PipelinePassPassDescription pass_description;
      pass_description.m_pixel_material_asset = material_asset->m_asset_id;
      pass_descriptions.push_back(std::move(pass_description));
      auto pipeline_action = std::make_unique<CustomPipelineAction>(
          generation_context->state->m_user_data.m_asset_library,
          generation_context->state->m_runtime_data.m_texture_cache, pass_descriptions);
      auto action = std::make_unique<EditorAction>(std::move(pipeline_action));
      action->SetID(generation_context->state->m_editor_data.m_next_action_id);

      auto& action_refs =
          generation_context->state->m_user_data.m_draw_call_id_to_actions[*lookup.draw_call_id];
      action_refs.push_back(action.get());

      generation_context->state->m_user_data.m_actions.push_back(std::move(action));
      generation_context->state->m_editor_data.m_next_action_id++;
    }
  }
}
}  // namespace GraphicsModEditor
