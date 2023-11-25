// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/Network/KD/Mail/MailCommon.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24::Mail
{

constexpr const char SEND_BOX_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                       "/wc24send.mbx";
class WC24SendList final
{
public:
  explicit WC24SendList(std::shared_ptr<FS::FileSystem> fs);

  bool ReadSendList();
  bool CheckSendList() const;
  void WriteSendList() const;
  bool IsDisabled() const;

  std::string_view GetMailFlag() const;

private:
  static constexpr u32 MAX_ENTRIES = 127;
  static constexpr u32 SEND_LIST_SIZE = 16384;

#pragma pack(push, 1)
  struct SendList final
  {
    MailListHeader header;
    std::array<MailListEntry, MAX_ENTRIES> entries;
  };
  static_assert(sizeof(SendList) == SEND_LIST_SIZE);
#pragma pack(pop)

  SendList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
  bool m_is_disabled = false;
};
}  // namespace NWC24::Mail
}  // namespace IOS::HLE
