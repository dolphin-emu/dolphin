// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <vector>

#include "Common/Assert.h"
#include "Common/Common.h"
#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#ifdef __APPLE__
#include "Common/DynamicLibrary.h"
#endif
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

#ifdef _WIN32
#include <windows.h>
#include <Shlwapi.h>
#include <commdlg.h>  // for GetSaveFileName
#include <direct.h>   // getcwd
#include <io.h>
#include <objbase.h>  // guid stuff
#include <shellapi.h>
#else
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CFBundle.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <mach-o/dyld.h>
#include <sys/param.h>
#endif

#ifdef ANDROID
#include "Common/StringUtil.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#endif

#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif

// This namespace has various generic functions related to files and paths.
// The code still needs a ton of cleanup.
// REMEMBER: strdup considered harmful!
namespace File
{
#ifdef ANDROID
static std::string s_android_sys_directory;
#endif

#ifdef __APPLE__
static Common::DynamicLibrary s_security_framework;

using DolSecTranslocateIsTranslocatedURL = Boolean (*)(CFURLRef path, bool* isTranslocated,
                                                       CFErrorRef* __nullable error);
using DolSecTranslocateCreateOriginalPathForURL = CFURLRef
__nullable (*)(CFURLRef translocatedPath, CFErrorRef* __nullable error);

static DolSecTranslocateIsTranslocatedURL s_is_translocated_url;
static DolSecTranslocateCreateOriginalPathForURL s_create_orig_path;
#endif

#ifdef _WIN32
FileInfo::FileInfo(const std::string& path)
{
  m_exists = _tstat64(UTF8ToTStr(path).c_str(), &m_stat) == 0;
}

FileInfo::FileInfo(const char* path) : FileInfo(std::string(path))
{
}
#else
FileInfo::FileInfo(const std::string& path) : FileInfo(path.c_str())
{
}

FileInfo::FileInfo(const char* path)
{
#ifdef ANDROID
  if (IsPathAndroidContent(path))
    AndroidContentInit(path);
  else
#endif
    m_exists = stat(path, &m_stat) == 0;
}
#endif

FileInfo::FileInfo(int fd)
{
  m_exists = fstat(fd, &m_stat) == 0;
}

#ifdef ANDROID
void FileInfo::AndroidContentInit(const std::string& path)
{
  const jlong result = GetAndroidContentSizeAndIsDirectory(path);
  m_exists = result != -1;
  m_stat.st_mode = result == -2 ? S_IFDIR : S_IFREG;
  m_stat.st_size = result >= 0 ? result : 0;
}
#endif

bool FileInfo::Exists() const
{
  return m_exists;
}

bool FileInfo::IsDirectory() const
{
  return m_exists ? S_ISDIR(m_stat.st_mode) : false;
}

bool FileInfo::IsFile() const
{
  return m_exists ? !S_ISDIR(m_stat.st_mode) : false;
}

u64 FileInfo::GetSize() const
{
  return IsFile() ? m_stat.st_size : 0;
}

// Returns true if the path exists
bool Exists(const std::string& path)
{
  return FileInfo(path).Exists();
}

// Returns true if the path exists and is a directory
bool IsDirectory(const std::string& path)
{
#ifdef _WIN32
  return PathIsDirectory(UTF8ToWString(path).c_str());
#else
  return FileInfo(path).IsDirectory();
#endif
}

// Returns true if the path exists and is a file
bool IsFile(const std::string& path)
{
  return FileInfo(path).IsFile();
}

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string& filename, IfAbsentBehavior behavior)
{
  INFO_LOG_FMT(COMMON, "Delete: file {}", filename);

#ifdef ANDROID
  if (StringBeginsWith(filename, "content://"))
  {
    const bool success = DeleteAndroidContent(filename);
    if (!success)
      WARN_LOG_FMT(COMMON, "Delete failed on {}", filename);
    return success;
  }
#endif

  const FileInfo file_info(filename);

  // Return true because we care about the file not being there, not the actual delete.
  if (!file_info.Exists())
  {
    if (behavior == IfAbsentBehavior::ConsoleWarning)
    {
      WARN_LOG_FMT(COMMON, "Delete: {} does not exist", filename);
    }
    return true;
  }

  // We can't delete a directory
  if (file_info.IsDirectory())
  {
    WARN_LOG_FMT(COMMON, "Delete failed: {} is a directory", filename);
    return false;
  }

#ifdef _WIN32
  if (!DeleteFile(UTF8ToTStr(filename).c_str()))
  {
    WARN_LOG_FMT(COMMON, "Delete: DeleteFile failed on {}: {}", filename, GetLastErrorString());
    return false;
  }
#else
  if (unlink(filename.c_str()) == -1)
  {
    WARN_LOG_FMT(COMMON, "Delete: unlink failed on {}: {}", filename, LastStrerrorString());
    return false;
  }
#endif

  return true;
}

