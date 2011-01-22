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
#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#else
#include <windows.h>
#endif

#include <string>

#include "FileSearch.h"

#include "StringUtil.h"


CFileSearch::CFileSearch(const CFileSearch::XStringVector& _rSearchStrings, const CFileSearch::XStringVector& _rDirectories)
{
	// Reverse the loop order for speed?
	for (size_t j = 0; j < _rSearchStrings.size(); j++)
	{
		for (size_t i = 0; i < _rDirectories.size(); i++)
		{
			FindFiles(_rSearchStrings[j], _rDirectories[i]);
		}
	}
}


void CFileSearch::FindFiles(const std::string& _searchString, const std::string& _strPath)
{
	std::string GCMSearchPath;
	BuildCompleteFilename(GCMSearchPath, _strPath, _searchString);
#ifdef _WIN32
	WIN32_FIND_DATA findData;
	HANDLE FindFirst = FindFirstFile(GCMSearchPath.c_str(), &findData);

	if (FindFirst != INVALID_HANDLE_VALUE)
	{
		bool bkeepLooping = true;

		while (bkeepLooping)
		{			
			if (findData.cFileName[0] != '.')
			{
				std::string strFilename;
				BuildCompleteFilename(strFilename, _strPath, findData.cFileName);
				m_FileNames.push_back(strFilename);
			}

			bkeepLooping = FindNextFile(FindFirst, &findData) ? true : false;
		}
	}
	FindClose(FindFirst);


#else
	size_t dot_pos = _searchString.rfind(".");

	if (dot_pos == std::string::npos)
		return;

	std::string ext = _searchString.substr(dot_pos);
	DIR* dir = opendir(_strPath.c_str());

	if (!dir)
		return;

	dirent* dp;

	while (true)
	{
		dp = readdir(dir);

		if (!dp)
			break;

		std::string s(dp->d_name);

		if ( (!ext.compare(".*") && s.compare(".") && s.compare("..")) ||
				((s.size() > ext.size()) && (!strcasecmp(s.substr(s.size() - ext.size()).c_str(), ext.c_str())) ))
		{
			std::string full_name;
			if (_strPath.c_str()[_strPath.size()-1] == DIR_SEP_CHR)
				full_name = _strPath + s;
			else
				full_name = _strPath + DIR_SEP + s;

			m_FileNames.push_back(full_name);
		}
	}

	closedir(dir);
#endif
}


const CFileSearch::XStringVector& CFileSearch::GetFileNames() const
{
	return m_FileNames;
}
