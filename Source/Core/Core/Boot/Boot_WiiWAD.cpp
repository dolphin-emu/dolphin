// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/WiiUtils.h"
#include "DiscIO/WiiWad.h"

bool CBoot::BootNANDTitle(const u64 title_id)
{
  UpdateStateFlags([](StateFlags* state) {
    state->type = 0x04;  // TYPE_NANDBOOT
  });

  auto* ios = IOS::HLE::GetIOS();
  SetupWiiMemory();
  return ios->GetES()->LaunchTitle(title_id);
}

bool CBoot::Boot_WiiWAD(const DiscIO::WiiWAD& wad)
{
  if (!WiiUtils::InstallWAD(*IOS::HLE::GetIOS(), wad, WiiUtils::InstallType::Temporary))
  {
    PanicAlertT("Cannot boot this WAD because it could not be installed to the NAND.");
    return false;
  }
  return BootNANDTitle(wad.GetTMD().GetTitleId());
}
