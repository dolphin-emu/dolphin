// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"

#ifdef _WIN32
#include <commdlg.h>   // for GetSaveFileName
#include <direct.h>    // getcwd
#include <io.h>
#include <objbase.h>   // guid stuff
#include <shellapi.h>
#include <windows.h>
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
#include <sys/param.h>
#endif

#ifndef S_ISDIR
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#endif

#if defined BSD4_4 || defined __FreeBSD__
#define stat64 stat
#define fstat64 fstat
#endif

// This namespace has various generic functions related to files and paths.
// The code still needs a ton of cleanup.
// REMEMBER: strdup considered harmful!
namespace File
{

// Remove any ending forward slashes from directory paths
// Modifies argument.
static void StripTailDirSlashes(std::string &fname)
{
	if (fname.length() > 1)
	{
		while (fname.back() == DIR_SEP_CHR)
			fname.pop_back();
	}
}

// Returns true if file filename exists
bool Exists(const std::string &filename)
{
	struct stat64 file_info;

	std::string copy(filename);
	StripTailDirSlashes(copy);

#ifdef _WIN32
	int result = _tstat64(UTF8ToTStr(copy).c_str(), &file_info);
#else
	int result = stat64(copy.c_str(), &file_info);
#endif

	return (result == 0);
}

// Returns true if filename is a directory
bool IsDirectory(const std::string &filename)
{
	struct stat64 file_info;

	std::string copy(filename);
	StripTailDirSlashes(copy);

#ifdef _WIN32
	int result = _tstat64(UTF8ToTStr(copy).c_str(), &file_info);
#else
	int result = stat64(copy.c_str(), &file_info);
#endif

	if (result < 0)
	{
		WARN_LOG(COMMON, "IsDirectory: stat failed on %s: %s",
				 filename.c_str(), GetLastErrorMsg().c_str());
		return false;
	}

	return S_ISDIR(file_info.st_mode);
}

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const std::string &filename)
{
	INFO_LOG(COMMON, "Delete: file %s", filename.c_str());

	// Return true because we care about the file no
	// being there, not the actual delete.
	if (!Exists(filename))
	{
		WARN_LOG(COMMON, "Delete: %s does not exist", filename.c_str());
		return true;
	}

	// We can't delete a directory
	if (IsDirectory(filename))
	{
		WARN_LOG(COMMON, "Delete failed: %s is a directory", filename.c_str());
		return false;
	}

#ifdef _WIN32
	if (!DeleteFile(UTF8ToTStr(filename).c_str()))
	{
		WARN_LOG(COMMON, "Delete: DeleteFile failed on %s: %s",
				 filename.c_str(), GetLastErrorMsg().c_str());
		return false;
	}
#else
	if (unlink(filename.c_str()) == -1)
	{
		WARN_LOG(COMMON, "Delete: unlink failed on %s: %s",
				 filename.c_str(), GetLastErrorMsg().c_str());
		return false;
	}
#endif

	return true;
}

// Returns true if successful, or path already exists.
bool CreateDir(const std::string &path)
{
	INFO_LOG(COMMON, "CreateDir: directory %s", path.c_str());
#ifdef _WIN32
	if (::CreateDirectory(UTF8ToTStr(path).c_str(), nullptr))
		return true;
	DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
	{
		WARN_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: already exists", path.c_str());
		return true;
	}
	ERROR_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: %i", path.c_str(), error);
	return false;
#else
	if (mkdir(path.c_str(), 0755) == 0)
		return true;

	int err = errno;

	if (err == EEXIST)
	{
		WARN_LOG(COMMON, "CreateDir: mkdir failed on %s: already exists", path.c_str());
		return true;
	}

	ERROR_LOG(COMMON, "CreateDir: mkdir failed on %s: %s", path.c_str(), strerror(err));
	return false;
#endif
}

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const std::string &fullPath)
{
	int panicCounter = 100;
	INFO_LOG(COMMON, "CreateFullPath: path %s", fullPath.c_str());

	if (File::Exists(fullPath))
	{
		INFO_LOG(COMMON, "CreateFullPath: path exists %s", fullPath.c_str());
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
		if (!File::IsDirectory(subPath))
			File::CreateDir(subPath);

		// A safety check
		panicCounter--;
		if (panicCounter <= 0)
		{
			ERROR_LOG(COMMON, "CreateFullPath: directory structure is too deep");
			return false;
		}
		position++;
	}
}


