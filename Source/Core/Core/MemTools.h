// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"


namespace EMM
{
	typedef u32 EAddr;
	void InstallExceptionHandler();
	void UninstallExceptionHandler();
}
