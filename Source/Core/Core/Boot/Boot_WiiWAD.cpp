// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
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

bool CBoot::Boot_WiiWAD(const std::string& _pFilename)
{
  UpdateStateFlags([](StateFlags* state) {
    state->type = 0x03;  // TYPE_RETURN
  });

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
