// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"
#include "Core/WiiUtils.h"
#include "DiscIO/VolumeWad.h"

bool CBoot::BootNANDTitle(const u64 title_id)
{
  UpdateStateFlags([](StateFlags* state) {
    state->type = 0x04;  // TYPE_NANDBOOT
  });

  auto es = IOS::HLE::GetIOS()->GetES();
  const IOS::ES::TicketReader ticket = es->FindSignedTicket(title_id);
  auto console_type = IOS::HLE::IOSC::ConsoleType::Retail;
  if (ticket.IsValid())
    console_type = ticket.GetConsoleType();
  else
    ERROR_LOG(BOOT, "No ticket was found for %016" PRIx64, title_id);
  SetupWiiMemory(console_type);
  return es->LaunchTitle(title_id);
}

bool CBoot::Boot_WiiWAD(const DiscIO::VolumeWAD& wad)
{
  if (!WiiUtils::InstallWAD(*IOS::HLE::GetIOS(), wad, WiiUtils::InstallType::Temporary))
  {
    PanicAlertT("Cannot boot this WAD because it could not be installed to the NAND.");
    return false;
  }
  return BootNANDTitle(wad.GetTMD().GetTitleId());
}
