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


/////////////////////////////////////////////////////////////////////////////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
#include "stdafx.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>


#include "Common.h"	// Common
#include "IniFile.h"
#include "LogManager.h"

#include "../../Core/Src/ConfigManager.h"
#include "FileSystemGCWii.h"
#include "VolumeCreator.h"
/////////////////////////////////////////////////////////////////////////////////////////////////


namespace FileMon
{

/////////////////////////////////////////////////////////////////////////////////////////////////
// Declarations and definitions
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
DiscIO::IVolume *OpenISO;
DiscIO::IFileSystem *pFileSystem = NULL;
std::vector<const DiscIO::SFileInfo *> GCFiles;
std::string ISOFile, CurrentFile;
bool FileAccess = true;
/////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////
// Filtered files
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
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
/////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////
// Read the GC file system
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void ReadGC(std::string FileName)
{
	OpenISO = DiscIO::CreateVolumeFromFilename(FileName);
	if (!OpenISO) return;
	if (!DiscIO::IsVolumeWiiDisc(OpenISO))
	{
		pFileSystem = DiscIO::CreateFileSystem(OpenISO);
		pFileSystem->GetFileList(GCFiles);
	}
	FileAccess = true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////
// Check if we should play this file
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void CheckFile(std::string File, int Size)
{
	// Don't do anything if the log is unselected
	if (!LogManager::GetInstance()->isEnable(LogTypes::FILEMON)) return;
	// Do nothing if we found the same file again
	if (CurrentFile == File) return;

	if (Size > 0) Size = (Size / 1000);
	std::string Str = StringFromFormat("%s kB %s", ThS(Size, true, 7).c_str(), File.c_str());
	if (ShowSound(File))
	{
		NOTICE_LOG(FILEMON, Str.c_str());
	}
	else
	{
		WARN_LOG(FILEMON, Str.c_str());
	}

	// Update the current file
	CurrentFile = File;
}
/////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////
// Find the GC filename
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void FindFilename(u64 offset)
{
	// Don't do anything if the log is unselected
	if (!LogManager::GetInstance()->isEnable(LogTypes::FILEMON)) return;
	if (!FileAccess) return;

	if (!pFileSystem || ISOFile != SConfig::GetInstance().m_LastFilename)
	{
		FileAccess = false;
		ReadGC(SConfig::GetInstance().m_LastFilename);
		ISOFile = SConfig::GetInstance().m_LastFilename;
		NOTICE_LOG(FILEMON, "Opening '%s'", ISOFile.c_str());
		return;
	}

	// File
	if (!pFileSystem->GetFileName(offset)) return;
	std::string File = std::string(pFileSystem->GetFileName(offset));
	// There's something wrong with the paths
	if (File.length() == 512) return;

	int Size = (int)pFileSystem->GetFileSize(File.c_str());

	CheckFile(File, Size);
}
/////////////////////////////////////////////////////////////////////////////////////////////////


} // FileMon
