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

#include "stdafx.h"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "Common.h"
#include "CompressedBlob.h"
#include "Hash.h"

#ifdef _WIN32
#include "../../../../Externals/zlib/zlib.h"
#else
// TODO: Include generic zlib.h
#include "../../../../Externals/zlib/zlib.h"
#endif

#ifdef _WIN32
#define fseek _fseeki64
#endif


namespace DiscIO
{

CompressedBlobReader::CompressedBlobReader(const char *filename)
{
	file = fopen(filename, "rb");
	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	fread(&header, sizeof(CompressedBlobHeader), 1, file);

	SetSectorSize(header.block_size);

	// cache block pointers and hashes
	block_pointers = new u64[header.num_blocks];
	fread(block_pointers, sizeof(u64), header.num_blocks, file);
	hashes = new u32[header.num_blocks];
	fread(hashes, sizeof(u32), header.num_blocks, file);

	data_offset = (sizeof(CompressedBlobHeader))
		+ (sizeof(u64)) * header.num_blocks   // skip block pointers
		+ (sizeof(u32)) * header.num_blocks;  // skip hashes

	// A compressed block is never ever longer than a decompressed block, so just header.block_size should be fine.
	// I still add some safety margin.
	zlib_buffer_size = header.block_size + 64;
	zlib_buffer = new u8[zlib_buffer_size];
	memset(zlib_buffer, 0, zlib_buffer_size);
}

CompressedBlobReader* CompressedBlobReader::Create(const char* filename)
{
	if (IsCompressedBlob(filename))
		return new CompressedBlobReader(filename);
	else
		return 0;
}

CompressedBlobReader::~CompressedBlobReader()
{
	delete [] zlib_buffer;
	delete [] block_pointers;
	delete [] hashes;
	fclose(file);
	file = 0;
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
	
	fseek(file, offset, SEEK_SET);
	fread(zlib_buffer, 1, comp_block_size, file);

	u8* source = zlib_buffer;
	u8* dest = out_ptr;

	// First, check hash.
	u32 block_hash = HashAdler32(source, comp_block_size);
	if (block_hash != hashes[block_num])
		PanicAlert("Hash of block %i is %08x instead of %08x. Your ISO is corrupt.",
		           block_num, block_hash, hashes[block_num]);

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
			PanicAlert("Failure reading block %i - out of data and not at end.", block_num);
		}
		inflateEnd(&z);
		if (uncomp_size != header.block_size)
			PanicAlert("Wrong block size");
	}
}

bool CompressFileToBlob(const char* infile, const char* outfile, u32 sub_type,
						int block_size, CompressCB callback, void* arg)
{
	if (IsCompressedBlob(infile))
	{
		PanicAlert("%s is already compressed! Cannot compress it further.", infile);
		return false;
	}

	FILE* inf = fopen(infile, "rb");
	if (!inf)
		return false;

	FILE* f = fopen(outfile, "wb");
	if (!f)
		return false;

	callback("Files opened, ready to compress.", 0, arg);

	fseek(inf, 0, SEEK_END);
	int insize = ftell(inf);
	fseek(inf, 0, SEEK_SET);
	CompressedBlobHeader header;
	header.magic_cookie = kBlobCookie;
	header.sub_type   = sub_type;
	header.block_size = block_size;
	header.data_size  = insize;

	// round upwards!
	header.num_blocks = (u32)((header.data_size + (block_size - 1)) / block_size);

	u64* offsets = new u64[header.num_blocks];
	u32* hashes = new u32[header.num_blocks];
	u8* out_buf = new u8[block_size];
	u8* in_buf = new u8[block_size];

	// seek past the header (we will write it at the end)
	fseek(f, sizeof(CompressedBlobHeader), SEEK_CUR);
	// seek past the offset and hash tables (we will write them at the end)
	fseek(f, (sizeof(u64) + sizeof(u32)) * header.num_blocks, SEEK_CUR);

	// Now we are ready to write compressed data!
	u64 position = 0;
	int num_compressed = 0;
	int num_stored = 0;
	for (u32 i = 0; i < header.num_blocks; i++)
	{
		if (i % (header.num_blocks / 1000) == 0)
		{
			u64 inpos = ftell(inf);
			int ratio = 0;
			if (inpos != 0)
				ratio = (int)(100 * position / inpos);
			char temp[512];
			sprintf(temp, "%i of %i blocks. compression ratio %i%%", i, header.num_blocks, ratio);
			callback(temp, (float)i / (float)header.num_blocks, arg);
		}

		offsets[i] = position;
		// u64 start = i * header.block_size;
		// u64 size = header.block_size;
		memset(in_buf, 0, header.block_size);
		fread(in_buf, header.block_size, 1, inf);
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
			PanicAlert("Deflate failed");
			goto cleanup;
		}

		int status = deflate(&z, Z_FINISH);
		int comp_size = block_size - z.avail_out;
		if ((status != Z_STREAM_END) || (z.avail_out < 10))
		{
			//PanicAlert("%i %i Store %i", i*block_size, position, comp_size);
			// let's store uncompressed
			offsets[i] |= 0x8000000000000000ULL;
			fwrite(in_buf, block_size, 1, f);
			hashes[i] = HashAdler32(in_buf, block_size);
			position += block_size;
			num_stored++;
		}
		else
		{
			// let's store compressed
			//PanicAlert("Comp %i to %i", block_size, comp_size);
			fwrite(out_buf, comp_size, 1, f);
			hashes[i] = HashAdler32(out_buf, comp_size);
			position += comp_size;
			num_compressed++;
		}

		deflateEnd(&z);
	}

	header.compressed_data_size = position;

	// Okay, go back and fill in headers
	fseek(f, 0, SEEK_SET);
	fwrite(&header, sizeof(header), 1, f);
	fwrite(offsets, sizeof(u64), header.num_blocks, f);
	fwrite(hashes, sizeof(u32), header.num_blocks, f);

cleanup:
	// Cleanup
	delete[] in_buf;
	delete[] out_buf;
	delete[] offsets;
	fclose(f);
	fclose(inf);
	callback("Done compressing disc image.", 1.0f, arg);
	return true;
}

bool DecompressBlobToFile(const char* infile, const char* outfile, CompressCB callback, void* arg)
{
	if (!IsCompressedBlob(infile))
	{
		PanicAlert("File not compressed");
		return false;
	}

	CompressedBlobReader* reader = CompressedBlobReader::Create(infile);
	if (!reader) return false;

	FILE* f = fopen(outfile, "wb");
	const CompressedBlobHeader &header = reader->GetHeader();
	u8* buffer = new u8[header.block_size];

	for (u64 i = 0; i < header.num_blocks; i++)
	{
		if (i % (header.num_blocks / 100) == 0)
		{
			callback("Unpacking", (float)i / (float)header.num_blocks, arg);
		}
		reader->Read(i * header.block_size, header.block_size, buffer);
		fwrite(buffer, header.block_size, 1, f);
	}

	delete reader;
	delete[] buffer;

#ifdef _WIN32
	// ector: _chsize sucks, not 64-bit safe
	// F|RES: changed to _chsize_s. i think it is 64-bit safe
	_chsize_s(_fileno(f), (long)header.data_size);
#else
	ftruncate(fileno(f), header.data_size);
#endif
	fclose(f);
	return true;
}

bool IsCompressedBlob(const char* filename)
{
	FILE* f = fopen(filename, "rb");

	if (!f)
		return false;

	CompressedBlobHeader header;
	fread(&header, sizeof(header), 1, f);
	fclose(f);
	return header.magic_cookie == kBlobCookie;
}

}  // namespace
