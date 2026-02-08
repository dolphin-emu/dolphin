// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WIACompression.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include <bzlib.h>
#include <lzma.h>
#include <zstd.h>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Common/Swap.h"
#include "DiscIO/LaggedFibonacciGenerator.h"

namespace DiscIO
{
static u32 LZMA2DictionarySize(u8 p)
{
  return (static_cast<u32>(2) | (p & 1)) << (p / 2 + 11);
}

Decompressor::~Decompressor() = default;

bool NoneDecompressor::Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                                  size_t* in_bytes_read)
{
  const size_t length =
      std::min(in.bytes_written - *in_bytes_read, out->data.size() - out->bytes_written);

  std::memcpy(out->data.data() + out->bytes_written, in.data.data() + *in_bytes_read, length);

  *in_bytes_read += length;
  out->bytes_written += length;

  m_done = in.data.size() == *in_bytes_read;
  return true;
}

PurgeDecompressor::PurgeDecompressor(u64 decompressed_size) : m_decompressed_size(decompressed_size)
{
}

bool PurgeDecompressor::Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                                   size_t* in_bytes_read)
{
  if (!m_started)
  {
    m_sha1_context = Common::SHA1::CreateContext();

    // Include the exception lists in the SHA-1 calculation (but not in the compression...)
    m_sha1_context->Update(in.data.data(), *in_bytes_read);

    m_started = true;
  }

  while (!m_done && in.bytes_written != *in_bytes_read &&
         (m_segment_bytes_written < sizeof(m_segment) || out->data.size() != out->bytes_written))
  {
    if (m_segment_bytes_written == 0 && *in_bytes_read == in.data.size() - Common::SHA1::DIGEST_LEN)
    {
      const size_t zeroes_to_write = std::min<size_t>(m_decompressed_size - m_out_bytes_written,
                                                      out->data.size() - out->bytes_written);

      std::memset(out->data.data() + out->bytes_written, 0, zeroes_to_write);

      out->bytes_written += zeroes_to_write;
      m_out_bytes_written += zeroes_to_write;

      if (m_out_bytes_written == m_decompressed_size && in.bytes_written == in.data.size())
      {
        const auto actual_hash = m_sha1_context->Finish();

        Common::SHA1::Digest expected_hash;
        std::memcpy(expected_hash.data(), in.data.data() + *in_bytes_read, expected_hash.size());

        *in_bytes_read += expected_hash.size();
        m_done = true;

        if (actual_hash != expected_hash)
          return false;
      }

      return true;
    }

    if (m_segment_bytes_written < sizeof(m_segment))
    {
      const size_t bytes_to_copy =
          std::min(in.bytes_written - *in_bytes_read, sizeof(m_segment) - m_segment_bytes_written);

      std::memcpy(reinterpret_cast<u8*>(&m_segment) + m_segment_bytes_written,
                  in.data.data() + *in_bytes_read, bytes_to_copy);
      m_sha1_context->Update(in.data.data() + *in_bytes_read, bytes_to_copy);

      *in_bytes_read += bytes_to_copy;
      m_bytes_read += bytes_to_copy;
      m_segment_bytes_written += bytes_to_copy;
    }

    if (m_segment_bytes_written < sizeof(m_segment))
      return true;

    const size_t offset = Common::swap32(m_segment.offset);
    const size_t size = Common::swap32(m_segment.size);

    if (m_out_bytes_written < offset)
    {
      const size_t zeroes_to_write =
          std::min(offset - m_out_bytes_written, out->data.size() - out->bytes_written);

      std::memset(out->data.data() + out->bytes_written, 0, zeroes_to_write);

      out->bytes_written += zeroes_to_write;
      m_out_bytes_written += zeroes_to_write;
    }

    if (m_out_bytes_written >= offset && m_out_bytes_written < offset + size)
    {
      const size_t bytes_to_copy = std::min(
          std::min(offset + size - m_out_bytes_written, out->data.size() - out->bytes_written),
          in.bytes_written - *in_bytes_read);

      std::memcpy(out->data.data() + out->bytes_written, in.data.data() + *in_bytes_read,
                  bytes_to_copy);
      m_sha1_context->Update(in.data.data() + *in_bytes_read, bytes_to_copy);

      *in_bytes_read += bytes_to_copy;
      m_bytes_read += bytes_to_copy;
      out->bytes_written += bytes_to_copy;
      m_out_bytes_written += bytes_to_copy;
    }

    if (m_out_bytes_written >= offset + size)
      m_segment_bytes_written = 0;
  }

  return true;
}

