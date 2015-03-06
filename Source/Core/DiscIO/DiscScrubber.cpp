// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

namespace DiscIO
{

namespace DiscScrubber
{

#define CLUSTER_SIZE 0x8000

static u8* m_free_table = nullptr;
static u64 m_file_size;
static u64 m_block_count;
static u32 m_block_size;
static int m_blocks_per_cluster;
static bool m_is_scrubbing = false;

static std::string m_file_name;
static IVolume* m_disc = nullptr;

struct SPartitionHeader
{
	u8* Ticket[0x2a4];
	u32 TMD_size;
	u64 TMD_offset;
	u32 cert_chain_size;
	u64 cert_chain_offset;
	// H3_size is always 0x18000
	u64 H3_offset;
	u64 data_offset;
	u64 data_size;
	// TMD would be here
	u64 DOL_offset;
	u64 DOL_size;
	u64 FST_offset;
	u64 FST_size;
	u32 apploader_size;
	u32 apploader_trailer_size;
};
struct SPartition
{
	u32 group_number;
	u32 number;
	u64 offset;
	u32 type;
	SPartitionHeader header;
};
struct SPartitionGroup
{
	u32 num_partitions;
	u64 partitions_offset;
	std::vector<SPartition> PartitionsVec;
};
static SPartitionGroup PartitionGroup[4];


void MarkAsUsed(u64 _offset, u64 _size);
void MarkAsUsedE(u64 _partition_data_offset, u64 _offset, u64 _size);
void ReadFromVolume(u64 _offset, u64 _length, u32& _buffer, bool _decrypt);
void ReadFromVolume(u64 _offset, u64 _length, u64& _buffer, bool _decrypt);
bool ParseDisc();
bool ParsePartitionData(SPartition& _rPartition);
u32 GetDOLSize(u64 _DOL_offset);


bool SetupScrub(const std::string& filename, int block_size)
{
	bool success = true;
	m_file_name = filename;
	m_block_size = block_size;

	if (CLUSTER_SIZE % m_block_size != 0)
	{
		ERROR_LOG(DISCIO, "Block size %i is not a factor of 0x8000, scrubbing not possible", m_block_size);
		return false;
	}

	m_blocks_per_cluster = CLUSTER_SIZE / m_block_size;

	m_disc = CreateVolumeFromFilename(filename);
	m_file_size = m_disc->GetSize();

	u32 num_clusters = (u32)(m_file_size / CLUSTER_SIZE);

	// Warn if not DVD5 or DVD9 size
	if (num_clusters != 0x23048 && num_clusters != 0x46090)
		WARN_LOG(DISCIO, "%s is not a standard sized Wii disc! (%x blocks)", filename.c_str(), num_clusters);

	// Table of free blocks
	m_free_table = new u8[num_clusters];
	std::fill(m_free_table, m_free_table + num_clusters, 1);

	// Fill out table of free blocks
	success = ParseDisc();
	// Done with it; need it closed for the next part
	delete m_disc;
	m_disc = nullptr;
	m_block_count = 0;

	// Let's not touch the file if we've failed up to here :p
	if (!success)
		Cleanup();

	m_is_scrubbing = success;
	return success;
}

size_t GetNextBlock(File::IOFile& in, u8* buffer)
{
	u64 current_offset = m_block_count * m_block_size;
	u64 i = current_offset / CLUSTER_SIZE;

	size_t ReadBytes = 0;
	if (m_is_scrubbing && m_free_table[i])
	{
		DEBUG_LOG(DISCIO, "Freeing 0x%016" PRIx64, current_offset);
		std::fill(buffer, buffer + m_block_size, 0xFF);
		in.Seek(m_block_size, SEEK_CUR);
		ReadBytes = m_block_size;
	}
	else
	{
		DEBUG_LOG(DISCIO, "Used    0x%016" PRIx64, current_offset);
		in.ReadArray(buffer, m_block_size, &ReadBytes);
	}

	m_block_count++;
	return ReadBytes;
}

void Cleanup()
{
	if (m_free_table) delete[] m_free_table;
	m_free_table = nullptr;
	m_file_size = 0;
	m_block_count = 0;
	m_block_size = 0;
	m_blocks_per_cluster = 0;
	m_is_scrubbing = false;
}

void MarkAsUsed(u64 _offset, u64 _size)
{
	u64 current_offset = _offset;
	u64 end_offset = current_offset + _size;

	DEBUG_LOG(DISCIO, "Marking 0x%016" PRIx64 " - 0x%016" PRIx64 " as used", _offset, end_offset);

	while ((current_offset < end_offset) && (current_offset < m_file_size))
	{
		m_free_table[current_offset / CLUSTER_SIZE] = 0;
		current_offset += CLUSTER_SIZE;
	}
}
// Compensate for 0x400(SHA-1) per 0x8000(cluster)
void MarkAsUsedE(u64 _partition_data_offset, u64 _offset, u64 _size)
{
	u64 offset;
	u64 size;

	offset = _offset / 0x7c00;
	offset = offset * CLUSTER_SIZE;
	offset += _partition_data_offset;

	size = _size / 0x7c00;
	size = (size + 1) * CLUSTER_SIZE;

	// Add on the offset in the first block for the case where data straddles blocks
	size += _offset % 0x7c00;

	MarkAsUsed(offset, size);
}

// Helper functions for reading the BE volume
void ReadFromVolume(u64 _offset, u64 _length, u32& _buffer, bool _decrypt)
{
	m_disc->Read(_offset, _length, (u8*)&_buffer, _decrypt);
	_buffer = Common::swap32(_buffer);
}
void ReadFromVolume(u64 _offset, u64 _length, u64& _buffer, bool _decrypt)
{
	m_disc->Read(_offset, _length, (u8*)&_buffer, _decrypt);
	_buffer = Common::swap32((u32)_buffer);
	_buffer <<= 2;
}

bool ParseDisc()
{
	// Mark the header as used - it's mostly 0s anyways
	MarkAsUsed(0, 0x50000);

	for (int x = 0; x < 4; x++)
	{
		ReadFromVolume(0x40000 + (x * 8) + 0, 4, PartitionGroup[x].num_partitions, false);
		ReadFromVolume(0x40000 + (x * 8) + 4, 4, PartitionGroup[x].partitions_offset, false);

		// Read all partitions
		for (u32 i = 0; i < PartitionGroup[x].num_partitions; i++)
		{
			SPartition Partition;

			Partition.group_number = x;
			Partition.number = i;

			ReadFromVolume(PartitionGroup[x].partitions_offset + (i * 8) + 0, 4, Partition.offset, false);
			ReadFromVolume(PartitionGroup[x].partitions_offset + (i * 8) + 4, 4, Partition.type, false);

			ReadFromVolume(Partition.offset + 0x2a4, 4, Partition.header.TMD_size, false);
			ReadFromVolume(Partition.offset + 0x2a8, 4, Partition.header.TMD_offset, false);
			ReadFromVolume(Partition.offset + 0x2ac, 4, Partition.header.cert_chain_size, false);
			ReadFromVolume(Partition.offset + 0x2b0, 4, Partition.header.cert_chain_offset, false);
			ReadFromVolume(Partition.offset + 0x2b4, 4, Partition.header.H3_offset, false);
			ReadFromVolume(Partition.offset + 0x2b8, 4, Partition.header.data_offset, false);
			ReadFromVolume(Partition.offset + 0x2bc, 4, Partition.header.data_size, false);

			PartitionGroup[x].PartitionsVec.push_back(Partition);
		}

		for (auto& rPartition : PartitionGroup[x].PartitionsVec)
		{
			const SPartitionHeader& rHeader = rPartition.header;

			MarkAsUsed(rPartition.offset, 0x2c0);

			MarkAsUsed(rPartition.offset + rHeader.TMD_offset, rHeader.TMD_size);
			MarkAsUsed(rPartition.offset + rHeader.cert_chain_offset, rHeader.cert_chain_size);
			MarkAsUsed(rPartition.offset + rHeader.H3_offset, 0x18000);
			// This would mark the whole (encrypted) data area
			// we need to parse FST and other crap to find what's free within it!
			//MarkAsUsed(rPartition.Offset + rHeader.data_offset, rHeader.data_size);

			// Parse Data! This is where the big gain is
			if (!ParsePartitionData(rPartition))
				return false;
		}
	}

	return true;
}

// Operations dealing with encrypted space are done here - the volume is swapped to allow this
bool ParsePartitionData(SPartition& _rPartition)
{
	bool ParsedOK = true;

	// Switch out the main volume temporarily
	IVolume *OldVolume = m_disc;

	// Ready some stuff
	m_disc = CreateVolumeFromFilename(m_file_name, _rPartition.group_number, _rPartition.number);
	std::unique_ptr<IFileSystem> filesystem(CreateFileSystem(m_disc));

	if (!filesystem)
	{
		ERROR_LOG(DISCIO, "Failed to create filesystem for group %d partition %u", _rPartition.group_number, _rPartition.number);
		ParsedOK = false;
	}
	else
	{
		std::vector<const SFileInfo *> Files;
		size_t numFiles = filesystem->GetFileList(Files);

		// Mark things as used which are not in the filesystem
		// Header, Header Information, Apploader
		ReadFromVolume(0x2440 + 0x14, 4, _rPartition.header.apploader_size, true);
		ReadFromVolume(0x2440 + 0x18, 4, _rPartition.header.apploader_trailer_size, true);
		MarkAsUsedE(_rPartition.offset
			+ _rPartition.header.data_offset
			, 0
			, 0x2440
			+ _rPartition.header.apploader_size
			+ _rPartition.header.apploader_trailer_size);

		// DOL
		ReadFromVolume(0x420, 4, _rPartition.header.DOL_offset, true);
		_rPartition.header.DOL_size = GetDOLSize(_rPartition.header.DOL_offset);
		MarkAsUsedE(_rPartition.offset
			+ _rPartition.header.data_offset
			, _rPartition.header.DOL_offset
			, _rPartition.header.DOL_size);

		// FST
		ReadFromVolume(0x424, 4, _rPartition.header.FST_offset, true);
		ReadFromVolume(0x428, 4, _rPartition.header.FST_size, true);
		MarkAsUsedE(_rPartition.offset
			+ _rPartition.header.data_offset
			, _rPartition.header.FST_offset
			, _rPartition.header.FST_size);

		// Go through the filesystem and mark entries as used
		for (size_t currentFile = 0; currentFile < numFiles; currentFile++)
		{
			DEBUG_LOG(DISCIO, "%s", currentFile ? (*Files.at(currentFile)).m_full_path.c_str() : "/");
			// Just 1byte for directory? - it will end up reserving a cluster this way
			if ((*Files.at(currentFile)).m_name_offset & 0x1000000)
				MarkAsUsedE(_rPartition.offset
				+ _rPartition.header.data_offset
				, (*Files.at(currentFile)).m_offset, 1);
			else
				MarkAsUsedE(_rPartition.offset
				+ _rPartition.header.data_offset
				, (*Files.at(currentFile)).m_offset, (*Files.at(currentFile)).m_file_size);
		}
	}

	// Swap back
	delete m_disc;
	m_disc = OldVolume;

	return ParsedOK;
}

u32 GetDOLSize(u64 _DOL_offset)
{
	u32 offset = 0, size = 0, max = 0;

	// Iterate through the 7 code segments
	for (u8 i = 0; i < 7; i++)
	{
		ReadFromVolume(_DOL_offset + 0x00 + i * 4, 4, offset, true);
		ReadFromVolume(_DOL_offset + 0x90 + i * 4, 4, size, true);
		if (offset + size > max)
			max = offset + size;
	}

	// Iterate through the 11 data segments
	for (u8 i = 0; i < 11; i++)
	{
		ReadFromVolume(_DOL_offset + 0x1c + i * 4, 4, offset, true);
		ReadFromVolume(_DOL_offset + 0xac + i * 4, 4, size, true);
		if (offset + size > max)
			max = offset + size;
	}

	return max;
}

} // namespace DiscScrubber

} // namespace DiscIO
