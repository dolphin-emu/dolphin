// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystemProxy.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE
{
static ES::TMDReader FindTMD(FSCore& fs, const std::string& tmd_path, Ticks ticks)
{
  const auto fd = fs.Open(PID_KERNEL, PID_KERNEL, tmd_path, FS::Mode::Read, {}, ticks);
  if (fd.Get() < 0)
    return {};

  std::vector<u8> tmd_bytes(fs.GetFileStatus(fd.Get(), ticks)->size);
  if (!fs.Read(fd.Get(), tmd_bytes.data(), tmd_bytes.size(), ticks))
    return {};

  return ES::TMDReader{std::move(tmd_bytes)};
}

ES::TMDReader ESCore::FindImportTMD(u64 title_id, Ticks ticks) const
{
  return FindTMD(m_ios.GetFSCore(), Common::GetImportTitlePath(title_id) + "/content/title.tmd",
                 ticks);
}

ES::TMDReader ESCore::FindInstalledTMD(u64 title_id, Ticks ticks) const
{
  return FindTMD(m_ios.GetFSCore(), Common::GetTMDFileName(title_id), ticks);
}

ES::TicketReader ESCore::FindSignedTicket(u64 title_id, std::optional<u8> desired_version) const
{
  std::string path = desired_version == 1 ? Common::GetV1TicketFileName(title_id) :
                                            Common::GetTicketFileName(title_id);
  auto ticket_file = m_ios.GetFS()->OpenFile(PID_KERNEL, PID_KERNEL, path, FS::Mode::Read);
  if (!ticket_file)
  {
    if (desired_version)
      // Desired ticket does not exist.
      return {};

    // Check if we are dealing with a v1 ticket
    path = Common::GetV1TicketFileName(title_id);
    ticket_file = m_ios.GetFS()->OpenFile(PID_KERNEL, PID_KERNEL, path, FS::Mode::Read);

    if (!ticket_file)
      return {};
  }

  std::vector<u8> signed_ticket(ticket_file->GetStatus()->size);
  if (!ticket_file->Read(signed_ticket.data(), signed_ticket.size()))
    return {};

  return ES::TicketReader{std::move(signed_ticket)};
}

static bool IsValidPartOfTitleID(const std::string& string)
{
  if (string.length() != 8)
    return false;
  return std::all_of(string.begin(), string.end(),
                     [](const auto character) { return std::isxdigit(character) != 0; });
}

static std::vector<u64> GetTitlesInTitleOrImport(FS::FileSystem* fs, const std::string& titles_dir)
{
  const auto entries = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, titles_dir);
  if (!entries)
  {
    ERROR_LOG_FMT(IOS_ES, "{} is not a directory", titles_dir);
    return {};
  }

  std::vector<u64> title_ids;

  // The /title and /import directories contain one directory per title type, and each of them has
  // a directory per title (where the name is the low 32 bits of the title ID in %08x format).
  for (const std::string& title_type : *entries)
  {
    if (!IsValidPartOfTitleID(title_type))
      continue;

    const std::string title_dir = fmt::format("{}/{}", titles_dir, title_type);
    const auto title_entries = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, title_dir);
    if (!title_entries)
      continue;

    for (const std::string& title_identifier : *title_entries)
    {
      if (!IsValidPartOfTitleID(title_identifier))
        continue;
      if (!fs->ReadDirectory(PID_KERNEL, PID_KERNEL,
                             fmt::format("{}/{}", title_dir, title_identifier)))
      {
        continue;
      }

      const u32 type = std::stoul(title_type, nullptr, 16);
      const u32 identifier = std::stoul(title_identifier, nullptr, 16);
      title_ids.push_back(static_cast<u64>(type) << 32 | identifier);
    }
  }

  return title_ids;
}

std::vector<u64> ESCore::GetInstalledTitles() const
{
  return GetTitlesInTitleOrImport(m_ios.GetFS().get(), "/title");
}

std::vector<u64> ESCore::GetTitleImports() const
{
  return GetTitlesInTitleOrImport(m_ios.GetFS().get(), "/import");
}

