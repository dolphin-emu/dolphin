// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class IVolume;

// file info of an FST entry
class IFileInfo
{
friend class const_iterator;

public:
  class const_iterator final : public std::iterator<std::input_iterator_tag, IFileInfo>
  {
  public:
    const_iterator(std::unique_ptr<IFileInfo> file_info) : m_file_info(std::move(file_info)) {}
    const_iterator(const const_iterator& it) : m_file_info(it.m_file_info->clone()) {}
    const_iterator(const_iterator&& it) : m_file_info(std::move(it.m_file_info)) {}
    ~const_iterator() {}
    const_iterator& operator=(const const_iterator& it)
    {
      m_file_info = it.m_file_info->clone();
      return *this;
    }
    const_iterator& operator=(const_iterator&& it)
    {
      m_file_info = std::move(it.m_file_info);
      return *this;
    }
    const_iterator& operator++()
    {
      ++*m_file_info;
      return *this;
    }
    bool operator==(const const_iterator& it) const { return *m_file_info == *it.m_file_info; }
    bool operator!=(const const_iterator& it) const { return !operator==(it); }
    // Incrementing or destroying an iterator will invalidate its returned references and
    // pointers, but will not invalidate copies of the iterator or file info object.
    const IFileInfo& operator*() const { return *m_file_info.get(); }
    const IFileInfo* operator->() const { return m_file_info.get(); }
  private:
    std::unique_ptr<IFileInfo> m_file_info;
  };

  virtual ~IFileInfo() {}
  bool operator==(const IFileInfo& other) const { return GetAddress() == other.GetAddress(); }
  bool operator!=(const IFileInfo& other) const { return !operator==(other); }
  virtual IFileInfo& operator++() = 0;
  virtual std::unique_ptr<IFileInfo> clone() const = 0;
  virtual const_iterator begin() const = 0;
  virtual const_iterator end() const = 0;

  // The offset of a file on the disc (inside the partition, if there is one).
  // Not guaranteed to return a meaningful value for directories.
  virtual u64 GetOffset() const = 0;
  // The size of a file.
  // Not guaranteed to return a meaningful value for directories.
  virtual u32 GetSize() const = 0;
  virtual bool IsDirectory() const = 0;
  // The number of files and directories in a directory, including those in subdirectories.
  // Not guaranteed to return a meaningful value for files.
  virtual u32 GetTotalChildren() const = 0;
  virtual std::string GetName() const = 0;
  // GetPath will find the parents of the current object and call GetName on them,
  // so it's slower than other functions. If you're traversing through folders
  // to get a file and its path, building the path while traversing is faster.
  virtual std::string GetPath() const = 0;

protected:
  // Used for comparisons with other file info objects
  virtual const void* GetAddress() const = 0;
};

class IFileSystem
{
public:
  IFileSystem(const IVolume* volume);
  virtual ~IFileSystem();

  // If IsValid is false, GetRoot must not be called. CreateFileSystem
  // takes care of this automatically, so other code is recommended to use it.
  virtual bool IsValid() const = 0;
  // The object returned by GetRoot and all objects created from it
  // are only valid for as long as the file system object is valid.
  virtual const IFileInfo& GetRoot() const = 0;
  // Returns nullptr if not found
  virtual std::unique_ptr<IFileInfo> FindFileInfo(const std::string& path) const = 0;
  // Returns nullptr if not found
  virtual std::unique_ptr<IFileInfo> FindFileInfo(u64 disc_offset) const = 0;

  virtual u64 ReadFile(const IFileInfo* file_info, u8* buffer, u64 max_buffer_size,
                       u64 offset_in_file = 0) const = 0;
  virtual bool ExportFile(const IFileInfo* file_info, const std::string& export_filename) const = 0;
  virtual bool ExportApploader(const std::string& export_folder) const = 0;
  virtual bool ExportDOL(const std::string& export_folder) const = 0;
  virtual u64 GetBootDOLOffset() const = 0;
  virtual u32 GetBootDOLSize(u64 dol_offset) const = 0;

protected:
  const IVolume* m_volume;
};

std::unique_ptr<IFileSystem> CreateFileSystem(const IVolume* volume);

}  // namespace
