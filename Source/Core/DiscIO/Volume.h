// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
class IVolume
{
public:
	// Increment CACHE_REVISION if the enums below are modified (ISOFile.cpp & GameFile.cpp)
	enum EPlatform
	{
		GAMECUBE_DISC = 0,
		WII_DISC,
		WII_WAD,
		ELF_DOL,
		NUMBER_OF_PLATFORMS
	};

	enum ECountry
	{
		COUNTRY_EUROPE = 0,
		COUNTRY_JAPAN,
		COUNTRY_USA,
		COUNTRY_AUSTRALIA,
		COUNTRY_FRANCE,
		COUNTRY_GERMANY,
		COUNTRY_ITALY,
		COUNTRY_KOREA,
		COUNTRY_NETHERLANDS,
		COUNTRY_RUSSIA,
		COUNTRY_SPAIN,
		COUNTRY_TAIWAN,
		COUNTRY_WORLD,
		COUNTRY_UNKNOWN,
		NUMBER_OF_COUNTRIES
	};

	// Languages 0 - 9 match the official Wii language numbering.
	// Languages 1 - 6 match the official GC PAL languages 0 - 5.
	enum ELanguage
	{
		LANGUAGE_JAPANESE = 0,
		LANGUAGE_ENGLISH = 1,
		LANGUAGE_GERMAN = 2,
		LANGUAGE_FRENCH = 3,
		LANGUAGE_SPANISH = 4,
		LANGUAGE_ITALIAN = 5,
		LANGUAGE_DUTCH = 6,
		LANGUAGE_SIMPLIFIED_CHINESE = 7,
		LANGUAGE_TRADITIONAL_CHINESE = 8,
		LANGUAGE_KOREAN = 9,
		LANGUAGE_UNKNOWN
	};

	IVolume() {}
	virtual ~IVolume() {}

	// decrypt parameter must be false if not reading a Wii disc
	virtual bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const = 0;
	template <typename T>
	bool ReadSwapped(u64 offset, T* buffer, bool decrypt) const
	{
		T temp;
		if (!Read(offset, sizeof(T), reinterpret_cast<u8*>(&temp), decrypt))
			return false;
		*buffer = Common::FromBigEndian(temp);
		return true;
	}

	virtual bool GetTitleID(u64*) const { return false; }
	virtual std::vector<u8> GetTMD() const { return {}; }
	virtual std::string GetUniqueID() const = 0;
	virtual std::string GetMakerID() const = 0;
	virtual u16 GetRevision() const = 0;
	virtual std::string GetInternalName() const = 0;
	virtual std::map<ELanguage, std::string> GetNames(bool prefer_long) const = 0;
	virtual std::map<ELanguage, std::string> GetDescriptions() const { return std::map<ELanguage, std::string>(); }
	virtual std::string GetCompany() const { return std::string(); }
	virtual std::vector<u32> GetBanner(int* width, int* height) const = 0;
	virtual u64 GetFSTSize() const = 0;
	virtual std::string GetApploaderDate() const = 0;
	// 0 is the first disc, 1 is the second disc
	virtual u8 GetDiscNumber() const { return 0; }

	virtual EPlatform GetVolumeType() const = 0;
	virtual bool SupportsIntegrityCheck() const { return false; }
	virtual bool CheckIntegrity() const { return false; }
	virtual bool ChangePartition(u64 offset) { return false; }

	virtual ECountry GetCountry() const = 0;
	virtual BlobType GetBlobType() const = 0;
	// Size of virtual disc (not always accurate)
	virtual u64 GetSize() const = 0;
	// Size on disc (compressed size)
	virtual u64 GetRawSize() const = 0;

	static std::vector<u32> GetWiiBanner(int* width, int* height, u64 title_id);

protected:
	template <u32 N>
	std::string DecodeString(const char(&data)[N]) const
	{
		// strnlen to trim NULLs
		std::string string(data, strnlen(data, sizeof(data)));

		// There don't seem to be any GC discs with the country set to Taiwan...
		// But maybe they would use Shift_JIS if they existed? Not sure
		bool use_shift_jis = (COUNTRY_JAPAN == GetCountry() || COUNTRY_TAIWAN == GetCountry());

		if (use_shift_jis)
			return SHIFTJISToUTF8(string);
		else
			return CP1252ToUTF8(string);
	}

	static std::map<IVolume::ELanguage, std::string> ReadWiiNames(const std::vector<u8>& data);

	static const size_t NUMBER_OF_LANGUAGES = 10;
	static const size_t NAME_STRING_LENGTH = 42;
	static const size_t NAME_BYTES_LENGTH = NAME_STRING_LENGTH * sizeof(u16);
	static const size_t NAMES_TOTAL_BYTES = NAME_BYTES_LENGTH * NUMBER_OF_LANGUAGES;
};

// Generic Switch function for all volumes
IVolume::ECountry CountrySwitch(u8 country_code);
u8 GetSysMenuRegion(u16 _TitleVersion);
std::string GetCompanyFromID(const std::string& company_id);

} // namespace
