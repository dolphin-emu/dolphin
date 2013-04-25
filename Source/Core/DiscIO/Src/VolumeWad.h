// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VOLUME_WAD
#define _VOLUME_WAD

#include "Volume.h"
#include "Blob.h"
#include "NANDContentLoader.h"

// --- this volume type is used for Wad files ---
// Some of this code might look redundant with the CNANDContentLoader class, however,
// We do not do any decryption here, we do raw read, so things are -Faster- 

namespace DiscIO
{
class CVolumeWAD : public IVolume
{
public:
	CVolumeWAD(IBlobReader* _pReader);
	~CVolumeWAD();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const;
	bool RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const { return false; }
	bool GetTitleID(u8* _pBuffer) const;
	std::string GetUniqueID() const;
	std::string GetMakerID() const;
	std::vector<std::string> GetNames() const;
	u32 GetFSTSize() const					{ return 0; }
	std::string GetApploaderDate() const	{ return "0"; }	
	ECountry GetCountry() const;
	u64 GetSize() const;
	u64 GetRawSize() const;

private:
	IBlobReader* m_pReader;
	u64 m_titleID;
	u32 OpeningBnrOffset, hdr_size, cert_size, tick_size, tmd_size, data_size;
	u8 m_Country;
};

} // namespace

#endif
