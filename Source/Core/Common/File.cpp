// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstdio>
#include <memory>
#include <streambuf>
#include <string>
#include <sys/stat.h>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#endif

#ifdef ANDROID
#include <android/asset_manager.h>

#include "jni/AndroidAssets.h"
#endif

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace File
{
ReadOnlyFile::~ReadOnlyFile() = default;

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

u64 IOFile::GetSize()
{
  if (!IsOpen())
  {
    ERROR_LOG(COMMON, "GetSize: file not open");
    return 0;
  }

  // can't use off_t here because it can be 32-bit
  u64 pos = ftello(m_file);
  if (fseeko(m_file, 0, SEEK_END) != 0)
  {
    ERROR_LOG(COMMON, "GetSize: seek failed %p: %s", m_file, GetLastErrorMsg().c_str());
    return 0;
  }

  u64 size = ftello(m_file);
  if ((size != pos) && (fseeko(m_file, pos, SEEK_SET) != 0))
  {
    ERROR_LOG(COMMON, "GetSize: seek failed %p: %s", m_file, GetLastErrorMsg().c_str());
    return 0;
  }

  return size;
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

bool IOFile::Read(void* data, size_t element_size, size_t count, size_t* pReadElements)
{
  size_t read_elements = 0;
  if (!IsOpen() || count != (read_elements = std::fread(data, element_size, count, m_file)))
    m_good = false;

  if (pReadElements)
    *pReadElements = read_elements;

  return m_good;
}

bool IOFile::Write(const void* data, size_t element_size, size_t count)
{
  if (!IsOpen() || count != std::fwrite(data, element_size, count, m_file))
    m_good = false;

  return IsGood();
}

#ifdef ANDROID
AndroidAsset::AndroidAsset() : m_file(nullptr), m_good(true)
{
}

AndroidAsset::AndroidAsset(const std::string& path) : m_file(nullptr), m_good(true)
{
  Open(path);
}

AndroidAsset::~AndroidAsset()
{
  Close();
}

AndroidAsset::AndroidAsset(AndroidAsset&& other) noexcept : m_file(nullptr), m_good(true)
{
  Swap(other);
}

AndroidAsset& AndroidAsset::operator=(AndroidAsset&& other) noexcept
{
  Swap(other);
  return *this;
}

void AndroidAsset::Swap(AndroidAsset& other) noexcept
{
  std::swap(m_file, other.m_file);
  std::swap(m_good, other.m_good);
}

bool AndroidAsset::Open(const std::string& path)
{
  Close();
  m_file = AndroidAssets::OpenFile(path.c_str(), AASSET_MODE_UNKNOWN);

  m_good = IsOpen();
  return m_good;
}

bool AndroidAsset::Close()
{
  if (IsOpen())
    AAsset_close(m_file);
  else
    m_good = false;

  m_file = nullptr;
  return m_good;
}

u64 AndroidAsset::GetSize()
{
  if (!IsOpen())
    return 0;

  return AAsset_getLength64(m_file);
}

bool AndroidAsset::Seek(s64 offset, int origin)
{
  if (!IsOpen() || 0 > AAsset_seek64(m_file, offset, origin))
    m_good = false;

  return m_good;
}

u64 AndroidAsset::Tell() const
{
  if (IsOpen())
    return AAsset_getLength64(m_file) - AAsset_getRemainingLength64(m_file);
  else
    return -1;
}

bool AndroidAsset::Read(void* data, size_t element_size, size_t count, size_t* pReadElements)
{
  int bytes_read = 0;
  if (!IsOpen())
  {
    m_good = false;
  }
  else
  {
    size_t bytes_to_read = element_size * count;
    bytes_read = AAsset_read(m_file, data, bytes_to_read);
    if (bytes_read < 0)
    {
      m_good = false;
      bytes_read = 0;
    }
    else if (static_cast<size_t>(bytes_read) != bytes_to_read)
    {
      m_good = false;
    }
  }

  if (pReadElements)
    *pReadElements = bytes_read / element_size;

  return m_good;
}
#endif

ReadOnlyFileBuffer::ReadOnlyFileBuffer(ReadOnlyFile* file, size_t buffer_size)
    : m_file(file), m_buffer(std::vector<char>(buffer_size))
{
}

int ReadOnlyFileBuffer::underflow()
{
  if (gptr() == egptr())
  {
    size_t bytes_read;
    m_file->ReadArray(m_buffer.data(), m_buffer.size(), &bytes_read);
    setg(m_buffer.data(), m_buffer.data(), m_buffer.data() + bytes_read);
  }

  return gptr() == egptr() ? std::char_traits<char>::eof() :
                             std::char_traits<char>::to_int_type(*gptr());
}

DirectoryIterator::~DirectoryIterator() = default;

StandardDirectoryIterator::StandardDirectoryIterator(const std::string& path) : m_base_path(path)
{
#ifdef _WIN32
  m_handle = FindFirstFile(UTF8ToTStr(path + "\\*").c_str(), &m_find_data);
#else
  m_dir = opendir(path.c_str());
#endif
}

StandardDirectoryIterator::~StandardDirectoryIterator()
{
#ifdef _WIN32
  FindClose(m_handle);
#else
  if (m_dir)
    closedir(m_dir);
#endif
}

std::string StandardDirectoryIterator::NextChild()
{
#ifdef _WIN32
  if (m_handle == INVALID_HANDLE_VALUE)
    return "";

  std::string name;
  if (m_first_child)
    name = TStrToUTF8(m_find_data.cFileName);
  else if (FindNextFile(m_handle, &m_find_data))
    name = TStrToUTF8(m_find_data.cFileName);
  else
    return "";

  m_first_child = false;
#else
  if (!m_dir)
    return "";

  dirent* result = readdir(m_dir);
  if (!result)
    return "";

  std::string name = result->d_name;
#endif

  if (name == "." || name == "..")
    return NextChild();

  return m_base_path + '/' + name;
}

#ifdef ANDROID
AndroidAssetDirectoryIterator::AndroidAssetDirectoryIterator(const std::string& path)
{
  // Note: Even if the directory doesn't exist, AAssetManager_openDir won't return nullptr
  m_dir = AndroidAssets::OpenDirectory(path.c_str());
}

AndroidAssetDirectoryIterator::~AndroidAssetDirectoryIterator()
{
  AAssetDir_close(m_dir);
}

std::string AndroidAssetDirectoryIterator::NextChild()
{
  const char* path = AAssetDir_getNextFileName(m_dir);
  if (!path)
    return "";  // End of directory
  return path;
}
#endif

Path::const_iterator::const_iterator() : m_child_path(std::make_shared<Path>()), m_iterator(nullptr)
{
}

Path::const_iterator::const_iterator(const Path& path) : m_child_path(std::make_shared<Path>(path))
{
#ifdef ANDROID
  if (path.m_is_asset)
    m_iterator = std::make_shared<AndroidAssetDirectoryIterator>(path.m_path);
#endif
  m_iterator = std::make_shared<StandardDirectoryIterator>(path.m_path);
}

Path::const_iterator& Path::const_iterator::operator++()
{
  m_child_path->m_path = m_iterator->NextChild();
#ifdef ANDROID
  if (m_child_path->m_path.empty())    // If m_iterator has reached its end,
    m_child_path->m_is_asset = false;  // make this object equal to Path::end()
#endif
  return *this;
}

Path::const_iterator Path::const_iterator::operator++(int)
{
  const_iterator old = *this;
  m_child_path->m_path = m_iterator->NextChild();
  return old;
}

Path Path::operator+(const std::string& str) const
{
  return Path(m_path + str);
}

Path& Path::operator+=(const std::string& str)
{
  m_path += str;
  return *this;
}

std::string Path::GetName() const
{
  std::string name;
  SplitPath(m_path, nullptr, &name);
  return name;
}

bool Path::Exists() const
{
#ifdef ANDROID
  if (m_is_asset)
    return AndroidAsset(m_path).IsOpen() || IsDirectory();
#endif

  struct stat file_info;

#ifdef _WIN32
  int result = _tstat64(UTF8ToTStr(m_path).c_str(), &file_info);
#else
  int result = stat(m_path.c_str(), &file_info);
#endif

  return result == 0;
}

bool Path::IsDirectory() const
{
#ifdef ANDROID
  if (m_is_asset)
    return !AndroidAssetDirectoryIterator(m_path).NextChild().empty();
#endif

  struct stat file_info;

#ifdef _WIN32
  int result = _tstat64(UTF8ToTStr(m_path).c_str(), &file_info);
#else
  int result = stat(m_path.c_str(), &file_info);
#endif

  if (result < 0)
  {
    WARN_LOG(COMMON, "IsDirectory: stat failed on %s: %s", m_path.c_str(), strerror(errno));
    return false;
  }

  return (file_info.st_mode & S_IFMT) == S_IFDIR;
}

u64 Path::GetFileSize() const
{
#ifdef ANDROID
  if (m_is_asset)
    return AndroidAsset(m_path).GetSize();
#endif

  struct stat file_info;
#ifdef _WIN32
  int result = _tstat64(UTF8ToTStr(m_path).c_str(), &file_info);
#else
  int result = stat(m_path.c_str(), &file_info);
#endif

  if (result != 0)
  {
    WARN_LOG(COMMON, "GetSize: failed: No such file");
    return 0;
  }
  else if ((file_info.st_mode & S_IFMT) == S_IFDIR)
  {
    WARN_LOG(COMMON, "GetSize: failed: is a directory");
    return 0;
  }

  return file_info.st_size;
}

std::unique_ptr<ReadOnlyFile> Path::OpenFile(bool binary) const
{
#ifdef ANDROID
  if (m_is_asset)
    return std::make_unique<AndroidAsset>(m_path);
#endif
  return std::make_unique<IOFile>(m_path, binary ? "rb" : "r");
}

}  // namespace File
