// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Core/HW/EXI_Device.h"

class PointerWrap;

class CEXIIPL : public IEXIDevice
{
public:
	CEXIIPL();
	virtual ~CEXIIPL();

	void SetCS(int _iCS) override;
	bool IsPresent() const override;
	void DoState(PointerWrap &p) override;

	static u32 GetGCTime();
	static u64 NetPlay_GetGCTime();

	static void Descrambler(u8* data, u32 size);

private:
	enum
	{
		ROM_SIZE = 1024*1024*2,
		ROM_MASK = (ROM_SIZE - 1)
	};

	enum
	{
		REGION_RTC       = 0x200000,
		REGION_SRAM      = 0x200001,
		REGION_UART      = 0x200100,
		REGION_UART_UNK  = 0x200103,
		REGION_BARNACLE  = 0x200113,
		REGION_WRTC0     = 0x210000,
		REGION_WRTC1     = 0x210001,
		REGION_WRTC2     = 0x210008,
		REGION_EUART_UNK = 0x300000,
		REGION_EUART     = 0x300001
	};

	// Region
	bool m_bNTSC;

	//! IPL
	u8* m_pIPL;

	// STATE_TO_SAVE
	//! RealTimeClock
	u8 m_RTC[4];

	//! Helper
	u32 m_uPosition;
	u32 m_uAddress;
	u32 m_uRWOffset;

	std::string m_buffer;
	bool m_FontsLoaded;

	void UpdateRTC();

	void TransferByte(u8& _uByte) override;
	bool IsWriteCommand() const { return !!(m_uAddress & (1 << 31)); }
	u32 CommandRegion() const { return (m_uAddress & ~(1 << 31)) >> 8; }

	void LoadFileToIPL(const std::string& filename, u32 offset);
	void LoadFontFile(const std::string& filename, u32 offset);
	std::string FindIPLDump(const std::string& path_prefix);
};
