// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common/ColorUtil.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"

namespace DiscIO
{
CVolumeGC::CVolumeGC(std::unique_ptr<IBlobReader> reader)
	: m_pReader(std::move(reader))
{
}

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

	char ID[6];

	if (!Read(0, sizeof(ID), reinterpret_cast<u8*>(ID)))
	{
		PanicAlertT("Failed to read unique ID from disc image");
		return NO_UID;
	}

	return DecodeString(ID);
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

	char makerID[2];
	if (!Read(0x4, 0x2, (u8*)&makerID))
		return std::string();

	return DecodeString(makerID);
}

u16 CVolumeGC::GetRevision() const
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

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetShortNames() const
{
	if (LoadBannerFile())
        return m_short_names;
	else
		return std::map<IVolume::ELanguage, std::string>();
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetLongNames() const
{
	if (LoadBannerFile())
		return m_long_names;
	else
		return std::map<IVolume::ELanguage, std::string>();
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetShortMakers() const
{
	if (LoadBannerFile())
		return m_short_makers;
	else
		return std::map<IVolume::ELanguage, std::string>();
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetLongMakers() const
{
	if (LoadBannerFile())
		return m_long_makers;
	else
		return std::map<IVolume::ELanguage, std::string>();
}

std::map<IVolume::ELanguage, std::string> CVolumeGC::GetDescriptions() const
{
	if (LoadBannerFile())
		return m_descriptions;
	else
		return std::map<IVolume::ELanguage, std::string>();
}

std::vector<u32> CVolumeGC::GetBanner(int* width, int* height) const
{
	if (LoadBannerFile())
	{
		*width = GC_BANNER_WIDTH;
		*height = GC_BANNER_HEIGHT;
		return image_buffer;
	}
	{
		*width = 0;
		*height = 0;
		return std::vector<u32>();
	}

}

u64 CVolumeGC::GetFSTSize() const
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

	return DecodeString(date);
}

BlobType CVolumeGC::GetBlobType() const
{
	return m_pReader ? m_pReader->GetBlobType() : BlobType::PLAIN;
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

u8 CVolumeGC::GetDiscNumber() const
{
	u8 disc_number;
	Read(6, 1, &disc_number);
	return disc_number;
}

IVolume::EPlatform CVolumeGC::GetVolumeType() const
{
	return GAMECUBE_DISC;
}

// Returns true if the loaded banner file is valid,
// regardless of whether it was loaded by the current call
bool CVolumeGC::LoadBannerFile() const
{
	// The methods ReadMultiLanguageStrings, GetCompany and GetBanner
	// need to access the opening.bnr file. These methods are
	// usually called one after another. The file is cached in
	// RAM to avoid reading it from the disc several times, but
	// if none of these methods are called, the file is never loaded.

	// If opening.bnr has been loaded already, return immediately
	if (m_banner_file_type != BANNER_NOT_LOADED)
		return m_banner_file_type != BANNER_INVALID;
	GCBanner banner_file;

	std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this));
	size_t file_size = (size_t)file_system->GetFileSize("opening.bnr");
	if (file_size == BNR1_SIZE || file_size == BNR2_SIZE)
	{
		file_system->ReadFile("opening.bnr", reinterpret_cast<u8*>(&banner_file), file_size);

		if (file_size == BNR1_SIZE && banner_file.id == 0x31524e42)      // "BNR1"
		{
			m_banner_file_type = BANNER_BNR1;
		}
		else if (file_size == BNR2_SIZE && banner_file.id == 0x32524e42) // "BNR2"
		{
			m_banner_file_type = BANNER_BNR2;
		}
		else
		{
			m_banner_file_type = BANNER_INVALID;
			WARN_LOG(DISCIO, "Invalid opening.bnr. Type: %0x Size: %0zx", banner_file.id, file_size);
		}
	}
	else
	{
		m_banner_file_type = BANNER_INVALID;
		WARN_LOG(DISCIO, "Invalid opening.bnr. Size: %0zx", file_size);
	}

	ExtractBannerInformation(banner_file);

	return m_banner_file_type != BANNER_INVALID;
}

void CVolumeGC::ExtractBannerInformation(GCBanner banner_file) const
{
	u32 number_of_languages = 0;
	ELanguage start_language = LANGUAGE_UNKNOWN;
	bool is_japanese = GetCountry() == ECountry::COUNTRY_JAPAN;

	switch (m_banner_file_type)
	{
	case BANNER_BNR1:	// NTSC
		number_of_languages = 1;
		start_language = is_japanese ? ELanguage::LANGUAGE_JAPANESE : ELanguage::LANGUAGE_ENGLISH;
		break;

	case BANNER_BNR2:	// PAL
		number_of_languages = 6;
		start_language = ELanguage::LANGUAGE_ENGLISH;
		break;

	// Shouldn't happen
	case BANNER_INVALID:
	case BANNER_NOT_LOADED:
		break;
	}
	image_buffer = std::vector<u32>(GC_BANNER_WIDTH * GC_BANNER_HEIGHT);
	ColorUtil::decode5A3image(image_buffer.data(), banner_file.image, GC_BANNER_WIDTH, GC_BANNER_HEIGHT);

	for (u32 i = 0; i < number_of_languages; ++i)
	{
		const GCBannerInformation& info = banner_file.information[i];
		ELanguage language = static_cast<ELanguage>(start_language + i);

		std::string description = DecodeString(info.description);
		if (!description.empty())
			m_descriptions[language] = description;

		std::string short_name = DecodeString(info.short_name);
		if (!short_name.empty())
			m_short_names[language] = short_name;

		std::string long_name = DecodeString(info.long_name);
		if (!long_name.empty())
			m_long_names[language] = long_name;

		std::string short_maker = DecodeString(info.short_maker);
		if (!short_maker.empty())
			m_short_makers[language] = short_maker;

		std::string long_maker = DecodeString(info.long_maker);
		if (!long_maker.empty())
			m_long_makers[language] = long_maker;
	}
}

} // namespace
