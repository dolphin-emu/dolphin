// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Config.h"
#include "Common/IniFile.h"

#include "DolphinWX/ControllerProfileLoader.h"

class INIProfileConfigLoader : public Config::ConfigLayerLoader
{
public:
  INIProfileConfigLoader(const std::string& filename)
      : ConfigLayerLoader(Config::LayerType::Base), m_filename(filename)
  {
  }

  void Load(Config::Layer* config_layer) override
  {
    IniFile ini;
    ini.Load(m_filename);
    const std::list<IniFile::Section>& system_sections = ini.GetSections();

    // This will only end up being one section
    // Just throw everything in to Main
    // This class can't be used within the global Config namespace anyway
    for (auto section : system_sections)
    {
      const std::string section_name = section.GetName();
      auto* config_section = config_layer->GetOrCreateSection(Config::System::Main, section_name);
      const IniFile::Section::SectionMap& section_map = section.GetValues();

      for (auto& value : section_map)
        config_section->Set(value.first, value.second);
    }
  }

  void Save(Config::Layer* config_layer) override
  {
    IniFile ini;
    const Config::LayerMap& sections = config_layer->GetLayerMap();
    // We will only ever have Main here
    for (const auto& system : sections)
    {
      for (const auto& section : system.second)
      {
        const std::string section_name = section->GetName();
        const Config::SectionValueMap& section_values = section->GetValues();

        IniFile::Section* ini_section = ini.GetOrCreateSection(section_name);

        for (auto& value : section_values)
          ini_section->Set(value.first, value.second);
      }
    }

    ini.Save(m_filename);
  }

private:
  const std::string m_filename;
};

// Loader generation
Config::ConfigLayerLoader* GenerateProfileConfigLoader(const std::string& filename)
{
  return new INIProfileConfigLoader(filename);
}