// Returns true if successful, or path already exists.
bool CreateDir(const std::string& path)
{
  INFO_LOG_FMT(COMMON, "CreateDir: directory {}", path);
#ifdef _WIN32
  if (::CreateDirectory(UTF8ToTStr(path).c_str(), nullptr))
    return true;
  const DWORD error = GetLastError();
  if (error == ERROR_ALREADY_EXISTS)
  {
    WARN_LOG_FMT(COMMON, "CreateDir: CreateDirectory failed on {}: already exists", path);
    return true;
  }
  ERROR_LOG_FMT(COMMON, "CreateDir: CreateDirectory failed on {}: {}", path, error);
  return false;
#else
  if (mkdir(path.c_str(), 0755) == 0)
    return true;

  const int err = errno;

  if (err == EEXIST)
  {
    WARN_LOG_FMT(COMMON, "CreateDir: mkdir failed on {}: already exists", path);
    return true;
  }

  ERROR_LOG_FMT(COMMON, "CreateDir: mkdir failed on {}: {}", path, strerror(err));
  return false;
#endif
}

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string& fullPath)
{
  int panicCounter = 100;
  INFO_LOG_FMT(COMMON, "CreateFullPath: path {}", fullPath);

  if (Exists(fullPath))
  {
    INFO_LOG_FMT(COMMON, "CreateFullPath: path exists {}", fullPath);
    return true;
  }

  size_t position = 0;
  while (true)
  {
    // Find next sub path
    position = fullPath.find(DIR_SEP_CHR, position);

    // we're done, yay!
    if (position == fullPath.npos)
      return true;

    // Include the '/' so the first call is CreateDir("/") rather than CreateDir("")
    std::string const subPath(fullPath.substr(0, position + 1));
    if (!IsDirectory(subPath))
      File::CreateDir(subPath);

    // A safety check
    panicCounter--;
    if (panicCounter <= 0)
    {
      ERROR_LOG_FMT(COMMON, "CreateFullPath: directory structure is too deep");
      return false;
    }
    position++;
  }
}

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string& filename, IfAbsentBehavior behavior)
{
  INFO_LOG_FMT(COMMON, "DeleteDir: directory {}", filename);

  // Return true because we care about the directory not being there, not the actual delete.
  if (!File::Exists(filename))
  {
    if (behavior == IfAbsentBehavior::ConsoleWarning)
    {
      WARN_LOG_FMT(COMMON, "DeleteDir: {} does not exist", filename);
    }
    return true;
  }

  // check if a directory
  if (!IsDirectory(filename))
  {
    ERROR_LOG_FMT(COMMON, "DeleteDir: Not a directory {}", filename);
    return false;
  }

#ifdef _WIN32
  if (::RemoveDirectory(UTF8ToTStr(filename).c_str()))
    return true;
  ERROR_LOG_FMT(COMMON, "DeleteDir: RemoveDirectory failed on {}: {}", filename,
                GetLastErrorString());
#else
  if (rmdir(filename.c_str()) == 0)
    return true;
  ERROR_LOG_FMT(COMMON, "DeleteDir: rmdir failed on {}: {}", filename, LastStrerrorString());
#endif

  return false;
}

