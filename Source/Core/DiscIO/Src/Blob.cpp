#ifdef WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "stdafx.h"
#include "../../../../Externals/zlib/zlib.h"

#include "Blob.h"
#include "MappedFile.h"

namespace DiscIO
{
const u32 kBlobCookie = 0xB10BC001;

// A blob file structure:
// BlobHeader
// u64 offsetsToBlocks[n], top bit specifies whether the block is compressed, or not.
// compressed data

// Blocks that won't compress to less than 97% of the original size are stored as-is.

struct BlobHeader // 32 bytes
{
	u32 magic_cookie; //0xB10BB10B
	u32 sub_type; // gc image, whatever
	u64 compressed_data_size;
	u64 data_size;
	u32 block_size;
	u32 num_blocks;
};

#ifdef _WIN32
class PlainFileReader
	: public IBlobReader
{
	HANDLE hFile;
	s64 size;
	private:
		PlainFileReader(HANDLE hFile_)
		{
			hFile = hFile_;
			DWORD size_low, size_high;
			size_low = GetFileSize(hFile, &size_high);
			size = ((u64)size_low) | ((u64)size_high << 32);
		}

	public:
		static PlainFileReader* Create(const char* filename)
		{
			HANDLE hFile = CreateFile(
				filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL
				);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				return new PlainFileReader(nFile);
			}
			return 0;
		}


		~PlainFileReader()
		{
			CloseHandle(hFile);
		}


		u64 GetDataSize() const {return(size);}


		u64 GetRawSize() const {return(size);}


		bool Read(u64 offset, u64 size, u8* out_ptr)
		{
			LONG offset_high = (LONG)(offset >> 32);
			SetFilePointer(hFile, (DWORD)(offset & 0xFFFFFFFF), &offset_high, FILE_BEGIN);

			if (size >= 0x100000000ULL)
			{
				return(false); // WTF, does windows really have this limitation?
			}

			DWORD unused;

			if (!ReadFile(hFile, out_ptr, DWORD(size & 0xFFFFFFFF), &unused, NULL))
			{
				return(false);
			}
			else
			{
				return(true);
			}
		}
};

#else // linux, 64-bit. We do not yet care about linux32
// Not optimal - will keep recreating mappings.
class PlainFileReader
	: public IBlobReader
{
	FILE* file_;
	s64 size;
	private:
		PlainFileReader(FILE* file__)
		{
			file_ = file__;
			#if 0
				fseek64(file_, 0, SEEK_END);
			#else
				fseek(file_, 0, SEEK_END); // I don't have fseek64 with gcc 4.3
			#endif
			size = ftell(file_);
			fseek(file_, 0, SEEK_SET);
		}


	public:
		static PlainFileReader* Create(const char* filename)
		{
			FILE* file_ = fopen(filename, "rb");
			if (file_)
			{
				return new PlainFileReader(file_);
			}
			return 0;
		}


		~PlainFileReader()
		{
			fclose(file_);
		}


		u64 GetDataSize() const
		{
			return(size);
		}


		u64 GetRawSize() const
		{
			return(size);
		}


		bool Read(u64 offset, u64 nbytes, u8* out_ptr)
		{
			fseek(file_, offset, SEEK_SET);
			fread(out_ptr, nbytes, 1, file_);
			return(true);
		}
};
#endif



class CompressedBlobReader
	: public IBlobReader
{
	enum { CACHE_SIZE = 32 };

	BlobHeader header;
	u64* block_pointers;
	int data_offset;
	u8* cache[CACHE_SIZE];
	u64 cache_tags[CACHE_SIZE];
	int cache_age[CACHE_SIZE];
	u64 counter;
	Common::IMappedFile* mapped_file;

	private:
		CompressedBlobReader(Common::IMappedFile* mapped_file_)
		{
			mapped_file = mapped_file_;
			counter = 0;

			u8* start = mapped_file->Lock(0, sizeof(BlobHeader));
			memcpy(&header, start, sizeof(BlobHeader));
			mapped_file->Unlock(start);

			block_pointers = (u64*)mapped_file->Lock(sizeof(BlobHeader), sizeof(u64) * header.num_blocks);
			data_offset = sizeof(BlobHeader) + sizeof(u64) * header.num_blocks;

			for (int i = 0; i < CACHE_SIZE; i++)
			{
				cache[i] = new u8[header.block_size];
				cache_tags[i] = (u64)(s64) - 1;
			}
		}


	public:
		static CompressedBlobReader* Create(const char* filename)
		{
			Common::IMappedFile* mapped_file =
				Common::IMappedFile::CreateMappedFile();
			if (mapped_file)
			{
				bool ok = mapped_file->Open(filename);
				if (ok)
				{
					return new CompressedBlobReader(mapped_file);
				}
				else
				{
					delete mapped_file;
				}
			}
			return 0;
		}


		~CompressedBlobReader()
		{
			for (int i = 0; i < CACHE_SIZE; i++)
			{
				delete[] cache[i];
			}

			mapped_file->Unlock((u8*)block_pointers);
			mapped_file->Close();
			delete mapped_file;
		}


		const BlobHeader& GetHeader() const
		{
			return(header);
		}


		u64 GetDataSize() const
		{
			return(header.data_size);
		}


		u64 GetRawSize() const
		{
			return(mapped_file->GetSize());
		}


		u64 GetBlockCompressedSize(u64 block_num) const
		{
			u64 start = block_pointers[block_num];

			if (block_num != header.num_blocks - 1)
			{
				return(block_pointers[block_num + 1] - start);
			}
			else
			{
				return(header.compressed_data_size - start);
			}
		}


		// IMPORTANT: Calling this function invalidates all earlier pointers gotten from this function.
		u8* GetBlock(u64 block_num)
		{
			if (cache_tags[0] != block_num)
			{
				cache_tags[0] = block_num;
				//PanicAlert("here2");
				// let's begin with a super naive implementation.
				// let's just use the start of the cache for now.
				bool uncompressed   = false;
				u32 comp_block_size = (u32)GetBlockCompressedSize(block_num);
				u64 offset = block_pointers[block_num] + data_offset;

				if (offset & (1ULL << 63))
				{
					if (comp_block_size != header.block_size)
					{
						PanicAlert("Uncompressed block with wrong size");
					}

					uncompressed = true;
					offset &= ~(1ULL << 63);
				}

				u8* source = mapped_file->Lock(offset, comp_block_size);
				u8* dest = cache[0];

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
						//PanicAlert("Failure reading block %i", block_num);
					}

					if (uncomp_size != header.block_size)
					{
						PanicAlert("Wrong block size");
					}

					inflateEnd(&z);
				}

				mapped_file->Unlock(source);
			}

			//PanicAlert("here3");
			return(cache[0]);
		}


		bool Read(u64 offset, u64 size, u8* out_ptr)
		{
			u64 startingBlock = offset / header.block_size;
			u64 remain = size;

			int positionInBlock = (int)(offset % header.block_size);
			u64 block = startingBlock;

			while (remain > 0)
			{
				u8* data = GetBlock(block);

				if (!data)
				{
					return(false);
				}

				u32 toCopy = header.block_size - positionInBlock;

				if (toCopy >= remain)
				{
					// yay, we are done!
					memcpy(out_ptr, data + positionInBlock, (size_t)remain);
					return(true);
				}
				else
				{
					memcpy(out_ptr, data + positionInBlock, toCopy);
					out_ptr += toCopy;
					remain -= toCopy;
					positionInBlock = 0;
					block++;
				}
			}

			PanicAlert("here4");
			return(true);
		}
};



