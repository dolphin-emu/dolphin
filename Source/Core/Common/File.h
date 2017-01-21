// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <cstdio>
#include <ios>
#include <istream>
#include <iterator>
#include <memory>
#include <streambuf>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/NonCopyable.h"

#ifdef ANDROID
struct AAsset;
struct AAssetDir;
#endif

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

#ifdef ANDROID
class AndroidAsset final : public ReadOnlyFile, public NonCopyable
{
public:
  AndroidAsset();
  AndroidAsset(const std::string& path);

  ~AndroidAsset() override;

  AndroidAsset(AndroidAsset&& other) noexcept;
  AndroidAsset& operator=(AndroidAsset&& other) noexcept;

  void Swap(AndroidAsset& other) noexcept;

  bool Open(const std::string& path);
  bool Close() override;

  bool IsOpen() const override { return m_file != nullptr; }
  // IsGood returns false after a read, write or other function fails
  bool IsGood() const override { return m_good; }
  // Reset IsGood to true
  void Clear() override { m_good = true; }
  bool Seek(s64 offset, int origin) override;
  u64 Tell() const override;
  u64 GetSize() override;

protected:
  bool Read(void* data, size_t element_size, size_t count, size_t* pReadElements) override;

private:
  AAsset* m_file;
  bool m_good;
};
#endif

class ReadOnlyFileBuffer : public std::streambuf
{
public:
  ReadOnlyFileBuffer(ReadOnlyFile* file, size_t buffer_size);
  int underflow() override;

private:
  ReadOnlyFile* m_file;
  std::vector<char> m_buffer;
};

struct ReadOnlyFileStreamBase
{
  ReadOnlyFileBuffer m_buffer;

  ReadOnlyFileStreamBase(ReadOnlyFile* file, size_t buffer_size) : m_buffer(file, buffer_size) {}
};

class ReadOnlyFileStream final : private virtual ReadOnlyFileStreamBase, public std::istream
{
public:
  // This object doesn't take ownership of the file object that's passed in.
  // The file object must remain in memory while this object is being used.
  ReadOnlyFileStream(ReadOnlyFile* file, size_t buffer_size = 65536)
      : ReadOnlyFileStreamBase(file, buffer_size), std::ios(&m_buffer), std::istream(&m_buffer)
  {
    if (!*file)
      setstate(std::ios::badbit);
  }
};

class DirectoryIterator
{
public:
  virtual ~DirectoryIterator();
  // Advances this iterator to the next child and returns its path.
  // An empty string is returned when the end is reached.
  virtual std::string NextChild() = 0;
};

class StandardDirectoryIterator final : public DirectoryIterator, private NonCopyable
{
public:
  StandardDirectoryIterator(const std::string& path);
  ~StandardDirectoryIterator();

  std::string NextChild() override;

private:
  std::string m_base_path;
#ifdef _WIN32
  HANDLE m_handle;
  WIN32_FIND_DATA m_find_data;
  bool m_first_child = true;
#else
  DIR* m_dir;
#endif
};

#ifdef ANDROID
class AndroidAssetDirectoryIterator final : public DirectoryIterator, private NonCopyable
{
public:
  AndroidAssetDirectoryIterator(const std::string& path);
  ~AndroidAssetDirectoryIterator();

  std::string NextChild() override;

private:
  AAssetDir* m_dir;
};
#endif

class Path final
{
  friend class const_iterator;

public:
  class const_iterator final
  {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Path;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    const_iterator();
    const_iterator(const Path& path);

    const_iterator& operator++();
    const_iterator operator++(int);

    const Path& operator*() const { return *m_child_path; }
    const Path* operator->() const { return m_child_path.get(); }
    bool operator==(const const_iterator& other) const
    {
      return *m_child_path == *other.m_child_path;
    }
    bool operator!=(const const_iterator& other) const { return !operator==(other); }
  private:
    std::shared_ptr<Path> m_child_path;
    std::shared_ptr<DirectoryIterator> m_iterator;
  };

#ifdef ANDROID
  Path() : m_is_asset(false) {}
  // These constuctors intentionally allow implicit conversion
  Path(const char* path, bool is_asset = false) : m_path(path), m_is_asset(is_asset) {}
  Path(const std::string& path, bool is_asset = false) : m_path(path), m_is_asset(is_asset) {}
  friend bool operator==(const Path& lhs, const Path& rhs)
  {
    return lhs.m_path == rhs.m_path && lhs.m_is_asset == rhs.m_is_asset;
  }
  friend bool operator<(const Path& lhs, const Path& rhs)
  {
    return lhs.m_is_asset == rhs.m_is_asset ? lhs.m_path < rhs.m_path : rhs.m_is_asset;
  }
#else
  Path() {}
  // These constuctors intentionally allow implicit conversion
  Path(const char* path) : m_path(path) {}
  Path(const std::string& path) : m_path(path) {}
  friend bool operator==(const Path& lhs, const Path& rhs) { return lhs.m_path == rhs.m_path; }
  friend bool operator<(const Path& lhs, const Path& rhs) { return lhs.m_path < rhs.m_path; }
#endif
  friend bool operator!=(const Path& lhs, const Path& rhs) { return !operator==(lhs, rhs); }
  friend bool operator>(const Path& lhs, const Path& rhs) { return operator<(rhs, lhs); }
  friend bool operator<=(const Path& lhs, const Path& rhs) { return !operator<(rhs, lhs); }
  friend bool operator>=(const Path& lhs, const Path& rhs) { return !operator<(lhs, rhs); }
  Path operator+(const std::string& str) const;
  Path& operator+=(const std::string& str);

  // This returns the name of the file or directory that the
  // path represents. It doesn't return the whole path.
  std::string GetName() const;

  bool Exists() const;
  bool IsDirectory() const;
  u64 GetFileSize() const;

  std::unique_ptr<ReadOnlyFile> OpenFile(bool binary) const;

  // For iterating through a directory
  const_iterator begin() const { return cbegin(); }
  const_iterator end() const { return cend(); }
  const_iterator cbegin() const { return const_iterator(*this); }
  const_iterator cend() const { return const_iterator(); }
private:
  std::string m_path;
#ifdef ANDROID
  bool m_is_asset;
#endif
};

}  // namespace File
