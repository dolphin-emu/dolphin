// Copyright (C) 2003-2008 Dolphin Project.

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

#ifdef _WIN32
#include <shlobj.h>    // for SHGetFolderPath
#include <shellapi.h>
#include <commdlg.h>   // for GetSaveFileName
#else

#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#endif

bool File::Exists(const std::string &filename)
{
#ifdef _WIN32
	return GetFileAttributes(filename.c_str()) != INVALID_FILE_ATTRIBUTES;
#else
	struct stat file_info;
	int result = stat(filename.c_str(), &file_info);
	return result == 0;
#endif
}

bool File::IsDirectory(const std::string &filename) {
#ifdef _WIN32
	return (GetFileAttributes(filename.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
        struct stat file_info;
	int result = stat(filename.c_str(), &file_info);
        if (result == 0)
          return S_ISDIR(file_info.st_mode);
        else 
          return false;
#endif
}

std::string SanitizePath(const std::string &filename) {
	std::string copy = filename;
	for (size_t i = 0; i < copy.size(); i++)
		if (copy[i] == '/')
			copy[i] = '\\';
	return copy;
}

void File::Launch(const std::string &filename)
{
#ifdef _WIN32
	std::string win_filename = SanitizePath(filename);
	SHELLEXECUTEINFO shex = { sizeof(shex) };
	shex.fMask = SEE_MASK_NO_CONSOLE; // | SEE_MASK_ASYNC_OK;
	shex.lpVerb = "open";
	shex.lpFile = win_filename.c_str();
	shex.nShow = SW_SHOWNORMAL;
	ShellExecuteEx(&shex);
#else
	// TODO: Insert GNOME/KDE code here.
#endif
}

void File::Explore(const std::string &path)
{
#ifdef _WIN32
	std::string win_path = SanitizePath(path);
	SHELLEXECUTEINFO shex = { sizeof(shex) };
	shex.fMask = SEE_MASK_NO_CONSOLE; // | SEE_MASK_ASYNC_OK;
	shex.lpVerb = "explore";
	shex.lpFile = win_path.c_str();
	shex.nShow = SW_SHOWNORMAL;
	ShellExecuteEx(&shex);
#else
	// TODO: Insert GNOME/KDE code here.
#endif
}

// Returns true if successful, or path already exists.
bool File::CreateDir(const std::string &path)
{
#ifdef _WIN32
	if (::CreateDirectory(path.c_str(), NULL))
		return true;
	DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
	{
		PanicAlert("%s already exists", path.c_str());
		return true;
	}
	PanicAlert("Error creating directory: %i", error);
	return false;
#else
    if (mkdir(path.c_str(), 0644) == 0)
		return true;

    int err = errno;

    if (err == EEXIST) {
		PanicAlert("%s already exists", path.c_str());
		return true;
    }

    PanicAlert("Error creating directory: %s", strerror(err));
	return false;
          
#endif
}

std::string GetUserDirectory() {
#ifdef _WIN32 
	char path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, path)))
	{
		return std::string(path);
	}
	return std::string("");
#else
	char *dir = getenv("HOME");
	if (!dir)
		return std::string("");
	return dir; 
#endif
}
