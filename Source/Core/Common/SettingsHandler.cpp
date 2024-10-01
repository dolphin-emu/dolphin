// Copyright 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Thanks to Treeki for writing the original class - 29/01/2012

#include "Common/SettingsHandler.h"

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <iomanip>
#include <string>

#include <fmt/chrono.h>

#include "Common/CommonTypes.h"

namespace Common
{
namespace
{
// Key used to encrypt/decrypt setting.txt contents
constexpr u32 INITIAL_SEED = 0x73B5DBFA;
}  // namespace

SettingsWriter::SettingsWriter() : m_buffer{}, m_position{0}, m_key{INITIAL_SEED}
{
}

const SettingsBuffer& SettingsWriter::GetBytes() const
{
  return m_buffer;
}

std::string SettingsReader::GetValue(std::string_view key) const
{
  constexpr char delim[] = "\n";
  std::string toFind = std::string(delim).append(key).append("=");
  size_t found = m_decoded.find(toFind);

  if (found != std::string_view::npos)
  {
    size_t delimFound = m_decoded.find(delim, found + toFind.length());
    if (delimFound == std::string_view::npos)
      delimFound = m_decoded.length() - 1;
    return m_decoded.substr(found + toFind.length(), delimFound - (found + toFind.length()));
  }
  else
  {
    toFind = std::string(key).append("=");
    found = m_decoded.find(toFind);
    if (found == 0)
    {
      size_t delimFound = m_decoded.find(delim, found + toFind.length());
      if (delimFound == std::string_view::npos)
        delimFound = m_decoded.length() - 1;
      return m_decoded.substr(found + toFind.length(), delimFound - (found + toFind.length()));
    }
  }

  return "";
}

SettingsReader::SettingsReader(const SettingsBuffer& buffer) : m_decoded{""}
{
  u32 position = 0;
  u32 key = INITIAL_SEED;
  while (position < buffer.size())
  {
    m_decoded.push_back((u8)(buffer[position] ^ key));
    position++;
    key = (key >> 31) | (key << 1);
  }

  // The decoded data normally uses CRLF line endings, but occasionally
  // (see the comment in WriteLine), lines can be separated by CRLFLF.
  // To handle this, we remove every CR and treat LF as the line ending.
  // (We ignore empty lines.)
  std::erase(m_decoded, '\x0d');
}

void SettingsWriter::AddSetting(std::string_view key, std::string_view value)
{
  WriteLine(fmt::format("{}={}\r\n", key, value));
}

void SettingsWriter::WriteLine(std::string_view str)
{
  const u32 old_position = m_position;
  const u32 old_key = m_key;

  // Encode and write the line
  for (char c : str)
    WriteByte(c);

  // If the encoded data contains a null byte, Nintendo's decoder will stop at that null byte
  // instead of decoding all the data. To avoid this: If the data we just wrote contains
  // a null byte, add an LF right before the line to prod the values into being different,
  // just like Nintendo does. Due to the chosen key, LF itself never encodes into a null byte.
  const auto begin = m_buffer.cbegin() + old_position;
  const auto end = m_buffer.cbegin() + m_position;
  if (std::find(begin, end, 0) != end)
  {
    m_key = old_key;
    m_position = old_position;
    WriteByte('\n');
    WriteLine(str);
  }
}

void SettingsWriter::WriteByte(u8 b)
{
  if (m_position >= m_buffer.size())
    return;

  m_buffer[m_position] = b ^ m_key;
  m_position++;
  m_key = (m_key >> 31) | (m_key << 1);
}

std::string SettingsWriter::GenerateSerialNumber()
{
  const std::time_t t = std::time(nullptr);

  // Must be 9 characters at most; otherwise the serial number will be rejected by SDK libraries,
  // as there is a check to ensure the string length is strictly lower than 10.
  // 3 for %j, 2 for %H, 2 for %M, 2 for %S.
  return fmt::format("{:%j%H%M%S}", fmt::localtime(t));
}
}  // namespace Common
