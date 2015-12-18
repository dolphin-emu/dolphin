// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// DiscScrubber removes the garbage data from discs (currently Wii only) which
// is on the disc due to encryption

// It could be adapted to GameCube discs, but the gain is most likely negligible,
// and having 1:1 backups of discs is always nice when they are reasonably sized

// Note: the technique is inspired by Wiiscrubber, but much simpler - intentionally :)

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"

namespace File { class IOFile; }

namespace DiscIO
{

class IVolume;

class DiscScrubber final
{
public:
	DiscScrubber();
	~DiscScrubber();

	bool SetupScrub(const std::string& filename, int block_size);
	size_t GetNextBlock(File::IOFile& in, u8* buffer);

private:
	struct SPartitionHeader final
	{
		u8* ticket[0x2a4];
		u32 tmd_size;
		u64 tmd_offset;
		u32 certificate_chain_size;
		u64 certificate_chain_offset;
		// H3Size is always 0x18000
		u64 h3_offset;
		u64 data_offset;
		u64 data_size;
		// TMD would be here
		u64 dol_offset;
		u64 dol_size;
		u64 fst_offset;
		u64 fst_size;
		u32 apploader_size;
		u32 apploader_trailer_size;
	};

	struct SPartition final
	{
		u32 group_number;
		u32 number;
		u64 offset;
		u32 type;
		SPartitionHeader header;
	};

	struct SPartitionGroup final
	{
		u32 num_partitions;
		u64 partitions_offset;
		std::vector<SPartition> partitions_vector;
	};

	void MarkAsUsed(u64 offset, u64 size);
	void MarkAsUsedE(u64 partition_data_offset, u64 input_offset, u64 input_size);
	bool ReadFromVolume(u64 offset, u32& buffer, bool decrypt);
	bool ReadFromVolume(u64 offset, u64& buffer, bool decrypt);
	bool ParseDisc();
	bool ParsePartitionData(SPartition& partition);

	static constexpr u32 CLUSTER_SIZE = 0x8000;

	std::string m_filename;
	std::unique_ptr<IVolume> m_disc;

	std::array<SPartitionGroup, 4> m_partition_group{};

	std::vector<u8> m_free_table;
	u64 m_filesize = 0;
	u64 m_block_count = 0;
	u32 m_block_size = 0;
	int m_blocks_per_cluster = 0;
	bool m_is_scrubbing = false;
};

} // namespace DiscIO
