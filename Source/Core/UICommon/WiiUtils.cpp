// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UICommon/WiiUtils.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/WiiWad.h"

namespace WiiUtils
{
bool InstallWAD(const std::string& wad_path)
{
  const DiscIO::WiiWAD wad{wad_path};
  if (!wad.IsValid())
  {
    PanicAlertT("WAD installation failed: The selected file is not a valid WAD.");
    return false;
  }

  const auto tmd = wad.GetTMD();
  IOS::HLE::Kernel ios;
  const auto es = ios.GetES();

  IOS::HLE::Device::ES::Context context;
  if (es->ImportTicket(wad.GetTicket().GetRawTicket()) < 0 ||
      es->ImportTitleInit(context, tmd.GetRawTMD()) < 0)
  {
    PanicAlertT("WAD installation failed: Could not initialise title import.");
    return false;
  }

  const bool contents_imported = [&]() {
    const u64 title_id = tmd.GetTitleId();
    for (const IOS::ES::Content& content : tmd.GetContents())
    {
      const std::vector<u8> data = wad.GetContent(content.index);

      if (es->ImportContentBegin(context, title_id, content.id) < 0 ||
          es->ImportContentData(context, 0, data.data(), static_cast<u32>(data.size())) < 0 ||
          es->ImportContentEnd(context, 0) < 0)
      {
        PanicAlertT("WAD installation failed: Could not import content %08x.", content.id);
        return false;
      }
    }
    return true;
  }();

  if ((contents_imported && es->ImportTitleDone(context) < 0) ||
      (!contents_imported && es->ImportTitleCancel(context) < 0))
  {
    PanicAlertT("WAD installation failed: Could not finalise title import.");
    return false;
  }

  DiscIO::CNANDContentManager::Access().ClearCache();
  return true;
}
}
