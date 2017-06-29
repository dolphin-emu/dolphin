// This file is under the public domain.

#pragma once

#include <iterator>
#include <memory>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#endif

#include "Common/CommonTypes.h"

namespace Common
{
// This class is designed for speed over convenience.
// You may want to consider using FileSearch.h instead.
class DirectoryEntry final
{
public:
#ifdef _WIN32
  using CharType = wchar_t;
  using StringType = std::wstring;
#else
  using CharType = char;
  using StringType = std::string;
#endif

  DirectoryEntry();
  explicit DirectoryEntry(const StringType& path);
  ~DirectoryEntry();

  DirectoryEntry(const DirectoryEntry& other) = delete;
  DirectoryEntry(DirectoryEntry&& other);
  DirectoryEntry& operator=(const DirectoryEntry& other) = delete;
  DirectoryEntry& operator=(DirectoryEntry&& other);

  void Advance();
  bool IsValid() const;

  // Returns the name, not the full path. Only call if IsValid returns true.
  const CharType* GetName() const;

  // Runs faster than File::IsDirectory on many systems. Only call if the following are true:
  // 1. IsValid returns true, and
  // 2. The OS is Windows or the path reference passed to the constructor is still valid
  // (Note that ToOSEncoding passes through references untouched on non-Windows systems.)
  bool IsDirectory() const;

  // Runs faster than File::GetSize on Windows. Only call if the following are true:
  // 1. IsValid returns true, and
  // 2. The OS is Windows or the path reference passed to the constructor is still valid
  // (Note that ToOSEncoding passes through references untouched on non-Windows systems.)
  u64 GetSize() const;

private:
  void Close();
  void Move(DirectoryEntry&& other);

  void SkipSpecialName();

#ifdef _WIN32
  HANDLE m_handle = INVALID_HANDLE_VALUE;
  WIN32_FIND_DATA m_find_data;
#else
  const std::string* m_path;
  DIR* m_dir = nullptr;
  dirent* m_dirent = nullptr;
#endif
};

std::string ToUTF8(const std::wstring& str);
const std::string& ToUTF8(const std::string& str);
std::string& ToUTF8(std::string& str);
std::string ToUTF8(std::string&& str);
#ifdef _WIN32
std::wstring ToOSEncoding(const std::string& str);
#endif
const DirectoryEntry::StringType& ToOSEncoding(const DirectoryEntry::StringType& str);
DirectoryEntry::StringType& ToOSEncoding(DirectoryEntry::StringType& str);
DirectoryEntry::StringType ToOSEncoding(DirectoryEntry::StringType&& str);

// Iterates through the names (not paths) of a directory's files and subdirectories.
class DirectoryIterator final
{
public:
  using iterator_category = std::input_iterator_tag;
  using value_type = DirectoryEntry;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;

  DirectoryIterator();
  explicit DirectoryIterator(const DirectoryEntry::StringType& path);

  bool operator==(const DirectoryIterator& other) const;
  bool operator!=(const DirectoryIterator& other) const;

  DirectoryIterator& operator++();
  DirectoryIterator operator++(int);

  const DirectoryEntry& operator*() const;
  const DirectoryEntry* operator->() const;

private:
  std::shared_ptr<DirectoryEntry> m_entry;
};

template <typename T>
class Directory final
{
public:
  explicit Directory(const T& path) : m_path(path) {}
  // Only call this if the path reference still is valid
  DirectoryIterator cbegin() { return DirectoryIterator(ToOSEncoding(m_path)); }
  DirectoryIterator cend() { return DirectoryIterator(); }
  // Only call this if the path reference still is valid
  DirectoryIterator begin() { return cbegin(); }
  DirectoryIterator end() { return cend(); }

private:
  const T& m_path;
};

}  // namespace Common
