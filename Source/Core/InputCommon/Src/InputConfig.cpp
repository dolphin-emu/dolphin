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

#include "InputConfig.h"

Plugin::Plugin( const char* const _ini_name, const char* const _gui_name, const char* const _profile_name  )
	: ini_name(_ini_name)
	, gui_name(_gui_name)
	, profile_name(_profile_name)
{
	// GCPads
	//for ( unsigned int i = 0; i<4; ++i )
		//controllers.push_back( new GCPad( i ) );
	// Wiimotes / disabled, cause it only the GUI half is done
	//for ( unsigned int i = 0; i<4; ++i )
	//	controllers.push_back( new Wiimote( i ) );
};

Plugin::~Plugin()
{
	// delete pads
	std::vector<ControllerEmu*>::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i != e; ++i )
		delete *i;
}

bool Plugin::LoadConfig()
{
	IniFile inifile;
	if (false == inifile.Load(std::string(File::GetUserPath(D_CONFIG_IDX)) + ini_name + ".ini"))
		return false;

	std::vector< ControllerEmu* >::const_iterator
		i = controllers.begin(),
		e = controllers.end();
	for ( ; i!=e; ++i ) {
		(*i)->LoadConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
	}
}

void Plugin::SaveConfig()
{
	std::string ini_filename = (std::string(File::GetUserPath(D_CONFIG_IDX)) + ini_name + ".ini" );

	IniFile inifile;
	inifile.Load(ini_filename);

	std::vector< ControllerEmu* >::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i!=e; ++i )
		(*i)->SaveConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
	
	inifile.Save(ini_filename);
}
