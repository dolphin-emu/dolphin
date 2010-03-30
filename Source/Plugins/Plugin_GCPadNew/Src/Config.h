#ifndef _CONFIG_H_
#define _CONFIG_H_

#define CONFIG_FILE_NAME		"GCPadNew.ini"

#include "ControllerInterface/ControllerInterface.h"
#include "../../../Core/Common/Src/thread.h"
#include "../../../Core/Common/Src/FileUtil.h"
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

	class Pad : public std::vector<ControllerInterface::ControlReference*>
	{
	public:
		class Options
		{
		public:
			Options() : adjust_diagonal(1), allow_background_input(false) {}
			
			ControlState adjust_diagonal;	// will contain the full diagonal value from the input device
			bool allow_background_input;
			
		}	options;
	};

	std::vector< ControllerEmu* >	controllers;

	Common::CriticalSection			controls_crit, interface_crit;		// lock controls first
	ControllerInterface				controller_interface;
	const std::string				name;
};

#endif
