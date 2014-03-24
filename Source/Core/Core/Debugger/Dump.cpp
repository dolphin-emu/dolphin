// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <string>

#include "Common/Common.h"
#include "Common/FileUtil.h"

#include "Core/Debugger/Dump.h"

CDump::CDump(const std::string& filename) :
	m_pData(nullptr)
{
	File::IOFile pStream(filename, "rb");
	if (pStream)
	{
		m_size = (size_t)pStream.GetSize();

		m_pData = new u8[m_size];

		pStream.ReadArray(m_pData, m_size);
	}
}

CDump::~CDump(void)
{
	if (m_pData != nullptr)
	{
		delete[] m_pData;
		m_pData = nullptr;
	}
}

int CDump::GetNumberOfSteps(void)
{
	return (int)(m_size / STRUCTUR_SIZE);
}

u32 CDump::GetGPR(int step, int gpr)
{
	u32 offset = step * STRUCTUR_SIZE;

	if (offset >= m_size)
		return -1;

	return Read32(offset + OFFSET_GPR + (gpr * 4));
}

u32 CDump::GetPC(int step)
{
	u32 offset = step * STRUCTUR_SIZE;

	if (offset >= m_size)
		return -1;

	return Read32(offset + OFFSET_PC);
}

u32 CDump::Read32(u32 pos)
{
	u32 result = (m_pData[pos+0] << 24) |
	             (m_pData[pos+1] << 16) |
	             (m_pData[pos+2] << 8)  |
	             (m_pData[pos+3] << 0);

	return result;
}
