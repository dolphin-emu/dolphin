// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <utility>
#include <vector>

#include <mbedtls/sha1.h>

#include "Common/Align.h"
#include "Common/Crypto/AES.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/ES/NandUtils.h"
#include "Core/ec_wii.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
ReturnCode ES::ImportTicket(const std::vector<u8>& ticket_bytes)
{
  IOS::ES::TicketReader ticket{ticket_bytes};
  if (!ticket.IsValid())
    return ES_EINVAL;

  const u32 ticket_device_id = ticket.GetDeviceId();
  const u32 device_id = EcWii::GetInstance().GetNGID();
  if (ticket_device_id != 0)
  {
    if (device_id != ticket_device_id)
    {
      WARN_LOG(IOS_ES, "Device ID mismatch: ticket %08x, device %08x", ticket_device_id, device_id);
      return ES_DEVICE_ID_MISMATCH;
    }
    const ReturnCode ret = static_cast<ReturnCode>(ticket.Unpersonalise());
    if (ret < 0)
    {
      ERROR_LOG(IOS_ES, "ImportTicket: Failed to unpersonalise ticket for %016" PRIx64 " (%d)",
                ticket.GetTitleId(), ret);
      return ret;
    }
  }

  if (!DiscIO::AddTicket(ticket))
    return ES_EIO;

  INFO_LOG(IOS_ES, "ImportTicket: Imported ticket for title %016" PRIx64, ticket.GetTitleId());
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(bytes.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return GetDefaultReply(ImportTicket(bytes));
}

ReturnCode ES::ImportTmd(Context& context, const std::vector<u8>& tmd_bytes)
{
  // Ioctlv 0x2b writes the TMD to /tmp/title.tmd (for imports) and doesn't seem to write it
  // to either /import or /title. So here we simply have to set the import TMD.
  context.title_import.tmd.SetBytes(tmd_bytes);
  // TODO: validate TMDs and return the proper error code (-1027) if the signature type is invalid.
  if (!context.title_import.tmd.IsValid())
    return ES_EINVAL;

  if (!IOS::ES::InitImport(context.title_import.tmd.GetTitleId()))
    return ES_EIO;

  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTmd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return GetDefaultReply(ImportTmd(context, tmd));
}

static void CleanUpStaleImport(const u64 title_id)
{
  const auto import_tmd = IOS::ES::FindImportTMD(title_id);
  if (!import_tmd.IsValid())
    File::DeleteDirRecursively(Common::GetImportTitlePath(title_id) + "/content");
  else
    IOS::ES::FinishImport(import_tmd);
}

ReturnCode ES::ImportTitleInit(Context& context, const std::vector<u8>& tmd_bytes)
{
  INFO_LOG(IOS_ES, "ImportTitleInit");
  context.title_import.tmd.SetBytes(tmd_bytes);
  if (!context.title_import.tmd.IsValid())
  {
    ERROR_LOG(IOS_ES, "Invalid TMD while adding title (size = %zd)", tmd_bytes.size());
    return ES_EINVAL;
  }

  // Finish a previous import (if it exists).
  CleanUpStaleImport(context.title_import.tmd.GetTitleId());

  if (!IOS::ES::InitImport(context.title_import.tmd.GetTitleId()))
    return ES_EIO;

  // TODO: check and use the other vectors.

  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTitleInit(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(4, 0))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return GetDefaultReply(ImportTitleInit(context, tmd));
}

ReturnCode ES::ImportContentBegin(Context& context, u64 title_id, u32 content_id)
{
  if (context.title_import.content_id != 0xFFFFFFFF)
  {
    ERROR_LOG(IOS_ES, "Trying to add content when we haven't finished adding "
                      "another content. Unsupported.");
    return ES_EINVAL;
  }
  context.title_import.content_id = content_id;

  context.title_import.content_buffer.clear();

  INFO_LOG(IOS_ES, "ImportContentBegin: title %016" PRIx64 ", content ID %08x", title_id,
           context.title_import.content_id);

  if (!context.title_import.tmd.IsValid())
    return ES_EINVAL;

  if (title_id != context.title_import.tmd.GetTitleId())
  {
    ERROR_LOG(IOS_ES, "ImportContentBegin: title id %016" PRIx64 " != "
                      "TMD title id %016" PRIx64 ", ignoring",
              title_id, context.title_import.tmd.GetTitleId());
    return ES_EINVAL;
  }

  // We're supposed to return a "content file descriptor" here, which is
  // passed to further AddContentData / AddContentFinish. But so far there is
  // no known content installer which performs content addition concurrently.
  // Instead we just log an error (see above) if this condition is detected.
  s32 content_fd = 0;
  return static_cast<ReturnCode>(content_fd);
}

IPCCommandResult ES::ImportContentBegin(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return GetDefaultReply(ES_EINVAL);

  u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  u32 content_id = Memory::Read_U32(request.in_vectors[1].address);
  return GetDefaultReply(ImportContentBegin(context, title_id, content_id));
}

ReturnCode ES::ImportContentData(Context& context, u32 content_fd, const u8* data, u32 data_size)
{
  INFO_LOG(IOS_ES, "ImportContentData: content fd %08x, size %d", content_fd, data_size);
  context.title_import.content_buffer.insert(context.title_import.content_buffer.end(), data,
                                             data + data_size);
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportContentData(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return GetDefaultReply(ES_EINVAL);

  u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  u8* data_start = Memory::GetPointer(request.in_vectors[1].address);
  return GetDefaultReply(
      ImportContentData(context, content_fd, data_start, request.in_vectors[1].size));
}

static bool CheckIfContentHashMatches(const std::vector<u8>& content, const IOS::ES::Content& info)
{
  std::array<u8, 20> sha1;
  mbedtls_sha1(content.data(), info.size, sha1.data());
  return sha1 == info.sha1;
}

static std::string GetImportContentPath(u64 title_id, u32 content_id)
{
  return Common::GetImportTitlePath(title_id) + StringFromFormat("/content/%08x.app", content_id);
}

ReturnCode ES::ImportContentEnd(Context& context, u32 content_fd)
{
  INFO_LOG(IOS_ES, "ImportContentEnd: content fd %08x", content_fd);

  if (context.title_import.content_id == 0xFFFFFFFF)
    return ES_EINVAL;

  if (!context.title_import.tmd.IsValid())
    return ES_EINVAL;

  // Try to find the title key from a pre-installed ticket.
  IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(context.title_import.tmd.GetTitleId());
  if (!ticket.IsValid())
    return ES_NO_TICKET;

  // The IV for title content decryption is the lower two bytes of the
  // content index, zero extended.
  IOS::ES::Content content_info;
  if (!context.title_import.tmd.FindContentById(context.title_import.content_id, &content_info))
    return ES_EINVAL;

  u8 iv[16] = {0};
  iv[0] = (content_info.index >> 8) & 0xFF;
  iv[1] = content_info.index & 0xFF;
  std::vector<u8> decrypted_data = Common::AES::Decrypt(ticket.GetTitleKey().data(), iv,
                                                        context.title_import.content_buffer.data(),
                                                        context.title_import.content_buffer.size());
  if (!CheckIfContentHashMatches(decrypted_data, content_info))
  {
    ERROR_LOG(IOS_ES, "ImportContentEnd: Hash for content %08x doesn't match", content_info.id);
    return ES_HASH_MISMATCH;
  }

  std::string content_path;
  if (content_info.IsShared())
  {
    IOS::ES::SharedContentMap shared_content{Common::FROM_SESSION_ROOT};
    content_path = shared_content.AddSharedContent(content_info.sha1);
  }
  else
  {
    content_path = GetImportContentPath(context.title_import.tmd.GetTitleId(),
                                        context.title_import.content_id);
  }
  File::CreateFullPath(content_path);

  const std::string temp_path = Common::RootUserPath(Common::FROM_SESSION_ROOT) +
                                StringFromFormat("/tmp/%08x.app", context.title_import.content_id);
  File::CreateFullPath(temp_path);

  {
    File::IOFile file(temp_path, "wb");
    if (!file.WriteBytes(decrypted_data.data(), content_info.size))
    {
      ERROR_LOG(IOS_ES, "ImportContentEnd: Failed to write to %s", temp_path.c_str());
      return ES_EIO;
    }
  }

  if (!File::Rename(temp_path, content_path))
  {
    ERROR_LOG(IOS_ES, "ImportContentEnd: Failed to move content to %s", content_path.c_str());
    return ES_EIO;
  }

  context.title_import.content_id = 0xFFFFFFFF;
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportContentEnd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  return GetDefaultReply(ImportContentEnd(context, content_fd));
}

ReturnCode ES::ImportTitleDone(Context& context)
{
  if (!context.title_import.tmd.IsValid())
    return ES_EINVAL;

  if (!WriteImportTMD(context.title_import.tmd))
    return ES_EIO;

  if (!FinishImport(context.title_import.tmd))
    return ES_EIO;

  INFO_LOG(IOS_ES, "ImportTitleDone: title %016" PRIx64, context.title_import.tmd.GetTitleId());
  context.title_import.tmd.SetBytes({});
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTitleDone(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return GetDefaultReply(ES_EINVAL);

  return GetDefaultReply(ImportTitleDone(context));
}

ReturnCode ES::ImportTitleCancel(Context& context)
{
  if (!context.title_import.tmd.IsValid())
    return ES_EINVAL;

  CleanUpStaleImport(context.title_import.tmd.GetTitleId());

  INFO_LOG(IOS_ES, "ImportTitleCancel: title %016" PRIx64, context.title_import.tmd.GetTitleId());
  context.title_import.tmd.SetBytes({});
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTitleCancel(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0) || !context.title_import.tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  return GetDefaultReply(ImportTitleCancel(context));
}

static bool CanDeleteTitle(u64 title_id)
{
  // IOS only allows deleting non-system titles (or a system title higher than 00000001-00000101).
  return static_cast<u32>(title_id >> 32) != 0x00000001 || static_cast<u32>(title_id) > 0x101;
}

ReturnCode ES::DeleteTitle(u64 title_id)
{
  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  const std::string title_dir = Common::GetTitlePath(title_id, Common::FROM_SESSION_ROOT);
  if (!File::IsDirectory(title_dir))
    return FS_ENOENT;

  if (!File::DeleteDirRecursively(title_dir))
  {
    ERROR_LOG(IOS_ES, "DeleteTitle: Failed to delete title directory: %s", title_dir.c_str());
    return FS_EACCESS;
  }
  // XXX: ugly, but until we drop CNANDContentManager everywhere, this is going to be needed.
  DiscIO::CNANDContentManager::Access().ClearCache();

  return IPC_SUCCESS;
}

IPCCommandResult ES::DeleteTitle(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  return GetDefaultReply(DeleteTitle(title_id));
}

ReturnCode ES::DeleteTicket(const u8* ticket_view)
{
  const u64 title_id = Common::swap64(ticket_view + offsetof(IOS::ES::TicketView, title_id));

  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  auto ticket = DiscIO::FindSignedTicket(title_id);
  if (!ticket.IsValid())
    return FS_ENOENT;

  const u64 ticket_id = Common::swap64(ticket_view + offsetof(IOS::ES::TicketView, ticket_id));
  ticket.DeleteTicket(ticket_id);

  const std::vector<u8>& new_ticket = ticket.GetRawTicket();
  const std::string ticket_path = Common::GetTicketFileName(title_id, Common::FROM_SESSION_ROOT);
  {
    File::IOFile ticket_file(ticket_path, "wb");
    if (!ticket_file || !ticket_file.WriteBytes(new_ticket.data(), new_ticket.size()))
      return ES_EIO;
  }

  // Delete the ticket file if it is now empty.
  if (new_ticket.empty())
    File::Delete(ticket_path);

  // Delete the ticket directory if it is now empty.
  const std::string ticket_parent_dir =
      Common::RootUserPath(Common::FROM_CONFIGURED_ROOT) +
      StringFromFormat("/ticket/%08x", static_cast<u32>(title_id >> 32));
  const auto ticket_parent_dir_entries = File::ScanDirectoryTree(ticket_parent_dir, false);
  if (ticket_parent_dir_entries.children.empty())
    File::DeleteDir(ticket_parent_dir);

  return IPC_SUCCESS;
}

IPCCommandResult ES::DeleteTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) ||
      request.in_vectors[0].size != sizeof(IOS::ES::TicketView))
  {
    return GetDefaultReply(ES_EINVAL);
  }
  return GetDefaultReply(DeleteTicket(Memory::GetPointer(request.in_vectors[0].address)));
}

