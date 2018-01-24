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
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Core/CommonTitles.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/ec_wii.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
static ReturnCode WriteTicket(const IOS::ES::TicketReader& ticket)
{
  const u64 title_id = ticket.GetTitleId();

  const std::string ticket_path = Common::GetTicketFileName(title_id, Common::FROM_SESSION_ROOT);
  File::CreateFullPath(ticket_path);

  File::IOFile ticket_file(ticket_path, "wb");
  if (!ticket_file)
    return ES_EIO;

  const std::vector<u8>& raw_ticket = ticket.GetBytes();
  return ticket_file.WriteBytes(raw_ticket.data(), raw_ticket.size()) ? IPC_SUCCESS : ES_EIO;
}

void ES::TitleImportExportContext::DoState(PointerWrap& p)
{
  p.Do(valid);
  p.Do(key_handle);
  tmd.DoState(p);
  p.Do(content.valid);
  p.Do(content.id);
  p.Do(content.iv);
  p.Do(content.buffer);
}

ReturnCode ES::ImportTicket(const std::vector<u8>& ticket_bytes, const std::vector<u8>& cert_chain)
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
    const ReturnCode ret = ticket.Unpersonalise(m_ios.GetIOSC());
    if (ret < 0)
    {
      ERROR_LOG(IOS_ES, "ImportTicket: Failed to unpersonalise ticket for %016" PRIx64 " (%d)",
                ticket.GetTitleId(), ret);
      return ret;
    }
  }

  const ReturnCode verify_ret =
      VerifyContainer(VerifyContainerType::Ticket, VerifyMode::UpdateCertStore, ticket, cert_chain);
  if (verify_ret != IPC_SUCCESS)
    return verify_ret;

  const ReturnCode write_ret = WriteTicket(ticket);
  if (write_ret != IPC_SUCCESS)
    return write_ret;

  INFO_LOG(IOS_ES, "ImportTicket: Imported ticket for title %016" PRIx64, ticket.GetTitleId());
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(bytes.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  std::vector<u8> cert_chain(request.in_vectors[1].size);
  Memory::CopyFromEmu(cert_chain.data(), request.in_vectors[1].address, request.in_vectors[1].size);
  return GetDefaultReply(ImportTicket(bytes, cert_chain));
}

constexpr std::array<u8, 16> NULL_KEY{};

// Used for exporting titles and importing them back (ImportTmd and ExportTitleInit).
static ReturnCode InitBackupKey(const IOS::ES::TMDReader& tmd, IOSC& iosc, IOSC::Handle* key)
{
  // Some versions of IOS have a bug that causes it to use a zeroed key instead of the PRNG key.
  // When Nintendo decided to fix it, they added checks to keep using the zeroed key only in
  // affected titles to avoid making existing exports useless.

  // Ignore the region byte.
  const u64 title_id = tmd.GetTitleId() | 0xff;
  const u32 title_flags = tmd.GetTitleFlags();
  const u32 affected_type = IOS::ES::TITLE_TYPE_0x10 | IOS::ES::TITLE_TYPE_DATA;
  if (title_id == Titles::SYSTEM_MENU || (title_flags & affected_type) != affected_type ||
      !(title_id == 0x00010005735841ff || title_id - 0x00010005735a41ff <= 0x700))
  {
    *key = IOSC::HANDLE_PRNG_KEY;
    return IPC_SUCCESS;
  }

  // Otherwise, use a null key.
  ReturnCode ret = iosc.CreateObject(key, IOSC::TYPE_SECRET_KEY, IOSC::SUBTYPE_AES128, PID_ES);
  return ret == IPC_SUCCESS ? iosc.ImportSecretKey(*key, NULL_KEY.data(), PID_ES) : ret;
}

static void ResetTitleImportContext(ES::Context* context, IOSC& iosc)
{
  if (context->title_import_export.key_handle)
    iosc.DeleteObject(context->title_import_export.key_handle, PID_ES);
  context->title_import_export = {};
}

