// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"
#include "Core/IOS/Network/KD/Mail/MailCommon.h"
#include "Core/IOS/Network/KD/Mail/WC24FriendList.h"
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

  void ReadSendList();
  bool CheckSendList() const;
  void WriteSendList() const;

  ErrorCode AddRegistrationMessages(const WC24FriendList& friend_list, u64 from);

  u32 GetNextEntryId() const;
  u32 GetNextEntryIndex() const;
  u32 GetTotalEntries() const;
  void InitEntry(u32 entry_index, u64 from);
  void SetPackedTo(u32 index, u32 offset, u32 size);

  ErrorCode DeleteMessage(const u32 index);
  u32 GetNumberOfMail() const;
  std::string_view GetMailFlag() const;
  std::vector<u32> GetPopulatedEntries() const;
  u32 GetMailSize(const u32 index) const;
  std::string GetMailPath(const u32 index) const;
  u32 GetIndex(const u32 index) const;
  static std::string GetUTCTime();

private:
  static constexpr u32 MAX_ENTRIES = 127;
  static constexpr char MAIL_REGISTRATION_STRING[] =
      "MAIL FROM: {}\r\nRCPT TO: {}\r\nDATA\r\nDate: Fri, 19 Nov 2021 00:00:00 GMT\r\nFrom: "
      "{}\r\nTo: {}\r\nMessage-Id: "
      "lol\r\nSubject: WC24 Cmd Message\r\nX-Wii-AppId: 0-00000001-0001\r\nX-Wii-Cmd: "
      "80010001\r\nMIME-Version: 1.0\r\nContent-Type: multipart/mixed;\r\n "
      "boundary=\"Boundary-NWC24-041B6CE500012\"\r\n--Boundary-NWC24-041B6CE500012\r\nContent-Type:"
      " text/plain; charset=us-ascii\r\nContent-Transfer-Encoding: 7bit\r\nWC24 Cmd "
      "Message\r\n--Boundary-NWC24-041B6CE500012\r\nContent-Type: application/octet-stream;\r\n "
      "name=a0000018.dat\r\nContent-Transfer-Encoding: base64\r\nContent-Disposition: "
      "attachment;\r\n "
      "filename=a0000018.dat\r\n\r\n "
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
      "\r\n\r\n--Boundary-NWC24-041B6CE500012--";

#pragma pack(push, 1)
  struct SendList final
  {
    MailListHeader header;
    MailListEntry entries[MAX_ENTRIES];
  };
#pragma pack(pop)

  SendList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
};
}  // namespace NWC24::Mail
}  // namespace IOS::HLE
