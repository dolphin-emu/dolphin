// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"
#include "Common/Common.h"

namespace HW
{
	void Init();
	void Shutdown();
	void DoState(PointerWrap &p);
}
