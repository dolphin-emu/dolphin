// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
class IVolume
{
public:
	IVolume() {}
	virtual ~IVolume() {}

	virtual bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const = 0;
	virtual bool RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const = 0;
	virtual bool GetTitleID(u8*) const { return false; }
	virtual void GetTMD(u8*, u32 *_sz) const { *_sz=0; }
	virtual std::string GetUniqueID() const = 0;
	virtual std::string GetRevisionSpecificUniqueID() const { return ""; }
	virtual std::string GetMakerID() const = 0;
	virtual int GetRevision() const { return 0; }
	// TODO: eliminate?
	virtual std::string GetName() const;
	virtual std::vector<std::string> GetNames() const = 0;
	virtual u32 GetFSTSize() const = 0;
	virtual std::string GetApploaderDate() const = 0;
	virtual bool SupportsIntegrityCheck() const { return false; }
	virtual bool CheckIntegrity() const { return false; }
	virtual bool IsDiscTwo() const { return false; }

	enum ECountry
	{
		COUNTRY_EUROPE = 0,
		COUNTRY_FRANCE,
		COUNTRY_RUSSIA,
		COUNTRY_USA,
		COUNTRY_JAPAN,
		COUNTRY_KOREA,
		COUNTRY_ITALY,
		COUNTRY_TAIWAN,
		COUNTRY_SDK,
		COUNTRY_UNKNOWN,
		COUNTRY_GERMANY,
		COUNTRY_AUSTRALIA,
		NUMBER_OF_COUNTRIES
	};

	virtual ECountry GetCountry() const = 0;
	virtual u64 GetSize() const = 0;

	// Size on disc (compressed size)
	virtual u64 GetRawSize() const = 0;
};

// Generic Switch function for all volumes
IVolume::ECountry CountrySwitch(u8 CountryCode);
u8 GetSysMenuRegion(u16 _TitleVersion);

} // namespace
