// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

namespace JitRegister
{

void Init();
void Shutdown();
void Register(const void* base_address, u32 code_size,
	const char* name, u32 original_address=0);

}
