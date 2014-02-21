// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for GC disc images ---

namespace DiscIO
{

class IBlobReader;

class CVolumeGC : public IVolume
{
public:
	CVolumeGC(IBlobReader* _pReader);
	~CVolumeGC();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const;
	bool RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const;
	std::string GetUniqueID() const;
	std::string GetRevisionSpecificUniqueID() const;
	std::string GetMakerID() const;
	int GetRevision() const;
	std::vector<std::string> GetNames() const;
	u32 GetFSTSize() const;
	std::string GetApploaderDate() const;
	ECountry GetCountry() const;
	u64 GetSize() const;
	u64 GetRawSize() const;
	bool IsDiscTwo() const;

	typedef std::string(*StringDecoder)(const std::string&);

	static StringDecoder GetStringDecoder(ECountry country);

private:
	IBlobReader* m_pReader;
};

} // namespace
