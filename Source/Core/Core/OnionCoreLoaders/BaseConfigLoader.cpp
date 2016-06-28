// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/OnionConfig.h"

#include "Core/OnionCoreLoaders/BaseConfigLoader.h"

const std::map<OnionConfig::System, int> system_to_ini = {
    {OnionConfig::System::SYSTEM_MAIN, F_DOLPHINCONFIG_IDX},
    {OnionConfig::System::SYSTEM_GCPAD, F_GCPADCONFIG_IDX},
    {OnionConfig::System::SYSTEM_WIIPAD, F_WIIPADCONFIG_IDX},
    {OnionConfig::System::SYSTEM_GCKEYBOARD, F_GCKEYBOARDCONFIG_IDX},
    {OnionConfig::System::SYSTEM_GFX, F_GFXCONFIG_IDX},
    {OnionConfig::System::SYSTEM_LOGGER, F_LOGGERCONFIG_IDX},
    {OnionConfig::System::SYSTEM_DEBUGGER, F_DEBUGGERCONFIG_IDX},
    {OnionConfig::System::SYSTEM_UI, F_UICONFIG_IDX},
};

// INI layer configuration loader
class INIBaseConfigLayerLoader : public OnionConfig::ConfigLayerLoader
{
public:
  INIBaseConfigLayerLoader() : ConfigLayerLoader(OnionConfig::LayerType::LAYER_BASE) {}
  void Load(OnionConfig::Layer* config_layer) override
  {
    for (const auto& system : system_to_ini)
    {
      IniFile ini;
      ini.Load(File::GetUserPath(system.second));
      const std::list<IniFile::Section>& system_sections = ini.GetSections();

      for (auto section : system_sections)
      {
        const std::string section_name = section.GetName();
        OnionConfig::Section* petal = config_layer->GetOrCreateSection(system.first, section_name);
        const IniFile::Section::SectionMap& section_map = section.GetValues();

        for (auto& value : section_map)
          petal->Set(value.first, value.second);
      }
    }
  }

  void Save(OnionConfig::Layer* config_layer) override
  {
    const OnionConfig::LayerMap& petals = config_layer->GetLayerMap();
    for (const auto& system : petals)
    {
      auto mapping = system_to_ini.find(system.first);
      if (mapping == system_to_ini.end())
      {
        ERROR_LOG(COMMON, "OnionConfig can't map system '%s' to an INI file!",
                  OnionConfig::GetSystemName(system.first).c_str());
        continue;
      }

      IniFile ini;
      ini.Load(File::GetUserPath(mapping->second));

      for (const auto& petal : system.second)
      {
        const std::string petal_name = petal->GetName();
        const OnionConfig::SectionValueMap& petal_values = petal->GetValues();

        IniFile::Section* section = ini.GetOrCreateSection(petal_name);

        for (auto& value : petal_values)
          section->Set(value.first, value.second);
      }

      ini.Save(File::GetUserPath(mapping->second));
    }
  }
};

// Loader generation
OnionConfig::ConfigLayerLoader* GenerateBaseConfigLoader()
{
  return new INIBaseConfigLayerLoader();
}
