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

#include <fstream>
#include <stdlib.h>

#include "XDGLookup.h"
#include "StringUtil.h"

namespace XDG
{

std::string HomeDir()
{
	char* dir = getenv("HOME");
	if (dir)
		return dir;
	dir = getenv("PWD");
	if (dir)
		return dir;
	return "/tmp";
}

std::string ConfigDir()
{
	char* config_home = getenv("XDG_CONFIG_HOME");
	if (!config_home || config_home[0] == 0)
		return HomeDir() + "/.config";
	else
		return config_home;
}

std::string Lookup(const std::string& type, const std::string& fallback)
{
	std::ifstream in_file;
	in_file.open((ConfigDir() + "/user-dirs.dirs").c_str());
	if (in_file.is_open())
	{
		char buffer[512];
		std::string user_dir;

		while (user_dir.empty() && !in_file.getline(buffer, 512).eof())
		{
			std::string line = buffer;
			StripSpaces(line);

			if (line.substr(0, 4) != "XDG_")
				continue;
			line.erase(0, 4);
			if (line.substr(0, type.size()) != type)
				continue;
			line.erase(0, type.size());
			if (line.substr(0, 4) != "_DIR")
				continue;
			line.erase(0, 4);

			StripSpaces(line);

			if (line[0] != '=')
				continue;
			line.erase(0, 1);

			StripSpaces(line);

			if (line[0] != '"')
				continue;
			line.erase(0, 1);

			if (line.substr(0, 6) == "$HOME/")
			{
				line.erase(0, 6);
				user_dir = HomeDir() + "/";
			}
			else if (line[0] != '/')
				continue;

			user_dir += line.substr(0, line.find_last_of("\""));
		}
	}

	return fallback;
}

}
