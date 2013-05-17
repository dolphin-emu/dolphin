// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
