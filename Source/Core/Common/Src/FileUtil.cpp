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

#include "Common.h"
#include "CommonPaths.h"
#include "FileUtil.h"
#include "StringUtil.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>		// for SHGetFolderPath
#include <shellapi.h>
#include <commdlg.h>	// for GetSaveFileName
#include <io.h>
#include <direct.h>		// getcwd
#else
#include <sys/param.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFBundle.h>
#endif

#include <fstream>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#endif

#ifdef BSD4_4
#define stat64 stat	// XXX
#define fstat64 fstat	// XXX
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
		size_t i = fname.length() - 1;
		while (fname[i] == DIR_SEP_CHR)
			fname[i--] = '\0';
	}
	return;
}

// Returns true if file filename exists
bool Exists(const char *filename)
{
	struct stat64 file_info;

	std::string copy(filename);
	StripTailDirSlashes(copy);

	int result = stat64(copy.c_str(), &file_info);

	return (result == 0);
}

// Returns true if filename is a directory
bool IsDirectory(const char *filename)
{
	struct stat64 file_info;

	std::string copy(filename);
	StripTailDirSlashes(copy);

	int result = stat64(copy.c_str(), &file_info);

	if (result < 0) {
		WARN_LOG(COMMON, "IsDirectory: stat failed on %s: %s", 
				 filename, GetLastErrorMsg());
		return false;
	}

	return S_ISDIR(file_info.st_mode);
}

// Deletes a given filename, return true on success
// Doesn't supports deleting a directory
bool Delete(const char *filename)
{
	INFO_LOG(COMMON, "Delete: file %s", filename);

	// Return true because we care about the file no 
	// being there, not the actual delete.
	if (!Exists(filename)) {
		WARN_LOG(COMMON, "Delete: %s does not exists", filename);
		return true;
	}

	// We can't delete a directory
	if (IsDirectory(filename)) {
		WARN_LOG(COMMON, "Delete: %s is a directory", filename);
		return false;
	}

#ifdef _WIN32
	if (!DeleteFile(filename)) {
		WARN_LOG(COMMON, "Delete: DeleteFile failed on %s: %s", 
				 filename, GetLastErrorMsg());
		return false;
	}
#else
	if (unlink(filename) == -1) {
		WARN_LOG(COMMON, "Delete: unlink failed on %s: %s", 
				 filename, GetLastErrorMsg());
		return false;
	}
#endif

	return true;
}

// Returns true if successful, or path already exists.
bool CreateDir(const char *path)
{
	INFO_LOG(COMMON, "CreateDir: directory %s", path);
#ifdef _WIN32
	if (::CreateDirectory(path, NULL))
		return true;
	DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS) 	{
		WARN_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: already exists", path);
		return true;
	}
	ERROR_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: %i", path, error);
	return false;
#else
	if (mkdir(path, 0755) == 0)
		return true;

	int err = errno;

	if (err == EEXIST) {
		WARN_LOG(COMMON, "CreateDir: mkdir failed on %s: already exists", path);
		return true;
	}

	ERROR_LOG(COMMON, "CreateDir: mkdir failed on %s: %s", path, strerror(err));
	return false;
#endif
}

// Creates the full path of fullPath returns true on success
bool CreateFullPath(const char *fullPath)
{
	int panicCounter = 100;
	INFO_LOG(COMMON, "CreateFullPath: path %s", fullPath);
		
	if (File::Exists(fullPath)) {
		INFO_LOG(COMMON, "CreateFullPath: path exists %s", fullPath);
		return true;
	}

	// safety check to ensure we have good dir seperators
	std::string strFullPath(fullPath);

	const char *position = strFullPath.c_str();
	while (true) {
		// Find next sub path
		position = strchr(position, DIR_SEP_CHR);

		// we're done, yay!
		if (! position)
			return true;
			
		position++;

		// Create next sub path
		int sLen = (int)(position - strFullPath.c_str());
		if (sLen > 0) {
			char *subPath = strndup(strFullPath.c_str(), sLen);
			if (!File::IsDirectory(subPath)) {
				File::CreateDir(subPath);
			}
			free(subPath);
		}

		// A safety check
		panicCounter--;
		if (panicCounter <= 0) {
			ERROR_LOG(COMMON, "CreateFullPath: directory structure too deep");
			return false;
		}
	}
}


