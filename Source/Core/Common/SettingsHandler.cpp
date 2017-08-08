// Copyright 2012 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Thanks to Treeki for writing the original class - 29/01/2012

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/SettingsHandler.h"
#include "Common/Timer.h"

SettingsHandler::SettingsHandler()
{
  Reset();
}

bool SettingsHandler::Open(const std::string& settings_file_path)
{
  Reset();

  File::IOFile file{settings_file_path, "rb"};
  if (!file.ReadBytes(m_buffer.data(), m_buffer.size()))
    return false;

  Decrypt();
  return true;
}

bool SettingsHandler::Save(const std::string& destination_file_path) const
{
  if (!File::CreateFullPath(destination_file_path))
    return false;

  File::IOFile file{destination_file_path, "wb"};
  return file.WriteBytes(m_buffer.data(), m_buffer.size());
}

const u8* SettingsHandler::GetData() const
{
  return m_buffer.data();
}

const std::string SettingsHandler::GetValue(const std::string& key)
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
