// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <polarssl/aes.h>
#include <polarssl/sha1.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWiiCrypted.h"

namespace DiscIO
{

CVolumeWiiCrypted::CVolumeWiiCrypted(std::unique_ptr<IBlobReader> reader, u64 _VolumeOffset,
									 const unsigned char* _pVolumeKey)
	: m_pReader(std::move(reader)),
	m_AES_ctx(new aes_context),
	m_pBuffer(nullptr),
	m_VolumeOffset(_VolumeOffset),
	m_dataOffset(0x20000),
	m_LastDecryptedBlockOffset(-1)
{
	aes_setkey_dec(m_AES_ctx.get(), _pVolumeKey, 128);
	m_pBuffer = new u8[s_block_total_size];
}

bool CVolumeWiiCrypted::ChangePartition(u64 offset)
{
	m_VolumeOffset = offset;
	m_LastDecryptedBlockOffset = -1;

	u8 volume_key[16];
	DiscIO::VolumeKeyForPartition(*m_pReader, offset, volume_key);
	aes_setkey_dec(m_AES_ctx.get(), volume_key, 128);
	return true;
}

CVolumeWiiCrypted::~CVolumeWiiCrypted()
{
	delete[] m_pBuffer;
	m_pBuffer = nullptr;
}

bool CVolumeWiiCrypted::Read(u64 _ReadOffset, u64 _Length, u8* _pBuffer, bool decrypt) const
{
	if (m_pReader == nullptr)
		return false;

	if (!decrypt)
		return m_pReader->Read(_ReadOffset, _Length, _pBuffer);

	FileMon::FindFilename(_ReadOffset);

	while (_Length > 0)
	{
		// Calculate block offset
		u64 Block  = _ReadOffset / s_block_data_size;
		u64 Offset = _ReadOffset % s_block_data_size;

		if (m_LastDecryptedBlockOffset != Block)
		{
			// Read the current block
			if (!m_pReader->Read(m_VolumeOffset + m_dataOffset + Block * s_block_total_size, s_block_total_size, m_pBuffer))
				return false;

			// Decrypt the block's data.
			// 0x3D0 - 0x3DF in m_pBuffer will be overwritten,
			// but that won't affect anything, because we won't
			// use the content of m_pBuffer anymore after this
			aes_crypt_cbc(m_AES_ctx.get(), AES_DECRYPT, s_block_data_size, m_pBuffer + 0x3D0,
			              m_pBuffer + s_block_header_size, m_LastDecryptedBlock);
			m_LastDecryptedBlockOffset = Block;

			// The only thing we currently use from the 0x000 - 0x3FF part
			// of the block is the IV (at 0x3D0), but it also contains SHA-1
			// hashes that IOS uses to check that discs aren't tampered with.
			// http://wiibrew.org/wiki/Wii_Disc#Encrypted
		}

		// Copy the decrypted data
		u64 MaxSizeToCopy = s_block_data_size - Offset;
		u64 CopySize = (_Length > MaxSizeToCopy) ? MaxSizeToCopy : _Length;
		memcpy(_pBuffer, &m_LastDecryptedBlock[Offset], (size_t)CopySize);

		// Update offsets
		_Length     -= CopySize;
		_pBuffer    += CopySize;
		_ReadOffset += CopySize;
	}

	return true;
}

bool CVolumeWiiCrypted::GetTitleID(u64* buffer) const
{
	// Tik is at m_VolumeOffset size 0x2A4
	// TitleID offset in tik is 0x1DC
	if (!Read(m_VolumeOffset + 0x1DC, sizeof(u64), reinterpret_cast<u8*>(buffer), false))
		return false;

	*buffer = Common::swap64(*buffer);
	return true;
}

std::unique_ptr<u8[]> CVolumeWiiCrypted::GetTMD(u32 *size) const
{
	*size = 0;
	u32 tmd_size;
	u32 tmd_address;

	Read(m_VolumeOffset + 0x2a4, sizeof(u32), (u8*)&tmd_size, false);
	Read(m_VolumeOffset + 0x2a8, sizeof(u32), (u8*)&tmd_address, false);
	tmd_size = Common::swap32(tmd_size);
	tmd_address = Common::swap32(tmd_address) << 2;

	if (tmd_size > 1024 * 1024 * 4)
	{
		// The size is checked so that a malicious or corrupt ISO
		// can't force Dolphin to allocate up to 4 GiB of memory.
		// 4 MiB should be much bigger than the size of TMDs and much smaller
		// than the amount of RAM in a computer that can run Dolphin.
		PanicAlert("TMD > 4 MiB");
		tmd_size = 1024 * 1024 * 4;
	}

	std::unique_ptr<u8[]> buf{ new u8[tmd_size] };
	Read(m_VolumeOffset + tmd_address, tmd_size, buf.get(), false);
	*size = tmd_size;
	return buf;
}

std::string CVolumeWiiCrypted::GetUniqueID() const
{
	if (m_pReader == nullptr)
		return std::string();

	char ID[6];

	if (!Read(0, 6, (u8*)ID, false))
		return std::string();

	return DecodeString(ID);
}


IVolume::ECountry CVolumeWiiCrypted::GetCountry() const
{
	if (!m_pReader)
		return COUNTRY_UNKNOWN;

	u8 country_code;
	m_pReader->Read(3, 1, &country_code);

	return CountrySwitch(country_code);
}

std::string CVolumeWiiCrypted::GetMakerID() const
{
	if (m_pReader == nullptr)
		return std::string();

	char makerID[2];

	if (!Read(0x4, 0x2, (u8*)&makerID, false))
		return std::string();

	return DecodeString(makerID);
}

u16 CVolumeWiiCrypted::GetRevision() const
{
	if (!m_pReader)
		return 0;

	u8 revision;
	if (!m_pReader->Read(7, 1, &revision))
		return 0;

	return revision;
}

std::string CVolumeWiiCrypted::GetInternalName() const
{
	char name_buffer[0x60];
	if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)&name_buffer, false))
		return DecodeString(name_buffer);

	return "";
}

