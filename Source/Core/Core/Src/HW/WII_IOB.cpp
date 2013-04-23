// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "ChunkFile.h"
#include "WII_IOB.h"

namespace WII_IOBridge
{

void Read8(u8& _rReturnValue, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void Read16(u16& _rReturnValue, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void Read32(u32& _rReturnValue, const u32 _Address)
{
	switch (_Address & 0xFFFF)
	{
	// NAND Loader ... no idea
	case 0x018:
		ERROR_LOG(WII_IOB, "IOP: Read32 from 0x18 = 0x%08x (NANDLoader)",  _Address);
		break;
	// WiiMenu... no idea
	case 0x24:
		ERROR_LOG(WII_IOB, "IOP: Read32 from 0x18 = 0x%08x (WiiMenu)",  _Address);
		break;


	case 0xc0:					// __VISendI2CData
		_rReturnValue = 0;
		INFO_LOG(WII_IOB, "IOP: Read32 from 0xc0 = 0x%08x (__VISendI2CData)", _rReturnValue);
		break;

	case 0xc4:					// __VISendI2CData
		_rReturnValue = 0;
		INFO_LOG(WII_IOB, "IOP: Read32 from 0xc4 = 0x%08x (__VISendI2CData)", _rReturnValue);
		break;

	case 0xc8:					// __VISendI2CData
		_rReturnValue = 0;
		INFO_LOG(WII_IOB, "IOP: Read32 from 0xc8 = 0x%08x (__VISendI2CData)", _rReturnValue);
		break;

	case 0x180:					// __AIClockInit
		_rReturnValue = 0;
		INFO_LOG(WII_IOB, "IOP: Read32 from 0x180 = 0x%08x (__AIClockInit)", _rReturnValue);
		return;

	case 0x1CC:					// __AIClockInit
		_rReturnValue = 0;
		INFO_LOG(WII_IOB, "IOP: Read32 from 0x1CC = 0x%08x (__AIClockInit)", _rReturnValue);
		return;

	case 0x1D0:					// __AIClockInit
		_rReturnValue = 0;
		INFO_LOG(WII_IOB, "IOP: Read32 from 0x1D0 = 0x%08x (__AIClockInit)", _rReturnValue);
		return;

	default:
		_dbg_assert_msg_(WII_IOB, 0, "IOP: Read32 from 0x%08x", _Address);
		break;
	}
}

void Read64(u64& _rReturnValue, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void Write8(const u8 _Value, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void Write16(const u16 _Value, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void Write32(const u32 _Value, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
	// NANDLoader ... no idea
	case 0x18:
		ERROR_LOG(WII_IOB, "IOP: Write32 0x%08x to 0x%08x (NANDLoader)", _Value, _Address);
		break;
	// WiiMenu... no idea
	case 0x24:
		ERROR_LOG(WII_IOB, "IOP: Write32 0x%08x to 0x%08x (WiiMenu)", _Value, _Address);
		break;

	case 0xc0:					// __VISendI2CData
		INFO_LOG(WII_IOB, "IOP: Write32 to 0xc0 = 0x%08x (__VISendI2CData)", _Value);
		break;

	case 0xc4:					// __VISendI2CData
		INFO_LOG(WII_IOB, "IOP: Write32 to 0xc4 = 0x%08x (__VISendI2CData)", _Value);
		break;

	case 0xc8:					// __VISendI2CData
		INFO_LOG(WII_IOB, "IOP: Write32 to 0xc8 = 0x%08x (__VISendI2CData)", _Value);
		break;

	case 0x180:					// __AIClockInit
		INFO_LOG(WII_IOB, "IOP: Write32 to 0x180 = 0x%08x (__AIClockInit)", _Value);
		return;

	case 0x1CC:					// __AIClockInit
		INFO_LOG(WII_IOB, "IOP: Write32 to 0x1D0 = 0x%08x (__AIClockInit)", _Value);
		return;

	case 0x1D0:					// __AIClockInit
		INFO_LOG(WII_IOB, "IOP: Write32 to 0x1D0 = 0x%08x (__AIClockInit)", _Value);
		return;

	default:
		_dbg_assert_msg_(WII_IOB, 0, "IOP: Write32 to 0x%08x", _Address);
		break;
	}
}

void Write64(const u64 _Value, const u32 _Address)
{
	//switch(_Address)
	//{
	//default:
		_dbg_assert_msg_(WII_IOB, 0, "IOP: Write32 to 0x%08x", _Address);
		//break;
	//}
}

} // end of namespace AudioInterfac