Bzip2Decompressor::~Bzip2Decompressor()
{
  if (m_started)
    BZ2_bzDecompressEnd(&m_stream);
}

bool Bzip2Decompressor::Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                                   size_t* in_bytes_read)
{
  if (!m_started)
  {
    if (BZ2_bzDecompressInit(&m_stream, 0, 0) != BZ_OK)
      return false;

    m_started = true;
  }

  char* const in_ptr = reinterpret_cast<char*>(const_cast<u8*>(in.data.data() + *in_bytes_read));
  m_stream.next_in = in_ptr;
  m_stream.avail_in = MathUtil::SaturatingCast<u32>(in.bytes_written - *in_bytes_read);

  char* const out_ptr = reinterpret_cast<char*>(out->data.data() + out->bytes_written);
  m_stream.next_out = out_ptr;
  m_stream.avail_out = MathUtil::SaturatingCast<u32>(out->data.size() - out->bytes_written);

  const int result = BZ2_bzDecompress(&m_stream);

  *in_bytes_read += m_stream.next_in - in_ptr;
  out->bytes_written += m_stream.next_out - out_ptr;

  m_done = result == BZ_STREAM_END;
  return result == BZ_OK || result == BZ_STREAM_END;
}

LZMADecompressor::LZMADecompressor(bool lzma2, const u8* filter_options, size_t filter_options_size)
{
  m_options.preset_dict = nullptr;

  if (!lzma2 && filter_options_size == 5)
  {
    // The dictionary size is stored as a 32-bit little endian unsigned integer
    static_assert(sizeof(m_options.dict_size) == sizeof(u32));
    std::memcpy(&m_options.dict_size, filter_options + 1, sizeof(u32));

    const u8 d = filter_options[0];
    if (d >= (9 * 5 * 5))
    {
      m_error_occurred = true;
    }
    else
    {
      m_options.lc = d % 9;
      const u8 e = d / 9;
      m_options.pb = e / 5;
      m_options.lp = e % 5;
    }
  }
  else if (lzma2 && filter_options_size == 1)
  {
    const u8 d = filter_options[0];
    if (d > 40)
      m_error_occurred = true;
    else
      m_options.dict_size = d == 40 ? 0xFFFFFFFF : LZMA2DictionarySize(d);
  }
  else
  {
    m_error_occurred = true;
  }

  m_filters[0].id = lzma2 ? LZMA_FILTER_LZMA2 : LZMA_FILTER_LZMA1;
  m_filters[0].options = &m_options;
  m_filters[1].id = LZMA_VLI_UNKNOWN;
  m_filters[1].options = nullptr;
}

LZMADecompressor::~LZMADecompressor()
{
  if (m_started)
    lzma_end(&m_stream);
}

bool LZMADecompressor::Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                                  size_t* in_bytes_read)
{
  if (!m_started)
  {
    if (m_error_occurred || lzma_raw_decoder(&m_stream, m_filters) != LZMA_OK)
      return false;

    m_started = true;
  }

  const u8* const in_ptr = in.data.data() + *in_bytes_read;
  m_stream.next_in = in_ptr;
  m_stream.avail_in = in.bytes_written - *in_bytes_read;

  u8* const out_ptr = out->data.data() + out->bytes_written;
  m_stream.next_out = out_ptr;
  m_stream.avail_out = out->data.size() - out->bytes_written;

  const lzma_ret result = lzma_code(&m_stream, LZMA_RUN);

  *in_bytes_read += m_stream.next_in - in_ptr;
  out->bytes_written += m_stream.next_out - out_ptr;

  m_done = result == LZMA_STREAM_END;
  return result == LZMA_OK || result == LZMA_STREAM_END;
}

