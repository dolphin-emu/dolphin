// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <polarssl/aes.h>
#include <polarssl/sha1.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWiiCrypted.h"

namespace DiscIO
{

CVolumeWiiCrypted::CVolumeWiiCrypted(IBlobReader* _pReader, u64 _VolumeOffset,
									 const unsigned char* _pVolumeKey)
	: m_pReader(_pReader),
	m_AES_ctx(new aes_context),
	m_pBuffer(nullptr),
	m_VolumeOffset(_VolumeOffset),
	m_dataOffset(0x20000),
	m_LastDecryptedBlockOffset(-1)
{
	aes_setkey_dec(m_AES_ctx.get(), _pVolumeKey, 128);
	m_pBuffer = new u8[0x8000];
}


CVolumeWiiCrypted::~CVolumeWiiCrypted()
{
	delete[] m_pBuffer;
	m_pBuffer = nullptr;
}

bool CVolumeWiiCrypted::RAWRead( u64 _Offset, u64 _Length, u8* _pBuffer ) const
{
	// HyperIris: hack for DVDLowUnencryptedRead
	// Medal Of Honor Heroes 2 read this DVD offset for PartitionsInfo
	// and, PartitionsInfo is not encrypted, let's read it directly.
	if (!m_pReader->Read(_Offset, _Length, _pBuffer))
	{
		return(false);
	}
	return true;
}

bool CVolumeWiiCrypted::Read(u64 _ReadOffset, u64 _Length, u8* _pBuffer) const
{
	if (m_pReader == nullptr)
	{
		return(false);
	}

	while (_Length > 0)
	{
		static unsigned char IV[16];

		// math block offset
		u64 Block  = _ReadOffset / 0x7C00;
		u64 Offset = _ReadOffset % 0x7C00;

		// read current block
		if (!m_pReader->Read(m_VolumeOffset + m_dataOffset + Block * 0x8000, 0x8000, m_pBuffer))
		{
			return(false);
		}

		if (m_LastDecryptedBlockOffset != Block)
		{
			memcpy(IV, m_pBuffer + 0x3d0, 16);
			aes_crypt_cbc(m_AES_ctx.get(), AES_DECRYPT, 0x7C00, IV, m_pBuffer + 0x400, m_LastDecryptedBlock);

			m_LastDecryptedBlockOffset = Block;
		}

		// copy the encrypted data
		u64 MaxSizeToCopy = 0x7C00 - Offset;
		u64 CopySize = (_Length > MaxSizeToCopy) ? MaxSizeToCopy : _Length;
		memcpy(_pBuffer, &m_LastDecryptedBlock[Offset], (size_t)CopySize);

		// increase buffers
		_Length -= CopySize;
		_pBuffer    += CopySize;
		_ReadOffset += CopySize;
	}

	return(true);
}

bool CVolumeWiiCrypted::GetTitleID(u8* _pBuffer) const
{
	// Tik is at m_VolumeOffset size 0x2A4
	// TitleID offset in tik is 0x1DC
	return RAWRead(m_VolumeOffset + 0x1DC, 8, _pBuffer);
}
void CVolumeWiiCrypted::GetTMD(u8* _pBuffer, u32 * _sz) const
{
	*_sz = 0;
	u32 tmdSz,
		tmdAddr;

	RAWRead(m_VolumeOffset + 0x2a4, sizeof(u32), (u8*)&tmdSz);
	RAWRead(m_VolumeOffset + 0x2a8, sizeof(u32), (u8*)&tmdAddr);
	tmdSz = Common::swap32(tmdSz);
	tmdAddr = Common::swap32(tmdAddr) << 2;
	RAWRead(m_VolumeOffset + tmdAddr, tmdSz, _pBuffer);
	*_sz = tmdSz;
}

std::string CVolumeWiiCrypted::GetUniqueID() const
{
	if (m_pReader == nullptr)
	{
		return std::string();
	}

	char ID[7];

	if (!Read(0, 6, (u8*)ID))
	{
		return std::string();
	}

	ID[6] = '\0';

	return ID;
}


IVolume::ECountry CVolumeWiiCrypted::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 CountryCode;
	m_pReader->Read(3, 1, &CountryCode);

