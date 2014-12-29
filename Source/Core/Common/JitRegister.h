// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once
#include "Common/CommonTypes.h"

namespace JitRegister
{

void Init();
void Shutdown();
void Register(const void* base_address, u32 code_size,
	const char* format, ...);

}