ReturnCode ES::DeleteTitleContent(u64 title_id) const
{
  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  const std::string content_dir = Common::GetTitleContentPath(title_id, Common::FROM_SESSION_ROOT);
  if (!File::IsDirectory(content_dir))
    return FS_ENOENT;

  for (const auto& file : File::ScanDirectoryTree(content_dir, false).children)
  {
    if (file.virtualName.size() == 12 && file.virtualName.compare(8, 4, ".app") == 0)
      File::Delete(file.physicalName);
  }

  return IPC_SUCCESS;
}

IPCCommandResult ES::DeleteTitleContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u64))
    return GetDefaultReply(ES_EINVAL);
  return GetDefaultReply(DeleteTitleContent(Memory::Read_U64(request.in_vectors[0].address)));
}

ReturnCode ES::ExportTitleInit(Context& context, u64 title_id, u8* tmd_bytes, u32 tmd_size)
{
  // No concurrent title import/export is allowed.
  if (context.title_export.valid)
    return ES_EINVAL;

  const auto tmd = IOS::ES::FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return FS_ENOENT;

  context.title_export.tmd = tmd;

  const auto ticket = DiscIO::FindSignedTicket(context.title_export.tmd.GetTitleId());
  if (!ticket.IsValid())
    return ES_NO_TICKET;
  if (ticket.GetTitleId() != context.title_export.tmd.GetTitleId())
    return ES_EINVAL;

  context.title_export.title_key = ticket.GetTitleKey();

  const auto& raw_tmd = context.title_export.tmd.GetRawTMD();
  if (tmd_size != raw_tmd.size())
    return ES_EINVAL;

  std::copy_n(raw_tmd.cbegin(), raw_tmd.size(), tmd_bytes);

  context.title_export.valid = true;
  return IPC_SUCCESS;
}

