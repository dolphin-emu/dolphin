// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/ConfigLoaders/BaseConfigLoader.h"

#include <algorithm>
#include <cstring>
#include <functional>
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

#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/IsSettingSaveable.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/USB/Bluetooth/BTBase.h"
#include "Core/SysConf.h"
#include "Core/System.h"

namespace ConfigLoaders
{
void SaveToSYSCONF(Config::LayerType layer, std::function<bool(const Config::Location&)> predicate)
{
  if (Core::IsRunning(Core::System::GetInstance()))
    return;

  IOS::HLE::Kernel ios;
  SysConf sysconf{ios.GetFS()};

  for (const Config::SYSCONFSetting& setting : Config::SYSCONF_SETTINGS)
  {
    std::visit(
        [&](auto* info) {
          if (predicate && !predicate(info->GetLocation()))
            return;

          const std::string key = info->GetLocation().section + "." + info->GetLocation().key;

          if (setting.type == SysConf::Entry::Type::Long)
          {
            sysconf.SetData<u32>(key, setting.type, Config::Get(layer, *info));
          }
          else if (setting.type == SysConf::Entry::Type::Byte)
          {
            sysconf.SetData<u8>(key, setting.type, static_cast<u8>(Config::Get(layer, *info)));
          }
          else if (setting.type == SysConf::Entry::Type::BigArray)
          {
            // Somewhat hacky support for IPL.SADR. The setting only stores the
            // first 4 bytes even thought the SYSCONF entry is much bigger.
            SysConf::Entry* entry = sysconf.GetOrAddEntry(key, setting.type);
            if (entry->bytes.size() < 0x1007 + 1)
              entry->bytes.resize(0x1007 + 1);
            *reinterpret_cast<u32*>(entry->bytes.data()) = Config::Get(layer, *info);
          }
        },
        setting.config_info);
  }

  sysconf.SetData<u32>("IPL.CB", SysConf::Entry::Type::Long, 0);
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
    {Config::System::DualShockUDPClient, F_DUALSHOCKUDPCLIENTCONFIG_IDX},
    {Config::System::FreeLook, F_FREELOOKCONFIG_IDX},
    {Config::System::Achievements, F_RETROACHIEVEMENTSCONFIG_IDX},
    // Config::System::Session should not be added to this list
};

// INI layer configuration loader
class BaseConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  BaseConfigLayerLoader() : ConfigLayerLoader(Config::LayerType::Base) {}
  void Load(Config::Layer* layer) override
  {
    // List of settings that under no circumstances should be loaded from the global config INI.
    static constexpr auto s_setting_disallowed = {
        &Config::MAIN_MEMORY_CARD_SIZE.GetLocation(),
    };

    LoadFromSYSCONF(layer);
    for (const auto& system : system_to_ini)
    {
      Common::IniFile ini;
      ini.Load(File::GetUserPath(system.second));
      const auto& system_sections = ini.GetSections();

      for (const auto& section : system_sections)
      {
        const std::string section_name = section.GetName();
        const auto& section_map = section.GetValues();

        for (const auto& value : section_map)
        {
          const Config::Location location{system.first, section_name, value.first};
          const bool load_disallowed =
              std::any_of(begin(s_setting_disallowed), end(s_setting_disallowed),
                          [&location](const Config::Location* l) { return *l == location; });
          if (load_disallowed)
            continue;

          layer->Set(location, value.second);
        }
      }
    }
  }

  void Save(Config::Layer* layer) override
  {
    SaveToSYSCONF(layer->GetLayer());

    std::map<Config::System, Common::IniFile> inis;

    for (const auto& system : system_to_ini)
    {
      inis[system.first].Load(File::GetUserPath(system.second));
    }

    for (const auto& config : layer->GetLayerMap())
    {
      const Config::Location& location = config.first;
      const std::optional<std::string>& value = config.second;

      // Done by SaveToSYSCONF
      if (location.system == Config::System::SYSCONF)
        continue;

      if (location.system == Config::System::Session)
        continue;

      if (location.system == Config::System::GameSettingsOnly)
        continue;

      auto ini = inis.find(location.system);
      if (ini == inis.end())
      {
        ERROR_LOG_FMT(COMMON, "Config can't map system '{}' to an INI file!",
                      Config::GetSystemName(location.system));
        continue;
      }

      if (!IsSettingSaveable(location))
        continue;

      if (value)
      {
        auto* ini_section = ini->second.GetOrCreateSection(location.section);
        ini_section->Set(location.key, *value);
      }
      else
      {
        ini->second.DeleteKey(location.section, location.key);
      }
    }

    for (const auto& system : system_to_ini)
    {
      inis[system.first].Save(File::GetUserPath(system.second));
    }
  }

private:
  void LoadFromSYSCONF(Config::Layer* layer)
  {
    if (Core::IsRunning(Core::System::GetInstance()))
      return;

    IOS::HLE::Kernel ios;
    SysConf sysconf{ios.GetFS()};
    for (const Config::SYSCONFSetting& setting : Config::SYSCONF_SETTINGS)
    {
      std::visit(
          [&](auto* info) {
            const Config::Location location = info->GetLocation();
            const std::string key = location.section + "." + location.key;
            if (setting.type == SysConf::Entry::Type::Long)
            {
              layer->Set(location, sysconf.GetData<u32>(key, info->GetDefaultValue()));
            }
            else if (setting.type == SysConf::Entry::Type::Byte)
            {
              layer->Set(location, sysconf.GetData<u8>(key, info->GetDefaultValue()));
            }
            else if (setting.type == SysConf::Entry::Type::BigArray)
            {
              // Somewhat hacky support for IPL.SADR. The setting only stores the
              // first 4 bytes even thought the SYSCONF entry is much bigger.
              u32 value = info->GetDefaultValue();
              SysConf::Entry* entry = sysconf.GetEntry(key);
              if (entry)
              {
                std::memcpy(&value, entry->bytes.data(),
                            std::min(entry->bytes.size(), sizeof(u32)));
              }
              layer->Set(location, value);
            }
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
}  // namespace ConfigLoaders
