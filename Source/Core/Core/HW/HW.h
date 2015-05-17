// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class PointerWrap;

namespace HW
{
	void Init();
	void Shutdown();
	void DoState(PointerWrap &p);
}
