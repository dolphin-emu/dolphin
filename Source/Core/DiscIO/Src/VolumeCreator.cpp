// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <vector>

#include <polarssl/aes.h>

#include "VolumeCreator.h"

#include "Volume.h"
#include "VolumeDirectory.h"
#include "VolumeGC.h"
#include "VolumeWiiCrypted.h"
#include "VolumeWad.h"

#include "Hash.h"
#include "StringUtil.h"

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

#ifndef _WIN32
struct SPartition
{
	u64 Offset;
	u32 Type;
}; //gcc 4.3 cries if it's local
#endif

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

const unsigned char g_MasterKey[16] = {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7};
const unsigned char g_MasterKeyK[16] = {0x63,0xb8,0x2b,0xb4,0xf4,0x61,0x4e,0x2e,0x13,0xf2,0xfe,0xfb,0xba,0x4c,0x9b,0x7e};

static IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum, bool Korean);
EDiscType GetDiscType(IBlobReader& _rReader);

IVolume* CreateVolumeFromFilename(const std::string& _rFilename, u32 _PartitionGroup, u32 _VolumeNum)
{
	IBlobReader* pReader = CreateBlobReader(_rFilename.c_str());
	if (pReader == NULL)
		return NULL;

	switch (GetDiscType(*pReader))
	{
		case DISC_TYPE_WII:
		case DISC_TYPE_GC:
			return new CVolumeGC(pReader);

		case DISC_TYPE_WAD:
			return new CVolumeWAD(pReader);

		case DISC_TYPE_WII_CONTAINER:
		{
			u8 region;
			pReader->Read(0x3,1,&region);

			IVolume* pVolume = CreateVolumeFromCryptedWiiImage(*pReader, _PartitionGroup, 0, _VolumeNum, region == 'K');

			if (pVolume == NULL)
			{
				delete pReader;
			}

			return pVolume;
		}
			break;

		case DISC_TYPE_UNK:
		default:
			std::string Filename, ext;
			SplitPath(_rFilename, NULL, &Filename, &ext);
			Filename += ext;
			NOTICE_LOG(DISCIO, "%s does not have the Magic word for a gcm, wiidisc or wad file\n"
						"Set Log Verbosity to Warning and attempt to load the game again to view the values", Filename.c_str());
			delete pReader;
			return NULL;
	}

	// unreachable code
	return NULL;
}

IVolume* CreateVolumeFromDirectory(const std::string& _rDirectory, bool _bIsWii, const std::string& _rApploader, const std::string& _rDOL)
{
	if (CVolumeDirectory::IsValidDirectory(_rDirectory))
		return new CVolumeDirectory(_rDirectory, _bIsWii, _rApploader, _rDOL);

	return NULL;
}

bool IsVolumeWiiDisc(const IVolume *_rVolume)
{
	u32 MagicWord = 0;
	_rVolume->Read(0x18, 4, (u8*)&MagicWord);

	return (Common::swap32(MagicWord) == 0x5D1C9EA3);
	//Gamecube 0xc2339f3d
}

bool IsVolumeWadFile(const IVolume *_rVolume)
{
	u32 MagicWord = 0;
	_rVolume->Read(0x02, 4, (u8*)&MagicWord);

	return (Common::swap32(MagicWord) == 0x00204973 || Common::swap32(MagicWord) == 0x00206962);
}

static IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum, bool Korean)
{
	CBlobBigEndianReader Reader(_rReader);

	u32 numPartitions = Reader.Read32(0x40000 + (_PartitionGroup * 8));
	u64 PartitionsOffset = (u64)Reader.Read32(0x40000 + (_PartitionGroup * 8) + 4) << 2;

	// Check if we're looking for a valid partition
	if ((int)_VolumeNum != -1 && _VolumeNum > numPartitions)
		return NULL;

	#ifdef _WIN32
	struct SPartition
	{
		u64 Offset;
		u32 Type;
	};
	#endif
	struct SPartitionGroup
	{
		u32 numPartitions;
		u64 PartitionsOffset;
		std::vector<SPartition> PartitionsVec;
	};
	SPartitionGroup PartitionGroup[4];

	// read all partitions
	for (u32 x = 0; x < 4; x++)
	{
		for (u32 i = 0; i < numPartitions; i++)
		{
			SPartition Partition;
			Partition.Offset = ((u64)Reader.Read32(PartitionsOffset + (i * 8) + 0)) << 2;
			Partition.Type   = Reader.Read32(PartitionsOffset + (i * 8) + 4);
			PartitionGroup[x].PartitionsVec.push_back(Partition);
		}
	}

	// return the partition type specified or number
	// types: 0 = game, 1 = firmware update, 2 = channel installer
	//  some partitions on ssbb use the ascii title id of the demo VC game they hold...
	for (size_t i = 0; i < PartitionGroup[_PartitionGroup].PartitionsVec.size(); i++)
	{
		const SPartition& rPartition = PartitionGroup[_PartitionGroup].PartitionsVec.at(i);

		if (rPartition.Type == _VolumeType || i == _VolumeNum)
		{
			u8 SubKey[16];
			_rReader.Read(rPartition.Offset + 0x1bf, 16, SubKey);

			u8 IV[16];
			memset(IV, 0, 16);
			_rReader.Read(rPartition.Offset + 0x44c, 8, IV);

			bool usingKoreanKey = false;
			// Issue: 6813
			// Magic value is at partition's offset + 0x1f1 (1byte)
			// If encrypted with the Korean key, the magic value would be 1
			// Otherwise it is zero
			if (Korean && Reader.Read8(rPartition.Offset + 0x1f1) == 1)
				usingKoreanKey = true;

			aes_context AES_ctx;
			aes_setkey_dec(&AES_ctx, (usingKoreanKey ? g_MasterKeyK : g_MasterKey), 128);

			u8 VolumeKey[16];
			aes_crypt_cbc(&AES_ctx, AES_DECRYPT, 16, IV, SubKey, VolumeKey);

			// -1 means the caller just wanted the partition with matching type
			if ((int)_VolumeNum == -1 || i == _VolumeNum)
				return new CVolumeWiiCrypted(&_rReader, rPartition.Offset, VolumeKey);
		}
	}

	return NULL;
}

EDiscType GetDiscType(IBlobReader& _rReader)
{
	CBlobBigEndianReader Reader(_rReader);
	u32 WiiMagic = Reader.Read32(0x18);
	u32 WiiContainerMagic = Reader.Read32(0x60);
	u32 WADMagic = Reader.Read32(0x02);
	u32 GCMagic = Reader.Read32(0x1C);

	// check for Wii
	if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic != 0)
		return DISC_TYPE_WII;
	if (WiiMagic == 0x5D1C9EA3 && WiiContainerMagic == 0)
		return DISC_TYPE_WII_CONTAINER;

	// check for WAD
	// 0x206962 for boot2 wads
	if (WADMagic == 0x00204973 || WADMagic == 0x00206962)
		return DISC_TYPE_WAD;

	// check for GC
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