bool CompressFileToBlob(const char* infile, const char* outfile, u32 sub_type, int block_size,
		CompressCB callback, void* arg)
{
	//Common::IMappedFile *in = Common::IMappedFile::CreateMappedFile();
	if (IsCompressedBlob(infile))
	{
		PanicAlert("%s is already compressed!", infile);
		return(false);
	}

	FILE* inf = fopen(infile, "rb");

	if (!inf)
	{
		return(false);
	}

	FILE* f = fopen(outfile, "wb");

	if (!f)
	{
		return(false);
	}

	callback("Files opened, ready to compress.", 0, arg);

	fseek(inf, 0, SEEK_END);
	int insize = ftell(inf);
	fseek(inf, 0, SEEK_SET);
	BlobHeader header;
	header.magic_cookie = kBlobCookie;
	header.sub_type   = sub_type;
	header.block_size = block_size;
	header.data_size  = insize;

	// round upwards!
	header.num_blocks = (u32)((header.data_size + (block_size - 1)) / block_size);

	u64* offsets = new u64[header.num_blocks];
	u8* out_buf = new u8[block_size];
	u8* in_buf = new u8[block_size];

	// seek past the header (we will write it at the end)
	fseek(f, sizeof(BlobHeader), SEEK_CUR);
	// seek past the offset table (we will write it at the end)
	fseek(f, sizeof(u64) * header.num_blocks, SEEK_CUR);
	// Now we are ready to write compressed data!
	u64 position = 0;
	int num_compressed = 0;
	int num_stored = 0;

	for (u32 i = 0; i < header.num_blocks; i++)
	{
		if (i % (header.num_blocks / 100) == 0)
		{
			u64 inpos = ftell(inf);
			int ratio = (int)(100 * position / inpos);
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
			position += block_size;
			num_stored++;
		}
		else
		{
			// let's store compressed
			//PanicAlert("Comp %i to %i", block_size, comp_size);
			fwrite(out_buf, comp_size, 1, f);
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

cleanup:
	// Cleanup
	delete[] in_buf;
	delete[] out_buf;
	delete[] offsets;
	fclose(f);
	fclose(inf);
	callback("Done.", 1.0f, arg);

	return(true);
}


bool DecompressBlobToFile(const char* infile, const char* outfile,
		CompressCB callback, void* arg)
{
	if (!IsCompressedBlob(infile))
	{
		PanicAlert("File not compressed");
		return(false);
	}

	CompressedBlobReader* reader = CompressedBlobReader::Create(infile);
	if (!reader) return false;

	FILE* f = fopen(outfile, "wb");
	const BlobHeader& header = reader->GetHeader();
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
	// TODO(ector): _chsize sucks, not 64-bit safe
	// F|RES: changed to _chsize_s. i think it is 64-bit safe
	_chsize_s(_fileno(f), (long)header.data_size);
#else
	ftruncate(fileno(f), header.data_size);
#endif
	fclose(f);
	return(true);
}


bool IsCompressedBlob(const char* filename)
{
	FILE* f = fopen(filename, "rb");

	if (!f)
	{
		return(0);
	}

	BlobHeader header;
	fread(&header, sizeof(header), 1, f);
	fclose(f);
	return(header.magic_cookie == kBlobCookie);
}


IBlobReader* CreateBlobReader(const char* filename)
{
	return IsCompressedBlob(filename)
		? static_cast<IBlobReader*>(CompressedBlobReader::Create(filename))
		: static_cast<IBlobReader*>(PlainFileReader::Create(filename));
}

} // namespace
