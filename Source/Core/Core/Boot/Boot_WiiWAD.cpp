// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StdMakeUnique.h"

#include "Core/ConfigManager.h"
#include "Core/PatchEngine.h"
#include "Core/Boot/Boot.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_FileIO.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/WiiWad.h"

static u32 state_checksum(u32 *buf, int len)
{
	u32 checksum = 0;
	len = len>>2;

	for (int i=0; i<len; i++)
	{
		checksum += buf[i];
	}

	return checksum;
}

struct StateFlags
{
	u32 checksum;
	u8 flags;
	u8 type;
	u8 discstate;
	u8 returnto;
	u32 unknown[6];
};

bool CBoot::Boot_WiiWAD(const std::string& _pFilename)
{
	std::string state_filename(Common::GetTitleDataPath(TITLEID_SYSMENU) + WII_STATE);

	if (File::Exists(state_filename))
	{
		File::IOFile state_file(state_filename, "r+b");
		StateFlags state;
		state_file.ReadBytes(&state, sizeof(StateFlags));

		state.type = 0x03; // TYPE_RETURN
		state.checksum = state_checksum((u32*)&state.flags, sizeof(StateFlags)-4);

		state_file.Seek(0, SEEK_SET);
		state_file.WriteBytes(&state, sizeof(StateFlags));
	}
	else
	{
		File::CreateFullPath(state_filename);
		File::IOFile state_file(state_filename, "a+b");
		StateFlags state;
		memset(&state,0,sizeof(StateFlags));
		state.type = 0x03; // TYPE_RETURN
		state.discstate = 0x01; // DISCSTATE_WII
		state.checksum = state_checksum((u32*)&state.flags, sizeof(StateFlags)-4);
		state_file.WriteBytes(&state, sizeof(StateFlags));
	}

	const DiscIO::INANDContentLoader& ContentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(_pFilename);
	if (!ContentLoader.IsValid())
		return false;

	u64 titleID = ContentLoader.GetTitleID();
	// create data directory
	File::CreateFullPath(Common::GetTitleDataPath(titleID));

	if (titleID == TITLEID_SYSMENU)
		HLE_IPC_CreateVirtualFATFilesystem();
	// setup Wii memory
	if (!SetupWiiMemory(ContentLoader.GetCountry()))
		return false;

	// DOL
	const DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(ContentLoader.GetBootIndex());
	if (pContent == nullptr)
		return false;

	WII_IPC_HLE_Interface::SetDefaultContentFile(_pFilename);

	std::unique_ptr<CDolLoader> pDolLoader;
	if (pContent->m_pData)
	{
		pDolLoader = std::make_unique<CDolLoader>(pContent->m_pData, pContent->m_Size);
	}
	else
	{
		pDolLoader = std::make_unique<CDolLoader>(pContent->m_Filename);
	}
	pDolLoader->Load();
	PC = pDolLoader->GetEntryPoint() | 0x80000000;

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
	if (pVolume != nullptr)
		PatchEngine::LoadPatches();

	return true;
}

