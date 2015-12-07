// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

namespace DiscIO
{

namespace DiscScrubber
{

#define CLUSTER_SIZE 0x8000

static u8* m_FreeTable = nullptr;
static u64 m_FileSize;
static u64 m_BlockCount;
static u32 m_BlockSize;
static int m_BlocksPerCluster;
static bool m_isScrubbing = false;

static std::string m_Filename;
static std::unique_ptr<IVolume> s_disc;

struct SPartitionHeader
{
	u8* Ticket[0x2a4];
	u32 TMDSize;
	u64 TMDOffset;
	u32 CertChainSize;
	u64 CertChainOffset;
	// H3Size is always 0x18000
	u64 H3Offset;
	u64 DataOffset;
	u64 DataSize;
	// TMD would be here
	u64 DOLOffset;
	u64 DOLSize;
	u64 FSTOffset;
	u64 FSTSize;
	u32 ApploaderSize;
	u32 ApploaderTrailerSize;
};
struct SPartition
{
	u32 GroupNumber;
	u32 Number;
	u64 Offset;
	u32 Type;
	SPartitionHeader Header;
};
struct SPartitionGroup
{
	u32 numPartitions;
	u64 PartitionsOffset;
	std::vector<SPartition> PartitionsVec;
};
static SPartitionGroup PartitionGroup[4];


void MarkAsUsed(u64 _Offset, u64 _Size);
void MarkAsUsedE(u64 _PartitionDataOffset, u64 _Offset, u64 _Size);
void ReadFromVolume(u64 _Offset, u32& _Buffer, bool _Decrypt);
void ReadFromVolume(u64 _Offset, u64& _Buffer, bool _Decrypt);
bool ParseDisc();
bool ParsePartitionData(SPartition& _rPartition);


bool SetupScrub(const std::string& filename, int block_size)
{
	bool success = true;
	m_Filename = filename;
	m_BlockSize = block_size;

	if (CLUSTER_SIZE % m_BlockSize != 0)
	{
		ERROR_LOG(DISCIO, "Block size %i is not a factor of 0x8000, scrubbing not possible", m_BlockSize);
		return false;
	}

	m_BlocksPerCluster = CLUSTER_SIZE / m_BlockSize;

	s_disc = CreateVolumeFromFilename(filename);
	if (!s_disc)
		return false;

	m_FileSize = s_disc->GetSize();

	u32 numClusters = (u32)(m_FileSize / CLUSTER_SIZE);

	// Warn if not DVD5 or DVD9 size
	if (numClusters != 0x23048 && numClusters != 0x46090)
		WARN_LOG(DISCIO, "%s is not a standard sized Wii disc! (%x blocks)", filename.c_str(), numClusters);

	// Table of free blocks
	m_FreeTable = new u8[numClusters];
	std::fill(m_FreeTable, m_FreeTable + numClusters, 1);

	// Fill out table of free blocks
	success = ParseDisc();

	// Done with it; need it closed for the next part
	s_disc.reset();
	m_BlockCount = 0;

	// Let's not touch the file if we've failed up to here :p
	if (!success)
		Cleanup();

	m_isScrubbing = success;
	return success;
}

size_t GetNextBlock(File::IOFile& in, u8* buffer)
{
	u64 CurrentOffset = m_BlockCount * m_BlockSize;
	u64 i = CurrentOffset / CLUSTER_SIZE;

	size_t ReadBytes = 0;
	if (m_isScrubbing && m_FreeTable[i])
	{
		DEBUG_LOG(DISCIO, "Freeing 0x%016" PRIx64, CurrentOffset);
		std::fill(buffer, buffer + m_BlockSize, 0xFF);
		in.Seek(m_BlockSize, SEEK_CUR);
		ReadBytes = m_BlockSize;
	}
	else
	{
		DEBUG_LOG(DISCIO, "Used    0x%016" PRIx64, CurrentOffset);
		in.ReadArray(buffer, m_BlockSize, &ReadBytes);
	}

	m_BlockCount++;
	return ReadBytes;
}

void Cleanup()
{
	if (m_FreeTable) delete[] m_FreeTable;
	m_FreeTable = nullptr;
	m_FileSize = 0;
	m_BlockCount = 0;
	m_BlockSize = 0;
	m_BlocksPerCluster = 0;
	m_isScrubbing = false;
}

void MarkAsUsed(u64 _Offset, u64 _Size)
{
	u64 CurrentOffset = _Offset;
	u64 EndOffset = CurrentOffset + _Size;

	DEBUG_LOG(DISCIO, "Marking 0x%016" PRIx64 " - 0x%016" PRIx64 " as used", _Offset, EndOffset);

	while ((CurrentOffset < EndOffset) && (CurrentOffset < m_FileSize))
	{
		m_FreeTable[CurrentOffset / CLUSTER_SIZE] = 0;
		CurrentOffset += CLUSTER_SIZE;
	}
}
// Compensate for 0x400(SHA-1) per 0x8000(cluster)
void MarkAsUsedE(u64 _PartitionDataOffset, u64 _Offset, u64 _Size)
{
	u64 Offset;
	u64 Size;

	Offset = _Offset / 0x7c00;
	Offset = Offset * CLUSTER_SIZE;
	Offset += _PartitionDataOffset;

	Size = _Size / 0x7c00;
	Size = (Size + 1) * CLUSTER_SIZE;

	// Add on the offset in the first block for the case where data straddles blocks
	Size += _Offset % 0x7c00;

	MarkAsUsed(Offset, Size);
}

// Helper functions for reading the BE volume
void ReadFromVolume(u64 _Offset, u32& _Buffer, bool _Decrypt)
{
	s_disc->Read(_Offset, sizeof(u32), (u8*)&_Buffer, _Decrypt);
	_Buffer = Common::swap32(_Buffer);
}
void ReadFromVolume(u64 _Offset, u64& _Buffer, bool _Decrypt)
{
	s_disc->Read(_Offset, sizeof(u32), (u8*)&_Buffer, _Decrypt);
	_Buffer = Common::swap32((u32)_Buffer);
	_Buffer <<= 2;
}

bool ParseDisc()
{
	// Mark the header as used - it's mostly 0s anyways
	MarkAsUsed(0, 0x50000);

	for (int x = 0; x < 4; x++)
	{
		ReadFromVolume(0x40000 + (x * 8) + 0, PartitionGroup[x].numPartitions, false);
		ReadFromVolume(0x40000 + (x * 8) + 4, PartitionGroup[x].PartitionsOffset, false);

		// Read all partitions
		for (u32 i = 0; i < PartitionGroup[x].numPartitions; i++)
		{
			SPartition Partition;

			Partition.GroupNumber = x;
			Partition.Number = i;

			ReadFromVolume(PartitionGroup[x].PartitionsOffset + (i * 8) + 0, Partition.Offset, false);
			ReadFromVolume(PartitionGroup[x].PartitionsOffset + (i * 8) + 4, Partition.Type, false);

			ReadFromVolume(Partition.Offset + 0x2a4, Partition.Header.TMDSize, false);
			ReadFromVolume(Partition.Offset + 0x2a8, Partition.Header.TMDOffset, false);
			ReadFromVolume(Partition.Offset + 0x2ac, Partition.Header.CertChainSize, false);
			ReadFromVolume(Partition.Offset + 0x2b0, Partition.Header.CertChainOffset, false);
			ReadFromVolume(Partition.Offset + 0x2b4, Partition.Header.H3Offset, false);
			ReadFromVolume(Partition.Offset + 0x2b8, Partition.Header.DataOffset, false);
			ReadFromVolume(Partition.Offset + 0x2bc, Partition.Header.DataSize, false);

			PartitionGroup[x].PartitionsVec.push_back(Partition);
		}

		for (auto& rPartition : PartitionGroup[x].PartitionsVec)
		{
			const SPartitionHeader& rHeader = rPartition.Header;

			MarkAsUsed(rPartition.Offset, 0x2c0);

			MarkAsUsed(rPartition.Offset + rHeader.TMDOffset, rHeader.TMDSize);
			MarkAsUsed(rPartition.Offset + rHeader.CertChainOffset, rHeader.CertChainSize);
			MarkAsUsed(rPartition.Offset + rHeader.H3Offset, 0x18000);
			// This would mark the whole (encrypted) data area
			// we need to parse FST and other crap to find what's free within it!
			//MarkAsUsed(rPartition.Offset + rHeader.DataOffset, rHeader.DataSize);

			// Parse Data! This is where the big gain is
			if (!ParsePartitionData(rPartition))
				return false;
		}
	}

	return true;
}

// Operations dealing with encrypted space are done here - the volume is swapped to allow this
bool ParsePartitionData(SPartition& partition)
{
	bool parsed_ok = true;

	// Switch out the main volume temporarily
	std::unique_ptr<IVolume> old_volume;
	s_disc.swap(old_volume);

	// Ready some stuff
	s_disc = CreateVolumeFromFilename(m_Filename, partition.GroupNumber, partition.Number);
	if (s_disc == nullptr)
	{
		ERROR_LOG(DISCIO, "Failed to create volume from file %s", m_Filename.c_str());
		s_disc.swap(old_volume);
		return false;
	}

	std::unique_ptr<IFileSystem> filesystem(CreateFileSystem(s_disc.get()));
	if (!filesystem)
	{
		ERROR_LOG(DISCIO, "Failed to create filesystem for group %d partition %u", partition.GroupNumber, partition.Number);
		parsed_ok = false;
	}
	else
	{
		// Mark things as used which are not in the filesystem
		// Header, Header Information, Apploader
		ReadFromVolume(0x2440 + 0x14, partition.Header.ApploaderSize, true);
		ReadFromVolume(0x2440 + 0x18, partition.Header.ApploaderTrailerSize, true);
		MarkAsUsedE(partition.Offset
			+ partition.Header.DataOffset
			, 0
			, 0x2440
			+ partition.Header.ApploaderSize
			+ partition.Header.ApploaderTrailerSize);

		// DOL
		ReadFromVolume(0x420, partition.Header.DOLOffset, true);
		partition.Header.DOLSize = filesystem->GetBootDOLSize(partition.Header.DOLOffset);
		MarkAsUsedE(partition.Offset
			+ partition.Header.DataOffset
			, partition.Header.DOLOffset
			, partition.Header.DOLSize);

		// FST
		ReadFromVolume(0x424, partition.Header.FSTOffset, true);
		ReadFromVolume(0x428, partition.Header.FSTSize, true);
		MarkAsUsedE(partition.Offset
			+ partition.Header.DataOffset
			, partition.Header.FSTOffset
			, partition.Header.FSTSize);

		// Go through the filesystem and mark entries as used
		for (SFileInfo file : filesystem->GetFileList())
		{
			DEBUG_LOG(DISCIO, "%s", file.m_FullPath.empty() ? "/" : file.m_FullPath.c_str());
			if ((file.m_NameOffset & 0x1000000) == 0)
				MarkAsUsedE(partition.Offset + partition.Header.DataOffset, file.m_Offset, file.m_FileSize);
		}
	}

	// Swap back
	s_disc.swap(old_volume);

	return parsed_ok;
}

} // namespace DiscScrubber

} // namespace DiscIO