ReturnCode ES::ImportTmd(Context& context, const std::vector<u8>& tmd_bytes)
{
  // Ioctlv 0x2b writes the TMD to /tmp/title.tmd (for imports) and doesn't seem to write it
  // to either /import or /title. So here we simply have to set the import TMD.
  ResetTitleImportContext(&context, m_ios.GetIOSC());
  context.title_import_export.tmd.SetBytes(tmd_bytes);
  if (!context.title_import_export.tmd.IsValid())
    return ES_EINVAL;

  std::vector<u8> cert_store;
  ReturnCode ret = ReadCertStore(&cert_store);
  if (ret != IPC_SUCCESS)
    return ret;

  ret = VerifyContainer(VerifyContainerType::TMD, VerifyMode::UpdateCertStore,
                        context.title_import_export.tmd, cert_store);
  if (ret != IPC_SUCCESS)
    return ret;

  if (!InitImport(context.title_import_export.tmd.GetTitleId()))
    return ES_EIO;

  ret = InitBackupKey(GetTitleContext().tmd, m_ios.GetIOSC(),
                      &context.title_import_export.key_handle);
  if (ret != IPC_SUCCESS)
    return ret;

  context.title_import_export.valid = true;
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTmd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  if (!IOS::ES::IsValidTMDSize(request.in_vectors[0].size))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return GetDefaultReply(ImportTmd(context, tmd));
}

