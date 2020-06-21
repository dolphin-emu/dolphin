// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "DiscIO/Blob.h"
#include "DiscIO/DiscScrubber.h"

namespace DiscIO
{
// This class wraps another BlobReader and zeroes out data that has been
// identified by DiscScrubber as unused.
class ScrubbedBlob : public BlobReader
{
public:
  static std::unique_ptr<ScrubbedBlob> Create(const std::string& path);

  BlobType GetBlobType() const override { return m_blob_reader->GetBlobType(); }

  u64 GetRawSize() const override { return m_blob_reader->GetRawSize(); }
  u64 GetDataSize() const override { return m_blob_reader->GetDataSize(); }
  bool IsDataSizeAccurate() const override { return m_blob_reader->IsDataSizeAccurate(); }

  u64 GetBlockSize() const override { return m_blob_reader->GetBlockSize(); }
  bool HasFastRandomAccessInBlock() const override
  {
    return m_blob_reader->HasFastRandomAccessInBlock();
  }
  std::string GetCompressionMethod() const override
  {
    return m_blob_reader->GetCompressionMethod();
  }

  bool Read(u64 offset, u64 size, u8* out_ptr) override;

private:
  ScrubbedBlob(std::unique_ptr<BlobReader> blob_reader, DiscScrubber scrubber);

  std::unique_ptr<BlobReader> m_blob_reader;
  DiscScrubber m_scrubber;
};

}  // namespace DiscIO
