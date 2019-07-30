// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/CompressedBlob.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
bool IsGCZBlob(File::IOFile& file);

CompressedBlobReader::CompressedBlobReader(File::IOFile file, const std::string& filename)
    : m_file(std::move(file)), m_file_name(filename)
{
  m_file_size = m_file.GetSize();
  m_file.Seek(0, SEEK_SET);
  m_file.ReadArray(&m_header, 1);

  SetSectorSize(m_header.block_size);

  // cache block pointers and hashes
  m_block_pointers.resize(m_header.num_blocks);
  m_file.ReadArray(m_block_pointers.data(), m_header.num_blocks);
  m_hashes.resize(m_header.num_blocks);
  m_file.ReadArray(m_hashes.data(), m_header.num_blocks);

  m_data_offset = (sizeof(CompressedBlobHeader)) +
                  (sizeof(u64)) * m_header.num_blocks     // skip block pointers
                  + (sizeof(u32)) * m_header.num_blocks;  // skip hashes

  // A compressed block is never ever longer than a decompressed block, so just header.block_size
  // should be fine.
  // I still add some safety margin.
  const u32 zlib_buffer_size = m_header.block_size + 64;
  m_zlib_buffer.resize(zlib_buffer_size);
}

std::unique_ptr<CompressedBlobReader> CompressedBlobReader::Create(File::IOFile file,
                                                                   const std::string& filename)
{
  if (IsGCZBlob(file))
    return std::unique_ptr<CompressedBlobReader>(
        new CompressedBlobReader(std::move(file), filename));

  return nullptr;
}

CompressedBlobReader::~CompressedBlobReader()
{
}

// IMPORTANT: Calling this function invalidates all earlier pointers gotten from this function.
u64 CompressedBlobReader::GetBlockCompressedSize(u64 block_num) const
{
  u64 start = m_block_pointers[block_num];
  if (block_num < m_header.num_blocks - 1)
    return m_block_pointers[block_num + 1] - start;
  else if (block_num == m_header.num_blocks - 1)
    return m_header.compressed_data_size - start;
  else
    PanicAlert("GetBlockCompressedSize - illegal block number %i", (int)block_num);
  return 0;
}

bool CompressedBlobReader::GetBlock(u64 block_num, u8* out_ptr)
{
  bool uncompressed = false;
  u32 comp_block_size = (u32)GetBlockCompressedSize(block_num);
  u64 offset = m_block_pointers[block_num] + m_data_offset;

  if (offset & (1ULL << 63))
  {
    if (comp_block_size != m_header.block_size)
      PanicAlert("Uncompressed block with wrong size");
    uncompressed = true;
    offset &= ~(1ULL << 63);
  }

  // clear unused part of zlib buffer. maybe this can be deleted when it works fully.
  memset(&m_zlib_buffer[comp_block_size], 0, m_zlib_buffer.size() - comp_block_size);

  m_file.Seek(offset, SEEK_SET);
  if (!m_file.ReadBytes(m_zlib_buffer.data(), comp_block_size))
  {
    PanicAlertT("The disc image \"%s\" is truncated, some of the data is missing.",
                m_file_name.c_str());
    m_file.Clear();
    return false;
  }

  // First, check hash.
  u32 block_hash = Common::HashAdler32(m_zlib_buffer.data(), comp_block_size);
  if (block_hash != m_hashes[block_num])
    PanicAlertT("The disc image \"%s\" is corrupt.\n"
                "Hash of block %" PRIu64 " is %08x instead of %08x.",
                m_file_name.c_str(), block_num, block_hash, m_hashes[block_num]);

  if (uncompressed)
  {
    std::copy(m_zlib_buffer.begin(), m_zlib_buffer.begin() + comp_block_size, out_ptr);
  }
  else
  {
    z_stream z = {};
    z.next_in = m_zlib_buffer.data();
    z.avail_in = comp_block_size;
    if (z.avail_in > m_header.block_size)
    {
      PanicAlert("We have a problem");
    }
    z.next_out = out_ptr;
    z.avail_out = m_header.block_size;
    inflateInit(&z);
    int status = inflate(&z, Z_FULL_FLUSH);
    u32 uncomp_size = m_header.block_size - z.avail_out;
    if (status != Z_STREAM_END)
    {
      // this seem to fire wrongly from time to time
      // to be sure, don't use compressed isos :P
      PanicAlert("Failure reading block %" PRIu64 " - out of data and not at end.", block_num);
    }
    inflateEnd(&z);
    if (uncomp_size != m_header.block_size)
    {
      PanicAlert("Wrong block size");
      return false;
    }
  }
  return true;
}

