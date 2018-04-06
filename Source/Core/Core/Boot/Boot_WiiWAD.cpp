// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/WiiUtils.h"

#include "DiscIO/WiiWad.h"

// cdb.vff is a virtual Fat filesystem created on first launch of sysmenu
// we create it here as it is faster ~3 minutes for me when sysmenu does it
// ~1 second created here
static void CreateVirtualFATFilesystem(std::shared_ptr<IOS::HLE::FS::FileSystem> fs)
{
  constexpr u32 SYSMENU_UID = 0x1000;
  constexpr u16 SYSMENU_GID = 0x1;

  const std::string cdb_path = "/title/00000001/00000002/data/cdb.vff";
  fs->CreateFile(SYSMENU_UID, SYSMENU_GID, cdb_path, 0, IOS::HLE::FS::Mode::ReadWrite,
                 IOS::HLE::FS::Mode::ReadWrite, IOS::HLE::FS::Mode::ReadWrite);

  constexpr size_t CDB_SIZE = 0x01400000;
  const auto metadata = fs->GetMetadata(SYSMENU_UID, SYSMENU_GID, cdb_path);
  if (!metadata || metadata->size >= CDB_SIZE)
    return;

  const auto fd = fs->OpenFile(SYSMENU_UID, SYSMENU_GID, cdb_path, IOS::HLE::FS::Mode::Write);
  if (!fd)
    return;

  constexpr u8 CDB_HEADER[0x20] = {'V', 'F', 'F', 0x20, 0xfe, 0xff, 1, 0, 1, 0x40, 0, 0, 0, 0x20};
  constexpr u8 CDB_FAT[4] = {0xf0, 0xff, 0xff, 0xff};
  std::vector<u8> data(CDB_SIZE);
  std::copy_n(CDB_HEADER, sizeof(CDB_HEADER), data.begin());
  std::copy_n(CDB_FAT, sizeof(CDB_FAT), data.begin() + sizeof(CDB_HEADER));
  std::copy_n(CDB_FAT, sizeof(CDB_FAT), data.begin() + 0x14020);
  // write the final 0 to 0 file from the second FAT to 20 MiB
  data[CDB_SIZE - 1] = 0;
  fs->WriteFile(*fd, data.data(), static_cast<u32>(data.size()));
}

bool CBoot::BootNANDTitle(const u64 title_id)
{
  UpdateStateFlags([](StateFlags* state) {
    state->type = 0x04;  // TYPE_NANDBOOT
  });

  auto* ios = IOS::HLE::GetIOS();

  if (title_id == Titles::SYSTEM_MENU)
    CreateVirtualFATFilesystem(ios->GetFS());

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
