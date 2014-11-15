// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <functional>
#include <regex>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

std::vector<std::string> DoFileSearch(const std::vector<std::string>& globs, const std::vector<std::string>& directories, bool recursive)
{
	std::string regex_str = "^(";
	for (const auto& str : globs)
	{
		if (regex_str.size() != 2)
			regex_str += "|";
		// convert glob to regex
		regex_str += std::regex_replace(std::regex_replace(str, std::regex("\\."), std::string("\\.")), std::regex("\\*"), std::string(".*"));
	}
	regex_str += ")$";
	std::regex regex(regex_str);
	std::vector<std::string> result;
	for (const std::string& directory : directories)
	{
		File::FSTEntry entry = File::ScanDirectoryTree(directory, recursive);

		std::function<void(File::FSTEntry&)> DoEntry;
		DoEntry = [&](File::FSTEntry& thisEntry) {
			if (std::regex_match(thisEntry.virtualName, regex))
				result.push_back(thisEntry.physicalName);
			for (auto& child : thisEntry.children)
				DoEntry(child);
		};
		DoEntry(entry);
	}
	// remove duplicates
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

