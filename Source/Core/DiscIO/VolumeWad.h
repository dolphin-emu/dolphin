// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for Wad files ---
// Some of this code might look redundant with the CNANDContentLoader class, however,
// We do not do any decryption here, we do raw read, so things are -Faster-

namespace DiscIO
{

class CVolumeWAD : public IVolume
{
public:
	CVolumeWAD(std::unique_ptr<IBlobReader> reader);
	~CVolumeWAD();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt = false) const override;
	bool GetTitleID(u64* buffer) const override;
	std::string GetUniqueID() const override;
	std::string GetMakerID() const override;
	u16 GetRevision() const override;
	std::string GetInternalName() const override { return ""; }
	std::map<IVolume::ELanguage, std::string> GetNames(bool prefer_long) const override;
	std::vector<u32> GetBanner(int* width, int* height) const override;
	u64 GetFSTSize() const override { return 0; }
	std::string GetApploaderDate() const override { return ""; }

	EPlatform GetVolumeType() const override;
	ECountry GetCountry() const override;

	BlobType GetBlobType() const override;
	u64 GetSize() const override;
	u64 GetRawSize() const override;

private:
	std::unique_ptr<IBlobReader> m_pReader;
	u32 m_offset;
	u32 m_tmd_offset;
	u32 m_opening_bnr_offset;
	u32 m_hdr_size;
	u32 m_cert_size;
	u32 m_tick_size;
	u32 m_tmd_size;
	u32 m_data_size;
};

} // namespace