// Deletes a directory filename, returns true on success
bool DeleteDir(const std::string &filename)
{
	INFO_LOG(COMMON, "DeleteDir: directory %s", filename.c_str());

	// check if a directory
	if (!File::IsDirectory(filename))
	{
		ERROR_LOG(COMMON, "DeleteDir: Not a directory %s", filename.c_str());
		return false;
	}

#ifdef _WIN32
	if (::RemoveDirectory(UTF8ToTStr(filename).c_str()))
		return true;
#else
	if (rmdir(filename.c_str()) == 0)
		return true;
#endif
	ERROR_LOG(COMMON, "DeleteDir: %s: %s", filename.c_str(), GetLastErrorMsg().c_str());

	return false;
}

// renames file srcFilename to destFilename, returns true on success
bool Rename(const std::string &srcFilename, const std::string &destFilename)
{
	INFO_LOG(COMMON, "Rename: %s --> %s",
			srcFilename.c_str(), destFilename.c_str());
#ifdef _WIN32
	auto sf = UTF8ToTStr(srcFilename);
	auto df = UTF8ToTStr(destFilename);
	// The Internet seems torn about whether ReplaceFile is atomic or not.
	// Hopefully it's atomic enough...
	if (ReplaceFile(df.c_str(), sf.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr))
		return true;
	// Might have failed because the destination doesn't exist.
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
	{
		if (MoveFile(sf.c_str(), df.c_str()))
			return true;
	}
#else
	if (rename(srcFilename.c_str(), destFilename.c_str()) == 0)
		return true;
#endif
	ERROR_LOG(COMMON, "Rename: failed %s --> %s: %s",
			  srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg().c_str());
	return false;
}

#ifndef _WIN32
static void FSyncPath(const char *path)
{
	int fd = open(path, O_RDONLY);
	if (fd != -1)
	{
		fsync(fd);
		close(fd);
	}
}
#endif

bool RenameSync(const std::string &srcFilename, const std::string &destFilename)
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
	char *path = strdup(srcFilename.c_str());
	FSyncPath(path);
	FSyncPath(dirname(path));
	free(path);
	path = strdup(destFilename.c_str());
	FSyncPath(dirname(path));
	free(path);
#endif
	return true;
}

// copies file srcFilename to destFilename, returns true on success
bool Copy(const std::string &srcFilename, const std::string &destFilename)
{
	INFO_LOG(COMMON, "Copy: %s --> %s",
			srcFilename.c_str(), destFilename.c_str());
#ifdef _WIN32
	if (CopyFile(UTF8ToTStr(srcFilename).c_str(), UTF8ToTStr(destFilename).c_str(), FALSE))
		return true;

	ERROR_LOG(COMMON, "Copy: failed %s --> %s: %s",
			srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg().c_str());
	return false;
#else

	// buffer size
#define BSIZE 1024

	char buffer[BSIZE];

	// Open input file
	std::ifstream input;
	OpenFStream(input, srcFilename, std::ifstream::in | std::ifstream::binary);
	if (!input.is_open())
	{
		ERROR_LOG(COMMON, "Copy: input failed %s --> %s: %s",
				srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg().c_str());
		return false;
	}

	// open output file
	File::IOFile output(destFilename, "wb");

	if (!output.IsOpen())
	{
		ERROR_LOG(COMMON, "Copy: output failed %s --> %s: %s",
				srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg().c_str());
		return false;
	}

	// copy loop
	while (!input.eof())
	{
		// read input
		input.read(buffer, BSIZE);
		if (!input)
		{
			ERROR_LOG(COMMON,
					"Copy: failed reading from source, %s --> %s: %s",
					srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg().c_str());
			return false;
		}

		// write output
		if (!output.WriteBytes(buffer, BSIZE))
		{
			ERROR_LOG(COMMON,
					"Copy: failed writing to output, %s --> %s: %s",
					srcFilename.c_str(), destFilename.c_str(), GetLastErrorMsg().c_str());
			return false;
		}
	}

	return true;
#endif
}

