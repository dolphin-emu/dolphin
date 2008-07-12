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

#ifndef _CONFIG_H
#define _CONFIG_H

#include <string>
#include <vector>

#include "Boot/Boot.h"

struct SConfig
{
	// default plugins ... hard coded inside the ini
	std::string m_DefaultGFXPlugin;
	std::string m_DefaultDSPPlugin;
	std::string m_DefaultPADPlugin;

	// name of the last used filename
	std::string m_LastFilename;

	// gcm folder
	std::vector<std::string>m_ISOFolder;

	SCoreStartupParameter m_LocalCoreStartupParameter;

	// save settings
	void SaveSettings();


	// load settings
	void LoadSettings();


	static SConfig& GetInstance() {return(m_Instance);}


	private:

		// constructor
		SConfig();

		// destructor
		~SConfig();

		static SConfig m_Instance;
};

#endif
