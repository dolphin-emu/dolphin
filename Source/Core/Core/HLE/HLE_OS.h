// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef HLE_OS_H
#define HLE_OS_H

#include "Common.h"

namespace HLE_OS
{
	void HLE_GeneralDebugPrint();
	void HLE_write_console();
	void HLE_OSPanic();
}

#endif
