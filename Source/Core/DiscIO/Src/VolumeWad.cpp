// Copyright (C) 2003 Dolphin Project.

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

#include <algorithm>
#include <math.h>

#include "VolumeWad.h"
#include "StringUtil.h"
#include "MathUtil.h"

#define ALIGN_40(x) ROUND_UP(Common::swap32(x), 0x40)

namespace DiscIO
{
CVolumeWAD::CVolumeWAD(IBlobReader* _pReader)
	: m_pReader(_pReader), OpeningBnrOffset(0), hdr_size(0), cert_size(0), tick_size(0), tmd_size(0), data_size(0)
{
	Read(0x00, 4, (u8*)&hdr_size);
	Read(0x08, 4, (u8*)&cert_size);
	Read(0x10, 4, (u8*)&tick_size);
	Read(0x14, 4, (u8*)&tmd_size);
	Read(0x18, 4, (u8*)&data_size);

	u32 TmdOffset = ALIGN_40(hdr_size) + ALIGN_40(cert_size) + ALIGN_40(tick_size);
	OpeningBnrOffset = TmdOffset + ALIGN_40(tmd_size) + ALIGN_40(data_size);
	// read the last digit of the titleID in the ticket
	Read(TmdOffset + 0x0193, 1, &m_Country);
	if (m_Country == 2) // SYSMENU
	{
		u16 titlever = 0;
		Read(TmdOffset + 0x01dc, 2, (u8*)&titlever);
		m_Country = GetSysMenuRegion(Common::swap16(titlever));
	}
}

CVolumeWAD::~CVolumeWAD()
{
	delete m_pReader;
}

bool CVolumeWAD::Read(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	if (m_pReader == NULL)
		return false;

	return m_pReader->Read(_Offset, _Length, _pBuffer);
}

IVolume::ECountry CVolumeWAD::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	return CountrySwitch(m_Country);
}

std::string CVolumeWAD::GetUniqueID() const
{
	std::string temp = GetMakerID();
	u32 Offset = ALIGN_40(hdr_size) + ALIGN_40(cert_size);

	char GameCode[8];
	if(!Read(Offset + 0x01E0, 4, (u8*)GameCode))
		return "0";

	GameCode[4] = temp.at(0);
	GameCode[5] = temp.at(1);
	GameCode[6] = 0;

	return GameCode;
}

std::string CVolumeWAD::GetMakerID() const
{
	u32 Offset = ALIGN_40(hdr_size) + ALIGN_40(cert_size) + ALIGN_40(tick_size);

	char temp[3] = {1};
	// Some weird channels use 0x0000 in place of the MakerID, so we need a check there
	if (!Read(0x198 + Offset, 2, (u8*)temp) || temp[0] == 0 || temp[1] == 0)
		return "00";

	temp[2] = 0;

	return temp;
}

bool CVolumeWAD::GetTitleID(u8* _pBuffer) const
{
	u32 Offset = ALIGN_40(hdr_size) + ALIGN_40(cert_size);

	if(!Read(Offset + 0x01DC, 8, _pBuffer))
		return false;

	return true;
}

std::vector<std::string> CVolumeWAD::GetNames() const
{
	std::vector<std::string> names;
	return names;

	u32 footer_size;
	if (!Read(0x1C, 4, (u8*)&footer_size))
	{
		return names;
	}
	
	footer_size = Common::swap32(footer_size);
	
	//Japanese, English, German, French, Spanish, Italian, Dutch, unknown, unknown, Korean
	for (int i = 0; i != 10; ++i)
	{
		static const u32 string_length = 42;
		static const u32 bytes_length = string_length * sizeof(u16);
		
		u16 temp[string_length];

		if (footer_size < 0xF1 || !Read(0x9C + (i * bytes_length) + OpeningBnrOffset, bytes_length, (u8*)&temp))
		{
			names.push_back("");
			ERROR_LOG(COMMON, "added empty WAD name");
		}
		else
		{
			std::wstring out_temp;
			out_temp.resize(string_length);
			std::transform(temp, temp + out_temp.size(), out_temp.begin(), (u16(&)(u16))Common::swap16);
			out_temp.erase(std::find(out_temp.begin(), out_temp.end(), 0x00), out_temp.end());
			
			names.push_back(UTF16ToUTF8(out_temp));
			ERROR_LOG(COMMON, "decoded WAD name: %s", names.back().c_str());
		}
	}

	return names;
}

u64 CVolumeWAD::GetSize() const
{
	if (m_pReader)
		return (size_t)m_pReader->GetDataSize();
	else
		return 0;
}

} // namespace
