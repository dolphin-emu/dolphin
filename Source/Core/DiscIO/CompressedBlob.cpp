// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/CompressedBlob.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#endif

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/MultithreadedCompressor.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
bool IsGCZBlob(File::IOFile& file);

CompressedBlobReader::CompressedBlobReader(File::IOFile file, const std::string& filename)
    : m_file(std::move(file)), m_file_name(filename)
{
  m_file_size = m_file.GetSize();
  m_file.Seek(0, File::SeekOrigin::Begin);
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

std::unique_ptr<BlobReader> CompressedBlobReader::CopyReader() const
{
  return Create(m_file.Duplicate("rb"), m_file_name);
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
    ERROR_LOG_FMT(DISCIO, "{} - illegal block number {}", __func__, block_num);
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
      ERROR_LOG_FMT(DISCIO, "Uncompressed block with wrong size");
    uncompressed = true;
    offset &= ~(1ULL << 63);
  }

  // clear unused part of zlib buffer. maybe this can be deleted when it works fully.
  memset(&m_zlib_buffer[comp_block_size], 0, m_zlib_buffer.size() - comp_block_size);

  m_file.Seek(offset, File::SeekOrigin::Begin);
  if (!m_file.ReadBytes(m_zlib_buffer.data(), comp_block_size))
  {
    ERROR_LOG_FMT(DISCIO, "The disc image \"{}\" is truncated, some of the data is missing.",
                  m_file_name);
    m_file.ClearError();
    return false;
  }

  // First, check hash.
  const u32 block_hash = Common::HashAdler32(m_zlib_buffer.data(), comp_block_size);
  if (block_hash != m_hashes[block_num])
  {
    ERROR_LOG_FMT(DISCIO,
                  "The disc image \"{}\" is corrupt.\n"
                  "Hash of block {} is {:08x} instead of {:08x}.",
                  m_file_name, block_num, block_hash, m_hashes[block_num]);
  }

  if (uncompressed)
  {
    std::copy_n(m_zlib_buffer.begin(), comp_block_size, out_ptr);
  }
  else
  {
    z_stream z = {};
    z.next_in = m_zlib_buffer.data();
    z.avail_in = comp_block_size;
    if (z.avail_in > m_header.block_size)
    {
      ERROR_LOG_FMT(DISCIO, "Compressed block size is larger than uncompressed block size");
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
      ERROR_LOG_FMT(DISCIO, "Failure reading block {} - out of data and not at end.", block_num);
    }
    inflateEnd(&z);
    if (uncomp_size != m_header.block_size)
    {
      ERROR_LOG_FMT(DISCIO, "Wrong block size");
      return false;
    }
  }
  return true;
}

struct CompressThreadState
{
  CompressThreadState() : z{} {}
  ~CompressThreadState() { deflateEnd(&z); }

  // z_stream will stop working if it changes address, so this object must not be moved
  CompressThreadState(const CompressThreadState&) = delete;
  CompressThreadState(CompressThreadState&&) = delete;
  CompressThreadState& operator=(const CompressThreadState&) = delete;
  CompressThreadState& operator=(CompressThreadState&&) = delete;

  std::vector<u8> compressed_buffer;
  z_stream z;
};

struct CompressParameters
{
  std::vector<u8> data{};
  u32 block_number = 0;
  u64 inpos = 0;
};

struct OutputParameters
{
  std::vector<u8> data{};
  u32 block_number = 0;
  bool compressed = false;
  u64 inpos = 0;
};

static ConversionResultCode SetUpCompressThreadState(CompressThreadState* state)
{
  return deflateInit(&state->z, 9) == Z_OK ? ConversionResultCode::Success :
                                             ConversionResultCode::InternalError;
}

static ConversionResult<OutputParameters> Compress(CompressThreadState* state,
                                                   CompressParameters parameters, int block_size,
                                                   std::vector<u32>* hashes, int* num_stored,
                                                   int* num_compressed)
{
  state->compressed_buffer.resize(block_size);

  int retval = deflateReset(&state->z);
  state->z.next_in = parameters.data.data();
  state->z.avail_in = block_size;
  state->z.next_out = state->compressed_buffer.data();
  state->z.avail_out = block_size;

  if (retval != Z_OK)
  {
    ERROR_LOG_FMT(DISCIO, "Deflate failed");
    return ConversionResultCode::InternalError;
  }

  const int status = deflate(&state->z, Z_FINISH);

  state->compressed_buffer.resize(block_size - state->z.avail_out);

  OutputParameters output_parameters;
  if ((status != Z_STREAM_END) || (state->z.avail_out < 10))
  {
    // let's store uncompressed
    ++*num_stored;
    output_parameters = OutputParameters{std::move(parameters.data), parameters.block_number, false,
                                         parameters.inpos};
  }
  else
  {
    // let's store compressed
    ++*num_compressed;
    output_parameters = OutputParameters{std::move(state->compressed_buffer),
                                         parameters.block_number, true, parameters.inpos};
  }

  (*hashes)[parameters.block_number] =
      Common::HashAdler32(output_parameters.data.data(), output_parameters.data.size());

  return std::move(output_parameters);
}

