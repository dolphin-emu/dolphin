// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
// file info of an FST entry
class FileInfo
{
  friend class const_iterator;

public:
  class const_iterator final
  {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = const FileInfo;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    const_iterator() : m_file_info(nullptr) {}
    const_iterator(std::unique_ptr<FileInfo> file_info) : m_file_info(std::move(file_info)) {}
    const_iterator(const const_iterator& it) : m_file_info(it.m_file_info->clone()) {}
    const_iterator(const_iterator&& it) noexcept : m_file_info(std::move(it.m_file_info)) {}
    ~const_iterator() = default;
    const_iterator& operator=(const const_iterator& it)
    {
      m_file_info = it.m_file_info ? it.m_file_info->clone() : nullptr;
      return *this;
    }
    const_iterator& operator=(const_iterator&& it) noexcept
    {
      m_file_info = std::move(it.m_file_info);
      return *this;
    }
    const_iterator& operator++()
    {
      ++*m_file_info;
      return *this;
    }
    const_iterator operator++(int)
    {
      const_iterator old = *this;
      ++*m_file_info;
      return old;
    }
    bool operator==(const const_iterator& it) const
    {
      return m_file_info ? (it.m_file_info && *m_file_info == *it.m_file_info) : (!it.m_file_info);
    }
    bool operator!=(const const_iterator& it) const { return !operator==(it); }
    // Incrementing or destroying an iterator will invalidate its returned references and
    // pointers, but will not invalidate copies of the iterator or file info object.
    const FileInfo& operator*() const { return *m_file_info.get(); }
    const FileInfo* operator->() const { return m_file_info.get(); }

  private:
    std::unique_ptr<FileInfo> m_file_info;
  };

  virtual ~FileInfo();

  bool operator==(const FileInfo& other) const { return GetAddress() == other.GetAddress(); }
  bool operator!=(const FileInfo& other) const { return !operator==(other); }
  virtual std::unique_ptr<FileInfo> clone() const = 0;
  virtual const_iterator cbegin() const { return begin(); }
  virtual const_iterator cend() const { return end(); }
  virtual const_iterator begin() const = 0;
  virtual const_iterator end() const = 0;

  // The offset of a file on the disc (inside the partition, if there is one).
  // Not guaranteed to return a meaningful value for directories.
  virtual u64 GetOffset() const = 0;
  // The size of a file.
  // Not guaranteed to return a meaningful value for directories.
  virtual u32 GetSize() const = 0;
  // For a file, returns its size. For a directory, returns the total size of its contents.
  u64 GetTotalSize() const;
  virtual bool IsRoot() const = 0;
  virtual bool IsDirectory() const = 0;
  // The number of files and directories in a directory, including those in subdirectories.
  // Not guaranteed to return a meaningful value for files.
  virtual u32 GetTotalChildren() const = 0;
  virtual std::string GetName() const = 0;
  virtual bool NameCaseInsensitiveEquals(std::string_view other) const = 0;
  // GetPath will find the parents of the current object and call GetName on them,
  // so it's slower than other functions. If you're traversing through folders
  // to get a file and its path, building the path while traversing is faster.
  virtual std::string GetPath() const = 0;

protected:
  // Only used for comparisons with other file info objects
  virtual uintptr_t GetAddress() const = 0;

  // Called by iterators
  virtual FileInfo& operator++() = 0;
};

class FileSystem
{
public:
  virtual ~FileSystem();

  // If IsValid is false, GetRoot must not be called.
  virtual bool IsValid() const = 0;
  // The object returned by GetRoot and all objects created from it
  // are only valid for as long as the file system object is valid.
  virtual const FileInfo& GetRoot() const = 0;
  // Returns nullptr if not found
  virtual std::unique_ptr<FileInfo> FindFileInfo(std::string_view path) const = 0;
  // Returns nullptr if not found
  virtual std::unique_ptr<FileInfo> FindFileInfo(u64 disc_offset) const = 0;
};

// Calling Volume::GetFileSystem instead of manually constructing a filesystem is recommended,
// because it will check IsValid for you, will automatically pick the right type of filesystem,
// and will cache the filesystem in case it's needed again later.

}  // namespace DiscIO
