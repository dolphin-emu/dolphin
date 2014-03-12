// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>

#include "Common/Common.h"
#include "Common/CommonPaths.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/HLE/HLE.h"
#include "Core/HLE/HLE_OS.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/PowerPC/PPCAnalyst.h"
#include "Core/PowerPC/PPCCache.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/SignatureDB.h"

#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeCreator.h"

#include "VideoCommon/TextureCacheBase.h"

namespace HLE_Misc
{

std::string args;
u32 argsPtr;

// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction()
{
	NPC = LR;
}

// If you want a function to panic, you can rename it PanicAlert :p
// Don't know if this is worth keeping.
void HLEPanicAlert()
{
	::PanicAlert("HLE: PanicAlert %08x", LR);
	NPC = LR;
}

void HBReload()
{
	// There isn't much we can do. Just stop cleanly.
	PowerPC::Pause();
	Host_Message(WM_USER_STOP);
}

void ExecuteDOL(u8* dolFile, u32 fileSize)
{
	// Clear memory before loading the dol
	for (u32 i = 0x80004000; i < Memory::Read_U32(0x00000034); i += 4)
	{
		// TODO: Should not write over the "save region"
		Memory::Write_U32(0x00000000, i);
	}
	CDolLoader dolLoader(dolFile, fileSize);
	dolLoader.Load();

	// Scan for common HLE functions
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableDebugging)
	{
		g_symbolDB.Clear();
		PPCAnalyst::FindFunctions(0x80004000, 0x811fffff, &g_symbolDB);
		SignatureDB db;
		if (db.Load(File::GetSysDirectory() + TOTALDB))
		{
			db.Apply(&g_symbolDB);
			HLE::PatchFunctions();
			db.Clear();
		}
	}

	PowerPC::ppcState.iCache.Reset();
	TextureCache::RequestInvalidateTextureCache();

	CWII_IPC_HLE_Device_usb_oh1_57e_305* s_Usb = GetUsbPointer();
	size_t size = s_Usb->m_WiiMotes.size();
	bool* wiiMoteConnected = new bool[size];
	for (unsigned int i = 0; i < size; i++)
		wiiMoteConnected[i] = s_Usb->m_WiiMotes[i].IsConnected();

	WII_IPC_HLE_Interface::Reset(true);
	WII_IPC_HLE_Interface::Init();
	s_Usb = GetUsbPointer();
	for (unsigned int i = 0; i < s_Usb->m_WiiMotes.size(); i++)
	{
		if (wiiMoteConnected[i])
		{
			s_Usb->m_WiiMotes[i].Activate(false);
			s_Usb->m_WiiMotes[i].Activate(true);
		}
		else
		{
			s_Usb->m_WiiMotes[i].Activate(false);
		}
	}

	delete[] wiiMoteConnected;

	if (argsPtr)
	{
		u32 args_base = Memory::Read_U32(0x800000f4);
		u32 ptr_to_num_args = 0xc;
		u32 num_args = 1;
		u32 hi_ptr = args_base + ptr_to_num_args + 4;
		u32 new_args_ptr = args_base + ptr_to_num_args + 8;

		Memory::Write_U32(ptr_to_num_args, args_base + 8);
		Memory::Write_U32(num_args, args_base + ptr_to_num_args);
		Memory::Write_U32(0x14, hi_ptr);

		for (unsigned int i = 0; i < args.length(); i++)
			Memory::WriteUnchecked_U8(args[i], new_args_ptr+i);
	}

	NPC = dolLoader.GetEntryPoint() | 0x80000000;
}

void LoadDOLFromDisc(std::string dol)
{
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(SConfig::GetInstance().m_LastFilename);
	DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

	if (dol.length() > 1 && dol.compare(0, 1, "/") == 0)
		dol = dol.substr(1);

	u32 fileSize = (u32) pFileSystem->GetFileSize(dol);
	u8* dolFile = new u8[fileSize];
	if (fileSize > 0)
	{
		pFileSystem->ReadFile(dol, dolFile, fileSize);
		ExecuteDOL(dolFile, fileSize);
	}
	delete[] dolFile;
}

void LoadBootDOLFromDisc()
{
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(SConfig::GetInstance().m_LastFilename);
	DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);
	u32 fileSize = pFileSystem->GetBootDOLSize();
	u8* dolFile = new u8[fileSize];
	if (fileSize > 0)
	{
		pFileSystem->GetBootDOL(dolFile, fileSize);
		ExecuteDOL(dolFile, fileSize);
	}
	delete[] dolFile;
}

u32 GetDolFileSize(std::string dol)
{
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(SConfig::GetInstance().m_LastFilename);
	DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

	std::string dolFile;

	if (dol.length() > 1 && dol.compare(0, 1, "/") == 0)
		dolFile = dol.substr(1);
	else
		dolFile = dol;

	return (u32)pFileSystem->GetFileSize(dolFile);
}

u16 GetIOSVersion()
{
	return Memory::Read_U16(0x00003140);
}

void OSGetResetCode()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		u32 resetCode = Memory::Read_U32(0xCC003024);

		if ((resetCode & 0x1fffffff) != 0)
		{
			GPR(3) = resetCode | 0x80000000;
		}
		else
		{
			GPR(3) = 0;
		}

		NPC = LR;
	}
	else
	{
		HLE::UnPatch("OSGetResetCode");
		NPC = PC;
	}
}

void OSBootDol()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bWii && GetIOSVersion() >= 30)
	{
		if ((GPR(4) >> 28) == 0x8)
		{
			u32 resetCode = GPR(30);

			// Reset game
			Memory::Write_U32(resetCode, 0xCC003024);
			LoadBootDOLFromDisc();
			return;
		}
		else if ((GPR(4) >> 28) == 0xA)
		{
			// Boot from disc partition
			PanicAlert("Boot Partition: %08x", GPR(26));
		}
		else if ((GPR(4) >> 28) == 0xC)
		{
			std::string dol;

			// Boot DOL from disc
			u32 ptr = GPR(28);
			Memory::GetString(dol, ptr);

			if (GetDolFileSize(dol) == 0)
			{
				ptr = GPR(30);
				Memory::GetString(dol, ptr);
				if (GetDolFileSize(dol) == 0)
				{
					// Cannot locate the dol file, exit.
					HLE::UnPatch("__OSBootDol");
					NPC = PC;
					return;
				}
			}

			argsPtr = Memory::Read_U32(GPR(5));
			Memory::GetString(args, argsPtr);
			LoadDOLFromDisc(dol);
			return;
		}
		else
		{
			PanicAlert("Unknown boot type: %08x", GPR(4));
		}
	}
	HLE::UnPatch("__OSBootDol");
	NPC = PC;
}

void HLEGeckoCodehandler()
{
	// Work around the codehandler not properly invalidating the icache, but
	// only the first few frames.
	// (Project M uses a conditional to only apply patches after something has
	// been read into memory, or such, so we do the first 5 frames.  More
	// robust alternative would be to actually detect memory writes, but that
	// would be even uglier.)
	u32 magic = 0xd01f1bad;
	u32 existing = Memory::Read_U32(0x80001800);
	if (existing - magic == 5)
	{
		return;
	}
	else if (existing - magic > 5)
	{
		existing = magic;
	}
	Memory::Write_U32(existing + 1, 0x80001800);
	PowerPC::ppcState.iCache.Reset();
}

}
