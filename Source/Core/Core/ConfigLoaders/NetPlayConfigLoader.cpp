// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/ConfigLoaders/NetPlayConfigLoader.h"

#include <memory>

#include "Common/Config/Config.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/NetPlayProto.h"

namespace ConfigLoaders
{
class NetPlayConfigLayerLoader final : public Config::ConfigLayerLoader
{
public:
  explicit NetPlayConfigLayerLoader(const NetSettings& settings)
      : ConfigLayerLoader(Config::LayerType::Netplay), m_settings(settings)
  {
  }

  void Load(Config::Layer* config_layer) override
  {
    Config::Section* core = config_layer->GetOrCreateSection(Config::System::Main, "Core");
    Config::Section* dsp = config_layer->GetOrCreateSection(Config::System::Main, "DSP");

    core->Set("CPUThread", m_settings.m_CPUthread);
    core->Set("CPUCore", m_settings.m_CPUcore);
    core->Set("SelectedLanguage", m_settings.m_SelectedLanguage);
    core->Set("OverrideGCLang", m_settings.m_OverrideGCLanguage);
    core->Set("DSPHLE", m_settings.m_DSPHLE);
    core->Set("OverclockEnable", m_settings.m_OCEnable);
    core->Set("Overclock", m_settings.m_OCFactor);
    core->Set("SlotA", m_settings.m_EXIDevice[0]);
    core->Set("SlotB", m_settings.m_EXIDevice[1]);
    core->Set("EnableSaving", m_settings.m_WriteToMemcard);

    dsp->Set("EnableJIT", m_settings.m_DSPEnableJIT);

    config_layer->Set(Config::SYSCONF_PROGRESSIVE_SCAN, m_settings.m_ProgressiveScan);
    config_layer->Set(Config::SYSCONF_PAL60, m_settings.m_PAL60);
  }

  void Save(Config::Layer* config_layer) override
  {
    // Do Nothing
  }

private:
  const NetSettings m_settings;
};

// Loader generation
std::unique_ptr<Config::ConfigLayerLoader> GenerateNetPlayConfigLoader(const NetSettings& settings)
{
  return std::make_unique<NetPlayConfigLayerLoader>(settings);
}
}
