// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

namespace TieredRecompiler
{
	bool Init();
	void Shutdown();
	// If we want to queue up work to the tiered recompiler, we do it here
	void AddWorkToQueue(u32 em_address);
}
