// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/OnionConfig.h"
#include "Common/Logging/Log.h"

#include "Core/OnionCoreLoaders/NetPlayConfigLoader.h"

class NetPlayConfigLayerLoader : public OnionConfig::ConfigLayerLoader
{
public:
	NetPlayConfigLayerLoader(const NetSettings& settings)
		: ConfigLayerLoader(OnionConfig::OnionLayerType::LAYER_NETPLAY),
		  m_settings(settings)
	{ }

	void Load(OnionConfig::BloomLayer* config_layer) override
	{
		OnionConfig::OnionPetal* core = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core");
		OnionConfig::OnionPetal* dsp = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "DSP");
		OnionConfig::OnionPetal* display = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, "Display");

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

		display->Set("ProgressiveScan", m_settings.m_ProgressiveScan);
		display->Set("PAL60", m_settings.m_PAL60);
	}

	void Save(OnionConfig::BloomLayer* config_layer) override
	{
		// Do Nothing
	}

private:
	const NetSettings m_settings;
};

// Loader generation
OnionConfig::ConfigLayerLoader* GenerateNetPlayConfigLoader(const NetSettings& settings)
{
	return new NetPlayConfigLayerLoader(settings);
}
