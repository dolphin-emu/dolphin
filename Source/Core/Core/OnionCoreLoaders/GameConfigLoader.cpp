// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/MsgHandler.h"
#include "Common/OnionConfig.h"
#include "Common/Logging/Log.h"

#include "Core/OnionCoreLoaders/GameConfigLoader.h"

// Returns all possible filenames in ascending order of priority
static std::vector<std::string> GetGameIniFilenames(const std::string& id, u16 revision)
{
	std::vector<std::string> filenames;

	// INIs that match all regions
	if (id.size() >= 4)
		filenames.push_back(id.substr(0, 3) + ".ini");

	// Regular INIs
	filenames.push_back(id + ".ini");

	// INIs with specific revisions
	filenames.push_back(id + StringFromFormat("r%d", revision) + ".ini");

	return filenames;
}

static std::tuple<OnionConfig::OnionSystem, std::string, std::string> MapINIToRealLocation(const std::string& section, const std::string& key)
{
	static std::map<std::pair<std::string, std::string>,
	                std::tuple<OnionConfig::OnionSystem, std::string, std::string>> ini_to_location =
	{
		// Core
		{ std::make_pair("Core", "CPUThread"),          std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "CPUThread") },
		{ std::make_pair("Core", "SkipIdle"),           std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "SkipIdle") },
		{ std::make_pair("Core", "SyncOnSkipIdle"),     std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "SyncOnSkipIdle") },
		{ std::make_pair("Core", "FPRF"),               std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "FPRF") },
		{ std::make_pair("Core", "AccurateNaNs"),       std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "AccurateNaNs") },
		{ std::make_pair("Core", "MMU"),                std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "MMU") },
		{ std::make_pair("Core", "DCBZ"),               std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "DCBZ") },
		{ std::make_pair("Core", "SyncGPU"),            std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "SyncGPU") },
		{ std::make_pair("Core", "FastDiscSpeed"),      std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "FastDiscSpeed") },
		{ std::make_pair("Core", "DSPHLE"),             std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "DSPHLE") },
		{ std::make_pair("Core", "GFXBackend"),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "GFXBackend") },
		{ std::make_pair("Core", "CPUCore"),            std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "CPUCore") },
		{ std::make_pair("Core", "HLE_BS2"),            std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "HLE_BS2") },
		{ std::make_pair("Core", "EmulationSpeed"),     std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "EmulationSpeed") },
		{ std::make_pair("Core", "FrameSkip"),          std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "FrameSkip") },
		{ std::make_pair("Core", "GPUDeterminismMode"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Core", "GPUDeterminismMode") },
		{ std::make_pair("Core", "ProgressiveScan"),    std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Display", "ProgressiveScan") },
		{ std::make_pair("Core", "PAL60"),              std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Display", "PAL60") },

		// Action Replay cheats
		{ std::make_pair("ActionReplay_Enabled", ""), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "ActionReplay_Enabled", "") },
		{ std::make_pair("ActionReplay", ""),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "ActionReplay", "") },

		// Gecko cheats
		{ std::make_pair("Gecko_Enabled", ""), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Gecko_Enabled", "") },
		{ std::make_pair("Gecko", ""),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "Gecko", "") },

		// OnFrame cheats
		{ std::make_pair("OnFrame_Enabled", ""), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "OnFrame_Enabled", "") },
		{ std::make_pair("OnFrame", ""),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "OnFrame", "") },

		// Debugger values
		{ std::make_pair("Watches", ""),      std::make_tuple(OnionConfig::OnionSystem::SYSTEM_DEBUGGER, "Watches", "") },
		{ std::make_pair("BreakPoints", ""),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_DEBUGGER, "BreakPoints", "") },
		{ std::make_pair("MemoryChecks", ""), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_DEBUGGER, "MemoryChecks", "") },

		// DSP
		{ std::make_pair("DSP", "Volume"),    std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "DSP", "Volume") },
		{ std::make_pair("DSP", "EnableJIT"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "DSP", "EnableJIT") },
		{ std::make_pair("DSP", "Backend"),   std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "DSP", "Backend") },

		// Controls
		// XXX: GCPad?
		{ std::make_pair("Controls", "WiimoteSource0"),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_WIIPAD, "Wiimote1", "Source") },
		{ std::make_pair("Controls", "WiimoteSource1"),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_WIIPAD, "Wiimote2", "Source") },
		{ std::make_pair("Controls", "WiimoteSource2"),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_WIIPAD, "Wiimote3", "Source") },
		{ std::make_pair("Controls", "WiimoteSource3"),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_WIIPAD, "Wiimote4", "Source") },
		{ std::make_pair("Controls", "WiimoteSourceBB"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_WIIPAD, "BalanceBoard", "Source") },
		// XXX: Make sure profiles are loaded properly

		// Video
		{ std::make_pair("Video_Hardware", "VSync"),                        std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hardware", "VSync") },
		{ std::make_pair("Video_Settings", "wideScreenHack"),               std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "wideScreenHack") },
		{ std::make_pair("Video_Settings", "AspectRatio"),                  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "AspectRatio") },
		{ std::make_pair("Video_Settings", "Crop"),                         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "Crop") },
		{ std::make_pair("Video_Settings", "UseXFB"),                       std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "UseXFB") },
		{ std::make_pair("Video_Settings", "UseRealXFB"),                   std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "UseRealXFB") },
		{ std::make_pair("Video_Settings", "SafeTextureCacheColorSamples"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "SafeTextureCacheColorSamples") },
		{ std::make_pair("Video_Settings", "HiresTextures"),                std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "HiresTextures") },
		{ std::make_pair("Video_Settings", "ConvertHiresTextures"),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "ConvertHiresTextures") },
		{ std::make_pair("Video_Settings", "CacheHiresTextures"),           std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "CacheHiresTextures") },
		{ std::make_pair("Video_Settings", "EnablePixelLighting"),          std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "EnablePixelLighting") },
		{ std::make_pair("Video_Settings", "FastDepthCalc"),                std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "FastDepthCalc") },
		{ std::make_pair("Video_Settings", "MSAA"),                         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "MSAA") },
		{ std::make_pair("Video_Settings", "SSAA"),                         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "SSAA") },
		{ std::make_pair("Video_Settings", "EFBScale"),                     std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "EFBScale") },
		{ std::make_pair("Video_Settings", "DisableFog"),                   std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Settings", "DisableFog") },

		{ std::make_pair("Video_Enhancements", "ForceFiltering"),       std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Enhancements", "ForceFiltering") },
		{ std::make_pair("Video_Enhancements", "MaxAnisotropy"),        std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Enhancements", "MaxAnisotropy") },
		{ std::make_pair("Video_Enhancements", "PostProcessingShader"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Enhancements", "PostProcessingShader") },

		{ std::make_pair("Video_Stereoscopy", "StereoMode"),     std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy", "StereoMode") },
		{ std::make_pair("Video_Stereoscopy", "StereoDepth"),    std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy", "StereoDepth") },
		{ std::make_pair("Video_Stereoscopy", "StereoSwapEyes"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy", "StereoSwapEyes") },

		{ std::make_pair("Video_Hacks", "EFBAccessEnable"),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks", "EFBAccessEnable") },
		{ std::make_pair("Video_Hacks", "BBoxEnable"),              std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks", "BBoxEnable") },
		{ std::make_pair("Video_Hacks", "ForceProgressive"),        std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks", "ForceProgressive") },
		{ std::make_pair("Video_Hacks", "EFBToTextureEnable"),      std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks", "EFBToTextureEnable") },
		{ std::make_pair("Video_Hacks", "EFBScaledCopy"),           std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks", "EFBScaledCopy") },
		{ std::make_pair("Video_Hacks", "EFBEmulateFormatChanges"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Hacks", "EFBEmulateFormatChanges") },

		// GameINI specific video settings
		{ std::make_pair("Video", "ProjectionHack"),    std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Video", "ProjectionHack") },
		{ std::make_pair("Video", "PH_SZNear"),         std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Video", "PH_SZNear") },
		{ std::make_pair("Video", "PH_SZFar"),          std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Video", "PH_SZFar") },
		{ std::make_pair("Video", "PH_ZNear"),          std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Video", "PH_ZNear") },
		{ std::make_pair("Video", "PH_ZFar"),           std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Video", "PH_ZFar") },
		{ std::make_pair("Video", "PerfQueriesEnable"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Video", "PerfQueriesEnable") },

		{ std::make_pair("Video_Stereoscopy", "StereoConvergence"),    std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy", "StereoConvergence") },
		{ std::make_pair("Video_Stereoscopy", "StereoEFBMonoDepth"),   std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy", "StereoEFBMonoDepth") },
		{ std::make_pair("Video_Stereoscopy", "StereoDepthPercentage"),std::make_tuple(OnionConfig::OnionSystem::SYSTEM_GFX, "Stereoscopy", "StereoDepthPercentage") },

		// UI
		{ std::make_pair("EmuState", "EmulationStateId"), std::make_tuple(OnionConfig::OnionSystem::SYSTEM_UI, "EmuState", "EmulationStateId") },
		{ std::make_pair("EmuState", "EmulationIssues"),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_UI, "EmuState", "EmulationIssues") },
		{ std::make_pair("EmuState", "Title"),  std::make_tuple(OnionConfig::OnionSystem::SYSTEM_UI, "EmuState", "Title") },
	};

	auto it = ini_to_location.find(std::make_pair(section, key));
	if (it == ini_to_location.end())
	{
		// XXX
		printf("Couldn't make %s-%s properly!\n", section.c_str(), key.c_str());
		return std::make_tuple(OnionConfig::OnionSystem::SYSTEM_MAIN, "FAIL!", key);
	}

	return ini_to_location[std::make_pair(section, key)];
}

// INI Game layer configuration loader
class INIGameConfigLayerLoader : public OnionConfig::ConfigLayerLoader
{
public:
	INIGameConfigLayerLoader(const std::string& id, u16 revision, bool global)
		: ConfigLayerLoader(
			global ?
				OnionConfig::OnionLayerType::LAYER_GLOBALGAME :
				OnionConfig::OnionLayerType::LAYER_LOCALGAME),
		  m_id(id), m_revision(revision)
	{}

	void Load(OnionConfig::BloomLayer* config_layer) override
	{
		IniFile ini;
		if (config_layer->GetLayer() == OnionConfig::OnionLayerType::LAYER_GLOBALGAME)
		{
			for (const std::string& filename : GetGameIniFilenames(m_id, m_revision))
				ini.Load(File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP + filename, true);
		}
		else
		{
			for (const std::string& filename : GetGameIniFilenames(m_id, m_revision))
				ini.Load(File::GetUserPath(D_GAMESETTINGS_IDX) + filename, true);
		}

		const std::list<IniFile::Section>& system_sections = ini.GetSections();

		for (auto section : system_sections)
		{
			const std::string section_name = section.GetName();
			if (section.HasLines())
			{
				// Trash INI File chunks
				std::vector<std::string> chunk;
				section.GetLines(&chunk, false);

				std::tuple<OnionConfig::OnionSystem, std::string, std::string> mapped_config = MapINIToRealLocation(section_name, "");
				OnionConfig::OnionPetal* petal = config_layer->GetOrCreatePetal(std::get<0>(mapped_config), std::get<1>(mapped_config));
				petal->SetLines(chunk);
			}

			// Regular key,value pairs
			const IniFile::Section::SectionMap& section_map = section.GetValues();

			for (auto& value : section_map)
			{
				std::tuple<OnionConfig::OnionSystem, std::string, std::string> mapped_config = MapINIToRealLocation(section_name, value.first);
				OnionConfig::OnionPetal* petal = config_layer->GetOrCreatePetal(std::get<0>(mapped_config), std::get<1>(mapped_config));

				petal->Set(std::get<2>(mapped_config), value.second);
			}
		}

		// Game INIs can have controller profiles embedded in to them
		std::string num[] = {"1", "2", "3", "4"};

		if (m_id != "00000000")
		{
			std::tuple<std::string, std::string, OnionConfig::OnionSystem> profile_info[] =
			{
				std::make_tuple("Pad", "GCPad", OnionConfig::OnionSystem::SYSTEM_GCPAD),
				std::make_tuple("Wiimote", "Wiimote", OnionConfig::OnionSystem::SYSTEM_WIIPAD),
			};

			for (auto& use_data : profile_info)
			{
				std::string type = std::get<0>(use_data);
				std::string path = "Profiles/" + std::get<1>(use_data) + "/";

				OnionConfig::OnionPetal* control_petal = config_layer->GetOrCreatePetal(std::get<2>(use_data), "Controls");

				for (int i = 0; i < 4; i++)
				{
					bool use_profile = false;
					std::string profile;
					if (control_petal->Exists(type + "Profile" + num[i]))
					{
						if (control_petal->Get(type + "Profile" + num[i], &profile))
						{
							if (File::Exists(File::GetUserPath(D_CONFIG_IDX) + path + profile + ".ini"))
							{
								use_profile = true;
							}
							else
							{
								// TODO: PanicAlert shouldn't be used for this.
								PanicAlertT("Selected controller profile does not exist");
							}
						}
					}

					if (use_profile)
					{
						IniFile profile_ini;
						profile_ini.Load(File::GetUserPath(D_CONFIG_IDX) + path + profile + ".ini");

						const IniFile::Section* section = profile_ini.GetOrCreateSection("Profile");
						const IniFile::Section::SectionMap& section_map = section->GetValues();
						for (auto& value : section_map)
						{
							OnionConfig::OnionPetal* petal = config_layer->GetOrCreatePetal(std::get<2>(use_data), std::get<1>(use_data) + num[i]);
							petal->Set(value.first, value.second);
						}
					}
				}
			}
		}
	}

	void Save(OnionConfig::BloomLayer* config_layer) override
	{
	}

private:
	const std::string m_id;
	const u16 m_revision;
};

// Loader generation
OnionConfig::ConfigLayerLoader* GenerateGlobalGameConfigLoader(const std::string& id, u16 revision)
{
	return new INIGameConfigLayerLoader(id, revision, true);
}

OnionConfig::ConfigLayerLoader* GenerateLocalGameConfigLoader(const std::string& id, u16 revision)
{
	return new INIGameConfigLayerLoader(id, revision, false);
}
