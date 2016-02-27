// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/NonCopyable.h"
#include "Common/StringUtil.h"

// User directory indices for GetUserPath
enum {
	D_USER_IDX,
	D_GCUSER_IDX,
	D_WIIROOT_IDX, // always points to User/Wii or global user-configured directory
	D_SESSION_WIIROOT_IDX, // may point to minimal temporary directory for determinism
	D_CONFIG_IDX, // global settings
	D_GAMESETTINGS_IDX, // user-specified settings which override both the global and the default settings (per game)
	D_MAPS_IDX,
	D_CACHE_IDX,
	D_SHADERCACHE_IDX,
	D_SHADERS_IDX,
	D_STATESAVES_IDX,
	D_SCREENSHOTS_IDX,
	D_HIRESTEXTURES_IDX,
	D_DUMP_IDX,
	D_DUMPFRAMES_IDX,
	D_DUMPAUDIO_IDX,
	D_DUMPTEXTURES_IDX,
	D_DUMPDSP_IDX,
	D_LOAD_IDX,
	D_LOGS_IDX,
	D_MAILLOGS_IDX,
	D_THEMES_IDX,
	D_PIPES_IDX,
	D_MEMORYWATCHER_IDX,
	F_DOLPHINCONFIG_IDX,
	F_DEBUGGERCONFIG_IDX,
	F_LOGGERCONFIG_IDX,
	F_MAINLOG_IDX,
	F_RAMDUMP_IDX,
	F_ARAMDUMP_IDX,
	F_FAKEVMEMDUMP_IDX,
	F_GCSRAM_IDX,
	F_MEMORYWATCHERLOCATIONS_IDX,
	F_MEMORYWATCHERSOCKET_IDX,
	NUM_PATH_INDICES
};

namespace File
{

// FileSystem tree node/
struct FSTEntry
{
	bool isDirectory;
	u64 size;                 // File length, or for directories, recursive count of children
	std::string physicalName; // Name on disk
	std::string virtualName;  // Name in FST names table
	std::vector<FSTEntry> children;
};

// Returns true if file filename exists
bool Exists(const std::string& filename);

// Returns true if filename is a directory
bool IsDirectory(const std::string& filename);

// Returns the size of filename (64bit)
u64 GetSize(const std::string& filename);

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

// Recursive or non-recursive list of files under directory.
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

// Gets a set user directory path
// Don't call prior to setting the base user directory
const std::string& GetUserPath(unsigned int dir_index);

// Sets a user directory path
// Rebuilds internal directory structure to compensate for the new directory
void SetUserPath(unsigned int dir_index, const std::string& path);

// probably doesn't belong here
std::string GetThemeDir(const std::string& theme_name);

// Returns the path to where the sys file are
std::string GetSysDirectory();

#ifdef __APPLE__
std::string GetBundleDirectory();
#endif

std::string& GetExeDirectory();

bool WriteStringToFile(const std::string& str, const std::string& filename);
bool ReadFileToString(const std::string& filename, std::string& str);

// simple wrapper for cstdlib (UNIX) / WinAPI (Windows) file functions to
// hopefully make error checking easier and make forgetting an fclose() harder
class IOFile : public NonCopyable
{
public:
	// Flags that can be provided to the Open method or to the IOFile constructor
	// to get alternative behaviors.
	enum OpenFlags : u32
	{
		OPENFLAGS_DEFAULT = 0x00,
		DISABLE_BUFFERING = 0x01,
	};

	IOFile();
	IOFile(const std::string& filename, const char openmode[],
	       OpenFlags flags = OPENFLAGS_DEFAULT);

	~IOFile();

	IOFile(IOFile&& other);
	IOFile& operator=(IOFile&& other);

	void Swap(IOFile& other);

	bool Open(const std::string& filename, const char openmode[],
	          OpenFlags flags = OPENFLAGS_DEFAULT);
	bool Close();

	template <typename T>
	bool ReadArray(T* data, size_t length, size_t* pReadBytes = nullptr)
	{
		size_t read_bytes = 0;
#ifdef _WIN32
		// ReadFile can only really handle 4GB reads at the time.
		// Loop over the API call to read more if 'length' would be larger.
		DWORD read = 1;
		LPBYTE data_buf = reinterpret_cast<LPBYTE>(data);
		size_t remaining_length = length * sizeof(T);
		size_t copy;
		BOOL rv;
		if (IsOpen())
		{
			while (remaining_length > 0 && read > 0)
			{
				// Always within the limit of what a DWORD can hold.
				copy = std::min(remaining_length, static_cast<size_t>(UINT32_MAX));
				// So this cast is safe.
				rv = ReadFile(m_file, data_buf, static_cast<DWORD>(copy), &read, nullptr);
				if (FALSE == rv || copy != read)
				{
					m_good = false;
					break;
				}
				remaining_length -= copy;
				data_buf += copy;
				read_bytes += copy;
			}
		}
		else
		{
			m_good = false;
		}
#else
		if (!IsOpen() || length != (read_bytes = std::fread(data, sizeof(T), length, m_file)))
			m_good = false;

#endif
		if (pReadBytes)
			*pReadBytes = read_bytes;

		return m_good;
	}

	template <typename T>
	bool WriteArray(const T* data, size_t length)
	{
#ifdef _WIN32
		// WriteFile can only really handle 4GB at the time.
		// Loop over the API call to read more if 'length' would be larger.
		DWORD written;
		LPBYTE data_buf = reinterpret_cast<LPBYTE>(const_cast<T*>(data));
		size_t remaining_length = length * sizeof(T);
		size_t copy;
		BOOL rv;
		if (IsOpen())
		{
			while (remaining_length > 0)
			{
				// Always within the limit of what a DWORD can hold.
				copy = std::min(remaining_length, static_cast<size_t>(UINT32_MAX));
				// So this cast is safe.
				rv = WriteFile(m_file, data_buf, static_cast<DWORD>(copy), &written, nullptr);
				if (FALSE == rv)
				{
					m_good = false;
					break;
				}
				remaining_length -= written;
				data_buf += written;
			}
		}
		else
		{
			m_good = false;
		}
#else
		if (!IsOpen() || length != std::fwrite(data, sizeof(T), length, m_file))
			m_good = false;
#endif

		return m_good;
	}

	bool ReadBytes(void* data, size_t length, size_t* read_bytes = nullptr)
	{
		return ReadArray(reinterpret_cast<char*>(data), length, read_bytes);
	}

	bool WriteBytes(const void* data, size_t length)
	{
		return WriteArray(reinterpret_cast<const char*>(data), length);
	}

	// Wrapper around StringFromFormat + WriteBytes.
	template <typename... T>
	bool WriteFormat(T&&... args)
	{
		std::string text = StringFromFormat(std::forward<T>(args)...);
		return WriteBytes(text.c_str(), text.size());
	}

	bool IsOpen() const
	{
#ifdef _WIN32
		return INVALID_HANDLE_VALUE != m_file;
#else
		return nullptr != m_file;
#endif
	}

	// m_good is set to false when a read, write or other function fails
	bool IsGood() const { return m_good; }
	bool IsEOF() const;
	operator bool() { return IsOpen(); }

	bool Seek(s64 off, int origin);
	u64 Tell() const;
	u64 GetSize() const;
	bool Resize(u64 size);
	bool Flush();

	// clear error state
	void Clear()
	{
		m_good = true;
#ifndef _WIN32
		std::clearerr(m_file);
#endif
	}

private:
#ifdef _WIN32
	HANDLE m_file;
#else
	std::FILE* m_file;
#endif
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
