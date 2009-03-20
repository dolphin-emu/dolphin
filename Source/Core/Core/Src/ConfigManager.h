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

#ifndef _CONFIGMANAGER_H
#define _CONFIGMANAGER_H

#include <string>
#include <vector>

#include "Boot/Boot.h"
#include "HW/EXI_Device.h"
#include "HW/SI_Device.h"

// HyperIris: not sure but a temporary implement
enum INTERFACE_LANGUAGE
{
	INTERFACE_ENGLISH = 0,
	INTERFACE_GERMAN,
	INTERFACE_FRENCH,
	INTERFACE_SPANISH,
	INTERFACE_ITALIAN,
	INTERFACE_DUTCH,

	INTERFACE_OTHER,
};

struct SConfig
{
	// hard coded default plugins ...
	std::string m_DefaultGFXPlugin;
	std::string m_DefaultDSPPlugin;
	std::string m_DefaultPADPlugin;
	std::string m_DefaultWiiMotePlugin;

	// name of the last used filename
	std::string m_LastFilename;

	// gcm folder
	std::vector<std::string>m_ISOFolder;

	SCoreStartupParameter m_LocalCoreStartupParameter;

	std::string m_strMemoryCardA;
	std::string m_strMemoryCardB;
	TEXIDevices m_EXIDevice[3];
	TSIDevices m_SIDevice[4];

	// interface language
	INTERFACE_LANGUAGE m_InterfaceLanguage;
	// other interface settings
	bool m_InterfaceToolbar;
	bool m_InterfaceStatusbar;
	bool m_InterfaceLogWindow;
	bool m_InterfaceConsole;

	// save settings
	void SaveSettings();

	// load settings
	void LoadSettings();

	/* Return the permanent and somewhat globally used instance of this struct
	   there is also a Core::GetStartupParameter() instance of it with almost
	   the same values */
	static SConfig& GetInstance() {return(m_Instance);}

	private:

		// constructor
		SConfig();

		// destructor
		~SConfig();

		static SConfig m_Instance;
};

#endif // endif config manager
