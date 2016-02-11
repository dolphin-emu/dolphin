// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/IniFile.h"
#include "Common/OnionConfig.h"

#include "DolphinWX/ControllerProfileLoader.h"

class INIProfileConfigLoader : public OnionConfig::ConfigLayerLoader
{
public:
	INIProfileConfigLoader(const std::string& filename)
		: ConfigLayerLoader(OnionConfig::OnionLayerType::LAYER_BASE),
		  m_filename(filename)
     	{}

	void Load(OnionConfig::BloomLayer* config_layer) override
	{
		IniFile ini;
		ini.Load(m_filename);
		const std::list<IniFile::Section>& system_sections = ini.GetSections();

		// This will only end up being one section
		// Just throw everything in to SYSTEM_MAIN
		// This class can't be used within the global OnionConfig namespace anyway
		for (auto section : system_sections)
		{
			const std::string section_name = section.GetName();
			OnionConfig::OnionPetal* petal = config_layer->GetOrCreatePetal(OnionConfig::OnionSystem::SYSTEM_MAIN, section_name);
			const IniFile::Section::SectionMap& section_map = section.GetValues();

			for (auto& value : section_map)
				petal->Set(value.first, value.second);
		}
	}

	void Save(OnionConfig::BloomLayer* config_layer) override
	{
		IniFile ini;
		const OnionConfig::BloomLayerMap& petals = config_layer->GetBloomLayerMap();
		// We will only ever have SYSTEM_MAIN here
		for (const auto& system : petals)
		{
			for (const auto& petal : system.second)
			{
				const std::string petal_name = petal->GetName();
				const OnionConfig::PetalValueMap& petal_values = petal->GetValues();

				IniFile::Section* section = ini.GetOrCreateSection(petal_name);

				for (auto& value : petal_values)
					section->Set(value.first, value.second);
			}
		}

		ini.Save(m_filename);
	}

private:
	const std::string m_filename;
};

// Loader generation
OnionConfig::ConfigLayerLoader* GenerateProfileConfigLoader(const std::string& filename)
{
	return new INIProfileConfigLoader(filename);
}
