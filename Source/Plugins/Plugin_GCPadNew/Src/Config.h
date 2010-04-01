#ifndef _CONFIG_H_
#define _CONFIG_H_

#define CONFIG_FILE_NAME		"GCPadNew.ini"

#include "ControllerInterface/ControllerInterface.h"
#include "Thread.h"
#include "FileUtil.h"
#include "IniFile.h"

#include "ControllerEmu.h"
#include "ControllerEmu/GCPad/GCPad.h"
#include "ControllerEmu/Wiimote/Wiimote.h"

#include <string>
#include <vector>
#include <map>
#include <sstream>

class Plugin
{
public:

	Plugin();
	~Plugin();

	void LoadConfig();
	void SaveConfig();

	std::vector< ControllerEmu* >	controllers;

	Common::CriticalSection			controls_crit, interface_crit;		// lock controls first
	ControllerInterface				controller_interface;
};

#endif