IPCCommandResult ES::ExportTitleInit(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  u8* tmd_bytes = Memory::GetPointer(request.io_vectors[0].address);
  const u32 tmd_size = request.io_vectors[0].size;

  return GetDefaultReply(ExportTitleInit(context, title_id, tmd_bytes, tmd_size));
}

ReturnCode ES::ExportContentBegin(Context& context, u64 title_id, u32 content_id)
{
  if (!context.title_export.valid || context.title_export.tmd.GetTitleId() != title_id)
  {
    ERROR_LOG(IOS_ES, "Tried to use ExportContentBegin with an invalid title export context.");
    return ES_EINVAL;
  }

  const auto& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
    return FS_ENOENT;

  const auto* content = content_loader.GetContentByID(content_id);
  if (!content)
    return ES_EINVAL;

  OpenedContent entry;
  entry.m_position = 0;
  entry.m_content = content->m_metadata;
  entry.m_title_id = title_id;
  content->m_Data->Open();

  u32 cfd = 0;
  while (context.title_export.contents.find(cfd) != context.title_export.contents.end())
    cfd++;

  TitleExportContext::ExportContent content_export;
  content_export.content = std::move(entry);
  content_export.iv[0] = (content->m_metadata.index >> 8) & 0xFF;
  content_export.iv[1] = content->m_metadata.index & 0xFF;

  context.title_export.contents.emplace(cfd, content_export);
  // IOS returns a content ID which is passed to further content calls.
  return static_cast<ReturnCode>(cfd);
}

