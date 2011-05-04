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

#include "Boot.h"
#include "../PowerPC/PowerPC.h"
#include "../HLE/HLE.h"
#include "../HW/Memmap.h"
#include "../ConfigManager.h"
#include "../PatchEngine.h"
#include "../IPC_HLE/WII_IPC_HLE.h"

#include "WiiWad.h"
#include "NANDContentLoader.h"
#include "FileUtil.h"
#include "Boot_DOL.h"
#include "Volume.h"
#include "VolumeCreator.h"
#include "CommonPaths.h"


bool CBoot::IsWiiWAD(const char *filename)
{
	return DiscIO::WiiWAD::IsWiiWAD(filename);
}

bool CBoot::Boot_WiiWAD(const char* _pFilename)
{
	const DiscIO::INANDContentLoader& ContentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(_pFilename);
	if (!ContentLoader.IsValid())
		return false;

	// create data directory
	File::CreateFullPath(Common::CreateTitleDataPath(ContentLoader.GetTitleID()) + DIR_SEP);

	// setup wii mem
	if (!SetupWiiMemory(ContentLoader.GetCountry()))
		return false;

	// DOL
	const DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex());
	if (pContent == NULL)
		return false;

	WII_IPC_HLE_Interface::SetDefaultContentFile(_pFilename);

	CDolLoader DolLoader(pContent->m_pData, pContent->m_Size);
	DolLoader.Load();
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

u64 CBoot::Install_WiiWAD(const char* _pFilename)
{
	if (!IsWiiWAD(_pFilename))
		return 0;
	const DiscIO::INANDContentLoader& ContentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(_pFilename);
	if (ContentLoader.IsValid() == false)
		return 0;

	u64 TitleID = ContentLoader.GetTitleID();
	u32 TitleID_HI = (u32)(TitleID >> 32);
	u32 TitleID_LO = (u32)(TitleID);

	//copy WAD's tmd header and contents to content directory

	char ContentPath[260+1];
	sprintf(ContentPath, "%stitle/%08x/%08x/content/",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), TitleID_HI, TitleID_LO);
	File::CreateFullPath(ContentPath);

	std::string TMDFileName(ContentPath);
	TMDFileName += "title.tmd";

	File::IOFile pTMDFile(TMDFileName, "wb");
	if (!pTMDFile)
	{
		PanicAlertT("WAD installation failed: error creating %s", TMDFileName.c_str());
		return 0;
	}

	pTMDFile.WriteBytes(ContentLoader.GetTmdHeader(), DiscIO::INANDContentLoader::TMD_HEADER_SIZE);
	
	for (u32 i = 0; i < ContentLoader.GetContentSize(); i++)
	{
		DiscIO::SNANDContent Content = ContentLoader.GetContent()[i];

		pTMDFile.WriteBytes(Content.m_Header, DiscIO::INANDContentLoader::CONTENT_HEADER_SIZE);

		char APPFileName[1024];
		if (Content.m_Type & 0x8000) //shared
		{
			sprintf(APPFileName, "%s", 
				DiscIO::CSharedContent::AccessInstance().AddSharedContent(Content.m_SHA1Hash).c_str());
		}
		else
		{
			sprintf(APPFileName, "%s%08x.app", ContentPath, Content.m_ContentID);
		}

		if (!File::Exists(APPFileName))
		{
			File::CreateFullPath(APPFileName);
			File::IOFile pAPPFile(APPFileName, "wb");
			if (!pAPPFile)
			{
				PanicAlertT("WAD installation failed: error creating %s", APPFileName);
				return 0;
			}
			
			pAPPFile.WriteBytes(Content.m_pData, Content.m_Size);
		}
		else
		{
			INFO_LOG(DISCIO, "Content %s already exists.", APPFileName);
		}
	}

	pTMDFile.Close();
	
	//Extract and copy WAD's ticket to ticket directory

	char TicketPath[260+1];
	sprintf(TicketPath, "%sticket/%08x/", File::GetUserPath(D_WIIUSER_IDX).c_str(), TitleID_HI);
	File::CreateFullPath(TicketPath);

	char TicketFileName[260+1];
	sprintf(TicketFileName, "%sticket/%08x/%08x.tik",
			File::GetUserPath(D_WIIUSER_IDX).c_str(), TitleID_HI, TitleID_LO);

	File::IOFile pTicketFile(TicketFileName, "wb");
	if (!pTicketFile)
	{
		PanicAlertT("WAD installation failed: error creating %s", TicketFileName);
		return 0;
	} 

	DiscIO::WiiWAD Wad(_pFilename);
	if (!Wad.IsValid())
	{
		return 0;
	}

	pTicketFile.WriteBytes(Wad.GetTicket(), Wad.GetTicketSize());
	
	if (!DiscIO::cUIDsys::AccessInstance().AddTitle(TitleID))
	{
		INFO_LOG(DISCIO, "Title %08x%08x, already exists in uid.sys", TitleID_HI, TitleID_LO);
	}

	return TitleID;
}


