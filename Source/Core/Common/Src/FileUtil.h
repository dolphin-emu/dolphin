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

#ifndef _FILEUTIL_H
#define _FILEUTIL_H

#include <string>
#include <vector>

#include "Common.h"

namespace File
{

struct FSTEntry
{
	bool isDirectory;
	u32 size;						// file length or number of entries from children
	std::string physicalName;		// name on disk
	std::string virtualName;		// name in FST names table
	std::vector<FSTEntry> children;
};

std::string SanitizePath(const char *filename);
bool Exists(const char *filename);
void Launch(const char *filename);
void Explore(const char *path);
bool IsDirectory(const char *filename);
bool CreateDir(const char *filename);
bool Delete(const char *filename);
bool DeleteDir(const char *filename);
bool Rename(const char *srcFilename, const char *destFilename);
u64 GetSize(const char *filename);
std::string GetUserDirectory();
bool CreateEmptyFile(const char *filename);

u32 ScanDirectoryTree(const std::string& _Directory, FSTEntry& parentEntry);

bool DeleteDirRecursively(const std::string& _Directory);

}  // namespace

#endif
