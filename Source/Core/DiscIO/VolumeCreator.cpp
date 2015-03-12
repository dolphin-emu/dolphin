// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include <polarssl/aes.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

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

	u32 Read32(u64 _offset)
	{
		u32 temp;
		m_rReader.Read(_offset, 4, (u8*)&temp);
		return Common::swap32(temp);
	}
	u16 Read16(u64 _offset)
	{
		u16 temp;
		m_rReader.Read(_offset, 2, (u8*)&temp);
		return Common::swap16(temp);
	}
	u8 Read8(u64 _offset)
	{
		u8 temp;
		m_rReader.Read(_offset, 1, &temp);
		return temp;
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

static IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _partition_group, u32 _volume_type, u32 _volume_num);
EDiscType GetDiscType(IBlobReader& _rReader);

IVolume* CreateVolumeFromFilename(const std::string& _rFilename, u32 _partition_group, u32 _volume_num)
{
	IBlobReader* pReader = CreateBlobReader(_rFilename);
	if (pReader == nullptr)
		return nullptr;

	switch (GetDiscType(*pReader))
	{
		case DISC_TYPE_WII:
		case DISC_TYPE_GC:
			return new CVolumeGC(pReader);

		case DISC_TYPE_WAD:
			return new CVolumeWAD(pReader);

		case DISC_TYPE_WII_CONTAINER:
		{
			IVolume* pVolume = CreateVolumeFromCryptedWiiImage(*pReader, _partition_group, 0, _volume_num);

			if (pVolume == nullptr)
			{
				delete pReader;
			}

			return pVolume;
		}

		case DISC_TYPE_UNK:
		default:
			std::string Filename, ext;
			SplitPath(_rFilename, nullptr, &Filename, &ext);
			Filename += ext;
			NOTICE_LOG(DISCIO, "%s does not have the Magic word for a gcm, wiidisc or wad file\n"
			                   "Set Log Verbosity to Warning and attempt to load the game again to view the values", Filename.c_str());
			delete pReader;
	}

	return nullptr;
}

IVolume* CreateVolumeFromDirectory(const std::string& _rDirectory, bool _bIsWii, const std::string& _rApploader, const std::string& _rDOL)
{
	if (CVolumeDirectory::IsValidDirectory(_rDirectory))
		return new CVolumeDirectory(_rDirectory, _bIsWii, _rApploader, _rDOL);

	return nullptr;
}

bool IsVolumeWadFile(const IVolume *_rVolume)
{
	u32 magic_word = 0;
	_rVolume->Read(0x02, 4, (u8*)&magic_word, false);

	return (Common::swap32(magic_word) == 0x00204973 || Common::swap32(magic_word) == 0x00206962);
}

void VolumeKeyForParition(IBlobReader& _rReader, u64 offset, u8* volume_key)
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

	aes_context AES_ctx;
	aes_setkey_dec(&AES_ctx, (usingKoreanKey ? s_master_key_korean : s_master_key), 128);

	aes_crypt_cbc(&AES_ctx, AES_DECRYPT, 16, IV, SubKey, volume_key);
}

static IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _partition_group, u32 _volume_type, u32 _volume_num)
{
	CBlobBigEndianReader Reader(_rReader);

	u32 num_partitions = Reader.Read32(0x40000 + (_partition_group * 8));
	u64 partitions_offset = (u64)Reader.Read32(0x40000 + (_partition_group * 8) + 4) << 2;

	// Check if we're looking for a valid partition
	if ((int)_volume_num != -1 && _volume_num > num_partitions)
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
		std::vector<SPartition> PartitionsVec;
	};
	SPartitionGroup PartitionGroup[4];

	// read all partitions
	for (SPartitionGroup& group : PartitionGroup)
	{
		for (u32 i = 0; i < num_partitions; i++)
		{
			SPartition Partition;
			Partition.offset = ((u64)Reader.Read32(partitions_offset + (i * 8) + 0)) << 2;
			Partition.type   = Reader.Read32(partitions_offset + (i * 8) + 4);
			group.PartitionsVec.push_back(Partition);
		}
	}

	// return the partition type specified or number
	// types: 0 = game, 1 = firmware update, 2 = channel installer
	//  some partitions on SSBB use the ascii title id of the demo VC game they hold...
	for (size_t i = 0; i < PartitionGroup[_partition_group].PartitionsVec.size(); i++)
	{
		const SPartition& rPartition = PartitionGroup[_partition_group].PartitionsVec.at(i);

		if ((rPartition.type == _volume_type && (int)_volume_num == -1) || i == _volume_num)
		{
			u8 volume_key[16];
			VolumeKeyForParition(_rReader, rPartition.offset, volume_key);
			return new CVolumeWiiCrypted(&_rReader, rPartition.offset, volume_key);
		}
	}

	return nullptr;
}

EDiscType GetDiscType(IBlobReader& _rReader)
{
	CBlobBigEndianReader Reader(_rReader);
	u32 wii_magic           = Reader.Read32(0x18);
	u32 wii_container_magic = Reader.Read32(0x60);
	u32 WAD_magic           = Reader.Read32(0x02);
	u32 GC_magic            = Reader.Read32(0x1C);

	// check for Wii
	if (wii_magic == 0x5D1C9EA3 && wii_container_magic != 0)
		return DISC_TYPE_WII;
	if (wii_magic == 0x5D1C9EA3 && wii_container_magic == 0)
		return DISC_TYPE_WII_CONTAINER;

	// check for WAD
	// 0x206962 for boot2 wads
	if (WAD_magic == 0x00204973 || WAD_magic == 0x00206962)
		return DISC_TYPE_WAD;

	// check for GC
	if (GC_magic == 0xC2339F3D)
		return DISC_TYPE_GC;

	WARN_LOG(DISCIO, "No known magic words found");
	WARN_LOG(DISCIO, "Wii  offset: 0x18 value: 0x%08x", wii_magic);
	WARN_LOG(DISCIO, "WiiC offset: 0x60 value: 0x%08x", wii_container_magic);
	WARN_LOG(DISCIO, "WAD  offset: 0x02 value: 0x%08x", WAD_magic);
	WARN_LOG(DISCIO, "GC   offset: 0x1C value: 0x%08x", GC_magic);

	return DISC_TYPE_UNK;
}

}  // namespace
