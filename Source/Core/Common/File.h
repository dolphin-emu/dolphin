// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

#include "Common/CommonTypes.h"

namespace File
{
// simple wrapper for cstdlib file functions to
// hopefully will make error checking easier
// and make forgetting an fclose() harder
class IOFile
{
public:
  IOFile();
  IOFile(std::FILE* file);
  IOFile(const std::string& filename, const char openmode[]);

  ~IOFile();

  IOFile(const IOFile&) = delete;
  IOFile& operator=(const IOFile&) = delete;

  IOFile(IOFile&& other) noexcept;
  IOFile& operator=(IOFile&& other) noexcept;

  void Swap(IOFile& other) noexcept;

  bool Open(const std::string& filename, const char openmode[]);
  bool Close();

  template <typename T>
  bool ReadArray(T* elements, size_t count, size_t* num_read = nullptr)
  {
    size_t read_count = 0;
    if (!IsOpen() || count != (read_count = std::fread(elements, sizeof(T), count, m_file)))
      m_good = false;

    if (num_read)
      *num_read = read_count;

    return m_good;
  }

  template <typename T>
  bool WriteArray(const T* elements, size_t count)
  {
    if (!IsOpen() || count != std::fwrite(elements, sizeof(T), count, m_file))
      m_good = false;

    return m_good;
  }

  bool ReadBytes(void* data, size_t length)
  {
    return ReadArray(reinterpret_cast<char*>(data), length);
  }

  bool WriteBytes(const void* data, size_t length)
  {
    return WriteArray(reinterpret_cast<const char*>(data), length);
  }

  bool IsOpen() const { return nullptr != m_file; }
  // m_good is set to false when a read, write or other function fails
  bool IsGood() const { return m_good; }
  explicit operator bool() const { return IsGood() && IsOpen(); }
  std::FILE* GetHandle() { return m_file; }
  void SetHandle(std::FILE* file);

  bool Seek(s64 off, int origin);
  u64 Tell() const;
  u64 GetSize() const;
  bool Resize(u64 size);
  bool Flush();

  // clear error state
  void Clear()
  {
    m_good = true;
    std::clearerr(m_file);
  }

private:
  std::FILE* m_file;
  bool m_good;
};

}  // namespace File
