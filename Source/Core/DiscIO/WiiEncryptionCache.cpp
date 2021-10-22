// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/WiiEncryptionCache.h"

#include <array>
#include <cstring>
#include <limits>
#include <memory>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
WiiEncryptionCache::WiiEncryptionCache(BlobReader* blob) : m_blob(blob)
{
}

WiiEncryptionCache::~WiiEncryptionCache() = default;

const std::array<u8, VolumeWii::GROUP_TOTAL_SIZE>*
WiiEncryptionCache::EncryptGroup(u64 offset, u64 partition_data_offset,
                                 u64 partition_data_decrypted_size, const Key& key,
                                 const HashExceptionCallback& hash_exception_callback)
{
  // Only allocate memory if this function actually ends up getting called
  if (!m_cache)
  {
    m_cache = std::make_unique<std::array<u8, VolumeWii::GROUP_TOTAL_SIZE>>();
    m_cached_offset = std::numeric_limits<u64>::max();
  }

  ASSERT(offset % VolumeWii::GROUP_TOTAL_SIZE == 0);
  const u64 group_offset_in_partition =
      offset / VolumeWii::GROUP_TOTAL_SIZE * VolumeWii::GROUP_DATA_SIZE;
  const u64 group_offset_on_disc = partition_data_offset + offset;

  if (m_cached_offset != group_offset_on_disc)
  {
    std::function<void(VolumeWii::HashBlock * hash_blocks)> hash_exception_callback_2;

    if (hash_exception_callback)
    {
      hash_exception_callback_2 =
          [offset, &hash_exception_callback](
              VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP]) {
            return hash_exception_callback(hash_blocks, offset);
          };
    }

    if (!VolumeWii::EncryptGroup(group_offset_in_partition, partition_data_offset,
                                 partition_data_decrypted_size, key, m_blob, m_cache.get(),
                                 hash_exception_callback_2))
    {
      m_cached_offset = std::numeric_limits<u64>::max();  // Invalidate the cache
      return nullptr;
    }

    m_cached_offset = group_offset_on_disc;
  }

  return m_cache.get();
}

bool WiiEncryptionCache::EncryptGroups(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset,
                                       u64 partition_data_decrypted_size, const Key& key,
                                       const HashExceptionCallback& hash_exception_callback)
{
  while (size > 0)
  {
    const std::array<u8, VolumeWii::GROUP_TOTAL_SIZE>* group =
        EncryptGroup(Common::AlignDown(offset, VolumeWii::GROUP_TOTAL_SIZE), partition_data_offset,
                     partition_data_decrypted_size, key, hash_exception_callback);

    if (!group)
      return false;

    const u64 offset_in_group = offset % VolumeWii::GROUP_TOTAL_SIZE;
    const u64 bytes_to_read = std::min(VolumeWii::GROUP_TOTAL_SIZE - offset_in_group, size);
    std::memcpy(out_ptr, group->data() + offset_in_group, bytes_to_read);

    offset += bytes_to_read;
    size -= bytes_to_read;
    out_ptr += bytes_to_read;
  }

  return true;
}

}  // namespace DiscIO
