// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VOLUME_WII_CRYPTED
#define _VOLUME_WII_CRYPTED

#include "Volume.h"
#include "Blob.h"
#include "Crypto/aes.h"

// --- this volume type is used for encrypted Wii images ---

namespace DiscIO
{
class CVolumeWiiCrypted	: public IVolume
{
public:
	CVolumeWiiCrypted(IBlobReader* _pReader, u64 _VolumeOffset, const unsigned char* _pVolumeKey);
	~CVolumeWiiCrypted();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer) const;
	bool RAWRead(u64 _Offset, u64 _Length, u8* _pBuffer) const;
	bool GetTitleID(u8* _pBuffer) const;
	void GetTMD(u8* _pBuffer, u32* _sz) const;
	std::string GetUniqueID() const;
	std::string GetMakerID() const;
	std::vector<std::string> GetNames() const;
	u32 GetFSTSize() const;
	std::string GetApploaderDate() const;
	ECountry GetCountry() const;
	u64 GetSize() const;
	u64 GetRawSize() const;

	bool SupportsIntegrityCheck() const { return true; }
	bool CheckIntegrity() const;

private:
	IBlobReader* m_pReader;

	u8* m_pBuffer;
	AES_KEY m_AES_KEY;

	u64 m_VolumeOffset;
	u64 dataOffset;

	mutable u64 m_LastDecryptedBlockOffset;
	mutable unsigned char m_LastDecryptedBlock[0x8000];
};

} // namespace

#endif

