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
#include <windows.h>
#include <shlobj.h>    // for SHGetFolderPath
#include <shellapi.h>
#include <commdlg.h>   // for GetSaveFileName
#include <io.h>
#else
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#endif

namespace File
{

bool Exists(const char *filename)
{
#ifdef _WIN32
	return GetFileAttributes(filename) != INVALID_FILE_ATTRIBUTES;
#else
	struct stat file_info;
	int result = stat(filename, &file_info);
	return result == 0;
#endif
}

bool IsDirectory(const char *filename)
{
#ifdef _WIN32
	DWORD Attribs = GetFileAttributes(filename);
	if (Attribs == INVALID_FILE_ATTRIBUTES)
		return false;

	return (GetFileAttributes(filename) & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
	struct stat file_info;
	int result = stat(filename, &file_info);
	if (result == 0)
		return S_ISDIR(file_info.st_mode);
	else
		return false;
#endif
}

bool Delete(const char *filename)
{
	if (!Exists(filename))
		return false;
	if (IsDirectory(filename))
		return false;
#ifdef _WIN32
	DeleteFile(filename);
#else
	unlink(filename);
#endif
	return true;
}

std::string SanitizePath(const char *filename)
{
	std::string copy = filename;
	for (size_t i = 0; i < copy.size(); i++)
		if (copy[i] == '/')
			copy[i] = '\\';
	return copy;
}

void Launch(const char *filename)
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

void Explore(const char *path)
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
bool CreateDir(const char *path)
{
#ifdef _WIN32
	if (::CreateDirectory(path, NULL))
		return true;
	DWORD error = GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
	{
		PanicAlert("%s already exists", path);
		return true;
	}
	PanicAlert("Error creating directory %s: %i", path, error);
	return false;
#else
	if (mkdir(path, 0755) == 0)
		return true;

	int err = errno;

	if (err == EEXIST)
	{
		PanicAlert("%s already exists", path);
		return true;
	}

	PanicAlert("Error creating directory %s: %s", path, strerror(err));
	return false;

#endif
}

bool DeleteDir(const char *filename)
{

	if (!File::IsDirectory(filename))
		return false;
#ifdef _WIN32
	return ::RemoveDirectory (filename) ? true : false;
#else
        if (rmdir(filename) == 0)
            return true;
        
        int err = errno;
        
        PanicAlert("Error removing directory %s",strerror(err));
        return false;
#endif
}

bool Rename(const char *srcFilename, const char *destFilename)
{
	return (rename(srcFilename, destFilename) == 0);
}

std::string GetUserDirectory()
{
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

u64 GetSize(const char *filename)
{
#ifdef _WIN32
    FILE *pFile = fopen(filename, "rb");
    if (pFile)
	{
            fseek(pFile, 0, SEEK_END);
            u64 pos = ftell(pFile);
            fclose(pFile);
            return pos;
        }
#else
    struct stat buf;
    if (stat(filename, &buf) == 0) {
        return buf.st_size;
    }
    int err = errno;
    PanicAlert("Error stating %s: %s", filename, strerror(err));
    
#endif
    return 0;
}

#ifdef _WIN32
static bool ReadFoundFile(const WIN32_FIND_DATA& ffd, FSTEntry& entry)
{
	// ignore files starting with a .
	if(strncmp(ffd.cFileName, ".", 1) == 0)
		return false;

	entry.virtualName = ffd.cFileName;

	if(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		entry.isDirectory = true;
	}
	else
	{
		entry.isDirectory = false;
		entry.size = ffd.nFileSizeLow;
	}

	return true;
}

u32 ScanDirectoryTree(const std::string& _Directory, FSTEntry& parentEntry)
{
	// Find the first file in the directory.
	WIN32_FIND_DATA ffd;
	std::string searchName = _Directory + "\\*";
	HANDLE hFind = FindFirstFile(searchName.c_str(), &ffd);

	u32 foundEntries = 0;

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			FSTEntry entry;

			if(ReadFoundFile(ffd, entry))
			{
				entry.physicalName = _Directory + "\\" + entry.virtualName;
				if(entry.isDirectory)
				{
					u32 childEntries = ScanDirectoryTree(entry.physicalName, entry);
					entry.size = childEntries;
					foundEntries += childEntries;
				}

				++foundEntries;

				parentEntry.children.push_back(entry);
			}
		} while (FindNextFile(hFind, &ffd) != 0);
	}

	FindClose(hFind);

	return foundEntries;
}
#else
u32 ScanDirectoryTree(const std::string& _Directory, FSTEntry& parentEntry)
{
    PanicAlert("Scan directory not implemanted yet\n");
    // TODO - Insert linux stuff here
    return 0;
}
#endif

bool CreateEmptyFile(const char *filename)
{
	FILE* pFile = fopen(filename, "wb");
	if (pFile == NULL)
		return false;

	fclose(pFile);
	return true;
}


bool DeleteDirRecursively(const std::string& _Directory)
{
#ifdef _WIN32

	bool Result = false;

	WIN32_FIND_DATA ffd;
	std::string searchName = _Directory + "\\*";
	HANDLE hFind = FindFirstFile(searchName.c_str(), &ffd);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			// check for "." and ".."
			if (((ffd.cFileName[0] == '.') && (ffd.cFileName[1] == 0x00)) ||
				((ffd.cFileName[0] == '.') && (ffd.cFileName[1] == '.') && (ffd.cFileName[2] == 0x00)))
				continue;

			// build path
			std::string newPath(_Directory);
			newPath += '\\';
			newPath += ffd.cFileName;

			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{			
				if (!File::DeleteDirRecursively(newPath))
					goto error_jmp;
			}
			else
			{
				if (!File::Delete(newPath.c_str()))
					goto error_jmp;
			}

		} while (FindNextFile(hFind, &ffd) != 0);
	}

	if (!File::DeleteDir(_Directory.c_str()))
		goto error_jmp;

	Result = true;

error_jmp:

	FindClose(hFind);

	return Result;
#else
        // taken from http://www.dreamincode.net/code/snippet2700.htm
        DIR *pdir = NULL; 
        pdir = opendir (_Directory.c_str());
        struct dirent *pent = NULL;
        
        if (pdir == NULL) { 
            return false; 
        } 

        char file[256];

        int counter = 1;
        
        while ((pent = readdir(pdir))) { 
            if (counter > 2) {
                for (int i = 0; i < 256; i++) file[i] = '\0';
                strcat(file, _Directory.c_str());
                if (pent == NULL) { 
                    return false; 
                } 
                strcat(file, pent->d_name); 
                if (IsDirectory(file) == true) {
                    DeleteDir(file);
                } else { 
                    remove(file);
                }
            }
            counter++;
        }

        
        return DeleteDir(_Directory.c_str());
#endif
}

} // namespace
