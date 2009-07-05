// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Globals.h"
#include "Common.h"
#include "IniFile.h"
#include "Config.h"
#include "AudioCommon.h"

CConfig g_Config;

CConfig::CConfig()
{
	Load();
}

void CConfig::Load()
{
	// first load defaults
        std::string temp;
        
	IniFile file;
	file.Load(FULL_CONFIG_DIR "DSP.ini");
	file.Get("Config", "EnableHLEAudio", &m_EnableHLEAudio, true); // Sound Settings
	file.Get("Config", "EnableRE0AudioFix", &m_EnableRE0Fix, false); // RE0 Hack
	ac_Config.Load(file);
}

void CConfig::Save()
{
	IniFile file;
	file.Load(FULL_CONFIG_DIR "DSP.ini");
	file.Set("Config", "EnableHLEAudio", m_EnableHLEAudio); // Sound Settings
	file.Set("Config", "EnableRE0AudioFix", m_EnableRE0Fix); // RE0 Hack
	ac_Config.Set(file);

	file.Save(FULL_CONFIG_DIR "DSP.ini");
}

void CConfig::GameIniLoad() {
	IniFile *iniFile = ((struct SConfig *)globals->config)->m_LocalCoreStartupParameter.gameIni;
	if (! iniFile) 
		return;

	if (iniFile->Exists("HLEaudio", "UseRE0Fix"))
		iniFile->Get("HLEaudio", "UseRE0Fix", &m_EnableRE0Fix, 0);
}

