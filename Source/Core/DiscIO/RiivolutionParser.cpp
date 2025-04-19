// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/RiivolutionParser.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <pugixml.hpp>

#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/StringUtil.h"
#include "DiscIO/GameModDescriptor.h"
#include "DiscIO/RiivolutionPatcher.h"

namespace DiscIO::Riivolution
{
Patch::~Patch() = default;

std::optional<Disc> ParseFile(const std::string& filename)
{
  ::File::IOFile f(filename, "rb");
  if (!f)
    return std::nullopt;

  std::vector<char> data;
  data.resize(f.GetSize());
  if (!f.ReadBytes(data.data(), data.size()))
    return std::nullopt;

  return ParseString(std::string_view(data.data(), data.size()), filename);
}

static std::map<std::string, std::string> ReadParams(const pugi::xml_node& node,
                                                     std::map<std::string, std::string> params = {})
{
  for (const auto& param_node : node.children("param"))
  {
    const std::string param_name = param_node.attribute("name").as_string();
    const std::string param_value = param_node.attribute("value").as_string();
    params[param_name] = param_value;
  }
  return params;
}

static std::vector<u8> ReadHexString(std::string_view sv)
{
  if ((sv.size() % 2) == 1)
    return {};
  if (sv.starts_with("0x") || sv.starts_with("0X"))
    sv = sv.substr(2);

  std::vector<u8> result;
  result.reserve(sv.size() / 2);
  while (!sv.empty())
  {
    u8 tmp;
    if (!TryParse(std::string(sv.substr(0, 2)), &tmp, 16))
      return {};
    result.push_back(tmp);
    sv = sv.substr(2);
  }
  return result;
}

std::optional<Disc> ParseString(std::string_view xml, std::string xml_path)
{
  pugi::xml_document doc;
  const auto parse_result = doc.load_buffer(xml.data(), xml.size());
  if (!parse_result)
    return std::nullopt;

  const auto wiidisc = doc.child("wiidisc");
  if (!wiidisc)
    return std::nullopt;

  Disc disc;
  disc.m_xml_path = std::move(xml_path);
  disc.m_version = wiidisc.attribute("version").as_int(-1);
  if (disc.m_version != 1)
    return std::nullopt;
  const std::string default_root = wiidisc.attribute("root").as_string();

  const auto id = wiidisc.child("id");
  if (id)
  {
    for (const auto& attribute : id.attributes())
    {
      const std::string_view attribute_name(attribute.name());
      if (attribute_name == "game")
        disc.m_game_filter.m_game = attribute.as_string();
      else if (attribute_name == "developer")
        disc.m_game_filter.m_developer = attribute.as_string();
      else if (attribute_name == "disc")
        disc.m_game_filter.m_disc = attribute.as_int(-1);
      else if (attribute_name == "version")
        disc.m_game_filter.m_version = attribute.as_int(-1);
    }

    auto xml_regions = id.children("region");
    if (xml_regions.begin() != xml_regions.end())
    {
      std::vector<std::string> regions;
      for (const auto& region : xml_regions)
        regions.push_back(region.attribute("type").as_string());
      disc.m_game_filter.m_regions = std::move(regions);
    }
  }

  const auto options = wiidisc.child("options");
  if (options)
  {
    for (const auto& section_node : options.children("section"))
    {
      auto& [section_name, section_options] = disc.m_sections.emplace_back();
      section_name = section_node.attribute("name").as_string();
      for (const auto& option_node : section_node.children("option"))
      {
        auto& [option_name, option_id, choices, selected_choice] = section_options.emplace_back();
        option_id = option_node.attribute("id").as_string();
        option_name = option_node.attribute("name").as_string();
        selected_choice = option_node.attribute("default").as_uint(0);
        auto option_params = ReadParams(option_node);
        for (const auto& choice_node : option_node.children("choice"))
        {
          auto& [choice_name, patch_references] = choices.emplace_back();
          choice_name = choice_node.attribute("name").as_string();
          auto choice_params = ReadParams(choice_node, option_params);
          for (const auto& patchref_node : choice_node.children("patch"))
          {
            auto& [patch_reference_id, params] = patch_references.emplace_back();
            patch_reference_id = patchref_node.attribute("id").as_string();
            params = ReadParams(patchref_node, choice_params);
          }
        }
      }
    }
    for (const auto& macro_node : options.children("macros"))
    {
      const std::string macro_id = macro_node.attribute("id").as_string();
      for (auto& [name, section_options] : disc.m_sections)
      {
        auto option_to_clone = std::ranges::find(section_options, macro_id, &Option::m_id);
        if (option_to_clone == section_options.end())
          continue;

        Option cloned_option = *option_to_clone;
        cloned_option.m_name = macro_node.attribute("name").as_string();
        for (auto& [choice_name, patch_references] : cloned_option.m_choices)
          for (auto& [patch_reference_id, params] : patch_references)
            params = ReadParams(macro_node, params);
      }
    }
  }

  const auto patches = wiidisc.children("patch");
  for (const auto& patch_node : patches)
  {
    Patch& patch = disc.m_patches.emplace_back();
    patch.m_id = patch_node.attribute("id").as_string();
    patch.m_root = patch_node.attribute("root").as_string();
    if (patch.m_root.empty())
      patch.m_root = default_root;

    for (const auto& patch_subnode : patch_node.children())
    {
      const std::string_view patch_name(patch_subnode.name());
      if (patch_name == "file" || patch_name == "dolphin_sys_file")
      {
        auto& [patch_disc, external, resize, create, offset, fileoffset, length] =
            patch_name == "dolphin_sys_file" ? patch.m_sys_file_patches.emplace_back() :
                                               patch.m_file_patches.emplace_back();
        patch_disc = patch_subnode.attribute("disc").as_string();
        external = patch_subnode.attribute("external").as_string();
        resize = patch_subnode.attribute("resize").as_bool(true);
        create = patch_subnode.attribute("create").as_bool(false);
        offset = patch_subnode.attribute("offset").as_uint(0);
        fileoffset = patch_subnode.attribute("fileoffset").as_uint(0);
        length = patch_subnode.attribute("length").as_uint(0);
      }
      else if (patch_name == "folder" || patch_name == "dolphin_sys_folder")
      {
        auto& [patch_disc, external, resize, create, recursive, length] =
            patch_name == "dolphin_sys_folder" ? patch.m_sys_folder_patches.emplace_back() :
                                                 patch.m_folder_patches.emplace_back();
        patch_disc = patch_subnode.attribute("disc").as_string();
        external = patch_subnode.attribute("external").as_string();
        resize = patch_subnode.attribute("resize").as_bool(true);
        create = patch_subnode.attribute("create").as_bool(false);
        recursive = patch_subnode.attribute("recursive").as_bool(true);
        length = patch_subnode.attribute("length").as_uint(0);
      }
      else if (patch_name == "savegame")
      {
        auto& [external, clone] = patch.m_savegame_patches.emplace_back();
        external = patch_subnode.attribute("external").as_string();
        clone = patch_subnode.attribute("clone").as_bool(true);
      }
      else if (patch_name == "memory")
      {
        auto& [offset, value, valuefile, original, ocarina, search, align] =
            patch.m_memory_patches.emplace_back();
        offset = patch_subnode.attribute("offset").as_uint(0);
        value = ReadHexString(patch_subnode.attribute("value").as_string());
        valuefile = patch_subnode.attribute("valuefile").as_string();
        original = ReadHexString(patch_subnode.attribute("original").as_string());
        ocarina = patch_subnode.attribute("ocarina").as_bool(false);
        search = patch_subnode.attribute("search").as_bool(false);
        align = patch_subnode.attribute("align").as_uint(1);
      }
    }
  }

  return disc;
}

static bool CheckRegion(const std::vector<std::string>& xml_regions, std::string_view game_region)
{
  if (xml_regions.begin() == xml_regions.end())
    return true;

  for (const auto& region : xml_regions)
  {
    if (region == game_region)
      return true;
  }

  return false;
}

bool Disc::IsValidForGame(const std::string& game_id, std::optional<u16> revision,
                          std::optional<u8> disc_number) const
{
  if (game_id.size() != 6)
    return false;

  const std::string_view game_id_full = std::string_view(game_id);
  const std::string_view game_region = game_id_full.substr(3, 1);
  const std::string_view game_developer = game_id_full.substr(4, 2);
  const int disc_number_int = std::optional<int>(disc_number).value_or(-1);
  const int revision_int = std::optional<int>(revision).value_or(-1);

  if (m_game_filter.m_game && !game_id_full.starts_with(*m_game_filter.m_game))
    return false;
  if (m_game_filter.m_developer && game_developer != *m_game_filter.m_developer)
    return false;
  if (m_game_filter.m_disc && disc_number_int != *m_game_filter.m_disc)
    return false;
  if (m_game_filter.m_version && revision_int != *m_game_filter.m_version)
    return false;
  if (m_game_filter.m_regions && !CheckRegion(*m_game_filter.m_regions, game_region))
    return false;

  return true;
}

std::vector<Patch> Disc::GeneratePatches(const std::string& game_id) const
{
  const std::string_view game_id_full = std::string_view(game_id);
  const std::string_view game_id_no_region = game_id_full.substr(0, 3);
  const std::string_view game_region = game_id_full.substr(3, 1);
  const std::string_view game_developer = game_id_full.substr(4, 2);

  const auto replace_variables =
      [&](std::string_view sv,
          const std::vector<std::pair<std::string, std::string_view>>& replacements) {
        std::string result;
        result.reserve(sv.size());
        while (!sv.empty())
        {
          bool replaced = false;
          for (const auto& [first, second] : replacements)
          {
            if (sv.starts_with(first))
            {
              for (char c : second)
                result.push_back(c);
              sv = sv.substr(first.size());
              replaced = true;
              break;
            }
          }
          if (replaced)
            continue;
          result.push_back(sv[0]);
          sv = sv.substr(1);
        }
        return result;
      };

  // Take only selected patches, replace placeholders in all strings, and return them.
  std::vector<Patch> active_patches;
  for (const auto& [name, options] : m_sections)
  {
    for (const auto& option : options)
    {
      const u32 selected = option.m_selected_choice;
      if (selected == 0 || selected > option.m_choices.size())
        continue;
      const auto& [choice_name, patch_references] = option.m_choices[selected - 1];
      for (const auto& [id, params] : patch_references)
      {
        const auto patch = std::ranges::find(m_patches, id, &Patch::m_id);
        if (patch == m_patches.end())
          continue;

        std::vector<std::pair<std::string, std::string_view>> replacements;
        replacements.emplace_back(std::pair{"{$__gameid}", game_id_no_region});
        replacements.emplace_back(std::pair{"{$__region}", game_region});
        replacements.emplace_back(std::pair{"{$__maker}", game_developer});
        for (const auto& [first, second] : params)
          replacements.emplace_back(std::pair{"{$" + first + "}", second});

        Patch& new_patch = active_patches.emplace_back(*patch);
        new_patch.m_root = replace_variables(new_patch.m_root, replacements);
        for (auto& file : new_patch.m_file_patches)
        {
          file.m_disc = replace_variables(file.m_disc, replacements);
          file.m_external = replace_variables(file.m_external, replacements);
        }
        for (auto& folder : new_patch.m_folder_patches)
        {
          folder.m_disc = replace_variables(folder.m_disc, replacements);
          folder.m_external = replace_variables(folder.m_external, replacements);
        }
        for (auto& [external, clone] : new_patch.m_savegame_patches)
        {
          external = replace_variables(external, replacements);
        }
        for (auto& memory : new_patch.m_memory_patches)
        {
          memory.m_valuefile = replace_variables(memory.m_valuefile, replacements);
        }
      }
    }
  }

  return active_patches;
}

std::vector<Patch> GenerateRiivolutionPatchesFromGameModDescriptor(
    const GameModDescriptorRiivolution& descriptor, const std::string& game_id,
    std::optional<u16> revision, std::optional<u8> disc_number)
{
  std::vector<Patch> result;
  for (const auto& [xml, root, options] : descriptor.patches)
  {
    auto parsed = ParseFile(xml);
    if (!parsed || !parsed->IsValidForGame(game_id, revision, disc_number))
      continue;

    for (auto& [section_name, section_options] : parsed->m_sections)
    {
      for (auto& [option_name, id, choices, selected_choice] : section_options)
      {
        const auto* info = [&]() -> const GameModDescriptorRiivolutionPatchOption* {
          for (const auto& o : options)
          {
            if (o.section_name == section_name)
            {
              if (!o.option_id.empty() && o.option_id == id)
                return &o;
              if (!o.option_name.empty() && o.option_name == option_name)
                return &o;
            }
          }
          return nullptr;
        }();
        if (info && info->choice <= choices.size())
          selected_choice = info->choice;
      }
    }

    for (auto& p : parsed->GeneratePatches(game_id))
    {
      p.m_file_data_loader =
          std::make_shared<FileDataLoaderHostFS>(root, parsed->m_xml_path, p.m_root);
      result.emplace_back(std::move(p));
    }
  }
  return result;
}

std::vector<Patch> GenerateRiivolutionPatchesFromConfig(const std::string root_directory,
                                                        const std::string& game_id,
                                                        std::optional<u16> revision,
                                                        std::optional<u8> disc_number)
{
  std::vector<Patch> result;

  // The way Riivolution stores settings only makes sense for standard game IDs.
  if (!(game_id.size() == 4 || game_id.size() == 6))
    return result;

  const std::optional<Config> config = ParseConfigFile(
      fmt::format("{}/riivolution/config/{}.xml", root_directory, game_id.substr(0, 4)));

  for (const std::string& path : Common::DoFileSearch({root_directory + "riivolution"}, {".xml"}))
  {
    std::optional<Disc> parsed = ParseFile(path);
    if (!parsed || !parsed->IsValidForGame(game_id, revision, disc_number))
      continue;
    if (config)
      ApplyConfigDefaults(&*parsed, *config);

    for (auto& patch : parsed->GeneratePatches(game_id))
    {
      patch.m_file_data_loader =
          std::make_shared<FileDataLoaderHostFS>(root_directory, parsed->m_xml_path, patch.m_root);
      result.emplace_back(std::move(patch));
    }
  }

  return result;
}

std::optional<Config> ParseConfigFile(const std::string& filename)
{
  ::File::IOFile f(filename, "rb");
  if (!f)
    return std::nullopt;

  std::vector<char> data;
  data.resize(f.GetSize());
  if (!f.ReadBytes(data.data(), data.size()))
    return std::nullopt;

  return ParseConfigString(std::string_view(data.data(), data.size()));
}

std::optional<Config> ParseConfigString(std::string_view xml)
{
  pugi::xml_document doc;
  const auto parse_result = doc.load_buffer(xml.data(), xml.size());
  if (!parse_result)
    return std::nullopt;

  const auto riivolution = doc.child("riivolution");
  if (!riivolution)
    return std::nullopt;

  Config config;
  config.m_version = riivolution.attribute("version").as_int(-1);
  if (config.m_version != 2)
    return std::nullopt;

  const auto options = riivolution.children("option");
  for (const auto& option_node : options)
  {
    auto& [option_id, option_default] = config.m_options.emplace_back();
    option_id = option_node.attribute("id").as_string();
    option_default = option_node.attribute("default").as_uint(0);
  }

  return config;
}

std::string WriteConfigString(const Config& config)
{
  pugi::xml_document doc;
  auto riivolution = doc.append_child("riivolution");
  riivolution.append_attribute("version").set_value(config.m_version);
  for (const auto& [option_id, option_default] : config.m_options)
  {
    auto option_node = riivolution.append_child("option");
    option_node.append_attribute("id").set_value(option_id.c_str());
    option_node.append_attribute("default").set_value(option_default);
  }

  std::stringstream ss;
  doc.print(ss, "  ");
  return ss.str();
}

bool WriteConfigFile(const std::string& filename, const Config& config)
{
  auto xml = WriteConfigString(config);
  if (xml.empty())
    return false;

  ::File::CreateFullPath(filename);
  ::File::IOFile f(filename, "wb");
  if (!f)
    return false;

  if (!f.WriteString(xml))
    return false;

  return true;
}

void ApplyConfigDefaults(Disc* disc, const Config& config)
{
  for (const auto& [option_id, option_default] : config.m_options)
  {
    auto* matching_option = [&]() -> Option* {
      for (auto& [name, options] : disc->m_sections)
      {
        for (auto& option : options)
        {
          if (option.m_id.empty())
          {
            if ((name + option.m_name) == option_id)
              return &option;
          }
          else
          {
            if (option.m_id == option_id)
              return &option;
          }
        }
      }
      return nullptr;
    }();
    if (matching_option)
      matching_option->m_selected_choice = option_default;
  }
}
}  // namespace DiscIO::Riivolution
