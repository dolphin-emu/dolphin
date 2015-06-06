// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
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
	: m_pReader(_pReader), m_offset(0), m_tmd_offset(0), m_opening_bnr_offset(0),
	m_hdr_size(0), m_cert_size(0), m_tick_size(0), m_tmd_size(0), m_data_size(0)
{
	// Source: http://wiibrew.org/wiki/WAD_files
	Read(0x00, 4, (u8*)&m_hdr_size);
	Read(0x08, 4, (u8*)&m_cert_size);
	Read(0x10, 4, (u8*)&m_tick_size);
	Read(0x14, 4, (u8*)&m_tmd_size);
	Read(0x18, 4, (u8*)&m_data_size);

	m_offset = ALIGN_40(m_hdr_size) + ALIGN_40(m_cert_size);
	m_tmd_offset = ALIGN_40(m_hdr_size) + ALIGN_40(m_cert_size) + ALIGN_40(m_tick_size);
	m_opening_bnr_offset = m_tmd_offset + ALIGN_40(m_tmd_size) + ALIGN_40(m_data_size);
}

CVolumeWAD::~CVolumeWAD()
{
}

bool CVolumeWAD::Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
	if (decrypt)
		PanicAlertT("Tried to decrypt data from a non-Wii volume");

	if (m_pReader == nullptr)
		return false;

	return m_pReader->Read(_Offset, _Length, _pBuffer);
}

IVolume::ECountry CVolumeWAD::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	// read the last digit of the titleID in the ticket
	u8 country_code;
	Read(m_tmd_offset + 0x0193, 1, &country_code);

	if (country_code == 2) // SYSMENU
	{
		u16 title_version = 0;
		Read(m_tmd_offset + 0x01dc, 2, (u8*)&title_version);
		country_code = GetSysMenuRegion(Common::swap16(title_version));
	}

	return CountrySwitch(country_code);
}

std::string CVolumeWAD::GetUniqueID() const
{
	std::string temp = GetMakerID();

	char GameCode[8];
	if (!Read(m_offset + 0x01E0, 4, (u8*)GameCode))
		return "0";

	GameCode[4] = temp.at(0);
	GameCode[5] = temp.at(1);
	GameCode[6] = 0;

	return GameCode;
}

std::string CVolumeWAD::GetMakerID() const
{
	char temp[3] = {1};
	// Some weird channels use 0x0000 in place of the MakerID, so we need a check there
	if (!Read(0x198 + m_tmd_offset, 2, (u8*)temp) || temp[0] == 0 || temp[1] == 0)
		return "00";

	temp[2] = 0;

	return temp;
}

bool CVolumeWAD::GetTitleID(u8* _pBuffer) const
{
	if (!Read(m_offset + 0x01DC, 8, _pBuffer))
		return false;

	return true;
}

u16 CVolumeWAD::GetRevision() const
{
	u16 revision;
	if (!m_pReader->Read(m_tmd_offset + 0x1dc, 2, (u8*)&revision))
		return 0;

	return Common::swap16(revision);
}

IVolume::EPlatform CVolumeWAD::GetVolumeType() const
{
	return WII_WAD;
}

std::map<IVolume::ELanguage, std::string> CVolumeWAD::GetNames() const
{
	std::vector<u8> name_data(NAMES_TOTAL_BYTES);
	if (!Read(m_opening_bnr_offset + 0x9C, NAMES_TOTAL_BYTES, name_data.data()))
		return std::map<IVolume::ELanguage, std::string>();
	return ReadWiiNames(name_data);
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