static ReturnCode InitTitleImportKey(const std::vector<u8>& ticket_bytes, IOSC& iosc,
                                     IOSC::Handle* handle)
{
  ReturnCode ret = iosc.CreateObject(handle, IOSC::TYPE_SECRET_KEY, IOSC::SUBTYPE_AES128, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  std::array<u8, 16> iv{};
  std::copy_n(&ticket_bytes[offsetof(IOS::ES::Ticket, title_id)], sizeof(u64), iv.begin());
  const u8 index = ticket_bytes[offsetof(IOS::ES::Ticket, common_key_index)];
  if (index > 1)
    return ES_INVALID_TICKET;

  return iosc.ImportSecretKey(
      *handle, index == 0 ? IOSC::HANDLE_COMMON_KEY : IOSC::HANDLE_NEW_COMMON_KEY, iv.data(),
      &ticket_bytes[offsetof(IOS::ES::Ticket, title_key)], PID_ES);
}

ReturnCode ES::ImportTitleInit(Context& context, const std::vector<u8>& tmd_bytes,
                               const std::vector<u8>& cert_chain)
{
  INFO_LOG(IOS_ES, "ImportTitleInit");
  ResetTitleImportContext(&context, m_ios.GetIOSC());
  context.title_import_export.tmd.SetBytes(tmd_bytes);
  if (!context.title_import_export.tmd.IsValid())
  {
    ERROR_LOG(IOS_ES, "Invalid TMD while adding title (size = %zd)", tmd_bytes.size());
    return ES_EINVAL;
  }

  // Finish a previous import (if it exists).
  FinishStaleImport(context.title_import_export.tmd.GetTitleId());

  ReturnCode ret = VerifyContainer(VerifyContainerType::TMD, VerifyMode::UpdateCertStore,
                                   context.title_import_export.tmd, cert_chain);
  if (ret != IPC_SUCCESS)
    return ret;

  const auto ticket = DiscIO::FindSignedTicket(context.title_import_export.tmd.GetTitleId());
  if (!ticket.IsValid())
    return ES_NO_TICKET;

  std::vector<u8> cert_store;
  ret = ReadCertStore(&cert_store);
  if (ret != IPC_SUCCESS)
    return ret;

  ret = VerifyContainer(VerifyContainerType::Ticket, VerifyMode::DoNotUpdateCertStore, ticket,
                        cert_store);
  if (ret != IPC_SUCCESS)
    return ret;

  ret = InitTitleImportKey(ticket.GetBytes(), m_ios.GetIOSC(),
                           &context.title_import_export.key_handle);
  if (ret != IPC_SUCCESS)
    return ret;

  if (!InitImport(context.title_import_export.tmd.GetTitleId()))
    return ES_EIO;

  context.title_import_export.valid = true;
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTitleInit(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(4, 0))
    return GetDefaultReply(ES_EINVAL);

  if (!IOS::ES::IsValidTMDSize(request.in_vectors[0].size))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  std::vector<u8> certs(request.in_vectors[1].size);
  Memory::CopyFromEmu(certs.data(), request.in_vectors[1].address, request.in_vectors[1].size);
  return GetDefaultReply(ImportTitleInit(context, tmd, certs));
}

ReturnCode ES::ImportContentBegin(Context& context, u64 title_id, u32 content_id)
{
  if (context.title_import_export.content.valid)
  {
    ERROR_LOG(IOS_ES, "Trying to add content when we haven't finished adding "
                      "another content. Unsupported.");
    return ES_EINVAL;
  }
  context.title_import_export.content = {};
  context.title_import_export.content.id = content_id;

  INFO_LOG(IOS_ES, "ImportContentBegin: title %016" PRIx64 ", content ID %08x", title_id,
           context.title_import_export.content.id);

  if (!context.title_import_export.valid)
    return ES_EINVAL;

  if (title_id != context.title_import_export.tmd.GetTitleId())
  {
    ERROR_LOG(IOS_ES, "ImportContentBegin: title id %016" PRIx64 " != "
                      "TMD title id %016" PRIx64 ", ignoring",
              title_id, context.title_import_export.tmd.GetTitleId());
    return ES_EINVAL;
  }

  // The IV for title content decryption is the lower two bytes of the
  // content index, zero extended.
  IOS::ES::Content content_info;
  if (!context.title_import_export.tmd.FindContentById(context.title_import_export.content.id,
                                                       &content_info))
    return ES_EINVAL;
  context.title_import_export.content.iv[0] = (content_info.index >> 8) & 0xFF;
  context.title_import_export.content.iv[1] = content_info.index & 0xFF;

  context.title_import_export.content.valid = true;

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
  context.title_import_export.content.buffer.insert(
      context.title_import_export.content.buffer.end(), data, data + data_size);
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

  if (!context.title_import_export.valid || !context.title_import_export.content.valid)
    return ES_EINVAL;

  std::vector<u8> decrypted_data(context.title_import_export.content.buffer.size());
  const ReturnCode decrypt_ret = m_ios.GetIOSC().Decrypt(
      context.title_import_export.key_handle, context.title_import_export.content.iv.data(),
      context.title_import_export.content.buffer.data(),
      context.title_import_export.content.buffer.size(), decrypted_data.data(), PID_ES);
  if (decrypt_ret != IPC_SUCCESS)
    return decrypt_ret;

  IOS::ES::Content content_info;
  context.title_import_export.tmd.FindContentById(context.title_import_export.content.id,
                                                  &content_info);
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
    content_path = GetImportContentPath(context.title_import_export.tmd.GetTitleId(),
                                        context.title_import_export.content.id);
  }
  File::CreateFullPath(content_path);

  const std::string temp_path =
      Common::RootUserPath(Common::FROM_SESSION_ROOT) +
      StringFromFormat("/tmp/%08x.app", context.title_import_export.content.id);
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

  context.title_import_export.content = {};
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
  if (!context.title_import_export.valid || context.title_import_export.content.valid)
    return ES_EINVAL;

  // Make sure all listed, non-optional contents have been imported.
  const u64 title_id = context.title_import_export.tmd.GetTitleId();
  const std::vector<IOS::ES::Content> contents = context.title_import_export.tmd.GetContents();
  const IOS::ES::SharedContentMap shared_content_map{Common::FROM_SESSION_ROOT};
  const bool has_all_required_contents =
      std::all_of(contents.cbegin(), contents.cend(), [&](const IOS::ES::Content& content) {
        if (content.IsOptional())
          return true;

        if (content.IsShared())
          return shared_content_map.GetFilenameFromSHA1(content.sha1).has_value();

        // Note: the import hasn't been finalised yet, so the whole title directory
        // is still in /import, not /title.
        return File::Exists(Common::GetImportTitlePath(title_id, Common::FROM_SESSION_ROOT) +
                            StringFromFormat("/content/%08x.app", content.id));
      });
  if (!has_all_required_contents)
    return ES_EINVAL;

  if (!WriteImportTMD(context.title_import_export.tmd))
    return ES_EIO;

  if (!FinishImport(context.title_import_export.tmd))
    return ES_EIO;

  INFO_LOG(IOS_ES, "ImportTitleDone: title %016" PRIx64, title_id);
  ResetTitleImportContext(&context, m_ios.GetIOSC());
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
  // The TMD buffer can exist without a valid title import context.
  if (context.title_import_export.tmd.GetBytes().empty() ||
      context.title_import_export.content.valid)
    return ES_EINVAL;

  if (context.title_import_export.valid)
  {
    const u64 title_id = context.title_import_export.tmd.GetTitleId();
    FinishStaleImport(title_id);
    INFO_LOG(IOS_ES, "ImportTitleCancel: title %016" PRIx64, title_id);
  }

  ResetTitleImportContext(&context, m_ios.GetIOSC());
  return IPC_SUCCESS;
}