ZstdDecompressor::ZstdDecompressor()
{
  m_stream = ZSTD_createDStream();
}

ZstdDecompressor::~ZstdDecompressor()
{
  ZSTD_freeDStream(m_stream);
}

bool ZstdDecompressor::Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                                  size_t* in_bytes_read)
{
  if (!m_stream)
    return false;

  ZSTD_inBuffer in_buffer{in.data.data(), in.bytes_written, *in_bytes_read};
  ZSTD_outBuffer out_buffer{out->data.data(), out->data.size(), out->bytes_written};

  const size_t result = ZSTD_decompressStream(m_stream, &out_buffer, &in_buffer);

  *in_bytes_read = in_buffer.pos;
  out->bytes_written = out_buffer.pos;

  m_done = result == 0;
  return !ZSTD_isError(result);
}

RVZPackDecompressor::RVZPackDecompressor(std::unique_ptr<Decompressor> decompressor,
                                         DecompressionBuffer decompressed, u64 data_offset,
                                         u32 rvz_packed_size)
    : m_decompressor(std::move(decompressor)), m_decompressed(std::move(decompressed)),
      m_data_offset(data_offset), m_rvz_packed_size(rvz_packed_size)
{
  m_bytes_read = m_decompressed.bytes_written;
}

bool RVZPackDecompressor::IncrementBytesRead(size_t x)
{
  m_bytes_read += x;
  return m_bytes_read <= m_rvz_packed_size;
}

std::optional<bool> RVZPackDecompressor::ReadToDecompressed(const DecompressionBuffer& in,
                                                            size_t* in_bytes_read,
                                                            size_t decompressed_bytes_read,
                                                            size_t bytes_to_read)
{
  if (m_decompressed.data.size() < decompressed_bytes_read + bytes_to_read)
    m_decompressed.data.resize(decompressed_bytes_read + bytes_to_read);

  if (m_decompressed.bytes_written < decompressed_bytes_read + bytes_to_read)
  {
    const size_t prev_bytes_written = m_decompressed.bytes_written;

    if (!m_decompressor->Decompress(in, &m_decompressed, in_bytes_read))
      return false;

    if (!IncrementBytesRead(m_decompressed.bytes_written - prev_bytes_written))
      return false;

    if (m_decompressed.bytes_written < decompressed_bytes_read + bytes_to_read)
      return true;
  }

  return std::nullopt;
}

bool RVZPackDecompressor::Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                                     size_t* in_bytes_read)
{
  while (out->data.size() != out->bytes_written && !Done())
  {
    if (m_size == 0)
    {
      if (m_decompressed.bytes_written == m_decompressed_bytes_read)
      {
        m_decompressed.data.resize(sizeof(u32));
        m_decompressed.bytes_written = 0;
        m_decompressed_bytes_read = 0;
      }

      std::optional<bool> result =
          ReadToDecompressed(in, in_bytes_read, m_decompressed_bytes_read, sizeof(u32));
      if (result)
        return *result;

      const u32 size = Common::swap32(m_decompressed.data.data() + m_decompressed_bytes_read);

      m_junk = size & 0x80000000;
      if (m_junk)
      {
        constexpr size_t SEED_SIZE = LaggedFibonacciGenerator::SEED_SIZE * sizeof(u32);
        constexpr size_t BLOCK_SIZE = 0x8000;

        result = ReadToDecompressed(in, in_bytes_read, m_decompressed_bytes_read + sizeof(u32),
                                    SEED_SIZE);
        if (result)
          return *result;

        m_lfg.SetSeed(m_decompressed.data.data() + m_decompressed_bytes_read + sizeof(u32));
        m_lfg.Forward(m_data_offset % BLOCK_SIZE);

        m_decompressed_bytes_read += SEED_SIZE;
      }

      m_decompressed_bytes_read += sizeof(u32);
      m_size = size & 0x7FFFFFFF;
    }

    size_t bytes_to_write = std::min<size_t>(m_size, out->data.size() - out->bytes_written);
    if (m_junk)
    {
      m_lfg.GetBytes(bytes_to_write, out->data.data() + out->bytes_written);
      out->bytes_written += bytes_to_write;
    }
    else
    {
      if (m_decompressed.bytes_written != m_decompressed_bytes_read)
      {
        bytes_to_write =
            std::min(bytes_to_write, m_decompressed.bytes_written - m_decompressed_bytes_read);

        std::memcpy(out->data.data() + out->bytes_written,
                    m_decompressed.data.data() + m_decompressed_bytes_read, bytes_to_write);

        m_decompressed_bytes_read += bytes_to_write;
        out->bytes_written += bytes_to_write;
      }
      else
      {
        const size_t prev_out_bytes_written = out->bytes_written;
        const size_t old_out_size = out->data.size();
        const size_t new_out_size = out->bytes_written + bytes_to_write;

        if (new_out_size < old_out_size)
          out->data.resize(new_out_size);

        if (!m_decompressor->Decompress(in, out, in_bytes_read))
          return false;

        out->data.resize(old_out_size);

        bytes_to_write = out->bytes_written - prev_out_bytes_written;

        if (!IncrementBytesRead(bytes_to_write))
          return false;

        if (bytes_to_write == 0)
          return true;
      }
    }

    m_data_offset += bytes_to_write;
    m_size -= static_cast<u32>(bytes_to_write);
  }

  // If out is full but not all data has been read from in, give the decompressor a chance to read
  // from in anyway. This is needed for the case where zstd has read everything except the checksum.
  if (out->data.size() == out->bytes_written && in.bytes_written != *in_bytes_read)
  {
    if (!m_decompressor->Decompress(in, out, in_bytes_read))
      return false;
  }

  return true;
}

