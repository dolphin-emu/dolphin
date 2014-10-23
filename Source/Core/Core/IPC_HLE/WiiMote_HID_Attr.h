// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#if 0
struct SAttrib
{
	u16 ID;
	u8* pData;
	u16 size;

	SAttrib(u16 _ID, u8* _Data, u16 _size)
		: ID(_ID)
		, pData(_Data)
		, size(_size)
	{ }
};

typedef std::vector<SAttrib> CAttribTable;

const CAttribTable& GetAttribTable();
#endif

const u8* GetAttribPacket(u32 serviceHandle, u32 cont, u32& _size);
