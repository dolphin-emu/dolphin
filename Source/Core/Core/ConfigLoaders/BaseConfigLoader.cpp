// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigLoaders/BaseConfigLoader.h"

const std::map<Config::System, int> system_to_ini = {
    {Config::System::Main, F_DOLPHINCONFIG_IDX},
    {Config::System::GCPad, F_GCPADCONFIG_IDX},
    {Config::System::WiiPad, F_WIIPADCONFIG_IDX},
    {Config::System::GCKeyboard, F_GCKEYBOARDCONFIG_IDX},
    {Config::System::GFX, F_GFXCONFIG_IDX},
    {Config::System::Logger, F_LOGGERCONFIG_IDX},
    {Config::System::Debugger, F_DEBUGGERCONFIG_IDX},
    {Config::System::UI, F_UICONFIG_IDX},
};

// INI layer configuration loader
class INIBaseConfigLayerLoader : public Config::ConfigLayerLoader
{
public:
  INIBaseConfigLayerLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer* config_layer) override
  {
    for (const auto& system : system_to_ini)
    {
      IniFile ini;
      ini.Load(File::GetUserPath(system.second));
      const std::list<IniFile::Section>& system_sections = ini.GetSections();

      for (auto section : system_sections)
      {
        const std::string section_name = section.GetName();
        Config::Section* config_section =
            config_layer->GetOrCreateSection(system.first, section_name);
        const IniFile::Section::SectionMap& section_map = section.GetValues();

        for (auto& value : section_map)
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
      ini.Load(File::GetUserPath(mapping->second));

      for (const auto& section : system.second)
      {
        const std::string section_name = section->GetName();
        const Config::SectionValueMap& section_values = section->GetValues();

        IniFile::Section* ini_section = ini.GetOrCreateSection(section_name);

        for (auto& value : section_values)
          ini_section->Set(value.first, value.second);
      }

      ini.Save(File::GetUserPath(mapping->second));
    }
  }
};

// Loader generation
Config::ConfigLayerLoader* GenerateBaseConfigLoader()
{
  return new INIBaseConfigLayerLoader();
}
