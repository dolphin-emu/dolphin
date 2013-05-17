// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "Thread.h"
#include "FileUtil.h"
#include "IniFile.h"

#include "ControllerInterface/ControllerInterface.h"
#include "ControllerEmu.h"

#include <string>
#include <vector>
#include <map>
#include <sstream>

// InputPlugin isn't a very good name anymore since it's used by GCPad/Wiimote
// which are not even plugins anymore.
class InputPlugin
{
public:

	InputPlugin(const char* const _ini_name, const char* const _gui_name,
		const char* const _profile_name)
		: ini_name(_ini_name), gui_name(_gui_name), profile_name(_profile_name) {}

	~InputPlugin();

	bool LoadConfig(bool isGC);
	void SaveConfig();

	std::vector< ControllerEmu* >	controllers;

	std::recursive_mutex controls_lock;		// for changing any control references

	const char * const		ini_name;
	const char * const		gui_name;
	const char * const		profile_name;
};

#endif
