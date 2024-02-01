// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigLoaders/GameConfigLoader.h"

#include <algorithm>
#include <array>
#include <list>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/Config/WiimoteSettings.h"
#include "Core/ConfigLoaders/IsSettingSaveable.h"

namespace ConfigLoaders
{
// Returns all possible filenames in ascending order of priority
std::vector<std::string> GetGameIniFilenames(const std::string& id, std::optional<u16> revision)
{
  std::vector<std::string> filenames;

  if (id.empty())
    return filenames;

  // Using the first letter or the 3 letters of the ID only makes sense
  // if the ID is an actual game ID (which has 6 characters).
  if (id.length() == 6)
  {
    // INIs that match the system code (unique for each Virtual Console system)
    filenames.push_back(id.substr(0, 1) + ".ini");

    // INIs that match all regions
    filenames.push_back(id.substr(0, 3) + ".ini");
  }

  // Regular INIs
  filenames.push_back(id + ".ini");

  // INIs with specific revisions
  if (revision)
    filenames.push_back(id + fmt::format("r{}", *revision) + ".ini");

  return filenames;
}

using Location = Config::Location;
using INIToLocationMap = std::map<std::pair<std::string, std::string>, Location>;
using INIToSectionMap = std::map<std::string, std::pair<Config::System, std::string>>;

// This is a mapping from the legacy section-key pairs to Locations.
// New settings do not need to be added to this mapping.
// See also: MapINIToRealLocation and GetINILocationFromConfig.
static const INIToLocationMap& GetINIToLocationMap()
{
  static const INIToLocationMap ini_to_location = {
      {{"Core", "ProgressiveScan"}, {Config::SYSCONF_PROGRESSIVE_SCAN.GetLocation()}},
      {{"Core", "PAL60"}, {Config::SYSCONF_PAL60.GetLocation()}},
      {{"Wii", "Widescreen"}, {Config::SYSCONF_WIDESCREEN.GetLocation()}},
      {{"Wii", "Language"}, {Config::SYSCONF_LANGUAGE.GetLocation()}},
      {{"Core", "HLE_BS2"}, {Config::MAIN_SKIP_IPL.GetLocation()}},
      {{"Core", "GameCubeLanguage"}, {Config::MAIN_GC_LANGUAGE.GetLocation()}},
      {{"Controls", "PadType0"}, {Config::GetInfoForSIDevice(0).GetLocation()}},
      {{"Controls", "PadType1"}, {Config::GetInfoForSIDevice(1).GetLocation()}},
      {{"Controls", "PadType2"}, {Config::GetInfoForSIDevice(2).GetLocation()}},
      {{"Controls", "PadType3"}, {Config::GetInfoForSIDevice(3).GetLocation()}},
      {{"Controls", "WiimoteSource0"}, {Config::WIIMOTE_1_SOURCE.GetLocation()}},
      {{"Controls", "WiimoteSource1"}, {Config::WIIMOTE_2_SOURCE.GetLocation()}},
      {{"Controls", "WiimoteSource2"}, {Config::WIIMOTE_3_SOURCE.GetLocation()}},
      {{"Controls", "WiimoteSource3"}, {Config::WIIMOTE_4_SOURCE.GetLocation()}},
      {{"Controls", "WiimoteSourceBB"}, {Config::WIIMOTE_BB_SOURCE.GetLocation()}},
  };
  return ini_to_location;
}

// This is a mapping from the legacy section names to system + section.
// New settings do not need to be added to this mapping.
// See also: MapINIToRealLocation and GetINILocationFromConfig.
static const INIToSectionMap& GetINIToSectionMap()
{
  static const INIToSectionMap ini_to_section = {
      {"Core", {Config::System::Main, "Core"}},
      {"DSP", {Config::System::Main, "DSP"}},
      {"Display", {Config::System::Main, "Display"}},
      {"Video_Hardware", {Config::System::GFX, "Hardware"}},
      {"Video_Settings", {Config::System::GFX, "Settings"}},
      {"Video_Enhancements", {Config::System::GFX, "Enhancements"}},
      {"Video_Stereoscopy", {Config::System::GFX, "Stereoscopy"}},
      {"Video_Hacks", {Config::System::GFX, "Hacks"}},
      {"Video", {Config::System::GFX, "GameSpecific"}},
      {"Controls", {Config::System::GameSettingsOnly, "Controls"}},
  };
  return ini_to_section;
}

// Converts from a legacy GameINI section-key tuple to a Location.
// Also supports the following format:
// [System.Section]
// Key = Value
static Location MapINIToRealLocation(const std::string& section, const std::string& key)
{
  static const INIToLocationMap& ini_to_location = GetINIToLocationMap();
  const auto it = ini_to_location.find({section, key});
  if (it != ini_to_location.end())
    return it->second;

  static const INIToSectionMap& ini_to_section = GetINIToSectionMap();
  const auto it2 = ini_to_section.find(section);
  if (it2 != ini_to_section.end())
    return {it2->second.first, it2->second.second, key};

  // Attempt to load it as a configuration option
  // It will be in the format of '<System>.<Section>'
  std::istringstream buffer(section);
  std::string system_str, config_section;

  bool fail = false;
  std::getline(buffer, system_str, '.');
  fail |= buffer.fail();
  std::getline(buffer, config_section, '.');
  fail |= buffer.fail();

  const std::optional<Config::System> system = Config::GetSystemFromName(system_str);
  if (!fail && system)
    return {*system, config_section, key};

  WARN_LOG_FMT(CORE, "Unknown game INI option in section {}: {}", section, key);
  return {Config::System::Main, "", ""};
}

static std::pair<std::string, std::string> GetINILocationFromConfig(const Location& location)
{
  static const INIToLocationMap& ini_to_location = GetINIToLocationMap();
  const auto it = std::find_if(ini_to_location.begin(), ini_to_location.end(),
                               [&location](const auto& entry) { return entry.second == location; });
  if (it != ini_to_location.end())
    return it->first;

  static const INIToSectionMap& ini_to_section = GetINIToSectionMap();
  const auto it2 =
      std::find_if(ini_to_section.begin(), ini_to_section.end(), [&location](const auto& entry) {
        return entry.second.first == location.system && entry.second.second == location.section;
      });
  if (it2 != ini_to_section.end())
    return {it2->first, location.key};

  return {Config::GetSystemName(location.system) + "." + location.section, location.key};
}

// INI Game layer configuration loader
class INIGameConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  INIGameConfigLayerLoader(const std::string& id, u16 revision, bool global)
      : ConfigLayerLoader(global ? Config::LayerType::GlobalGame : Config::LayerType::LocalGame),
        m_id(id), m_revision(revision)
  {
  }

