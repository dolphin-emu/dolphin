// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
static std::vector<IOS::ES::Content> GetStoredContentsFromTMD(const IOS::ES::TMDReader& tmd)
{
  if (!tmd.IsValid())
    return {};

  const DiscIO::CSharedContent shared{Common::FROM_SESSION_ROOT};
  const std::vector<IOS::ES::Content> contents = tmd.GetContents();

  std::vector<IOS::ES::Content> stored_contents;

  std::copy_if(contents.begin(), contents.end(), std::back_inserter(stored_contents),
               [&tmd, &shared](const auto& content) {
                 if (content.IsShared())
                 {
                   const std::string path = shared.GetFilenameFromSHA1(content.sha1.data());
                   return path != "unk" && File::Exists(path);
                 }
                 return File::Exists(
                     Common::GetTitleContentPath(tmd.GetTitleId(), Common::FROM_SESSION_ROOT) +
                     StringFromFormat("%08x.app", content.id));
               });

  return stored_contents;
}

// Used by the GetStoredContents ioctlvs. This assumes that the first output vector
// is used for the content count (u32).
IPCCommandResult ES::GetStoredContentsCount(const IOS::ES::TMDReader& tmd,
                                            const IOCtlVRequest& request)
{
  if (request.io_vectors[0].size != sizeof(u32) || !tmd.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u16 num_contents = static_cast<u16>(GetStoredContentsFromTMD(tmd).size());
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
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  if (request.in_vectors[1].size != sizeof(u32) ||
      request.io_vectors[0].size != Memory::Read_U32(request.in_vectors[1].address) * sizeof(u32))
  {
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
  }

  const auto contents = GetStoredContentsFromTMD(tmd);
  const u32 max_content_count = Memory::Read_U32(request.in_vectors[1].address);
  for (u32 i = 0; i < std::min(static_cast<u32>(contents.size()), max_content_count); ++i)
    Memory::Write_U32(contents[i].id, request.io_vectors[0].address + i * sizeof(u32));

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetStoredContentsCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u64))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const DiscIO::CNANDContentLoader& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
  return GetStoredContentsCount(content_loader.GetTMD(), request);
}

IPCCommandResult ES::GetStoredContents(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) || request.in_vectors[0].size != sizeof(u64))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const DiscIO::CNANDContentLoader& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
  return GetStoredContents(content_loader.GetTMD(), request);
}

IPCCommandResult ES::GetTMDStoredContentsCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  std::vector<u8> tmd_bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
  return GetStoredContentsCount(IOS::ES::TMDReader{std::move(tmd_bytes)}, request);
}

IPCCommandResult ES::GetTMDStoredContents(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  std::vector<u8> tmd_bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
  return GetStoredContents(IOS::ES::TMDReader{std::move(tmd_bytes)}, request);
}

static bool IsValidPartOfTitleID(const std::string& string)
{
  if (string.length() != 8)
    return false;
  return std::all_of(string.begin(), string.end(),
                     [](const auto character) { return std::isxdigit(character) != 0; });
}

// Returns a vector of title IDs. IOS does not check the TMD at all here.
static std::vector<u64> GetInstalledTitles()
{
  const std::string titles_dir = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/title";
  if (!File::IsDirectory(titles_dir))
  {
    ERROR_LOG(IOS_ES, "/title is not a directory");
    return {};
  }

  std::vector<u64> title_ids;

  // The /title directory contains one directory per title type, and each of them contains
  // a directory per title (where the name is the low 32 bits of the title ID in %08x format).
  const auto entries = File::ScanDirectoryTree(titles_dir, true);
  for (const File::FSTEntry& title_type : entries.children)
  {
    if (!title_type.isDirectory || !IsValidPartOfTitleID(title_type.virtualName))
      continue;

    if (title_type.children.empty())
      continue;

    for (const File::FSTEntry& title_identifier : title_type.children)
    {
      if (!title_identifier.isDirectory || !IsValidPartOfTitleID(title_identifier.virtualName))
        continue;

      const u32 type = std::stoul(title_type.virtualName, nullptr, 16);
      const u32 identifier = std::stoul(title_identifier.virtualName, nullptr, 16);
      title_ids.push_back(static_cast<u64>(type) << 32 | identifier);
    }
  }

  return title_ids;
}

