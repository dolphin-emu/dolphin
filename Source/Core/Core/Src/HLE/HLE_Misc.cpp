// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include "Common.h"
#include "HLE_OS.h"

#include "../PowerPC/PowerPC.h"
#include "../HW/Memmap.h"
#include "../Host.h"
#include "IPC_HLE/WII_IPC_HLE_Device_DI.h"
#include "ConfigManager.h"
#include "VolumeCreator.h"
#include "Filesystem.h"
#include "../Boot/Boot_DOL.h"
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
#include "HLE.h"
#include "PowerPC/PPCAnalyst.h"
#include "PowerPC/SignatureDB.h"
#include "PowerPC/PPCSymbolDB.h"
#include "CommonPaths.h"
#include "TextureCacheBase.h"

namespace HLE_Misc
{

std::string args;
u32 argsPtr;
u32 bootType;

// Helper to quickly read the floating point value at a memory location.
inline float F(u32 addr)
{
	u32 mem = Memory::ReadFast32(addr);
	return *((float*)&mem);
}

// Helper to quickly write a floating point value to a memory location.
inline void FW(u32 addr, float x) 
{
	u32 data = *((u32*)&x);
	Memory::WriteUnchecked_U32(data, addr);
}

// If you just want to kill a function, one of the three following are usually appropriate.
// According to the PPC ABI, the return value is always in r3.
void UnimplementedFunction()
{
	NPC = LR;
}

void UnimplementedFunctionTrue()
{
	GPR(3) = 1;
	NPC = LR;
}

void UnimplementedFunctionFalse()
{
	GPR(3) = 0;
	NPC = LR;
}

void GXPeekZ()
{
	// Just some fake Z value.
	Memory::Write_U32(0xFFFFFF, GPR(5));
	NPC = LR;
}

void GXPeekARGB()
{
	// Just some fake color value.
	Memory::Write_U32(0xFFFFFFFF, GPR(5));
	NPC = LR;
}

// If you want a function to panic, you can rename it PanicAlert :p
// Don't know if this is worth keeping.
void HLEPanicAlert()
{
	::PanicAlert("HLE: PanicAlert %08x", LR);
	NPC = LR;
}

// Computes the cosine of the angle between the two fvec3s pointed at by r3 and r4.
void SMB_EvilVecCosine()
{
	u32 r3 = GPR(3);
	u32 r4 = GPR(4);

	float x1 = F(r3);
	float y1 = F(r3 + 4);
	float z1 = F(r3 + 8);

	float x2 = F(r4);
	float y2 = F(r4 + 4);
	float z2 = F(r4 + 8);

	float s1 = x1*x1 + y1*y1 + z1*z1;
	float s2 = x2*x2 + y2*y2 + z2*z2;
	
	float dot = x1*x2 + y1*y2 + z1*z2;

	rPS0(1) = dot / sqrtf(s1 * s2);
	NPC = LR;
}

// Normalizes the vector pointed at by r3.
void SMB_EvilNormalize()
{
	u32 r3 = GPR(3);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float len = x*x + y*y + z*z;
	float inv_len;
	if (len <= 0)
		inv_len = 0;
	else
		inv_len = 1.0f / sqrtf(len);
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r3, x);
	FW(r3 + 4, y);
	FW(r3 + 8, z);
	NPC = LR;
}

// Scales the vector pointed at by r3 to the length specified by f0.
// Writes results to vector pointed at by r4.
void SMB_evil_vec_setlength()
{
	u32 r3 = GPR(3);
	u32 r4 = GPR(4);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float inv_len = (float)(rPS0(0) / sqrt(x*x + y*y + z*z));
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r4, x);
	FW(r4 + 4, y);
	FW(r4 + 8, z);
	NPC = LR;
}

