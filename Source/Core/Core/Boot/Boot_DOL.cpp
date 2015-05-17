// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"

#include "Core/Boot/Boot_DOL.h"
#include "Core/HW/Memmap.h"
#include "Core/PowerPC/PowerPC.h"

CDolLoader::CDolLoader(u8* _pBuffer, u32 _Size)
	: m_isWii(false)
{
	Initialize(_pBuffer, _Size);
}

CDolLoader::CDolLoader(const std::string& filename)
	: m_isWii(false)
{
	const u64 size = File::GetSize(filename);
	u8* const tmpBuffer = new u8[(size_t)size];

	{
	File::IOFile pStream(filename, "rb");
	pStream.ReadBytes(tmpBuffer, (size_t)size);
	}

	Initialize(tmpBuffer, (u32)size);
	delete[] tmpBuffer;
}

CDolLoader::~CDolLoader()
{
	for (auto& sect : text_section)
	{
		delete [] sect;
		sect = nullptr;
	}

	for (auto& sect : data_section)
	{
		delete [] sect;
		sect = nullptr;
	}
}

void CDolLoader::Initialize(u8* _pBuffer, u32 _Size)
{
	memcpy(&m_dolheader, _pBuffer, sizeof(SDolHeader));

	// swap memory
	u32* p = (u32*)&m_dolheader;
	for (size_t i = 0; i < (sizeof(SDolHeader)/sizeof(u32)); i++)
		p[i] = Common::swap32(p[i]);

	for (auto& sect : text_section)
		sect = nullptr;
	for (auto& sect : data_section)
		sect = nullptr;

	u32 HID4_pattern = 0x7c13fba6;
	u32 HID4_mask = 0xfc1fffff;

	for (int i = 0; i < DOL_NUM_TEXT; i++)
	{
		if (m_dolheader.textOffset[i] != 0)
		{
			text_section[i] = new u8[m_dolheader.textSize[i]];
			memcpy(text_section[i], _pBuffer + m_dolheader.textOffset[i], m_dolheader.textSize[i]);
			for (unsigned int j = 0; j < (m_dolheader.textSize[i]/sizeof(u32)); j++)
			{
				u32 word = Common::swap32(((u32*)text_section[i])[j]);
				if ((word & HID4_mask) == HID4_pattern)
				{
					m_isWii = true;
					break;
				}
			}
		}
	}

	for (int i = 0; i < DOL_NUM_DATA; i++)
	{
		if (m_dolheader.dataOffset[i] != 0)
		{
			data_section[i] = new u8[m_dolheader.dataSize[i]];
			memcpy(data_section[i], _pBuffer + m_dolheader.dataOffset[i], m_dolheader.dataSize[i]);
		}
	}
}

void CDolLoader::Load()
{
	// load all text (code) sections
	for (int i = 0; i < DOL_NUM_TEXT; i++)
	{
		if (m_dolheader.textOffset[i] != 0)
		{
			for (u32 num = 0; num < m_dolheader.textSize[i]; num++)
				Memory::Write_U8(text_section[i][num], m_dolheader.textAddress[i] + num);
		}
	}

	// load all data sections
	for (int i = 0; i < DOL_NUM_DATA; i++)
	{
		if (m_dolheader.dataOffset[i] != 0)
		{
			for (u32 num = 0; num < m_dolheader.dataSize[i]; num++)
				Memory::Write_U8(data_section[i][num], m_dolheader.dataAddress[i] + num);
		}
	}
}
