// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _FILEUTIL_H_
#define _FILEUTIL_H_

#include <string>
#include <vector>
#include <string.h>

#include "Common.h"

// User directory indices for GetUserPath
enum {
	D_USER_IDX,
	D_GCUSER_IDX,
	D_WIIUSER_IDX,
	D_WIIROOT_IDX,
	D_CONFIG_IDX,
	D_GAMECONFIG_IDX,
	D_MAPS_IDX,
	D_CACHE_IDX,
	D_SHADERCACHE_IDX,
	D_SHADERS_IDX,
	D_STATESAVES_IDX,
	D_SCREENSHOTS_IDX,
	D_OPENCL_IDX,
	D_HIRESTEXTURES_IDX,
	D_DUMP_IDX,
	D_DUMPFRAMES_IDX,
	D_DUMPAUDIO_IDX,
	D_DUMPTEXTURES_IDX,
	D_DUMPDSP_IDX,
	D_LOGS_IDX,
	D_MAILLOGS_IDX,
	D_WIISYSCONF_IDX,
	F_DOLPHINCONFIG_IDX,
	F_DSPCONFIG_IDX,
	F_DEBUGGERCONFIG_IDX,
	F_LOGGERCONFIG_IDX,
	F_MAINLOG_IDX,
	F_WIISYSCONF_IDX,
	F_RAMDUMP_IDX,
	F_ARAMDUMP_IDX,
	F_GCSRAM_IDX,
};

namespace File
{

// FileSystem tree node/ 
struct FSTEntry
{
	bool isDirectory;
	u64 size;						// file length or number of entries from children
	std::string physicalName;		// name on disk
	std::string virtualName;		// name in FST names table
	std::vector<FSTEntry> children;
};

// Returns true if file filename exists
bool Exists(const char *filename);

// Returns true if filename is a directory
bool IsDirectory(const char *filename);

// Returns the size of filename (64bit)
u64 GetSize(const char *filename);

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd);

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE *f);

// Returns true if successful, or path already exists.
bool CreateDir(const char *filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const char *fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const char *filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const char *filename);

// renames file srcFilename to destFilename, returns true on success 
bool Rename(const char *srcFilename, const char *destFilename);

// copies file srcFilename to destFilename, returns true on success 
bool Copy(const char *srcFilename, const char *destFilename);

// creates an empty file filename, returns true on success 
bool CreateEmptyFile(const char *filename);

// Scans the directory tree gets, starting from _Directory and adds the
// results into parentEntry. Returns the number of files+directories found
u32 ScanDirectoryTree(const char *directory, FSTEntry& parentEntry);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const char *directory);

// Returns the current directory
std::string GetCurrentDir();

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const char *source_path, const char *dest_path);

// Set the current directory to given directory
bool SetCurrentDir(const char *directory);

// Returns a pointer to a string with a Dolphin data dir in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
const char *GetUserPath(int DirIDX);

// Returns the path to where the plugins are
std::string GetPluginsDirectory();

// Returns the path to where the sys file are
std::string GetSysDirectory();

#ifdef __APPLE__
std::string GetBundleDirectory();
#endif

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename);
bool ReadFileToString(bool text_file, const char *filename, std::string &str);

}  // namespace

#endif
