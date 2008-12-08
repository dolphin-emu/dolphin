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

#include "Common.h"
#include "ChunkFile.h"
#include "WII_IOB.h"

namespace WII_IOBridge
{

void HWCALL Read8(u8& _rReturnValue, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void HWCALL Read16(u16& _rReturnValue, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void HWCALL Read32(u32& _rReturnValue, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
	case 0xc0:					// __VISendI2CData		
		_rReturnValue = 0;
		LOGV(WII_IOB, 2, "IOP: Read32 from 0xc0 = 0x%08x (__VISendI2CData)", _rReturnValue);
		break;

	case 0xc4:					// __VISendI2CData		
		_rReturnValue = 0;
		LOGV(WII_IOB, 2, "IOP: Read32 from 0xc4 = 0x%08x (__VISendI2CData)", _rReturnValue);
		break;

	case 0xc8:					// __VISendI2CData		
		_rReturnValue = 0;
		LOGV(WII_IOB, 2, "IOP: Read32 from 0xc8 = 0x%08x (__VISendI2CData)", _rReturnValue);
		break;

	case 0x180:					// __AIClockInit		
		_rReturnValue = 0;
		LOG(WII_IOB, "IOP: Read32 from 0x180 = 0x%08x (__AIClockInit)", _rReturnValue);
		return;

	case 0x1CC:					// __AIClockInit		
		_rReturnValue = 0;
		LOG(WII_IOB, "IOP: Read32 from 0x1CC = 0x%08x (__AIClockInit)", _rReturnValue);
		return;

	case 0x1D0:					// __AIClockInit		
		_rReturnValue = 0;
		LOG(WII_IOB, "IOP: Read32 from 0x1D0 = 0x%08x (__AIClockInit)", _rReturnValue);
		return;

	default:
		_dbg_assert_msg_(WII_IOB, 0, "IOP: Read32 from 0x%08x", _Address);
		break;
	}	
}

void HWCALL Read64(u64& _rReturnValue, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void HWCALL Write8(const u8 _Value, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void HWCALL Write16(const u16 _Value, const u32 _Address)
{
	_dbg_assert_(WII_IOB, 0);
}

void HWCALL Write32(const u32 _Value, const u32 _Address)
{
	switch(_Address & 0xFFFF)
	{
	case 0xc0:					// __VISendI2CData		
		LOGV(WII_IOB, 2, "IOP: Write32 to 0xc0 = 0x%08x (__VISendI2CData)", _Value);
		break;

	case 0xc4:					// __VISendI2CData		
		LOGV(WII_IOB, 2, "IOP: Write32 to 0xc4 = 0x%08x (__VISendI2CData)", _Value);
		break;

	case 0xc8:					// __VISendI2CData		
		LOGV(WII_IOB, 2, "IOP: Write32 to 0xc8 = 0x%08x (__VISendI2CData)", _Value);
		break;

	case 0x180:					// __AIClockInit		
		LOG(WII_IOB, "IOP: Write32 to 0x180 = 0x%08x (__AIClockInit)", _Value);
		return;

	case 0x1CC:					// __AIClockInit		
		LOG(WII_IOB, "IOP: Write32 to 0x1D0 = 0x%08x (__AIClockInit)", _Value);
		return;

	case 0x1D0:					// __AIClockInit		
		LOG(WII_IOB, "IOP: Write32 to 0x1D0 = 0x%08x (__AIClockInit)", _Value);
		return;

	default:
		_dbg_assert_msg_(WII_IOB, 0, "IOP: Write32 to 0x%08x", _Address);
		break;
	}	
}

void HWCALL Write64(const u64 _Value, const u32 _Address)
{
	//switch(_Address)
	//{
	//default:
		_dbg_assert_msg_(WII_IOB, 0, "IOP: Write32 to 0x%08x", _Address);
		//break;
	//}
}

} // end of namespace AudioInterfac
