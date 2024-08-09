// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FileUtil.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <limits.h>
#include <stack>
#include <string>
#include <sys/stat.h>
#include <system_error>
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
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
#include <Windows.h>
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
#include "jni/AndroidCommon/AndroidCommon.h"
#endif

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

namespace fs = std::filesystem;

namespace File
{
#ifdef ANDROID
static std::string s_android_sys_directory;
static std::string s_android_driver_directory;
static std::string s_android_lib_directory;
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

FileInfo::FileInfo(const std::string& path) : FileInfo(path.c_str())
{
}

FileInfo::FileInfo(const char* path)
{
#ifdef ANDROID
  if (IsPathAndroidContent(path))
  {
    const jlong result = GetAndroidContentSizeAndIsDirectory(path);
    m_status.type((result == -2) ? fs::file_type::directory : fs::file_type::regular);
    m_size = (result >= 0) ? result : 0;
    m_exists = result != -1;
  }
  else
#endif
  {
    const auto fs_path = StringToPath(path);
    std::error_code error;
    m_status = fs::status(fs_path, error);
    m_size = fs::file_size(fs_path, error);
    if (error)
      m_size = 0;
    m_exists = fs::exists(m_status);
  }
}

bool FileInfo::Exists() const
{
  return m_exists;
}

bool FileInfo::IsDirectory() const
{
  return fs::is_directory(m_status);
}

bool FileInfo::IsFile() const
{
  return Exists() ? !fs::is_directory(m_status) : false;
}

u64 FileInfo::GetSize() const
{
  if (!IsFile())
    return 0;
  return m_size;
}

// Returns true if the path exists
bool Exists(const std::string& path)
{
  return FileInfo(path).Exists();
}

// Returns true if the path exists and is a directory
bool IsDirectory(const std::string& path)
{
  return FileInfo(path).IsDirectory();
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
  DEBUG_LOG_FMT(COMMON, "{}: file {}", __func__, filename);

#ifdef ANDROID
  if (filename.starts_with("content://"))
  {
    const bool success = DeleteAndroidContent(filename);
    if (!success)
      WARN_LOG_FMT(COMMON, "{} failed on {}", __func__, filename);
    return success;
  }
#endif

  auto native_path = StringToPath(filename);
  std::error_code error;
  auto status = fs::status(native_path, error);

  // Return true because we care about the file not being there, not the actual delete.
  if (!fs::exists(status))
  {
    if (behavior == IfAbsentBehavior::ConsoleWarning)
    {
      WARN_LOG_FMT(COMMON, "{}: {} does not exist", __func__, filename);
    }
    return true;
  }

  // fs::remove can only delete an empty directory. Legacy dolphin behavior is just to bail.
  if (fs::is_directory(status))
  {
    WARN_LOG_FMT(COMMON, "{} failed: {} is a directory", __func__, filename);
    return false;
  }

  if (!fs::remove(native_path, error))
  {
    WARN_LOG_FMT(COMMON, "{}: failed on {}: {}", __func__, filename, error.message());
    return false;
  }

  return true;
}

bool CreateDir(const std::string& path)
{
  DEBUG_LOG_FMT(COMMON, "{}: directory {}", __func__, path);

  std::error_code error;
  auto native_path = StringToPath(path);
  bool success = fs::create_directory(native_path, error);
  // If the path was not created, check if it was a pre-existing directory
  std::error_code error_ignored;
  if (!success && fs::is_directory(native_path, error_ignored))
    success = true;
  if (!success)
    ERROR_LOG_FMT(COMMON, "{}: failed on {}: {}", __func__, path, error.message());
  return success;
}

bool CreateDirs(std::string_view path)
{
  DEBUG_LOG_FMT(COMMON, "{}: directory {}", __func__, path);

  std::error_code error;
  auto native_path = StringToPath(path);
  bool success = fs::create_directories(native_path, error);
  // If the path was not created, check if it was a pre-existing directory
  std::error_code error_ignored;
  if (!success && fs::is_directory(native_path, error_ignored))
    success = true;
  if (!success)
    ERROR_LOG_FMT(COMMON, "{}: failed on {}: {}", __func__, path, error.message());
  return success;
}

bool CreateFullPath(std::string_view fullPath)
{
  DEBUG_LOG_FMT(COMMON, "{}: path {}", __func__, fullPath);

  std::error_code error;
  auto native_path = StringToPath(fullPath).parent_path();
  bool success = fs::create_directories(native_path, error);
  // If the path was not created, check if it was a pre-existing directory
  std::error_code error_ignored;
  if (!success && fs::is_directory(native_path, error_ignored))
    success = true;
  if (!success)
    ERROR_LOG_FMT(COMMON, "{}: failed on {}: {}", __func__, fullPath, error.message());
  return success;
}

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string& filename, IfAbsentBehavior behavior)
{
  DEBUG_LOG_FMT(COMMON, "{}: directory {}", __func__, filename);

  auto native_path = StringToPath(filename);
  std::error_code error;
  auto status = fs::status(native_path, error);

  // Return true because we care about the directory not being there, not the actual delete.
  if (!fs::exists(status))
  {
    if (behavior == IfAbsentBehavior::ConsoleWarning)
    {
      WARN_LOG_FMT(COMMON, "{}: {} does not exist", __func__, filename);
    }
    return true;
  }

  // check if a directory
  if (!fs::is_directory(status))
  {
    ERROR_LOG_FMT(COMMON, "{}: Not a directory {}", __func__, filename);
    return false;
  }

  if (!fs::remove(native_path, error))
  {
    WARN_LOG_FMT(COMMON, "{}: failed on {}: {}", __func__, filename, error.message());
    return false;
  }

  return true;
}

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string& srcFilename, const std::string& destFilename)
{
  DEBUG_LOG_FMT(COMMON, "{}: {} --> {}", __func__, srcFilename, destFilename);
  std::error_code error;
  std::filesystem::rename(StringToPath(srcFilename), StringToPath(destFilename), error);
  if (error)
  {
    ERROR_LOG_FMT(COMMON, "{} failed: {} --> {}: {}", __func__, srcFilename, destFilename,
                  error.message());
  }
  return !error;
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
  int fd = -1;
  // XXX is this really needed?
  errno_t err = _wsopen_s(&fd, UTF8ToWString(srcFilename).c_str(), _O_RDONLY, _SH_DENYNO,
                          _S_IREAD | _S_IWRITE);
  if (!err && fd >= 0)
  {
    if (_commit(fd) != 0)
      ERROR_LOG_FMT(COMMON, "{} sync failed on {}: {}", __func__, srcFilename, err);
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

bool CopyRegularFile(std::string_view source_path, std::string_view destination_path)
{
  DEBUG_LOG_FMT(COMMON, "{}: {} --> {}", __func__, source_path, destination_path);

  auto src_path = StringToPath(source_path);
  auto dst_path = StringToPath(destination_path);
  std::error_code error;
  bool copied = fs::copy_file(src_path, dst_path, fs::copy_options::overwrite_existing, error);
  if (!copied)
  {
    ERROR_LOG_FMT(COMMON, "{}: failed {} --> {}: {}", __func__, source_path, destination_path,
                  error.message());
  }
  return copied;
}

// Returns the size of a file (or returns 0 if the path isn't a file that exists)
u64 GetSize(const std::string& path)
{
  return FileInfo(path).GetSize();
}

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE* f)
{
  // can't use off_t here because it can be 32-bit
  const u64 pos = ftello(f);
  if (fseeko(f, 0, SEEK_END) != 0)
  {
    ERROR_LOG_FMT(COMMON, "GetSize: seek failed {}: {}", fmt::ptr(f), Common::LastStrerrorString());
    return 0;
  }

  const u64 size = ftello(f);
  if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0))
  {
    ERROR_LOG_FMT(COMMON, "GetSize: seek failed {}: {}", fmt::ptr(f), Common::LastStrerrorString());
    return 0;
  }

  return size;
}

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string& filename)
{
  DEBUG_LOG_FMT(COMMON, "CreateEmptyFile: {}", filename);

  if (!File::IOFile(filename, "wb"))
  {
    ERROR_LOG_FMT(COMMON, "CreateEmptyFile: failed {}: {}", filename, Common::LastStrerrorString());
    return false;
  }

  return true;
}