// Returns the size of filename (64bit)
u64 GetSize(const std::string &filename)
{
	if (!Exists(filename))
	{
		WARN_LOG(COMMON, "GetSize: failed %s: No such file", filename.c_str());
		return 0;
	}

	if (IsDirectory(filename))
	{
		WARN_LOG(COMMON, "GetSize: failed %s: is a directory", filename.c_str());
		return 0;
	}

	struct stat64 buf;
#ifdef _WIN32
	if (_tstat64(UTF8ToTStr(filename).c_str(), &buf) == 0)
#else
	if (stat64(filename.c_str(), &buf) == 0)
#endif
	{
		DEBUG_LOG(COMMON, "GetSize: %s: %lld",
				filename.c_str(), (long long)buf.st_size);
		return buf.st_size;
	}

	ERROR_LOG(COMMON, "GetSize: Stat failed %s: %s",
			filename.c_str(), GetLastErrorMsg().c_str());
	return 0;
}

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd)
{
	struct stat64 buf;
	if (fstat64(fd, &buf) != 0)
	{
		ERROR_LOG(COMMON, "GetSize: stat failed %i: %s",
			fd, GetLastErrorMsg().c_str());
		return 0;
	}
	return buf.st_size;
}

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE *f)
{
	// can't use off_t here because it can be 32-bit
	u64 pos = ftello(f);
	if (fseeko(f, 0, SEEK_END) != 0)
	{
		ERROR_LOG(COMMON, "GetSize: seek failed %p: %s",
			  f, GetLastErrorMsg().c_str());
		return 0;
	}

	u64 size = ftello(f);
	if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0))
	{
		ERROR_LOG(COMMON, "GetSize: seek failed %p: %s",
			  f, GetLastErrorMsg().c_str());
		return 0;
	}

	return size;
}

// creates an empty file filename, returns true on success
bool CreateEmptyFile(const std::string &filename)
{
	INFO_LOG(COMMON, "CreateEmptyFile: %s", filename.c_str());

	if (!File::IOFile(filename, "wb"))
	{
		ERROR_LOG(COMMON, "CreateEmptyFile: failed %s: %s",
				  filename.c_str(), GetLastErrorMsg().c_str());
		return false;
	}

	return true;
}


// Scans the directory tree gets, starting from _Directory and adds the
// results into parentEntry. Returns the number of files+directories found
FSTEntry ScanDirectoryTree(const std::string &directory, bool recursive)
{
	INFO_LOG(COMMON, "ScanDirectoryTree: directory %s", directory.c_str());
	// How many files + directories we found
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
	struct dirent dirent, *result = nullptr;

	DIR *dirp = opendir(directory.c_str());
	if (!dirp)
			return parent_entry;

		// non Windows loop
		while (!readdir_r(dirp, &dirent, &result) && result)
		{
			const std::string virtual_name(result->d_name);
	#endif
			if (virtual_name == "." || virtual_name == "..")
				continue;
			auto physical_name = directory + DIR_SEP + virtual_name;
			FSTEntry entry;
			entry.isDirectory = IsDirectory(physical_name);
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
				entry.size = GetSize(physical_name);
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
	closedir(dirp);
#endif
	// Return number of entries found.
	return parent_entry;
}


// Deletes the given directory and anything under it. Returns true on success.
bool DeleteDirRecursively(const std::string &directory)
{
	INFO_LOG(COMMON, "DeleteDirRecursively: %s", directory.c_str());
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
	struct dirent dirent, *result = nullptr;
	DIR *dirp = opendir(directory.c_str());
	if (!dirp)
		return false;

	// non Windows loop
	while (!readdir_r(dirp, &dirent, &result) && result)
	{
		const std::string virtualName = result->d_name;
#endif

		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
			((virtualName[0] == '.') && (virtualName[1] == '.') &&
			 (virtualName[2] == '\0')))
			continue;

		std::string newPath = directory + DIR_SEP_CHR + virtualName;
		if (IsDirectory(newPath))
		{
			if (!DeleteDirRecursively(newPath))
			{
				#ifndef _WIN32
				closedir(dirp);
				#endif

				return false;
			}
		}
		else
		{
			if (!File::Delete(newPath))
			{
				#ifndef _WIN32
				closedir(dirp);
				#endif

				return false;
			}
		}

#ifdef _WIN32
	} while (FindNextFile(hFind, &ffd) != 0);
	FindClose(hFind);
#else
	}
	closedir(dirp);
