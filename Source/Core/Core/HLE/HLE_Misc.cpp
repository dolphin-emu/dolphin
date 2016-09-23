// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/ConfigManager.h"
#include "Core/GeckoCode.h"
#include "Core/HLE/HLE_Misc.h"
#include "Core/HW/CPU.h"
#include "Core/Host.h"
#include "Core/PowerPC/PPCCache.h"
#include "Core/PowerPC/PowerPC.h"

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
  CPU::Break();
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
  u32 gch_gameid = PowerPC::HostRead_U32(Gecko::INSTALLER_BASE_ADDRESS);
  if (gch_gameid - Gecko::MAGIC_GAMEID == 5)
  {
    return;
  }
  else if (gch_gameid - Gecko::MAGIC_GAMEID > 5)
  {
    gch_gameid = Gecko::MAGIC_GAMEID;
  }
  PowerPC::HostWrite_U32(gch_gameid + 1, Gecko::INSTALLER_BASE_ADDRESS);

  PowerPC::ppcState.iCache.Reset();
}
}