#ifdef ANDROID
static FSTEntry ScanDirectoryTreeAndroidContent(std::string directory, bool recursive)
{
  FSTEntry parent_entry;
  parent_entry.physicalName = directory;
  parent_entry.isDirectory = true;
  parent_entry.size = 0;

  for (const auto& child_name : GetAndroidContentChildNames(directory))
  {
    const auto physical_name = directory + DIR_SEP + child_name;
    const FileInfo file_info(physical_name);
    FSTEntry entry;

    entry.isDirectory = file_info.IsDirectory();
    if (entry.isDirectory)
    {
      if (recursive)
        entry = ScanDirectoryTreeAndroidContent(physical_name, true);
      else
        entry.size = 0;
      parent_entry.size += entry.size;
    }
    else
    {
      entry.size = file_info.GetSize();
    }
    entry.virtualName = child_name;
    entry.physicalName = physical_name;

    ++parent_entry.size;
    parent_entry.children.push_back(entry);
  }

  return parent_entry;
}
#endif

// Recursive or non-recursive list of files and directories under directory.
FSTEntry ScanDirectoryTree(std::string directory, bool recursive)
{
  DEBUG_LOG_FMT(COMMON, "{}: directory {}", __func__, directory);

#ifdef ANDROID
  if (IsPathAndroidContent(directory))
    return ScanDirectoryTreeAndroidContent(directory, recursive);
#endif

  auto path_to_physical_name = [](const fs::path& path) {
#ifdef _WIN32
    // TODO Ideally this would not be needed - dolphin really should not have code directly mucking
    // about with directory separators (for host paths - emulated paths may require it) and instead
    // use fs::path to interact with them.
    auto wpath = path.wstring();
    std::replace(wpath.begin(), wpath.end(), L'\\', L'/');
    return WStringToUTF8(wpath);
#else
    return PathToString(path);
#endif
  };

  auto dirent_to_fstent = [&](const fs::directory_entry& entry) {
    return FSTEntry{
        .isDirectory = entry.is_directory(),
        .size = entry.is_directory() || entry.is_fifo() ? 0 : entry.file_size(),
        .physicalName = path_to_physical_name(entry.path()),
        .virtualName = PathToString(entry.path().filename()),
    };
  };

  auto calc_dir_size = [](FSTEntry* dir) {
    dir->size += dir->children.size();
    for (auto& child : dir->children)
      if (child.isDirectory)
        dir->size += child.size;
  };

  const auto directory_path = StringToPath(directory);

  FSTEntry parent_entry;
  parent_entry.physicalName = path_to_physical_name(directory_path);
  parent_entry.isDirectory = fs::is_directory(directory_path);
  parent_entry.size = 0;

  std::error_code error;
  if (recursive)
  {
    int prev_depth = 0;
    std::stack<FSTEntry*> dir_fsts;
    dir_fsts.push(&parent_entry);
    for (auto it = fs::recursive_directory_iterator(directory_path, error);
         it != fs::recursive_directory_iterator(); it.increment(error))
    {
      const int cur_depth = it.depth();
      if (cur_depth > prev_depth)
      {
        dir_fsts.push(&dir_fsts.top()->children.back());
      }
      else if (cur_depth < prev_depth)
      {
        while (dir_fsts.size() != static_cast<size_t>(cur_depth) + 1u)
        {
          calc_dir_size(dir_fsts.top());
          dir_fsts.pop();
        }
      }
      dir_fsts.top()->children.emplace_back(dirent_to_fstent(*it));
      prev_depth = cur_depth;
    }
    while (dir_fsts.size())
    {
      calc_dir_size(dir_fsts.top());
      dir_fsts.pop();
    }
  }
  else
  {
    for (auto it = fs::directory_iterator(directory_path, error); it != fs::directory_iterator();
         it.increment(error))
    {
      parent_entry.children.emplace_back(dirent_to_fstent(*it));
    }
    calc_dir_size(&parent_entry);
  }

  if (error)
  {
    // NOTE Possibly partial file list still returned
    ERROR_LOG_FMT(COMMON, "{} error on {}: {}", __func__, directory, error.message());
  }

  return parent_entry;
}

// Deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string& directory)
{
  DEBUG_LOG_FMT(COMMON, "{}: {}", __func__, directory);

  std::error_code error;
  const std::uintmax_t num_removed = std::filesystem::remove_all(StringToPath(directory), error);
  const bool success = num_removed != 0 && !error;
  if (!success)
    ERROR_LOG_FMT(COMMON, "{}: {} failed {}", __func__, directory, error.message());
  return success;
}

bool Copy(std::string_view source_path, std::string_view dest_path, bool overwrite_existing)
{
  DEBUG_LOG_FMT(COMMON, "{}: {} --> {} ({})", __func__, source_path, dest_path,
                overwrite_existing ? "overwrite" : "preserve");

  auto src_path = StringToPath(source_path);
  auto dst_path = StringToPath(dest_path);
  std::error_code error;
  auto options = fs::copy_options::recursive;
  if (overwrite_existing)
    options |= fs::copy_options::overwrite_existing;
  fs::copy(src_path, dst_path, options, error);
  if (error)
  {
    std::error_code error_ignored;
    if (fs::equivalent(src_path, dst_path, error_ignored))
      return true;

    ERROR_LOG_FMT(COMMON, "{}: failed {} --> {} ({}): {}", __func__, source_path, dest_path,
                  overwrite_existing ? "overwrite" : "preserve", error.message());
    return false;
  }
  return true;
}

