// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _ARDECRYPT_H_
#define _ARDECRYPT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "Common.h"
#include "ActionReplay.h"

namespace ActionReplay
{

void DecryptARCode(std::vector<std::string> vCodes, std::vector<AREntry> &ops);

} //namespace

#endif //_ARDECRYPT_H_