bool CompressFileToBlob(const std::string& infile_path, const std::string& outfile_path,
                        u32 sub_type, int block_size, CompressCB callback, void* arg)
{
  bool scrubbing = false;

  File::IOFile infile(infile_path, "rb");
  if (IsGCZBlob(infile))
  {
    PanicAlertT("\"%s\" is already compressed! Cannot compress it further.", infile_path.c_str());
    return false;
  }

  if (!infile)
  {
    PanicAlertT("Failed to open the input file \"%s\".", infile_path.c_str());
    return false;
  }

  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertT("Failed to open the output file \"%s\".\n"
                "Check that you have permissions to write the target folder and that the media can "
                "be written.",
                outfile_path.c_str());
    return false;
  }

  DiscScrubber disc_scrubber;
  std::unique_ptr<VolumeDisc> volume;
  if (sub_type == 1)
  {
    volume = CreateDisc(infile_path);
    if (!volume || !disc_scrubber.SetupScrub(volume.get(), block_size))
    {
      PanicAlertT("\"%s\" failed to be scrubbed. Probably the image is corrupt.",
                  infile_path.c_str());
      return false;
    }

    scrubbing = true;
  }

  z_stream z = {};
  if (deflateInit(&z, 9) != Z_OK)
    return false;

  callback(Common::GetStringT("Files opened, ready to compress."), 0, arg);

  CompressedBlobHeader header;
  header.magic_cookie = GCZ_MAGIC;
  header.sub_type = sub_type;
  header.block_size = block_size;
  header.data_size = infile.GetSize();

  // round upwards!
  header.num_blocks = (u32)((header.data_size + (block_size - 1)) / block_size);

  std::vector<u64> offsets(header.num_blocks);
  std::vector<u32> hashes(header.num_blocks);
  std::vector<u8> out_buf(block_size);
  std::vector<u8> in_buf(block_size);

  // seek past the header (we will write it at the end)
  outfile.Seek(sizeof(CompressedBlobHeader), SEEK_CUR);
  // seek past the offset and hash tables (we will write them at the end)
  outfile.Seek((sizeof(u64) + sizeof(u32)) * header.num_blocks, SEEK_CUR);
  // seek to the start of the input file to make sure we get everything
  infile.Seek(0, SEEK_SET);

  // Now we are ready to write compressed data!
  u64 position = 0;
  int num_compressed = 0;
  int num_stored = 0;
  int progress_monitor = std::max<int>(1, header.num_blocks / 1000);
  bool success = true;

  for (u32 i = 0; i < header.num_blocks; i++)
  {
    if (i % progress_monitor == 0)
    {
      const u64 inpos = infile.Tell();
      int ratio = 0;
      if (inpos != 0)
        ratio = (int)(100 * position / inpos);

      const std::string temp =
          StringFromFormat(Common::GetStringT("%i of %i blocks. Compression ratio %i%%").c_str(), i,
                           header.num_blocks, ratio);
      bool was_cancelled = !callback(temp, (float)i / (float)header.num_blocks, arg);
      if (was_cancelled)
      {
        success = false;
        break;
      }
    }

    offsets[i] = position;

    size_t read_bytes;
    if (scrubbing)
      read_bytes = disc_scrubber.GetNextBlock(infile, in_buf.data());
    else
      infile.ReadArray(in_buf.data(), header.block_size, &read_bytes);
    if (read_bytes < header.block_size)
      std::fill(in_buf.begin() + read_bytes, in_buf.begin() + header.block_size, 0);

    int retval = deflateReset(&z);
    z.next_in = in_buf.data();
    z.avail_in = header.block_size;
    z.next_out = out_buf.data();
    z.avail_out = block_size;

    if (retval != Z_OK)
    {
      ERROR_LOG(DISCIO, "Deflate failed");
      success = false;
      break;
    }

    int status = deflate(&z, Z_FINISH);
    int comp_size = block_size - z.avail_out;

    u8* write_buf;
    int write_size;
    if ((status != Z_STREAM_END) || (z.avail_out < 10))
    {
      // PanicAlert("%i %i Store %i", i*block_size, position, comp_size);
      // let's store uncompressed
      write_buf = in_buf.data();
      offsets[i] |= 0x8000000000000000ULL;
      write_size = block_size;
      num_stored++;
    }
    else
    {
      // let's store compressed
      // PanicAlert("Comp %i to %i", block_size, comp_size);
      write_buf = out_buf.data();
      write_size = comp_size;
      num_compressed++;
    }

    if (!outfile.WriteBytes(write_buf, write_size))
    {
      PanicAlertT("Failed to write the output file \"%s\".\n"
                  "Check that you have enough space available on the target drive.",
                  outfile_path.c_str());
      success = false;
      break;
    }

    position += write_size;

    hashes[i] = Common::HashAdler32(write_buf, write_size);
  }

  header.compressed_data_size = position;

  if (!success)
  {
    // Remove the incomplete output file.
    outfile.Close();
    File::Delete(outfile_path);
  }
  else
  {
    // Okay, go back and fill in headers
    outfile.Seek(0, SEEK_SET);
    outfile.WriteArray(&header, 1);
    outfile.WriteArray(offsets.data(), header.num_blocks);
    outfile.WriteArray(hashes.data(), header.num_blocks);
  }

  // Cleanup
  deflateEnd(&z);

  if (success)
  {
    callback(Common::GetStringT("Done compressing disc image."), 1.0f, arg);
  }
  return success;
}

