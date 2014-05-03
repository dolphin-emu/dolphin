// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>
#include <zlib.h>

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "DiscIO/Blob.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/DiscScrubber.h"


namespace DiscIO
{

CompressedBlobReader::CompressedBlobReader(const std::string& filename) : file_name(filename)
{
	m_file.Open(filename, "rb");
	file_size = File::GetSize(filename);
	m_file.ReadArray(&header, 1);

	SetSectorSize(header.block_size);

	// cache block pointers and hashes
	block_pointers = new u64[header.num_blocks];
	m_file.ReadArray(block_pointers, header.num_blocks);
	hashes = new u32[header.num_blocks];
	m_file.ReadArray(hashes, header.num_blocks);

	data_offset = (sizeof(CompressedBlobHeader))
		+ (sizeof(u64)) * header.num_blocks   // skip block pointers
		+ (sizeof(u32)) * header.num_blocks;  // skip hashes

	// A compressed block is never ever longer than a decompressed block, so just header.block_size should be fine.
	// I still add some safety margin.
	zlib_buffer_size = header.block_size + 64;
	zlib_buffer = new u8[zlib_buffer_size];
	memset(zlib_buffer, 0, zlib_buffer_size);
}

CompressedBlobReader* CompressedBlobReader::Create(const std::string& filename)
{
	if (IsCompressedBlob(filename))
		return new CompressedBlobReader(filename);
	else
		return nullptr;
}

CompressedBlobReader::~CompressedBlobReader()
{
	delete [] zlib_buffer;
	delete [] block_pointers;
	delete [] hashes;
}

// IMPORTANT: Calling this function invalidates all earlier pointers gotten from this function.
u64 CompressedBlobReader::GetBlockCompressedSize(u64 block_num) const
{
	u64 start = block_pointers[block_num];
	if (block_num < header.num_blocks - 1)
		return block_pointers[block_num + 1] - start;
	else if (block_num == header.num_blocks - 1)
		return header.compressed_data_size - start;
	else
		PanicAlert("GetBlockCompressedSize - illegal block number %i", (int)block_num);
	return 0;
}

void CompressedBlobReader::GetBlock(u64 block_num, u8 *out_ptr)
{
	bool uncompressed = false;
	u32 comp_block_size = (u32)GetBlockCompressedSize(block_num);
	u64 offset = block_pointers[block_num] + data_offset;

	if (offset & (1ULL << 63))
	{
		if (comp_block_size != header.block_size)
			PanicAlert("Uncompressed block with wrong size");
		uncompressed = true;
		offset &= ~(1ULL << 63);
	}

	// clear unused part of zlib buffer. maybe this can be deleted when it works fully.
	memset(zlib_buffer + comp_block_size, 0, zlib_buffer_size - comp_block_size);

	m_file.Seek(offset, SEEK_SET);
	m_file.ReadBytes(zlib_buffer, comp_block_size);

	u8* source = zlib_buffer;
	u8* dest = out_ptr;

	// First, check hash.
	u32 block_hash = HashAdler32(source, comp_block_size);
	if (block_hash != hashes[block_num])
		PanicAlert("Hash of block %" PRIu64 " is %08x instead of %08x.\n"
		           "Your ISO, %s, is corrupt.",
		           block_num, block_hash, hashes[block_num],
		           file_name.c_str());

	if (uncompressed)
	{
		memcpy(dest, source, comp_block_size);
	}
	else
	{
		z_stream z;
		memset(&z, 0, sizeof(z));
		z.next_in  = source;
		z.avail_in = comp_block_size;
		if (z.avail_in > header.block_size)
		{
			PanicAlert("We have a problem");
		}
		z.next_out  = dest;
		z.avail_out = header.block_size;
		inflateInit(&z);
		int status = inflate(&z, Z_FULL_FLUSH);
		u32 uncomp_size = header.block_size - z.avail_out;
		if (status != Z_STREAM_END)
		{
			// this seem to fire wrongly from time to time
			// to be sure, don't use compressed isos :P
			PanicAlert("Failure reading block %" PRIu64 " - out of data and not at end.", block_num);
		}
		inflateEnd(&z);
		if (uncomp_size != header.block_size)
			PanicAlert("Wrong block size");
	}
}

bool CompressFileToBlob(const std::string& infile, const std::string& outfile, u32 sub_type,
						int block_size, CompressCB callback, void* arg)
{
	bool scrubbing = false;

	if (IsCompressedBlob(infile))
	{
		PanicAlertT("%s is already compressed! Cannot compress it further.", infile.c_str());
		return false;
	}

	if (sub_type == 1)
	{
		if (!DiscScrubber::SetupScrub(infile, block_size))
		{
			PanicAlertT("%s failed to be scrubbed. Probably the image is corrupt.", infile.c_str());
			return false;
		}

		scrubbing = true;
	}

	File::IOFile inf(infile, "rb");
	File::IOFile f(outfile, "wb");

	if (!f || !inf)
		return false;

	callback("Files opened, ready to compress.", 0, arg);

	CompressedBlobHeader header;
	header.magic_cookie = kBlobCookie;
	header.sub_type   = sub_type;
	header.block_size = block_size;
	header.data_size  = File::GetSize(infile);

	// round upwards!
	header.num_blocks = (u32)((header.data_size + (block_size - 1)) / block_size);

	u64* offsets = new u64[header.num_blocks];
	u32* hashes = new u32[header.num_blocks];
	u8* out_buf = new u8[block_size];
	u8* in_buf = new u8[block_size];

	// seek past the header (we will write it at the end)
	f.Seek(sizeof(CompressedBlobHeader), SEEK_CUR);
	// seek past the offset and hash tables (we will write them at the end)
	f.Seek((sizeof(u64) + sizeof(u32)) * header.num_blocks, SEEK_CUR);

	// Now we are ready to write compressed data!
	u64 position = 0;
	int num_compressed = 0;
	int num_stored = 0;
	int progress_monitor = std::max<int>(1, header.num_blocks / 1000);

	for (u32 i = 0; i < header.num_blocks; i++)
	{
		if (i % progress_monitor == 0)
		{
			const u64 inpos = inf.Tell();
			int ratio = 0;
			if (inpos != 0)
				ratio = (int)(100 * position / inpos);
			char temp[512];
			sprintf(temp, "%i of %i blocks. Compression ratio %i%%", i, header.num_blocks, ratio);
			callback(temp, (float)i / (float)header.num_blocks, arg);
		}

		offsets[i] = position;
		// u64 start = i * header.block_size;
		// u64 size = header.block_size;
		std::fill(in_buf, in_buf + header.block_size, 0);
		if (scrubbing)
			DiscScrubber::GetNextBlock(inf, in_buf);
		else
			inf.ReadBytes(in_buf, header.block_size);
		z_stream z;
		memset(&z, 0, sizeof(z));
		z.zalloc = Z_NULL;
		z.zfree  = Z_NULL;
		z.opaque = Z_NULL;
		z.next_in   = in_buf;
		z.avail_in  = header.block_size;
		z.next_out  = out_buf;
		z.avail_out = block_size;
		int retval = deflateInit(&z, 9);

		if (retval != Z_OK)
		{
			ERROR_LOG(DISCIO, "Deflate failed");
			goto cleanup;
		}

		int status = deflate(&z, Z_FINISH);
		int comp_size = block_size - z.avail_out;
		if ((status != Z_STREAM_END) || (z.avail_out < 10))
		{
			//PanicAlert("%i %i Store %i", i*block_size, position, comp_size);
			// let's store uncompressed
			offsets[i] |= 0x8000000000000000ULL;
			f.WriteBytes(in_buf, block_size);
			hashes[i] = HashAdler32(in_buf, block_size);
			position += block_size;
			num_stored++;
		}
		else
		{
			// let's store compressed
			//PanicAlert("Comp %i to %i", block_size, comp_size);
			f.WriteBytes(out_buf, comp_size);
			hashes[i] = HashAdler32(out_buf, comp_size);
			position += comp_size;
			num_compressed++;
		}

		deflateEnd(&z);
	}

	header.compressed_data_size = position;

	// Okay, go back and fill in headers
	f.Seek(0, SEEK_SET);
	f.WriteArray(&header, 1);
	f.WriteArray(offsets, header.num_blocks);
	f.WriteArray(hashes, header.num_blocks);

cleanup:
	// Cleanup
	delete[] in_buf;
	delete[] out_buf;
	delete[] offsets;
	delete[] hashes;

	DiscScrubber::Cleanup();
	callback("Done compressing disc image.", 1.0f, arg);
	return true;
}

bool DecompressBlobToFile(const std::string& infile, const std::string& outfile, CompressCB callback, void* arg)
{
	if (!IsCompressedBlob(infile))
	{
		PanicAlertT("File not compressed");
		return false;
	}

	CompressedBlobReader* reader = CompressedBlobReader::Create(infile);
	if (!reader)
		return false;

	File::IOFile f(outfile, "wb");
	if (!f)
	{
		delete reader;
		return false;
	}

	const CompressedBlobHeader &header = reader->GetHeader();
	u8* buffer = new u8[header.block_size];
	int progress_monitor = std::max<int>(1, header.num_blocks / 100);

	for (u64 i = 0; i < header.num_blocks; i++)
	{
		if (i % progress_monitor == 0)
		{
			callback("Unpacking", (float)i / (float)header.num_blocks, arg);
		}
		reader->Read(i * header.block_size, header.block_size, buffer);
		f.WriteBytes(buffer, header.block_size);
	}

	delete[] buffer;

	f.Resize(header.data_size);

	delete reader;

	return true;
}

bool IsCompressedBlob(const std::string& filename)
{
	File::IOFile f(filename, "rb");

	CompressedBlobHeader header;
	return f.ReadArray(&header, 1) && (header.magic_cookie == kBlobCookie);
}

}  // namespace
