// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"

#include "Core/Boot/Boot.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/IOS/FS/FileIO.h"
#include "Core/IOS/IPC.h"
#include "Core/PatchEngine.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

static u32 state_checksum(u32* buf, int len)
{
  u32 checksum = 0;
  len = len >> 2;

  for (int i = 0; i < len; i++)
  {
    checksum += buf[i];
  }

  return checksum;
}

struct StateFlags
{
  u32 checksum;
  u8 flags;
  u8 type;
  u8 discstate;
  u8 returnto;
  u32 unknown[6];
};

bool CBoot::Boot_WiiWAD(const std::string& _pFilename)
{
  std::string state_filename(Common::GetTitleDataPath(TITLEID_SYSMENU, Common::FROM_SESSION_ROOT) +
                             WII_STATE);

  if (File::Exists(state_filename))
  {
    File::IOFile state_file(state_filename, "r+b");
    StateFlags state;
    state_file.ReadBytes(&state, sizeof(StateFlags));

    state.type = 0x03;  // TYPE_RETURN
    state.checksum = state_checksum((u32*)&state.flags, sizeof(StateFlags) - 4);

    state_file.Seek(0, SEEK_SET);
    state_file.WriteBytes(&state, sizeof(StateFlags));
  }
  else
  {
    File::CreateFullPath(state_filename);
    File::IOFile state_file(state_filename, "a+b");
    StateFlags state;
    memset(&state, 0, sizeof(StateFlags));
    state.type = 0x03;       // TYPE_RETURN
    state.discstate = 0x01;  // DISCSTATE_WII
    state.checksum = state_checksum((u32*)&state.flags, sizeof(StateFlags) - 4);
    state_file.WriteBytes(&state, sizeof(StateFlags));
  }

  const DiscIO::CNANDContentLoader& ContentLoader =
      DiscIO::CNANDContentManager::Access().GetNANDLoader(_pFilename);
  if (!ContentLoader.IsValid())
    return false;

  u64 titleID = ContentLoader.GetTitleID();
  // create data directory
  File::CreateFullPath(Common::GetTitleDataPath(titleID, Common::FROM_SESSION_ROOT));

  if (titleID == TITLEID_SYSMENU)
    IOS::HLE::CreateVirtualFATFilesystem();
  // setup Wii memory

  u64 ios_title_id = 0x0000000100000000ULL | ContentLoader.GetIosVersion();
  if (!SetupWiiMemory(ios_title_id))
    return false;
  // DOL
  const DiscIO::SNANDContent* pContent =
      ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex());
  if (pContent == nullptr)
    return false;

  IOS::HLE::SetDefaultContentFile(_pFilename);

  std::unique_ptr<CDolLoader> pDolLoader = std::make_unique<CDolLoader>(pContent->m_Data->Get());
  if (!pDolLoader->IsValid())
    return false;

  pDolLoader->Load();
  // NAND titles start with address translation off at 0x3400 (via the PPC bootstub)
  // The state of other CPU registers (like the BAT registers) doesn't matter much
  // because the realmode code at 0x3400 initializes everything itself anyway.
  MSR = 0;
  PC = 0x3400;

  // Load patches and run startup patches
  const std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(_pFilename));
  if (pVolume != nullptr)
    PatchEngine::LoadPatches();

  return true;
}
