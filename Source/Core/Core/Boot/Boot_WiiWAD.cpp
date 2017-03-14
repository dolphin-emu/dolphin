// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstddef>
#include <memory>
#include <numeric>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"

#include "Core/Boot/Boot.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FileIO.h"
#include "Core/IOS/IPC.h"
#include "Core/PatchEngine.h"

#include "DiscIO/NANDContentLoader.h"

struct StateFlags
{
  u32 checksum;
  u8 flags;
  u8 type;
  u8 discstate;
  u8 returnto;
  u32 unknown[6];
};

static u32 StateChecksum(const StateFlags& flags)
{
  constexpr size_t length_in_bytes = sizeof(StateFlags) - 4;
  constexpr size_t num_elements = length_in_bytes / sizeof(u32);
  std::array<u32, num_elements> flag_data;

  std::memcpy(flag_data.data(), &flags.flags, length_in_bytes);

  return std::accumulate(flag_data.cbegin(), flag_data.cend(), 0U);
}

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
    state.checksum = StateChecksum(state);

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
    state.checksum = StateChecksum(state);
    state_file.WriteBytes(&state, sizeof(StateFlags));
  }

  const DiscIO::CNANDContentLoader& ContentLoader =
      DiscIO::CNANDContentManager::Access().GetNANDLoader(_pFilename);
  if (!ContentLoader.IsValid())
    return false;

  u64 titleID = ContentLoader.GetTMD().GetTitleId();
  // create data directory
  File::CreateFullPath(Common::GetTitleDataPath(titleID, Common::FROM_SESSION_ROOT));

  if (titleID == TITLEID_SYSMENU)
    IOS::HLE::CreateVirtualFATFilesystem();
  // setup Wii memory

  if (!SetupWiiMemory(ContentLoader.GetTMD().GetIOSId()))
    return false;

  IOS::HLE::Device::ES::LoadWAD(_pFilename);
  if (!IOS::HLE::BootstrapPPC(ContentLoader))
    return false;

  return true;
}