bool RVZPackDecompressor::Done() const
{
  return m_size == 0 && m_rvz_packed_size == m_bytes_read &&
         m_decompressed.bytes_written == m_decompressed_bytes_read && m_decompressor->Done();
}

Compressor::~Compressor() = default;

PurgeCompressor::PurgeCompressor() = default;

PurgeCompressor::~PurgeCompressor() = default;

bool PurgeCompressor::Start(std::optional<u64> size)
{
  m_buffer.clear();
  m_bytes_written = 0;

  m_sha1_context = Common::SHA1::CreateContext();

  return true;
}

bool PurgeCompressor::AddPrecedingDataOnlyForPurgeHashing(const u8* data, size_t size)
{
  m_sha1_context->Update(data, size);
  return true;
}

bool PurgeCompressor::Compress(const u8* data, size_t size)
{
  // We could add support for calling this twice if we're fine with
  // making the code more complicated, but there's no need to support it
  ASSERT_MSG(DISCIO, m_bytes_written == 0,
             "Calling PurgeCompressor::Compress() twice is not supported");

  m_buffer.resize(size + sizeof(PurgeSegment) + Common::SHA1::DIGEST_LEN);

  size_t bytes_read = 0;

  while (true)
  {
    const auto first_non_zero =
        std::find_if(data + bytes_read, data + size, [](u8 x) { return x != 0; });

    const u32 non_zero_data_start = static_cast<u32>(first_non_zero - data);
    if (non_zero_data_start == size)
      break;

    size_t non_zero_data_end = non_zero_data_start;
    size_t sequence_length = 0;
    for (size_t i = non_zero_data_start; i < size; ++i)
    {
      if (data[i] == 0)
      {
        ++sequence_length;
      }
      else
      {
        sequence_length = 0;
        non_zero_data_end = i + 1;
      }

      // To avoid wasting space, only count runs of zeroes that are of a certain length
      // (unless there is nothing after the run of zeroes, then we might as well always count it)
      if (sequence_length > sizeof(PurgeSegment))
        break;
    }

    const u32 non_zero_data_length = static_cast<u32>(non_zero_data_end - non_zero_data_start);

    const PurgeSegment segment{Common::swap32(non_zero_data_start),
                               Common::swap32(non_zero_data_length)};
    std::memcpy(m_buffer.data() + m_bytes_written, &segment, sizeof(segment));
    m_bytes_written += sizeof(segment);

    std::memcpy(m_buffer.data() + m_bytes_written, data + non_zero_data_start,
                non_zero_data_length);
    m_bytes_written += non_zero_data_length;

    bytes_read = non_zero_data_end;
  }

  return true;
}

