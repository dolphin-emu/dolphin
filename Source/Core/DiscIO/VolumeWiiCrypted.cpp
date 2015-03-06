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
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/FileMonitor.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWiiCrypted.h"

namespace DiscIO
{

CVolumeWiiCrypted::CVolumeWiiCrypted(IBlobReader* _pReader, u64 _volume_offset,
									 const unsigned char* _pVolumeKey)
	: m_pReader(_pReader),
	m_AES_ctx(new aes_context),
	m_pBuffer(nullptr),
	m_volume_offset(_volume_offset),
	m_data_offset(0x20000),
	m_last_decrypted_block_offset(-1)
{
	aes_setkey_dec(m_AES_ctx.get(), _pVolumeKey, 128);
	m_pBuffer = new u8[s_block_total_size];
}

bool CVolumeWiiCrypted::ChangePartition(u64 offset)
{
	m_volume_offset = offset;
	m_last_decrypted_block_offset = -1;

	u8 volume_key[16];
	DiscIO::VolumeKeyForParition(*m_pReader, offset, volume_key);
	aes_setkey_dec(m_AES_ctx.get(), volume_key, 128);
	return true;
}

CVolumeWiiCrypted::~CVolumeWiiCrypted()
{
	delete[] m_pBuffer;
	m_pBuffer = nullptr;
}

bool CVolumeWiiCrypted::Read(u64 _read_offset, u64 _length, u8* _pBuffer, bool decrypt) const
{
	if (m_pReader == nullptr)
		return false;

	if (!decrypt)
		return m_pReader->Read(_read_offset, _length, _pBuffer);

	FileMon::FindFilename(_read_offset);

	while (_length > 0)
	{
		// Calculate block offset
		u64 block  = _read_offset / s_block_data_size;
		u64 offset = _read_offset % s_block_data_size;

		if (m_last_decrypted_block_offset != block)
		{
			// Read the current block
			if (!m_pReader->Read(m_volume_offset + m_data_offset + block * s_block_total_size, s_block_total_size, m_pBuffer))
				return false;

			// Decrypt the block's data.
			// 0x3D0 - 0x3DF in m_pBuffer will be overwritten,
			// but that won't affect anything, because we won't
			// use the content of m_pBuffer anymore after this
			aes_crypt_cbc(m_AES_ctx.get(), AES_DECRYPT, s_block_data_size, m_pBuffer + 0x3D0,
			              m_pBuffer + s_block_header_size, m_last_decrypted_block);
			m_last_decrypted_block_offset = block;

			// The only thing we currently use from the 0x000 - 0x3FF part
			// of the block is the IV (at 0x3D0), but it also contains SHA-1
			// hashes that IOS uses to check that discs aren't tampered with.
			// http://wiibrew.org/wiki/Wii_Disc#Encrypted
		}

		// Copy the decrypted data
		u64 max_size_to_copy = s_block_data_size - offset;
		u64 copy_size = (_length > max_size_to_copy) ? max_size_to_copy : _length;
		memcpy(_pBuffer, &m_last_decrypted_block[offset], (size_t)copy_size);

		// Update offsets
		_length      -= copy_size;
		_pBuffer     += copy_size;
		_read_offset += copy_size;
	}

	return true;
}

bool CVolumeWiiCrypted::GetTitleID(u8* _pBuffer) const
{
	// Ticket is at m_volume_offset size 0x2A4
	// TitleID offset in ticket is 0x1DC
	return Read(m_volume_offset + 0x1DC, 8, _pBuffer, false);
}

std::unique_ptr<u8[]> CVolumeWiiCrypted::GetTMD(u32 *size) const
{
	*size = 0;
	u32 tmd_size;
	u32 tmd_address;

	Read(m_volume_offset + 0x2a4, sizeof(u32), (u8*)&tmd_size,    false);
	Read(m_volume_offset + 0x2a8, sizeof(u32), (u8*)&tmd_address, false);
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
	Read(m_volume_offset + tmd_address, tmd_size, buf.get(), false);
	*size = tmd_size;
	return buf;
}

std::string CVolumeWiiCrypted::GetUniqueID() const
{
	if (m_pReader == nullptr)
		return std::string();

	char ID[7];

	if (!Read(0, 6, (u8*)ID, false))
		return std::string();

	ID[6] = '\0';

	return ID;
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

	char maker_ID[3];

	if (!Read(0x4, 0x2, (u8*)&maker_ID, false))
		return std::string();

	maker_ID[2] = '\0';

	return maker_ID;
}

int CVolumeWiiCrypted::GetRevision() const
{
	if (!m_pReader)
		return 0;

	u8 revision;
	if (!m_pReader->Read(7, 1, &revision))
		return 0;

	return revision;
}

std::vector<std::string> CVolumeWiiCrypted::GetNames() const
{
	std::vector<std::string> names;

	auto const string_decoder = CVolumeGC::GetStringDecoder(GetCountry());

	char name[0xFF] = {};
	if (m_pReader != nullptr && Read(0x20, 0x60, (u8*)&name, true))
		names.push_back(string_decoder(name));

	return names;
}

u32 CVolumeWiiCrypted::GetFSTSize() const
{
	if (m_pReader == nullptr)
		return 0;

	u32 size;

	if (!Read(0x428, 0x4, (u8*)&size, true))
		return 0;

	return size;
}

std::string CVolumeWiiCrypted::GetApploaderDate() const
{
	if (m_pReader == nullptr)
		return std::string();

	char date[16];

	if (!Read(0x2440, 0x10, (u8*)&date, true))
		return std::string();

	date[10] = '\0';

	return date;
}

bool CVolumeWiiCrypted::IsWiiDisc() const
{
	return true;
}

bool CVolumeWiiCrypted::IsDiscTwo() const
{
	u8 disc_two_check;
	m_pReader->Read(6, 1, &disc_two_check);
	return (disc_two_check == 1);
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
	u32 part_size_div_4;
	Read(m_volume_offset + 0x2BC, 4, (u8*)&part_size_div_4, false);
	u64 part_data_size = (u64)Common::swap32(part_size_div_4) * 4;

	u32 nClusters = (u32)(part_data_size / 0x8000);
	for (u32 cluster_ID = 0; cluster_ID < nClusters; ++cluster_ID)
	{
		u64 cluster_offset = m_volume_offset + m_data_offset + (u64)cluster_ID * 0x8000;

		// Read and decrypt the cluster metadata
		u8 cluster_MD_crypted[0x400];
		u8 cluster_MD[0x400];
		u8 IV[16] = { 0 };
		if (!m_pReader->Read(cluster_offset, 0x400, cluster_MD_crypted))
		{
			NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read metadata", cluster_ID);
			return false;
		}
		aes_crypt_cbc(m_AES_ctx.get(), AES_DECRYPT, 0x400, IV, cluster_MD_crypted, cluster_MD);


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
			if (cluster_MD[idx] != 0)
				meaningless = true;

		if (meaningless)
			continue;

		u8 cluster_data[0x7C00];
		if (!Read((u64)cluster_ID * 0x7C00, 0x7C00, cluster_data, true))
		{
			NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: could not read data", cluster_ID);
			return false;
		}

		for (u32 hash_ID = 0; hash_ID < 31; ++hash_ID)
		{
			u8 hash[20];

			sha1(cluster_data + hash_ID * 0x400, 0x400, hash);

			// Note that we do not use strncmp here
			if (memcmp(hash, cluster_MD + hash_ID * 20, 20))
			{
				NOTICE_LOG(DISCIO, "Integrity Check: fail at cluster %d: hash %d is invalid", cluster_ID, hash_ID);
				return false;
			}
		}
	}

	return true;
}

} // namespace
