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

#include "Common.h"
#include "IniFile.h"
#include "Config.h"

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
	file.Get("Config", "EnableDTKMusic", &m_EnableDTKMusic, true);
	file.Get("Config", "EnableThrottle", &m_EnableThrottle, true);

#ifdef _WIN32
        file.Get("Config", "Backend", &temp, "DSound");
#else
        file.Get("Config", "Backend", &temp, "AOSound");
#endif
        strncpy(sBackend, temp.c_str(), 16);
}

void CConfig::Save()
{
	IniFile file;
	file.Load(FULL_CONFIG_DIR "DSP.ini");
	file.Set("Config", "EnableHLEAudio", m_EnableHLEAudio); // Sound Settings
	file.Set("Config", "EnableDTKMusic", m_EnableDTKMusic);
	file.Set("Config", "EnableThrottle", m_EnableThrottle);
        file.Set("Config", "Backend", sBackend);
        
	file.Save(FULL_CONFIG_DIR "DSP.ini");
}
