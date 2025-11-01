// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/DirectIOFile.h"

#include <cassert>
#include <utility>

#if defined(_WIN32)
#include <string_view>

#include <windows.h>

#include "Common/CommonFuncs.h"
#include "Common/StringUtil.h"
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "Common/Assert.h"

namespace File
{
DirectIOFile::DirectIOFile() = default;

DirectIOFile::~DirectIOFile()
{
  Close();
}

DirectIOFile::DirectIOFile(const DirectIOFile& other)
{
  *this = other.Duplicate();
}

DirectIOFile& DirectIOFile::operator=(const DirectIOFile& other)
{
  return *this = other.Duplicate();
}

DirectIOFile::DirectIOFile(DirectIOFile&& other)
{
  Swap(other);
}

DirectIOFile& DirectIOFile::operator=(DirectIOFile&& other)
{
  Close();
  Swap(other);
  return *this;
}

DirectIOFile::DirectIOFile(const std::string& path, const char open_mode[])
{
  Open(path, open_mode);
}

bool DirectIOFile::Open(const std::string& path, const char open_mode[])
{
  ASSERT(!IsOpen());

  m_current_offset = 0;

#ifdef _WIN32
  const std::string_view open_mode_str{open_mode};

  DWORD desired_access = 0;
  DWORD share_mode = 0;
  DWORD creation_disposition = OPEN_EXISTING;

  if (open_mode_str.contains('r'))
  {
    desired_access |= GENERIC_READ;
    share_mode |= FILE_SHARE_READ;
  }
  if (open_mode_str.contains('w'))
  {
    desired_access |= GENERIC_WRITE;
    share_mode |= FILE_SHARE_WRITE;
    creation_disposition = OPEN_ALWAYS;
  }

  m_handle = CreateFile(UTF8ToTStr(path).c_str(), desired_access, share_mode, nullptr,
                        creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (!IsOpen())
    WARN_LOG_FMT(COMMON, "CreateFile: {}", Common::GetLastErrorString());
#else
  // Leveraging IOFile to avoid reimplementing OS-specific opening procedures.
  if (IOFile file(path, open_mode); file.IsOpen())
    m_fd = dup(fileno(file.GetHandle()));
#endif

  return IsOpen();
}

bool DirectIOFile::Close()
{
  if (!IsOpen())
    return true;

#if defined(_WIN32)
  return CloseHandle(std::exchange(m_handle, INVALID_HANDLE_VALUE)) != 0;
#else
  return close(std::exchange(m_fd, -1)) == 0;
#endif
}

bool DirectIOFile::IsOpen() const
{
#if defined(_WIN32)
  return m_handle != INVALID_HANDLE_VALUE;
#else
  return m_fd != -1;
#endif
}

#if defined(_WIN32)
template <auto* TransferFunc>
static bool OverlappedTransfer(HANDLE handle, u64 offset, auto* data_ptr, u64 size)
{
  // ReadFile/WriteFile take a 32bit size so we must loop to handle our 64bit size.
  while (true)
  {
    OVERLAPPED overlapped{};
    overlapped.Offset = DWORD(offset);
    overlapped.OffsetHigh = DWORD(offset >> 32);

    DWORD bytes_transferred{};
    if (TransferFunc(handle, data_ptr, DWORD(size), &bytes_transferred, &overlapped) == 0)
    {
      ERROR_LOG_FMT(COMMON, "OverlappedTransfer: {}", Common::GetLastErrorString());
      return false;
    }

    size -= bytes_transferred;

    if (size == 0)
      return true;

    offset += bytes_transferred;
    data_ptr += bytes_transferred;
  }
}
#endif

bool DirectIOFile::OffsetRead(u64 offset, u8* out_ptr, u64 size)
{
  assert(IsOpen());

#if defined(_WIN32)
  return OverlappedTransfer<ReadFile>(m_handle, offset, out_ptr, size);
#else
  return pread(m_fd, out_ptr, size, off_t(offset)) == ssize_t(size);
#endif
}

bool DirectIOFile::OffsetWrite(u64 offset, const u8* in_ptr, u64 size)
{
  assert(IsOpen());

#if defined(_WIN32)
  return OverlappedTransfer<WriteFile>(m_handle, offset, in_ptr, size);
#else
  return pwrite(m_fd, in_ptr, size, off_t(offset)) == ssize_t(size);
#endif
}

u64 DirectIOFile::GetSize() const
{
  assert(IsOpen());

#if defined(_WIN32)
  LARGE_INTEGER result{};
  if (GetFileSizeEx(m_handle, &result) != 0)
    return result.QuadPart;
#else
  struct stat st{};
  if (fstat(m_fd, &st) == 0)
    return st.st_size;
#endif

  return -1;
}

bool DirectIOFile::Seek(s64 offset, SeekOrigin origin)
{
  assert(IsOpen());

  u64 reference_pos = 0;
  switch (origin)
  {
  case SeekOrigin::Begin:
    break;
  case SeekOrigin::Current:
    reference_pos = m_current_offset;
    break;
  case SeekOrigin::End:
    if (const auto end_pos = GetSize(); end_pos != u64(-1))
      reference_pos = end_pos;
    else
      return false;
    break;
  }

  // Don't let our current offset underflow.
  if (offset < 0 && u64(-offset) > reference_pos)
    return false;

  m_current_offset = reference_pos + offset;
  return true;
}

void DirectIOFile::Swap(DirectIOFile& other)
{
#if defined(_WIN32)
  std::swap(m_handle, other.m_handle);
#else
  std::swap(m_fd, other.m_fd);
#endif
  std::swap(m_current_offset, other.m_current_offset);
}

DirectIOFile DirectIOFile::Duplicate() const
{
  DirectIOFile result;

  if (!IsOpen())
    return result;

#if defined(_WIN32)
  const auto current_process = GetCurrentProcess();
  if (DuplicateHandle(current_process, m_handle, current_process, &result.m_handle, 0, FALSE,
                      DUPLICATE_SAME_ACCESS) == 0)
  {
    ERROR_LOG_FMT(COMMON, "DuplicateHandle: {}", Common::GetLastErrorString());
  }
#else
  result.m_fd = dup(m_fd);
#endif

  ASSERT(result.IsOpen());

  result.m_current_offset = m_current_offset;

  return result;
}

}  // namespace File
