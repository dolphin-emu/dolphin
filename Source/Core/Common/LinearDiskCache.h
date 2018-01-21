// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

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
    using std::ios_base;

// Since we're reading/writing directly to the storage of K instances,
// K must be trivially copyable. TODO: Remove #if once GCC 5.0 is a
// minimum requirement.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 5
    static_assert(std::has_trivial_copy_constructor<K>::value,
                  "K must be a trivially copyable type");
#else
    static_assert(std::is_trivially_copyable<K>::value, "K must be a trivially copyable type");
#endif

    // close any currently opened file
    Close();
    m_num_entries = 0;

    // try opening for reading/writing
    File::OpenFStream(m_file, filename, ios_base::in | ios_base::out | ios_base::binary);

    m_file.seekg(0, std::ios::end);
    std::fstream::pos_type end_pos = m_file.tellg();
    m_file.seekg(0, std::ios::beg);
    std::fstream::pos_type start_pos = m_file.tellg();
    std::streamoff file_size = end_pos - start_pos;

    m_header.Init();
    if (m_file.is_open() && ValidateHeader())
    {
      // good header, read some key/value pairs
      K key;

      V* value = nullptr;
      u32 value_size = 0;
      u32 entry_number = 0;

      std::fstream::pos_type last_pos = m_file.tellg();

      while (Read(&value_size))
      {
        std::streamoff next_extent = (last_pos - start_pos) + sizeof(value_size) + value_size;
        if (next_extent > file_size)
          break;

        delete[] value;
        value = new V[value_size];

        // read key/value and pass to reader
        if (Read(&key) && Read(value, value_size) && Read(&entry_number) &&
            entry_number == m_num_entries + 1)
        {
          reader.Read(key, value, value_size);
        }
        else
        {
          break;
        }

        m_num_entries++;
        last_pos = m_file.tellg();
      }
      m_file.seekp(last_pos);
      m_file.clear();

      delete[] value;
      return m_num_entries;
    }

    // failed to open file for reading or bad header
    // close and recreate file
    Close();
    m_file.open(filename, ios_base::out | ios_base::trunc | ios_base::binary);
    WriteHeader();
    return 0;
  }

  void Sync() { m_file.flush(); }
  void Close()
  {
    if (m_file.is_open())
      m_file.close();
    // clear any error flags
    m_file.clear();
  }

  // Appends a key-value pair to the store.
  void Append(const K& key, const V* value, u32 value_size)
  {
    // TODO: Should do a check that we don't already have "key"? (I think each caller does that
    // already.)
    Write(&value_size);
    Write(&key);
    Write(value, value_size);
    m_num_entries++;
    Write(&m_num_entries);
  }

private:
  void WriteHeader() { Write(&m_header); }
  bool ValidateHeader()
  {
    char file_header[sizeof(Header)];

    return (Read(file_header, sizeof(Header)) &&
            !memcmp((const char*)&m_header, file_header, sizeof(Header)));
  }

  template <typename D>
  bool Write(const D* data, u32 count = 1)
  {
    return m_file.write((const char*)data, count * sizeof(D)).good();
  }

  template <typename D>
  bool Read(const D* data, u32 count = 1)
  {
    return m_file.read((char*)data, count * sizeof(D)).good();
  }

  struct Header
  {
    void Init()
    {
      // Null-terminator is intentionally not copied.
      std::memcpy(&id, "DCAC", sizeof(u32));
      std::memcpy(ver, scm_rev_git_str.c_str(), std::min(scm_rev_git_str.size(), sizeof(ver)));
    }

    u32 id;
    const u16 key_t_size = sizeof(K);
    const u16 value_t_size = sizeof(V);
    char ver[40] = {};

  } m_header;

  std::fstream m_file;
  u32 m_num_entries;
};