IPCCommandResult ES::ExportContentBegin(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0) || request.in_vectors[0].size != 8 ||
      request.in_vectors[1].size != 4)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const u32 content_id = Memory::Read_U32(request.in_vectors[1].address);

  return GetDefaultReply(ExportContentBegin(context, title_id, content_id));
}

ReturnCode ES::ExportContentData(Context& context, u32 content_fd, u8* data, u32 data_size)
{
  const auto iterator = context.title_export.contents.find(content_fd);
  if (!context.title_export.valid || iterator == context.title_export.contents.end() ||
      iterator->second.content.m_position >= iterator->second.content.m_content.size)
  {
    return ES_EINVAL;
  }

  auto& metadata = iterator->second.content;

  const auto& content_loader = AccessContentDevice(metadata.m_title_id);
  const auto* content = content_loader.GetContentByID(metadata.m_content.id);
  content->m_Data->Open();

  const u32 length =
      std::min(static_cast<u32>(metadata.m_content.size - metadata.m_position), data_size);
  std::vector<u8> buffer(length);

  if (!content->m_Data->GetRange(metadata.m_position, length, buffer.data()))
  {
    ERROR_LOG(IOS_ES, "ExportContentData: ES_SHORT_READ");
    return ES_SHORT_READ;
  }

  // IOS aligns the buffer to 32 bytes. Since we also need to align it to 16 bytes,
  // let's just follow IOS here.
  buffer.resize(Common::AlignUp(buffer.size(), 32));

  const std::vector<u8> output =
      Common::AES::Encrypt(context.title_export.title_key.data(), iterator->second.iv.data(),
                           buffer.data(), buffer.size());
  std::copy_n(output.cbegin(), output.size(), data);
  metadata.m_position += length;
  return IPC_SUCCESS;
}

