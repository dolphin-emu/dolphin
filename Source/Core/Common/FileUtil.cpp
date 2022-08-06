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

namespace fs = std::filesystem;

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
  {
    m_path = StringToPath(path);
    std::error_code error;
    m_status = fs::status(m_path, error);
    m_exists = fs::exists(m_status);
  }
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
  std::error_code error;
  std::uintmax_t size = fs::file_size(m_path, error);
  if (error)
    return 0;
  return size;
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
  if (StringBeginsWith(filename, "content://"))
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

// Returns true if successful, or path already exists.
bool CreateDir(const std::string& path)
{
  DEBUG_LOG_FMT(COMMON, "{}: directory {}", __func__, path);

  std::error_code error;
  auto native_path = StringToPath(path);
  bool success = fs::create_directory(native_path, error);
  // If the path was not created, check if it was a pre-existing directory
  if (!success && fs::is_directory(native_path))
    success = true;
  if (!success)
    ERROR_LOG_FMT(COMMON, "{}: failed on {}: {}", __func__, path, error.message());
  return success;
}

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string& fullPath)
{
  DEBUG_LOG_FMT(COMMON, "{}: path {}", __func__, fullPath);

  std::error_code error;
  auto native_path = StringToPath(fullPath);
  bool success = fs::create_directories(native_path, error);
  // If the path was not created, check if it was a pre-existing directory
  if (!success && fs::is_directory(native_path))
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

// copies file source_path to destination_path, returns true on success
bool Copy(const std::string& source_path, const std::string& destination_path)
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
  DEBUG_LOG_FMT(COMMON, "CreateEmptyFile: {}", filename);

  if (!File::IOFile(filename, "wb"))
  {
    ERROR_LOG_FMT(COMMON, "CreateEmptyFile: failed {}: {}", filename, LastStrerrorString());
    return false;
  }

  return true;
}

// Recursive or non-recursive list of files and directories under directory.
FSTEntry ScanDirectoryTree(std::string directory, bool recursive)
{
#ifdef _WIN32
  if (!directory.empty() && (directory.back() == '/' || directory.back() == '\\'))
    directory.pop_back();
#else
  if (!directory.empty() && directory.back() == '/')
    directory.pop_back();
#endif

  DEBUG_LOG_FMT(COMMON, "ScanDirectoryTree: directory {}", directory);
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
  DEBUG_LOG_FMT(COMMON, "{}: {}", __func__, directory);

  std::error_code error;
  const std::uintmax_t num_removed = std::filesystem::remove_all(StringToPath(directory), error);
  const bool success = num_removed != 0 && !error;
  if (!success)
    ERROR_LOG_FMT(COMMON, "{}: {} failed {}", __func__, directory, error.message());
  return success;
}

// Create directory and copy contents (optionally overwrites existing files)
bool CopyDir(const std::string& source_path, const std::string& dest_path, const bool destructive)
{
  auto src_path = StringToPath(source_path);
  auto dst_path = StringToPath(dest_path);
  if (fs::equivalent(src_path, dst_path))
    return true;

  DEBUG_LOG_FMT(COMMON, "{}: {} --> {}", __func__, source_path, dest_path);

  auto options = fs::copy_options::recursive;
  if (destructive)
    options |= fs::copy_options::overwrite_existing;
  std::error_code error;
  bool copied = fs::copy_file(src_path, dst_path, options, error);
  if (!copied)
  {
    ERROR_LOG_FMT(COMMON, "{}: failed {} --> {}: {}", __func__, source_path, dest_path,
                  error.message());
  }
  return copied;
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
  auto exe_path = GetModuleName(nullptr);
  if (!exe_path)
    return {};
  std::error_code error;
  auto exe_path_absolute = fs::absolute(exe_path.value(), error);
  if (error)
    return {};
  return PathToString(exe_path_absolute);
#elif defined(__APPLE__)
  return GetBundleDirectory();
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
    s_user_paths[D_GRAPHICSMOD_IDX] = s_user_paths[D_LOAD_IDX] + GRAPHICSMOD_DIR DIR_SEP;
    s_user_paths[D_WIISDCARDSYNCFOLDER_IDX] = s_user_paths[D_LOAD_IDX] + WIISDSYNC_DIR DIR_SEP;
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
    s_user_paths[F_WIISDCARDIMAGE_IDX] = s_user_paths[D_LOAD_IDX] + WII_SD_CARD_IMAGE;

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
  while (StringEndsWith(path, "/"))
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
