// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/IOFile.h"

#include <cstddef>
#include <cstdio>
#include <string>

#ifdef _WIN32
#include <io.h>

#include "Common/CommonFuncs.h"
#include "Common/StringUtil.h"
#else
#include <unistd.h>
#endif

#ifdef ANDROID
#include <algorithm>

#include "jni/AndroidCommon/AndroidCommon.h"
#endif

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

namespace File
{
IOFile::IOFile() : m_file(nullptr), m_good(true)
{
}

IOFile::IOFile(std::FILE* file) : m_file(file), m_good(true)
{
}

IOFile::IOFile(const std::string& filename, const char openmode[], SharedAccess sh)
    : m_file(nullptr), m_good(true)
{
  Open(filename, openmode, sh);
}

IOFile::~IOFile()
{
  Close();
}

IOFile::IOFile(IOFile&& other) noexcept : m_file(nullptr), m_good(true)
{
  Swap(other);
}

IOFile& IOFile::operator=(IOFile&& other) noexcept
{
  Swap(other);
  return *this;
}

void IOFile::Swap(IOFile& other) noexcept
{
  std::swap(m_file, other.m_file);
  std::swap(m_good, other.m_good);
}

bool IOFile::Open(const std::string& filename, const char openmode[],
                  [[maybe_unused]] SharedAccess sh)
{
  Close();

#ifdef _WIN32
  if (sh == SharedAccess::Default)
  {
    m_good = _tfopen_s(&m_file, UTF8ToTStr(filename).c_str(), UTF8ToTStr(openmode).c_str()) == 0;
  }
  else if (sh == SharedAccess::Read)
  {
    m_file = _tfsopen(UTF8ToTStr(filename).c_str(), UTF8ToTStr(openmode).c_str(), SH_DENYWR);
    m_good = m_file != nullptr;
  }
#else
#ifdef ANDROID
  if (IsPathAndroidContent(filename))
    m_file = fdopen(OpenAndroidContent(filename, OpenModeToAndroid(openmode)), openmode);
  else
#endif
    m_file = std::fopen(filename.c_str(), openmode);

  m_good = m_file != nullptr;
#endif

  return m_good;
}

bool IOFile::Close()
{
  if (!IsOpen() || 0 != std::fclose(m_file))
    m_good = false;

  m_file = nullptr;
  return m_good;
}

IOFile IOFile::Duplicate(const char openmode[]) const
{
#ifdef _WIN32
  return IOFile(_fdopen(_dup(_fileno(m_file)), openmode));
#else   // _WIN32
  return IOFile(fdopen(dup(fileno(m_file)), openmode));
#endif  // _WIN32
}

void IOFile::SetHandle(std::FILE* file)
{
  Close();
  ClearError();
  m_file = file;
}

u64 IOFile::GetSize() const
{
  if (IsOpen())
    return File::GetSize(m_file);
  else
    return 0;
}

bool IOFile::Seek(s64 offset, SeekOrigin origin)
{
  int fseek_origin;
  switch (origin)
  {
  case SeekOrigin::Begin:
    fseek_origin = SEEK_SET;
    break;
  case SeekOrigin::Current:
    fseek_origin = SEEK_CUR;
    break;
  case SeekOrigin::End:
    fseek_origin = SEEK_END;
    break;
  default:
    return false;
  }

  if (!IsOpen() || 0 != fseeko(m_file, offset, fseek_origin))
    m_good = false;

  return m_good;
}

u64 IOFile::Tell() const
{
  if (IsOpen())
    return ftello(m_file);
  else
    return UINT64_MAX;
}

bool IOFile::Flush()
{
  if (!IsOpen() || 0 != std::fflush(m_file))
    m_good = false;

  return m_good;
}

bool IOFile::Resize(u64 size)
{
#ifdef _WIN32
  if (!IsOpen() || 0 != _chsize_s(_fileno(m_file), size))
#else
  // TODO: handle 64bit and growing
  if (!IsOpen() || 0 != ftruncate(fileno(m_file), size))
#endif
    m_good = false;

  return m_good;
}

}  // namespace File
