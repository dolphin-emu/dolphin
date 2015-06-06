// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <initializer_list>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Movie.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

VideoConfig g_Config;
VideoConfig g_ActiveConfig;

void UpdateActiveConfig()
{
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved())
		Movie::SetGraphicsConfig();
	g_ActiveConfig = g_Config;
}

VideoConfig::VideoConfig()
{
	iLog = 0;

	// Exclusive fullscreen flags
	bFullscreen = false;
	bExclusiveMode = false;

	// Needed for the first frame, I think
	fAspectRatioHackW = 1;
	fAspectRatioHackH = 1;

	// disable all features by default
	backend_info.APIType = API_NONE;
	backend_info.bSupportsExclusiveFullscreen = false;
}

void VideoConfig::Load(const std::string& ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	for (auto group : {&m_hardware_group, &m_settings_group, &m_enhancements_group, &m_hacks_group, &m_stereoscopy_group})
	{
		IniFile::Section* section = iniFile.GetOrCreateSection(group->m_name);
		for (auto& option : group->m_options)
		{
			option->SetDefault();
			section->Get(*option);
		}
	}

	// Load common settings
	iniFile.Load(File::GetUserPath(F_DOLPHINCONFIG_IDX));
	IniFile::Section* interface = iniFile.GetOrCreateSection("Interface");
	bool bTmp;
	interface->Get("UsePanicHandlers", &bTmp, true);
	SetEnableAlert(bTmp);

	// Shader Debugging causes a huge slowdown and it's easy to forget about it
	// since it's not exposed in the settings dialog. It's only used by
	// developers, so displaying an obnoxious message avoids some confusion and
	// is not too annoying/confusing for users.
	//
	// XXX(delroth): This is kind of a bad place to put this, but the current
	// VideoCommon is a mess and we don't have a central initialization
	// function to do these kind of checks. Instead, the init code is
	// triplicated for each video backend.
	if (bEnableShaderDebugging)
		OSD::AddMessage("Warning: Shader Debugging is enabled, performance will suffer heavily", 15000);

	VerifyValidity();
}

void VideoConfig::GameIniLoad()
{
	bool gfx_override_exists = false;

	// XXX: Again, bad place to put OSD messages at (see delroth's comment above)
	// XXX: This will add an OSD message for each projection hack value... meh
	IniFile iniFile = SConfig::GetInstance().LoadGameIni();

	int old_EFBScale = iEFBScale;

	for (auto group : {&m_hardware_group, &m_settings_group, &m_enhancements_group, &m_hacks_group, &m_stereoscopy_group})
	{
		IniFile::Section* section = iniFile.GetOrCreateSection(std::string("Video_") + group->m_name);
		for (auto& option : group->m_options)
		{
			if (section->Get(*option))
			{
				std::string msg = StringFromFormat("Note: Option \"%s\" is overridden by game ini.", option->GetKey().c_str());
				OSD::AddMessage(msg, 7500);
				gfx_override_exists = true;
			}
		}
	}

	if (iEFBScale == SCALE_FORCE_INTEGRAL)
	{
		switch (old_EFBScale)
		{
		case SCALE_AUTO:
			iEFBScale = SCALE_AUTO_INTEGRAL;
			break;
		case SCALE_1_5X:
			iEFBScale = SCALE_1X;
			break;
		case SCALE_2_5X:
			iEFBScale = SCALE_2X;
			break;
		default:
			iEFBScale = old_EFBScale;
			break;
		}
	}

	if (gfx_override_exists)
		OSD::AddMessage("Warning: Opening the graphics configuration will reset settings and might cause issues!", 10000);
}

void VideoConfig::VerifyValidity()
{
	// TODO: Check iMaxAnisotropy value
	if (iAdapter < 0 || iAdapter > ((int)backend_info.Adapters.size() - 1)) iAdapter = 0;
	if (iMultisampleMode < 0 || iMultisampleMode >= (int)backend_info.AAModes.size()) iMultisampleMode = 0;

	if (iStereoMode > 0)
	{
		if (!backend_info.bSupportsGeometryShaders)
		{
			OSD::AddMessage("Stereoscopic 3D isn't supported by your GPU, support for OpenGL 3.2 is required.", 10000);
			iStereoMode = 0;
		}

		if (bUseXFB && bUseRealXFB)
		{
			OSD::AddMessage("Stereoscopic 3D isn't supported with Real XFB, turning off stereoscopy.", 10000);
			iStereoMode = 0;
		}
	}
}

void VideoConfig::Save(const std::string& ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	for (auto group : {&m_hardware_group, &m_settings_group, &m_enhancements_group, &m_hacks_group, &m_stereoscopy_group})
	{
		IniFile::Section* section = iniFile.GetOrCreateSection(group->m_name);
		for (auto& option : group->m_options)
		{
			section->Set(*option);
		}
	}

	iniFile.Save(ini_file);
}

bool VideoConfig::IsVSync()
{
	return bVSync && !Core::GetIsFramelimiterTempDisabled();
}
