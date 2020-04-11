// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <inttypes.h>

#include "Core/PatchEngine.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/OverlayVolume.h"

namespace DiscIO
{
std::map<u64, std::vector<u8>>
OverlayVolumeDisc::CalculateDiscOffsets(Volume& disc, std::vector<PatchEngine::Patch>& patches)
{
  auto partition = disc.GetGamePartition();
  const DiscIO::FileSystem* fs = disc.GetFileSystem(partition);

  auto offsets = std::map<u64, std::vector<u8>>();

  for (const auto& patch : patches)
  {
    if (!patch.active)
      continue;

    const auto& file = fs->FindFileInfo(patch.path);
    if (file)
    {
      for (const auto& entry : patch.entries)
      {
        u64 partition_offset = file->GetOffset() + entry.address;
        std::vector<u8> data;
        switch (entry.type)
        {
        case PatchEngine::PatchType::Patch32Bit:
          data.push_back(static_cast<u8>(entry.value >> 24));
          data.push_back(static_cast<u8>(entry.value >> 16));
          // [[fallthrough]];
        case PatchEngine::PatchType::Patch16Bit:
          data.push_back(static_cast<u8>(entry.value >> 8));
          // [[fallthrough]];
        case PatchEngine::PatchType::Patch8Bit:
          data.push_back(static_cast<u8>(entry.value));
          break;
        default:
          // unknown patch type
          // In the future we could add support for larger patch entries.

          // Downstream code deals with vectors of bytes which could be arbitrary length
          break;
        }
        offsets.emplace(partition_offset, data);
      }

      INFO_LOG(ACTIONREPLAY, "Applied patch '%s' to file '%s'", patch.name.c_str(),
               patch.path.c_str());
    }
  }

  return offsets;
}

static void applyPatchesToBuffer(u64 offset, u64 length, u8* buffer,
                                 std::map<u64, std::vector<u8>> patches)
{
  u64 end_offset = offset + length;

  // Scan through patches which might overlap our current read request and apply them

  // Because the patches are variably sized, we scan in reverse order to take advantage
  // of upper_bound, which gives us the first patch that starts after the read buffer.
  auto it = patches.upper_bound(end_offset);

  while (it-- != patches.begin())  // Loop backwards until start of s_disc_patches
  {
    if (it->first + it->second.size() < offset)
    {
      // Patch is before the read request, we can stop searching
      return;
    }
    else if (it->first >= offset)
    {
      // Patch starts inside the read request
      u64 start = it->first - offset;
      std::memmove(buffer + start, it->second.data(), std::min(it->second.size(), length - start));
      INFO_LOG(DVDINTERFACE, "patch applied at %08" PRIx64, offset + start);
    }
    else if (it->first + it->second.size() > offset)
    {
      // Patch overlaps with the start of the read request
      u64 start = offset - it->first;
      std::memmove(buffer, it->second.data() + start, std::min(it->second.size() - start, length));
      INFO_LOG(DVDINTERFACE, "patch applied at %08" PRIx64, offset);
    }
  }
}

bool OverlayVolumeDisc::Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const
{
  if (inner->Read(offset, length, buffer, partition))
  {
    if (partition ==
        inner->GetGamePartition())  // Currently we only support patching the game partition
    {
      applyPatchesToBuffer(offset, length, buffer, patchOffsets);
    }
    return true;
  }
  return false;
}

}  // namespace DiscIO
