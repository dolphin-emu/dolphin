// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "MultipartParser.h"

#include "Core/IOS/Network/KD/Mail/WC24FriendList.h"
#include "Core/IOS/Network/KD/Mail/WC24Receive.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

#include <map>
#include <vector>

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24
{
class MailParser final
{
public:
  MailParser(const std::string& boundary, u32 num_of_mail, const WC24FriendList friend_list);

  ErrorCode Parse(const std::string& buf);
  ErrorCode ParseContentTransferEncoding(u32 index, u32 receive_index,
                                         WC24ReceiveList& receive_list) const;
  ErrorCode ParseContentType(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseDate(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseFrom(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  void ParseSubject(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseTo(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseWiiAppId(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseWiiCmd(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;

  std::vector<u8> GetMessageData(u32 index) const;
  std::string GetHeaderValue(u32 index, const std::string& key) const;
  u32 GetHeaderLength(u32 index) const;

private:
  enum class ContentType
  {
    Binary,
    WiiMini,
    MessageBoard,
    Jpeg,
    WiiPicture,
    Alt,
    Mixed,
    Related,
    HTML,
    Plain
  };

  static void EmptyCallback(const char* buffer, size_t start, size_t end, void* user_data){};

  // Minutes from January 1st 1900 to Unix epoch.
  static constexpr u32 MINUTES_FROM_EPOCH_TO_1900 = 2208988800;

  const std::map<std::string, ContentType> m_content_types = {
      {"application/octet-stream", ContentType::Binary},
      {"application/x-wii-minidata", ContentType::WiiMini},
      {"application/x-wii-msgboard", ContentType::MessageBoard},
      {"image/jpeg", ContentType::Jpeg},
      {"image/x-wii-picture", ContentType::WiiPicture},
      {"multipart/alternative", ContentType::Alt},
      {"multipart/mixed", ContentType::Mixed},
      {"multipart/related", ContentType::Related},
      {"text/html", ContentType::HTML},
      {"text/plain", ContentType::Plain},
  };

  MultipartParser parser{};
  WC24FriendList m_friend_list;
  std::vector<std::string> m_message_data;
  s32 current_index{};
};
}  // namespace NWC24
}  // namespace IOS::HLE
