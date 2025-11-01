// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"

namespace File
{

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

  // Files are always opened in "binary" mode.
  // `open_mode` can be "r", "w", or both combined.
  // Feel free extend this `open_mode` interface or change it to something more modern.
  explicit DirectIOFile(const std::string& path, const char open_mode[]);
  bool Open(const std::string& path, const char open_mode[]);

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

  // Returns u64(-1) on error.
  u64 GetSize() const;

  bool Seek(s64 offset, SeekOrigin origin);
  u64 Tell() const { return m_current_offset; }

private:
  void Swap(DirectIOFile& other);
  DirectIOFile Duplicate() const;

#if defined(_WIN32)
  // A workaround to avoid including <windows.h> in this header.
  // HANDLE is just void* and INVALID_HANDLE_VALUE is -1.
  using HandleType = void*;
  HandleType m_handle{HandleType(-1)};
#else
  int m_fd{-1};
#endif

  u64 m_current_offset{};
};

}  // namespace File
