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
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"

#include "Core/Boot/Boot.h"
#include "Core/CommonTitles.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileIO.h"
#include "Core/IOS/IOS.h"

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
  std::string state_filename(
      Common::GetTitleDataPath(Titles::SYSTEM_MENU, Common::FROM_SESSION_ROOT) + WII_STATE);

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

  const DiscIO::NANDContentLoader& ContentLoader =
      DiscIO::NANDContentManager::Access().GetNANDLoader(_pFilename);
  if (!ContentLoader.IsValid())
    return false;

  u64 titleID = ContentLoader.GetTMD().GetTitleId();

  if (!IOS::ES::IsChannel(titleID))
  {
    PanicAlertT("This WAD is not bootable.");
    return false;
  }

  // create data directory
  File::CreateFullPath(Common::GetTitleDataPath(titleID, Common::FROM_SESSION_ROOT));

  if (titleID == Titles::SYSTEM_MENU)
    IOS::HLE::CreateVirtualFATFilesystem();
  // setup Wii memory

  if (!SetupWiiMemory(ContentLoader.GetTMD().GetIOSId()))
    return false;

  IOS::HLE::Device::ES::LoadWAD(_pFilename);

  // TODO: kill these manual calls and just use ES_Launch here, as soon as the direct WAD
  //       launch hack is dropped.
  auto* ios = IOS::HLE::GetIOS();
  IOS::ES::UIDSys uid_map{Common::FROM_SESSION_ROOT};
  ios->SetUidForPPC(uid_map.GetOrInsertUIDForTitle(titleID));
  ios->SetGidForPPC(ContentLoader.GetTMD().GetGroupId());

  if (!ios->BootstrapPPC(ContentLoader))
    return false;

  return true;
}
