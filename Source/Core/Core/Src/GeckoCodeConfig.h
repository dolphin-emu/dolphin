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

#ifndef __GECKOCODECONFIG_h__
#define __GECKOCODECONFIG_h__

#include "GeckoCode.h"

#include "IniFile.h"

namespace Gecko
{

void LoadCodes(const IniFile& inifile, std::vector<GeckoCode>& gcodes);
void SaveCodes(IniFile& inifile, const std::vector<GeckoCode>& gcodes);

};

#endif
