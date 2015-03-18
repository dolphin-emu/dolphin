// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <unordered_set>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Boot/Boot.h"

#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

namespace FileMon
{

static DiscIO::IVolume *OpenISO = nullptr;
static DiscIO::IFileSystem *pFileSystem = nullptr;
static std::vector<const DiscIO::SFileInfo *> DiscFiles;
static std::string ISOFile = "", CurrentFile = "";
static bool FileAccess = true;

// Filtered files
bool IsSoundFile(const std::string& filename)
{
	std::string extension;
	SplitPath(filename, nullptr, nullptr, &extension);
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	static std::unordered_set<std::string> extensions = {
		".adp",   // 1080 Avalanche, Crash Bandicoot, etc.
		".adx",   // Sonic Adventure 2 Battle, etc.
		".afc",   // Zelda WW
		".ast",   // Zelda TP, Mario Kart
		".brstm", // Wii Sports, Wario Land, etc.
		".dsp",   // Metroid Prime
		".hps",   // SSB Melee
		".ogg",   // Tony Hawk's Underground 2
		".sad",   // Disaster
		".snd",   // Tales of Symphonia
		".song",  // Tales of Symphonia
		".ssm",   // Custom Robo, Kirby Air Ride, etc.
		".str",   // Harry Potter & the Sorcerer's Stone
	};

	return extensions.find(extension) != extensions.end();
}


// Read the file system
void ReadFileSystem(const std::string& filename)
{
	// Should have an actual Shutdown procedure or something
	if (OpenISO != nullptr)
	{
		delete OpenISO;
		OpenISO = nullptr;
	}
	if (pFileSystem != nullptr)
	{
		delete pFileSystem;
		pFileSystem = nullptr;
	}

	// DiscFiles' pointers are no longer valid after pFileSystem is cleared
	DiscFiles.clear();
	OpenISO = DiscIO::CreateVolumeFromFilename(filename);
	if (!OpenISO)
		return;

	if (!OpenISO->IsWadFile())
	{
		pFileSystem = DiscIO::CreateFileSystem(OpenISO);

		if (!pFileSystem)
			return;

		pFileSystem->GetFileList(DiscFiles);
	}

	FileAccess = true;
}

// Logs a file if it passes a few checks
void CheckFile(const std::string& file, u64 size)
{
	// Don't do anything if the log is unselected
	if (!LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON, LogTypes::LWARNING))
		return;
	// Do nothing if we found the same file again
	if (CurrentFile == file)
		return;

	if (size > 0)
		size = (size / 1000);

	std::string str = StringFromFormat("%s kB %s", ThousandSeparate(size, 7).c_str(), file.c_str());
	if (IsSoundFile(file))
	{
		INFO_LOG(FILEMON, "%s", str.c_str());
	}
	else
	{
		WARN_LOG(FILEMON, "%s", str.c_str());
	}

	// Update the current file
	CurrentFile = file;
}


// Find the filename
void FindFilename(u64 offset)
{
	// Don't do anything if a game is not running
	if (Core::GetState() != Core::CORE_RUN)
		return;

	// Or if the log is unselected
	if (!LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON, LogTypes::LWARNING))
		return;

	// Or if we don't have file access
	if (!FileAccess)
		return;

	if (!pFileSystem || ISOFile != SConfig::GetInstance().m_LastFilename)
	{
		FileAccess = false;
		ReadFileSystem(SConfig::GetInstance().m_LastFilename);
		ISOFile = SConfig::GetInstance().m_LastFilename;
		INFO_LOG(FILEMON, "Opening '%s'", ISOFile.c_str());
		return;
	}

	const std::string filename = pFileSystem->GetFileName(offset);

	if (filename.empty())
		return;

	CheckFile(filename, pFileSystem->GetFileSize(filename));
}

void Close()
{
	if (OpenISO != nullptr)
	{
		delete OpenISO;
		OpenISO = nullptr;
	}

	if (pFileSystem != nullptr)
	{
		delete pFileSystem;
		pFileSystem = nullptr;
	}

	// DiscFiles' pointers are no longer valid after pFileSystem is cleared
	DiscFiles.clear();

	ISOFile = "";
	CurrentFile = "";
	FileAccess = true;
}


} // FileMon
