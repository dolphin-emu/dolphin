// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string_view>
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

  bool ReadSendList();
  bool CheckSendList() const;
  void WriteSendList() const;
  bool IsDisabled() const;

  std::string_view GetMailFlag() const;
  u32 GetNumberOfMail() const;
  std::vector<u32> GetMailToSend() const;
  u32 GetEntryId(u32 entry_index) const;
  u32 GetMailSize(u32 index) const;
  ErrorCode DeleteMessage(u32 index);
  std::string GetMailPath(u32 index) const;
  u32 GetNextEntryId() const;
  u32 GetNextEntryIndex() const;
  std::optional<u32> GetNextFreeEntryIndex() const;

  ErrorCode AddRegistrationMessages(const WC24FriendList& friend_list, u64 sender);

private:
  static constexpr u32 MAX_ENTRIES = 127;
  static constexpr u32 SEND_LIST_SIZE = 16384;

  // Format for the message Wii Mail sends when trying to register a Wii Friend.
  // Most fields can be static such as the X-Wii-AppId which is the Wii Menu,
  // X-Wii-Cmd which is the registration command, and the attached file which is
  // just 128 bytes of base64 encoded 0 bytes. That file is supposed to be friend profile data which
  // is written to nwc24fl.bin, although it has been observed to always be 0.
  static constexpr char MAIL_REGISTRATION_STRING[] =
      "MAIL FROM: {0:016d}@wii.com\r\n"
      "RCPT TO: {1:016d}wii.com\r\n"
      "DATA\r\n"
      "Date: {2:%a, %d %b %Y %X} GMT\r\n"
      "From: {0:016d}@wii.com\r\n"
      "To: {1:016d}@wii.com\r\n"
      "Message-Id: <00002000B0DF6BB47FE0303E0DB0D@wii.com>\r\n"
      "Subject: WC24 Cmd Message\r\n"
      "X-Wii-AppId: 0-00000001-0001\r\n"
      "X-Wii-Cmd: 80010001\r\n"
      "MIME-Version: 1.0\r\n"
      "Content-Type: multipart/mixed;\r\n "
      "boundary=\"Boundary-NWC24-041B6CE500012\"\r\n"
      "--Boundary-NWC24-041B6CE500012\r\n"
      "Content-Type: text/plain; charset=us-ascii\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "WC24 Cmd Message\r\n"
      "--Boundary-NWC24-041B6CE500012\r\n"
      "Content-Type: application/octet-stream;\r\n "
      "name=a0000018.dat\r\n"
      "Content-Transfer-Encoding: base64\r\n"
      "Content-Disposition: attachment;\r\n "
      "filename=a0000018.dat\r\n\r\n "
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
      "\r\n\r\n"
      "--Boundary-NWC24-041B6CE500012--";

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