#endif
	File::DeleteDir(directory);

	return true;
}

// Create directory and copy contents (does not overwrite existing files)
void CopyDir(const std::string &source_path, const std::string &dest_path)
{
	if (source_path == dest_path) return;
	if (!File::Exists(source_path)) return;
	if (!File::Exists(dest_path)) File::CreateFullPath(dest_path);

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
	struct dirent dirent, *result = nullptr;
	DIR *dirp = opendir(source_path.c_str());
	if (!dirp) return;

	while (!readdir_r(dirp, &dirent, &result) && result)
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
			if (!File::Exists(dest)) File::CreateFullPath(dest + DIR_SEP);
			CopyDir(source, dest);
		}
		else if (!File::Exists(dest)) File::Copy(source, dest);
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
	char *dir;
	// Get the current working directory (getcwd uses malloc)
	if (!(dir = __getcwd(nullptr, 0)))
	{
		ERROR_LOG(COMMON, "GetCurrentDirectory failed: %s",
				GetLastErrorMsg().c_str());
		return nullptr;
	}
	std::string strDir = dir;
	free(dir);
	return strDir;
}

// Sets the current directory to the given directory
bool SetCurrentDir(const std::string &directory)
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
	CoCreateGuid(&guid);
	TCHAR tguid[40];
	StringFromGUID2(guid, tguid, 39);
	tguid[39] = 0;
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

std::string GetTempFilenameForAtomicWrite(const std::string &path)
{
	std::string abs = path;
#ifdef _WIN32
	TCHAR absbuf[MAX_PATH];
	if (_tfullpath(absbuf, UTF8ToTStr(path).c_str(), MAX_PATH) != nullptr)
		abs = TStrToUTF8(absbuf);
#else
	char absbuf[PATH_MAX];
	if (realpath(path.c_str(), absbuf) != nullptr)
		abs = absbuf;
#endif
	return abs + ".xxx";
}

#if defined(__APPLE__)
std::string GetBundleDirectory()
{
	CFURLRef BundleRef;
	char AppBundlePath[MAXPATHLEN];
	// Get the main bundle for the app
	BundleRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFStringRef BundlePath = CFURLCopyFileSystemPath(BundleRef, kCFURLPOSIXPathStyle);
	CFStringGetFileSystemRepresentation(BundlePath, AppBundlePath, sizeof(AppBundlePath));
	CFRelease(BundleRef);
	CFRelease(BundlePath);

	return AppBundlePath;
}
#endif

#ifdef _WIN32
std::string& GetExeDirectory()
{
	static std::string DolphinPath;
	if (DolphinPath.empty())
	{
		TCHAR Dolphin_exe_Path[2048];
		GetModuleFileName(nullptr, Dolphin_exe_Path, 2048);
		DolphinPath = TStrToUTF8(Dolphin_exe_Path);
		DolphinPath = DolphinPath.substr(0, DolphinPath.find_last_of('\\'));
	}
	return DolphinPath;
}
#endif

std::string GetSysDirectory()
{
	std::string sysDir;

#if defined (__APPLE__)
	sysDir = GetBundleDirectory() + DIR_SEP + SYSDATA_DIR;
#elif defined (_WIN32)
	sysDir = GetExeDirectory() + DIR_SEP + SYSDATA_DIR;
#else
	sysDir = SYSDATA_DIR;
#endif
	sysDir += DIR_SEP;

	INFO_LOG(COMMON, "GetSysDirectory: Setting to %s:", sysDir.c_str());
	return sysDir;
}

