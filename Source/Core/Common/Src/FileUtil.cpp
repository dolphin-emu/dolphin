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

#include <fstream>
#include <sys/stat.h>

#ifndef S_ISDIR
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#endif

namespace File
{

inline void stripTailDirSlashes(std::string& fname) {
	while(fname.at(fname.length() - 1) == DIR_SEP_CHR)
		fname.resize(fname.length() - 1);
}

bool Exists(const char *filename)
{
	struct stat file_info;

	std::string copy = filename;
	stripTailDirSlashes(copy);

	int result = stat(copy.c_str(), &file_info);
	return (result == 0);
}

bool IsDirectory(const char *filename)
{
	struct stat file_info;
	std::string copy = filename;
	stripTailDirSlashes(copy);

	int result = stat(copy.c_str(), &file_info);
	if (result == 0)
		return S_ISDIR(file_info.st_mode);
	else
		return false;
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
#ifdef _WIN32
    for (size_t i = 0; i < copy.size(); i++)
        if (copy[i] == '/')
            copy[i] = '\\';
#else
    // Should we do the otherway around?
#endif
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

// Create several dirs
bool CreateDirectoryStructure(const std::string& _rFullPath)
{
	int PanicCounter = 10;

	size_t Position = 0;
	while(true)
	{
		// Find next sub path, support both \ and / directory separators
		{
			size_t nextPosition = _rFullPath.find(DIR_SEP_CHR, Position);
			Position = nextPosition;

			if (Position == std::string::npos)
				return true;

			Position++;
		}

		// Create next sub path
		std::string SubPath = _rFullPath.substr(0, Position);
		if (!SubPath.empty())
		{
			if (!File::IsDirectory(SubPath.c_str()))
			{
				File::CreateDir(SubPath.c_str());
				LOG(WII_IPC_FILEIO, "    CreateSubDir %s", SubPath.c_str());
			}
		}

		// A safety check
		PanicCounter--;
		if (PanicCounter <= 0)
		{
			PanicAlert("CreateDirectoryStruct creates way to much dirs...");
			return false;
		}
	}
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

bool Copy(const char *srcFilename, const char *destFilename)
{
#ifdef _WIN32 
	return (CopyFile(srcFilename, destFilename, FALSE) == TRUE) ? true : false;
#else

#define BSIZE 1024

        int rnum, wnum, err;
        char buffer[BSIZE];
        FILE *output, *input;
                
        if (! (input = fopen(srcFilename, "r"))) {
            err = errno;
            PanicAlert("Error copying from %s: %s", srcFilename, strerror(err));
            return false;
        }

        if (! (output = fopen(destFilename, "w"))) {
            err = errno;
            PanicAlert("Error copying to %s: %s", destFilename, strerror(err));
            return false;
        }

	while(! feof(input)) {	
            if((rnum = fread(buffer, sizeof(char), BSIZE, input)) != BSIZE) {
		if(ferror(input) != 0){
                    PanicAlert("can't read source file\n");
                    return false;
                }
            }
            
            if((wnum = fwrite(buffer, sizeof(char), rnum, output))!= rnum){
		PanicAlert("can't write output file\n");
		return false;
            }
	}
        
        fclose(input);
        fclose(output);

        return true;
      /*
        std::ifstream ifs(srcFilename, std::ios::binary);
        std::ofstream ofs(destFilename, std::ios::binary);
        
        ofs << ifs.rdbuf();

        ifs.close();
        ofs.close();
          
        return true;*/
#endif
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
	if(!Exists(filename))
		return 0;

    struct stat buf;
    if (stat(filename, &buf) == 0) {
        return buf.st_size;
    }
    int err = errno;
    
	PanicAlert("Error accessing %s: %s", filename, strerror(err));
    
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

void GetCurrentDirectory(std::string& _rDirectory)
{
	char tmpBuffer[MAX_PATH+1];
	getcwd(tmpBuffer, MAX_PATH);
	_rDirectory = tmpBuffer;
}

} // namespace
