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
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#endif

#if defined(__APPLE__)
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFUrl.h>
#include <CoreFoundation/CFBundle.h>
#include <sys/param.h>
#endif

#include <fstream>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#endif

// This namespace has various generic functions related to files and paths.
// The code still needs a ton of cleanup.
// REMEMBER: strdup considered harmful!
namespace File
{

// Remove any ending forward slashes from directory paths
// Modifies argument.
inline char *StripTailDirSlashes(char *fname)
{
	int len = (int)strlen(fname);
	int i = len - 1;
	if (len > 1)
		while (fname[i] == DIR_SEP_CHR)
			fname[i--] = '\0';
	return fname;
}

// Returns true if file filename exists
bool Exists(const char *filename)
{
	struct stat file_info;
		
	char *copy = StripTailDirSlashes(__strdup(filename));
	int result = stat(copy, &file_info);
	free(copy);

	return (result == 0);
}

// Returns true if filename is a directory
bool IsDirectory(const char *filename)
{
	struct stat file_info;

	char *copy = StripTailDirSlashes(__strdup(filename));

	int result = stat(copy, &file_info);
	free(copy);

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
	INFO_LOG(COMMON, "Delete: file %s\n", filename);

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
		WARN_LOG(COMMON, "Delete: DeleteFile failed on %s: %s", 
				 filename, GetLastErrorMsg());
		return false;
	}
#endif

	return true;
}

// Returns true if successful, or path already exists.
bool CreateDir(const char *path)
{
	INFO_LOG(COMMON, "CreateDir: directory %s\n", path);
#ifdef _WIN32
	if (::CreateDirectory(path, NULL))
		return true;
	DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS) 	{
		WARN_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: already exists",  path);
		return true;
	}
	ERROR_LOG(COMMON, "CreateDir: CreateDirectory failed on %s: %i", path, error);
	return false;
#else
	if (mkdir(path, 0755) == 0)
		return true;

	int err = errno;

	if (err == EEXIST) {
		WARN_LOG(COMMON, "CreateDir: mkdir failed on %s: already exists",  path);
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
	INFO_LOG(COMMON, "CreateFullPath: path %s\n", fullPath);
		
	if (File::Exists(fullPath)) {
		INFO_LOG(COMMON, "CreateFullPath: path exists %s\n", fullPath);
		return true;
	}

	// safety check to ensure we have good dir seperators
	std::string strFullPath(fullPath);
	NormalizeDirSep(&strFullPath);

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
		DEBUG_LOG(COMMON, "GetSize: %s: %ld", filename, buf.st_size);
		return buf.st_size;
	}

	ERROR_LOG(COMMON, "GetSize: Stat failed %s: %s",
			  filename, GetLastErrorMsg());
	return 0;
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
#endif
	File::DeleteDir(directory);
		
	return true;
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

//get the full config dir
char *GetConfigDirectory()
{

	static char path[MAX_PATH] = {0};
	if (strlen(path) > 0)
		return path;
	snprintf(path, sizeof(path), "%s" DIR_SEP CONFIG_FILE, GetUserDirectory());
	return path;

}

std::string GetBundleDirectory() 
{
	// Plugin path will be Dolphin.app/Contents/PlugIns
	CFURLRef BundleRef;
	char AppBundlePath[MAXPATHLEN];
	// Get the main bundle for the app
	BundleRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFStringRef BundlePath = CFURLCopyFileSystemPath(BundleRef, kCFURLPOSIXPathStyle);
	CFStringGetFileSystemRepresentation(BundlePath, AppBundlePath, sizeof(AppBundlePath));
	CFRelease(BundleRef);
	CFRelease(BundlePath);
#if defined(HAVE_WX) && HAVE_WX
	return AppBundlePath;
#else
	std::string NoWxBundleDirectory;
	NoWxBundleDirectory=AppBundlePath;
	NoWxBundleDirectory+=DIR_SEP;
	NoWxBundleDirectory+="Dolphin.app";
	return NoWxBundleDirectory;
#endif
}
#endif

// Returns the path to where the plugins are
std::string GetPluginsDirectory()
{
	std::string pluginsDir;

#if defined (__APPLE__)
	pluginsDir = GetBundleDirectory();
	pluginsDir += DIR_SEP;
	pluginsDir += PLUGINS_DIR;
#elif defined __linux__
	pluginsDir = PLUGINS_DIR;
	// FIXME global install
#else
	pluginsDir = PLUGINS_DIR;	
#endif

	pluginsDir += DIR_SEP;

	INFO_LOG(COMMON, "GetPluginsDirectory: Setting to %s:", pluginsDir.c_str());
	return pluginsDir;
	
}

// Returns the path to where the sys file are
std::string GetSysDirectory()
{
	std::string sysDir;

#if defined (__APPLE__)
	sysDir = GetBundleDirectory();
	sysDir += DIR_SEP;
	sysDir += SYSDATA_DIR;
#elif defined __linux__
	sysDir = SYSDATA_DIR;
	// FIXME global install
#else
	sysDir = SYSDATA_DIR;	
#endif

	sysDir += DIR_SEP;
	INFO_LOG(COMMON, "GetSysDirectory: Setting to %s:", sysDir.c_str());
	return sysDir;
}

// Returns a pointer to a string with a Dolphin data dir in the user's home
// directory. To be used in "multi-user" mode (that is, installed).
const char *GetUserDirectory()
{
	// Make sure we only need to do it once
	static char path[MAX_PATH] = {0};
	if (strlen(path) > 0)
		return path;

#ifdef WIN32
	char homedir[MAX_PATH];
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, path)))
		return NULL;
#else
	char *homedir = getenv("HOME");
	if (!homedir)
		return NULL;
#endif

	snprintf(path, sizeof(path), "%s" DIR_SEP DOLPHIN_DATA_DIR, homedir);
	INFO_LOG(COMMON, "GetUserDirectory: Setting to %s:", path);
	return path;
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
	fseek(f, 0, SEEK_END);
	size_t len = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = new char[len + 1];
	buf[fread(buf, 1, len, f)] = 0;
	str = std::string(buf, len);
	fclose(f);
	delete [] buf;
	return true;
}

} // namespace
