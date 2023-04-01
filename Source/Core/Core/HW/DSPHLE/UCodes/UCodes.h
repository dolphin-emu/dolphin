// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>

#include "Common/CommonTypes.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/DSPHLE/DSPHLE.h"

#define UCODE_ROM                   0x00000000
#define UCODE_INIT_AUDIO_SYSTEM     0x00000001
#define UCODE_NULL                  0xFFFFFFFF

class CMailHandler;
class PointerWrap;

constexpr bool ExramRead(u32 address)
{
	return (address & 0x10000000) != 0;
}

inline u8 HLEMemory_Read_U8(u32 address)
{
	if (ExramRead(address))
		return Memory::m_pEXRAM[address & Memory::EXRAM_MASK];
	else
		return Memory::m_pRAM[address & Memory::RAM_MASK];
}

inline u16 HLEMemory_Read_U16(u32 address)
{
	u16 value;

	if (ExramRead(address))
		std::memcpy(&value, &Memory::m_pEXRAM[address & Memory::EXRAM_MASK], sizeof(u16));
	else
		std::memcpy(&value, &Memory::m_pRAM[address & Memory::RAM_MASK], sizeof(u16));

	return Common::swap16(value);
}

inline u32 HLEMemory_Read_U32(u32 address)
{
	u32 value;

	if (ExramRead(address))
		std::memcpy(&value, &Memory::m_pEXRAM[address & Memory::EXRAM_MASK], sizeof(u32));
	else
		std::memcpy(&value, &Memory::m_pRAM[address & Memory::RAM_MASK], sizeof(u32));

	return Common::swap32(value);
}

inline void* HLEMemory_Get_Pointer(u32 address)
{
	if (ExramRead(address))
		return &Memory::m_pEXRAM[address & Memory::EXRAM_MASK];
	else
		return &Memory::m_pRAM[address & Memory::RAM_MASK];
}

class UCodeInterface
{
public:
	UCodeInterface(DSPHLE *dsphle, u32 crc)
		: m_mail_handler(dsphle->AccessMailHandler())
		, m_upload_setup_in_progress(false)
		, m_dsphle(dsphle)
		, m_crc(crc)
		, m_next_ucode()
		, m_next_ucode_steps(0)
		, m_needs_resume_mail(false)
	{
	}

	virtual ~UCodeInterface()
	{
	}

	virtual void HandleMail(u32 mail) = 0;
	virtual void Update() = 0;

	virtual void DoState(PointerWrap &p)
	{
		DoStateShared(p);
	}

	static u32 GetCRC(UCodeInterface* ucode)
	{
		return ucode ? ucode->m_crc : UCODE_NULL;
	}

protected:
	void PrepareBootUCode(u32 mail);

	// Some ucodes (notably zelda) require a resume mail to be
	// sent if they are be started via PrepareBootUCode.
	// The HLE can use this to
	bool NeedsResumeMail();

	void DoStateShared(PointerWrap &p);

	CMailHandler& m_mail_handler;

	enum EDSP_Codes
	{
		DSP_INIT      = 0xDCD10000,
		DSP_RESUME    = 0xDCD10001,
		DSP_YIELD     = 0xDCD10002,
		DSP_DONE      = 0xDCD10003,
		DSP_SYNC      = 0xDCD10004,
		DSP_FRAME_END = 0xDCD10005,
	};

	// UCode is forwarding mails to PrepareBootUCode
	// UCode only needs to set this to true, UCodeInterface will set to false when done!
	bool m_upload_setup_in_progress;

	// Need a pointer back to DSPHLE to switch ucodes.
	DSPHLE *m_dsphle;

	// used for reconstruction when loading saves,
	// and for variations within certain ucodes.
	u32 m_crc;

private:
	struct NextUCodeInfo
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
	NextUCodeInfo m_next_ucode;
	int m_next_ucode_steps;

	bool m_needs_resume_mail;
};

UCodeInterface* UCodeFactory(u32 crc, DSPHLE *dsphle, bool wii);
