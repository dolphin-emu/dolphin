// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _HW_H
#define _HW_H

#include "Common.h"
#include "ChunkFile.h"

namespace HW
{
	void Init();
	void Shutdown();
	void DoState(PointerWrap &p);
}

#endif