  void Load(Config::Layer* layer) override
  {
    Common::IniFile ini;
    if (layer->GetLayer() == Config::LayerType::GlobalGame)
    {
      for (const std::string& filename : GetGameIniFilenames(m_id, m_revision))
        ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
    }
    else
    {
      for (const std::string& filename : GetGameIniFilenames(m_id, m_revision))
        ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
    }

    const auto& system_sections = ini.GetSections();

    for (const auto& section : system_sections)
    {
      LoadFromSystemSection(layer, section);
    }

    LoadControllerConfig(layer);
  }

  void Save(Config::Layer* layer) override;

private:
  void LoadControllerConfig(Config::Layer* layer) const
  {
    // Game INIs can have controller profiles embedded in to them
    static const std::array<char, 4> nums = {{'1', '2', '3', '4'}};

    if (m_id == "00000000")
      return;

    const std::array<std::tuple<std::string, std::string, Config::System>, 2> profile_info = {{
        std::make_tuple("Pad", "GCPad", Config::System::GCPad),
        std::make_tuple("Wiimote", "Wiimote", Config::System::WiiPad),
    }};

    for (const auto& use_data : profile_info)
    {
      std::string type = std::get<0>(use_data);
      std::string path = "Profiles/" + std::get<1>(use_data) + "/";

      const auto control_section = [&](std::string key) {
        return Config::Location{std::get<2>(use_data), "Controls", key};
      };

      for (const char num : nums)
      {
        if (auto profile = layer->Get<std::string>(control_section(type + "Profile" + num)))
        {
          std::string ini_path = File::GetUserPath(D_CONFIG_IDX) + path + *profile + ".ini";
          if (!File::Exists(ini_path))
          {
            // TODO: PanicAlert shouldn't be used for this.
            PanicAlertFmtT("Selected controller profile does not exist");
            continue;
          }

          Common::IniFile profile_ini;
          profile_ini.Load(ini_path);

          const auto* ini_section = profile_ini.GetOrCreateSection("Profile");
          const auto& section_map = ini_section->GetValues();
          for (const auto& value : section_map)
          {
            Config::Location location{std::get<2>(use_data), std::get<1>(use_data) + num,
                                      value.first};
            layer->Set(location, value.second);
          }
        }
      }
    }
  }

  void LoadFromSystemSection(Config::Layer* layer, const Common::IniFile::Section& section) const
  {
    const std::string section_name = section.GetName();

    // Regular key,value pairs
    const auto& section_map = section.GetValues();

    for (const auto& value : section_map)
    {
      const auto location = MapINIToRealLocation(section_name, value.first);

      if (location.section.empty() && location.key.empty())
        continue;

      if (location.system == Config::System::Session)
        continue;

      layer->Set(location, value.second);
    }
  }

  const std::string m_id;
  const u16 m_revision;
};

void INIGameConfigLayerLoader::Save(Config::Layer* layer)
{
  if (layer->GetLayer() != Config::LayerType::LocalGame)
    return;

  Common::IniFile ini;
  for (const std::string& file_name : GetGameIniFilenames(m_id, m_revision))
    ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + file_name, true);

  for (const auto& config : layer->GetLayerMap())
  {
    const Config::Location& location = config.first;
    const std::optional<std::string>& value = config.second;

    if (!IsSettingSaveable(location) || location.system == Config::System::Session)
      continue;

    const auto ini_location = GetINILocationFromConfig(location);
    if (ini_location.first.empty() && ini_location.second.empty())
      continue;

    if (value)
    {
      auto* ini_section = ini.GetOrCreateSection(ini_location.first);
      ini_section->Set(ini_location.second, *value);
    }
    else
    {
      ini.DeleteKey(ini_location.first, ini_location.second);
    }
  }

  // Try to save to the revision specific INI first, if it exists.
  const std::string gameini_with_rev =
      File::GetUserPath(D_GAMESETTINGS_IDX) + m_id + fmt::format("r{}", m_revision) + ".ini";
  if (File::Exists(gameini_with_rev))
  {
    ini.Save(gameini_with_rev);
    return;
  }

  // Otherwise, save to the game INI. We don't try any INI broader than that because it will
  // likely cause issues with cheat codes and game patches.
  const std::string gameini = File::GetUserPath(D_GAMESETTINGS_IDX) + m_id + ".ini";
  ini.Save(gameini);
}

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader> GenerateGlobalGameConfigLoader(const std::string& id,
                                                                          u16 revision)
{
  return std::make_unique<INIGameConfigLayerLoader>(id, revision, true);
}

std::unique_ptr<Config::ConfigLayerLoader> GenerateLocalGameConfigLoader(const std::string& id,
                                                                         u16 revision)
{
  return std::make_unique<INIGameConfigLayerLoader>(id, revision, false);
}
}  // namespace ConfigLoaders
