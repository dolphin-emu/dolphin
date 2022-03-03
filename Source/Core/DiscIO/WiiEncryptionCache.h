// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <limits>
#include <memory>

#include "Common/CommonTypes.h"
#include "DiscIO/VolumeWii.h"

namespace DiscIO
{
class BlobReader;

class WiiEncryptionCache
{
public:
  using Key = std::array<u8, VolumeWii::AES_KEY_SIZE>;
  using HashExceptionCallback = std::function<void(
      VolumeWii::HashBlock hash_blocks[VolumeWii::BLOCKS_PER_GROUP], u64 offset)>;

  // The blob pointer is kept around for the lifetime of this object.
  explicit WiiEncryptionCache(BlobReader* blob);
  ~WiiEncryptionCache();

  WiiEncryptionCache(WiiEncryptionCache&&) = default;
  WiiEncryptionCache& operator=(WiiEncryptionCache&&) = default;

  // It would be possible to write a custom copy constructor and assignment operator
  // for this class, but there has been no reason to do so.
  WiiEncryptionCache(const WiiEncryptionCache&) = delete;
  WiiEncryptionCache& operator=(const WiiEncryptionCache&) = delete;

  // Encrypts exactly one group.
  // If the returned pointer is nullptr, reading from the blob failed.
  // If the returned pointer is not nullptr, it is guaranteed to be valid until
  // the next call of this function or the destruction of this object.
  const std::array<u8, VolumeWii::GROUP_TOTAL_SIZE>*
  EncryptGroup(u64 offset, u64 partition_data_offset, u64 partition_data_decrypted_size,
               const Key& key, const HashExceptionCallback& hash_exception_callback = {});

  // Encrypts a variable number of groups, as determined by the offset and size parameters.
  // Supports reading groups partially.
  bool EncryptGroups(u64 offset, u64 size, u8* out_ptr, u64 partition_data_offset,
                     u64 partition_data_decrypted_size, const Key& key,
                     const HashExceptionCallback& hash_exception_callback = {});

private:
  BlobReader* m_blob;
  std::unique_ptr<std::array<u8, VolumeWii::GROUP_TOTAL_SIZE>> m_cache;
  u64 m_cached_offset = 0;
};

}  // namespace DiscIO