// Internal square root function in the crazy math lib. Acts a bit odd, just read it. It's not a bug :p
void SMB_sqrt_internal()
{
	double f = sqrt(rPS0(1));
	rPS0(0) = rPS0(1);
	rPS1(0) = rPS0(1);
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

// Internal inverse square root function in the crazy math lib. 
void SMB_rsqrt_internal()
{
	double f = 1.0 / sqrt(rPS0(1));
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

void SMB_atan2()
{
	// in: f1 = x, f2 = y
	// out: r3 = angle
	double angle = atan2(rPS0(1), rPS0(2));
	int angle_fixpt = (int)(angle / 3.14159 * 32767);
	if (angle_fixpt < -32767) angle_fixpt = -32767;
	if (angle_fixpt > 32767) angle_fixpt = 32767;
	GPR(3) = angle_fixpt;
	NPC = LR;
}


// F-zero math lib range: 8006d044 - 8006f770

void FZero_kill_infinites()
{
	// TODO: Kill infinites in FPR(1)

	NPC = LR;
}

void FZ_sqrt() {
	u32 r3 = GPR(3);
	double x = rPS0(0);
	x = sqrt(x);
	FW(r3, (float)x);
	rPS0(0) = x;
	NPC = LR;
}

// Internal square root function in the crazy math lib. Acts a bit odd, just read it. It's not a bug :p
void FZ_sqrt_internal()
{
	double f = sqrt(rPS0(1));
	rPS0(0) = rPS0(1);
	rPS1(0) = rPS0(1);
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

// Internal inverse square root function in the crazy math lib. 
void FZ_rsqrt_internal()
{
	double f = 1.0 / sqrt(rPS0(1));
	rPS0(1) = f;
	rPS1(1) = f;
	NPC = LR;
}

void FZero_evil_vec_normalize()
{
	u32 r3 = GPR(3);
	float x = F(r3);
	float y = F(r3 + 4);
	float z = F(r3 + 8);
	float sq_len = x*x + y*y + z*z;
	float inv_len = 1.0f / sqrtf(sq_len);
	x *= inv_len;
	y *= inv_len;
	z *= inv_len;
	FW(r3, x);
	FW(r3 + 4, y);
	FW(r3 + 8, z);
	rPS0(1) = inv_len * sq_len;  // len
	rPS1(1) = inv_len * sq_len;  // len
	NPC = LR;

	/*
.evil_vec_something

(f6, f7, f8) <- [r3]
f1 = f6 * f6
f1 += f7 * f7
f1 += f8 * f8
f2 = mystery
f4 = f2 * f1
f3 = f2 + f2
f1 = 1/f0

f6 *= f1
f7 *= f1
f8 *= f1

8006d668: lis	r5, 0xE000
8006d684: lfs	f2, 0x01A0 (r5)
8006d69c: fmr	f0,f2
8006d6a0: fmuls	f4,f2,f1
8006d6a4: fadds	f3,f2,f2
8006d6a8: frsqrte	f1,f0,f1
8006d6ac: fadds	f3,f3,f2
8006d6b0: fmuls	f5,f1,f1
8006d6b4: fnmsubs	f5,f5,f4,f3
8006d6b8: fmuls	f1,f1,f5
8006d6bc: fmuls	f5,f1,f1
8006d6c0: fnmsubs	f5,f5,f4,f3
8006d6c4: fmuls	f1,f1,f5
8006d6c8: fmuls	f6,f6,f1
8006d6cc: stfs	f6, 0 (r3)
8006d6d0: fmuls	f7,f7,f1
8006d6d4: stfs	f7, 0x0004 (r3)
8006d6d8: fmuls	f8,f8,f1
8006d6dc: stfs	f8, 0x0008 (r3)
8006d6e0: fmuls	f1,f1,f0
8006d6e4: blr	
*/
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
		if (db.Load((File::GetSysDirectory() + TOTALDB).c_str()))
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
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(SConfig::GetInstance().m_LastFilename.c_str());
	DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

	if (dol.length() > 1 && dol.compare(0, 1, "/") == 0)
		dol = dol.substr(1);

	u32 fileSize = (u32) pFileSystem->GetFileSize(dol.c_str());
	u8* dolFile = new u8[fileSize];
	if (fileSize > 0)
	{
		pFileSystem->ReadFile(dol.c_str(), dolFile, fileSize);
		ExecuteDOL(dolFile, fileSize);
	}
	delete[] dolFile;
}

void LoadBootDOLFromDisc()
{
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(SConfig::GetInstance().m_LastFilename.c_str());
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
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(SConfig::GetInstance().m_LastFilename.c_str());
	DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

	std::string dolFile;

	if (dol.length() > 1 && dol.compare(0, 1, "/") == 0)
		dolFile = dol.substr(1);
	else
		dolFile = dol;

	return (u32)pFileSystem->GetFileSize(dolFile.c_str());
}

void gc_memmove()
{
	u32 dest = GPR(3);
	u32 src = GPR(4);
	u32 count = GPR(5);
	memmove((u8*)(Memory::base + dest), (u8*)(Memory::base + src), count);
	NPC = LR;
}

void gc_memcpy()
{
	u32 dest = GPR(3);
	u32 src = GPR(4);
	u32 count = GPR(5);
	memcpy((u8*)(Memory::base + dest), (u8*)(Memory::base + src), count);
	NPC = LR;
}

void gc_memset()
{
	u32 dest = GPR(3);
	u32 ch = GPR(4);
	u32 count = GPR(5);
	memset((u8*)(Memory::base + dest), ch, count);
	NPC = LR;
}

void gc_memcmp()
{
	u32 dest = GPR(3);
	u32 src = GPR(4);
	u32 count = GPR(5);
	GPR(3) = memcmp((u8*)(Memory::base + dest), (u8*)(Memory::base + src), count);
	NPC = LR;
}

void div2i()
{
	s64 num = (s64)(GPR(3)) << 32 | GPR(4);
	s64 den = (s64)(GPR(5)) << 32 | GPR(6);
	s64 quo = num / den;
	GPR(3) = quo >> 32;
	GPR(4) = quo & 0xffffffff;
	NPC = LR;
}

void div2u()
{
	u64 num = (u64)(GPR(3)) << 32 | GPR(4);
	u64 den = (u64)(GPR(5)) << 32 | GPR(6);
	u64 quo = num / den;
	GPR(3) = quo >> 32;
	GPR(4) = quo & 0xffffffff;
	NPC = LR;
}

void OSGetResetCode()
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

u16 GetIOSVersion()
{
	return Memory::Read_U16(0x00003140);
}

void OSBootDol()
{
	if (GetIOSVersion() >= 30)
	{
		bootType = GPR(4);

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

}
