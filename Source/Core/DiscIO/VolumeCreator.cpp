// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"
#include "DiscIO/VolumeDirectory.h"
#include "DiscIO/VolumeGC.h"
#include "DiscIO/VolumeWad.h"
#include "DiscIO/VolumeWiiCrypted.h"


namespace DiscIO
{
enum EDiscType
{
	DISC_TYPE_UNK,
	DISC_TYPE_WII,
	DISC_TYPE_WII_CONTAINER,
	DISC_TYPE_GC,
	DISC_TYPE_WAD
};

class CBlobBigEndianReader
{
public:
	CBlobBigEndianReader(IBlobReader& _rReader) : m_rReader(_rReader) {}

	u32 Read32(u64 _Offset)
	{
		u32 Temp;
		m_rReader.Read(_Offset, 4, (u8*)&Temp);
		return Common::swap32(Temp);
	}
	u16 Read16(u64 _Offset)
	{
		u16 Temp;
		m_rReader.Read(_Offset, 2, (u8*)&Temp);
		return Common::swap16(Temp);
	}
	u8 Read8(u64 _Offset)
	{
		u8 Temp;
		m_rReader.Read(_Offset, 1, &Temp);
		return Temp;
	}
private:
	IBlobReader& m_rReader;
};

static const unsigned char s_master_key[16] = {
	0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,
	0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7
};

static const unsigned char s_master_key_korean[16] = {
	0x63,0xb8,0x2b,0xb4,0xf4,0x61,0x4e,0x2e,
	0x13,0xf2,0xfe,0xfb,0xba,0x4c,0x9b,0x7e
};

static std::unique_ptr<IVolume> CreateVolumeFromCryptedWiiImage(std::unique_ptr<IBlobReader> reader, u32 partition_group, u32 volume_type, u32 volume_number);
EDiscType GetDiscType(IBlobReader& _rReader);

std::unique_ptr<IVolume> CreateVolumeFromFilename(const std::string& filename, u32 partition_group, u32 volume_number)
{
	std::unique_ptr<IBlobReader> reader(CreateBlobReader(filename));
	if (reader == nullptr)
		return nullptr;

	switch (GetDiscType(*reader))
	{
		case DISC_TYPE_WII:
		case DISC_TYPE_GC:
			return std::make_unique<CVolumeGC>(std::move(reader));

		case DISC_TYPE_WAD:
			return std::make_unique<CVolumeWAD>(std::move(reader));

		case DISC_TYPE_WII_CONTAINER:
			return CreateVolumeFromCryptedWiiImage(std::move(reader), partition_group, 0, volume_number);

		case DISC_TYPE_UNK:
		default:
			std::string name, extension;
			SplitPath(filename, nullptr, &name, &extension);
			name += extension;
			NOTICE_LOG(DISCIO, "%s does not have the Magic word for a gcm, wiidisc or wad file\n"
						"Set Log Verbosity to Warning and attempt to load the game again to view the values", name.c_str());
	}

	return nullptr;
}

std::unique_ptr<IVolume> CreateVolumeFromDirectory(const std::string& directory, bool is_wii, const std::string& apploader, const std::string& dol)
{
	if (CVolumeDirectory::IsValidDirectory(directory))
		return std::make_unique<CVolumeDirectory>(directory, is_wii, apploader, dol);

	return nullptr;
}

void VolumeKeyForPartition(IBlobReader& _rReader, u64 offset, u8* VolumeKey)
{
	CBlobBigEndianReader Reader(_rReader);

	u8 SubKey[16];
	_rReader.Read(offset + 0x1bf, 16, SubKey);

	u8 IV[16];
	memset(IV, 0, 16);
	_rReader.Read(offset + 0x44c, 8, IV);

	bool usingKoreanKey = false;
	// Issue: 6813
	// Magic value is at partition's offset + 0x1f1 (1byte)
	// If encrypted with the Korean key, the magic value would be 1
	// Otherwise it is zero
	if (Reader.Read8(0x3) == 'K' && Reader.Read8(offset + 0x1f1) == 1)
		usingKoreanKey = true;

	mbedtls_aes_context AES_ctx;
	mbedtls_aes_setkey_dec(&AES_ctx, (usingKoreanKey ? s_master_key_korean : s_master_key), 128);

	mbedtls_aes_crypt_cbc(&AES_ctx, MBEDTLS_AES_DECRYPT, 16, IV, SubKey, VolumeKey);
}

static std::unique_ptr<IVolume> CreateVolumeFromCryptedWiiImage(std::unique_ptr<IBlobReader> reader, u32 partition_group, u32 volume_type, u32 volume_number)
{
	CBlobBigEndianReader big_endian_reader(*reader);

	u32 numPartitions = big_endian_reader.Read32(0x40000 + (partition_group * 8));
	u64 PartitionsOffset = (u64)big_endian_reader.Read32(0x40000 + (partition_group * 8) + 4) << 2;

	// Check if we're looking for a valid partition
	if ((int)volume_number != -1 && volume_number > numPartitions)
		return nullptr;

	struct SPartition
	{
		u64 offset;
		u32 type;
	};

	struct SPartitionGroup
	{
		u32 num_partitions;
		u64 partitions_offset;
		std::vector<SPartition> partitions;
	};
	SPartitionGroup partition_groups[4];

	// Read all partitions
	for (SPartitionGroup& group : partition_groups)
	{
		for (u32 i = 0; i < numPartitions; i++)
		{
			SPartition partition;
			partition.offset = ((u64)big_endian_reader.Read32(PartitionsOffset + (i * 8) + 0)) << 2;
			partition.type   = big_endian_reader.Read32(PartitionsOffset + (i * 8) + 4);
			group.partitions.push_back(partition);
		}
	}

	// Return the partition type specified or number
	// types: 0 = game, 1 = firmware update, 2 = channel installer
	//  some partitions on SSBB use the ASCII title id of the demo VC game they hold...
	for (size_t i = 0; i < partition_groups[partition_group].partitions.size(); i++)
	{
		const SPartition& partition = partition_groups[partition_group].partitions.at(i);

		if ((partition.type == volume_type && (int)volume_number == -1) || i == volume_number)
		{
			u8 volume_key[16];
			VolumeKeyForPartition(*reader, partition.offset, volume_key);
			return std::make_unique<CVolumeWiiCrypted>(std::move(reader), partition.offset, volume_key);
		}
	}

	return nullptr;
}

EDiscType GetDiscType(IBlobReader& _rReader)
{
	CBlobBigEndianReader Reader(_rReader);
	u32 WiiMagic = Reader.Read32(0x18);
	u32 WiiContainerMagic = Reader.Read32(0x60);
	u32 WADMagic = Reader.Read32(0x02);
	u32 GCMagic = Reader.Read32(0x1C);

	// Check for Wii
	if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic != 0)
		return DISC_TYPE_WII;
	if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic == 0)
		return DISC_TYPE_WII_CONTAINER;

	// Check for WAD
	// 0x206962 for boot2 wads
	if (WADMagic == 0x00204973 || WADMagic == 0x00206962)
		return DISC_TYPE_WAD;

	// Check for GC
	if (GCMagic == 0xC2339F3D)
		return DISC_TYPE_GC;

	WARN_LOG(DISCIO, "No known magic words found");
	WARN_LOG(DISCIO, "Wii  offset: 0x18 value: 0x%08x", WiiMagic);
	WARN_LOG(DISCIO, "WiiC offset: 0x60 value: 0x%08x", WiiContainerMagic);
	WARN_LOG(DISCIO, "WAD  offset: 0x02 value: 0x%08x", WADMagic);
	WARN_LOG(DISCIO, "GC   offset: 0x1C value: 0x%08x", GCMagic);

	return DISC_TYPE_UNK;
}

}  // namespace
