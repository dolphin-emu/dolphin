// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/KD/Mail/MailParser.h"
#include "Common/Align.h"
#include "Common/BitUtils.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/IOS/Network/KD/Mail/MailCommon.h"

#include <chrono>
#include <regex>

namespace IOS::HLE::NWC24::Mail
{
MailParser::MailParser(const std::string& boundary, const u32 num_of_mail,
                       WC24ReceiveList* receive_list)
    : m_receive_list(receive_list), m_message_data(num_of_mail + 1), m_headers(num_of_mail + 1)
{
  m_parser.setBoundary(boundary);
  m_parser.onPartBegin = EmptyCallback;
  m_parser.onHeaderField = [&](const char* buf, size_t start, size_t end, void* user_data) {
    const Header header_key = std::make_pair(std::string(buf + start, end - start), std::string());
    m_headers[current_index].push_back(header_key);
  };
  m_parser.onHeaderValue = [&](const char* buf, size_t start, size_t end, void* user_data) {
    m_headers[current_index][current_header].second = std::string(buf + start, end - start);
  };
  m_parser.onHeaderEnd = [&](const char* buf, size_t start, size_t end, void* user_data) {
    current_header++;
  };
  m_parser.onPartData = [&](const char* buf, size_t start, size_t end, void* user_data) {
    m_message_data[current_index].append(std::string(buf + start, end - start));
  };
  m_parser.onPartEnd = [&](const char* buf, size_t start, size_t end, void* user_data) {
    current_index++;
    current_header = 0;
  };
  m_parser.onEnd = EmptyCallback;
}

ErrorCode MailParser::Parse(std::string_view buf)
{
  m_parser.feed(reinterpret_cast<const char*>(buf.data()), buf.size());

  if (m_parser.hasError())
  {
    ERROR_LOG_FMT(IOS_WC24, "Mail parser failed with error: {}", m_parser.getErrorMessage());
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

std::string MailParser::GetHeaderValue(u32 index, std::string_view key,
                                       IsMultipart is_multipart) const
{
  // Multipart fields are parsed in a way that allow for the headers to be stored in a pair where
  // we don't need to do any string parsing. The raw message on the other hand doesn't do that
  // because we require the entire message to get offsets of many fields.
  if (is_multipart == IsMultipart{true})
  {
    for (const auto& [name, value] : m_headers[index])
    {
      if (name == key)
      {
        return std::string{value};
      }
    }

    return {};
  }
  else
  {
    std::string val{};
    const std::vector<std::string> raw_fields = SplitString(m_message_data[index], '\n');
    for (u32 i = 0; i < raw_fields.size(); i++)
    {
      std::vector<std::string> key_value = SplitString(raw_fields[i], ':');
      if (Common::CaseInsensitiveEquals(key_value[0], key))
      {
        // There should always be a key value pair if the above is true.
        if (key_value.size() < 2)
          return {};

        // The To header acts differently as there can be multiple recipients
        // Each recipient is on a new line, seperated by a comma.
        // Once we have reached a recipient with no comma, we are done.
        if (key == "To" && key_value[1].find(',') != std::string::npos)
        {
          // Append the first recipient.
          // There is a space at the beginning we need to get rid of
          val += key_value[1].substr(1) + '\n';

          for (u32 j = i + 1; j < raw_fields.size() - i + 1; j++)
          {
            val += raw_fields[j] + '\n';

            if (raw_fields[j].find(',') == std::string::npos)
            {
              // Remove CRLF newlines and break
              val.erase(val.size() - 2);
              break;
            }
          }
          break;
        }

        // Remove the header key and join the rest of the strings
        key_value.erase(key_value.begin());
        val = StripWhitespace(JoinStrings(key_value, ":"));
        break;
      }
    }

    return val;
  }
}

u32 MailParser::GetHeaderLength(u32 index) const
{
  return m_message_data[index].find("\r\n\r") + 4;
}

std::string MailParser::GetMessage(u32 index, IsMultipart is_multipart) const
{
  if (is_multipart == IsMultipart{true})
    return m_message_data[index];

  return m_message_data[index].substr(GetHeaderLength(index), m_message_data[index].size());
}

std::string MailParser::GetFullMessage(u32 index) const
{
  return m_message_data[index];
}

ErrorCode MailParser::ParseContentTransferEncoding(u32 index, u32 receive_index,
                                                   IsMultipart is_multipart) const
{
  std::string message = GetMessage(index, is_multipart);
  u32 message_length = message.size();

  const std::string str_transfer_enc =
      GetHeaderValue(index, "Content-Transfer-Encoding", is_multipart);
  if (str_transfer_enc.empty())
  {
    // If it is empty the Wii will assume either 7-bit or 8-bit.
    m_receive_list->SetMessageLength(receive_index, message_length);
    m_receive_list->SetEncodedMessageLength(receive_index, message_length);

    // We will return not found to tell us that the field does not exist.
    // This ensures we do not set the content transfer encoding field in multipart.
    return WC24_ERR_NOT_FOUND;
  }

  if (str_transfer_enc == "7bit" || str_transfer_enc == "8bit")
  {
    m_receive_list->SetEncodedMessageLength(receive_index, message_length);
    m_receive_list->SetMessageLength(receive_index, message_length);
  }
  else if (str_transfer_enc == "base64")
  {
    m_receive_list->SetEncodedMessageLength(receive_index, message_length);

    // Remove the newlines and base64 padding.
    const u32 padding = std::count(message.begin(), message.end(), '=');
    message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());
    message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
    message_length = message.size() - padding;

    m_receive_list->SetMessageLength(receive_index, (message_length * 3) / 4);
  }
  else
  {
    // TODO (Sketch): Implement quoted-printable. I do not know what app(s) uses this encoding, but
    // it is supported by KD.
    return WC24_ERR_NOT_SUPPORTED;
  }

  if (is_multipart != IsMultipart{true})
  {
    // We don't have the required data to do this for multipart.
    const u32 offset = m_message_data[index].find("Content-Transfer-Encoding") + 27;
    const u32 size = str_transfer_enc.size();
    m_receive_list->SetPackedContentTransferEncoding(receive_index, offset, size);
  }

  return WC24_OK;
}

ErrorCode MailParser::ParseContentType(u32 index, u32 receive_index, IsMultipart is_multipart)
{
  const std::string str_type = GetHeaderValue(index, "Content-Type", is_multipart);
  if (str_type.empty())
    return WC24_ERR_FORMAT;

  const std::vector<std::string> values = SplitString(str_type, ';');

  std::string str_content_type = values[0];
  Common::ToLower(&str_content_type);
  const auto content_type_iter = m_content_types.find(str_content_type);
  if (content_type_iter == m_content_types.end())
    return WC24_ERR_NOT_SUPPORTED;

  m_content_type_str = content_type_iter->first;
  m_content_type = content_type_iter->second;
  if (m_content_type == ContentType::Plain)
  {
    if (values.size() < 2)
      return WC24_ERR_NOT_SUPPORTED;

    if (is_multipart != IsMultipart{true})
      m_receive_list->UpdateFlag(receive_index, 0xfffeffff, FlagOP::And);

    m_charset = values[1].substr(9);
    if (is_multipart == IsMultipart{true})
      return WC24_OK;

    const u32 offset = GetFullMessage(index).find("Content-Type:") + str_content_type.size() + 24;
    m_receive_list->SetPackedCharset(receive_index, offset, m_charset.size());
  }
  else if (m_content_type == ContentType::Alt || m_content_type == ContentType::Mixed ||
           m_content_type == ContentType::Related)
  {
    if (is_multipart != IsMultipart{true})
      m_receive_list->UpdateFlag(receive_index, 0x10000, FlagOP::Or);

    std::string boundary_val{};
    if (values.size() < 2)
    {
      // With emails, this typically evaluates to false. On the occasion that we receive a multipart
      // message from a Wii, the boundary is stored two lines below the content type. A much safer
      // way to get it is to search for it.
      const u32 boundary_pos = GetFullMessage(index).find("boundary=\"");
      boundary_val = m_message_data[index].substr(boundary_pos + 10);
      boundary_val = boundary_val.substr(0, boundary_val.find('"'));
    }
    else
    {
      boundary_val = values[1];
      boundary_val.erase(0, 11);
      boundary_val.erase(boundary_val.end() - 1, boundary_val.end());
    }

    const std::string boundary = fmt::format("--{}", boundary_val);
    MailParser parser{boundary_val, 2, m_receive_list};

    // The multipart parser is very particular, we need to find `--boundary` and pass only that.
    std::string multipart_message = m_message_data[index];
    const size_t multipart_offset = multipart_message.find(boundary);
    if (multipart_offset == std::string::npos)
      return WC24_ERR_FORMAT;

    parser.Parse(multipart_message.substr(multipart_offset));
    // The receive list has space for 3 multipart fields. If there are more in the message, they
    // will be ignored.
    for (int i = 0; i < 3; i++)
    {
      ErrorCode reply = parser.ParseMultipartField(this, index, i, receive_index);
      if (reply != WC24_OK)
      {
        return reply;
      }
    }
  }
  else
  {
    if (is_multipart != IsMultipart{true})
      m_receive_list->UpdateFlag(receive_index, 0xfffeffff, FlagOP::And);
  }

  return WC24_OK;
}

ErrorCode MailParser::ParseMultipartField(const MailParser* parent, u32 parent_index,
                                          u32 multipart_index, u32 receive_index)
{
  if (m_headers[multipart_index].empty())
    return WC24_OK;

  // The first multipart field is written to the base fields. The rest are put in their special
  // fields.
  if (multipart_index == 0)
  {
    const u32 offset = parent->GetFullMessage(parent_index).find(m_parser.boundary);
    ErrorCode err = ParseContentTransferEncoding(multipart_index, receive_index, IsMultipart{true});
    if (err != WC24_OK && err != WC24_ERR_NOT_FOUND)
      return err;

    // Set the transfer encoding if needed.
    if (err == WC24_OK)
    {
      const u32 content_encoding_offset =
          parent->GetFullMessage(parent_index).find("Content-Transfer-Encoding", offset) + 27;
      const u32 size =
          GetHeaderValue(multipart_index, "Content-Transfer-Encoding", IsMultipart{true}).size();
      m_receive_list->SetPackedContentTransferEncoding(receive_index, content_encoding_offset,
                                                       size);
    }

    err = ParseContentType(multipart_index, receive_index, IsMultipart{true});
    if (err != WC24_OK)
      return err;

    // Set the content type
    const u32 content_type_size = m_charset.size();
    const u32 content_type_offset =
        parent->GetFullMessage(parent_index).find("Content-Type", offset) +
        m_content_type_str.size() + 24;
    m_receive_list->SetPackedCharset(receive_index, content_type_offset, content_type_size);

    // Finally the message offset.
    m_receive_list->SetMessageOffset(
        receive_index,
        parent->GetFullMessage(parent_index).find(GetMessage(multipart_index, IsMultipart{true})));
    return WC24_OK;
  }

  const u32 offset =
      parent->GetFullMessage(parent_index).find(GetMessage(multipart_index, IsMultipart{true}));

  const ErrorCode err = ParseContentType(multipart_index, receive_index, IsMultipart{true});
  if (err != WC24_OK)
    return err;

  m_receive_list->SetMultipartContentType(receive_index, multipart_index - 1,
                                          static_cast<u32>(m_content_type));

  std::string message = GetMessage(multipart_index, IsMultipart{true});
  m_receive_list->SetMultipartField(receive_index, multipart_index - 1, offset, message.size());

  // Remove the newlines and base64 padding.
  const u32 padding = std::count(message.begin(), message.end(), '=');
  message.erase(std::remove(message.begin(), message.end(), '\r'), message.end());
  message.erase(std::remove(message.begin(), message.end(), '\n'), message.end());
  const u32 message_length = message.size() - padding;

  m_receive_list->SetMultipartSize(receive_index, multipart_index - 1, (message_length * 3) / 4);

  return WC24_OK;
}

void MailParser::ParseDate(u32 index, u32 receive_index) const
{
  // The date that it wants is minutes since January 1st 1900.
  const std::string date = GetHeaderValue(index, "Date");
  if (date.empty())
    return;

  std::tm time{};
  std::istringstream ss(date);

  ss >> std::get_time(&time, "%d %b %Y %T %z");
  const u32 seconds_since_1900 = mktime(&time) + MINUTES_FROM_EPOCH_TO_1900;
  m_receive_list->SetTime(receive_index, static_cast<u32>(std::floor(seconds_since_1900 / 60)));
}

ErrorCode MailParser::ParseFrom(u32 index, u32 receive_index, WC24FriendList& friend_list) const
{
  u64 friend_code{};
  u64 value{};
  const std::string str_friend = GetHeaderValue(index, "From");
  if (str_friend.empty())
    return WC24_ERR_FORMAT;

  // Determine if this is a Wii sender or email.
  if (std::regex_search(str_friend, m_wii_number_regex))
  {
    friend_code = std::stoull(str_friend.substr(1, 16), nullptr, 10);
    value = friend_code;
  }
  else
  {
    // For emails, the friend code stored in the nwc24fl.bin differs from the value we need to set.
    friend_code = WC24FriendList::ConvertEmailToFriendCode(str_friend);
    value = u64{GetFullMessage(index).find("From") + 2} << 32 | static_cast<u64>(str_friend.size());
  }

  if (!friend_list.IsFriend(Common::swap64(friend_code)) && friend_code != NINTENDO_FRIEND_CODE)
  {
    WARN_LOG_FMT(IOS_WC24, "Received message from someone who is not a friend, discarding.");
    return WC24_ERR_NOT_FOUND;
  }

  m_receive_list->SetFromFriendCode(receive_index, value);

  const u32 from_address_size = str_friend.size();
  const u32 offset = m_message_data[index].find("From:") + 6;
  m_receive_list->SetPackedFrom(receive_index, offset, from_address_size);

  return WC24_OK;
}

void MailParser::ParseSubject(u32 index, u32 receive_index) const
{
  const std::string subject = GetHeaderValue(index, "Subject");
  if (subject.empty())
    return;

  const u32 offset = m_message_data[index].find("Subject") + 9;
  m_receive_list->SetPackedSubject(receive_index, offset, subject.size());
}

ErrorCode MailParser::ParseTo(u32 index, u32 receive_index) const
{
  const std::string to = GetHeaderValue(index, "To");
  if (to.empty())
    return WC24_ERR_FORMAT;

  const u32 offset = m_message_data[index].find("To") + 4;
  m_receive_list->SetPackedTo(receive_index, offset, to.size());
  m_receive_list->SetNumberOfRecipients(receive_index,
                                        static_cast<u8>(std::count(to.begin(), to.end(), ',')) + 1);

  return WC24_OK;
}

ErrorCode MailParser::ParseWiiAppId(u32 index, u32 receive_index) const
{
  const std::string full_id = GetHeaderValue(index, "X-Wii-AppId");
  if (full_id.empty())
  {
    // Fallback to default values
    m_receive_list->SetWiiAppGroupId(receive_index, 0);
    m_receive_list->SetWiiAppId(receive_index, 0);
    return WC24_OK;
  }

  // Sanity checks to make sure everything is valid.
  const char s = full_id.at(0);
  if (full_id.size() != 15 || s - 48 > 4 || full_id.at(1) != '-' || full_id.at(10) != '-')
    return WC24_ERR_FORMAT;

  if ((s & 1) != 0)
    m_receive_list->UpdateFlag(receive_index, 4, FlagOP::Or);

  if (Common::ExtractBit(s, 1) != 0)
    m_receive_list->UpdateFlag(receive_index, 8, FlagOP::Or);

  // Determine the app ID.
  u32 app_id = std::stoul(full_id.substr(2, 10), nullptr, 16);
  m_receive_list->SetWiiAppId(receive_index, app_id);

  // Now the group id.
  u32 group_id = std::stoul(full_id.substr(11, 14), nullptr, 16);
  m_receive_list->SetWiiAppGroupId(receive_index, static_cast<u16>(group_id));

  return WC24_OK;
}

ErrorCode MailParser::ParseWiiCmd(u32 index, u32 receive_index) const
{
  std::string str_cmd = GetHeaderValue(index, "X-Wii-Cmd");
  if (str_cmd.empty())
    return WC24_OK;

  if (str_cmd.size() != 8)
    return WC24_ERR_FORMAT;

  u32 cmd = std::stoul(str_cmd, nullptr, 16);
  m_receive_list->SetWiiCmd(receive_index, cmd);

  return WC24_OK;
}
}  // namespace IOS::HLE::NWC24::Mail