// Deletes a directory filename, returns true on success
bool DeleteDir(const char *filename)
{
	INFO_LOG(COMMON, "DeleteDir: directory %s", filename);

	// check if a directory
	if (!File::IsDirectory(filename)) {
		ERROR_LOG(COMMON, "DeleteDir: Not a directory %s",
				  filename);
		return false;
	}

#ifdef _WIN32
	if (::RemoveDirectory(filename))
		return true;
#else
	if (rmdir(filename) == 0)
		return true;
#endif
	ERROR_LOG(COMMON, "DeleteDir: %s: %s",
			  filename, GetLastErrorMsg());

	return false;
}

// renames file srcFilename to destFilename, returns true on success 
bool Rename(const char *srcFilename, const char *destFilename)
{
	INFO_LOG(COMMON, "Rename: %s --> %s", 
			 srcFilename, destFilename);
	if (rename(srcFilename, destFilename) == 0)
		return true;
	ERROR_LOG(COMMON, "Rename: failed %s --> %s: %s", 
			  srcFilename, destFilename, GetLastErrorMsg());
	return false;
}

// copies file srcFilename to destFilename, returns true on success 
bool Copy(const char *srcFilename, const char *destFilename)
{
	INFO_LOG(COMMON, "Copy: %s --> %s", 
			 srcFilename, destFilename);
#ifdef _WIN32
	if (CopyFile(srcFilename, destFilename, FALSE))
		return true;
		
	ERROR_LOG(COMMON, "Copy: failed %s --> %s: %s", 
			  srcFilename, destFilename, GetLastErrorMsg());
	return false;
#else

	// buffer size
#define BSIZE 1024

	char buffer[BSIZE];

	// Open input file
	FILE *input = fopen(srcFilename, "rb");
	if (!input)
	{
		ERROR_LOG(COMMON, "Copy: input failed %s --> %s: %s", 
				  srcFilename, destFilename, GetLastErrorMsg());
		return false;
	}

	// open output file
	FILE *output = fopen(destFilename, "wb");
	if (!output)
	{
		fclose(input);
		ERROR_LOG(COMMON, "Copy: output failed %s --> %s: %s", 
				  srcFilename, destFilename, GetLastErrorMsg());
		return false;
	}

	// copy loop
	while (!feof(input))
	{
		// read input
		int rnum = fread(buffer, sizeof(char), BSIZE, input);
		if (rnum != BSIZE)
		{
			if (ferror(input) != 0) {
				ERROR_LOG(COMMON, 
						  "Copy: failed reading from source, %s --> %s: %s", 
						  srcFilename, destFilename, GetLastErrorMsg());
				return false;
			}
		}

		// write output
		int wnum = fwrite(buffer, sizeof(char), rnum, output);
		if (wnum != rnum)
		{
			ERROR_LOG(COMMON, 
					  "Copy: failed writing to output, %s --> %s: %s", 
					  srcFilename, destFilename, GetLastErrorMsg());
			return false;
		}
	}
	// close flushs
	fclose(input);
	fclose(output);
	return true;
#endif
}

// Returns the size of filename (64bit)
u64 GetSize(const char *filename)
{
	if (!Exists(filename)) {
		WARN_LOG(COMMON, "GetSize: failed %s: No such file"
				 ,filename);
		return 0;
	}

	if (IsDirectory(filename)) {
		WARN_LOG(COMMON, "GetSize: failed %s: is a directory"
				 ,filename);
		return 0;
	}
	// on windows it's actually _stat64 defined in commonFuncs
	struct stat64 buf;
	if (stat64(filename, &buf) == 0) {
		DEBUG_LOG(COMMON, "GetSize: %s: %lld",
			filename, (long long)buf.st_size);
		return buf.st_size;
	}

	ERROR_LOG(COMMON, "GetSize: Stat failed %s: %s",
			  filename, GetLastErrorMsg());
	return 0;
}

// Overloaded GetSize, accepts file descriptor
u64 GetSize(const int fd)
{
	struct stat64 buf;
	if (fstat64(fd, &buf) != 0) {
		ERROR_LOG(COMMON, "GetSize: stat failed %i: %s",
			fd, GetLastErrorMsg());
		return 0;
	}
	return buf.st_size;
}

// Overloaded GetSize, accepts FILE*
u64 GetSize(FILE *f)
{
	// can't use off_t here because it can be 32-bit
	u64 pos = ftello(f);
	if (fseeko(f, 0, SEEK_END) != 0) {
		ERROR_LOG(COMMON, "GetSize: seek failed %p: %s",
			  f, GetLastErrorMsg());
		return 0;
	}
	u64 size = ftello(f);
	if ((size != pos) && (fseeko(f, pos, SEEK_SET) != 0)) {
		ERROR_LOG(COMMON, "GetSize: seek failed %p: %s",
			  f, GetLastErrorMsg());
		return 0;
	}
	return size;
}

