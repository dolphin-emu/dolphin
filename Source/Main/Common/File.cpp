// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"

namespace File
{
IOFile::IOFile() : m_file(nullptr), m_good(true)
{
}

IOFile::IOFile(std::FILE* file) : m_file(file), m_good(true)
{
}

IOFile::IOFile(const std::string& filename, const char openmode[]) : m_file(nullptr), m_good(true)
{
  Open(filename, openmode);
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

bool IOFile::Open(const std::string& filename, const char openmode[])
{
  Close();
#ifdef _WIN32
  m_good = _tfopen_s(&m_file, UTF8ToTStr(filename).c_str(), UTF8ToTStr(openmode).c_str()) == 0;
#else
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

void IOFile::SetHandle(std::FILE* file)
{
  Close();
  Clear();
  m_file = file;
}

u64 IOFile::GetSize() const
{
  if (IsOpen())
    return File::GetSize(m_file);
  else
    return 0;
}

bool IOFile::Seek(s64 off, int origin)
{
  if (!IsOpen() || 0 != fseeko(m_file, off, origin))
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
