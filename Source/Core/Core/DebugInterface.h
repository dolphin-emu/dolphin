// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#define DEBUG_PORT 17477

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

namespace Debug
{
	int Init();
	void Update();
}