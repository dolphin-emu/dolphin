// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/BaseConfigLoader.h"

#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/SysConf.h"

#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/IsSettingSaveable.h"
#include "Core/Core.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"

namespace ConfigLoaders
{
void SaveToSYSCONF(Config::Layer* layer)
{
  if (Core::IsRunning())
    return;

  SysConf sysconf{Common::FromWhichRoot::FROM_CONFIGURED_ROOT};

  for (const Config::SYSCONFSetting& setting : Config::SYSCONF_SETTINGS)
  {
    std::visit(
        [layer, &setting, &sysconf](auto& info) {
          const std::string key = info.location.section + "." + info.location.key;

          if (setting.type == SysConf::Entry::Type::Long)
            sysconf.SetData<u32>(key, setting.type, layer->Get(info));
          else if (setting.type == SysConf::Entry::Type::Byte)
            sysconf.SetData<u8>(key, setting.type, static_cast<u8>(layer->Get(info)));
        },
        setting.config_info);
  }

  // Disable WiiConnect24's standby mode. If it is enabled, it prevents us from receiving
  // shutdown commands in the State Transition Manager (STM).
  // TODO: remove this if and once Dolphin supports WC24 standby mode.
  SysConf::Entry* idle_entry = sysconf.GetOrAddEntry("IPL.IDL", SysConf::Entry::Type::SmallArray);
  if (idle_entry->bytes.empty())
    idle_entry->bytes = std::vector<u8>(2);
  else
    idle_entry->bytes[0] = 0;
  NOTICE_LOG(CORE, "Disabling WC24 'standby' (shutdown to idle) to avoid hanging on shutdown");

  IOS::HLE::RestoreBTInfoSection(&sysconf);
  sysconf.Save();
}

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
class BaseConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  BaseConfigLayerLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer* config_layer) override
  {
    LoadFromSYSCONF(config_layer);
    for (const auto& system : system_to_ini)
    {
      IniFile ini;
      ini.Load(File::GetUserPath(system.second));
      const std::list<IniFile::Section>& system_sections = ini.GetSections();

      for (const auto& section : system_sections)
      {
        const std::string section_name = section.GetName();
        Config::Section* config_section =
            config_layer->GetOrCreateSection(system.first, section_name);
        const IniFile::Section::SectionMap& section_map = section.GetValues();

        for (const auto& value : section_map)
        {
          const Config::ConfigLocation location{system.first, section_name, value.first};
          if (location == Config::GFX_EFB_SCALE.location)
          {
            std::optional<int> efb_scale = Config::ConvertFromLegacyEFBScale(value.second);
            if (efb_scale)
              config_section->Set(value.first, *efb_scale);
          }
          else
          {
            config_section->Set(value.first, value.second);
          }
        }
      }
    }
  }

  void Save(Config::Layer* config_layer) override
  {
    const Config::LayerMap& sections = config_layer->GetLayerMap();
    for (const auto& system : sections)
    {
      if (system.first == Config::System::SYSCONF)
      {
        SaveToSYSCONF(config_layer);
        continue;
      }

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

        for (const auto& value : section_values)
        {
          const Config::ConfigLocation location{system.first, section->GetName(), value.first};
          if (!IsSettingSaveable(location))
            continue;

          if (location == Config::GFX_EFB_SCALE.location)
          {
            std::optional<int> efb_scale = Config::ConvertToLegacyEFBScale(value.second);
            if (efb_scale)
              ini_section->Set(value.first, *efb_scale);
          }
          else
          {
            ini_section->Set(value.first, value.second);
          }
        }
      }

      ini.Save(File::GetUserPath(mapping->second));
    }
  }

private:
  void LoadFromSYSCONF(Config::Layer* layer)
  {
    if (Core::IsRunning())
      return;

    SysConf sysconf{Common::FromWhichRoot::FROM_CONFIGURED_ROOT};
    for (const Config::SYSCONFSetting& setting : Config::SYSCONF_SETTINGS)
    {
      std::visit(
          [&](auto& info) {
            const std::string key = info.location.section + "." + info.location.key;
            auto* section =
                layer->GetOrCreateSection(Config::System::SYSCONF, info.location.section);

            if (setting.type == SysConf::Entry::Type::Long)
              section->Set(info.location.key, sysconf.GetData<u32>(key, info.default_value));
            else if (setting.type == SysConf::Entry::Type::Byte)
              section->Set(info.location.key, sysconf.GetData<u8>(key, info.default_value));
          },
          setting.config_info);
    }
  }
};

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader> GenerateBaseConfigLoader()
{
  return std::make_unique<BaseConfigLayerLoader>();
}
}