bool DecompressBlobToFile(const std::string& infile_path, const std::string& outfile_path,
                          CompressCB callback, void* arg)
{
  std::unique_ptr<CompressedBlobReader> reader;
  {
    File::IOFile infile(infile_path, "rb");
    if (!IsGCZBlob(infile))
    {
      PanicAlertT("File not compressed");
      return false;
    }

    reader = CompressedBlobReader::Create(std::move(infile), infile_path);
  }

  if (!reader)
  {
    PanicAlertT("Failed to open the input file \"%s\".", infile_path.c_str());
    return false;
  }

  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertT("Failed to open the output file \"%s\".\n"
                "Check that you have permissions to write the target folder and that the media can "
                "be written.",
                outfile_path.c_str());
    return false;
  }

  const CompressedBlobHeader& header = reader->GetHeader();
  static const size_t BUFFER_BLOCKS = 32;
  size_t buffer_size = header.block_size * BUFFER_BLOCKS;
  size_t last_buffer_size = header.block_size * (header.num_blocks % BUFFER_BLOCKS);
  std::vector<u8> buffer(buffer_size);
  u32 num_buffers = (header.num_blocks + BUFFER_BLOCKS - 1) / BUFFER_BLOCKS;
  int progress_monitor = std::max<int>(1, num_buffers / 100);
  bool success = true;

  for (u64 i = 0; i < num_buffers; i++)
  {
    if (i % progress_monitor == 0)
    {
      const bool was_cancelled =
          !callback(Common::GetStringT("Unpacking"), (float)i / (float)num_buffers, arg);
      if (was_cancelled)
      {
        success = false;
        break;
      }
    }
    const size_t sz = i == num_buffers - 1 ? last_buffer_size : buffer_size;
    reader->Read(i * buffer_size, sz, buffer.data());
    if (!outfile.WriteBytes(buffer.data(), sz))
    {
      PanicAlertT("Failed to write the output file \"%s\".\n"
                  "Check that you have enough space available on the target drive.",
                  outfile_path.c_str());
      success = false;
      break;
    }
  }

  if (!success)
  {
    // Remove the incomplete output file.
    outfile.Close();
    File::Delete(outfile_path);
  }
  else
  {
    outfile.Resize(header.data_size);
  }

  return success;
}

bool IsGCZBlob(File::IOFile& file)
{
  const u64 position = file.Tell();
  if (!file.Seek(0, SEEK_SET))
    return false;
  CompressedBlobHeader header;
  bool is_gcz = file.ReadArray(&header, 1) && header.magic_cookie == GCZ_MAGIC;
  file.Seek(position, SEEK_SET);
  return is_gcz;
}

}  // namespace DiscIO