// Repeatedly invokes func until it returns true or max_attempts failures.
// Waits after each failure, with each delay doubling in length.
template <typename FuncType>
static bool AttemptMaxTimesWithExponentialDelay(int max_attempts, std::chrono::milliseconds delay,
                                                std::string_view func_name, const FuncType& func)
{
  for (int failed_attempts = 0; failed_attempts < max_attempts; ++failed_attempts)
  {
    if (func())
    {
      return true;
    }
    if (failed_attempts + 1 < max_attempts)
    {
      INFO_LOG_FMT(COMMON, "{} attempt failed, delaying for {} milliseconds", func_name,
                   delay.count());
      std::this_thread::sleep_for(delay);
      delay *= 2;
    }
  }
  return false;
}

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string& srcFilename, const std::string& destFilename)
{
  INFO_LOG_FMT(COMMON, "Rename: {} --> {}", srcFilename, destFilename);
#ifdef _WIN32
  const std::wstring source_wstring = UTF8ToTStr(srcFilename);
  const std::wstring destination_wstring = UTF8ToTStr(destFilename);

  // On Windows ReplaceFile can fail spuriously due to antivirus checking or other noise.
  // Retry the operation with increasing delays, and if none of them work there's probably a
  // persistent problem.
  const bool success = AttemptMaxTimesWithExponentialDelay(
      3, std::chrono::milliseconds(5), "Rename", [&source_wstring, &destination_wstring] {
        if (ReplaceFile(destination_wstring.c_str(), source_wstring.c_str(), nullptr,
                        REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
        {
          return true;
        }
        // Might have failed because the destination doesn't exist.
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
          return MoveFile(source_wstring.c_str(), destination_wstring.c_str()) != 0;
        }
        return false;
      });
  constexpr auto error_string_func = GetLastErrorString;
#else
  const bool success = rename(srcFilename.c_str(), destFilename.c_str()) == 0;
  constexpr auto error_string_func = LastStrerrorString;
#endif
  if (!success)
  {
    ERROR_LOG_FMT(COMMON, "Rename: rename failed on {} --> {}: {}", srcFilename, destFilename,
                  error_string_func());
  }
  return success;
}

#ifndef _WIN32
static void FSyncPath(const char* path)
{
  int fd = open(path, O_RDONLY);
  if (fd != -1)
  {
    fsync(fd);
    close(fd);
  }
}
#endif

bool RenameSync(const std::string& srcFilename, const std::string& destFilename)
{
  if (!Rename(srcFilename, destFilename))
    return false;
#ifdef _WIN32
  int fd = _topen(UTF8ToTStr(srcFilename).c_str(), _O_RDONLY);
  if (fd != -1)
  {
    _commit(fd);
    close(fd);
  }
#else
  char* path = strdup(srcFilename.c_str());
  FSyncPath(path);
  FSyncPath(dirname(path));
  free(path);
  path = strdup(destFilename.c_str());
  FSyncPath(dirname(path));
  free(path);
#endif
  return true;
}

// copies file source_path to destination_path, returns true on success
bool Copy(const std::string& source_path, const std::string& destination_path)
{
  INFO_LOG_FMT(COMMON, "Copy: {} --> {}", source_path, destination_path);
#ifdef _WIN32
  if (CopyFile(UTF8ToTStr(source_path).c_str(), UTF8ToTStr(destination_path).c_str(), FALSE))
    return true;

  ERROR_LOG_FMT(COMMON, "Copy: failed {} --> {}: {}", source_path, destination_path,
                GetLastErrorString());
  return false;
#else
  std::ifstream source{source_path, std::ios::binary};
  std::ofstream destination{destination_path, std::ios::binary};
  destination << source.rdbuf();
  return source.good() && destination.good();
#endif
}

// Returns the size of a file (or returns 0 if the path isn't a file that exists)
u64 GetSize(const std::string& path)
{
  return FileInfo(path).GetSize();
}

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd)
{
  return FileInfo(fd).GetSize();
}

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE* f)
{
  // can't use off_t here because it can be 32-bit
  const u64 pos = ftello(f);
  if (fseeko(f, 0, SEEK_END) != 0)
  {
    ERROR_LOG_FMT(COMMON, "GetSize: seek failed {}: {}", fmt::ptr(f), LastStrerrorString());
    return 0;
  }

  const u64 size = ftello(f);
  if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0))
  {
    ERROR_LOG_FMT(COMMON, "GetSize: seek failed {}: {}", fmt::ptr(f), LastStrerrorString());
    return 0;
  }

  return size;
}

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string& filename)
{
  INFO_LOG_FMT(COMMON, "CreateEmptyFile: {}", filename);

  if (!File::IOFile(filename, "wb"))
  {
    ERROR_LOG_FMT(COMMON, "CreateEmptyFile: failed {}: {}", filename, LastStrerrorString());
    return false;
  }

  return true;
}

