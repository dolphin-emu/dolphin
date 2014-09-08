// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeWad.h"

#define ALIGN_40(x) ROUND_UP(Common::swap32(x), 0x40)

namespace DiscIO
{
CVolumeWAD::CVolumeWAD(IBlobReader* _pReader)
	: m_pReader(_pReader), m_opening_bnr_offset(0), m_hdr_size(0)
	, m_cert_size(0), m_tick_size(0), m_tmd_size(0), m_data_size(0)
{
	Read(0x00, 4, (u8*)&m_hdr_size);
	Read(0x08, 4, (u8*)&m_cert_size);
	Read(0x10, 4, (u8*)&m_tick_size);
	Read(0x14, 4, (u8*)&m_tmd_size);
	Read(0x18, 4, (u8*)&m_data_size);

	u32 TmdOffset = ALIGN_40(m_hdr_size) + ALIGN_40(m_cert_size) + ALIGN_40(m_tick_size);
	m_opening_bnr_offset = TmdOffset + ALIGN_40(m_tmd_size) + ALIGN_40(m_data_size);
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
}

bool CVolumeWAD::Read(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	if (m_pReader == nullptr)
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
	u32 Offset = ALIGN_40(m_hdr_size) + ALIGN_40(m_cert_size);

	char GameCode[8];
	if (!Read(Offset + 0x01E0, 4, (u8*)GameCode))
		return "0";

	GameCode[4] = temp.at(0);
	GameCode[5] = temp.at(1);
	GameCode[6] = 0;

	return GameCode;
}

std::string CVolumeWAD::GetMakerID() const
{
	u32 Offset = ALIGN_40(m_hdr_size) + ALIGN_40(m_cert_size) + ALIGN_40(m_tick_size);

	char temp[3] = {1};
	// Some weird channels use 0x0000 in place of the MakerID, so we need a check there
	if (!Read(0x198 + Offset, 2, (u8*)temp) || temp[0] == 0 || temp[1] == 0)
		return "00";

	temp[2] = 0;

	return temp;
}

bool CVolumeWAD::GetTitleID(u8* _pBuffer) const
{
	u32 Offset = ALIGN_40(m_hdr_size) + ALIGN_40(m_cert_size);

	if (!Read(Offset + 0x01DC, 8, _pBuffer))
		return false;

	return true;
}

std::vector<std::string> CVolumeWAD::GetNames() const
{
	std::vector<std::string> names;

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

		if (footer_size < 0xF1 || !Read(0x9C + (i * bytes_length) + m_opening_bnr_offset, bytes_length, (u8*)&temp))
		{
			names.push_back("");
		}
		else
		{
			std::wstring out_temp;
			out_temp.resize(string_length);
			std::transform(temp, temp + out_temp.size(), out_temp.begin(), (u16(&)(u16))Common::swap16);
			out_temp.erase(std::find(out_temp.begin(), out_temp.end(), 0x00), out_temp.end());

			names.push_back(UTF16ToUTF8(out_temp));
		}
	}

	return names;
}

u64 CVolumeWAD::GetSize() const
{
	if (m_pReader)
		return m_pReader->GetDataSize();
	else
		return 0;
}

u64 CVolumeWAD::GetRawSize() const
{
	if (m_pReader)
		return m_pReader->GetRawSize();
	else
		return 0;
}

} // namespace
