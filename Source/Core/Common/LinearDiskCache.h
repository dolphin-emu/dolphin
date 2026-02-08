// Copyright 2010 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "Common/Version.h"

// On disk format:
// header{
// u32 'DCAC';
// u32 version;  // svn_rev
// u16 sizeof(key_type);
// u16 sizeof(value_type);
//}

// key_value_pair{
// u32 value_size;
// key_type   key;
// value_type[value_size]   value;
//}

namespace Common
{
template <typename K, typename V>
class LinearDiskCacheReader
{
public:
  virtual void Read(const K& key, const V* value, u32 value_size) = 0;
};

// Dead simple unsorted key-value store with append functionality.
// No random read functionality, all reading is done in OpenAndRead.
// Keys and values can contain any characters, including \0.
//
// Suitable for caching generated shader bytecode between executions.
// Not tuned for extreme performance but should be reasonably fast.
// Does not support keys or values larger than 2GB, which should be reasonable.
// Keys must have non-zero length; values can have zero length.

// K and V are some POD type
// K : the key type
// V : value array type
template <typename K, typename V>
class LinearDiskCache
{
public:
  // return number of read entries
  u32 OpenAndRead(const std::string& filename, LinearDiskCacheReader<K, V>& reader)
  {
    // Since we're reading/writing directly to the storage of K instances,
    // K must be trivially copyable.
    static_assert(std::is_trivially_copyable<K>::value, "K must be a trivially copyable type");

    // close any currently opened file
    Close();
    m_num_entries = 0;

    // try opening for reading/writing
    m_file.Open(filename, "r+b");

    const u64 file_size = m_file.GetSize();

    m_header.Init();
    if (m_file.IsOpen() && ValidateHeader())
    {
      // good header, read some key/value pairs
      K key;

      std::unique_ptr<V[]> value = nullptr;
      u32 value_size = 0;
      u32 entry_number = 0;
      u64 last_valid_value_start = m_file.Tell();

      while (m_file.ReadArray(&value_size, 1))
      {
        const u64 next_extent = m_file.Tell() + sizeof(value_size) + value_size;
        if (next_extent > file_size)
          break;

        // TODO: use make_unique_for_overwrite in C++20
        value = std::unique_ptr<V[]>(new V[value_size]);

        // read key/value and pass to reader
        if (m_file.ReadArray(&key, 1) && m_file.ReadArray(value.get(), value_size) &&
            m_file.ReadArray(&entry_number, 1) && entry_number == m_num_entries + 1)
        {
          last_valid_value_start = m_file.Tell();
          reader.Read(key, value.get(), value_size);
        }
        else
        {
          break;
        }

        m_num_entries++;
      }
      m_file.ClearError();
      m_file.Seek(last_valid_value_start, File::SeekOrigin::Begin);

      return m_num_entries;
    }

    // failed to open file for reading or bad header
    // close and recreate file
    Close();
    m_file.Open(filename, "wb");
    WriteHeader();
    return 0;
  }

  void Sync() { m_file.Flush(); }
  void Close()
  {
    if (m_file.IsOpen())
      m_file.Close();
  }

  // Appends a key-value pair to the store.
  void Append(const K& key, const V* value, u32 value_size)
  {
    // TODO: Should do a check that we don't already have "key"? (I think each caller does that
    // already.)
    m_file.WriteArray(&value_size, 1);
    m_file.WriteArray(&key, 1);
    m_file.WriteArray(value, value_size);
    m_num_entries++;
    m_file.WriteArray(&m_num_entries, 1);
  }

private:
  void WriteHeader() { m_file.WriteArray(&m_header, 1); }
  bool ValidateHeader()
  {
    char file_header[sizeof(Header)];

    return (m_file.ReadArray(file_header, sizeof(Header)) &&
            !memcmp((const char*)&m_header, file_header, sizeof(Header)));
  }

  struct Header
  {
    void Init()
    {
      // Null-terminator is intentionally not copied.
      std::memcpy(&id, "DCAC", sizeof(u32));
      std::memcpy(ver, Common::GetScmRevGitStr().c_str(),
                  std::min(Common::GetScmRevGitStr().size(), sizeof(ver)));
    }

    u32 id = 0;
    const u16 key_t_size = sizeof(K);
    const u16 value_t_size = sizeof(V);
    char ver[40] = {};

  } m_header;

  File::IOFile m_file;
  u32 m_num_entries = 0;
};
}  // namespace Common