std::vector<u64> ESCore::GetTitlesWithTickets() const
{
  const auto fs = m_ios.GetFS();
  const auto entries = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, "/ticket");
  if (!entries)
  {
    ERROR_LOG_FMT(IOS_ES, "/ticket is not a directory");
    return {};
  }

  std::vector<u64> title_ids;

  // The /ticket directory contains one directory per title type, and each of them contains
  // one ticket per title (where the name is the low 32 bits of the title ID in %08x format).
  for (const std::string& title_type : *entries)
  {
    if (!IsValidPartOfTitleID(title_type))
      continue;

    const auto sub_entries = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, "/ticket/" + title_type);
    if (!sub_entries)
      continue;

    for (const std::string& file_name : *sub_entries)
    {
      const std::string name_without_ext = file_name.substr(0, 8);
      if (fs->ReadDirectory(PID_KERNEL, PID_KERNEL,
                            fmt::format("/ticket/{}/{}", title_type, file_name)) ||
          !IsValidPartOfTitleID(name_without_ext) ||
          (name_without_ext + ".tik" != file_name && name_without_ext + ".tv1" != file_name))
      {
        continue;
      }

      const u32 type = std::stoul(title_type, nullptr, 16);
      const u32 identifier = std::stoul(name_without_ext, nullptr, 16);
      title_ids.push_back(static_cast<u64>(type) << 32 | identifier);
    }
  }

  return title_ids;
}

std::vector<ES::Content>
ESCore::GetStoredContentsFromTMD(const ES::TMDReader& tmd,
                                 CheckContentHashes check_content_hashes) const
{
  if (!tmd.IsValid())
    return {};

  const std::vector<ES::Content> contents = tmd.GetContents();

  std::vector<ES::Content> stored_contents;

  std::copy_if(contents.begin(), contents.end(), std::back_inserter(stored_contents),
               [this, &tmd, check_content_hashes](const ES::Content& content) {
                 const auto fs = m_ios.GetFS();

                 const std::string path = GetContentPath(tmd.GetTitleId(), content);
                 if (path.empty())
                   return false;

                 // Check whether the content file exists.
                 const auto file = fs->OpenFile(PID_KERNEL, PID_KERNEL, path, FS::Mode::Read);
                 if (!file.Succeeded())
                   return false;

                 // If content hash checks are disabled, all we have to do is check for existence.
                 if (check_content_hashes == CheckContentHashes::No)
                   return true;

                 // Otherwise, check whether the installed content SHA1 matches the expected hash.
                 std::vector<u8> content_data(file->GetStatus()->size);
                 if (!file->Read(content_data.data(), content_data.size()))
                   return false;
                 return Common::SHA1::CalculateDigest(content_data) == content.sha1;
               });

  return stored_contents;
}

u32 ESCore::GetSharedContentsCount() const
{
  const auto entries = m_ios.GetFS()->ReadDirectory(PID_KERNEL, PID_KERNEL, "/shared1");
  return static_cast<u32>(
      std::count_if(entries->begin(), entries->end(), [this](const std::string& entry) {
        return !m_ios.GetFS()->ReadDirectory(PID_KERNEL, PID_KERNEL, "/shared1/" + entry) &&
               entry.size() == 12 && entry.compare(8, 4, ".app") == 0;
      }));
}

std::vector<std::array<u8, 20>> ESCore::GetSharedContents() const
{
  const ES::SharedContentMap map{m_ios.GetFSCore()};
  return map.GetHashes();
}

static bool DeleteDirectoriesIfEmpty(FS::FileSystem* fs, const std::string& path)
{
  std::string::size_type position = std::string::npos;
  do
  {
    const auto directory = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, path.substr(0, position));
    if ((directory && directory->empty()) ||
        (!directory && directory.Error() != FS::ResultCode::NotFound))
    {
      if (fs->Delete(PID_KERNEL, PID_KERNEL, path.substr(0, position)) != FS::ResultCode::Success)
        return false;
    }
    position = path.find_last_of('/', position - 1);
  } while (position != 0);
  return true;
}

