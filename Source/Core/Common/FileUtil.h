// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>

#include "Common/CommonTypes.h"

#ifdef _WIN32
#include "Common/StringUtil.h"
#endif

#ifdef ANDROID
#include "Common/StringUtil.h"
#include "jni/AndroidCommon/AndroidCommon.h"
#endif

// User directory indices for GetUserPath
enum
{
  D_USER_IDX,
  D_GCUSER_IDX,
  D_WIIROOT_IDX,          // always points to User/Wii or global user-configured directory
  D_SESSION_WIIROOT_IDX,  // may point to minimal temporary directory for determinism
  D_CONFIG_IDX,           // global settings
  D_GAMESETTINGS_IDX,     // user-specified settings which override both the global and the default
                          // settings (per game)
  D_SKYLANDERS_IDX,

  D_MAPS_IDX,
  D_CACHE_IDX,
  D_COVERCACHE_IDX,
  D_REDUMPCACHE_IDX,
  D_SHADERCACHE_IDX,
  D_RETROACHIEVEMENTSCACHE_IDX,
  D_SHADERS_IDX,
  D_STATESAVES_IDX,
  D_SCREENSHOTS_IDX,
  D_HIRESTEXTURES_IDX,
  D_RIIVOLUTION_IDX,
  D_DUMP_IDX,
  D_DUMPFRAMES_IDX,
  D_DUMPOBJECTS_IDX,
  D_DUMPAUDIO_IDX,
  D_DUMPTEXTURES_IDX,
  D_DUMPMESHES_IDX,
  D_DUMPDSP_IDX,
  D_DUMPSSL_IDX,
  D_DUMPDEBUG_IDX,
  D_DUMPDEBUG_BRANCHWATCH_IDX,
  D_DUMPDEBUG_JITBLOCKS_IDX,
  D_LOAD_IDX,
  D_LOGS_IDX,
  D_MAILLOGS_IDX,
  D_THEMES_IDX,
  D_STYLES_IDX,
  D_PIPES_IDX,
  D_MEMORYWATCHER_IDX,
  D_WFSROOT_IDX,
  D_BACKUP_IDX,
  D_RESOURCEPACK_IDX,
  D_DYNAMICINPUT_IDX,
  D_GRAPHICSMOD_IDX,
  D_GBAUSER_IDX,
  D_GBASAVES_IDX,
  D_WIISDCARDSYNCFOLDER_IDX,
  D_GPU_DRIVERS_EXTRACTED,
  D_GPU_DRIVERS_TMP,
  D_GPU_DRIVERS_HOOKS,
  D_GPU_DRIVERS_FILE_REDIRECT,
  D_ASM_ROOT_IDX,
  FIRST_FILE_USER_PATH_IDX,
  F_DOLPHINCONFIG_IDX = FIRST_FILE_USER_PATH_IDX,
  F_GCPADCONFIG_IDX,
  F_WIIPADCONFIG_IDX,
  F_GCKEYBOARDCONFIG_IDX,
  F_GFXCONFIG_IDX,
  F_LOGGERCONFIG_IDX,
  F_MAINLOG_IDX,
  F_MEM1DUMP_IDX,
  F_MEM2DUMP_IDX,
  F_ARAMDUMP_IDX,
  F_FAKEVMEMDUMP_IDX,
  F_GCSRAM_IDX,
  F_MEMORYWATCHERLOCATIONS_IDX,
  F_MEMORYWATCHERSOCKET_IDX,
  F_WIISDCARDIMAGE_IDX,
  F_DUALSHOCKUDPCLIENTCONFIG_IDX,
  F_FREELOOKCONFIG_IDX,
  F_GBABIOS_IDX,
  F_RETROACHIEVEMENTSCONFIG_IDX,
  NUM_PATH_INDICES
};

namespace File
{
// FileSystem tree node
struct FSTEntry
{
  bool isDirectory = false;
  u64 size = 0;              // File length, or for directories, recursive count of children
  std::string physicalName;  // Name on disk
  std::string virtualName;   // Name in FST names table
  std::vector<FSTEntry> children;
};

// The functions in this class are functionally identical to the standalone functions
// below, but if you are going to be calling more than one of the functions using the
// same path, creating a single FileInfo object and calling its functions multiple
// times is faster than calling standalone functions multiple times.
class FileInfo final
{
public:
  explicit FileInfo(const std::string& path);
  explicit FileInfo(const char* path);

