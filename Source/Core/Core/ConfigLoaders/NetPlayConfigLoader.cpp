// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/NetPlayConfigLoader.h"

#include <memory>

#include "Common/CommonPaths.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/NetPlayProto.h"

namespace ConfigLoaders
{
class NetPlayConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  explicit NetPlayConfigLayerLoader(const NetPlay::NetSettings& settings)
      : ConfigLayerLoader(Config::LayerType::Netplay), m_settings(settings)
  {
  }

  void Load(Config::Layer* layer) override
  {
    layer->Set(Config::MAIN_CPU_THREAD, m_settings.m_CPUthread);
    layer->Set(Config::MAIN_CPU_CORE, m_settings.m_CPUcore);
    layer->Set(Config::MAIN_GC_LANGUAGE, m_settings.m_SelectedLanguage);
    layer->Set(Config::MAIN_OVERRIDE_GC_LANGUAGE, m_settings.m_OverrideGCLanguage);
    layer->Set(Config::MAIN_DSP_HLE, m_settings.m_DSPHLE);
    layer->Set(Config::MAIN_OVERCLOCK_ENABLE, m_settings.m_OCEnable);
    layer->Set(Config::MAIN_OVERCLOCK, m_settings.m_OCFactor);
    layer->Set(Config::MAIN_SLOT_A, static_cast<int>(m_settings.m_EXIDevice[0]));
    layer->Set(Config::MAIN_SLOT_B, static_cast<int>(m_settings.m_EXIDevice[1]));
    layer->Set(Config::MAIN_WII_SD_CARD_WRITABLE, m_settings.m_WriteToMemcard);
    layer->Set(Config::MAIN_REDUCE_POLLING_RATE, m_settings.m_ReducePollingRate);

    layer->Set(Config::MAIN_DSP_JIT, m_settings.m_DSPEnableJIT);

    layer->Set(Config::SYSCONF_PROGRESSIVE_SCAN, m_settings.m_ProgressiveScan);
    layer->Set(Config::SYSCONF_PAL60, m_settings.m_PAL60);

    if (m_settings.m_SyncSaveData)
    {
      if (!m_settings.m_IsHosting)
      {
        const std::string path = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY DIR_SEP;
        layer->Set(Config::MAIN_GCI_FOLDER_A_PATH_OVERRIDE, path + "Card A");
        layer->Set(Config::MAIN_GCI_FOLDER_B_PATH_OVERRIDE, path + "Card B");

        const std::string file = File::GetUserPath(D_GCUSER_IDX) + GC_MEMCARD_NETPLAY + "%c." +
                                 m_settings.m_SaveDataRegion + ".raw";
        layer->Set(Config::MAIN_MEMCARD_A_PATH, StringFromFormat(file.c_str(), 'A'));
        layer->Set(Config::MAIN_MEMCARD_B_PATH, StringFromFormat(file.c_str(), 'B'));
      }

      layer->Set(Config::MAIN_GCI_FOLDER_CURRENT_GAME_ONLY, true);
    }
  }

  void Save(Config::Layer* layer) override
  {
    // Do Nothing
  }

private:
  const NetPlay::NetSettings m_settings;
};

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader>
GenerateNetPlayConfigLoader(const NetPlay::NetSettings& settings)
{
  return std::make_unique<NetPlayConfigLayerLoader>(settings);
}
}  // namespace ConfigLoaders
