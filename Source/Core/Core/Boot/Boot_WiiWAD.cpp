// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/System.h"
#include "Core/WiiUtils.h"
#include "DiscIO/VolumeWad.h"

bool CBoot::BootNANDTitle(Core::System& system, const u64 title_id)
{
  UpdateStateFlags([](StateFlags* state) {
    state->type = 0x04;  // TYPE_NANDBOOT
  });

  auto es = system.GetIOS()->GetESDevice();
  const IOS::ES::TicketReader ticket = es->GetCore().FindSignedTicket(title_id);
  auto console_type = IOS::HLE::IOSC::ConsoleType::Retail;
  if (ticket.IsValid())
    console_type = ticket.GetConsoleType();
  else
    ERROR_LOG_FMT(BOOT, "No ticket was found for {:016x}", title_id);
  SetupWiiMemory(system, console_type);
  return es->LaunchTitle(title_id);
}

bool CBoot::Boot_WiiWAD(Core::System& system, const DiscIO::VolumeWAD& wad)
{
  if (!WiiUtils::InstallWAD(*system.GetIOS(), wad, WiiUtils::InstallType::Temporary))
  {
    PanicAlertFmtT("Cannot boot this WAD because it could not be installed to the NAND.");
    return false;
  }
  return BootNANDTitle(system, wad.GetTMD().GetTitleId());
}