constexpr FS::Modes title_dir_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::Read};
constexpr FS::Modes content_dir_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None};
constexpr FS::Modes data_dir_modes{FS::Mode::ReadWrite, FS::Mode::None, FS::Mode::None};

bool ESCore::CreateTitleDirectories(u64 title_id, u16 group_id) const
{
  const auto fs = m_ios.GetFS();

  const std::string content_dir = Common::GetTitleContentPath(title_id);
  const auto result1 =
      fs->CreateFullPath(PID_KERNEL, PID_KERNEL, content_dir + '/', 0, title_dir_modes);
  const auto result2 =
      fs->SetMetadata(PID_KERNEL, content_dir, PID_KERNEL, PID_KERNEL, 0, content_dir_modes);
  if (result1 != FS::ResultCode::Success || result2 != FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to create or set metadata on content dir for {:016x}", title_id);
    return false;
  }

  const std::string data_dir = Common::GetTitleDataPath(title_id);
  const auto data_dir_contents = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, data_dir);
  if (!data_dir_contents && (data_dir_contents.Error() != FS::ResultCode::NotFound ||
                             fs->CreateDirectory(PID_KERNEL, PID_KERNEL, data_dir, 0,
                                                 data_dir_modes) != FS::ResultCode::Success))
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to create data dir for {:016x}", title_id);
    return false;
  }

  ES::UIDSys uid_sys{m_ios.GetFSCore()};
  const u32 uid = uid_sys.GetOrInsertUIDForTitle(title_id);
  if (fs->SetMetadata(0, data_dir, uid, group_id, 0, data_dir_modes) != FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to set metadata on data dir for {:016x}", title_id);
    return false;
  }

  return true;
}

bool ESCore::InitImport(const ES::TMDReader& tmd)
{
  if (!CreateTitleDirectories(tmd.GetTitleId(), tmd.GetGroupId()))
    return false;

  const auto fs = m_ios.GetFS();
  const std::string import_content_dir = Common::GetImportTitlePath(tmd.GetTitleId()) + "/content";
  const auto result =
      fs->CreateFullPath(PID_KERNEL, PID_KERNEL, import_content_dir + '/', 0, content_dir_modes);
  if (result != FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "InitImport: Failed to create content dir for {:016x}", tmd.GetTitleId());
    return false;
  }

  // IOS moves the title content directory to /import if the TMD exists during an import.
  const auto file_info =
      fs->GetMetadata(PID_KERNEL, PID_KERNEL, Common::GetTMDFileName(tmd.GetTitleId()));
  if (!file_info || !file_info->is_file)
    return true;

  const std::string content_dir = Common::GetTitleContentPath(tmd.GetTitleId());
  const auto rename_result = fs->Rename(PID_KERNEL, PID_KERNEL, content_dir, import_content_dir);
  if (rename_result != FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "InitImport: Failed to move content dir for {:016x}", tmd.GetTitleId());
    return false;
  }
  DeleteDirectoriesIfEmpty(m_ios.GetFS().get(), import_content_dir);
  return true;
}

bool ESCore::FinishImport(const ES::TMDReader& tmd)
{
  const auto fs = m_ios.GetFS();
  const u64 title_id = tmd.GetTitleId();
  const std::string import_content_dir = Common::GetImportTitlePath(title_id) + "/content";

  // Remove everything not listed in the TMD.
  std::unordered_set<std::string> expected_entries = {"title.tmd"};
  for (const auto& content_info : tmd.GetContents())
    expected_entries.insert(fmt::format("{:08x}.app", content_info.id));
  const auto entries = fs->ReadDirectory(PID_KERNEL, PID_KERNEL, import_content_dir);
  if (!entries)
    return false;
  for (const std::string& name : *entries)
  {
    const std::string absolute_path = fmt::format("{}/{}", import_content_dir, name);
    // There should not be any directory in there. Remove it.
    if (fs->ReadDirectory(PID_KERNEL, PID_KERNEL, absolute_path))
      fs->Delete(PID_KERNEL, PID_KERNEL, absolute_path);
    else if (expected_entries.find(name) == expected_entries.end())
      fs->Delete(PID_KERNEL, PID_KERNEL, absolute_path);
  }

  const std::string content_dir = Common::GetTitleContentPath(title_id);
  if (fs->Rename(PID_KERNEL, PID_KERNEL, import_content_dir, content_dir) !=
      FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "FinishImport: Failed to rename import directory to {}", content_dir);
    return false;
  }
  return true;
}

