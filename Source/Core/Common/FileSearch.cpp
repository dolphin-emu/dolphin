// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <functional>

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
		for (auto& child : top.children)
			DoEntry(child);
	}
	// remove duplicates
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::vector<std::string> DoFileSearch(const std::vector<std::string>& exts, const std::vector<std::string>& directories, bool recursive)
{
	bool accept_all = std::find(exts.begin(), exts.end(), "") != exts.end();
	return FileSearchWithTest(directories, recursive, [&](const File::FSTEntry& entry) {
		if (accept_all)
			return true;
		std::string name = entry.virtualName;
		std::transform(name.begin(), name.end(), name.begin(), ::tolower);
		return std::any_of(exts.begin(), exts.end(), [&](const std::string& ext) {
			return name.length() >= ext.length() && name.compare(name.length() - ext.length(), ext.length(), ext) == 0;
		});
	});
}

// Result includes the passed directories themselves as well as their subdirectories.
std::vector<std::string> FindSubdirectories(const std::vector<std::string>& directories, bool recursive)
{
	return FileSearchWithTest(directories, true, [&](const File::FSTEntry& entry) {
		return entry.isDirectory;
	});
}
