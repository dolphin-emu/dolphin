// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstdio>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/NonCopyable.h"

namespace File
{
class ReadOnlyFile
{
public:
  virtual ~ReadOnlyFile();
  template <typename T>
  bool ReadArray(T* data, size_t length, size_t* pReadElements = nullptr)
  {
    return Read(data, sizeof(T), length, pReadElements);
  }

  bool ReadBytes(void* data, size_t length)
  {
    return ReadArray(reinterpret_cast<char*>(data), length);
  }

  virtual bool IsOpen() const = 0;
  virtual bool Close() = 0;

  // IsGood returns false after a read, write or other function fails
  virtual bool IsGood() const = 0;
  // Reset IsGood to true
  virtual void Clear() = 0;

  virtual bool Seek(s64 off, int origin) = 0;
  virtual u64 Tell() const = 0;
  virtual u64 GetSize() = 0;

  explicit operator bool() const { return IsGood() && IsOpen(); }
protected:
  virtual bool Read(void* data, size_t element_size, size_t count,
                    size_t* pReadElements = nullptr) = 0;
};

// simple wrapper for cstdlib file functions to
// hopefully will make error checking easier
// and make forgetting an fclose() harder
class IOFile final : public ReadOnlyFile, public NonCopyable
{
public:
  IOFile();
  IOFile(std::FILE* file);
  IOFile(const std::string& filename, const char openmode[]);

  ~IOFile() override;

  IOFile(IOFile&& other) noexcept;
  IOFile& operator=(IOFile&& other) noexcept;

  void Swap(IOFile& other) noexcept;

  bool Open(const std::string& filename, const char openmode[]);
  bool Close() override;

  template <typename T>
  bool WriteArray(const T* data, size_t length)
  {
    return Write(data, sizeof(T), length);
  }

  bool WriteBytes(const void* data, size_t length)
  {
    return WriteArray(reinterpret_cast<const char*>(data), length);
  }

  bool IsOpen() const override { return nullptr != m_file; }
  // IsGood returns false after a read, write or other function fails
  bool IsGood() const override { return m_good; }
  // Reset IsGood to true
  void Clear() override
  {
    m_good = true;
    std::clearerr(m_file);
  }

  std::FILE* GetHandle() { return m_file; }
  void SetHandle(std::FILE* file);

  bool Seek(s64 off, int origin) override;
  u64 Tell() const override;
  u64 GetSize() override;
  bool Resize(u64 size);
  bool Flush();

protected:
  bool Read(void* data, size_t element_size, size_t count, size_t* pReadElements) override;

private:
  bool Write(const void* data, size_t element_size, size_t count);

  std::FILE* m_file;
  bool m_good;
};

}  // namespace File
