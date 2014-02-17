// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Common.h"
#include "Common/ChunkFile.h"

namespace HW
{
	void Init();
	void Shutdown();
	void DoState(PointerWrap &p);
}
