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

#include "VideoCommon/VideoBackendBase.h"

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
  bool bSetEmulationSpeed;
  bool bSetVolume;
  std::array<bool, MAX_BBMOTES> bSetWiimoteSource;
  std::array<bool, SerialInterface::MAX_SI_CHANNELS> bSetPads;
  std::array<bool, ExpansionInterface::MAX_EXI_CHANNELS> bSetEXIDevice;

private:
  bool valid;
  bool bCPUThread;
  bool bEnableCheats;
  bool bSyncGPUOnSkipIdleHack;
  bool bFPRF;
  bool bAccurateNaNs;
  bool bMMU;
  bool bDCBZOFF;
  bool bLowDCBZHack;
  bool m_EnableJIT;
  bool bSyncGPU;
  bool bFastDiscSpeed;
  bool bDSPHLE;
  bool bHLE_BS2;
  int iSelectedLanguage;
  int iCPUCore;
  int Volume;
  float m_EmulationSpeed;
  float m_OCFactor;
  bool m_OCEnable;
  std::string strBackend;
  std::string sBackend;
  std::string m_strGPUDeterminismMode;
  std::array<int, MAX_BBMOTES> iWiimoteSource;
  std::array<SerialInterface::SIDevices, SerialInterface::MAX_SI_CHANNELS> Pads;
  std::array<ExpansionInterface::TEXIDevices, ExpansionInterface::MAX_EXI_CHANNELS> m_EXIDevice;
};

void ConfigCache::SaveConfig(const SConfig& config)
{
  valid = true;

  bCPUThread = config.bCPUThread;
  bEnableCheats = config.bEnableCheats;
  bSyncGPUOnSkipIdleHack = config.bSyncGPUOnSkipIdleHack;
  bFPRF = config.bFPRF;
  bAccurateNaNs = config.bAccurateNaNs;
  bMMU = config.bMMU;
  bDCBZOFF = config.bDCBZOFF;
  m_EnableJIT = config.m_DSPEnableJIT;
  bSyncGPU = config.bSyncGPU;
  bFastDiscSpeed = config.bFastDiscSpeed;
  bDSPHLE = config.bDSPHLE;
  bHLE_BS2 = config.bHLE_BS2;
  iSelectedLanguage = config.SelectedLanguage;
  iCPUCore = config.iCPUCore;
  Volume = config.m_Volume;
  m_EmulationSpeed = config.m_EmulationSpeed;
  strBackend = config.m_strVideoBackend;
  sBackend = config.sBackend;
  m_strGPUDeterminismMode = config.m_strGPUDeterminismMode;
  m_OCFactor = config.m_OCFactor;
  m_OCEnable = config.m_OCEnable;

  std::copy(std::begin(g_wiimote_sources), std::end(g_wiimote_sources), std::begin(iWiimoteSource));
  std::copy(std::begin(config.m_SIDevice), std::end(config.m_SIDevice), std::begin(Pads));
  std::copy(std::begin(config.m_EXIDevice), std::end(config.m_EXIDevice), std::begin(m_EXIDevice));

  bSetEmulationSpeed = false;
  bSetVolume = false;
  bSetWiimoteSource.fill(false);
  bSetPads.fill(false);
  bSetEXIDevice.fill(false);
}

