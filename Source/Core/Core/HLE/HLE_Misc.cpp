// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/Host.h"
#include "Core/HLE/HLE_Misc.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCCache.h"

namespace HLE_Misc
{

static std::string args;

// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction()
{
	NPC = LR;
}

// If you want a function to panic, you can rename it PanicAlert :p
// Don't know if this is worth keeping.
void HLEPanicAlert()
{
	::PanicAlert("HLE: PanicAlert %08x", LR);
	NPC = LR;
}

void HBReload()
{
	// There isn't much we can do. Just stop cleanly.
	PowerPC::Pause();
	Host_Message(WM_USER_STOP);
}

void HLEGeckoCodehandler()
{
	// Work around the codehandler not properly invalidating the icache, but
	// only the first few frames.
	// (Project M uses a conditional to only apply patches after something has
	// been read into memory, or such, so we do the first 5 frames.  More
	// robust alternative would be to actually detect memory writes, but that
	// would be even uglier.)
	u32 magic = 0xd01f1bad;
	u32 existing = PowerPC::HostRead_U32(0x80001800);
	if (existing - magic == 5)
	{
		return;
	}
	else if (existing - magic > 5)
	{
		existing = magic;
	}
	PowerPC::HostWrite_U32(existing + 1, 0x80001800);
	PowerPC::ppcState.iCache.Reset();
}

}
