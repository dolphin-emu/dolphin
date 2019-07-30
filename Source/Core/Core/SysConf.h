// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Utilities to parse and modify a Wii SYSCONF file and its sections.

#pragma once

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/NandPaths.h"

namespace IOS::HLE::FS
{
class FileHandle;
class FileSystem;
}  // namespace IOS::HLE::FS

class SysConf final
{
public:
  explicit SysConf(std::shared_ptr<IOS::HLE::FS::FileSystem> fs);
  ~SysConf();

  void Clear();
  void Load();
  bool Save() const;

  struct Entry
  {
    enum Type : u8
    {
      BigArray = 1,
      SmallArray = 2,
      Byte = 3,
      Short = 4,
      Long = 5,
      LongLong = 6,
      // Should really be named Bool, but this conflicts with some random macro. :/
      ByteBool = 7,
    };

    Entry(Type type_, const std::string& name_);
    Entry(Type type_, const std::string& name_, const std::vector<u8>& bytes_);
    Entry(Type type_, const std::string& name_, std::vector<u8>&& bytes_);

    // Intended for use with the non array types.
    template <typename T>
    T GetData(T default_value) const
    {
      if (bytes.size() != sizeof(T))
        return default_value;
      T value;
      std::memcpy(&value, bytes.data(), bytes.size());
      return value;
    }
    template <typename T>
    void SetData(T value)
    {
      ASSERT(sizeof(value) == bytes.size());
      std::memcpy(bytes.data(), &value, bytes.size());
    }

    Type type;
    std::string name;
    std::vector<u8> bytes;
  };

  void AddEntry(Entry&& entry);
  Entry* GetEntry(const std::string& key);
  const Entry* GetEntry(const std::string& key) const;
  Entry* GetOrAddEntry(const std::string& key, Entry::Type type);
  void RemoveEntry(const std::string& key);

  // Intended for use with the non array types.
  template <typename T>
  T GetData(const std::string& key, T default_value) const
  {
    const Entry* entry = GetEntry(key);
    return entry ? entry->GetData(default_value) : default_value;
  }
  template <typename T>
  void SetData(const std::string& key, Entry::Type type, T value)
  {
    GetOrAddEntry(key, type)->SetData(value);
  }

private:
  void InsertDefaultEntries();
  bool LoadFromFile(const IOS::HLE::FS::FileHandle& file);

  std::vector<Entry> m_entries;
  std::shared_ptr<IOS::HLE::FS::FileSystem> m_fs;
};