	return CountrySwitch(CountryCode);
}

std::string CVolumeWiiCrypted::GetMakerID() const
{
	if (m_pReader == nullptr)
	{
		return std::string();
	}

	char makerID[3];

	if (!Read(0x4, 0x2, (u8*)&makerID))
	{
		return std::string();
	}

	makerID[2] = '\0';

	return makerID;
}

std::vector<std::string> CVolumeWiiCrypted::GetNames() const
{
	std::vector<std::string> names;

	auto const string_decoder = CVolumeGC::GetStringDecoder(GetCountry());

	char name[0xFF] = {};
	if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)&name))
		names.push_back(string_decoder(name));

	return names;
}

u32 CVolumeWiiCrypted::GetFSTSize() const
{
	if (m_pReader == nullptr)
	{
		return 0;
	}

	u32 size;

	if (!Read(0x428, 0x4, (u8*)&size))
	{
		return 0;
	}

	return size;
}

std::string CVolumeWiiCrypted::GetApploaderDate() const
{
	if (m_pReader == nullptr)
	{
		return std::string();
	}

	char date[16];

	if (!Read(0x2440, 0x10, (u8*)&date))
	{
		return std::string();
	}

	date[10] = '\0';

	return date;
}

u64 CVolumeWiiCrypted::GetSize() const
{
	if (m_pReader)
	{
		return m_pReader->GetDataSize();
	}
	else
	{
		return 0;
	}
}

u64 CVolumeWiiCrypted::GetRawSize() const
{
	if (m_pReader)
	{
		return m_pReader->GetRawSize();
	}
	else
	{
		return 0;
	}
}

bool CVolumeWiiCrypted::CheckIntegrity() const
{
	// Get partition data size
	u32 partSizeDiv4;
	RAWRead(m_VolumeOffset + 0x2BC, 4, (u8*)&partSizeDiv4);
	u64 partDataSize = (u64)Common::swap32(partSizeDiv4) * 4;

	u32 nClusters = (u32)(partDataSize / 0x8000);
	for (u32 clusterID = 0; clusterID < nClusters; ++clusterID)
	{
		u64 clusterOff = m_VolumeOffset + m_dataOffset + (u64)clusterID * 0x8000;

		// Read and decrypt the cluster metadata
		u8 clusterMDCrypted[0x400];
		u8 clusterMD[0x400];
		u8 IV[16] = { 0 };
		if (!m_pReader->Read(clusterOff, 0x400, clusterMDCrypted))
		{
			NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read metadata", clusterID);
			return false;
		}
		aes_crypt_cbc(m_AES_ctx.get(), AES_DECRYPT, 0x400, IV, clusterMDCrypted, clusterMD);


		// Some clusters have invalid data and metadata because they aren't
		// meant to be read by the game (for example, holes between files). To
		// try to avoid reporting errors because of these clusters, we check
		// the 0x00 paddings in the metadata.
		//
		// This may cause some false negatives though: some bad clusters may be
		// skipped because they are *too* bad and are not even recognized as
		// valid clusters. To be improved.
		bool meaningless = false;
		for (u32 idx = 0x26C; idx < 0x280; ++idx)
			if (clusterMD[idx] != 0)
				meaningless = true;

		if (meaningless)
			continue;

		u8 clusterData[0x7C00];
		if (!Read((u64)clusterID * 0x7C00, 0x7C00, clusterData))
		{
			NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read data", clusterID);
			return false;
		}

		for (u32 hashID = 0; hashID < 31; ++hashID)
		{
			u8 hash[20];

			sha1(clusterData + hashID * 0x400, 0x400, hash);

			// Note that we do not use strncmp here
			if (memcmp(hash, clusterMD + hashID * 20, 20))
			{
				NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: hash %d is invalid", clusterID, hashID);
				return false;
			}
		}
	}

	return true;
}

} // namespace