IPCCommandResult ES::ImportTitleCancel(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
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
  // XXX: ugly, but until we drop NANDContentManager everywhere, this is going to be needed.
  DiscIO::NANDContentManager::Access().ClearCache();

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

  const std::vector<u8>& new_ticket = ticket.GetBytes();
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

ReturnCode ES::DeleteContent(u64 title_id, u32 content_id) const
{
  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return FS_ENOENT;

  IOS::ES::Content content;
  if (!tmd.FindContentById(content_id, &content))
    return ES_EINVAL;

  if (!File::Delete(Common::GetTitleContentPath(title_id, Common::FROM_SESSION_ROOT) +
                    StringFromFormat("%08x.app", content_id)))
  {
    return FS_ENOENT;
  }

  return IPC_SUCCESS;
}

IPCCommandResult ES::DeleteContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0) || request.in_vectors[0].size != sizeof(u64) ||
      request.in_vectors[1].size != sizeof(u32))
  {
    return GetDefaultReply(ES_EINVAL);
  }
  return GetDefaultReply(DeleteContent(Memory::Read_U64(request.in_vectors[0].address),
                                       Memory::Read_U32(request.in_vectors[1].address)));
}

ReturnCode ES::ExportTitleInit(Context& context, u64 title_id, u8* tmd_bytes, u32 tmd_size)
{
  // No concurrent title import/export is allowed.
  if (context.title_import_export.valid)
    return ES_EINVAL;

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return FS_ENOENT;

  ResetTitleImportContext(&context, m_ios.GetIOSC());
  context.title_import_export.tmd = tmd;

  const ReturnCode ret = InitBackupKey(GetTitleContext().tmd, m_ios.GetIOSC(),
                                       &context.title_import_export.key_handle);
  if (ret != IPC_SUCCESS)
    return ret;

  const std::vector<u8>& raw_tmd = context.title_import_export.tmd.GetBytes();
  if (tmd_size != raw_tmd.size())
    return ES_EINVAL;

  std::copy_n(raw_tmd.cbegin(), raw_tmd.size(), tmd_bytes);

  context.title_import_export.valid = true;
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
  context.title_import_export.content = {};
  if (!context.title_import_export.valid ||
      context.title_import_export.tmd.GetTitleId() != title_id)
  {
    ERROR_LOG(IOS_ES, "Tried to use ExportContentBegin with an invalid title export context.");
    return ES_EINVAL;
  }

  IOS::ES::Content content_info;
  if (!context.title_import_export.tmd.FindContentById(content_id, &content_info))
    return ES_EINVAL;

  context.title_import_export.content.id = content_id;
  context.title_import_export.content.valid = true;

  const s32 ret = OpenContent(context.title_import_export.tmd, content_info.index, 0);
  if (ret < 0)
  {
    ResetTitleImportContext(&context, m_ios.GetIOSC());
    return static_cast<ReturnCode>(ret);
  }

  context.title_import_export.content.iv[0] = (content_info.index >> 8) & 0xFF;
  context.title_import_export.content.iv[1] = content_info.index & 0xFF;

  // IOS returns a content ID which is passed to further content calls.
  return static_cast<ReturnCode>(ret);
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
  if (!context.title_import_export.valid || !context.title_import_export.content.valid || !data ||
      data_size == 0)
  {
    CloseContent(content_fd, 0);
    context.title_import_export = {};
    return ES_EINVAL;
  }

  std::vector<u8> buffer(data_size);
  const s32 read_size = ReadContent(content_fd, buffer.data(), static_cast<u32>(buffer.size()), 0);
  if (read_size < 0)
  {
    CloseContent(content_fd, 0);
    ResetTitleImportContext(&context, m_ios.GetIOSC());
    return ES_SHORT_READ;
  }

  // IOS aligns the buffer to 32 bytes. Since we also need to align it to 16 bytes,
  // let's just follow IOS here.
  buffer.resize(Common::AlignUp(buffer.size(), 32));

  std::vector<u8> output(buffer.size());
  const ReturnCode decrypt_ret = m_ios.GetIOSC().Encrypt(
      context.title_import_export.key_handle, context.title_import_export.content.iv.data(),
      buffer.data(), buffer.size(), output.data(), PID_ES);
  if (decrypt_ret != IPC_SUCCESS)
    return decrypt_ret;

  std::copy(output.cbegin(), output.cend(), data);
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
  if (!context.title_import_export.valid || !context.title_import_export.content.valid)
    return ES_EINVAL;
  return CloseContent(content_fd, 0);
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
  ResetTitleImportContext(&context, m_ios.GetIOSC());
  return IPC_SUCCESS;
}

