// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <list>
#include <map>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigLoaders/BaseConfigLoader.h"

namespace ConfigLoaders
{
const std::map<Config::System, const std::string& (*)()> system_to_ini = {
    {Config::System::Main, Paths::GetDolphinConfigFile},
    {Config::System::GCPad, Paths::GetGCPadConfigFile},
    {Config::System::WiiPad, Paths::GetWiiPadConfigFile},
    {Config::System::GCKeyboard, Paths::GetGCKeyboardConfigFile},
    {Config::System::GFX, Paths::GetGFXConfigFile},
    {Config::System::Logger, Paths::GetLoggerConfigFile},
    {Config::System::Debugger, Paths::GetDebuggerConfigFile},
    {Config::System::UI, Paths::GetUIConfigFile},
};

// INI layer configuration loader
class INIBaseConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  INIBaseConfigLayerLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer* config_layer) override
  {
    for (const auto& system : system_to_ini)
    {
      IniFile ini;
      ini.Load(system.second());
      const std::list<IniFile::Section>& system_sections = ini.GetSections();

      for (const auto& section : system_sections)
      {
        const std::string section_name = section.GetName();
        Config::Section* config_section =
            config_layer->GetOrCreateSection(system.first, section_name);
        const IniFile::Section::SectionMap& section_map = section.GetValues();

        for (const auto& value : section_map)
          config_section->Set(value.first, value.second);
      }
    }
  }

  void Save(Config::Layer* config_layer) override
  {
    const Config::LayerMap& sections = config_layer->GetLayerMap();
    for (const auto& system : sections)
    {
      auto mapping = system_to_ini.find(system.first);
      if (mapping == system_to_ini.end())
      {
        ERROR_LOG(COMMON, "Config can't map system '%s' to an INI file!",
                  Config::GetSystemName(system.first).c_str());
        continue;
      }

      IniFile ini;
      ini.Load(mapping->second());

      for (const auto& section : system.second)
      {
        const std::string section_name = section.GetName();
        const Config::SectionValueMap& section_values = section.GetValues();

        IniFile::Section* ini_section = ini.GetOrCreateSection(section_name);

        for (const auto& value : section_values)
          ini_section->Set(value.first, value.second);
      }

      ini.Save(mapping->second());
    }
  }
};

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader> GenerateBaseConfigLoader()
{
  return std::make_unique<INIBaseConfigLayerLoader>();
}
}
