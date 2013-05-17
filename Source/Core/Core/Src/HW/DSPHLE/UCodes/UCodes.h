// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _UCODES_H
#define _UCODES_H

#include "Common.h"
#include "ChunkFile.h"
#include "Thread.h"

#include "../DSPHLE.h"
#include "../../Memmap.h"

#define UCODE_ROM                   0x00000000
#define UCODE_INIT_AUDIO_SYSTEM     0x00000001
#define UCODE_NULL                  0xFFFFFFFF

class CMailHandler;

inline bool ExramRead(u32 _uAddress)
{
	if (_uAddress & 0x10000000)
		return true;
	else
		return false;
}

inline u8 HLEMemory_Read_U8(u32 _uAddress)
{
	if (ExramRead(_uAddress))
		return Memory::m_pEXRAM[_uAddress & Memory::EXRAM_MASK];
	else
		return Memory::m_pRAM[_uAddress & Memory::RAM_MASK];
}

inline u16 HLEMemory_Read_U16(u32 _uAddress)
{
	if (ExramRead(_uAddress))
		return Common::swap16(*(u16*)&Memory::m_pEXRAM[_uAddress & Memory::EXRAM_MASK]);
	else
		return Common::swap16(*(u16*)&Memory::m_pRAM[_uAddress & Memory::RAM_MASK]);
}

inline u32 HLEMemory_Read_U32(u32 _uAddress)
{
	if (ExramRead(_uAddress))
		return Common::swap32(*(u32*)&Memory::m_pEXRAM[_uAddress & Memory::EXRAM_MASK]);
	else
		return Common::swap32(*(u32*)&Memory::m_pRAM[_uAddress & Memory::RAM_MASK]);
}

inline void* HLEMemory_Get_Pointer(u32 _uAddress)
{
	if (ExramRead(_uAddress))
		return &Memory::m_pEXRAM[_uAddress & Memory::EXRAM_MASK];
	else
		return &Memory::m_pRAM[_uAddress & Memory::RAM_MASK];
}

class IUCode
{
public:
	IUCode(DSPHLE *dsphle, u32 _crc)
		: m_rMailHandler(dsphle->AccessMailHandler())
		, m_UploadSetupInProgress(false)
		, m_DSPHLE(dsphle)
		, m_CRC(_crc)
		, m_NextUCode()
		, m_NextUCode_steps(0)
		, m_NeedsResumeMail(false)
	{}

	virtual ~IUCode()
	{}

	virtual void HandleMail(u32 _uMail) = 0;

	// Cycles are out of the 81/121mhz the DSP runs at.
	virtual void Update(int cycles) = 0;
	virtual void MixAdd(short* buffer, int size) {}
	virtual u32 GetUpdateMs() = 0;

	virtual void DoState(PointerWrap &p) { DoStateShared(p); }

	static u32 GetCRC(IUCode* pUCode) { return pUCode ? pUCode->m_CRC : UCODE_NULL; }

protected:
	void PrepareBootUCode(u32 mail);

	// Some ucodes (notably zelda) require a resume mail to be
	// sent if they are be started via PrepareBootUCode.
	// The HLE can use this to 
	bool NeedsResumeMail();

	void DoStateShared(PointerWrap &p);

	CMailHandler& m_rMailHandler;
	std::mutex m_csMix;

	enum EDSP_Codes
	{
		DSP_INIT        = 0xDCD10000,
		DSP_RESUME      = 0xDCD10001,
		DSP_YIELD       = 0xDCD10002,
		DSP_DONE        = 0xDCD10003,
		DSP_SYNC        = 0xDCD10004,
		DSP_FRAME_END   = 0xDCD10005,
	};

	// UCode is forwarding mails to PrepareBootUCode
	// UCode only needs to set this to true, IUCode will set to false when done!
	bool m_UploadSetupInProgress;

	// Need a pointer back to DSPHLE to switch ucodes.
	DSPHLE *m_DSPHLE;

	// used for reconstruction when loading saves,
	// and for variations within certain ucodes.
	u32 m_CRC;

private:
	struct SUCode
	{
		u32 mram_dest_addr;
		u16 mram_size;
		u16 mram_dram_addr;
		u32 iram_mram_addr;
		u16 iram_size;
		u16 iram_dest;
		u16 iram_startpc;
		u32 dram_mram_addr;
		u16 dram_size;
		u16 dram_dest;
	};
	SUCode	m_NextUCode;
	int	m_NextUCode_steps;

	bool m_NeedsResumeMail;
};

extern IUCode* UCodeFactory(u32 _CRC, DSPHLE *dsp_hle, bool bWii);

#endif
