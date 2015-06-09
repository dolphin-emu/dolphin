// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#if !__clang__ && __GNUC__ == 4 && __GNUC_MINOR__ < 9
#error <regex> is broken in GCC < 4.9; please upgrade
#endif

#include <algorithm>
#include <functional>
#include <regex>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"

static std::vector<std::string> FileSearchWithTest(const std::vector<std::string>& directories, bool recursive, std::function<bool(const File::FSTEntry &)> callback)
{
	std::vector<std::string> result;
	for (const std::string& directory : directories)
	{
		File::FSTEntry top = File::ScanDirectoryTree(directory, recursive);

		std::function<void(File::FSTEntry&)> DoEntry;
		DoEntry = [&](File::FSTEntry& entry) {
			if (callback(entry))
				result.push_back(entry.physicalName);
			for (auto& child : entry.children)
				DoEntry(child);
		};
		DoEntry(top);
	}
	// remove duplicates
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::vector<std::string> DoFileSearch(const std::vector<std::string>& globs, const std::vector<std::string>& directories, bool recursive)
{
	std::string regex_str = "^(";
	for (const auto& str : globs)
	{
		if (regex_str.size() != 2)
			regex_str += "|";
		// convert glob to regex
		regex_str += std::regex_replace(std::regex_replace(str, std::regex("\\."), "\\."), std::regex("\\*"), ".*");
	}
	regex_str += ")$";
	std::regex regex(regex_str, std::regex_constants::icase);
	return FileSearchWithTest(directories, recursive, [&](const File::FSTEntry& entry) {
		return std::regex_match(entry.virtualName, regex);
	});
}

std::vector<std::string> FindSubdirectories(const std::vector<std::string>& directories, bool recursive)
{
	return FileSearchWithTest(directories, true, [&](const File::FSTEntry& entry) {
		return entry.isDirectory;
	});
}
