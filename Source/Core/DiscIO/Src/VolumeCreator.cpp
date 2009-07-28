// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <vector>

#include "AES/aes.h"

#include "VolumeCreator.h"

#include "Volume.h"
#include "VolumeDirectory.h"
#include "VolumeGC.h"
#include "VolumeWiiCrypted.h"
#include "VolumeWad.h"

#include "Hash.h"

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
		return(Common::swap32(Temp));
	}

private:
	IBlobReader& m_rReader;
};

const unsigned char g_MasterKey[16] = {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7};
const unsigned char g_MasterKeyK[16] = {0x63,0xb8,0x2b,0xb4,0xf4,0x61,0x4e,0x2e,0x13,0xf2,0xfe,0xfb,0xba,0x4c,0x9b,0x7e};

IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum, bool Korean);
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
			delete pReader;
			return NULL;
	}

	// unreachable code
	return NULL;
}

IVolume* CreateVolumeFromDirectory(const std::string& _rDirectory, bool _bIsWii)
{
	if (CVolumeDirectory::IsValidDirectory(_rDirectory))
		return new CVolumeDirectory(_rDirectory, _bIsWii);
	
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

IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _PartitionGroup, u32 _VolumeType, u32 _VolumeNum, bool Korean)
{
	CBlobBigEndianReader Reader(_rReader);

	u32 numPartitions = Reader.Read32(0x40000 + (_PartitionGroup * 8));
	u64 PartitionsOffset = (u64)Reader.Read32(0x40000 + (_PartitionGroup * 8) + 4) << 2;

	// Check if we're looking for a valid partition
	if (_VolumeNum != -1 && _VolumeNum > numPartitions)
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

			AES_KEY AES_KEY;
			AES_set_decrypt_key((Korean ? g_MasterKeyK : g_MasterKey), 128, &AES_KEY);

			u8 VolumeKey[16];
			AES_cbc_encrypt(SubKey, VolumeKey, 16, &AES_KEY, IV, AES_DECRYPT);

			// -1 means the caller just wanted the partition with matching type
			if (_VolumeNum == -1 || i == _VolumeNum)
				return new CVolumeWiiCrypted(&_rReader, rPartition.Offset + 0x20000, VolumeKey);
		}
	}

	return NULL;
}

EDiscType GetDiscType(IBlobReader& _rReader)
{
	CBlobBigEndianReader Reader(_rReader);

	// check for wii
	{
		u32 MagicWord = Reader.Read32(0x18);

		if (MagicWord == 0x5D1C9EA3)
		{
			if (Reader.Read32(0x60) != 0)
				return(DISC_TYPE_WII);
			else
				return(DISC_TYPE_WII_CONTAINER);
		}
	}

	// check for WAD
	{
		u32 MagicWord = Reader.Read32(0x02);

		// 0x206962 for boot2 wads
		if (MagicWord == 0x00204973 || MagicWord == 0x00206962)
			return(DISC_TYPE_WAD);
	}

	// check for GC
	{
		u32 MagicWord = Reader.Read32(0x1C);

		if (MagicWord == 0xC2339F3D)
			return(DISC_TYPE_GC);
	}

	return DISC_TYPE_UNK;
}

}  // namespace
