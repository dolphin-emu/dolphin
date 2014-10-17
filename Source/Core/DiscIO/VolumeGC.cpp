// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"

namespace DiscIO
{
CVolumeGC::CVolumeGC(IBlobReader* _pReader)
	: m_pReader(_pReader)
{}

CVolumeGC::~CVolumeGC()
{
}

bool CVolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	if (m_pReader == nullptr)
		return false;

	FileMon::FindFilename(_Offset);

	return m_pReader->Read(_Offset, _Length, _pBuffer);
}

bool CVolumeGC::RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const
{
	return Read(_Offset, _Length, _pBuffer);
}

std::string CVolumeGC::GetUniqueID() const
{
	static const std::string NO_UID("NO_UID");
	if (m_pReader == nullptr)
		return NO_UID;

	char ID[7];

	if (!Read(0, sizeof(ID), reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read unique ID from disc image");
		return NO_UID;
	}

	ID[6] = '\0';

	return ID;
}

std::string CVolumeGC::GetRevisionSpecificUniqueID() const
{
	return GetUniqueID() + StringFromFormat("r%d", GetRevision());
}

IVolume::ECountry CVolumeGC::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 CountryCode;
	m_pReader->Read(3, 1, &CountryCode);

	return CountrySwitch(CountryCode);
}

std::string CVolumeGC::GetMakerID() const
{
	if (m_pReader == nullptr)
		return std::string();

	char makerID[3];
	if (!Read(0x4, 0x2, (u8*)&makerID))
		return std::string();
	makerID[2] = '\0';

	return makerID;
}

int CVolumeGC::GetRevision() const
{
	if (!m_pReader)
		return 0;

	u8 Revision;
	if (!Read(7, 1, &Revision))
		return 0;

	return Revision;
}

std::vector<std::string> CVolumeGC::GetNames() const
{
	std::vector<std::string> names;

	auto const string_decoder = GetStringDecoder(GetCountry());

	char name[0x60 + 1] = {};
	if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)name))
		names.push_back(string_decoder(name));

	return names;
}

u32 CVolumeGC::GetFSTSize() const
{
	if (m_pReader == nullptr)
		return 0;

	u32 size;
	if (!Read(0x428, 0x4, (u8*)&size))
		return 0;

	return Common::swap32(size);
}

std::string CVolumeGC::GetApploaderDate() const
{
	if (m_pReader == nullptr)
		return std::string();

	char date[16];
	if (!Read(0x2440, 0x10, (u8*)&date))
		return std::string();
	// Should be 0 already, but just in case
	date[10] = '\0';

	return date;
}

u64 CVolumeGC::GetSize() const
{
	if (m_pReader)
		return m_pReader->GetDataSize();
	else
		return 0;
}

u64 CVolumeGC::GetRawSize() const
{
	if (m_pReader)
		return m_pReader->GetRawSize();
	else
		return 0;
}

bool CVolumeGC::IsDiscTwo() const
{
	bool discTwo = false;
	Read(6,1, (u8*) &discTwo);
	return discTwo;
}

CVolumeGC::StringDecoder CVolumeGC::GetStringDecoder(ECountry country)
{
	return (COUNTRY_JAPAN == country || COUNTRY_TAIWAN == country) ?
		SHIFTJISToUTF8 : CP1252ToUTF8;
}

} // namespace
