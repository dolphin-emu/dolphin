// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/NonCopyable.h"

#ifdef _WIN32
#include "Common/StringUtil.h"
#endif

namespace File
{
// FileSystem tree node/
struct FSTEntry
{
  bool isDirectory;
  u64 size;                  // File length, or for directories, recursive count of children
  std::string physicalName;  // Name on disk
  std::string virtualName;   // Name in FST names table
  std::vector<FSTEntry> children;
};

// Returns true if file filename exists
bool Exists(const std::string& filename);

// Returns true if filename is a directory
bool IsDirectory(const std::string& filename);

// Returns the size of filename (64bit)
u64 GetSize(const std::string& filename);

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd);

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE* f);

// Returns true if successful, or path already exists.
bool CreateDir(const std::string& filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string& fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string& filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string& filename);

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string& srcFilename, const std::string& destFilename);

// ditto, but syncs the source file and, on Unix, syncs the directories after rename
bool RenameSync(const std::string& srcFilename, const std::string& destFilename);

// copies file srcFilename to destFilename, returns true on success
bool Copy(const std::string& srcFilename, const std::string& destFilename);

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string& filename);

// Recursive or non-recursive list of files and directories under directory.
FSTEntry ScanDirectoryTree(const std::string& directory, bool recursive);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string& directory);

// Returns the current directory
std::string GetCurrentDir();

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string& source_path, const std::string& dest_path);

// Set the current directory to given directory
bool SetCurrentDir(const std::string& directory);

// Creates and returns the path to a new temporary directory.
std::string CreateTempDir();

// Get a filename that can hopefully be atomically renamed to the given path.
std::string GetTempFilenameForAtomicWrite(const std::string& path);

bool WriteStringToFile(const std::string& str, const std::string& filename);
bool ReadFileToString(const std::string& filename, std::string& str);

// simple wrapper for cstdlib file functions to
// hopefully will make error checking easier
// and make forgetting an fclose() harder
class IOFile : public NonCopyable
{
public:
  IOFile();
  IOFile(std::FILE* file);
  IOFile(const std::string& filename, const char openmode[]);

  ~IOFile();

  IOFile(IOFile&& other) noexcept;
  IOFile& operator=(IOFile&& other) noexcept;

  void Swap(IOFile& other) noexcept;

  bool Open(const std::string& filename, const char openmode[]);
  bool Close();

  template <typename T>
  bool ReadArray(T* data, size_t length, size_t* pReadBytes = nullptr)
  {
    size_t read_bytes = 0;
    if (!IsOpen() || length != (read_bytes = std::fread(data, sizeof(T), length, m_file)))
      m_good = false;

    if (pReadBytes)
      *pReadBytes = read_bytes;

    return m_good;
  }

  template <typename T>
  bool WriteArray(const T* data, size_t length)
  {
    if (!IsOpen() || length != std::fwrite(data, sizeof(T), length, m_file))
      m_good = false;

    return m_good;
  }

  bool ReadBytes(void* data, size_t length)
  {
    return ReadArray(reinterpret_cast<char*>(data), length);
  }

  bool WriteBytes(const void* data, size_t length)
  {
    return WriteArray(reinterpret_cast<const char*>(data), length);
  }

  bool IsOpen() const { return nullptr != m_file; }
  // m_good is set to false when a read, write or other function fails
  bool IsGood() const { return m_good; }
  explicit operator bool() const { return IsGood() && IsOpen(); }
  std::FILE* GetHandle() { return m_file; }
  void SetHandle(std::FILE* file);

  bool Seek(s64 off, int origin);
  u64 Tell() const;
  u64 GetSize();
  bool Resize(u64 size);
  bool Flush();

  // clear error state
  void Clear()
  {
    m_good = true;
    std::clearerr(m_file);
  }

private:
  std::FILE* m_file;
  bool m_good;
};

}  // namespace

// To deal with Windows being dumb at unicode:
template <typename T>
void OpenFStream(T& fstream, const std::string& filename, std::ios_base::openmode openmode)
{
#ifdef _WIN32
  fstream.open(UTF8ToTStr(filename).c_str(), openmode);
#else
  fstream.open(filename.c_str(), openmode);
#endif
}
