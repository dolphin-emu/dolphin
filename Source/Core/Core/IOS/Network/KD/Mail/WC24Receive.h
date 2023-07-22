// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

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
constexpr const char RECEIVE_BOX_PATH[] = "/" WII_WC24CONF_DIR "/mbox"
                                          "/wc24recv.mbx";

constexpr char REGISTRATION_MESSAGE_TEXT[] =
    "MAIL FROM: {}\r\nRCPT TO: {}\r\nDATA\r\nDate: {}\r\nFrom: {}\r\nTo: {}\r\nMessage-Id: "
    "{}\r\nX-Wii-AppId: {}\r\nX-Wii-Cmd: {}\r\nContent-Type: text/plain; "
    "charset=us-ascii\r\nContent-Transfer-Encoding: 7bit\r\n\r\nWC24 Cmd Message";

class WC24ReceiveList final
{
public:
  static std::string GetMailPath(u32 index);

  explicit WC24ReceiveList(std::shared_ptr<FS::FileSystem> fs);
  void ReadReceiveList();
  bool CheckReceiveList() const;
  void WriteReceiveList() const;

  u32 GetIndex(u32 index) const;
  u32 GetNextEntryId() const;
  u32 GetNextEntryIndex() const;
  u32 GetAppID(u32 index) const;

  void FinalizeEntry(u32 index);
  void ClearEntry(u32 index);
  void InitFlag(u32 index);
  void UpdateFlag(u32 index, u32 value, FlagOP op);
  void SetMessageId(u32 index, u32 id);
  void SetMessageSize(u32 index, u32 size);
  void SetHeaderLength(u32 index, u32 size);
  void SetMessageOffset(u32 index, u32 offset);
  void SetEncodedMessageLength(u32 index, u32 size);
  void SetMessageLength(u32 index, u32 size);
  void SetPackedContentTransferEncoding(u32 index, u32 offset, u32 size);
  void SetPackedCharset(u32 index, u32 offset, u32 size);
  void SetMultipartField(u32 index, u32 multipart_index, u32 offset, u32 size);
  void SetMultipartContentType(u32 index, u32 multipart_index, u32 type);
  void SetMultipartSize(u32 index, u32 multipart_index, u32 size);
  void SetTime(u32 index, u32 time);
  void SetFromFriendCode(u32 index, u64 friend_code);
  void SetPackedFrom(u32 index, u32 offset, u32 size);
  void SetPackedSubject(u32 index, u32 offset, u32 size);
  void SetPackedTo(u32 index, u32 offset, u32 size);
  void SetWiiAppId(u32 index, u32 id);
  void SetWiiAppGroupId(u32 index, u16 id);
  void SetWiiCmd(u32 index, u32 cmd);

private:
  static constexpr u32 MAX_ENTRIES = 255;

#pragma pack(push, 1)
  struct ReceiveList final
  {
    MailListHeader header;
    MailListEntry entries[MAX_ENTRIES];
  };
#pragma pack(pop)

  ReceiveList m_data;
  std::shared_ptr<FS::FileSystem> m_fs;
};
}  // namespace NWC24::Mail
}  // namespace IOS::HLE
