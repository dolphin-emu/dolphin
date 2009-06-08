// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Boot.h"
#include "../PowerPC/PowerPC.h"
#include "../HLE/HLE.h"
#include "../HW/Memmap.h"
#include "../ConfigManager.h"
#include "../PatchEngine.h"
#include "../IPC_HLE/WII_IPC_HLE.h"

#include "NANDContentLoader.h"
#include "FileUtil.h"
#include "Boot_DOL.h"
#include "Volume.h"
#include "VolumeCreator.h"


bool CBoot::IsWiiWAD(const char *filename)
{
    return DiscIO::CNANDContentManager::IsWiiWAD(filename);
}

bool CBoot::Boot_WiiWAD(const char* _pFilename)
{        
    const DiscIO::INANDContentLoader& ContentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(_pFilename);
    if (ContentLoader.IsValid() == false)
        return false;

    // create Home directory
    char Path[260+1];
    u64 TitleID = ContentLoader.GetTitleID();
    char* pTitleID = (char*)&TitleID;
    sprintf(Path, FULL_WII_USER_DIR "title//%02x%02x%02x%02x/%02x%02x%02x%02x/data/nocopy/",
        (u8)pTitleID[7], (u8)pTitleID[6], (u8)pTitleID[5], (u8)pTitleID[4],
        (u8)pTitleID[3], (u8)pTitleID[2], (u8)pTitleID[1], (u8)pTitleID[0]);
    File::CreateFullPath(Path);

    // setup wii mem
    if (SetupWiiMemory(ContentLoader.GetCountry()) == false)
        return false;

	// DOL
    const DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex());
    if (pContent == NULL)
        return false;

    WII_IPC_HLE_Interface::SetDefaultContentFile(_pFilename);

    CDolLoader DolLoader(pContent->m_pData, pContent->m_Size);
	PC = DolLoader.GetEntryPoint() | 0x80000000;

	// Pass the "#002 check"
	// Apploader should write the IOS version and revision to 0x3140, and compare it
	// to 0x3188 to pass the check, but we don't do it, and i don't know where to read the IOS rev...
	// Currently we just write 0xFFFF for the revision, copy manually and it works fine :p

	// TODO : figure it correctly : where should we read the IOS rev that the wad "needs" ?
	Memory::Write_U16(ContentLoader.GetIosVersion(), 0x00003140);
	Memory::Write_U16(0xFFFF, 0x00003142);
	Memory::Write_U32(Memory::Read_U32(0x00003140), 0x00003188);

	// Load patches and run startup patches
	const DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(_pFilename);
	if (pVolume != NULL)
		PatchEngine::LoadPatches(pVolume->GetUniqueID().c_str());

    return true;
}


