// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
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

bool CVolumeGC::Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
	if (decrypt)
		PanicAlertT("Tried to decrypt data from a non-Wii volume");

	if (m_pReader == nullptr)
		return false;

	FileMon::FindFilename(_Offset);

	return m_pReader->Read(_Offset, _Length, _pBuffer);
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

IVolume::ECountry CVolumeGC::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 country_code;
	m_pReader->Read(3, 1, &country_code);

	return CountrySwitch(country_code);
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

	u8 revision;
	if (!Read(7, 1, &revision))
		return 0;

	return revision;
}

std::string CVolumeGC::GetInternalName() const
{
	char name[0x60];
	if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)name))
		return DecodeString(name);
	else
		return "";
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetNames() const
{
	std::map<IVolume::ELanguage, std::string> names;

	if (!LoadBannerFile())
		return names;

	u32 name_count = 0;
	IVolume::ELanguage language;
	bool is_japanese = GetCountry() == IVolume::ECountry::COUNTRY_JAPAN;

	switch (m_banner_file_type)
	{
	case BANNER_BNR1:
		name_count = 1;
		language = is_japanese ? IVolume::ELanguage::LANGUAGE_JAPANESE : IVolume::ELanguage::LANGUAGE_ENGLISH;
		break;

	case BANNER_BNR2:
		name_count = 6;
		language = IVolume::ELanguage::LANGUAGE_ENGLISH;
		break;

	case BANNER_INVALID:
	case BANNER_NOT_LOADED:
		break;
	}

	auto const banner = reinterpret_cast<const GCBanner*>(m_banner_file.data());

	for (u32 i = 0; i < name_count; ++i)
	{
		auto& comment = banner->comment[i];
		std::string name = DecodeString(comment.longTitle);

		if (name.empty())
			name = DecodeString(comment.shortTitle);

		if (!name.empty())
			names[(IVolume::ELanguage)(language + i)] = name;
	}

	return names;
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetDescriptions() const
{
	std::map<IVolume::ELanguage, std::string> descriptions;

	if (!LoadBannerFile())
		return descriptions;

	u32 desc_count = 0;
	IVolume::ELanguage language;
	bool is_japanese = GetCountry() == IVolume::ECountry::COUNTRY_JAPAN;

	switch (m_banner_file_type)
	{
	case BANNER_BNR1:
		desc_count = 1;
		language = is_japanese ? IVolume::ELanguage::LANGUAGE_JAPANESE : IVolume::ELanguage::LANGUAGE_ENGLISH;
		break;

	case BANNER_BNR2:
		language = IVolume::ELanguage::LANGUAGE_ENGLISH;
		desc_count = 6;
		break;

	case BANNER_INVALID:
	case BANNER_NOT_LOADED:
		break;
	}

	auto banner = reinterpret_cast<const GCBanner*>(m_banner_file.data());

	for (u32 i = 0; i < desc_count; ++i)
	{
		auto& data = banner->comment[i].comment;
		std::string description = DecodeString(data);

		if (!description.empty())
			descriptions[(IVolume::ELanguage)(language + i)] = description;
	}

	return descriptions;
}

std::string CVolumeGC::GetCompany() const
{
	if (!LoadBannerFile())
		return "";

	auto const pBanner = (GCBanner*)m_banner_file.data();
	std::string company = DecodeString(pBanner->comment[0].longMaker);

	if (company.empty())
		company = DecodeString(pBanner->comment[0].shortMaker);

	return company;
}

std::vector<u32> CVolumeGC::GetBanner(int* width, int* height) const
{
	if (!LoadBannerFile())
	{
		*width = 0;
		*height = 0;
		return std::vector<u32>();
	}

	std::vector<u32> image_buffer(GC_BANNER_WIDTH * GC_BANNER_HEIGHT);
	auto const pBanner = (GCBanner*)m_banner_file.data();
	ColorUtil::decode5A3image(image_buffer.data(), pBanner->image, GC_BANNER_WIDTH, GC_BANNER_HEIGHT);
	*width = GC_BANNER_WIDTH;
	*height = GC_BANNER_HEIGHT;
	return image_buffer;
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
	u8 disc_two_check;
	Read(6, 1, &disc_two_check);
	return (disc_two_check == 1);
}

bool CVolumeGC::LoadBannerFile() const
{
	// The methods GetNames, GetDescriptions, GetCompany and GetBanner
	// all need to access the opening.bnr file. These four methods are
	// typically called after each other, so we store the file in RAM
	// to avoid reading it from the disc several times. However,
	// if none of these methods are called, the file is never loaded.

	if (m_banner_file_type != BANNER_NOT_LOADED)
		return m_banner_file_type != BANNER_INVALID;

	std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this));
	size_t file_size = (size_t)file_system->GetFileSize("opening.bnr");
	if (file_size == BNR1_SIZE || file_size == BNR2_SIZE)
	{
		m_banner_file.resize(file_size);
		file_system->ReadFile("opening.bnr", m_banner_file.data(), m_banner_file.size());

		u32 bannerSignature = *(u32*)m_banner_file.data();
		switch (bannerSignature)
		{
		case 0x31524e42:	// "BNR1"
			m_banner_file_type = BANNER_BNR1;
			break;
		case 0x32524e42:	// "BNR2"
			m_banner_file_type = BANNER_BNR2;
			break;
		default:
			m_banner_file_type = BANNER_INVALID;
			WARN_LOG(DISCIO, "Invalid opening.bnr type");
			break;
		}
	}
	else
	{
		m_banner_file_type = BANNER_INVALID;
		WARN_LOG(DISCIO, "Invalid opening.bnr size: %0lx", (unsigned long)file_size);
	}

	return m_banner_file_type != BANNER_INVALID;
}

} // namespace