bool ESCore::WriteImportTMD(const ES::TMDReader& tmd)
{
  const auto fs = m_ios.GetFS();
  const std::string tmd_path = "/tmp/title.tmd";
  {
    const auto file = fs->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, tmd_path, content_dir_modes);
    if (!file || !file->Write(tmd.GetBytes().data(), tmd.GetBytes().size()))
      return false;
  }

  const std::string dest =
      fmt::format("{}/content/title.tmd", Common::GetImportTitlePath(tmd.GetTitleId()));
  return fs->Rename(PID_KERNEL, PID_KERNEL, tmd_path, dest) == FS::ResultCode::Success;
}

void ESCore::FinishStaleImport(u64 title_id)
{
  const auto fs = m_ios.GetFS();
  const auto import_tmd = FindImportTMD(title_id);
  if (!import_tmd.IsValid())
  {
    fs->Delete(PID_KERNEL, PID_KERNEL, Common::GetImportTitlePath(title_id) + "/content");
    DeleteDirectoriesIfEmpty(fs.get(), Common::GetImportTitlePath(title_id));
    DeleteDirectoriesIfEmpty(fs.get(), Common::GetTitlePath(title_id));
  }
  else
  {
    FinishImport(import_tmd);
  }
}

void ESCore::FinishAllStaleImports()
{
  const std::vector<u64> titles = GetTitleImports();
  for (const u64& title_id : titles)
    FinishStaleImport(title_id);
}

std::string ESCore::GetContentPath(const u64 title_id, const ES::Content& content,
                                   Ticks ticks) const
{
  if (content.IsShared())
  {
    ES::SharedContentMap content_map{m_ios.GetFSCore()};
    ticks.Add(content_map.GetTicks());
    return content_map.GetFilenameFromSHA1(content.sha1).value_or("");
  }
  return fmt::format("{}/{:08x}.app", Common::GetTitleContentPath(title_id), content.id);
}

s32 ESDevice::WriteSystemFile(const std::string& path, const std::vector<u8>& data, Ticks ticks)
{
  auto& fs = GetEmulationKernel().GetFSCore();
  const std::string tmp_path = "/tmp/" + PathToFileName(path);

  auto result = fs.CreateFile(PID_KERNEL, PID_KERNEL, tmp_path, {},
                              {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None}, ticks);
  if (result != FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to create temporary file {}: {}", tmp_path,
                  static_cast<int>(result));
    return FS::ConvertResult(result);
  }

  auto fd = fs.Open(PID_KERNEL, PID_KERNEL, tmp_path, FS::Mode::ReadWrite, {}, ticks);
  if (fd.Get() < 0)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to open temporary file {}: {}", tmp_path, fd.Get());
    return fd.Get();
  }

  if (fs.Write(fd.Get(), data.data(), u32(data.size()), {}, ticks) != s32(data.size()))
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to write to temporary file {}", tmp_path);
    return ES_EIO;
  }

  if (const auto ret = fs.Close(fd.Release(), ticks); ret != IPC_SUCCESS)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to close temporary file {}", tmp_path);
    return ret;
  }

  result = fs.RenameFile(PID_KERNEL, PID_KERNEL, tmp_path, path, ticks);
  if (result != FS::ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to move launch file to final destination ({}): {}", path,
                  static_cast<int>(result));
    return FS::ConvertResult(result);
  }

  return IPC_SUCCESS;
}

}  // namespace IOS::HLE
