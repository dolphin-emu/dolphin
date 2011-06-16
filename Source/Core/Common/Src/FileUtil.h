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

#include <fstream>
#include <cstdio>
#include <string>
#include <vector>
#include <string.h>

#include "Common.h"

// User directory indices for GetUserPath
enum {
	D_USER_IDX,
	D_GCUSER_IDX,
	D_WIIROOT_IDX,
	D_WIIUSER_IDX,
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
	NUM_PATH_INDICES
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
bool Exists(const std::string &filename);

// Returns true if filename is a directory
bool IsDirectory(const std::string &filename);

// Returns the size of filename (64bit)
u64 GetSize(const std::string &filename);

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd);

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE *f);

// Returns true if successful, or path already exists.
bool CreateDir(const std::string &filename);

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string &fullPath);

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string &filename);

// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string &filename);

// renames file srcFilename to destFilename, returns true on success 
bool Rename(const std::string &srcFilename, const std::string &destFilename);

// copies file srcFilename to destFilename, returns true on success 
bool Copy(const std::string &srcFilename, const std::string &destFilename);

// creates an empty file filename, returns true on success 
bool CreateEmptyFile(const std::string &filename);

// Scans the directory tree gets, starting from _Directory and adds the
// results into parentEntry. Returns the number of files+directories found
u32 ScanDirectoryTree(const std::string &directory, FSTEntry& parentEntry);

// deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string &directory);

// Returns the current directory
std::string GetCurrentDir();

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string &source_path, const std::string &dest_path);

// Set the current directory to given directory
bool SetCurrentDir(const std::string &directory);

// Returns a pointer to a string with a Dolphin data dir in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
std::string &GetUserPath(const unsigned int DirIDX, const std::string &newPath="");

// Returns the path to where the sys file are
std::string GetSysDirectory();

#ifdef __APPLE__
std::string GetBundleDirectory();
#endif

#ifdef _WIN32
std::string &GetExeDirectory();
#endif

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename);
bool ReadFileToString(bool text_file, const char *filename, std::string &str);

// simple wrapper for cstdlib file functions to
// hopefully will make error checking easier
// and make forgetting an fclose() harder
class IOFile : NonCopyable
{
public:
	IOFile();
	IOFile(std::FILE* file);
	IOFile(const std::string& filename, const char openmode[]);

	~IOFile();

	bool Open(const std::string& filename, const char openmode[]);
	bool Close();

	template <typename T>
	bool ReadArray(T* data, size_t length)
	{
		if (!IsOpen() || length != std::fread(data, sizeof(T), length, m_file))
			m_good = false;

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

	bool IsOpen() { return NULL != m_file; }

	// m_good is set to false when a read, write or other function fails
	bool IsGood() {	return m_good; }
	operator void*() { return m_good ? m_file : NULL; }

	std::FILE* ReleaseHandle();

	std::FILE* GetHandle() { return m_file; }

	void SetHandle(std::FILE* file);

	bool Seek(s64 off, int origin);
	u64 Tell();
	u64 GetSize();
	bool Resize(u64 size);
	bool Flush();

	// clear error state
	void Clear() { m_good = true; std::clearerr(m_file); }

private:
	IOFile& operator=(const IOFile&) /*= delete*/;

	std::FILE* m_file;
	bool m_good;
};

}  // namespace

#endif
