// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/CachedBlob.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/MemArena.h"

#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
class CacheFiller final
{
public:
  explicit CacheFiller(std::unique_ptr<BlobReader> reader, bool attempt_to_scrub)
      : m_thread{&CacheFiller::ThreadFunc, this, std::move(reader), attempt_to_scrub}
  {
  }

  ~CacheFiller()
  {
    m_stop_thread.store(true, std::memory_order_relaxed);
    m_thread.join();
  }

  bool Read(u64 offset, u64 size, u8* out_ptr)
  {
    switch (GetCacheState(offset, size))
    {
    case CacheState::Cached:
      std::memcpy(out_ptr, m_memory_region_data + offset, size);
      return true;

    case CacheState::Scrubbed:
      WARN_LOG_FMT(DISCIO,
                   "CachedBlobReader: Read({}, {}) hits a scrubbed cluster which is not cached.",
                   offset, size);
      return false;

    default:
      return false;
    }
  }

private:
  enum class CacheState
  {
    Cached,
    NotCached,
    Scrubbed,
  };

  CacheState GetCacheState(u64 offset, u64 size)
  {
    const auto cache_pos = m_cache_filled_pos.load(std::memory_order_acquire);

    const auto end_pos = offset + size;
    if (end_pos > cache_pos)
      return CacheState::NotCached;

    // Note: Read(0, 0) would race scrubber setup, but then we'd check 0 clusters, so all good.

    for (u64 i = offset; i < end_pos; i += DiscScrubber::CLUSTER_SIZE)
    {
      if (m_scrubber.CanBlockBeScrubbed(i))
        return CacheState::Scrubbed;
    }

    return CacheState::Cached;
  }

  void ThreadFunc(std::unique_ptr<BlobReader> reader, bool attempt_to_scrub)
  {
    static constexpr auto PERIODIC_LOG_TIME = std::chrono::seconds{1};

    const auto start_time = Clock::now();
    const u64 total_size = reader->GetDataSize();

    m_memory_region_data = static_cast<u8*>(m_memory_region.Create(total_size));
    if (m_memory_region_data == nullptr)
    {
      ERROR_LOG_FMT(DISCIO, "CachedBlobReader: Failed to create memory region.");
      return;
    }

    // Returns CLUSTER_SIZE or smaller at the end of the file.
    const auto get_read_count = [&](u64 pos) {
      return std::min<u64>(total_size - pos, DiscScrubber::CLUSTER_SIZE);
    };

    // Used for periodic progress logging.
    u64 total_bytes_to_commit = total_size;

    if (attempt_to_scrub)
    {
      const auto volume = CreateVolume(reader->CopyReader());
      if (volume != nullptr && m_scrubber.SetupScrub(*volume))
      {
        for (u64 i = 0; i < total_size; i += DiscScrubber::CLUSTER_SIZE)
        {
          if (m_scrubber.CanBlockBeScrubbed(i))
            total_bytes_to_commit -= get_read_count(i);
        }
      }
      else
      {
        WARN_LOG_FMT(DISCIO, "CachedBlobReader: Failed to scrub. The entire file will be cached.");
      }
    }

    auto next_log_time = start_time + PERIODIC_LOG_TIME;
    u64 read_offset = 0;
    u64 committed_count = 0;

    while (true)
    {
      if (m_stop_thread.load(std::memory_order_relaxed))
      {
        NOTICE_LOG_FMT(DISCIO, "CachedBlobReader: Stopped");
        break;
      }

      const auto read_count = get_read_count(read_offset);
      if (read_count == 0)
      {
        const auto total_time = DT_s{Clock::now() - start_time}.count();

        static constexpr auto mib_scale = double(1 << 20);

        NOTICE_LOG_FMT(
            DISCIO, "CachedBlobReader: Completed. Cached {:.2f} of {:.2f} MiB in {:.2f} seconds.",
            committed_count / mib_scale, total_size / mib_scale, total_time);
        break;
      }

      if (!m_scrubber.CanBlockBeScrubbed(read_offset))
      {
        // CLUSTER_SIZE is evenly divisible into LazyMemoryRegion::BLOCK_SIZE on Windows.
        m_memory_region.EnsureMemoryPageWritable(read_offset);

        if (!reader->Read(read_offset, read_count, m_memory_region_data + read_offset))
        {
          ERROR_LOG_FMT(DISCIO, "CachedBlobReader: Read failed at position: {}", read_offset);
          break;
        }

        committed_count += read_count;
      }

      read_offset += read_count;
      m_cache_filled_pos.store(read_offset, std::memory_order_release);

      if (Clock::now() >= next_log_time)
      {
        INFO_LOG_FMT(DISCIO, "CachedBlobReader: Progress: {}%",
                     committed_count * 100 / total_bytes_to_commit);
        next_log_time += PERIODIC_LOG_TIME;
      }
    }
  }

  // The thread has read non-scrubbed bytes into memory up to this point.
  std::atomic<u64> m_cache_filled_pos{};

  Common::LazyMemoryRegion m_memory_region;
  u8* m_memory_region_data{};

  DiscScrubber m_scrubber;

  std::atomic_bool m_stop_thread{};
  std::thread m_thread;
};

class CachedBlobReader final : public BlobReader
{
public:
  explicit CachedBlobReader(std::unique_ptr<BlobReader> reader, bool attempt_to_scrub)
      : m_cache_filler{std::make_shared<CacheFiller>(reader->CopyReader(), attempt_to_scrub)},
        m_reader{std::move(reader)}

  {
    NOTICE_LOG_FMT(DISCIO, "CachedBlobReader: Created");
  }

  CachedBlobReader(std::shared_ptr<CacheFiller> cache_filler, std::unique_ptr<BlobReader> reader)
      : m_cache_filler{std::move(cache_filler)}, m_reader{std::move(reader)}
  {
    INFO_LOG_FMT(DISCIO, "CachedBlobReader: Copied");
  }

  BlobType GetBlobType() const override { return BlobType::PLAIN; }

  std::unique_ptr<BlobReader> CopyReader() const override
  {
    return std::make_unique<CachedBlobReader>(m_cache_filler, m_reader->CopyReader());
  }

  u64 GetRawSize() const override { return GetDataSize(); }
  u64 GetDataSize() const override { return m_reader->GetDataSize(); }
  DataSizeType GetDataSizeType() const override { return m_reader->GetDataSizeType(); }

  u64 GetBlockSize() const override { return 0; }
  bool HasFastRandomAccessInBlock() const override { return true; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

  bool Read(u64 offset, u64 size, u8* out_ptr) override
  {
    return m_cache_filler->Read(offset, size, out_ptr) || m_reader->Read(offset, size, out_ptr);
  }

private:
  // A shared object does the cache filling for sensible CopyReader behavior.
  const std::shared_ptr<CacheFiller> m_cache_filler;

  const std::unique_ptr<BlobReader> m_reader;
};

std::unique_ptr<BlobReader> CreateCachedBlobReader(std::unique_ptr<BlobReader> reader)
{
  return std::make_unique<CachedBlobReader>(std::move(reader), false);
}

std::unique_ptr<BlobReader> CreateScrubbingCachedBlobReader(std::unique_ptr<BlobReader> reader)
{
  return std::make_unique<CachedBlobReader>(std::move(reader), true);
}

}  // namespace DiscIO
