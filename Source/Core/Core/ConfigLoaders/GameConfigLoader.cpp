// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ConfigLoaders/GameConfigLoader.h"

namespace ConfigLoaders
{
// Returns all possible filenames in ascending order of priority
static std::vector<std::string> GetGameIniFilenames(const std::string& id, u16 revision)
{
  std::vector<std::string> filenames;

  // INIs that match all regions
  if (id.size() >= 4)
    filenames.push_back(id.substr(0, 3) + ".ini");

  // Regular INIs
  filenames.push_back(id + ".ini");

  // INIs with specific revisions
  filenames.push_back(id + StringFromFormat("r%d", revision) + ".ini");

  return filenames;
}

struct ConfigLocation
{
  Config::System system;
  std::string section;
  std::string key;

  bool operator==(const ConfigLocation& other) const
  {
    return std::tie(system, section, key) == std::tie(other.system, other.section, other.key);
  }

  bool operator!=(const ConfigLocation& other) const { return !operator==(other); }
};

static std::map<std::pair<std::string, std::string>, ConfigLocation> ini_to_location = {
    // Core
    {{"Core", "CPUThread"}, {Config::System::Main, "Core", "CPUThread"}},
    {{"Core", "SkipIdle"}, {Config::System::Main, "Core", "SkipIdle"}},
    {{"Core", "SyncOnSkipIdle"}, {Config::System::Main, "Core", "SyncOnSkipIdle"}},
    {{"Core", "FPRF"}, {Config::System::Main, "Core", "FPRF"}},
    {{"Core", "AccurateNaNs"}, {Config::System::Main, "Core", "AccurateNaNs"}},
    {{"Core", "MMU"}, {Config::System::Main, "Core", "MMU"}},
    {{"Core", "DCBZ"}, {Config::System::Main, "Core", "DCBZ"}},
    {{"Core", "SyncGPU"}, {Config::System::Main, "Core", "SyncGPU"}},
    {{"Core", "FastDiscSpeed"}, {Config::System::Main, "Core", "FastDiscSpeed"}},
    {{"Core", "DSPHLE"}, {Config::System::Main, "Core", "DSPHLE"}},
    {{"Core", "GFXBackend"}, {Config::System::Main, "Core", "GFXBackend"}},
    {{"Core", "CPUCore"}, {Config::System::Main, "Core", "CPUCore"}},
    {{"Core", "HLE_BS2"}, {Config::System::Main, "Core", "HLE_BS2"}},
    {{"Core", "EmulationSpeed"}, {Config::System::Main, "Core", "EmulationSpeed"}},
    {{"Core", "GPUDeterminismMode"}, {Config::System::Main, "Core", "GPUDeterminismMode"}},
    {{"Core", "ProgressiveScan"}, {Config::System::Main, "Display", "ProgressiveScan"}},
    {{"Core", "PAL60"}, {Config::System::Main, "Display", "PAL60"}},

    // Action Replay cheats
    {{"ActionReplay_Enabled", ""}, {Config::System::Main, "ActionReplay_Enabled", ""}},
    {{"ActionReplay", ""}, {Config::System::Main, "ActionReplay", ""}},

    // Gecko cheats
    {{"Gecko_Enabled", ""}, {Config::System::Main, "Gecko_Enabled", ""}},
    {{"Gecko", ""}, {Config::System::Main, "Gecko", ""}},

    // OnLoad cheats
    {{"OnLoad", ""}, {Config::System::Main, "OnLoad", ""}},

    // OnFrame cheats
    {{"OnFrame_Enabled", ""}, {Config::System::Main, "OnFrame_Enabled", ""}},
    {{"OnFrame", ""}, {Config::System::Main, "OnFrame", ""}},

    // Speedhacks
    {{"Speedhacks", ""}, {Config::System::Main, "Speedhacks", ""}},

    // Debugger values
    {{"Watches", ""}, {Config::System::Debugger, "Watches", ""}},
    {{"BreakPoints", ""}, {Config::System::Debugger, "BreakPoints", ""}},
    {{"MemoryChecks", ""}, {Config::System::Debugger, "MemoryChecks", ""}},

    // DSP
    {{"DSP", "Volume"}, {Config::System::Main, "DSP", "Volume"}},
    {{"DSP", "EnableJIT"}, {Config::System::Main, "DSP", "EnableJIT"}},
    {{"DSP", "Backend"}, {Config::System::Main, "DSP", "Backend"}},

    // Controls
    {{"Controls", "PadType0"}, {Config::System::GCPad, "Core", "SIDevice0"}},
    {{"Controls", "PadType1"}, {Config::System::GCPad, "Core", "SIDevice1"}},
    {{"Controls", "PadType2"}, {Config::System::GCPad, "Core", "SIDevice2"}},
    {{"Controls", "PadType3"}, {Config::System::GCPad, "Core", "SIDevice3"}},
    {{"Controls", "WiimoteSource0"}, {Config::System::WiiPad, "Wiimote1", "Source"}},
    {{"Controls", "WiimoteSource1"}, {Config::System::WiiPad, "Wiimote2", "Source"}},
    {{"Controls", "WiimoteSource2"}, {Config::System::WiiPad, "Wiimote3", "Source"}},
    {{"Controls", "WiimoteSource3"}, {Config::System::WiiPad, "Wiimote4", "Source"}},
    {{"Controls", "WiimoteSourceBB"}, {Config::System::WiiPad, "BalanceBoard", "Source"}},

    // Controller profiles
    {{"Controls", "PadProfile1"}, {Config::System::GCPad, "Controls", "PadProfile1"}},
    {{"Controls", "PadProfile2"}, {Config::System::GCPad, "Controls", "PadProfile2"}},
    {{"Controls", "PadProfile3"}, {Config::System::GCPad, "Controls", "PadProfile3"}},
    {{"Controls", "PadProfile4"}, {Config::System::GCPad, "Controls", "PadProfile4"}},
    {{"Controls", "WiimoteProfile1"}, {Config::System::WiiPad, "Controls", "WiimoteProfile1"}},
    {{"Controls", "WiimoteProfile2"}, {Config::System::WiiPad, "Controls", "WiimoteProfile2"}},
    {{"Controls", "WiimoteProfile3"}, {Config::System::WiiPad, "Controls", "WiimoteProfile3"}},
    {{"Controls", "WiimoteProfile4"}, {Config::System::WiiPad, "Controls", "WiimoteProfile4"}},

    // Video
    {{"Video_Hardware", "VSync"}, {Config::System::GFX, "Hardware", "VSync"}},
    {{"Video_Settings", "wideScreenHack"}, {Config::System::GFX, "Settings", "wideScreenHack"}},
    {{"Video_Settings", "AspectRatio"}, {Config::System::GFX, "Settings", "AspectRatio"}},
    {{"Video_Settings", "Crop"}, {Config::System::GFX, "Settings", "Crop"}},
    {{"Video_Settings", "UseXFB"}, {Config::System::GFX, "Settings", "UseXFB"}},
    {{"Video_Settings", "UseRealXFB"}, {Config::System::GFX, "Settings", "UseRealXFB"}},
    {{"Video_Settings", "SafeTextureCacheColorSamples"},
     {Config::System::GFX, "Settings", "SafeTextureCacheColorSamples"}},
    {{"Video_Settings", "HiresTextures"}, {Config::System::GFX, "Settings", "HiresTextures"}},
    {{"Video_Settings", "ConvertHiresTextures"},
     {Config::System::GFX, "Settings", "ConvertHiresTextures"}},
    {{"Video_Settings", "CacheHiresTextures"},
     {Config::System::GFX, "Settings", "CacheHiresTextures"}},
    {{"Video_Settings", "EnablePixelLighting"},
     {Config::System::GFX, "Settings", "EnablePixelLighting"}},
    {{"Video_Settings", "ForcedSlowDepth"}, {Config::System::GFX, "Settings", "ForcedSlowDepth"}},
    {{"Video_Settings", "MSAA"}, {Config::System::GFX, "Settings", "MSAA"}},
    {{"Video_Settings", "SSAA"}, {Config::System::GFX, "Settings", "SSAA"}},
    {{"Video_Settings", "EFBScale"}, {Config::System::GFX, "Settings", "EFBScale"}},
    {{"Video_Settings", "DisableFog"}, {Config::System::GFX, "Settings", "DisableFog"}},

    {{"Video_Enhancements", "ForceFiltering"},
     {Config::System::GFX, "Enhancements", "ForceFiltering"}},
    {{"Video_Enhancements", "MaxAnisotropy"},
     {Config::System::GFX, "Enhancements", "MaxAnisotropy"}},
    {{"Video_Enhancements", "PostProcessingShader"},
     {Config::System::GFX, "Enhancements", "PostProcessingShader"}},

    {{"Video_Stereoscopy", "StereoMode"}, {Config::System::GFX, "Stereoscopy", "StereoMode"}},
    {{"Video_Stereoscopy", "StereoDepth"}, {Config::System::GFX, "Stereoscopy", "StereoDepth"}},
    {{"Video_Stereoscopy", "StereoSwapEyes"},
     {Config::System::GFX, "Stereoscopy", "StereoSwapEyes"}},

    {{"Video_Hacks", "EFBAccessEnable"}, {Config::System::GFX, "Hacks", "EFBAccessEnable"}},
    {{"Video_Hacks", "BBoxEnable"}, {Config::System::GFX, "Hacks", "BBoxEnable"}},
    {{"Video_Hacks", "ForceProgressive"}, {Config::System::GFX, "Hacks", "ForceProgressive"}},
    {{"Video_Hacks", "EFBToTextureEnable"}, {Config::System::GFX, "Hacks", "EFBToTextureEnable"}},
    {{"Video_Hacks", "EFBScaledCopy"}, {Config::System::GFX, "Hacks", "EFBScaledCopy"}},
    {{"Video_Hacks", "EFBEmulateFormatChanges"},
     {Config::System::GFX, "Hacks", "EFBEmulateFormatChanges"}},

    // GameINI specific video settings
    {{"Video", "ProjectionHack"}, {Config::System::GFX, "Video", "ProjectionHack"}},
    {{"Video", "PH_SZNear"}, {Config::System::GFX, "Video", "PH_SZNear"}},
    {{"Video", "PH_SZFar"}, {Config::System::GFX, "Video", "PH_SZFar"}},
    {{"Video", "PH_ZNear"}, {Config::System::GFX, "Video", "PH_ZNear"}},
    {{"Video", "PH_ZFar"}, {Config::System::GFX, "Video", "PH_ZFar"}},
    {{"Video", "PH_ExtraParam"}, {Config::System::GFX, "Video", "PH_ExtraParam"}},
    {{"Video", "PerfQueriesEnable"}, {Config::System::GFX, "Video", "PerfQueriesEnable"}},

    {{"Video_Stereoscopy", "StereoConvergence"},
     {Config::System::GFX, "Stereoscopy", "StereoConvergence"}},
    {{"Video_Stereoscopy", "StereoEFBMonoDepth"},
     {Config::System::GFX, "Stereoscopy", "StereoEFBMonoDepth"}},
    {{"Video_Stereoscopy", "StereoDepthPercentage"},
     {Config::System::GFX, "Stereoscopy", "StereoDepthPercentage"}},

    // UI
    {{"EmuState", "EmulationStateId"}, {Config::System::UI, "EmuState", "EmulationStateId"}},
    {{"EmuState", "EmulationIssues"}, {Config::System::UI, "EmuState", "EmulationIssues"}},
    {{"EmuState", "Title"}, {Config::System::UI, "EmuState", "Title"}},
};

static ConfigLocation MapINIToRealLocation(const std::string& section, const std::string& key)
{
  auto it = ini_to_location.find({section, key});
  if (it == ini_to_location.end())
  {
    // Try again, but this time with an empty key
    // Certain sections like 'Speedhacks' has keys that are variable
    it = ini_to_location.find({section, ""});
    if (it != ini_to_location.end())
      return it->second;

    // Attempt to load it as a configuration option
    // It will be in the format of '<System>.<Section>'
    std::istringstream buffer(section);
    std::string system_str, config_section;

    bool fail = false;
    std::getline(buffer, system_str, '.');
    fail |= buffer.fail();
    std::getline(buffer, config_section, '.');
    fail |= buffer.fail();

    if (!fail)
      return {Config::GetSystemFromName(system_str), config_section, key};

    WARN_LOG(CORE, "Unknown game INI option in section %s: %s", section.c_str(), key.c_str());
    return {Config::System::Main, "", ""};
  }

  return ini_to_location[{section, key}];
}

static std::pair<std::string, std::string> GetINILocationFromConfig(const ConfigLocation& location)
{
  auto it = std::find_if(ini_to_location.begin(), ini_to_location.end(),
                         [&location](const auto& entry) { return entry.second == location; });

  if (it != ini_to_location.end())
    return it->first;

  // Try again, but this time with an empty key
  // Certain sections like 'Speedhacks' have keys that are variable
  it = std::find_if(ini_to_location.begin(), ini_to_location.end(), [&location](const auto& entry) {
    return std::tie(entry.second.system, entry.second.key) ==
           std::tie(location.system, location.key);
  });
  if (it != ini_to_location.end())
    return it->first;

  WARN_LOG(CORE, "Unknown option: %s.%s", location.section.c_str(), location.key.c_str());
  return {"", ""};
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

  void Load(Config::Layer* config_layer) override
  {
    IniFile ini;
    if (config_layer->GetLayer() == Config::LayerType::GlobalGame)
    {
      for (const std::string& filename : GetGameIniFilenames(m_id, m_revision))
        ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
    }
    else
    {
      for (const std::string& filename : GetGameIniFilenames(m_id, m_revision))
        ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
    }

    const std::list<IniFile::Section>& system_sections = ini.GetSections();

    for (const auto& section : system_sections)
    {
      LoadFromSystemSection(config_layer, section);
    }

    LoadControllerConfig(config_layer);
  }

  void Save(Config::Layer* config_layer) override;

private:
  void LoadControllerConfig(Config::Layer* config_layer) const
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

      Config::Section* control_section =
          config_layer->GetOrCreateSection(std::get<2>(use_data), "Controls");

      for (const char num : nums)
      {
        bool use_profile = false;
        std::string profile;
        if (control_section->Exists(type + "Profile" + num))
        {
          if (control_section->Get(type + "Profile" + num, &profile))
          {
            if (File::Exists(File::GetUserPath(D_CONFIG_IDX) + path + profile + ".ini"))
            {
              use_profile = true;
            }
            else
            {
              // TODO: PanicAlert shouldn't be used for this.
              PanicAlertT("Selected controller profile does not exist");
            }
          }
        }

        if (use_profile)
        {
          IniFile profile_ini;
          profile_ini.Load(File::GetUserPath(D_CONFIG_IDX) + path + profile + ".ini");

          const IniFile::Section* ini_section = profile_ini.GetOrCreateSection("Profile");
          const IniFile::Section::SectionMap& section_map = ini_section->GetValues();
          for (const auto& value : section_map)
          {
            Config::Section* section = config_layer->GetOrCreateSection(
                std::get<2>(use_data), std::get<1>(use_data) + num);
            section->Set(value.first, value.second);
          }
        }
      }
    }
  }

  void LoadFromSystemSection(Config::Layer* config_layer, const IniFile::Section& section) const
  {
    const std::string section_name = section.GetName();
    if (section.HasLines())
    {
      // Trash INI File chunks
      std::vector<std::string> chunk;
      section.GetLines(&chunk, true);

      if (chunk.size())
      {
        const auto mapped_config = MapINIToRealLocation(section_name, "");

        if (mapped_config.section.empty() && mapped_config.key.empty())
          return;

        auto* config_section =
            config_layer->GetOrCreateSection(mapped_config.system, mapped_config.section);
        config_section->SetLines(chunk);
      }
    }

    // Regular key,value pairs
    const IniFile::Section::SectionMap& section_map = section.GetValues();

    for (const auto& value : section_map)
    {
      const auto mapped_config = MapINIToRealLocation(section_name, value.first);

      if (mapped_config.section.empty() && mapped_config.key.empty())
        continue;

      auto* config_section =
          config_layer->GetOrCreateSection(mapped_config.system, mapped_config.section);
      config_section->Set(mapped_config.key, value.second);
    }
  }

  const std::string m_id;
  const u16 m_revision;
};

void INIGameConfigLayerLoader::Save(Config::Layer* config_layer)
{
  if (config_layer->GetLayer() != Config::LayerType::LocalGame)
    return;

  IniFile ini;
  for (const std::string& file_name : GetGameIniFilenames(m_id, m_revision))
    ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + file_name, true);

  for (const auto& system : config_layer->GetLayerMap())
  {
    for (const auto& section : system.second)
    {
      for (const auto& value : section.GetValues())
      {
        const auto ini_location =
            GetINILocationFromConfig({system.first, section.GetName(), value.first});
        if (ini_location.first.empty() && ini_location.second.empty())
          continue;

        IniFile::Section* ini_section = ini.GetOrCreateSection(ini_location.first);
        ini_section->Set(ini_location.second, value.second);
      }
    }
  }

  // Try to save to the revision specific INI first, if it exists.
  const std::string gameini_with_rev =
      File::GetUserPath(D_GAMESETTINGS_IDX) + m_id + StringFromFormat("r%d", m_revision) + ".ini";
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
}
