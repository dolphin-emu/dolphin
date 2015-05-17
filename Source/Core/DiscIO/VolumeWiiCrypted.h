// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
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
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt) const override;
	bool GetTitleID(u8* _pBuffer) const override;
	virtual std::unique_ptr<u8[]> GetTMD(u32 *_sz) const override;
	std::string GetUniqueID() const override;
	std::string GetMakerID() const override;
	int GetRevision() const override;
	std::string GetInternalName() const override;
	std::map<IVolume::ELanguage, std::string> GetNames() const override;
	u32 GetFSTSize() const override;
	std::string GetApploaderDate() const override;

	bool IsDiscTwo() const override;
	bool IsWiiDisc() const override;
	bool SupportsIntegrityCheck() const override { return true; }
	bool CheckIntegrity() const override;
	bool ChangePartition(u64 offset) override;

	ECountry GetCountry() const override;
	u64 GetSize() const override;
	u64 GetRawSize() const override;



private:
	static const unsigned int s_block_header_size = 0x0400;
	static const unsigned int s_block_data_size   = 0x7C00;
	static const unsigned int s_block_total_size  = s_block_header_size + s_block_data_size;

	std::unique_ptr<IBlobReader> m_pReader;
	std::unique_ptr<aes_context> m_AES_ctx;

	u8* m_pBuffer;

	u64 m_VolumeOffset;
	u64 m_dataOffset;

	mutable u64 m_LastDecryptedBlockOffset;
	mutable unsigned char m_LastDecryptedBlock[s_block_data_size];
};

} // namespace
