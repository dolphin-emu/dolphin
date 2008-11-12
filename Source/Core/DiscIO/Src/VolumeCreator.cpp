// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Hash.h"

namespace DiscIO
{
enum EDiscType
{
	DISC_TYPE_UNK,
	DISC_TYPE_WII,
	DISC_TYPE_WII_CONTAINER,
	DISC_TYPE_GC
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

unsigned char g_MasterKey[16];
bool g_MasterKeyInit = false;

IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _VolumeType);
EDiscType GetDiscType(IBlobReader& _rReader);

IVolume* CreateVolumeFromFilename(const std::string& _rFilename)
{
	IBlobReader* pReader = CreateBlobReader(_rFilename.c_str());

	if (pReader == NULL)
		return NULL;

	switch (GetDiscType(*pReader))
	{
	    case DISC_TYPE_WII:
	    case DISC_TYPE_GC:
		    return new CVolumeGC(pReader);

	    case DISC_TYPE_WII_CONTAINER:
	    {
		    IVolume* pVolume = CreateVolumeFromCryptedWiiImage(*pReader, 0);

		    if (pVolume == NULL)
		    {
			    delete pReader;
		    }

		    return(pVolume);
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
}

IVolume* CreateVolumeFromCryptedWiiImage(IBlobReader& _rReader, u32 _VolumeType)
{
	if (!g_MasterKeyInit)
	{
		FILE* pT = fopen(WII_MASTERKEY_FILE, "rb");
		if (pT == NULL)
		{
			PanicAlert("Can't open " WII_MASTERKEY_FILE);
			return NULL;
		}

		fread(g_MasterKey, 16, 1, pT);
		fclose(pT);
		const u32 keyhash = 0x4bc30936;
		u32 hash = HashAdler32(g_MasterKey, 16);
		if (hash != keyhash)
			PanicAlert("Your Wii disc decryption key is bad.", keyhash, hash);
		else
			g_MasterKeyInit = true;
	}

	CBlobBigEndianReader Reader(_rReader);

	u32 numPartitions = Reader.Read32(0x40000);
	u64 PartitionsOffset = (u64)Reader.Read32(0x40004) << 2;
	#ifdef _WIN32
	struct SPartition
	{
		u64 Offset;
		u32 Type;
	};
	#endif
	std::vector<SPartition> PartitionsVec;
	
	// read all partitions
	for (u32 i = 0; i < numPartitions; i++)
	{
		SPartition Partition;
		Partition.Offset = ((u64)Reader.Read32(PartitionsOffset + (i * 8) + 0)) << 2;
		Partition.Type   = Reader.Read32(PartitionsOffset + (i * 8) + 4);
		PartitionsVec.push_back(Partition);
	}

	// find the partition with the game... type == 1 is prolly the firmware update partition
	for (size_t i = 0; i < PartitionsVec.size(); i++)
	{
		const SPartition& rPartition = PartitionsVec[i];

		if (rPartition.Type == _VolumeType)
		{
			u8 SubKey[16];
			_rReader.Read(rPartition.Offset + 0x1bf, 16, SubKey);

			u8 IV[16];
			memset(IV, 0, 16);
			_rReader.Read(rPartition.Offset + 0x44c, 8, IV);

			AES_KEY AES_KEY;
			AES_set_decrypt_key(g_MasterKey, 128, &AES_KEY);

			u8 VolumeKey[16];
			AES_cbc_encrypt(SubKey, VolumeKey, 16, &AES_KEY, IV, AES_DECRYPT);

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

	// check for GC
	{
		u32 MagicWord = Reader.Read32(0x1C);

		if (MagicWord == 0xC2339F3D)
			return(DISC_TYPE_GC);
	}

	return DISC_TYPE_UNK;
}

}  // namespace
