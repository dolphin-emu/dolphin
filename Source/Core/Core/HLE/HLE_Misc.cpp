// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HLE/HLE_Misc.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/HW/CPU.h"
#include "Core/Host.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include <Common/CommonPaths.h>
#include <Core/ConfigManager.h>

namespace HLE_Misc
{
// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();
  ppc_state.npc = LR(ppc_state);
}

const char* GetGeckoCodeHandlerPath()
{
  int code_handler_value = Config::Get(Config::MAIN_CODE_HANDLER); // Get the integer value
  switch (code_handler_value)
  {
    case 0: return GECKO_CODE_HANDLER; // Dolphin (Stock)
    case 1: return GECKO_CODE_HANDLER_MPN; // MPN (Extended)
    default:
      return GECKO_CODE_HANDLER_MPN;  // Fallback
  }
}

bool IsGeckoCodeHandlerEnabled()
{
  int code_handler_value = Config::Get(Config::MAIN_CODE_HANDLER); // Get the integer value
  return code_handler_value == 1; // Return true for 1 and 2
}

bool IsGeckoCodeHandlerMPN()
{
  int code_handler_value = Config::Get(Config::MAIN_CODE_HANDLER); // Get the integer value
  return code_handler_value == 2; // Return true for 1 and 2
}


void HBReload(const Core::CPUThreadGuard& guard)
{
  // There isn't much we can do. Just stop cleanly.
  auto& system = guard.GetSystem();
  system.GetCPU().Break();
  Host_Message(HostMessageID::WMUserStop);
}

void GeckoCodeHandlerICacheFlush(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();

  // Work around the codehandler not properly invalidating the icache, but
  // only the first few frames.
  // (Project M uses a conditional to only apply patches after something has
  // been read into memory, or such, so we do the first 5 frames.  More
  // robust alternative would be to actually detect memory writes, but that
  // would be even uglier.)

  const bool is_mpn_handler_and_game_id_gp7e01 =
      IsGeckoCodeHandlerMPN() && (SConfig::GetInstance().GetGameID() == "GP7E01");
  const bool is_mpn_handler_and_game_id_gp6e01 =
      IsGeckoCodeHandlerMPN() && (SConfig::GetInstance().GetGameID() == "GP6E01");
  const bool is_mpn_handler_and_game_id_gp5e01 =
      IsGeckoCodeHandlerMPN() && (SConfig::GetInstance().GetGameID() == "GP5E01");

  u32 codelist_hook = is_mpn_handler_and_game_id_gp7e01 ? Gecko::INSTALLER_BASE_ADDRESS_MP7 :
                      is_mpn_handler_and_game_id_gp6e01 ? Gecko::INSTALLER_BASE_ADDRESS_MP6 :
                      is_mpn_handler_and_game_id_gp5e01 ? Gecko::INSTALLER_BASE_ADDRESS_MP5 :
                                                          Gecko::INSTALLER_BASE_ADDRESS;

  u32 gch_gameid = PowerPC::MMU::HostRead_U32(guard, codelist_hook);
  if (gch_gameid - Gecko::MAGIC_GAMEID == 5)
  {
    return;
  }
  else if (gch_gameid - Gecko::MAGIC_GAMEID > 5)
  {
    gch_gameid = Gecko::MAGIC_GAMEID;
  }
  PowerPC::MMU::HostWrite_U32(guard, gch_gameid + 1, codelist_hook);

  ppc_state.iCache.Reset();
}

// Because Dolphin messes around with the CPU state instead of patching the game binary, we
// need a way to branch into the GCH from an arbitrary PC address. Branching is easy, returning
// back is the hard part. This HLE function acts as a trampoline that restores the original LR, SP,
// and PC before the magic, invisible BL instruction happened.
void GeckoReturnTrampoline(const Core::CPUThreadGuard& guard)
{
  auto& system = guard.GetSystem();
  auto& ppc_state = system.GetPPCState();

  // Stack frame is built in GeckoCode.cpp, Gecko::RunCodeHandler.
  const u32 SP = ppc_state.gpr[1];
  ppc_state.gpr[1] = PowerPC::MMU::HostRead_U32(guard, SP + 8);
  ppc_state.npc = PowerPC::MMU::HostRead_U32(guard, SP + 12);
  LR(ppc_state) = PowerPC::MMU::HostRead_U32(guard, SP + 16);
  ppc_state.cr.Set(PowerPC::MMU::HostRead_U32(guard, SP + 20));
  for (int i = 0; i < 14; ++i)
  {
    ppc_state.ps[i].SetBoth(PowerPC::MMU::HostRead_U64(guard, SP + 24 + 2 * i * sizeof(u64)),
                            PowerPC::MMU::HostRead_U64(guard, SP + 24 + (2 * i + 1) * sizeof(u64)));
  }
}
}  // namespace HLE_Misc
