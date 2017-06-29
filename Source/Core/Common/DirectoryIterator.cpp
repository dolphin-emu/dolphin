// This file is under the public domain.

#include "Common/DirectoryIterator.h"

#include <string>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#include "Common/FileUtil.h"
#endif

#include "Common/StringUtil.h"

namespace Common
{
DirectoryEntry::DirectoryEntry() = default;

#ifdef _WIN32
DirectoryEntry::DirectoryEntry(const StringType& path)
{
  m_handle = FindFirstFile((path + L"\\*").c_str(), &m_find_data);
  if (IsValid())
    SkipSpecialName();
}
#else
DirectoryEntry::DirectoryEntry(const StringType& path) : m_path(&path)
{
  m_dir = opendir(path.c_str());
  if (m_dir)
    Advance();
}
#endif

DirectoryEntry::~DirectoryEntry()
{
  Close();
}

DirectoryEntry::DirectoryEntry(DirectoryEntry&& other)
{
  Move(std::move(other));
}

DirectoryEntry& DirectoryEntry::operator=(DirectoryEntry&& other)
{
  Close();
  Move(std::move(other));
  return *this;
}

void DirectoryEntry::Close()
{
#ifdef _WIN32
  FindClose(m_handle);
#else
  if (m_dir)
    closedir(m_dir);
#endif
}

void DirectoryEntry::Move(DirectoryEntry&& other)
{
#ifdef _WIN32
  m_handle = other.m_handle;
  m_find_data = other.m_find_data;

  other.m_handle = INVALID_HANDLE_VALUE;
#else
  m_path = other.m_path;
  m_dir = other.m_dir;
  m_dirent = other.m_dirent;

  other.m_dir = nullptr;
#endif
}

void DirectoryEntry::Advance()
{
#ifdef _WIN32
  if (!FindNextFile(m_handle, &m_find_data))
  {
    FindClose(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
    return;
  }
#else
  m_dirent = readdir(m_dir);
  if (!m_dirent)
    return;
#endif

  SkipSpecialName();
}

void DirectoryEntry::SkipSpecialName()
{
  // Advance to the next name if the current name is . or ..
  const CharType* const name = GetName();
  if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
    Advance();
}

bool DirectoryEntry::IsValid() const
{
#ifdef _WIN32
  return m_handle != INVALID_HANDLE_VALUE;
#else
  return m_dirent != nullptr;
#endif
}

#ifdef _WIN32
const wchar_t* DirectoryEntry::GetName() const
{
  return m_find_data.cFileName;
}
#else
const char* DirectoryEntry::GetName() const
{
  return m_dirent->d_name;
}
#endif

bool DirectoryEntry::IsDirectory() const
{
#ifdef _WIN32
  // Fast
  return static_cast<bool>(m_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
#else

#ifdef _DIRENT_HAVE_D_TYPE
  // Fast
  if (m_dirent->d_type != DT_UNKNOWN)
    return m_dirent->d_type == DT_DIR;
#endif

  // Not as fast
  return File::IsDirectory(*m_path + '/' + GetName());
#endif
}

u64 DirectoryEntry::GetSize() const
{
#ifdef _WIN32
  // Fast
  return (static_cast<u64>(m_find_data.nFileSizeHigh) << 32) | m_find_data.nFileSizeLow;
#else
  // Not as fast
  return File::GetSize(*m_path + '/' + GetName());
#endif
}

std::string ToUTF8(const std::wstring& str)
{
  return UTF16ToUTF8(str);
}

const std::string& ToUTF8(const std::string& str)
{
  return str;
}

std::string& ToUTF8(std::string& str)
{
  return str;
}

std::string ToUTF8(std::string&& str)
{
  return str;
}

#ifdef _WIN32
std::wstring ToOSEncoding(const std::string& str)
{
  return UTF8ToUTF16(str);
}
#endif

const DirectoryEntry::StringType& ToOSEncoding(const DirectoryEntry::StringType& str)
{
  return str;
}

DirectoryEntry::StringType& ToOSEncoding(DirectoryEntry::StringType& str)
{
  return str;
}

DirectoryEntry::StringType ToOSEncoding(DirectoryEntry::StringType&& str)
{
  return str;
}

DirectoryIterator::DirectoryIterator() : m_entry(std::make_shared<DirectoryEntry>())
{
}

DirectoryIterator::DirectoryIterator(const DirectoryEntry::StringType& path)
    : m_entry(std::make_shared<DirectoryEntry>(path))
{
}

bool DirectoryIterator::operator==(const DirectoryIterator& other) const
{
  // This implementation is only useful for comparing with end iterators
  return !m_entry->IsValid() && !other.m_entry->IsValid();
}

bool DirectoryIterator::operator!=(const DirectoryIterator& other) const
{
  return !operator==(other);
}

DirectoryIterator& DirectoryIterator::operator++()
{
  m_entry->Advance();
  return *this;
}

DirectoryIterator DirectoryIterator::operator++(int)
{
  DirectoryIterator old = *this;
  m_entry->Advance();
  return old;
}

const DirectoryIterator::value_type& DirectoryIterator::operator*() const
{
  return *m_entry;
}

const DirectoryIterator::value_type* DirectoryIterator::operator->() const
{
  return m_entry.get();
}
};
