// Copyright (C) 2003 Dolphin Project.

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

#include "Render3dVision.h"
#include "IniFile.h"

bool Render3dVision::enable3dVision = false;

bool Render3dVision::isEnable3dVision()
{
	return enable3dVision;
}

void Render3dVision::setEnable3dVision(bool value)
{
	enable3dVision = value;
}

void Render3dVision::loadConfig(const char* filename)
{
	IniFile iniFile;
	iniFile.Load(filename);
	
	iniFile.Get("Settings", "Enable3dVision", &enable3dVision, false);
}

void Render3dVision::saveConfig(const char* filename)
{
	IniFile iniFile;
	iniFile.Load(filename);

	iniFile.Set("Settings", "Enable3dVision", enable3dVision);
	
	iniFile.Save(filename);
}