bool PurgeCompressor::End()
{
  m_sha1_context->Update(m_buffer.data(), m_bytes_written);

  const auto digest = m_sha1_context->Finish();
  std::memcpy(m_buffer.data() + m_bytes_written, digest.data(), sizeof(digest));

  m_bytes_written += sizeof(digest);

  ASSERT(m_bytes_written <= m_buffer.size());

  return true;
}

const u8* PurgeCompressor::GetData() const
{
  return m_buffer.data();
}

size_t PurgeCompressor::GetSize() const
{
  return m_bytes_written;
}

Bzip2Compressor::Bzip2Compressor(int compression_level) : m_compression_level(compression_level)
{
}

Bzip2Compressor::~Bzip2Compressor()
{
  BZ2_bzCompressEnd(&m_stream);
}

bool Bzip2Compressor::Start(std::optional<u64> size)
{
  ASSERT_MSG(DISCIO, m_stream.state == nullptr,
             "Called Bzip2Compressor::Start() twice without calling Bzip2Compressor::End()");

  m_buffer.clear();
  m_stream.next_out = reinterpret_cast<char*>(m_buffer.data());

  return BZ2_bzCompressInit(&m_stream, m_compression_level, 0, 0) == BZ_OK;
}

bool Bzip2Compressor::Compress(const u8* data, size_t size)
{
  m_stream.next_in = reinterpret_cast<char*>(const_cast<u8*>(data));
  m_stream.avail_in = static_cast<unsigned int>(size);

  ExpandBuffer(size);

  while (m_stream.avail_in != 0)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    if (BZ2_bzCompress(&m_stream, BZ_RUN) != BZ_RUN_OK)
      return false;
  }

  return true;
}

bool Bzip2Compressor::End()
{
  bool success = true;

  while (true)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    const int result = BZ2_bzCompress(&m_stream, BZ_FINISH);
    if (result != BZ_FINISH_OK && result != BZ_STREAM_END)
      success = false;
    if (result != BZ_FINISH_OK)
      break;
  }

  if (BZ2_bzCompressEnd(&m_stream) != BZ_OK)
    success = false;

  return success;
}

void Bzip2Compressor::ExpandBuffer(size_t bytes_to_add)
{
  const size_t bytes_written = GetSize();
  m_buffer.resize(m_buffer.size() + bytes_to_add);
  m_stream.next_out = reinterpret_cast<char*>(m_buffer.data()) + bytes_written;
  m_stream.avail_out = static_cast<unsigned int>(m_buffer.size() - bytes_written);
}

const u8* Bzip2Compressor::GetData() const
{
  return m_buffer.data();
}

size_t Bzip2Compressor::GetSize() const
{
  return static_cast<size_t>(reinterpret_cast<u8*>(m_stream.next_out) - m_buffer.data());
}

LZMACompressor::LZMACompressor(bool lzma2, int compression_level, u8 compressor_data_out[7],
                               u8* compressor_data_size_out)
{
  // lzma_lzma_preset returns false on success for some reason
  if (lzma_lzma_preset(&m_options, static_cast<uint32_t>(compression_level)))
  {
    m_initialization_failed = true;
    return;
  }

  if (!lzma2)
  {
    if (compressor_data_size_out)
      *compressor_data_size_out = 5;

    if (compressor_data_out)
    {
      ASSERT(m_options.lc < 9);
      ASSERT(m_options.lp < 5);
      ASSERT(m_options.pb < 5);
      compressor_data_out[0] =
          static_cast<u8>((m_options.pb * 5 + m_options.lp) * 9 + m_options.lc);

      // The dictionary size is stored as a 32-bit little endian unsigned integer
      static_assert(sizeof(m_options.dict_size) == sizeof(u32));
      std::memcpy(compressor_data_out + 1, &m_options.dict_size, sizeof(u32));
    }
  }
  else
  {
    if (compressor_data_size_out)
      *compressor_data_size_out = 1;

    if (compressor_data_out)
    {
      u8 encoded_dict_size = 0;
      while (encoded_dict_size < 40 && m_options.dict_size > LZMA2DictionarySize(encoded_dict_size))
        ++encoded_dict_size;

      compressor_data_out[0] = encoded_dict_size;
    }
  }

  m_filters[0].id = lzma2 ? LZMA_FILTER_LZMA2 : LZMA_FILTER_LZMA1;
  m_filters[0].options = &m_options;
  m_filters[1].id = LZMA_VLI_UNKNOWN;
  m_filters[1].options = nullptr;
}

