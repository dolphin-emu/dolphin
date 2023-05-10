// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24
{

constexpr const char SEND_BOX_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                       "/wc24send.mbx";
class WC24SendList final
{
public:
  explicit WC24SendList(std::shared_ptr<FS::FileSystem> fs);
  void ReadSendList();
  bool CheckSendList() const;
  void WriteSendList() const;
  ErrorCode DeleteMessage(const u32 index);
  u32 GetNumberOfMail() const;
  std::string_view GetMailFlag() const;
  std::vector<u32> GetPopulatedEntries() const;
  u32 GetMailSize(const u32 index) const;
  std::string GetMailPath(const u32 index) const;
  u32 GetIndex(const u32 index) const;

private:
  static constexpr u32 SEND_LIST_MAGIC = 0x57635466;  // WcTf
  static constexpr u32 MAX_ENTRIES = 127;

#pragma pack(push, 1)
  struct SendListHeader final
  {
    u32 magic;    // 'WcTf' 0x57635466
    u32 version;  // 4 in Wii Menu 4.x
    u32 number_of_mail;
    u32 total_entries;
    u32 total_size_of_messages;
    u32 filesize;
    u32 next_index;
    u32 next_index_offset;
    u32 unk2;
    u32 vff_free_space;
    u8 unk3[48];
    char mail_flag[40];
  };

  struct SendListEntry final
  {
    u32 index;
    u32 unk1 [[maybe_unused]];
    u32 mail_size;
    u8 _[116];
  };

  struct SendList final
  {
    SendListHeader header;
    SendListEntry entries[MAX_ENTRIES];
  };
#pragma pack(pop)

  SendList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
};
}  // namespace NWC24
}  // namespace IOS::HLE