static std::string s_user_paths[NUM_PATH_INDICES];
static void RebuildUserDirectories(unsigned int dir_index)
{
	switch (dir_index)
	{
	case D_USER_IDX:
		s_user_paths[D_GCUSER_IDX]         = s_user_paths[D_USER_IDX] + GC_USER_DIR DIR_SEP;
		s_user_paths[D_WIIROOT_IDX]        = s_user_paths[D_USER_IDX] + WII_USER_DIR;
		s_user_paths[D_CONFIG_IDX]         = s_user_paths[D_USER_IDX] + CONFIG_DIR DIR_SEP;
		s_user_paths[D_GAMESETTINGS_IDX]   = s_user_paths[D_USER_IDX] + GAMESETTINGS_DIR DIR_SEP;
		s_user_paths[D_MAPS_IDX]           = s_user_paths[D_USER_IDX] + MAPS_DIR DIR_SEP;
		s_user_paths[D_CACHE_IDX]          = s_user_paths[D_USER_IDX] + CACHE_DIR DIR_SEP;
		s_user_paths[D_SHADERCACHE_IDX]    = s_user_paths[D_USER_IDX] + SHADERCACHE_DIR DIR_SEP;
		s_user_paths[D_SHADERS_IDX]        = s_user_paths[D_USER_IDX] + SHADERS_DIR DIR_SEP;
		s_user_paths[D_STATESAVES_IDX]     = s_user_paths[D_USER_IDX] + STATESAVES_DIR DIR_SEP;
		s_user_paths[D_SCREENSHOTS_IDX]    = s_user_paths[D_USER_IDX] + SCREENSHOTS_DIR DIR_SEP;
		s_user_paths[D_LOAD_IDX]           = s_user_paths[D_USER_IDX] + LOAD_DIR DIR_SEP;
		s_user_paths[D_HIRESTEXTURES_IDX]  = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
		s_user_paths[D_DUMP_IDX]           = s_user_paths[D_USER_IDX] + DUMP_DIR DIR_SEP;
		s_user_paths[D_DUMPFRAMES_IDX]     = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
		s_user_paths[D_DUMPAUDIO_IDX]      = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
		s_user_paths[D_DUMPTEXTURES_IDX]   = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
		s_user_paths[D_DUMPDSP_IDX]        = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
		s_user_paths[D_LOGS_IDX]           = s_user_paths[D_USER_IDX] + LOGS_DIR DIR_SEP;
		s_user_paths[D_MAILLOGS_IDX]       = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
		s_user_paths[D_THEMES_IDX]         = s_user_paths[D_USER_IDX] + THEMES_DIR DIR_SEP;
		s_user_paths[F_DOLPHINCONFIG_IDX]  = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
		s_user_paths[F_DEBUGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
		s_user_paths[F_LOGGERCONFIG_IDX]   = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
		s_user_paths[F_MAINLOG_IDX]        = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
		s_user_paths[F_RAMDUMP_IDX]        = s_user_paths[D_DUMP_IDX] + RAM_DUMP;
		s_user_paths[F_ARAMDUMP_IDX]       = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
		s_user_paths[F_FAKEVMEMDUMP_IDX]   = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
		s_user_paths[F_GCSRAM_IDX]         = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
		break;

	case D_CONFIG_IDX:
		s_user_paths[F_DOLPHINCONFIG_IDX]  = s_user_paths[D_CONFIG_IDX] + DOLPHIN_CONFIG;
		s_user_paths[F_DEBUGGERCONFIG_IDX] = s_user_paths[D_CONFIG_IDX] + DEBUGGER_CONFIG;
		s_user_paths[F_LOGGERCONFIG_IDX]   = s_user_paths[D_CONFIG_IDX] + LOGGER_CONFIG;
		break;

	case D_GCUSER_IDX:
		s_user_paths[F_GCSRAM_IDX]         = s_user_paths[D_GCUSER_IDX] + GC_SRAM;
		break;

	case D_DUMP_IDX:
		s_user_paths[D_DUMPFRAMES_IDX]     = s_user_paths[D_DUMP_IDX] + DUMP_FRAMES_DIR DIR_SEP;
		s_user_paths[D_DUMPAUDIO_IDX]      = s_user_paths[D_DUMP_IDX] + DUMP_AUDIO_DIR DIR_SEP;
		s_user_paths[D_DUMPTEXTURES_IDX]   = s_user_paths[D_DUMP_IDX] + DUMP_TEXTURES_DIR DIR_SEP;
		s_user_paths[D_DUMPDSP_IDX]        = s_user_paths[D_DUMP_IDX] + DUMP_DSP_DIR DIR_SEP;
		s_user_paths[F_RAMDUMP_IDX]        = s_user_paths[D_DUMP_IDX] + RAM_DUMP;
		s_user_paths[F_ARAMDUMP_IDX]       = s_user_paths[D_DUMP_IDX] + ARAM_DUMP;
		s_user_paths[F_FAKEVMEMDUMP_IDX]   = s_user_paths[D_DUMP_IDX] + FAKEVMEM_DUMP;
		break;

	case D_LOGS_IDX:
		s_user_paths[D_MAILLOGS_IDX]       = s_user_paths[D_LOGS_IDX] + MAIL_LOGS_DIR DIR_SEP;
		s_user_paths[F_MAINLOG_IDX]        = s_user_paths[D_LOGS_IDX] + MAIN_LOG;
		break;

	case D_LOAD_IDX:
		s_user_paths[D_HIRESTEXTURES_IDX]  = s_user_paths[D_LOAD_IDX] + HIRES_TEXTURES_DIR DIR_SEP;
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

	// If theme does not exist in user's dir load from shared directory
	if (!File::Exists(dir))
		dir = GetSysDirectory() + THEMES_DIR "/" + theme_name + "/";

	return dir;
}

bool WriteStringToFile(const std::string &str, const std::string& filename)
{
	return File::IOFile(filename, "wb").WriteBytes(str.data(), str.size());
}

bool ReadFileToString(const std::string& filename, std::string &str)
{
	File::IOFile file(filename, "rb");
	auto const f = file.GetHandle();

	if (!f)
		return false;

	size_t read_size;
	str.resize(GetSize(f));
	bool retval = file.ReadArray(&str[0], str.size(), &read_size);

	return retval;
}

IOFile::IOFile()
	: m_file(nullptr), m_good(true)
{}

IOFile::IOFile(std::FILE* file)
	: m_file(file), m_good(true)
{}

IOFile::IOFile(const std::string& filename, const char openmode[])
	: m_file(nullptr), m_good(true)
{
	Open(filename, openmode);
}

IOFile::~IOFile()
{
	Close();
}

IOFile::IOFile(IOFile&& other)
	: m_file(nullptr), m_good(true)
{
	Swap(other);
}

IOFile& IOFile::operator=(IOFile&& other)
{
	Swap(other);
	return *this;
}

void IOFile::Swap(IOFile& other)
{
	std::swap(m_file, other.m_file);
	std::swap(m_good, other.m_good);
}

bool IOFile::Open(const std::string& filename, const char openmode[])
{
	Close();
#ifdef _WIN32
	_tfopen_s(&m_file, UTF8ToTStr(filename).c_str(), UTF8ToTStr(openmode).c_str());
#else
	m_file = fopen(filename.c_str(), openmode);
#endif

	m_good = IsOpen();
	return m_good;
}

bool IOFile::Close()
{
	if (!IsOpen() || 0 != std::fclose(m_file))
		m_good = false;

	m_file = nullptr;
	return m_good;
}

std::FILE* IOFile::ReleaseHandle()
{
	std::FILE* const ret = m_file;
	m_file = nullptr;
	return ret;
}

void IOFile::SetHandle(std::FILE* file)
{
	Close();
	Clear();
	m_file = file;
}

u64 IOFile::GetSize()
{
	if (IsOpen())
		return File::GetSize(m_file);
	else
		return 0;
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
		return -1;
}

bool IOFile::Flush()
{
	if (!IsOpen() || 0 != std::fflush(m_file))
		m_good = false;

	return m_good;
}

bool IOFile::Resize(u64 size)
{
	if (!IsOpen() || 0 !=
#ifdef _WIN32
		// ector: _chsize sucks, not 64-bit safe
		// F|RES: changed to _chsize_s. i think it is 64-bit safe
		_chsize_s(_fileno(m_file), size)
#else
		// TODO: handle 64bit and growing
		ftruncate(fileno(m_file), size)
#endif
	)
		m_good = false;

	return m_good;
}

} // namespace