static bool MoveWithOverwrite(const std::filesystem::path& src, const std::filesystem::path& dst,
                              std::error_code& error)
{
  fs::rename(src, dst, error);
  if (!error)
    return true;

  // rename failed, try fallbacks

  if (!fs::is_directory(src))
  {
    // src is not a directory (ie, probably a file), try to copy file + delete
    if (!fs::copy_file(src, dst, fs::copy_options::overwrite_existing, error))
      return false;
    if (!fs::remove(src, error))
      return false;
    return true;
  }

  // src is a directory, recurse into it and try to move all sub-elements one by one
  // this usually happens because the target is a non-empty directory
  for (fs::directory_iterator it(src, error); it != fs::directory_iterator(); it.increment(error))
  {
    if (error)
      return false;
    if (!MoveWithOverwrite(it->path(), dst / it->path().filename(), error))
      return false;
  }
  if (error)
    return false;

  // all sub-elements moved, remove top directory
  if (!fs::remove(src, error))
    return false;

  return true;
}

bool MoveWithOverwrite(std::string_view source_path, std::string_view dest_path)
{
  DEBUG_LOG_FMT(COMMON, "{}: {} --> {}", __func__, source_path, dest_path);
  auto src_path = StringToPath(source_path);
  auto dst_path = StringToPath(dest_path);
  std::error_code error;
  if (!MoveWithOverwrite(src_path, dst_path, error))
  {
    ERROR_LOG_FMT(COMMON, "{}: failed {} --> {}: {}", __func__, source_path, dest_path,
                  error.message());
  }
  return true;
}

// Returns the current directory
std::string GetCurrentDir()
{
  std::error_code error;
  auto directory = PathToString(fs::current_path(error));
  if (error)
  {
    ERROR_LOG_FMT(COMMON, "{} failed: {}", __func__, error.message());
    return {};
  }
  return directory;
}

