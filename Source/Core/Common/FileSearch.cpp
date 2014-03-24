// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/StringUtil.h"

#ifndef _WIN32
#include <dirent.h>
#else
#include <windows.h>
#endif


CFileSearch::CFileSearch(const CFileSearch::XStringVector& rSearchStrings, const CFileSearch::XStringVector& rDirectories)
{
	// Reverse the loop order for speed?
	for (auto& rSearchString : rSearchStrings)
	{
		for (auto& rDirectory : rDirectories)
		{
			FindFiles(rSearchString, rDirectory);
		}
	}
}


void CFileSearch::FindFiles(const std::string& searchString, const std::string& strPath)
{
	std::string GCMSearchPath;
	BuildCompleteFilename(GCMSearchPath, strPath, searchString);
#ifdef _WIN32
	WIN32_FIND_DATA findData;
	HANDLE FindFirst = FindFirstFile(UTF8ToTStr(GCMSearchPath).c_str(), &findData);

	if (FindFirst != INVALID_HANDLE_VALUE)
	{
		bool bkeepLooping = true;

		while (bkeepLooping)
		{
			if (findData.cFileName[0] != '.')
			{
				std::string strFilename;
				BuildCompleteFilename(strFilename, strPath, TStrToUTF8(findData.cFileName));
				m_FileNames.push_back(strFilename);
			}

			bkeepLooping = FindNextFile(FindFirst, &findData) ? true : false;
		}
	}
	FindClose(FindFirst);


#else
	// TODO: super lame/broken

	auto end_match(searchString);

	// assuming we have a "*.blah"-like pattern
	if (!end_match.empty() && end_match[0] == '*')
		end_match.erase(0, 1);

	// ugly
	if (end_match == ".*")
		end_match.clear();

	DIR* dir = opendir(strPath.c_str());

	if (!dir)
		return;

	while (auto const dp = readdir(dir))
	{
		std::string found(dp->d_name);

		if ((found != ".") && (found != "..") &&
		    (found.size() >= end_match.size()) &&
		    std::equal(end_match.rbegin(), end_match.rend(), found.rbegin()))
		{
			std::string full_name;
			if (strPath.c_str()[strPath.size()-1] == DIR_SEP_CHR)
				full_name = strPath + found;
			else
				full_name = strPath + DIR_SEP + found;

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