// creates an empty file filename, returns true on success 
bool CreateEmptyFile(const char *filename)
{
	INFO_LOG(COMMON, "CreateEmptyFile: %s", filename); 

	FILE *pFile = fopen(filename, "wb");
	if (!pFile) {
		ERROR_LOG(COMMON, "CreateEmptyFile: failed %s: %s",
				  filename, GetLastErrorMsg());
		return false;
	}
	fclose(pFile);
	return true;
}


// Scans the directory tree gets, starting from _Directory and adds the
// results into parentEntry. Returns the number of files+directories found
u32 ScanDirectoryTree(const char *directory, FSTEntry& parentEntry)
{
	INFO_LOG(COMMON, "ScanDirectoryTree: directory %s", directory);
	// How many files + directories we found
	u32 foundEntries = 0;
	char *virtualName;
#ifdef _WIN32
	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;
	char searchName[MAX_PATH + 3];
	strncpy(searchName, directory, MAX_PATH);
	strcat(searchName, "\\*");

	HANDLE hFind = FindFirstFile(searchName, &ffd);
	if (hFind == INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return foundEntries;
	}
	// windows loop
	do {
		FSTEntry entry;
		virtualName = ffd.cFileName;
#else
	struct dirent dirent, *result = NULL;
	
	DIR *dirp = opendir(directory);
	if (!dirp)
		return 0;

	// non windows loop
	while (!readdir_r(dirp, &dirent, &result) && result) {
		FSTEntry entry;
		virtualName = result->d_name;
#endif
		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
			((virtualName[0] == '.') && (virtualName[1] == '.') && 
			 (virtualName[2] == '\0')))
			continue;
		entry.virtualName = virtualName;
		entry.physicalName = directory;
		entry.physicalName += DIR_SEP + entry.virtualName;
		
		if (IsDirectory(entry.physicalName.c_str())) {
			entry.isDirectory = true;
			// is a directory, lets go inside
			entry.size = ScanDirectoryTree(entry.physicalName.c_str(), entry);
			foundEntries += (u32)entry.size;
		} else { // is a file 
			entry.isDirectory = false;
			entry.size = GetSize(entry.physicalName.c_str());
		}
		++foundEntries;
		// Push into the tree
		parentEntry.children.push_back(entry);		
#ifdef _WIN32 
	} while (FindNextFile(hFind, &ffd) != 0);
	FindClose(hFind);
#else
	}
	closedir(dirp);
