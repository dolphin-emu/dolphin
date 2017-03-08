// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"

namespace File
{
class IOFile;
}

// This class is meant to edit the values in a given Wii SYSCONF file
// It currently does not add/remove/rearrange sections,
// instead only modifies exiting sections' data

#define SYSCONF_SIZE 0x4000

enum SysconfType
{
  Type_BigArray = 1,
  Type_SmallArray,
  Type_Byte,
  Type_Short,
  Type_Long,
  Type_LongLong,
  Type_Bool
};

struct SSysConfHeader
{
  char version[4];
  u16 numEntries;
};

struct SSysConfEntry
{
  u16 offset = 0;
  SysconfType type;
  u8 nameLength = 0;
  char name[32] = {};
  u16 dataLength = 0;
  std::vector<u8> data;

  template <class T>
  T GetData() const
  {
    T extracted_data;
    std::memcpy(&extracted_data, data.data(), sizeof(T));
    return extracted_data;
  }
  bool GetArrayData(u8* dest, u16 destSize) const
  {
    if (dest && destSize >= dataLength)
    {
      memcpy(dest, data.data(), dataLength);
      return true;
    }
    return false;
  }
  bool SetArrayData(const u8* buffer, u16 bufferSize)
  {
    if (buffer)
    {
      memcpy(data.data(), buffer, std::min<u16>(bufferSize, dataLength));
      return true;
    }
    return false;
  }
};

class SysConf
{
public:
  SysConf(Common::FromWhichRoot root_type);
  ~SysConf();

  bool IsValid() const { return m_IsValid; }
  template <class T>
  T GetData(const char* sectionName) const
  {
    if (!m_IsValid)
    {
      PanicAlertT("Trying to read from invalid SYSCONF");
      return 0;
    }

    auto index = m_Entries.cbegin();
    for (; index < m_Entries.cend() - 1; ++index)
    {
      if (strcmp(index->name, sectionName) == 0)
        break;
    }
    if (index == m_Entries.cend() - 1)
    {
      PanicAlertT("Section %s not found in SYSCONF", sectionName);
      return 0;
    }

    return index->GetData<T>();
  }

  bool GetArrayData(const char* sectionName, u8* dest, u16 destSize) const
  {
    if (!m_IsValid)
    {
      PanicAlertT("Trying to read from invalid SYSCONF");
      return false;
    }

    auto index = m_Entries.cbegin();
    for (; index < m_Entries.cend() - 1; ++index)
    {
      if (strcmp(index->name, sectionName) == 0)
        break;
    }
    if (index == m_Entries.cend() - 1)
    {
      PanicAlertT("Section %s not found in SYSCONF", sectionName);
      return false;
    }

    return index->GetArrayData(dest, destSize);
  }

  bool SetArrayData(const char* sectionName, const u8* buffer, u16 bufferSize)
  {
    if (!m_IsValid)
      return false;

    auto index = m_Entries.begin();
    for (; index < m_Entries.end() - 1; ++index)
    {
      if (strcmp(index->name, sectionName) == 0)
        break;
    }
    if (index == m_Entries.end() - 1)
    {
      PanicAlertT("Section %s not found in SYSCONF", sectionName);
      return false;
    }

    return index->SetArrayData(buffer, bufferSize);
  }

  template <class T>
  bool SetData(const char* sectionName, T newValue)
  {
    if (!m_IsValid)
      return false;

    auto index = m_Entries.begin();
    for (; index < m_Entries.end() - 1; ++index)
    {
      if (strcmp(index->name, sectionName) == 0)
        break;
    }
    if (index == m_Entries.end() - 1)
    {
      PanicAlertT("Section %s not found in SYSCONF", sectionName);
      return false;
    }

    std::memcpy(index->data.data(), &newValue, sizeof(T));
    return true;
  }

  bool Save();
  bool SaveToFile(const std::string& filename);
  bool LoadFromFile(const std::string& filename);
  bool Reload();
  void UpdateLocation(Common::FromWhichRoot root_type);

private:
  bool LoadFromFileInternal(File::IOFile&& file);
  void GenerateSysConf();
  void Clear();
  void ApplySettingsFromMovie();

  std::string m_Filename;
  std::string m_FilenameDefault;
  std::vector<SSysConfEntry> m_Entries;
  bool m_IsValid = false;
};