std::map<IVolume::ELanguage, std::string> CVolumeWiiCrypted::GetNames(bool prefer_long) const
{
	std::unique_ptr<IFileSystem> file_system(CreateFileSystem(this));
	std::vector<u8> opening_bnr(NAMES_TOTAL_BYTES);
	opening_bnr.resize(file_system->ReadFile("opening.bnr", opening_bnr.data(), opening_bnr.size(), 0x5C));
	return ReadWiiNames(opening_bnr);
}

u64 CVolumeWiiCrypted::GetFSTSize() const
{
	if (m_pReader == nullptr)
		return 0;

	u32 size;

	if (!Read(0x428, 0x4, (u8*)&size, true))
		return 0;

	return (u64)Common::swap32(size) << 2;
}

std::string CVolumeWiiCrypted::GetApploaderDate() const
{
	if (m_pReader == nullptr)
		return std::string();

	char date[16];

	if (!Read(0x2440, 0x10, (u8*)&date, true))
		return std::string();

	return DecodeString(date);
}

IVolume::EPlatform CVolumeWiiCrypted::GetVolumeType() const
{
	return WII_DISC;
}

u8 CVolumeWiiCrypted::GetDiscNumber() const
{
	u8 disc_number;
	m_pReader->Read(6, 1, &disc_number);
	return disc_number;
}

BlobType CVolumeWiiCrypted::GetBlobType() const
{
	return m_pReader ? m_pReader->GetBlobType() : BlobType::PLAIN;
}

u64 CVolumeWiiCrypted::GetSize() const
{
	if (m_pReader)
		return m_pReader->GetDataSize();
	else
		return 0;
}

u64 CVolumeWiiCrypted::GetRawSize() const
{
	if (m_pReader)
		return m_pReader->GetRawSize();
	else
		return 0;
}

bool CVolumeWiiCrypted::CheckIntegrity() const
{
	// Get partition data size
	u32 partSizeDiv4;
	Read(m_VolumeOffset + 0x2BC, 4, (u8*)&partSizeDiv4, false);
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
		if (!Read((u64)clusterID * 0x7C00, 0x7C00, clusterData, true))
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
