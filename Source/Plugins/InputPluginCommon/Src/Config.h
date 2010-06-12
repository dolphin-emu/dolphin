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