IPCCommandResult ES::ExportTitleDone(Context& context, const IOCtlVRequest& request)
{
  return GetDefaultReply(ExportTitleDone(context));
}

ReturnCode ES::DeleteSharedContent(const std::array<u8, 20>& sha1) const
{
  IOS::ES::SharedContentMap map{Common::FromWhichRoot::FROM_SESSION_ROOT};
  const auto content_path = map.GetFilenameFromSHA1(sha1);
  if (!content_path)
    return ES_EINVAL;

  // Check whether the shared content is used by a system title.
  const std::vector<u64> titles = GetInstalledTitles();
  const bool is_used_by_system_title = std::any_of(titles.begin(), titles.end(), [&](u64 id) {
    if (!IOS::ES::IsTitleType(id, IOS::ES::TitleType::System))
      return false;

    const auto tmd = FindInstalledTMD(id);
    if (!tmd.IsValid())
      return true;

    const auto contents = tmd.GetContents();
    return std::any_of(contents.begin(), contents.end(),
                       [&sha1](const auto& content) { return content.sha1 == sha1; });
  });

  // Any shared content used by a system title cannot be deleted.
  if (is_used_by_system_title)
    return ES_EINVAL;

  // Delete the shared content and update the content map.
  if (!File::Delete(*content_path))
    return FS_ENOENT;

  if (!map.DeleteSharedContent(sha1))
    return ES_EIO;

  return IPC_SUCCESS;
}

IPCCommandResult ES::DeleteSharedContent(const IOCtlVRequest& request)
{
  std::array<u8, 20> sha1;
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sha1.size())
    return GetDefaultReply(ES_EINVAL);
  Memory::CopyFromEmu(sha1.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return GetDefaultReply(DeleteSharedContent(sha1));
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