// Returns a vector of title IDs for which there is a ticket.
static std::vector<u64> GetTitlesWithTickets()
{
  const std::string titles_dir = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/ticket";
  if (!File::IsDirectory(titles_dir))
  {
    ERROR_LOG(IOS_ES, "/ticket is not a directory");
    return {};
  }

  std::vector<u64> title_ids;

  // The /ticket directory contains one directory per title type, and each of them contains
  // one ticket per title (where the name is the low 32 bits of the title ID in %08x format).
  const auto entries = File::ScanDirectoryTree(titles_dir, true);
  for (const File::FSTEntry& title_type : entries.children)
  {
    if (!title_type.isDirectory || !IsValidPartOfTitleID(title_type.virtualName))
      continue;

    if (title_type.children.empty())
      continue;

    for (const File::FSTEntry& ticket : title_type.children)
    {
      const std::string name_without_ext = ticket.virtualName.substr(0, 8);
      if (ticket.isDirectory || !IsValidPartOfTitleID(name_without_ext) ||
          name_without_ext + ".tik" != ticket.virtualName)
      {
        continue;
      }

      const u32 type = std::stoul(title_type.virtualName, nullptr, 16);
      const u32 identifier = std::stoul(name_without_ext, nullptr, 16);
      title_ids.push_back(static_cast<u64>(type) << 32 | identifier);
    }
  }

  return title_ids;
}

IPCCommandResult ES::GetTitleCount(const std::vector<u64>& titles, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != 4)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  Memory::Write_U32(static_cast<u32>(titles.size()), request.io_vectors[0].address);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitles(const std::vector<u64>& titles, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

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
  const std::vector<u64> titles = GetInstalledTitles();
  INFO_LOG(IOS_ES, "GetTitleCount: %zu titles", titles.size());
  return GetTitleCount(titles, request);
}

IPCCommandResult ES::GetTitles(const IOCtlVRequest& request)
{
  return GetTitles(GetInstalledTitles(), request);
}

IPCCommandResult ES::GetStoredTMDSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  if (!Loader.IsValid() || !Loader.GetTMD().IsValid())
    return GetDefaultReply(FS_ENOENT);

  const u32 tmd_size = static_cast<u32>(Loader.GetTMD().GetRawTMD().size());
  Memory::Write_U32(tmd_size, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETSTOREDTMDSIZE: title: %08x/%08x (view size %i)",
           (u32)(TitleID >> 32), (u32)TitleID, tmd_size);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetStoredTMD(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  // TODO: actually use this param in when writing to the outbuffer :/
  const u32 MaxCount = Memory::Read_U32(request.in_vectors[1].address);
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  if (!Loader.IsValid() || !Loader.GetTMD().IsValid())
    return GetDefaultReply(FS_ENOENT);

  const std::vector<u8> raw_tmd = Loader.GetTMD().GetRawTMD();
  if (raw_tmd.size() != request.io_vectors[0].size)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());

  INFO_LOG(IOS_ES, "IOCTL_ES_GETSTOREDTMD: title: %08x/%08x (buffer size: %i)",
           (u32)(TitleID >> 32), (u32)TitleID, MaxCount);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetOwnedTitleCount(const IOCtlVRequest& request)
{
  const std::vector<u64> titles = GetTitlesWithTickets();
  INFO_LOG(IOS_ES, "GetOwnedTitleCount: %zu titles", titles.size());
  return GetTitleCount(titles, request);
}

IPCCommandResult ES::GetOwnedTitles(const IOCtlVRequest& request)
{
  return GetTitles(GetTitlesWithTickets(), request);
}

IPCCommandResult ES::GetBoot2Version(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETBOOT2VERSION");

  // as of 26/02/2012, this was latest bootmii version
  Memory::Write_U32(4, request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
