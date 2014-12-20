// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"


namespace EMM
{
	extern const bool g_exception_handlers_supported;
	void InstallExceptionHandler();
	void UninstallExceptionHandler();
}