// Recursive or non-recursive list of files and directories under directory.
FSTEntry ScanDirectoryTree(const std::string& directory, bool recursive)
{
  INFO_LOG_FMT(COMMON, "ScanDirectoryTree: directory {}", directory);
  FSTEntry parent_entry;
  parent_entry.physicalName = directory;
  parent_entry.isDirectory = true;
  parent_entry.size = 0;
#ifdef _WIN32
  // Find the first file in the directory.
  WIN32_FIND_DATA ffd;

  HANDLE hFind = FindFirstFile(UTF8ToTStr(directory + "\\*").c_str(), &ffd);
  if (hFind == INVALID_HANDLE_VALUE)
  {
    FindClose(hFind);
    return parent_entry;
  }
  // Windows loop
  do
  {
    const std::string virtual_name(TStrToUTF8(ffd.cFileName));
#else
  DIR* dirp = nullptr;

#ifdef ANDROID
  std::vector<std::string> child_names;
  if (IsPathAndroidContent(directory))
  {
    child_names = GetAndroidContentChildNames(directory);
  }
  else
#endif
  {
    dirp = opendir(directory.c_str());
    if (!dirp)
      return parent_entry;
  }

#ifdef ANDROID
  auto it = child_names.cbegin();
#endif

  // non Windows loop
  while (true)
  {
    std::string virtual_name;

#ifdef ANDROID
    if (!dirp)
    {
      if (it == child_names.cend())
        break;
      virtual_name = *it;
      ++it;
    }
    else
#endif
    {
      dirent* result = readdir(dirp);
      if (!result)
        break;
      virtual_name = result->d_name;
    }
#endif
    if (virtual_name == "." || virtual_name == "..")
      continue;
    auto physical_name = directory + DIR_SEP + virtual_name;
    FSTEntry entry;
    const FileInfo file_info(physical_name);
    entry.isDirectory = file_info.IsDirectory();
    if (entry.isDirectory)
    {
      if (recursive)
        entry = ScanDirectoryTree(physical_name, true);
      else
        entry.size = 0;
      parent_entry.size += entry.size;
    }
    else
    {
      entry.size = file_info.GetSize();
    }
    entry.virtualName = virtual_name;
    entry.physicalName = physical_name;

    ++parent_entry.size;
    // Push into the tree
    parent_entry.children.push_back(entry);
#ifdef _WIN32
  } while (FindNextFile(hFind, &ffd) != 0);
  FindClose(hFind);
#else
  }
  if (dirp)
    closedir(dirp);
#endif

  return parent_entry;
}

// Deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string& directory)
{
  INFO_LOG_FMT(COMMON, "DeleteDirRecursively: {}", directory);
  bool success = true;

#ifdef _WIN32
  // Find the first file in the directory.
  WIN32_FIND_DATA ffd;
  HANDLE hFind = FindFirstFile(UTF8ToTStr(directory + "\\*").c_str(), &ffd);

  if (hFind == INVALID_HANDLE_VALUE)
  {
    FindClose(hFind);
    return false;
  }

  // Windows loop
  do
  {
    const std::string virtualName(TStrToUTF8(ffd.cFileName));
#else
  DIR* dirp = opendir(directory.c_str());
  if (!dirp)
    return false;

  // non Windows loop
  while (dirent* result = readdir(dirp))
  {
    const std::string virtualName = result->d_name;
#endif

    // check for "." and ".."
    if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
        ((virtualName[0] == '.') && (virtualName[1] == '.') && (virtualName[2] == '\0')))
      continue;

    std::string newPath = directory + DIR_SEP_CHR + virtualName;
    if (IsDirectory(newPath))
    {
      if (!DeleteDirRecursively(newPath))
      {
        success = false;
        break;
      }
    }
    else
    {
      if (!File::Delete(newPath))
      {
        success = false;
        break;
      }
    }

#ifdef _WIN32
  } while (FindNextFile(hFind, &ffd) != 0);
  FindClose(hFind);
#else
  }
  closedir(dirp);
