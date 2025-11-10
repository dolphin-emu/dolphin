// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

namespace File
{
enum class AccessMode
{
  Read,
  Write,
  ReadAndWrite,
};

enum class OpenMode
{
  // Based on the provided `AccessMode`.
  // Read: Existing.
  // Write: Truncate.
  // ReadAndWrite: Existing.
  Default,

  // Either create a new file or open an existing file.
  Always,

  // Like `Always`, but also erase the contents of an existing file.
  Truncate,

  // Require a file to already exist. Fail otherwise.
  Existing,

  // Require a new file be created. Fail if one already exists.
  Create,
};

// This file wrapper avoids use of the underlying system file position.
// It keeps track of its own file position and read/write calls directly use it.
// This makes copied handles entirely thread safe.
class DirectIOFile final
{
public:
  DirectIOFile();
  ~DirectIOFile();

  // Copies and moves are allowed.
  DirectIOFile(const DirectIOFile&);
  DirectIOFile& operator=(const DirectIOFile&);
  DirectIOFile(DirectIOFile&&);
  DirectIOFile& operator=(DirectIOFile&&);

  explicit DirectIOFile(const std::string& path, AccessMode access_mode,
                        OpenMode open_mode = OpenMode::Default);

  bool Open(const std::string& path, AccessMode access_mode,
            OpenMode open_mode = OpenMode::Default);

  bool Close();

  bool IsOpen() const;

  // An offset from the start of the file may be specified directly.
  // These explicit offset versions entirely ignore the current file position.
  // They are thread safe, even when used on the same object.

  bool OffsetRead(u64 offset, u8* out_ptr, u64 size);
  bool OffsetRead(u64 offset, std::span<u8> out_data)
  {
    return OffsetRead(offset, out_data.data(), out_data.size());
  }
  bool OffsetWrite(u64 offset, const u8* in_ptr, u64 size);
  bool OffsetWrite(auto offset, std::span<const u8> in_data)
  {
    return OffsetWrite(offset, in_data.data(), in_data.size());
  }

  // These Read/Write functions advance the current position on success.

  bool Read(u8* out_ptr, u64 size)
  {
    if (!OffsetRead(m_current_offset, out_ptr, size))
      return false;
    m_current_offset += size;
    return true;
  }
  bool Read(std::span<u8> out_data) { return Read(out_data.data(), out_data.size()); }

  bool Write(const u8* in_ptr, u64 size)
  {
    if (!OffsetWrite(m_current_offset, in_ptr, size))
      return false;
    m_current_offset += size;
    return true;
  }
  bool Write(std::span<const u8> in_data) { return Write(in_data.data(), in_data.size()); }

  // Returns 0 on error.
  u64 GetSize() const;

  bool Seek(s64 offset, SeekOrigin origin);

  // Returns 0 when not open.
  u64 Tell() const { return m_current_offset; }

  bool Flush();

  auto GetHandle() const
  {
#if defined(_WIN32)
    return m_handle;
#else
    return m_fd;
#endif
  }

private:
  void Swap(DirectIOFile& other);
  DirectIOFile Duplicate() const;

#if defined(_WIN32)
  // A workaround to avoid including <windows.h> in this header.
  // HANDLE is just void* and INVALID_HANDLE_VALUE is -1.
  using HandleType = void*;
  HandleType m_handle{HandleType(-1)};
#else
  using HandleType = int;
  HandleType m_fd{-1};
#endif

  u64 m_current_offset{};
};

// These take an open file handle to avoid failures from other processes trying to open our files.
// This is mainly an issue on Windows.

bool Resize(DirectIOFile& file, u64 size);

// Attempts to replace a destination file if one already exists.
// Note: Windows uses `file`. Elsewhere uses `source_path`, but `file` is checked for openness.
bool Rename(DirectIOFile& file, const std::string& source_path,
            const std::string& destination_path);
// Note: Ditto, only Windows actually uses the file handle. Provide both.
bool Delete(DirectIOFile& file, const std::string& filename);

}  // namespace File
