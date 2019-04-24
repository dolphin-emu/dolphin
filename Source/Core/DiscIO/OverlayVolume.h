// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "DiscIO/Volume.h"

namespace PatchEngine
{
struct Patch;
}

namespace DiscIO
{
class OverlayVolume : public Volume
{
public:
  OverlayVolume(std::unique_ptr<Volume> innerVolume, std::vector<PatchEngine::Patch>& patches)
      : inner(std::move(innerVolume))
  {
    patchOffsets = CalculateDiscOffsets(*inner, patches);
  };

  virtual bool Read(u64 offset, u64 length, u8* buffer, const Partition& partition) const;

private:
  std::unique_ptr<Volume> inner;
  std::map<u64, std::vector<u8>> patchOffsets;

  static std::map<u64, std::vector<u8>>
  CalculateDiscOffsets(Volume& disc, std::vector<PatchEngine::Patch>& patches);

  // These are all pass-through wrappers to the inner volume
public:
  virtual bool IsEncryptedAndHashed() const { return inner->IsEncryptedAndHashed(); }
  virtual std::vector<Partition> GetPartitions() const { return inner->GetPartitions(); }
  virtual Partition GetGamePartition() const { return inner->GetGamePartition(); }
  virtual std::optional<u32> GetPartitionType(const Partition& partition) const
  {
    return inner->GetPartitionType(partition);
  }
  virtual std::optional<u64> GetTitleID(const Partition& partition) const
  {
    return inner->GetTitleID(partition);
  }
  virtual const IOS::ES::TicketReader& GetTicket(const Partition& partition) const
  {
    return inner->GetTicket(partition);
  }
  virtual const IOS::ES::TMDReader& GetTMD(const Partition& partition) const
  {
    return inner->GetTMD(partition);
  }
  // Returns a non-owning pointer. Returns nullptr if the file system couldn't be read.
  virtual const FileSystem* GetFileSystem(const Partition& partition) const
  {
    return inner->GetFileSystem(partition);
  }
  virtual u64 PartitionOffsetToRawOffset(u64 offset, const Partition& partition) const
  {
    return inner->PartitionOffsetToRawOffset(offset, partition);
  }
  virtual std::string GetGameID(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetGameID(partition);
  }
  virtual std::string GetGameTDBID(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetGameTDBID(partition);
  }
  virtual std::string GetMakerID(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetMakerID(partition);
  }
  virtual std::optional<u16> GetRevision(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetRevision(partition);
  }
  virtual std::string GetInternalName(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetInternalName(partition);
  }
  virtual std::map<Language, std::string> GetShortNames() const { return inner->GetShortNames(); }
  virtual std::map<Language, std::string> GetLongNames() const { return inner->GetLongNames(); }
  virtual std::map<Language, std::string> GetShortMakers() const { return inner->GetShortMakers(); }
  virtual std::map<Language, std::string> GetLongMakers() const { return inner->GetLongMakers(); }
  virtual std::map<Language, std::string> GetDescriptions() const
  {
    return inner->GetDescriptions();
  }
  virtual std::vector<u32> GetBanner(u32* width, u32* height) const
  {
    return inner->GetBanner(width, height);
  }
  virtual std::string GetApploaderDate(const Partition& partition) const
  {
    return inner->GetApploaderDate(partition);
  }
  // 0 is the first disc, 1 is the second disc
  virtual std::optional<u8> GetDiscNumber(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetDiscNumber(partition);
  }
  virtual Platform GetVolumeType() const { return inner->GetVolumeType(); }
  virtual bool SupportsIntegrityCheck() const { return inner->SupportsIntegrityCheck(); }
  virtual bool CheckIntegrity(const Partition& partition) const
  {
    return inner->CheckIntegrity(partition);
  }
  virtual Region GetRegion() const { return inner->GetRegion(); }
  virtual Country GetCountry(const Partition& partition = PARTITION_NONE) const
  {
    return inner->GetCountry(partition);
  }
  virtual BlobType GetBlobType() const { return inner->GetBlobType(); }
  // Size of virtual disc (may be inaccurate depending on the blob type)
  virtual u64 GetSize() const { return inner->GetSize(); }
  // Size on disc (compressed size)
  virtual u64 GetRawSize() const { return inner->GetRawSize(); }
  virtual u32 GetOffsetShift() const { return inner->GetOffsetShift(); }
};
}  // namespace DiscIO
