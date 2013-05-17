// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>


#include "Common.h"
#include "IniFile.h"
#include "LogManager.h"

#include "../../Core/Src/Core.h"
#include "../../Core/Src/ConfigManager.h"
#include "FileSystemGCWii.h"
#include "VolumeCreator.h"

namespace FileMon
{

DiscIO::IVolume *OpenISO = NULL;
DiscIO::IFileSystem *pFileSystem = NULL;
std::vector<const DiscIO::SFileInfo *> GCFiles;
std::string ISOFile = "", CurrentFile = "";
bool FileAccess = true;

// Filtered files
bool ShowSound(std::string FileName)
{
	std::string Ending;
	SplitPath(FileName, NULL, NULL, &Ending);
	std::transform(Ending.begin(),Ending.end(),Ending.begin(),::tolower);

	if (
		   (Ending == ".adp") // 1080 Avalanche, Crash Bandicoot, etc
		|| (Ending == ".afc") // Zelda WW
		|| (Ending == ".ast") // Zelda TP, Mario Kart
		|| (Ending == ".dsp") // Metroid Prime
		|| (Ending == ".hps") // SSB Melee

		|| (Ending == ".brstm") // Wii Sports, Wario Land, etc
		|| (Ending == ".sad") // Disaster
		)
		return true;

	return false;
}


// Read the GC file system
void ReadGC(std::string FileName)
{
	// Should have an actual Shutdown procedure or something
	if(OpenISO != NULL)
	{
		delete OpenISO;
		OpenISO = NULL;
	}
	if(pFileSystem != NULL)
	{
		delete pFileSystem;
		pFileSystem = NULL;
	}

	// GCFiles' pointers are no longer valid after pFileSystem is cleared
	GCFiles.clear();
	OpenISO = DiscIO::CreateVolumeFromFilename(FileName);
	if (!OpenISO) return;
	if (!DiscIO::IsVolumeWiiDisc(OpenISO) && !DiscIO::IsVolumeWadFile(OpenISO))
	{
		pFileSystem = DiscIO::CreateFileSystem(OpenISO);
		if(!pFileSystem) return;
		pFileSystem->GetFileList(GCFiles);
	}
	FileAccess = true;
}

// Check if we should play this file
void CheckFile(std::string File, u64 Size)
{
	// Don't do anything if the log is unselected
	if (!LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON))
		return;
	// Do nothing if we found the same file again
	if (CurrentFile == File)
		return;

	if (Size > 0)
		Size = (Size / 1000);

	std::string Str = StringFromFormat("%s kB %s", ThousandSeparate(Size, 7).c_str(), File.c_str());
	if (ShowSound(File))
	{
		INFO_LOG(FILEMON, "%s", Str.c_str());
	}
	else
	{
		WARN_LOG(FILEMON, "%s", Str.c_str());
	}

	// Update the current file
	CurrentFile = File;
}


// Find the GC filename
void FindFilename(u64 offset)
{
	// Don't do anything if a game is not running
	if (Core::GetState() != Core::CORE_RUN)
		return;

	// Or if the log is unselected
	if (!LogManager::GetInstance()->IsEnabled(LogTypes::FILEMON))
		return;

	// Or if we don't have file access
	if (!FileAccess)
		return;

	if (!pFileSystem || ISOFile != SConfig::GetInstance().m_LastFilename)
	{
		FileAccess = false;
		ReadGC(SConfig::GetInstance().m_LastFilename);
		ISOFile = SConfig::GetInstance().m_LastFilename;
		INFO_LOG(FILEMON, "Opening '%s'", ISOFile.c_str());
		return;
	}

	const char *fname = pFileSystem->GetFileName(offset);

	// There's something wrong with the paths
	if (!fname || (strlen(fname) == 512))
		return;
	
	CheckFile(fname, pFileSystem->GetFileSize(fname));
}

void Close()
{
	if(OpenISO != NULL)
	{
		delete OpenISO;
		OpenISO = NULL;
	}

	if(pFileSystem != NULL)
	{
		delete pFileSystem;
		pFileSystem = NULL;
	}

	// GCFiles' pointers are no longer valid after pFileSystem is cleared
	GCFiles.clear();

	ISOFile = "";
	CurrentFile = "";
	FileAccess = true;
}


} // FileMon
