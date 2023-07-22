// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "MultipartParser.h"

#include "Core/IOS/Network/KD/Mail/WC24FriendList.h"
#include "Core/IOS/Network/KD/Mail/WC24Receive.h"
#include "Core/IOS/Network/KD/NWC24Config.h"

#include <map>
#include <regex>
#include <vector>

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}
namespace NWC24::Mail
{
class MailParser final
{
public:
  MailParser(const std::string& boundary, u32 num_of_mail, const WC24FriendList friend_list);

  /// Used with the mail flag
  enum class ContentType : u32
  {
    Plain = 0x10000,
    HTML = 0x10001,
    Binary = 0x30000,
    MessageBoard = 0x30001,
    WiiMini = 0x30002,
    Jpeg = 0x20000,
    WiiPicture = 0x20001,
    Mixed = 0xF0000,
    Alt = 0xF0001,
    Related = 0xF0002
  };

  enum class IsMultipart : bool;

  ErrorCode Parse(const std::string& buf);
  ErrorCode ParseContentTransferEncoding(u32 index, u32 receive_index,
                                         WC24ReceiveList& receive_list,
                                         IsMultipart is_multipart = IsMultipart{false}) const;
  ErrorCode ParseContentType(u32 index, u32 receive_index, WC24ReceiveList& receive_list,
                             IsMultipart is_multipart = IsMultipart{false});
  ErrorCode ParseDate(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseFrom(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  void ParseSubject(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseTo(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseWiiAppId(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;
  ErrorCode ParseWiiCmd(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const;

  std::vector<u8> GetMessageData(u32 index) const;
  std::string GetMessage(u32 index, IsMultipart is_multipart = IsMultipart{false}) const;
  std::string GetFullMessage(u32 index) const;
  std::string GetHeaderValue(u32 index, std::string_view key,
                             IsMultipart is_multipart = IsMultipart{false}) const;
  u32 GetHeaderLength(u32 index) const;

private:
  static void EmptyCallback(const char* buffer, size_t start, size_t end, void* user_data){};
  ErrorCode ParseMultipartField(const MailParser* parent, u32 parent_index, u32 multipart_index,
                                u32 receive_index, WC24ReceiveList& receive_list);

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

  using Header = std::pair<std::string_view, std::string_view>;
  using Headers = std::vector<Header>;

  MultipartParser m_parser{};
  WC24FriendList m_friend_list;
  std::vector<std::string> m_message_data;
  std::vector<Headers> m_headers;
  std::regex m_wii_number_regex{"w\\d{16}"};
  u32 current_index{};
  u32 current_header{};
  std::string m_charset{};
  std::string m_content_type_str{};
  ContentType m_content_type{};
};
}  // namespace NWC24::Mail
}  // namespace IOS::HLE