static ConversionResultCode Output(OutputParameters parameters, File::IOFile* outfile,
                                   u64* position, std::vector<u64>* offsets, int progress_monitor,
                                   u32 num_blocks, CompressCB callback)
{
  u64 offset = *position;
  if (!parameters.compressed)
    offset |= 0x8000000000000000ULL;
  (*offsets)[parameters.block_number] = offset;

  *position += parameters.data.size();

  if (!outfile->WriteBytes(parameters.data.data(), parameters.data.size()))
    return ConversionResultCode::WriteFailed;

  if (parameters.block_number % progress_monitor == 0)
  {
    const int ratio =
        parameters.inpos == 0 ? 0 : static_cast<int>(100 * *position / parameters.inpos);

    const std::string text = Common::FmtFormatT("{0} of {1} blocks. Compression ratio {2}%",
                                                parameters.block_number, num_blocks, ratio);

    const float completion = static_cast<float>(parameters.block_number) / num_blocks;

    if (!callback(text, completion))
      return ConversionResultCode::Canceled;
  }

  return ConversionResultCode::Success;
}

bool ConvertToGCZ(BlobReader* infile, const std::string& infile_path,
                  const std::string& outfile_path, u32 sub_type, int block_size,
                  CompressCB callback)
{
  ASSERT(infile->GetDataSizeType() == DataSizeType::Accurate);

  File::IOFile outfile(outfile_path, "wb");
  if (!outfile)
  {
    PanicAlertFmtT(
        "Failed to open the output file \"{0}\".\n"
        "Check that you have permissions to write the target folder and that the media can "
        "be written.",
        outfile_path);
    return false;
  }

  callback(Common::GetStringT("Files opened, ready to compress."), 0);

  CompressedBlobHeader header;
  header.magic_cookie = GCZ_MAGIC;
  header.sub_type = sub_type;
  header.block_size = block_size;
  header.data_size = infile->GetDataSize();

  // round upwards!
  header.num_blocks = (u32)((header.data_size + (block_size - 1)) / block_size);

  std::vector<u64> offsets(header.num_blocks);
  std::vector<u32> hashes(header.num_blocks);

  // seek past the header (we will write it at the end)
  outfile.Seek(sizeof(CompressedBlobHeader), File::SeekOrigin::Current);
  // seek past the offset and hash tables (we will write them at the end)
  outfile.Seek((sizeof(u64) + sizeof(u32)) * header.num_blocks, File::SeekOrigin::Current);

  // Now we are ready to write compressed data!
  u64 inpos = 0;
  u64 position = 0;
  int num_compressed = 0;
  int num_stored = 0;
  int progress_monitor = std::max<int>(1, header.num_blocks / 1000);

  const auto compress = [&](CompressThreadState* state, CompressParameters parameters) {
    return Compress(state, std::move(parameters), block_size, &hashes, &num_stored,
                    &num_compressed);
  };

  const auto output = [&](OutputParameters parameters) {
    return Output(std::move(parameters), &outfile, &position, &offsets, progress_monitor,
                  header.num_blocks, callback);
  };

  MultithreadedCompressor<CompressThreadState, CompressParameters, OutputParameters> compressor(
      SetUpCompressThreadState, compress, output);

  std::vector<u8> in_buf(block_size);
  for (u32 i = 0; i < header.num_blocks; i++)
  {
    if (compressor.GetStatus() != ConversionResultCode::Success)
      break;

    const u64 bytes_to_read = std::min<u64>(block_size, header.data_size - inpos);

    if (!infile->Read(inpos, bytes_to_read, in_buf.data()))
    {
      compressor.SetError(ConversionResultCode::ReadFailed);
      break;
    }

    std::fill(in_buf.begin() + bytes_to_read, in_buf.begin() + header.block_size, 0);

    inpos += block_size;

    compressor.CompressAndWrite(CompressParameters{in_buf, i, inpos});
  }

  compressor.Shutdown();

  header.compressed_data_size = position;

  const ConversionResultCode result = compressor.GetStatus();

  if (result != ConversionResultCode::Success)
  {
    // Remove the incomplete output file.
    outfile.Close();
    File::Delete(outfile_path);
  }
  else
  {
    // Okay, go back and fill in headers
    outfile.Seek(0, File::SeekOrigin::Begin);
    outfile.WriteArray(&header, 1);
    outfile.WriteArray(offsets.data(), header.num_blocks);
    outfile.WriteArray(hashes.data(), header.num_blocks);

    callback(Common::GetStringT("Done compressing disc image."), 1.0f);
  }

  if (result == ConversionResultCode::ReadFailed)
    PanicAlertFmtT("Failed to read from the input file \"{0}\".", infile_path);

  if (result == ConversionResultCode::WriteFailed)
  {
    PanicAlertFmtT("Failed to write the output file \"{0}\".\n"
                   "Check that you have enough space available on the target drive.",
                   outfile_path);
  }

  return result == ConversionResultCode::Success;
}

bool IsGCZBlob(File::IOFile& file)
{
  const u64 position = file.Tell();
  if (!file.Seek(0, File::SeekOrigin::Begin))
    return false;
  CompressedBlobHeader header;
  bool is_gcz = file.ReadArray(&header, 1) && header.magic_cookie == GCZ_MAGIC;
  file.Seek(position, File::SeekOrigin::Begin);
  return is_gcz;
}

}  // namespace DiscIO
