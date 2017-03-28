// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <cinttypes>
#include <cstdio>
#include <string>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/ES/NandUtils.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
// Used by the GetStoredContents ioctlvs. This assumes that the first output vector
// is used for the content count (u32).
IPCCommandResult ES::GetStoredContentsCount(const IOS::ES::TMDReader& tmd,
                                            const IOCtlVRequest& request)
{
  if (request.io_vectors[0].size != sizeof(u32) || !tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  const u16 num_contents = static_cast<u16>(IOS::ES::GetStoredContentsFromTMD(tmd).size());
  Memory::Write_U32(num_contents, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "GetStoredContentsCount (0x%x):  %u content(s) for %016" PRIx64, request.request,
           num_contents, tmd.GetTitleId());
  return GetDefaultReply(IPC_SUCCESS);
}

// Used by the GetStoredContents ioctlvs. This assumes that the second input vector is used
// for the content count and the output vector is used to store a list of content IDs (u32s).
IPCCommandResult ES::GetStoredContents(const IOS::ES::TMDReader& tmd, const IOCtlVRequest& request)
{
  if (!tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  if (request.in_vectors[1].size != sizeof(u32) ||
      request.io_vectors[0].size != Memory::Read_U32(request.in_vectors[1].address) * sizeof(u32))
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const auto contents = IOS::ES::GetStoredContentsFromTMD(tmd);
  const u32 max_content_count = Memory::Read_U32(request.in_vectors[1].address);
  for (u32 i = 0; i < std::min(static_cast<u32>(contents.size()), max_content_count); ++i)
    Memory::Write_U32(contents[i].id, request.io_vectors[0].address + i * sizeof(u32));

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetStoredContentsCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u64))
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const IOS::ES::TMDReader tmd = IOS::ES::FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);
  return GetStoredContentsCount(tmd, request);
}

IPCCommandResult ES::GetStoredContents(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) || request.in_vectors[0].size != sizeof(u64))
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const IOS::ES::TMDReader tmd = IOS::ES::FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);
  return GetStoredContents(tmd, request);
}

IPCCommandResult ES::GetTMDStoredContentsCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd_bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
  return GetStoredContentsCount(IOS::ES::TMDReader{std::move(tmd_bytes)}, request);
}

IPCCommandResult ES::GetTMDStoredContents(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd_bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
  return GetStoredContents(IOS::ES::TMDReader{std::move(tmd_bytes)}, request);
}

IPCCommandResult ES::GetTitleCount(const std::vector<u64>& titles, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != 4)
    return GetDefaultReply(ES_EINVAL);

  Memory::Write_U32(static_cast<u32>(titles.size()), request.io_vectors[0].address);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitles(const std::vector<u64>& titles, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  const size_t max_count = Memory::Read_U32(request.in_vectors[0].address);
  for (size_t i = 0; i < std::min(max_count, titles.size()); i++)
  {
    Memory::Write_U64(titles[i], request.io_vectors[0].address + static_cast<u32>(i) * sizeof(u64));
    INFO_LOG(IOS_ES, "     title %016" PRIx64, titles[i]);
  }
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitleCount(const IOCtlVRequest& request)
{
  const std::vector<u64> titles = IOS::ES::GetInstalledTitles();
  INFO_LOG(IOS_ES, "GetTitleCount: %zu titles", titles.size());
  return GetTitleCount(titles, request);
}

IPCCommandResult ES::GetTitles(const IOCtlVRequest& request)
{
  return GetTitles(IOS::ES::GetInstalledTitles(), request);
}

IPCCommandResult ES::GetStoredTMDSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const IOS::ES::TMDReader tmd = IOS::ES::FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const u32 tmd_size = static_cast<u32>(tmd.GetRawTMD().size());
  Memory::Write_U32(tmd_size, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "GetStoredTMDSize: %u bytes  for %016" PRIx64, tmd_size, title_id);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetStoredTMD(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const IOS::ES::TMDReader tmd = IOS::ES::FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  // TODO: actually use this param in when writing to the outbuffer :/
  const u32 MaxCount = Memory::Read_U32(request.in_vectors[1].address);

  const std::vector<u8> raw_tmd = tmd.GetRawTMD();
  if (raw_tmd.size() != request.io_vectors[0].size)
    return GetDefaultReply(ES_EINVAL);

  Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());

  INFO_LOG(IOS_ES, "GetStoredTMD: title %016" PRIx64 " (buffer size: %u)", title_id, MaxCount);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetOwnedTitleCount(const IOCtlVRequest& request)
{
  const std::vector<u64> titles = IOS::ES::GetTitlesWithTickets();
  INFO_LOG(IOS_ES, "GetOwnedTitleCount: %zu titles", titles.size());
  return GetTitleCount(titles, request);
}

IPCCommandResult ES::GetOwnedTitles(const IOCtlVRequest& request)
{
  return GetTitles(IOS::ES::GetTitlesWithTickets(), request);
}

IPCCommandResult ES::GetBoot2Version(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
    return GetDefaultReply(ES_EINVAL);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETBOOT2VERSION");

  // as of 26/02/2012, this was latest bootmii version
  Memory::Write_U32(4, request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