IPCCommandResult ES::ExportContentData(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 4 ||
      request.io_vectors[0].size == 0)
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  u8* data = Memory::GetPointer(request.io_vectors[0].address);
  const u32 bytes_to_read = request.io_vectors[0].size;

  return GetDefaultReply(ExportContentData(context, content_fd, data, bytes_to_read));
}

ReturnCode ES::ExportContentEnd(Context& context, u32 content_fd)
{
  const auto iterator = context.title_export.contents.find(content_fd);
  if (!context.title_export.valid || iterator == context.title_export.contents.end() ||
      iterator->second.content.m_position != iterator->second.content.m_content.size)
  {
    return ES_EINVAL;
  }

  // XXX: Check the content hash, as IOS does?

  const auto& content_loader = AccessContentDevice(iterator->second.content.m_title_id);
  content_loader.GetContentByID(iterator->second.content.m_content.id)->m_Data->Close();

  context.title_export.contents.erase(iterator);
  return IPC_SUCCESS;
}

IPCCommandResult ES::ExportContentEnd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 4)
    return GetDefaultReply(ES_EINVAL);

  const u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  return GetDefaultReply(ExportContentEnd(context, content_fd));
}

ReturnCode ES::ExportTitleDone(Context& context)
{
  if (!context.title_export.valid)
    return ES_EINVAL;

  context.title_export.valid = false;
  return IPC_SUCCESS;
}

IPCCommandResult ES::ExportTitleDone(Context& context, const IOCtlVRequest& request)
{
  return GetDefaultReply(ExportTitleDone(context));
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