  // Returns true if the path exists
  bool Exists() const;
  // Returns true if the path exists and is a directory
  bool IsDirectory() const;
  // Returns true if the path exists and is a file
  bool IsFile() const;
  // Returns the size of a file (or returns 0 if the path doesn't refer to a file)
  u64 GetSize() const;

private:
  std::filesystem::file_status m_status;
  std::uintmax_t m_size;
  bool m_exists;
};

// Returns true if the path exists
bool Exists(const std::string& path);

// Returns true if the path exists and is a directory
bool IsDirectory(const std::string& path);

// Returns true if the path exists and is a file
bool IsFile(const std::string& path);

// Returns the size of a file (or returns 0 if the path isn't a file that exists)
u64 GetSize(const std::string& path);

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE* f);

// Creates a single directory. Returns true if successful or if the path already exists.
bool CreateDir(const std::string& filename);

// Creates directories recursively. Returns true if successful or if the path already exists.
bool CreateDirs(std::string_view filename);

// Creates the full path to the file given in fullPath.
// That is, for path '/a/b/c.bin', creates folders '/a' and '/a/b'.
// Returns true if creation is successful or if the path already exists.
bool CreateFullPath(std::string_view fullPath);

enum class IfAbsentBehavior
{
  ConsoleWarning,
  NoConsoleWarning
};

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string& filename,
            IfAbsentBehavior behavior = IfAbsentBehavior::ConsoleWarning);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string& filename,
               IfAbsentBehavior behavior = IfAbsentBehavior::ConsoleWarning);

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string& srcFilename, const std::string& destFilename);

// ditto, but syncs the source file and, on Unix, syncs the directories after rename
bool RenameSync(const std::string& srcFilename, const std::string& destFilename);

// Copies a file at source_path to destination_path, as if by std::filesystem::copy_file().
// If a file already exists at destination_path it is overwritten. Returns true on success.
bool CopyRegularFile(std::string_view source_path, std::string_view destination_path);

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string& filename);

// Recursive or non-recursive list of files and directories under directory.
FSTEntry ScanDirectoryTree(std::string directory, bool recursive);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string& directory);

// Returns the current directory
std::string GetCurrentDir();

// Copies source_path to dest_path, as if by std::filesystem::copy(). Returns true on success or if
// the source and destination are already the same (as determined by std::filesystem::equivalent()).
bool Copy(std::string_view source_path, std::string_view dest_path,
          bool overwrite_existing = false);

// Moves source_path to dest_path. On success, the source_path will no longer exist, and the
// dest_path will contain the data previously in source_path. Files in dest_path will be overwritten
// if they match files in source_path, but files that only exist in dest_path will be kept. No
// guarantee on the state is given on failure; the move may have completely failed or partially
// completed.
bool MoveWithOverwrite(std::string_view source_path, std::string_view dest_path);

// Set the current directory to given directory
bool SetCurrentDir(const std::string& directory);

// Creates and returns the path to a new temporary directory.
std::string CreateTempDir();

// Get a filename that can hopefully be atomically renamed to the given path.
std::string GetTempFilenameForAtomicWrite(std::string path);

// Gets a set user directory path
// Don't call prior to setting the base user directory
const std::string& GetUserPath(unsigned int dir_index);

// Sets a user directory path
// Rebuilds internal directory structure to compensate for the new directory
void SetUserPath(unsigned int dir_index, std::string path);

// probably doesn't belong here
std::string GetThemeDir(const std::string& theme_name);

// Returns the path to where the sys file are
const std::string& GetSysDirectory();

#ifdef ANDROID
void SetSysDirectory(const std::string& path);
void SetGpuDriverDirectories(const std::string& path, const std::string& lib_path);
const std::string GetGpuDriverDirectory(unsigned int dir_index);
#endif

#ifdef __APPLE__
std::string GetBundleDirectory();
#endif

std::string GetExePath();
std::string GetExeDirectory();

bool WriteStringToFile(const std::string& filename, std::string_view str);
bool ReadFileToString(const std::string& filename, std::string& str);

// To deal with Windows not fully supporting UTF-8 and Android not fully supporting paths.
template <typename T>
void OpenFStream(T& fstream, const std::string& filename, std::ios_base::openmode openmode)
{
#ifdef _WIN32
  fstream.open(UTF8ToTStr(filename).c_str(), openmode);
#else
#ifdef ANDROID
  // Unfortunately it seems like the non-standard __open is the only way to use a file descriptor
  if (IsPathAndroidContent(filename))
    fstream.__open(OpenAndroidContent(filename, OpenModeToAndroid(openmode)), openmode);
  else
#endif
    fstream.open(filename.c_str(), openmode);
#endif
}

}  // namespace File
