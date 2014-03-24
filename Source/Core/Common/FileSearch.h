// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

class CFileSearch
{
public:
	typedef std::vector<std::string>XStringVector;

	CFileSearch(const XStringVector& rSearchStrings, const XStringVector& rDirectories);
	const XStringVector& GetFileNames() const;

private:

	void FindFiles(const std::string& searchString, const std::string& strPath);

	XStringVector m_FileNames;
};