void ConfigCache::RestoreConfig(SConfig* config)
{
  if (!valid)
    return;

  valid = false;

  config->bCPUThread = bCPUThread;
  config->bEnableCheats = bEnableCheats;
  config->bSyncGPUOnSkipIdleHack = bSyncGPUOnSkipIdleHack;
  config->bFPRF = bFPRF;
  config->bAccurateNaNs = bAccurateNaNs;
  config->bMMU = bMMU;
  config->bDCBZOFF = bDCBZOFF;
  config->bLowDCBZHack = bLowDCBZHack;
  config->m_DSPEnableJIT = m_EnableJIT;
  config->bSyncGPU = bSyncGPU;
  config->bFastDiscSpeed = bFastDiscSpeed;
  config->bDSPHLE = bDSPHLE;
  config->bHLE_BS2 = bHLE_BS2;
  config->SelectedLanguage = iSelectedLanguage;
  config->iCPUCore = iCPUCore;

  // Only change these back if they were actually set by game ini, since they can be changed while a
  // game is running.
  if (bSetVolume)
    config->m_Volume = Volume;

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

  for (unsigned int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
  {
    if (bSetPads[i])
      config->m_SIDevice[i] = Pads[i];
  }

  if (bSetEmulationSpeed)
    config->m_EmulationSpeed = m_EmulationSpeed;

  for (unsigned int i = 0; i < ExpansionInterface::MAX_EXI_CHANNELS; ++i)
  {
    if (bSetEXIDevice[i])
      config->m_EXIDevice[i] = m_EXIDevice[i];
  }

  config->m_strVideoBackend = strBackend;
  config->sBackend = sBackend;
  config->m_strGPUDeterminismMode = m_strGPUDeterminismMode;
  config->m_OCFactor = m_OCFactor;
  config->m_OCEnable = m_OCEnable;
  VideoBackendBase::ActivateBackend(config->m_strVideoBackend);
}

static ConfigCache config_cache;

static GPUDeterminismMode ParseGPUDeterminismMode(const std::string& mode)
{
  if (mode == "auto")
    return GPU_DETERMINISM_AUTO;
  if (mode == "none")
    return GPU_DETERMINISM_NONE;
  if (mode == "fake-completion")
    return GPU_DETERMINISM_FAKE_COMPLETION;

  NOTICE_LOG(BOOT, "Unknown GPU determinism mode %s", mode.c_str());
  return GPU_DETERMINISM_AUTO;
}

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
    IniFile::Section* core_section = game_ini.GetOrCreateSection("Core");
    IniFile::Section* dsp_section = game_ini.GetOrCreateSection("DSP");
    IniFile::Section* controls_section = game_ini.GetOrCreateSection("Controls");

    core_section->Get("CPUThread", &StartUp.bCPUThread, StartUp.bCPUThread);
    core_section->Get("EnableCheats", &StartUp.bEnableCheats, StartUp.bEnableCheats);
    core_section->Get("SyncOnSkipIdle", &StartUp.bSyncGPUOnSkipIdleHack,
                      StartUp.bSyncGPUOnSkipIdleHack);
    core_section->Get("FPRF", &StartUp.bFPRF, StartUp.bFPRF);
    core_section->Get("AccurateNaNs", &StartUp.bAccurateNaNs, StartUp.bAccurateNaNs);
    core_section->Get("MMU", &StartUp.bMMU, StartUp.bMMU);
    core_section->Get("DCBZ", &StartUp.bDCBZOFF, StartUp.bDCBZOFF);
    core_section->Get("LowDCBZHack", &StartUp.bLowDCBZHack, StartUp.bLowDCBZHack);
    core_section->Get("SyncGPU", &StartUp.bSyncGPU, StartUp.bSyncGPU);
    core_section->Get("FastDiscSpeed", &StartUp.bFastDiscSpeed, StartUp.bFastDiscSpeed);
    core_section->Get("DSPHLE", &StartUp.bDSPHLE, StartUp.bDSPHLE);
    core_section->Get("GFXBackend", &StartUp.m_strVideoBackend, StartUp.m_strVideoBackend);
    core_section->Get("CPUCore", &StartUp.iCPUCore, StartUp.iCPUCore);
    core_section->Get("HLE_BS2", &StartUp.bHLE_BS2, StartUp.bHLE_BS2);
    core_section->Get("GameCubeLanguage", &StartUp.SelectedLanguage, StartUp.SelectedLanguage);
    if (core_section->Get("EmulationSpeed", &StartUp.m_EmulationSpeed, StartUp.m_EmulationSpeed))
      config_cache.bSetEmulationSpeed = true;

    if (dsp_section->Get("Volume", &StartUp.m_Volume, StartUp.m_Volume))
      config_cache.bSetVolume = true;
    dsp_section->Get("EnableJIT", &StartUp.m_DSPEnableJIT, StartUp.m_DSPEnableJIT);
    dsp_section->Get("Backend", &StartUp.sBackend, StartUp.sBackend);
    VideoBackendBase::ActivateBackend(StartUp.m_strVideoBackend);
    core_section->Get("GPUDeterminismMode", &StartUp.m_strGPUDeterminismMode,
                      StartUp.m_strGPUDeterminismMode);
    core_section->Get("Overclock", &StartUp.m_OCFactor, StartUp.m_OCFactor);
    core_section->Get("OverclockEnable", &StartUp.m_OCEnable, StartUp.m_OCEnable);

    for (unsigned int i = 0; i < SerialInterface::MAX_SI_CHANNELS; ++i)
    {
      int source;
      controls_section->Get(StringFromFormat("PadType%u", i), &source, -1);
      if (source >= SerialInterface::SIDEVICE_NONE && source < SerialInterface::SIDEVICE_COUNT)
      {
        StartUp.m_SIDevice[i] = static_cast<SerialInterface::SIDevices>(source);
        config_cache.bSetPads[i] = true;
      }
    }

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

  StartUp.m_GPUDeterminismMode = ParseGPUDeterminismMode(StartUp.m_strGPUDeterminismMode);

  // Movie settings
  if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
  {
    // TODO: remove this once ConfigManager starts using OnionConfig.
    StartUp.bCPUThread = Config::Get(Config::MAIN_CPU_THREAD);
    StartUp.bDSPHLE = Config::Get(Config::MAIN_DSP_HLE);
    StartUp.bFastDiscSpeed = Config::Get(Config::MAIN_FAST_DISC_SPEED);
    StartUp.iCPUCore = Config::Get(Config::MAIN_CPU_CORE);
    StartUp.bSyncGPU = Config::Get(Config::MAIN_SYNC_GPU);
    if (!StartUp.bWii)
      StartUp.SelectedLanguage = Config::Get(Config::MAIN_GC_LANGUAGE);
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
  {
    Config::AddLayer(ConfigLoaders::GenerateNetPlayConfigLoader(g_NetPlaySettings));
    StartUp.bCPUThread = g_NetPlaySettings.m_CPUthread;
    StartUp.bEnableCheats = g_NetPlaySettings.m_EnableCheats;
    StartUp.bDSPHLE = g_NetPlaySettings.m_DSPHLE;
    StartUp.bEnableMemcardSdWriting = g_NetPlaySettings.m_WriteToMemcard;
    StartUp.bCopyWiiSaveNetplay = g_NetPlaySettings.m_CopyWiiSave;
    StartUp.iCPUCore = g_NetPlaySettings.m_CPUcore;
    StartUp.SelectedLanguage = g_NetPlaySettings.m_SelectedLanguage;
    StartUp.bOverrideGCLanguage = g_NetPlaySettings.m_OverrideGCLanguage;
    StartUp.m_DSPEnableJIT = g_NetPlaySettings.m_DSPEnableJIT;
    StartUp.m_OCEnable = g_NetPlaySettings.m_OCEnable;
    StartUp.m_OCFactor = g_NetPlaySettings.m_OCFactor;
    StartUp.m_EXIDevice[0] = g_NetPlaySettings.m_EXIDevice[0];
    StartUp.m_EXIDevice[1] = g_NetPlaySettings.m_EXIDevice[1];
    config_cache.bSetEXIDevice[0] = true;
    config_cache.bSetEXIDevice[1] = true;
  }
  else
  {
    g_SRAM_netplay_initialized = false;
  }

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
    ConfigLoaders::SaveToSYSCONF(Config::GetLayer(Config::LayerType::Meta));

  const bool load_ipl = !StartUp.bWii && !StartUp.bHLE_BS2 &&
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
// Conversely, we also shouldn't just ignore any changes to SYSCONF, as it may cause
// temporary settings (from Movie, Netplay, game INIs, etc.) to stick around.
//
// To avoid inconveniences in most cases, we always restore only the overridden settings.
static void RestoreSYSCONF()
{
  // This layer contains the new SYSCONF settings (including any temporary settings).
  auto layer = std::make_unique<Config::Layer>(ConfigLoaders::GenerateBaseConfigLoader());
  for (const auto& setting : Config::SYSCONF_SETTINGS)
  {
    std::visit(
        [&](auto& info) {
          // If this setting was overridden, then we copy the base layer value back to the SYSCONF.
          // Otherwise we leave the new value in the SYSCONF.
          if (Config::GetActiveLayerForConfig(info) != Config::LayerType::Base)
            layer->Set(info, Config::GetBase(info));
        },
        setting.config_info);
  }
  // Save the SYSCONF.
  layer->Save();
  Config::AddLayer(std::move(layer));
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
