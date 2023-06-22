// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/ScrubbedBlob.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "Common/Align.h"
#include "DiscIO/Blob.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/VolumeDisc.h"

namespace DiscIO
{
ScrubbedBlob::ScrubbedBlob(std::unique_ptr<BlobReader> blob_reader, DiscScrubber scrubber)
    : m_blob_reader(std::move(blob_reader)), m_scrubber(std::move(scrubber))
{
}

std::unique_ptr<ScrubbedBlob> ScrubbedBlob::Create(const std::string& path)
{
  std::unique_ptr<VolumeDisc> disc = CreateDisc(path);
  if (!disc)
    return nullptr;

  DiscScrubber scrubber;
  if (!scrubber.SetupScrub(disc.get()))
    return nullptr;

  std::unique_ptr<BlobReader> blob = CreateBlobReader(path);
  if (!blob)
    return nullptr;

  return std::unique_ptr<ScrubbedBlob>(new ScrubbedBlob(std::move(blob), std::move(scrubber)));
}

bool ScrubbedBlob::Read(u64 offset, u64 size, u8* out_ptr)
{
  while (size > 0)
  {
    constexpr size_t CLUSTER_SIZE = DiscScrubber::CLUSTER_SIZE;
    const u64 bytes_to_read =
        std::min(Common::AlignDown(offset + CLUSTER_SIZE, CLUSTER_SIZE) - offset, size);

    if (m_scrubber.CanBlockBeScrubbed(offset))
    {
      std::fill_n(out_ptr, bytes_to_read, 0);
    }
    else
    {
      if (!m_blob_reader->Read(offset, bytes_to_read, out_ptr))
        return false;
    }

    offset += bytes_to_read;
    size -= bytes_to_read;
    out_ptr += bytes_to_read;
  }

  return true;
}

}  // namespace DiscIO
