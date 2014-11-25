// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <polarssl/aes.h>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for encrypted Wii images ---

namespace DiscIO
{

class IBlobReader;

class CVolumeWiiCrypted : public IVolume
{
public:
	CVolumeWiiCrypted(IBlobReader* _pReader, u64 _VolumeOffset, const unsigned char* _pVolumeKey);
	~CVolumeWiiCrypted();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const override;
	bool RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const override;
	bool GetTitleID(u8* _pBuffer) const override;
	void GetTMD(u8* _pBuffer, u32* _sz) const override;
	std::string GetUniqueID() const override;
	std::string GetMakerID() const override;
	std::vector<std::string> GetNames() const override;
	u32 GetFSTSize() const override;
	std::string GetApploaderDate() const override;
	ECountry GetCountry() const override;
	u64 GetSize() const override;
	u64 GetRawSize() const override;
	int GetRevision() const override;

	bool SupportsIntegrityCheck() const override { return true; }
	bool CheckIntegrity() const override;

private:
	std::unique_ptr<IBlobReader> m_pReader;
	std::unique_ptr<aes_context> m_AES_ctx;

	u8* m_pBuffer;

	u64 m_VolumeOffset;
	u64 m_dataOffset;

	mutable u64 m_LastDecryptedBlockOffset;
	mutable unsigned char m_LastDecryptedBlock[0x8000];
};

} // namespace
