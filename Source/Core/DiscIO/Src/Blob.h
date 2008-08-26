#ifndef _BLOB_H
#define _BLOB_H

/*
   Code not big-endian safe.

   BLOB

   Blobs are Binary Large OBjects. For example, a typical DVD image.
   Often, you may want to store these things in a highly compressed format, but still
   allow random access.

   Always read your BLOBs using an interface returned by CreateBlobReader(). It will
   detect whether the file is a compressed blob, or just a big hunk of data, and
   automatically do the right thing.

   To create new BLOBs, use CompressFileToBlob.

   Right now caching of decompressed chunks doesn't work, so it's a bit slow.
*/

#include "Common.h"

namespace DiscIO
{
class IBlobReader
{
	public:

		virtual ~IBlobReader() {}


		virtual u64 GetRawSize() const  = 0;
		virtual u64 GetDataSize() const = 0;
		virtual bool Read(u64 offset, u64 size, u8* out_ptr) = 0;


	protected:

		IBlobReader() {}


	private:

		IBlobReader(const IBlobReader& other) {}
};


IBlobReader* CreateBlobReader(const char* filename);
bool IsCompressedBlob(const char* filename);


typedef void (*CompressCB)(const char* text, float percent, void* arg);

bool CompressFileToBlob(const char* infile, const char* outfile, u32 sub_type = 0, int sector_size = 16384,
		CompressCB callback = 0, void* arg = 0);
bool DecompressBlobToFile(const char* infile, const char* outfile,
		CompressCB callback = 0, void* arg = 0);
}

#endif