LZMACompressor::~LZMACompressor()
{
  lzma_end(&m_stream);
}

bool LZMACompressor::Start(std::optional<u64> size)
{
  if (m_initialization_failed)
    return false;

  m_buffer.clear();
  m_stream.next_out = m_buffer.data();

  return lzma_raw_encoder(&m_stream, m_filters) == LZMA_OK;
}

bool LZMACompressor::Compress(const u8* data, size_t size)
{
  m_stream.next_in = data;
  m_stream.avail_in = size;

  ExpandBuffer(size);

  while (m_stream.avail_in != 0)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    if (lzma_code(&m_stream, LZMA_RUN) != LZMA_OK)
      return false;
  }

  return true;
}

bool LZMACompressor::End()
{
  while (true)
  {
    if (m_stream.avail_out == 0)
      ExpandBuffer(0x100);

    switch (lzma_code(&m_stream, LZMA_FINISH))
    {
    case LZMA_OK:
      break;
    case LZMA_STREAM_END:
      return true;
    default:
      return false;
    }
  }
}

void LZMACompressor::ExpandBuffer(size_t bytes_to_add)
{
  const size_t bytes_written = GetSize();
  m_buffer.resize(m_buffer.size() + bytes_to_add);
  m_stream.next_out = m_buffer.data() + bytes_written;
  m_stream.avail_out = m_buffer.size() - bytes_written;
}

const u8* LZMACompressor::GetData() const
{
  return m_buffer.data();
}

size_t LZMACompressor::GetSize() const
{
  return static_cast<size_t>(m_stream.next_out - m_buffer.data());
}

ZstdCompressor::ZstdCompressor(int compression_level)
{
  m_stream = ZSTD_createCStream();

  if (ZSTD_isError(ZSTD_CCtx_setParameter(m_stream, ZSTD_c_compressionLevel, compression_level)) ||
      ZSTD_isError(ZSTD_CCtx_setParameter(m_stream, ZSTD_c_contentSizeFlag, 0)))
  {
    m_stream = nullptr;
  }
}

ZstdCompressor::~ZstdCompressor()
{
  ZSTD_freeCStream(m_stream);
}

bool ZstdCompressor::Start(std::optional<u64> size)
{
  if (!m_stream)
    return false;

  m_buffer.clear();
  m_out_buffer = {};

  if (ZSTD_isError(ZSTD_CCtx_reset(m_stream, ZSTD_reset_session_only)))
    return false;

  if (size)
  {
    if (ZSTD_isError(ZSTD_CCtx_setPledgedSrcSize(m_stream, *size)))
      return false;
  }

  return true;
}

bool ZstdCompressor::Compress(const u8* data, size_t size)
{
  ZSTD_inBuffer in_buffer{data, size, 0};

  ExpandBuffer(size);

  while (in_buffer.size != in_buffer.pos)
  {
    if (m_out_buffer.size == m_out_buffer.pos)
      ExpandBuffer(0x100);

    if (ZSTD_isError(ZSTD_compressStream(m_stream, &m_out_buffer, &in_buffer)))
      return false;
  }

  return true;
}

bool ZstdCompressor::End()
{
  while (true)
  {
    if (m_out_buffer.size == m_out_buffer.pos)
      ExpandBuffer(0x100);

    const size_t result = ZSTD_endStream(m_stream, &m_out_buffer);
    if (ZSTD_isError(result))
      return false;
    if (result == 0)
      return true;
  }
}

void ZstdCompressor::ExpandBuffer(size_t bytes_to_add)
{
  m_buffer.resize(m_buffer.size() + bytes_to_add);

  m_out_buffer.dst = m_buffer.data();
  m_out_buffer.size = m_buffer.size();
}

}  // namespace DiscIO
