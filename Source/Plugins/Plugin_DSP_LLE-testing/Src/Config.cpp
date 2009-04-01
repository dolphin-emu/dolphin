// Copyright (C) 2003-2009 Dolphin Project.

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
#include "AudioCommon.h"

#define LLE_CONFIG_FILE "DSPLLE.ini"

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
	file.Load(FULL_CONFIG_DIR LLE_CONFIG_FILE);
	ac_Config.Load(file);
}

void CConfig::Save()
{
	IniFile file;
	file.Load(FULL_CONFIG_DIR LLE_CONFIG_FILE);
	ac_Config.Set(file);

	file.Save(FULL_CONFIG_DIR LLE_CONFIG_FILE);
}