#endif
  if (success)
    File::DeleteDir(directory);

  return success;
}

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string& source_path, const std::string& dest_path, bool destructive)
{
  if (source_path == dest_path)
    return;
  if (!Exists(source_path))
    return;
  if (!Exists(dest_path))
    File::CreateFullPath(dest_path);

#ifdef _WIN32
  WIN32_FIND_DATA ffd;
  HANDLE hFind = FindFirstFile(UTF8ToTStr(source_path + "\\*").c_str(), &ffd);

  if (hFind == INVALID_HANDLE_VALUE)
  {
    FindClose(hFind);
    return;
  }

  do
  {
    const std::string virtualName(TStrToUTF8(ffd.cFileName));
#else
  DIR* dirp = opendir(source_path.c_str());
  if (!dirp)
    return;

  while (dirent* result = readdir(dirp))
  {
    const std::string virtualName(result->d_name);
#endif
    // check for "." and ".."
    if (virtualName == "." || virtualName == "..")
      continue;

    std::string source = source_path + DIR_SEP + virtualName;
    std::string dest = dest_path + DIR_SEP + virtualName;
    if (IsDirectory(source))
    {
      if (!Exists(dest))
        File::CreateFullPath(dest + DIR_SEP);
      CopyDir(source, dest, destructive);
    }
    else if (!destructive && !Exists(dest))
    {
      Copy(source, dest);
    }
    else if (destructive)
    {
      Rename(source, dest);
    }
#ifdef _WIN32
  } while (FindNextFile(hFind, &ffd) != 0);
  FindClose(hFind);
#else
  }
  closedir(dirp);
#endif
}

// Returns the current directory
std::string GetCurrentDir()
{
  // Get the current working directory (getcwd uses malloc)
  char* dir = __getcwd(nullptr, 0);
  if (!dir)
  {
    ERROR_LOG_FMT(COMMON, "GetCurrentDirectory failed: {}", LastStrerrorString());
    return "";
  }
  std::string strDir = dir;
  free(dir);
  return strDir;
}

// Sets the current directory to the given directory
bool SetCurrentDir(const std::string& directory)
{
  return __chdir(directory.c_str()) == 0;
}

std::string CreateTempDir()
{
#ifdef _WIN32
  TCHAR temp[MAX_PATH];
  if (!GetTempPath(MAX_PATH, temp))
    return "";

  GUID guid;
  if (FAILED(CoCreateGuid(&guid)))
  {
    return "";
  }
  OLECHAR tguid[40]{};
  if (!StringFromGUID2(guid, tguid, _countof(tguid)))
  {
    return "";
  }
  std::string dir = TStrToUTF8(temp) + "/" + TStrToUTF8(tguid);
  if (!CreateDir(dir))
    return "";
  dir = ReplaceAll(dir, "\\", DIR_SEP);
  return dir;
#else
  const char* base = getenv("TMPDIR") ?: "/tmp";
  std::string path = std::string(base) + "/DolphinWii.XXXXXX";
  if (!mkdtemp(&path[0]))
    return "";
  return path;
#endif
}

std::string GetTempFilenameForAtomicWrite(std::string path)
{
#ifdef _WIN32
  std::unique_ptr<TCHAR[], decltype(&std::free)> absbuf{
      _tfullpath(nullptr, UTF8ToTStr(path).c_str(), 0), std::free};
  if (absbuf != nullptr)
  {
    path = TStrToUTF8(absbuf.get());
  }
#else
  char absbuf[PATH_MAX];
  if (realpath(path.c_str(), absbuf) != nullptr)
    path = absbuf;
#endif
  return std::move(path) + ".xxx";
}

#if defined(__APPLE__)
std::string GetBundleDirectory()
{
  CFURLRef bundle_ref = CFBundleCopyBundleURL(CFBundleGetMainBundle());

  // Starting in macOS Sierra, apps downloaded from the Internet may be
  // "translocated" to a read-only DMG and executed from there. This is
  // done to prevent a scenario where an attacker can replace a trusted
  // app's resources to load untrusted code.
  //
  // We should return Dolphin's actual location on the filesystem in
  // this function, so bundle_ref will be untranslocated if necessary.
  //
  // More information: https://objective-see.com/blog/blog_0x15.html

  // The APIs to deal with translocated paths are private, so we have
  // to dynamically load them from the Security framework.
  //
  // The headers can be found under "Security" on opensource.apple.com:
  // Security/OSX/libsecurity_translocate/lib/SecTranslocate.h
  if (!s_security_framework.IsOpen())
  {
    s_security_framework.Open("/System/Library/Frameworks/Security.framework/Security");
    s_security_framework.GetSymbol("SecTranslocateIsTranslocatedURL", &s_is_translocated_url);
    s_security_framework.GetSymbol("SecTranslocateCreateOriginalPathForURL", &s_create_orig_path);
  }

  bool is_translocated = false;
  s_is_translocated_url(bundle_ref, &is_translocated, nullptr);

  if (is_translocated)
  {
    CFURLRef untranslocated_ref = s_create_orig_path(bundle_ref, nullptr);
    CFRelease(bundle_ref);
    bundle_ref = untranslocated_ref;
  }

  char app_bundle_path[MAXPATHLEN];
  CFStringRef bundle_path = CFURLCopyFileSystemPath(bundle_ref, kCFURLPOSIXPathStyle);
  CFStringGetFileSystemRepresentation(bundle_path, app_bundle_path, sizeof(app_bundle_path));
  CFRelease(bundle_ref);
  CFRelease(bundle_path);

  return app_bundle_path;
}
#endif

