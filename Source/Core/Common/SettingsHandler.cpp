// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Thanks to Treeki for writing the original class - 29/01/2012

#include "Common/SettingsHandler.h"

#include <cstddef>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#include "Common/CommonTypes.h"

namespace Common
{
SettingsHandler::SettingsHandler()
{
  Reset();
}

SettingsHandler::SettingsHandler(Buffer&& buffer)
{
  SetBytes(std::move(buffer));
}

const SettingsHandler::Buffer& SettingsHandler::GetBytes() const
{
  return m_buffer;
}

void SettingsHandler::SetBytes(Buffer&& buffer)
{
  Reset();
  m_buffer = std::move(buffer);
  Decrypt();
}

std::string SettingsHandler::GetValue(const std::string& key) const
{
  std::string delim = std::string("\r\n");
  std::string toFind = delim + key + "=";
  size_t found = decoded.find(toFind);

  if (found != decoded.npos)
  {
    size_t delimFound = decoded.find(delim, found + toFind.length());
    if (delimFound == decoded.npos)
      delimFound = decoded.length() - 1;
    return decoded.substr(found + toFind.length(), delimFound - (found + toFind.length()));
  }
  else
  {
    toFind = key + "=";
    found = decoded.find(toFind);
    if (found == 0)
    {
      size_t delimFound = decoded.find(delim, found + toFind.length());
      if (delimFound == decoded.npos)
        delimFound = decoded.length() - 1;
      return decoded.substr(found + toFind.length(), delimFound - (found + toFind.length()));
    }
  }

  return "";
}

void SettingsHandler::Decrypt()
{
  const u8* str = m_buffer.data();
  while (*str != 0)
  {
    if (m_position >= m_buffer.size())
      return;
    decoded.push_back((u8)(m_buffer[m_position] ^ m_key));
    m_position++;
    str++;
    m_key = (m_key >> 31) | (m_key << 1);
  }
}

void SettingsHandler::Reset()
{
  decoded = "";
  m_position = 0;
  m_key = INITIAL_SEED;
  m_buffer = {};
}

void SettingsHandler::AddSetting(const std::string& key, const std::string& value)
{
  for (const char& c : key)
  {
    WriteByte(c);
  }

  WriteByte('=');

  for (const char& c : value)
  {
    WriteByte(c);
  }

  WriteByte(13);
  WriteByte(10);
}

void SettingsHandler::WriteByte(u8 b)
{
  if (m_position >= m_buffer.size())
    return;

  m_buffer[m_position] = b ^ m_key;
  m_position++;
  m_key = (m_key >> 31) | (m_key << 1);
}

std::string SettingsHandler::GenerateSerialNumber()
{
  const std::time_t t = std::time(nullptr);

  // Must be 9 characters at most; otherwise the serial number will be rejected by SDK libraries,
  // as there is a check to ensure the string length is strictly lower than 10.
  // 3 for %j, 2 for %H, 2 for %M, 2 for %S.
  std::stringstream stream;
  stream << std::put_time(std::localtime(&t), "%j%H%M%S");
  return stream.str();
}
}  // namespace Common