#endif
	// Return number of entries found.
	return foundEntries;
}

	
// deletes the given directory and anything under it. Returns true on
// success.
bool DeleteDirRecursively(const char *directory)
{
	INFO_LOG(COMMON, "DeleteDirRecursively: %s", directory);
#ifdef _WIN32
	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;
	char searchName[MAX_PATH + 3] = {0};

	strncpy(searchName, directory, MAX_PATH);
	strcat(searchName, "\\*");

	HANDLE hFind = FindFirstFile(searchName, &ffd);

	if (hFind == INVALID_HANDLE_VALUE) {
		FindClose(hFind);
		return false;
	}
		
	// windows loop
	do {
		char *virtualName = ffd.cFileName;
#else
	struct dirent dirent, *result = NULL;
	DIR *dirp = opendir(directory);
	if (!dirp)
		return false;

	// non windows loop
	while (!readdir_r(dirp, &dirent, &result) && result) {
		char *virtualName = result->d_name;
#endif

		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
			((virtualName[0] == '.') && (virtualName[1] == '.') && 
			 (virtualName[2] == '\0')))
			continue;

		char newPath[MAX_PATH];
		sprintf(newPath, "%s%c%s", directory, DIR_SEP_CHR, virtualName);
		if (IsDirectory(newPath)) {
			if (!DeleteDirRecursively(newPath))
				return false;
		} else {
			if (!File::Delete(newPath))
				return false;
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

//Create directory and copy contents (does not overwrite existing files)
void CopyDir(const char *source_path, const char *dest_path)
{
#ifndef _WIN32
	if (!strcmp(source_path, dest_path)) return;
	if (!File::Exists(source_path)) return;
	if (!File::Exists(dest_path)) File::CreateFullPath(dest_path);

	char *virtualName;
	struct dirent dirent, *result = NULL;
	DIR *dirp = opendir(source_path);
	if (!dirp) return;

	while (!readdir_r(dirp, &dirent, &result) && result)
	{
		virtualName = result->d_name;
		// check for "." and ".."
		if (((virtualName[0] == '.') && (virtualName[1] == '\0')) ||
			((virtualName[0] == '.') && (virtualName[1] == '.') &&
			(virtualName[2] == '\0')))
			continue;

		char source[FILENAME_MAX], dest[FILENAME_MAX];
		sprintf(source, "%s%s", source_path, virtualName);
		sprintf(dest, "%s%s", dest_path, virtualName);
		if (IsDirectory(source))
		{
			const unsigned int srclen = strlen(source);
			const unsigned int destlen = strlen(dest);
			source[srclen] = '/'; source[srclen+1] = '\0';
			dest[destlen]  = '/'; dest[destlen+1]  = '\0';
			if (!File::Exists(dest)) File::CreateFullPath(dest);
			CopyDir(source, dest);
		}
		else if (!File::Exists(dest)) File::Copy(source, dest);
	}
	closedir(dirp);
#endif
}

// Returns the current directory
std::string GetCurrentDir()
{
	char *dir;
	// Get the current working directory (getcwd uses malloc) 
	if (!(dir = __getcwd(NULL, 0))) {

		ERROR_LOG(COMMON, "GetCurrentDirectory failed: %s",
				  GetLastErrorMsg());
		return NULL;
	}
	std::string strDir = dir;
	free(dir);
	return strDir;
}

// Sets the current directory to the given directory
bool SetCurrentDir(const char *_rDirectory)
{
	return __chdir(_rDirectory) == 0;
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

std::string GetSysDirectory()
{
	std::string sysDir;

#if defined (__APPLE__)
	sysDir = GetBundleDirectory();
	sysDir += DIR_SEP;
	sysDir += SYSDATA_DIR;
#else
	sysDir = SYSDATA_DIR;
#endif
	sysDir += DIR_SEP;

	INFO_LOG(COMMON, "GetSysDirectory: Setting to %s:", sysDir.c_str());
	return sysDir;
}

// Returns a pointer to a string with a Dolphin data dir or file in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
std::string &GetUserPath(int DirIDX)
{
	static std::string UserDir;
	static std::string GCUserDir;
	static std::string WiiUserDir;
	static std::string WiiRootDir;
	static std::string ConfigDir;
	static std::string GameConfigDir;
	static std::string MapsDir;
	static std::string CacheDir;
	static std::string ShaderCacheDir;
	static std::string ShadersDir;
	static std::string StateSavesDir;
	static std::string ScreenShotsDir;
	static std::string OpenCLDir;
	static std::string HiresTexturesDir;
	static std::string DumpDir;
	static std::string DumpFramesDir;
	static std::string DumpAudioDir;
	static std::string DumpTexturesDir;
	static std::string DumpDSPDir;
	static std::string LogsDir;
	static std::string MailLogsDir;
	static std::string WiiSYSCONFDir;
	static std::string DolphinConfig;
	static std::string DSPConfig;
	static std::string DebuggerConfig;
	static std::string LoggerConfig;
	static std::string MainLog;
	static std::string WiiSYSCONF;
	static std::string RamDump;
	static std::string ARamDump;
	static std::string GCSRam;
	static std::string Default;

	// Set up all paths and files on the first run
	if (UserDir.empty())
	{
#ifdef _WIN32
		// Keep the directory setup the way it was on windows
		UserDir = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
#else
		if (File::Exists(ROOT_DIR DIR_SEP USERDATA_DIR))
			UserDir = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
		else
			UserDir = std::string(getenv("HOME")) + DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;
#endif
		INFO_LOG(COMMON, "GetUserPath: Setting user directory to %s:", UserDir.c_str());

		GCUserDir = UserDir + GC_USER_DIR DIR_SEP;
		WiiUserDir = UserDir + WII_USER_DIR DIR_SEP;
		WiiRootDir = UserDir + WII_USER_DIR;
		ConfigDir = UserDir + CONFIG_DIR DIR_SEP;
		GameConfigDir = UserDir + GAMECONFIG_DIR DIR_SEP;
		MapsDir = UserDir + MAPS_DIR DIR_SEP;
		CacheDir = UserDir + CACHE_DIR DIR_SEP;
		ShaderCacheDir = UserDir + SHADERCACHE_DIR DIR_SEP;
		ShadersDir = UserDir + SHADERS_DIR DIR_SEP;
		StateSavesDir = UserDir + STATESAVES_DIR DIR_SEP;
		ScreenShotsDir = UserDir + SCREENSHOTS_DIR DIR_SEP;
		OpenCLDir = UserDir + OPENCL_DIR DIR_SEP;
		HiresTexturesDir = UserDir + HIRES_TEXTURES_DIR DIR_SEP;
		DumpDir = UserDir + DUMP_DIR DIR_SEP;
		DumpFramesDir = UserDir + DUMP_FRAMES_DIR DIR_SEP;
		DumpAudioDir = UserDir + DUMP_AUDIO_DIR DIR_SEP;
		DumpTexturesDir = UserDir + DUMP_TEXTURES_DIR DIR_SEP;
		DumpDSPDir = UserDir + DUMP_DSP_DIR DIR_SEP;
		LogsDir = UserDir + LOGS_DIR DIR_SEP;
		MailLogsDir = UserDir + MAIL_LOGS_DIR DIR_SEP;
		WiiSYSCONFDir = UserDir + WII_SYSCONF_DIR DIR_SEP;
		DolphinConfig = ConfigDir + DOLPHIN_CONFIG;
		DSPConfig = ConfigDir + DSP_CONFIG;
		DebuggerConfig = ConfigDir + DEBUGGER_CONFIG;
		LoggerConfig = ConfigDir + LOGGER_CONFIG;
		MainLog = LogsDir + MAIN_LOG;
		WiiSYSCONF = WiiSYSCONFDir + WII_SYSCONF;
		RamDump = DumpDir + RAM_DUMP;
		ARamDump = DumpDir + ARAM_DUMP;
		GCSRam = GCUserDir + GC_SRAM;
	}
	switch (DirIDX)
	{
		case D_USER_IDX:
			return UserDir;
		case D_GCUSER_IDX:
			return GCUserDir;
		case D_WIIUSER_IDX:
			return WiiUserDir;
		case D_WIIROOT_IDX:
			return WiiRootDir;
		case D_CONFIG_IDX:
			return ConfigDir;
		case D_GAMECONFIG_IDX:
			return GameConfigDir;
		case D_MAPS_IDX:
			return MapsDir;
		case D_CACHE_IDX:
			return CacheDir;
		case D_SHADERCACHE_IDX:
			return ShaderCacheDir;
		case D_SHADERS_IDX:
			return ShadersDir;
		case D_STATESAVES_IDX:
			return StateSavesDir;
		case D_SCREENSHOTS_IDX:
			return ScreenShotsDir;
		case D_OPENCL_IDX:
			return OpenCLDir;
		case D_HIRESTEXTURES_IDX:
			return HiresTexturesDir;
		case D_DUMP_IDX:
			return DumpDir;
		case D_DUMPFRAMES_IDX:
			return DumpFramesDir;
		case D_DUMPAUDIO_IDX:
			return DumpAudioDir;
		case D_DUMPTEXTURES_IDX:
			return DumpTexturesDir;
		case D_DUMPDSP_IDX:
			return DumpDSPDir;
		case D_LOGS_IDX:
			return LogsDir;
		case D_MAILLOGS_IDX:
			return MailLogsDir;
		case D_WIISYSCONF_IDX:
			return WiiSYSCONFDir;
		case F_DOLPHINCONFIG_IDX:
			return DolphinConfig;
		case F_DSPCONFIG_IDX:
			return DSPConfig;
		case F_DEBUGGERCONFIG_IDX:
			return DebuggerConfig;
		case F_LOGGERCONFIG_IDX:
			return LoggerConfig;
		case F_MAINLOG_IDX:
			return MainLog;
		case F_WIISYSCONF_IDX:
			return WiiSYSCONF;
		case F_RAMDUMP_IDX:
			return RamDump;
		case F_ARAMDUMP_IDX:
			return ARamDump;
		case F_GCSRAM_IDX:
			return GCSRam;
		default:
			return Default;
	}
}

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename)
{
	FILE *f = fopen(filename, text_file ? "w" : "wb");
	if (!f)
		return false;
	size_t len = str.size();
	if (len != fwrite(str.data(), 1, str.size(), f))
	{
		fclose(f);
		return false;
	}
	fclose(f);
	return true;
}

bool ReadFileToString(bool text_file, const char *filename, std::string &str)
{
	FILE *f = fopen(filename, text_file ? "r" : "rb");
	if (!f)
		return false;
	size_t len = (size_t)GetSize(f);
	char *buf = new char[len + 1];
	buf[fread(buf, 1, len, f)] = 0;
	str = std::string(buf, len);
	fclose(f);
	delete [] buf;
	return true;
}

} // namespace
