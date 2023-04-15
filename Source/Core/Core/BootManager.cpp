// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// File description
// -------------
// Purpose of this file: Collect boot settings for Core::Init()

// Call sequence: This file has one of the first function called when a game is booted,
// the boot sequence in the code is:

// DolphinWX:    FrameTools.cpp         StartGame
// Core          BootManager.cpp        BootCore
//               Core.cpp               Init                     Thread creation
//                                      EmuThread                Calls CBoot::BootUp
//               Boot.cpp               CBoot::BootUp()
//                                      CBoot::EmulatedBS2_Wii() / GC() or Load_BS2()

#include "Core/BootManager.h"

#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"

#include "Core/AchievementManager.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigLoaders/NetPlayConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

#include "DiscIO/Enums.h"

#include "VideoCommon/VideoBackendBase.h"

namespace BootManager
{
// Boot the ISO or file
bool BootCore(std::unique_ptr<BootParameters> boot, const WindowSystemInfo& wsi)
{
  if (!boot)
    return false;

  SConfig& StartUp = SConfig::GetInstance();

  if (!StartUp.SetPathsAndGameMetadata(*boot))
    return false;

  // Movie settings
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
  {
    for (ExpansionInterface::Slot slot : ExpansionInterface::MEMCARD_SLOTS)
    {
      if (Movie::IsUsingMemcard(slot) && Movie::IsStartingFromClearSave() && !StartUp.bWii)
      {
        const auto raw_path =
            File::GetUserPath(D_GCUSER_IDX) +
            fmt::format("Movie{}.raw", slot == ExpansionInterface::Slot::A ? 'A' : 'B');
        if (File::Exists(raw_path))
          File::Delete(raw_path);

        const auto movie_path = File::GetUserPath(D_GCUSER_IDX) + "Movie";
        if (File::Exists(movie_path))
          File::DeleteDirRecursively(movie_path);
      }
    }
  }

  if (NetPlay::IsNetPlayRunning())
  {
    const NetPlay::NetSettings* netplay_settings = boot->boot_session_data.GetNetplaySettings();
    if (!netplay_settings)
      return false;

    Config::AddLayer(ConfigLoaders::GenerateNetPlayConfigLoader(*netplay_settings));
    StartUp.bCopyWiiSaveNetplay = netplay_settings->savedata_load;
  }

  // Override out-of-region languages/countries to prevent games from crashing or behaving oddly
  if (!Config::Get(Config::MAIN_OVERRIDE_REGION_SETTINGS))
  {
    Config::SetCurrent(
        Config::MAIN_GC_LANGUAGE,
        DiscIO::ToGameCubeLanguage(StartUp.GetLanguageAdjustedForRegion(false, StartUp.m_region)));

    if (StartUp.bWii)
    {
      const u32 wii_language =
          static_cast<u32>(StartUp.GetLanguageAdjustedForRegion(true, StartUp.m_region));
      if (wii_language != Config::Get(Config::SYSCONF_LANGUAGE))
        Config::SetCurrent(Config::SYSCONF_LANGUAGE, wii_language);

      const u8 country_code = static_cast<u8>(Config::Get(Config::SYSCONF_COUNTRY));
      if (StartUp.m_region != DiscIO::SysConfCountryToRegion(country_code))
      {
        switch (StartUp.m_region)
        {
        case DiscIO::Region::NTSC_J:
          Config::SetCurrent(Config::SYSCONF_COUNTRY, 0x01);  // Japan
          break;
        case DiscIO::Region::NTSC_U:
          Config::SetCurrent(Config::SYSCONF_COUNTRY, 0x31);  // United States
          break;
        case DiscIO::Region::PAL:
          Config::SetCurrent(Config::SYSCONF_COUNTRY, 0x6c);  // Switzerland
          break;
        case DiscIO::Region::NTSC_K:
          Config::SetCurrent(Config::SYSCONF_COUNTRY, 0x88);  // South Korea
          break;
        case DiscIO::Region::Unknown:
          break;
        }
      }
    }
  }

  // Some NTSC Wii games such as Doc Louis's Punch-Out!! and
  // 1942 (Virtual Console) crash if the PAL60 option is enabled
  if (StartUp.bWii && DiscIO::IsNTSC(StartUp.m_region) && Config::Get(Config::SYSCONF_PAL60))
    Config::SetCurrent(Config::SYSCONF_PAL60, false);

  // Disable loading time emulation for Riivolution-patched games until we have proper emulation.
  if (!boot->riivolution_patches.empty())
    Config::SetCurrent(Config::MAIN_FAST_DISC_SPEED, true);

  Core::System::GetInstance().Initialize();

  Core::UpdateWantDeterminism(/*initial*/ true);

  if (StartUp.bWii)
  {
    Core::InitializeWiiRoot(Core::WantsDeterminism());

    // Ensure any new settings are written to the SYSCONF
    if (!Core::WantsDeterminism())
    {
      Core::BackupWiiSettings();
      ConfigLoaders::SaveToSYSCONF(Config::LayerType::Meta);
    }
    else
    {
      ConfigLoaders::SaveToSYSCONF(Config::LayerType::Meta, [](const Config::Location& location) {
        return Config::GetActiveLayerForConfig(location) >= Config::LayerType::Movie;
      });
    }
  }

#ifdef USE_RETRO_ACHIEVEMENTS
  std::string path = "";
  if (std::holds_alternative<BootParameters::Disc>(boot->parameters))
  {
    path = std::get<BootParameters::Disc>(boot->parameters).path;
  }
  AchievementManager::GetInstance()->LoadGameByFilenameAsync(
      path, [](AchievementManager::ResponseType r_type) {});
#endif  // USE_RETRO_ACHIEVEMENTS

  const bool load_ipl = !StartUp.bWii && !Config::Get(Config::MAIN_SKIP_IPL) &&
                        std::holds_alternative<BootParameters::Disc>(boot->parameters);
  if (load_ipl)
  {
    return Core::Init(
        std::make_unique<BootParameters>(
            BootParameters::IPL{StartUp.m_region,
                                std::move(std::get<BootParameters::Disc>(boot->parameters))},
            std::move(boot->boot_session_data)),
        wsi);
  }
  return Core::Init(std::move(boot), wsi);
}

// SYSCONF can be modified during emulation by the user and internally, which makes it
// a bad idea to just always overwrite it with the settings from the base layer.
//
// Conversely, we also shouldn't just accept any changes to SYSCONF, as it may cause
// temporary settings (from Movie, Netplay, game INIs, etc.) to stick around.
//
// To avoid inconveniences in most cases, we accept changes that aren't being overriden by a
// non-base layer, and restore only the overriden settings.
static void RestoreSYSCONF()
{
  // This layer contains the new SYSCONF settings (including any temporary settings).
  Config::Layer temp_layer(Config::LayerType::Base);
  // Use a separate loader so the temp layer doesn't automatically save
  ConfigLoaders::GenerateBaseConfigLoader()->Load(&temp_layer);

  for (const auto& setting : Config::SYSCONF_SETTINGS)
  {
    std::visit(
        [&](auto* info) {
          // If this setting was overridden, then we copy the base layer value back to the SYSCONF.
          // Otherwise we leave the new value in the SYSCONF.
          if (Config::GetActiveLayerForConfig(*info) == Config::LayerType::Base)
            Config::SetBase(*info, temp_layer.Get(*info));
        },
        setting.config_info);
  }
  // Save the SYSCONF.
  Config::GetLayer(Config::LayerType::Base)->Save();
}

void RestoreConfig()
{
  Core::ShutdownWiiRoot();

  if (!Core::WiiRootIsTemporary())
  {
    Core::RestoreWiiSettings(Core::RestoreReason::EmulationEnd);
    RestoreSYSCONF();
  }

  Config::ClearCurrentRunLayer();
  Config::RemoveLayer(Config::LayerType::Movie);
  Config::RemoveLayer(Config::LayerType::Netplay);
  Config::RemoveLayer(Config::LayerType::GlobalGame);
  Config::RemoveLayer(Config::LayerType::LocalGame);
  SConfig::GetInstance().ResetRunningGameMetadata();
}

}  // namespace BootManager
