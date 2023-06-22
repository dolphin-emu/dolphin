// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include <bzlib.h>
#include <lzma.h>
#include <zstd.h>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "DiscIO/LaggedFibonacciGenerator.h"

namespace DiscIO
{
struct DecompressionBuffer
{
  std::vector<u8> data;
  size_t bytes_written = 0;
};

struct PurgeSegment
{
  u32 offset;
  u32 size;
};
static_assert(sizeof(PurgeSegment) == 0x08, "Wrong size for WIA purge segment");

class Decompressor
{
public:
  virtual ~Decompressor();

  virtual bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                          size_t* in_bytes_read) = 0;
  virtual bool Done() const { return m_done; };

protected:
  bool m_done = false;
};

class NoneDecompressor final : public Decompressor
{
public:
  bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                  size_t* in_bytes_read) override;
};

// This class assumes that more bytes won't be added to in once in.bytes_written == in.data.size()
// and that *in_bytes_read initially will be equal to the size of the exception lists
class PurgeDecompressor final : public Decompressor
{
public:
  PurgeDecompressor(u64 decompressed_size);
  bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                  size_t* in_bytes_read) override;

private:
  const u64 m_decompressed_size;

  PurgeSegment m_segment = {};
  size_t m_bytes_read = 0;
  size_t m_segment_bytes_written = 0;
  size_t m_out_bytes_written = 0;
  bool m_started = false;

  std::unique_ptr<Common::SHA1::Context> m_sha1_context;
};

class Bzip2Decompressor final : public Decompressor
{
public:
  ~Bzip2Decompressor();

  bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                  size_t* in_bytes_read) override;

private:
  bz_stream m_stream = {};
  bool m_started = false;
};

class LZMADecompressor final : public Decompressor
{
public:
  LZMADecompressor(bool lzma2, const u8* filter_options, size_t filter_options_size);
  ~LZMADecompressor();

  bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                  size_t* in_bytes_read) override;

private:
  lzma_stream m_stream = LZMA_STREAM_INIT;
  lzma_options_lzma m_options = {};
  lzma_filter m_filters[2];
  bool m_started = false;
  bool m_error_occurred = false;
};

class ZstdDecompressor final : public Decompressor
{
public:
  ZstdDecompressor();
  ~ZstdDecompressor();

  bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                  size_t* in_bytes_read) override;

private:
  ZSTD_DStream* m_stream;
};

class RVZPackDecompressor final : public Decompressor
{
public:
  RVZPackDecompressor(std::unique_ptr<Decompressor> decompressor, DecompressionBuffer decompressed,
                      u64 data_offset, u32 rvz_packed_size);

  bool Decompress(const DecompressionBuffer& in, DecompressionBuffer* out,
                  size_t* in_bytes_read) override;

  bool Done() const override;

private:
  bool IncrementBytesRead(size_t x);
  std::optional<bool> ReadToDecompressed(const DecompressionBuffer& in, size_t* in_bytes_read,
                                         size_t decompressed_bytes_read, size_t bytes_to_read);

  std::unique_ptr<Decompressor> m_decompressor;
  DecompressionBuffer m_decompressed;
  size_t m_decompressed_bytes_read = 0;
  size_t m_bytes_read;
  u64 m_data_offset;
  u32 m_rvz_packed_size;

  u32 m_size = 0;
  bool m_junk = false;
  LaggedFibonacciGenerator m_lfg;
};

class Compressor
{
public:
  virtual ~Compressor();

  // First call Start, then AddDataOnlyForPurgeHashing/Compress any number of times,
  // then End, then GetData/GetSize any number of times.

  virtual bool Start(std::optional<u64> size) = 0;
  virtual bool AddPrecedingDataOnlyForPurgeHashing(const u8* data, size_t size) { return true; }
  virtual bool Compress(const u8* data, size_t size) = 0;
  virtual bool End() = 0;

  virtual const u8* GetData() const = 0;
  virtual size_t GetSize() const = 0;
};

class PurgeCompressor final : public Compressor
{
public:
  PurgeCompressor();
  ~PurgeCompressor();

  bool Start(std::optional<u64> size) override;
  bool AddPrecedingDataOnlyForPurgeHashing(const u8* data, size_t size) override;
  bool Compress(const u8* data, size_t size) override;
  bool End() override;

  const u8* GetData() const override;
  size_t GetSize() const override;

private:
  std::vector<u8> m_buffer;
  size_t m_bytes_written = 0;
  std::unique_ptr<Common::SHA1::Context> m_sha1_context;
};

class Bzip2Compressor final : public Compressor
{
public:
  Bzip2Compressor(int compression_level);
  ~Bzip2Compressor();

  bool Start(std::optional<u64> size) override;
  bool Compress(const u8* data, size_t size) override;
  bool End() override;

  const u8* GetData() const override;
  size_t GetSize() const override;

private:
  void ExpandBuffer(size_t bytes_to_add);

  bz_stream m_stream = {};
  std::vector<u8> m_buffer;
  int m_compression_level;
};

class LZMACompressor final : public Compressor
{
public:
  LZMACompressor(bool lzma2, int compression_level, u8 compressor_data_out[7],
                 u8* compressor_data_size_out);
  ~LZMACompressor();

  bool Start(std::optional<u64> size) override;
  bool Compress(const u8* data, size_t size) override;
  bool End() override;

  const u8* GetData() const override;
  size_t GetSize() const override;

private:
  void ExpandBuffer(size_t bytes_to_add);

  lzma_stream m_stream = LZMA_STREAM_INIT;
  lzma_options_lzma m_options = {};
  lzma_filter m_filters[2];
  std::vector<u8> m_buffer;
  bool m_initialization_failed = false;
};

class ZstdCompressor final : public Compressor
{
public:
  ZstdCompressor(int compression_level);
  ~ZstdCompressor();

  bool Start(std::optional<u64> size) override;
  bool Compress(const u8* data, size_t size) override;
  bool End() override;

  const u8* GetData() const override { return m_buffer.data(); }
  size_t GetSize() const override { return m_out_buffer.pos; }

private:
  void ExpandBuffer(size_t bytes_to_add);

  ZSTD_CStream* m_stream;
  ZSTD_outBuffer m_out_buffer{};
  std::vector<u8> m_buffer;
};

}  // namespace DiscIO
