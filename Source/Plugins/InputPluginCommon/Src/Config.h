// Copyright (C) 2010 Dolphin Project.

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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <ControllerInterface/ControllerInterface.h>
#include "Thread.h"
#include "FileUtil.h"
#include "IniFile.h"

#include "ControllerEmu.h"

#include <string>
#include <vector>
#include <map>
#include <sstream>

class Plugin
{
public:

	Plugin( const char* const _ini_name, const char* const _gui_name, const char* const _profile_name  );
	~Plugin();

	bool LoadConfig();
	void SaveConfig();

	std::vector< ControllerEmu* >	controllers;

	Common::CriticalSection			controls_crit, interface_crit;		// lock controls first
	ControllerInterface				controller_interface;

	const char * const		ini_name;
	const char * const		gui_name;
	const char * const		profile_name;
};

#endif
