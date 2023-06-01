// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/MailParser.h"
#include "Common/Align.h"
#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#include <chrono>
#include <regex>

namespace IOS::HLE::NWC24
{
MailParser::MailParser(const std::string& boundary, const u32 num_of_mail,
                       const WC24FriendList friend_list)
    : m_friend_list(friend_list), m_message_data(num_of_mail + 1)
{
  m_parser.setBoundary(boundary);
  m_parser.onPartBegin = EmptyCallback;
  m_parser.onHeaderField = EmptyCallback;
  m_parser.onHeaderValue = EmptyCallback;
  m_parser.onPartData = [&](const char* buf, size_t start, size_t end, void* user_data) {
    m_message_data[current_index].append(std::string(buf + start, end - start));
  };
  m_parser.onPartEnd = [&](const char* buf, size_t start, size_t end, void* user_data) {
    current_index++;
  };
  m_parser.onEnd = EmptyCallback;
}

ErrorCode MailParser::Parse(const std::string& buf)
{
  m_parser.feed(reinterpret_cast<const char*>(buf.data()), buf.size());

  std::string_view err = {m_parser.getErrorMessage()};
  if (err != "No error.")
  {
    ERROR_LOG_FMT(IOS_WC24, "Mail parser failed with error: {}", err);
    return WC24_ERR_FATAL;
  }

  return WC24_OK;
}

std::vector<u8> MailParser::GetMessageData(u32 index) const
{
  std::vector<u8> data{m_message_data[index].begin(), m_message_data[index].end()};
  data.resize(Common::AlignUp(data.size(), 32));
  return data;
}

std::string MailParser::GetHeaderValue(u32 index, const std::string& key) const
{
  std::string val{};
  const std::vector<std::string> raw_fields = SplitString(m_message_data[index], '\n');
  for (const std::string& field : raw_fields)
  {
    const std::vector<std::string> key_value = SplitString(field, ':');
    if (key_value[0] == key)
    {
      val = StripWhitespace(key_value[1]);
      break;
    }
  }

  return val;
}

u32 MailParser::GetHeaderLength(u32 index) const
{
  return m_message_data[index].find("\r\n\r") + 4;
}

ErrorCode MailParser::ParseContentTransferEncoding(u32 index, u32 receive_index,
                                                   WC24ReceiveList& receive_list) const
{
  const std::string str_transfer_enc = GetHeaderValue(index, "Content-Transfer-Encoding");
  const std::string message =
      m_message_data[index].substr(GetHeaderLength(index), m_message_data[index].size());
  const u32 message_length = message.size();
  if (str_transfer_enc == "7bit" || str_transfer_enc == "8bit")
  {
    receive_list.SetEncodedMessageLength(receive_index, message_length);
    receive_list.SetMessageLength(receive_index, message_length);
  }
  else if (str_transfer_enc == "base64")
  {
    const u32 padding = std::count(message.begin(), message.end(), '=');
    receive_list.SetEncodedMessageLength(receive_index, message_length);
    receive_list.SetMessageLength(
        receive_index,
        static_cast<u32>(std::floor(StripWhitespace(message).size() / 4 * 3)) - padding);
  }
  else
  {
    // TODO: Implement quoted-printable
    return WC24_ERR_NOT_SUPPORTED;
  }

  const u32 offset = m_message_data[index].find("Content-Transfer-Encoding") + 27;
  const u32 size = str_transfer_enc.size();
  receive_list.SetPackedContentTransferEncoding(receive_index, offset, size + 1);

  return WC24_OK;
}

ErrorCode MailParser::ParseContentType(u32 index, u32 receive_index,
                                       IOS::HLE::NWC24::WC24ReceiveList& receive_list) const
{
  const std::string str_type = GetHeaderValue(index, "Content-Type");
  if (str_type.empty())
    return WC24_ERR_FORMAT;

  const std::vector<std::string> values = SplitString(str_type, ';');

  std::string str_content_type = values[0];
  Common::ToLower(&str_content_type);
  const auto content_type_iter = m_content_types.find(str_content_type);
  if (content_type_iter == m_content_types.end())
    return WC24_ERR_NOT_SUPPORTED;

  const ContentType content_type = content_type_iter->second;
  if (content_type == ContentType::Plain)
  {
    const std::string charset = values[1].substr(9, values[1].size());
    const u32 offset = m_message_data[index].find("Content-Type:") + 14 + values[0].size() + 10;
    receive_list.SetPackedCharset(receive_index, offset, charset.size() + 1);
    receive_list.UpdateFlag(receive_index, 0xfffeffff, WC24ReceiveList::AND);
  }
  else if (content_type == ContentType::Alt || content_type == ContentType::Mixed ||
           content_type == ContentType::Related)
  {
    if (values.size() != 2)
    {
      return WC24_ERR_FORMAT;
    }

    // TODO: These types are nested multipart, parse them.
    // From a quick observance, messages from actual emails are these types.
  }
  else
  {
    // TODO: Everything else; does something with the currently unknown fields.
    receive_list.UpdateFlag(receive_index, 0xfffeffff, WC24ReceiveList::AND);
  }

  return WC24_OK;
}

ErrorCode MailParser::ParseDate(u32 index, u32 receive_index,
                                IOS::HLE::NWC24::WC24ReceiveList& receive_list) const
{
  // The date that it wants is minutes since January 1st 1900.
  const std::string date = GetHeaderValue(index, "Date");
  if (date.empty())
    return WC24_ERR_FORMAT;

  std::tm time{};
  std::istringstream ss(date);

  ss >> std::get_time(&time, "%d %b %Y %T %z");
  const u32 seconds_since_1900 = mktime(&time) + MINUTES_FROM_EPOCH_TO_1900;
  receive_list.SetTime(receive_index, static_cast<u32>(std::floor(seconds_since_1900 / 60)));

  return WC24_OK;
}

ErrorCode MailParser::ParseFrom(u32 index, u32 receive_index,
                                NWC24::WC24ReceiveList& receive_list) const
{
  u64 friend_code{};
  const std::string str_friend = GetHeaderValue(index, "From");
  if (str_friend.empty())
    return WC24_ERR_FORMAT;

  // Determine if this is a Wii sender or email.
  if (std::regex_search(str_friend, m_wii_number_regex))
  {
    // Wii
    const std::string str_wii_friend = str_friend.substr(1, 16);

    const bool did_parse = TryParse(str_wii_friend, &friend_code);
    if (!did_parse)
    {
      ERROR_LOG_FMT(IOS_WC24, "Invalid friend code.");
      return WC24_ERR_FORMAT;
    }
  }
  else
  {
    // Email
    friend_code = WC24FriendList::ConvertEmailToFriendCode(str_friend);
  }

  if (!m_friend_list.CheckFriend(Common::swap64(friend_code)))
  {
    NOTICE_LOG_FMT(IOS_WC24, "Received message from someone who is not a friend; discarding.");
    return WC24_ERR_NOT_FOUND;
  }
  
  receive_list.SetFromFriendCode(receive_index, friend_code);

  const u32 from_address_size = str_friend.size();
  const u32 offset = m_message_data[index].find("From:") + 6;
  receive_list.SetPackedFrom(receive_index, offset, from_address_size);

  return WC24_OK;
}

void MailParser::ParseSubject(u32 index, u32 receive_index,
                              NWC24::WC24ReceiveList& receive_list) const
{
  const std::string subject = GetHeaderValue(index, "Subject");
  if (subject.empty())
    return;

  const u32 offset = m_message_data[index].find("Subject") + 9;
  receive_list.SetPackedSubject(receive_index, offset, subject.size());
}

ErrorCode MailParser::ParseTo(u32 index, u32 receive_index,
                              NWC24::WC24ReceiveList& receive_list) const
{
  const std::string to = GetHeaderValue(index, "To");
  if (to.empty())
    return WC24_ERR_FORMAT;

  const u32 offset = m_message_data[index].find("To") + 4;
  receive_list.SetPackedTo(receive_index, offset, to.size());

  return WC24_OK;
}

ErrorCode MailParser::ParseWiiAppId(u32 index, u32 receive_index,
                                    NWC24::WC24ReceiveList& receive_list) const
{
  const std::string full_id = GetHeaderValue(index, "X-Wii-AppId");
  if (full_id.empty())
    return WC24_OK;

  // Sanity checks to make sure everything is valid.
  const char s = full_id.at(0);
  if (full_id.size() != 15 || s - 48 > 4 || full_id.at(1) != '-' || full_id.at(10) != '-')
    return WC24_ERR_FORMAT;

  if ((s & 1) != 0)
    receive_list.UpdateFlag(receive_index, 4, WC24ReceiveList::OR);

  if (Common::ExtractBit(s, 1) != 0)
    receive_list.UpdateFlag(receive_index, 8, WC24ReceiveList::OR);

  // Determine the app ID.
  u32 app_id = std::stoul(full_id.substr(2, 10), nullptr, 16);
  receive_list.SetWiiAppId(receive_index, app_id);

  // Now the group id.
  u32 group_id = std::stoul(full_id.substr(11, 14), nullptr, 16);
  receive_list.SetWiiAppGroupId(receive_index, group_id);

  return WC24_OK;
}
ErrorCode MailParser::ParseWiiCmd(u32 index, u32 receive_index, WC24ReceiveList& receive_list) const
{
  std::string str_cmd = GetHeaderValue(index, "X-Wii-Cmd");
  if (str_cmd.empty())
    return WC24_OK;

  if (str_cmd.size() != 8)
    return WC24_ERR_FORMAT;

  u32 cmd = std::stoul(str_cmd, nullptr, 16);
  receive_list.SetWiiCmd(receive_index, cmd);

  return WC24_OK;
}
}  // namespace IOS::HLE::NWC24