// Sets the current directory to the given directory
bool SetCurrentDir(const std::string& directory)
{
  std::error_code error;
  fs::current_path(StringToPath(directory), error);
  if (error)
  {
    ERROR_LOG_FMT(COMMON, "{} failed: {}", __func__, error.message());
    return false;
  }
  return true;
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
  std::error_code error;
  auto absolute_path = fs::absolute(StringToPath(path), error);
  if (!error)
    path = PathToString(absolute_path);
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
#ifdef _WIN32
  auto exe_path = Common::GetModuleName(nullptr);
  if (!exe_path)
    return {};
  std::error_code error;
  auto exe_path_absolute = fs::absolute(exe_path.value(), error);
  if (error)
    return {};
  return PathToString(exe_path_absolute);
#elif defined(__APPLE__)
  return GetBundleDirectory();
#elif defined(__FreeBSD__)
  int name[4]{CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
  size_t length = 0;
  if (sysctl(name, 4, nullptr, &length, nullptr, 0) != 0 || length == 0)
    return {};
  std::string dolphin_exe_path(length, '\0');
  if (sysctl(name, 4, dolphin_exe_path.data(), &length, nullptr, 0) != 0)
    return {};
  return dolphin_exe_path;
#else
  char dolphin_exe_path[PATH_MAX];
  ssize_t len = ::readlink("/proc/self/exe", dolphin_exe_path, sizeof(dolphin_exe_path));
  if (len == -1 || len == sizeof(dolphin_exe_path))
  {
    len = 0;
  }
  dolphin_exe_path[len] = '\0';
  return dolphin_exe_path;
#endif
}

std::string GetExeDirectory()
{
  return PathToString(StringToPath(GetExePath()).parent_path());
}

static std::string CreateSysDirectoryPath()
{
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
  const std::string sys_directory = GetBundleDirectory() + DIR_SEP SYSDATA_DIR DIR_SEP;
#elif defined(_WIN32) || defined(LINUX_LOCAL_DEV)
  const std::string sys_directory = GetExeDirectory() + DIR_SEP SYSDATA_DIR DIR_SEP;
#elif defined ANDROID
  const std::string sys_directory = s_android_sys_directory + DIR_SEP;
  ASSERT_MSG(COMMON, !s_android_sys_directory.empty(), "Sys directory has not been set");
#else
  const std::string sys_directory = SYSDATA_DIR DIR_SEP;
#endif

  INFO_LOG_FMT(COMMON, "CreateSysDirectoryPath: Setting to {}", sys_directory);
  return sys_directory;
}

const std::string& GetSysDirectory()
{
  static const std::string sys_directory = CreateSysDirectoryPath();
  return sys_directory;
}

#ifdef ANDROID
void SetSysDirectory(const std::string& path)
{
  INFO_LOG_FMT(COMMON, "Setting Sys directory to {}", path);
  ASSERT_MSG(COMMON, s_android_sys_directory.empty(), "Sys directory already set to {}",
             s_android_sys_directory);
  s_android_sys_directory = path;
}

void SetGpuDriverDirectories(const std::string& path, const std::string& lib_path)
{
  INFO_LOG_FMT(COMMON, "Setting Driver directory to {} and library path to {}", path, lib_path);
  ASSERT_MSG(COMMON, s_android_driver_directory.empty(), "Driver directory already set to {}",
             s_android_driver_directory);
  ASSERT_MSG(COMMON, s_android_lib_directory.empty(), "Library directory already set to {}",
             s_android_lib_directory);
  s_android_driver_directory = path;
  s_android_lib_directory = lib_path;
}

const std::string GetGpuDriverDirectory(unsigned int dir_index)
{
  switch (dir_index)
  {
  case D_GPU_DRIVERS_EXTRACTED:
    return s_android_driver_directory + DIR_SEP GPU_DRIVERS_EXTRACTED DIR_SEP;
  case D_GPU_DRIVERS_TMP:
    return s_android_driver_directory + DIR_SEP GPU_DRIVERS_TMP DIR_SEP;
  case D_GPU_DRIVERS_HOOKS:
    return s_android_lib_directory;
  case D_GPU_DRIVERS_FILE_REDIRECT:
    return s_android_driver_directory + DIR_SEP GPU_DRIVERS_FILE_REDIRECT DIR_SEP;
  }
  return "";
}

#endif

static std::string s_user_paths[NUM_PATH_INDICES];
static void RebuildUserDirectories(unsigned int dir_index)
{
  switch (dir_index)
  {
  case D_USER_IDX:
    s_user_paths[D_GCUSER_IDX] = s_user_paths[D_USER_IDX] + GC_USER_DIR DIR_SEP;
    s_user_paths[D_WIIROOT_IDX] = s_user_paths[D_USER_IDX] + WII_USER_DIR DIR_SEP;
    s_user_paths[D_CONFIG_IDX] = s_user_paths[D_USER_IDX] + CONFIG_DIR DIR_SEP;
    s_user_paths[D_GAMESETTINGS_IDX] = s_user_paths[D_USER_IDX] + GAMESETTINGS_DIR DIR_SEP;
    s_user_paths[D_MAPS_IDX] = s_user_paths[D_USER_IDX] + MAPS_DIR DIR_SEP;
    s_user_paths[D_CACHE_IDX] = s_user_paths[D_USER_IDX] + CACHE_DIR DIR_SEP;
    s_user_paths[D_COVERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + COVERCACHE_DIR DIR_SEP;
    s_user_paths[D_REDUMPCACHE_IDX] = s_user_paths[D_CACHE_IDX] + REDUMPCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + SHADERCACHE_DIR DIR_SEP;
    s_user_paths[D_RETROACHIEVEMENTSCACHE_IDX] =
        s_user_paths[D_CACHE_IDX] + RETROACHIEVEMENTSCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERS_IDX] = s_user_paths[D_USER_IDX] + SHADERS_DIR DIR_SEP;
    s_user_paths[D_STATESAVES_IDX] = s_user_paths[D_USER_IDX] + STATESAVES_DIR DIR_SEP;
    s_user_paths[D_SCREENSHOTS_IDX] = s_user_paths[D_USER_IDX] + SCREENSHOTS_DIR DIR_SEP;
    s_user_paths[D_LOAD_IDX] = s_user_paths[D_USER_IDX] + LOAD_DIR DIR_SEP;
    s_user_paths[D_HIRESTEXTURES_IDX] = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_RIIVOLUTION_IDX] = s_user_paths[D_LOAD_IDX] + RIIVOLUTION_DIR DIR_SEP;
    s_user_paths[D_DUMP_IDX] = s_user_paths[D_USER_IDX] + DUMP_DIR DIR_SEP;
    s_user_paths[D_DUMPFRAMES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
    s_user_paths[D_DUMPOBJECTS_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_OBJECTS_DIR DIR_SEP;
    s_user_paths[D_DUMPAUDIO_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
    s_user_paths[D_DUMPTEXTURES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMPMESHES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_MESHES_DIR DIR_SEP;
    s_user_paths[D_DUMPDSP_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
    s_user_paths[D_DUMPSSL_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_SSL_DIR DIR_SEP;
    s_user_paths[D_DUMPDEBUG_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DEBUG_DIR DIR_SEP;
    s_user_paths[D_DUMPDEBUG_BRANCHWATCH_IDX] =
        s_user_paths[D_DUMPDEBUG_IDX] + DUMP_DEBUG_BRANCHWATCH_DIR DIR_SEP;
    s_user_paths[D_DUMPDEBUG_JITBLOCKS_IDX] =
        s_user_paths[D_DUMPDEBUG_IDX] + DUMP_DEBUG_JITBLOCKS_DIR DIR_SEP;
    s_user_paths[D_LOGS_IDX] = s_user_paths[D_USER_IDX] + LOGS_DIR DIR_SEP;
    s_user_paths[D_MAILLOGS_IDX] = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
    s_user_paths[D_THEMES_IDX] = s_user_paths[D_USER_IDX] + THEMES_DIR DIR_SEP;
    s_user_paths[D_STYLES_IDX] = s_user_paths[D_USER_IDX] + STYLES_DIR DIR_SEP;
    s_user_paths[D_PIPES_IDX] = s_user_paths[D_USER_IDX] + PIPES_DIR DIR_SEP;
    s_user_paths[D_WFSROOT_IDX] = s_user_paths[D_USER_IDX] + WFSROOT_DIR DIR_SEP;
    s_user_paths[D_BACKUP_IDX] = s_user_paths[D_USER_IDX] + BACKUP_DIR DIR_SEP;
    s_user_paths[D_RESOURCEPACK_IDX] = s_user_paths[D_USER_IDX] + RESOURCEPACK_DIR DIR_SEP;
    s_user_paths[D_DYNAMICINPUT_IDX] = s_user_paths[D_LOAD_IDX] + DYNAMICINPUT_DIR DIR_SEP;
    s_user_paths[D_GRAPHICSMOD_IDX] = s_user_paths[D_LOAD_IDX] + GRAPHICSMOD_DIR DIR_SEP;
    s_user_paths[D_WIISDCARDSYNCFOLDER_IDX] = s_user_paths[D_LOAD_IDX] + WIISDSYNC_DIR DIR_SEP;
    s_user_paths[F_DOLPHINCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
    s_user_paths[F_GCPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCPAD_CONFIG;
    s_user_paths[F_WIIPADCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + WIIPAD_CONFIG;
    s_user_paths[F_GCKEYBOARDCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GCKEYBOARD_CONFIG;
    s_user_paths[F_GFXCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + GFX_CONFIG;
    s_user_paths[F_LOGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
    s_user_paths[F_DUALSHOCKUDPCLIENTCONFIG_IDX] =
        s_user_paths[D_CONFIG_IDX] + DUALSHOCKUDPCLIENT_CONFIG;
    s_user_paths[F_FREELOOKCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + FREELOOK_CONFIG;
    s_user_paths[F_RETROACHIEVEMENTSCONFIG_IDX] =
        s_user_paths[D_CONFIG_IDX] + RETROACHIEVEMENTS_CONFIG;
    s_user_paths[F_MAINLOG_IDX] = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
    s_user_paths[F_MEM1DUMP_IDX] = s_user_paths[D_DUMP_IDX] + MEM1_DUMP;
    s_user_paths[F_MEM2DUMP_IDX] = s_user_paths[D_DUMP_IDX] + MEM2_DUMP;
    s_user_paths[F_ARAMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
    s_user_paths[F_FAKEVMEMDUMP_IDX] = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
    s_user_paths[F_GCSRAM_IDX] = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
    s_user_paths[F_WIISDCARDIMAGE_IDX] = s_user_paths[D_LOAD_IDX] + WII_SD_CARD_IMAGE;

    s_user_paths[D_MEMORYWATCHER_IDX] = s_user_paths[D_USER_IDX] + MEMORYWATCHER_DIR DIR_SEP;
    s_user_paths[F_MEMORYWATCHERLOCATIONS_IDX] =
        s_user_paths[D_MEMORYWATCHER_IDX] + MEMORYWATCHER_LOCATIONS;
    s_user_paths[F_MEMORYWATCHERSOCKET_IDX] =
        s_user_paths[D_MEMORYWATCHER_IDX] + MEMORYWATCHER_SOCKET;

    s_user_paths[D_GBAUSER_IDX] = s_user_paths[D_USER_IDX] + GBA_USER_DIR DIR_SEP;
    s_user_paths[D_GBASAVES_IDX] = s_user_paths[D_GBAUSER_IDX] + GBASAVES_DIR DIR_SEP;
    s_user_paths[F_GBABIOS_IDX] = s_user_paths[D_GBAUSER_IDX] + GBA_BIOS;

    s_user_paths[D_ASM_ROOT_IDX] = s_user_paths[D_USER_IDX] + ASSEMBLY_DIR DIR_SEP;

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
    s_user_paths[F_LOGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
    s_user_paths[F_DUALSHOCKUDPCLIENTCONFIG_IDX] =
        s_user_paths[D_CONFIG_IDX] + DUALSHOCKUDPCLIENT_CONFIG;
    s_user_paths[F_FREELOOKCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + FREELOOK_CONFIG;
    s_user_paths[F_RETROACHIEVEMENTSCONFIG_IDX] =
        s_user_paths[D_CONFIG_IDX] + RETROACHIEVEMENTS_CONFIG;
    break;

  case D_CACHE_IDX:
    s_user_paths[D_COVERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + COVERCACHE_DIR DIR_SEP;
    s_user_paths[D_REDUMPCACHE_IDX] = s_user_paths[D_CACHE_IDX] + REDUMPCACHE_DIR DIR_SEP;
    s_user_paths[D_SHADERCACHE_IDX] = s_user_paths[D_CACHE_IDX] + SHADERCACHE_DIR DIR_SEP;
    s_user_paths[D_RETROACHIEVEMENTSCACHE_IDX] =
        s_user_paths[D_CACHE_IDX] + RETROACHIEVEMENTSCACHE_DIR DIR_SEP;
    break;

  case D_GCUSER_IDX:
    s_user_paths[F_GCSRAM_IDX] = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
    break;

  case D_DUMP_IDX:
    s_user_paths[D_DUMPFRAMES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
    s_user_paths[D_DUMPOBJECTS_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_OBJECTS_DIR DIR_SEP;
    s_user_paths[D_DUMPAUDIO_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
    s_user_paths[D_DUMPTEXTURES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
    s_user_paths[D_DUMPMESHES_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_MESHES_DIR DIR_SEP;
    s_user_paths[D_DUMPDSP_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
    s_user_paths[D_DUMPSSL_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_SSL_DIR DIR_SEP;
    s_user_paths[D_DUMPDEBUG_IDX] = s_user_paths[D_DUMP_IDX] + DUMP_DEBUG_DIR DIR_SEP;
    s_user_paths[D_DUMPDEBUG_BRANCHWATCH_IDX] =
        s_user_paths[D_DUMPDEBUG_IDX] + DUMP_DEBUG_BRANCHWATCH_DIR DIR_SEP;
    s_user_paths[D_DUMPDEBUG_JITBLOCKS_IDX] =
        s_user_paths[D_DUMPDEBUG_IDX] + DUMP_DEBUG_JITBLOCKS_DIR DIR_SEP;
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
    s_user_paths[D_RIIVOLUTION_IDX] = s_user_paths[D_LOAD_IDX] + RIIVOLUTION_DIR DIR_SEP;
    s_user_paths[D_DYNAMICINPUT_IDX] = s_user_paths[D_LOAD_IDX] + DYNAMICINPUT_DIR DIR_SEP;
    s_user_paths[D_GRAPHICSMOD_IDX] = s_user_paths[D_LOAD_IDX] + GRAPHICSMOD_DIR DIR_SEP;
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
void SetUserPath(unsigned int dir_index, std::string path)
{
  if (path.empty())
    return;

#ifdef _WIN32
  // On Windows, replace all '\' with '/' since we assume the latter in various places in the
  // codebase.
  for (char& c : path)
  {
    if (c == '\\')
      c = '/';
  }
#endif

  // Directories should end with a separator, files should not.
  while (path.ends_with('/'))
    path.pop_back();
  if (path.empty())
    return;
  const bool is_directory = dir_index < FIRST_FILE_USER_PATH_IDX;
  if (is_directory)
    path.push_back('/');

  s_user_paths[dir_index] = std::move(path);
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