std::string GetExePath()
{
  static const std::string dolphin_path = [] {
    std::string result;
#ifdef _WIN32
    auto dolphin_exe_path = GetModuleName(nullptr);
    if (dolphin_exe_path)
    {
      std::unique_ptr<TCHAR[], decltype(&std::free)> dolphin_exe_expanded_path{
          _tfullpath(nullptr, dolphin_exe_path->c_str(), 0), std::free};
      if (dolphin_exe_expanded_path)
      {
        result = TStrToUTF8(dolphin_exe_expanded_path.get());
      }
      else
      {
        result = TStrToUTF8(*dolphin_exe_path);
      }
    }
#elif defined(__APPLE__)
    result = GetBundleDirectory();
    result = result.substr(0, result.find_last_of("Dolphin.app/Contents/MacOS") + 1);
#else
    char dolphin_exe_path[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", dolphin_exe_path, sizeof(dolphin_exe_path));
    if (len == -1 || len == sizeof(dolphin_exe_path))
    {
      len = 0;
    }
    dolphin_exe_path[len] = '\0';
    result = dolphin_exe_path;
#endif
    return result;
  }();
  return dolphin_path;
}

std::string GetExeDirectory()
{
  std::string exe_path = GetExePath();
#ifdef _WIN32
  return exe_path.substr(0, exe_path.rfind('\\'));
#else
  return exe_path.substr(0, exe_path.rfind('/'));
#endif
}

std::string GetSysDirectory()
{
  std::string sysDir;

#if defined(_WIN32) || defined(LINUX_LOCAL_DEV)
#define SYSDATA_DIR "Sys"
#elif defined __APPLE__
#define SYSDATA_DIR "Contents/Resources/Sys"
#else
#ifdef DATA_DIR
#define SYSDATA_DIR DATA_DIR "sys"
#else
#define SYSDATA_DIR "sys"
#endif
#endif

#if defined(__APPLE__)
  sysDir = GetBundleDirectory() + DIR_SEP + SYSDATA_DIR;
#elif defined(_WIN32) || defined(LINUX_LOCAL_DEV)
  sysDir = GetExeDirectory() + DIR_SEP + SYSDATA_DIR;
#elif defined ANDROID
  sysDir = s_android_sys_directory;
  ASSERT_MSG(COMMON, !sysDir.empty(), "Sys directory has not been set");
#else
  sysDir = SYSDATA_DIR;
#endif
  sysDir += DIR_SEP;

  INFO_LOG_FMT(COMMON, "GetSysDirectory: Setting to {}:", sysDir);
  return sysDir;
}

#ifdef ANDROID
void SetSysDirectory(const std::string& path)
{
  INFO_LOG_FMT(COMMON, "Setting Sys directory to {}", path);
  s_android_sys_directory = path;
}
#endif

static std::string s_user_paths[NUM_PATH_INDICES];
static void RebuildUserDirectories(unsigned int dir_index)
{
  switch (dir_index)
  {
  case D_USER_IDX:
    s_user_paths[D_GCUSER_IDX] = s_user_paths[D_USER_IDX] + GC_USER_DIR DIR_SEP;
    s_user_paths[D_WIIROOT_IDX] = s_user_paths[D_USER_IDX] + WII_USER_DIR;
    s_user_paths[D_CONFIG_IDX] = s_user_paths[D_USER_IDX] + CONFIG_DIR DIR_SEP;
    s_user_paths[D_GAMESETTINGS_IDX] = s_user_paths[D_USER_IDX] + GAMESETTINGS_DIR DIR_SEP;
    s_user_paths[D_MAPS_IDX] = s_user_paths[D_USER_IDX] + MAPS_DIR DIR_SEP;
    s_user_paths[D_CACHE_IDX] = s_user_paths[D_USER_IDX] + CACHE_DIR DIR_SEP;
    s_user_paths[D_COVERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + COVERCACHE_DIR DIR_SEP;
    s_user_paths[D_REDUMPCACHE_IDX] = s_user_paths[D_CACHE_IDX] + REDUMPCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + SHADERCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERS_IDX] = s_user_paths[D_USER_IDX] + SHADERS_DIR DIR_SEP;
    s_user_paths[D_STATESAVES_IDX] = s_user_paths[D_USER_IDX] + STATESAVES_DIR DIR_SEP;
    s_user_paths[D_SCREENSHOTS_IDX] = s_user_paths[D_USER_IDX] + SCREENSHOTS_DIR DIR_SEP;
    s_user_paths[D_LOAD_IDX] = s_user_paths[D_USER_IDX] + LOAD_DIR DIR_SEP;
    s_user_paths[D_HIRESTEXTURES_IDX] = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMP_IDX] = s_user_paths[D_USER_IDX] + DUMP_DIR DIR_SEP;
    s_user_paths[D_DUMPFRAMES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
    s_user_paths[D_DUMPOBJECTS_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_OBJECTS_DIR DIR_SEP;
    s_user_paths[D_DUMPAUDIO_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
    s_user_paths[D_DUMPTEXTURES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMPDSP_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
    s_user_paths[D_DUMPSSL_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_SSL_DIR DIR_SEP;
    s_user_paths[D_LOGS_IDX] = s_user_paths[D_USER_IDX] + LOGS_DIR DIR_SEP;
    s_user_paths[D_MAILLOGS_IDX] = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
    s_user_paths[D_THEMES_IDX] = s_user_paths[D_USER_IDX] + THEMES_DIR DIR_SEP;
    s_user_paths[D_STYLES_IDX] = s_user_paths[D_USER_IDX] + STYLES_DIR DIR_SEP;
    s_user_paths[D_PIPES_IDX] = s_user_paths[D_USER_IDX] + PIPES_DIR DIR_SEP;
    s_user_paths[D_WFSROOT_IDX] = s_user_paths[D_USER_IDX] + WFSROOT_DIR DIR_SEP;
    s_user_paths[D_BACKUP_IDX] = s_user_paths[D_USER_IDX] + BACKUP_DIR DIR_SEP;
    s_user_paths[D_RESOURCEPACK_IDX] = s_user_paths[D_USER_IDX] + RESOURCEPACK_DIR DIR_SEP;
    s_user_paths[D_DYNAMICINPUT_IDX] = s_user_paths[D_LOAD_IDX] + DYNAMICINPUT_DIR DIR_SEP;
    s_user_paths[F_DOLPHINCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
    s_user_paths[F_GCPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCPAD_CONFIG;
    s_user_paths[F_WIIPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + WIIPAD_CONFIG;
    s_user_paths[F_GCKEYBOARDCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCKEYBOARD_CONFIG;
    s_user_paths[F_GFXCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GFX_CONFIG;
    s_user_paths[F_DEBUGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
    s_user_paths[F_LOGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
    s_user_paths[F_DUALSHOCKUDPCLIENTCONFIG_IDX] =
        s_user_paths[D_CONFIG_IDX] + DUALSHOCKUDPCLIENT_CONFIG;
    s_user_paths[F_FREELOOKCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + FREELOOK_CONFIG;
    s_user_paths[F_MAINLOG_IDX] = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
    s_user_paths[F_MEM1DUMP_IDX] = s_user_paths[D_DUMP_IDX] + MEM1_DUMP;
    s_user_paths[F_MEM2DUMP_IDX] = s_user_paths[D_DUMP_IDX] + MEM2_DUMP;
    s_user_paths[F_ARAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
    s_user_paths[F_FAKEVMEMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
    s_user_paths[F_GCSRAM_IDX] = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
    s_user_paths[F_WIISDCARD_IDX] = s_user_paths[D_WIIROOT_IDX] + DIR_SEP WII_SDCARD;

    s_user_paths[D_MEMORYWATCHER_IDX] = s_user_paths[D_USER_IDX] + MEMORYWATCHER_DIR DIR_SEP;
    s_user_paths[F_MEMORYWATCHERLOCATIONS_IDX] =
        s_user_paths[D_MEMORYWATCHER_IDX] + MEMORYWATCHER_LOCATIONS;
    s_user_paths[F_MEMORYWATCHERSOCKET_IDX] =
        s_user_paths[D_MEMORYWATCHER_IDX] + MEMORYWATCHER_SOCKET;

    s_user_paths[D_GBAUSER_IDX] = s_user_paths[D_USER_IDX] + GBA_USER_DIR DIR_SEP;
    s_user_paths[D_GBASAVES_IDX] = s_user_paths[D_GBAUSER_IDX] + GBASAVES_DIR DIR_SEP;
    s_user_paths[F_GBABIOS_IDX] = s_user_paths[D_GBAUSER_IDX] + GBA_BIOS;

    // The shader cache has moved to the cache directory, so remove the old one.
    // TODO: remove that someday.
    File::DeleteDirRecursively(s_user_paths[D_USER_IDX] + SHADERCACHE_LEGACY_DIR DIR_SEP);
    break;

  case D_CONFIG_IDX:
    s_user_paths[F_DOLPHINCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
    s_user_paths[F_GCPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCPAD_CONFIG;
    s_user_paths[F_GCKEYBOARDCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCKEYBOARD_CONFIG;
    s_user_paths[F_WIIPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + WIIPAD_CONFIG;
    s_user_paths[F_GFXCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GFX_CONFIG;
    s_user_paths[F_DEBUGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
    s_user_paths[F_LOGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
    s_user_paths[F_DUALSHOCKUDPCLIENTCONFIG_IDX] =
        s_user_paths[D_CONFIG_IDX] + DUALSHOCKUDPCLIENT_CONFIG;
    s_user_paths[F_FREELOOKCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + FREELOOK_CONFIG;
    break;

  case D_CACHE_IDX:
    s_user_paths[D_COVERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + COVERCACHE_DIR DIR_SEP;
    s_user_paths[D_REDUMPCACHE_IDX] = s_user_paths[D_CACHE_IDX] + REDUMPCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + SHADERCACHE_DIR DIR_SEP;
    break;

  case D_GCUSER_IDX:
    s_user_paths[F_GCSRAM_IDX] = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
    break;

  case D_DUMP_IDX:
    s_user_paths[D_DUMPFRAMES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
    s_user_paths[D_DUMPOBJECTS_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_OBJECTS_DIR DIR_SEP;
    s_user_paths[D_DUMPAUDIO_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
    s_user_paths[D_DUMPTEXTURES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMPDSP_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
    s_user_paths[D_DUMPSSL_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_SSL_DIR DIR_SEP;
    s_user_paths[F_MEM1DUMP_IDX] = s_user_paths[D_DUMP_IDX] + MEM1_DUMP;
    s_user_paths[F_MEM2DUMP_IDX] = s_user_paths[D_DUMP_IDX] + MEM2_DUMP;
    s_user_paths[F_ARAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
    s_user_paths[F_FAKEVMEMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
    break;

  case D_LOGS_IDX:
    s_user_paths[D_MAILLOGS_IDX] = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
    s_user_paths[F_MAINLOG_IDX] = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
    break;

  case D_LOAD_IDX:
    s_user_paths[D_HIRESTEXTURES_IDX] = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DYNAMICINPUT_IDX] = s_user_paths[D_LOAD_IDX] + DYNAMICINPUT_DIR DIR_SEP;
    break;
  }
}

// Gets a set user directory path
// Don't call prior to setting the base user directory
const std::string& GetUserPath(unsigned int dir_index)
{
  return s_user_paths[dir_index];
}

// Sets a user directory path
// Rebuilds internal directory structure to compensate for the new directory
void SetUserPath(unsigned int dir_index, const std::string& path)
{
  if (path.empty())
    return;

  s_user_paths[dir_index] = path;
  RebuildUserDirectories(dir_index);
}

std::string GetThemeDir(const std::string& theme_name)
{
  std::string dir = File::GetUserPath(D_THEMES_IDX) + theme_name + "/";
  if (Exists(dir))
    return dir;

  // If the theme doesn't exist in the user dir, load from shared directory
  dir = GetSysDirectory() + THEMES_DIR "/" + theme_name + "/";
  if (Exists(dir))
    return dir;

  // If the theme doesn't exist at all, load the default theme
  return GetSysDirectory() + THEMES_DIR "/" DEFAULT_THEME_DIR "/";
}

bool WriteStringToFile(const std::string& filename, std::string_view str)
{
  return File::IOFile(filename, "wb").WriteBytes(str.data(), str.size());
}

bool ReadFileToString(const std::string& filename, std::string& str)
{
  File::IOFile file(filename, "rb");

  if (!file)
    return false;

  str.resize(file.GetSize());
  return file.ReadArray(str.data(), str.size());
}

}  // namespace File
