// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Common/Config/Config.h"
#include "Core/Boot/Boot.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigLoaders/GameConfigLoader.h"
#include "Core/ConfigLoaders/MovieConfigLoader.h"
#include "Core/ConfigLoaders/NetPlayConfigLoader.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"

#include "DiscIO/Enums.h"

namespace BootManager
{
// TODO this is an ugly hack which allows us to restore values trampled by per-game settings
// Apply fire liberally
struct ConfigCache
{
public:
  // fill the cache with values from the configuration
  void SaveConfig(const SConfig& config);
  // restore values to the configuration from the cache
  void RestoreConfig(SConfig* config);

  // These store if the relevant setting should be reset back later (true) or if it should be left
  // alone on restore (false)
  std::array<bool, MAX_BBMOTES> bSetWiimoteSource;

private:
  bool valid;
  std::array<int, MAX_BBMOTES> iWiimoteSource;
};

void ConfigCache::SaveConfig(const SConfig& config)
{
  valid = true;
  std::copy(std::begin(g_wiimote_sources), std::end(g_wiimote_sources), std::begin(iWiimoteSource));
  bSetWiimoteSource.fill(false);
}

void ConfigCache::RestoreConfig(SConfig* config)
{
  if (!valid)
    return;

  valid = false;

  if (config->bWii)
  {
    for (unsigned int i = 0; i < MAX_BBMOTES; ++i)
    {
      if (bSetWiimoteSource[i])
      {
        g_wiimote_sources[i] = iWiimoteSource[i];
        WiimoteReal::ChangeWiimoteSource(i, iWiimoteSource[i]);
      }
    }
  }
}

static ConfigCache config_cache;

// Boot the ISO or file
bool BootCore(std::unique_ptr<BootParameters> boot)
{
  if (!boot)
    return false;

  SConfig& StartUp = SConfig::GetInstance();

  StartUp.bRunCompareClient = false;
  StartUp.bRunCompareServer = false;

  config_cache.SaveConfig(StartUp);

  if (!StartUp.SetPathsAndGameMetadata(*boot))
    return false;

  // Load game specific settings
  if (!std::holds_alternative<BootParameters::IPL>(boot->parameters))
  {
    IniFile game_ini = StartUp.LoadGameIni();

    // General settings
    IniFile::Section* controls_section = game_ini.GetOrCreateSection("Controls");

    // Wii settings
    if (StartUp.bWii)
    {
      int source;
      for (unsigned int i = 0; i < MAX_WIIMOTES; ++i)
      {
        controls_section->Get(StringFromFormat("WiimoteSource%u", i), &source, -1);
        if (source != -1 && g_wiimote_sources[i] != (unsigned)source &&
            source >= WIIMOTE_SRC_NONE && source <= WIIMOTE_SRC_HYBRID)
        {
          config_cache.bSetWiimoteSource[i] = true;
          g_wiimote_sources[i] = source;
          WiimoteReal::ChangeWiimoteSource(i, source);
        }
      }
      controls_section->Get("WiimoteSourceBB", &source, -1);
      if (source != -1 && g_wiimote_sources[WIIMOTE_BALANCE_BOARD] != (unsigned)source &&
          (source == WIIMOTE_SRC_NONE || source == WIIMOTE_SRC_REAL))
      {
        config_cache.bSetWiimoteSource[WIIMOTE_BALANCE_BOARD] = true;
        g_wiimote_sources[WIIMOTE_BALANCE_BOARD] = source;
        WiimoteReal::ChangeWiimoteSource(WIIMOTE_BALANCE_BOARD, source);
      }
    }
  }

  // Movie settings
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
  {
    for (int i = 0; i < 2; ++i)
    {
      if (Movie::IsUsingMemcard(i) && Movie::IsStartingFromClearSave() && !StartUp.bWii)
      {
        if (File::Exists(File::GetUserPath(D_GCUSER_IDX) +
                         StringFromFormat("Movie%s.raw", (i == 0) ? "A" : "B")))
          File::Delete(File::GetUserPath(D_GCUSER_IDX) +
                       StringFromFormat("Movie%s.raw", (i == 0) ? "A" : "B"));
        if (File::Exists(File::GetUserPath(D_GCUSER_IDX) + "Movie"))
          File::DeleteDirRecursively(File::GetUserPath(D_GCUSER_IDX) + "Movie");
      }
    }
  }

  if (NetPlay::IsNetPlayRunning())
    Config::AddLayer(ConfigLoaders::GenerateNetPlayConfigLoader(g_NetPlaySettings));
  else
    g_SRAM_netplay_initialized = false;

  const bool ntsc = DiscIO::IsNTSC(StartUp.m_region);

  // Apply overrides
  // Some NTSC GameCube games such as Baten Kaitos react strangely to
  // language settings that would be invalid on an NTSC system
  if (!StartUp.bOverrideGCLanguage && ntsc)
  {
    StartUp.SelectedLanguage = 0;
  }

  // Some NTSC Wii games such as Doc Louis's Punch-Out!! and
  // 1942 (Virtual Console) crash if the PAL60 option is enabled
  if (StartUp.bWii && ntsc)
    Config::SetCurrent(Config::SYSCONF_PAL60, false);

  // Ensure any new settings are written to the SYSCONF
  if (StartUp.bWii)
    ConfigLoaders::SaveToSYSCONF(Config::LayerType::Meta);

  const bool load_ipl = !StartUp.bWii && !Config::Get(Config::MAIN_SKIP_IPL) &&
                        std::holds_alternative<BootParameters::Disc>(boot->parameters);
  if (load_ipl)
  {
    return Core::Init(std::make_unique<BootParameters>(BootParameters::IPL{
        StartUp.m_region, std::move(std::get<BootParameters::Disc>(boot->parameters))}));
  }
  return Core::Init(std::move(boot));
}

void Stop()
{
  Core::Stop();
  RestoreConfig();
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
        [&](auto& info) {
          // If this setting was overridden, then we copy the base layer value back to the SYSCONF.
          // Otherwise we leave the new value in the SYSCONF.
          if (Config::GetActiveLayerForConfig(info) == Config::LayerType::Base)
            Config::SetBase(info, temp_layer.Get(info));
        },
        setting.config_info);
  }
  // Save the SYSCONF.
  Config::GetLayer(Config::LayerType::Base)->Save();
}

void RestoreConfig()
{
  RestoreSYSCONF();
  Config::ClearCurrentRunLayer();
  Config::RemoveLayer(Config::LayerType::Movie);
  Config::RemoveLayer(Config::LayerType::Netplay);
  Config::RemoveLayer(Config::LayerType::GlobalGame);
  Config::RemoveLayer(Config::LayerType::LocalGame);
  SConfig::GetInstance().ResetRunningGameMetadata();
  config_cache.RestoreConfig(&SConfig::GetInstance());
}

}  // namespace
