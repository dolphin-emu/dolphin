// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "Common/Align.h"
#include "Common/Crypto/SHA1.h"
#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/CommonTitles.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/Uids.h"
#include "Core/System.h"

namespace IOS::HLE
{
static ReturnCode WriteTicket(FS::FileSystem* fs, const ES::TicketReader& ticket)
{
  const u64 title_id = ticket.GetTitleId();
  const std::string path = ticket.IsV1Ticket() ? Common::GetV1TicketFileName(title_id) :
                                                 Common::GetTicketFileName(title_id);

  constexpr FS::Modes ticket_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None};
  fs->CreateFullPath(PID_KERNEL, PID_KERNEL, path, 0, ticket_modes);
  const auto file = fs->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, path, ticket_modes);
  if (!file)
    return FS::ConvertResult(file.Error());

  const std::vector<u8>& raw_ticket = ticket.GetBytes();
  return file->Write(raw_ticket.data(), raw_ticket.size()) ? IPC_SUCCESS : ES_EIO;
}

void ESCore::TitleImportExportContext::DoState(PointerWrap& p)
{
  p.Do(valid);
  p.Do(key_handle);
  tmd.DoState(p);
  p.Do(content.valid);
  p.Do(content.id);
  p.Do(content.iv);
  p.Do(content.buffer);
}

ReturnCode ESCore::ImportTicket(const std::vector<u8>& ticket_bytes,
                                const std::vector<u8>& cert_chain, TicketImportType type,
                                VerifySignature verify_signature)
{
  ES::TicketReader ticket{ticket_bytes};
  if (!ticket.IsValid())
    return ES_EINVAL;

  const u32 ticket_device_id = ticket.GetDeviceId();
  const u32 device_id = m_ios.GetIOSC().GetDeviceId();
  if (type == TicketImportType::PossiblyPersonalised && ticket_device_id != 0)
  {
    if (device_id != ticket_device_id)
    {
      WARN_LOG_FMT(IOS_ES, "Device ID mismatch: ticket {:08x}, device {:08x}", ticket_device_id,
                   device_id);
      return ES_DEVICE_ID_MISMATCH;
    }
    const ReturnCode ret = ticket.Unpersonalise(m_ios.GetIOSC());
    if (ret < 0)
    {
      ERROR_LOG_FMT(IOS_ES, "ImportTicket: Failed to unpersonalise ticket for {:016x} ({})",
                    ticket.GetTitleId(), Common::ToUnderlying(ret));
      return ret;
    }
  }

  if (verify_signature != VerifySignature::No)
  {
    const ReturnCode verify_ret = VerifyContainer(VerifyContainerType::Ticket,
                                                  VerifyMode::UpdateCertStore, ticket, cert_chain);
    if (verify_ret != IPC_SUCCESS)
      return verify_ret;
  }

  const ReturnCode write_ret = WriteTicket(m_ios.GetFS().get(), ticket);
  if (write_ret != IPC_SUCCESS)
    return write_ret;

  INFO_LOG_FMT(IOS_ES, "ImportTicket: Imported ticket for title {:016x}", ticket.GetTitleId());
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  std::vector<u8> bytes(request.in_vectors[0].size);
  memory.CopyFromEmu(bytes.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  std::vector<u8> cert_chain(request.in_vectors[1].size);
  memory.CopyFromEmu(cert_chain.data(), request.in_vectors[1].address, request.in_vectors[1].size);
  return IPCReply(m_core.ImportTicket(bytes, cert_chain));
}

constexpr std::array<u8, 16> NULL_KEY{};

// Used for exporting titles and importing them back (ImportTmd and ExportTitleInit).
static ReturnCode InitBackupKey(u64 tid, u32 title_flags, IOSC& iosc, IOSC::Handle* key)
{
  // Some versions of IOS have a bug that causes it to use a zeroed key instead of the PRNG key.
  // When Nintendo decided to fix it, they added checks to keep using the zeroed key only in
  // affected titles to avoid making existing exports useless.

  // Ignore the region byte.
  const u64 title_id = tid | 0xff;
  constexpr u32 affected_type = ES::TITLE_TYPE_0x10 | ES::TITLE_TYPE_DATA;
  if (title_id == Titles::SYSTEM_MENU || (title_flags & affected_type) != affected_type ||
      !(title_id == 0x00010005735841ff || title_id - 0x00010005735a41ff <= 0x700))
  {
    *key = IOSC::HANDLE_PRNG_KEY;
    return IPC_SUCCESS;
  }

  // Otherwise, use a null key.
  ReturnCode ret =
      iosc.CreateObject(key, IOSC::TYPE_SECRET_KEY, IOSC::ObjectSubType::AES128, PID_ES);
  return ret == IPC_SUCCESS ? iosc.ImportSecretKey(*key, NULL_KEY.data(), PID_ES) : ret;
}

static void ResetTitleImportContext(ESCore::Context* context, IOSC& iosc)
{
  if (context->title_import_export.key_handle)
    iosc.DeleteObject(context->title_import_export.key_handle, PID_ES);
  context->title_import_export = {};
}

ReturnCode ESCore::ImportTmd(Context& context, const std::vector<u8>& tmd_bytes,
                             u64 caller_title_id, u32 caller_title_flags)
{
  INFO_LOG_FMT(IOS_ES, "ImportTmd");

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
  {
    ERROR_LOG_FMT(IOS_ES, "ImportTmd: VerifyContainer failed with error {}",
                  Common::ToUnderlying(ret));
    return ret;
  }

  if (!InitImport(context.title_import_export.tmd))
  {
    ERROR_LOG_FMT(IOS_ES, "ImportTmd: Failed to initialise title import");
    return ES_EIO;
  }

  ret = InitBackupKey(caller_title_id, caller_title_flags, m_ios.GetIOSC(),
                      &context.title_import_export.key_handle);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG_FMT(IOS_ES, "ImportTmd: InitBackupKey failed with error {}",
                  Common::ToUnderlying(ret));
    return ret;
  }

  INFO_LOG_FMT(IOS_ES, "ImportTmd: All checks passed, marking context as valid");
  context.title_import_export.valid = true;
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportTmd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return IPCReply(ES_EINVAL);

  if (!ES::IsValidTMDSize(request.in_vectors[0].size))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  std::vector<u8> tmd(request.in_vectors[0].size);
  memory.CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return IPCReply(m_core.ImportTmd(context, tmd, m_core.m_title_context.tmd.GetTitleId(),
                                   m_core.m_title_context.tmd.GetTitleFlags()));
}

static ReturnCode InitTitleImportKey(const std::vector<u8>& ticket_bytes, IOSC& iosc,
                                     IOSC::Handle* handle)
{
  ReturnCode ret =
      iosc.CreateObject(handle, IOSC::TYPE_SECRET_KEY, IOSC::ObjectSubType::AES128, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  std::array<u8, 16> iv{};
  std::copy_n(&ticket_bytes[offsetof(ES::Ticket, title_id)], sizeof(u64), iv.begin());
  const u8 index = ticket_bytes[offsetof(ES::Ticket, common_key_index)];
  if (index >= IOSC::COMMON_KEY_HANDLES.size())
    return ES_INVALID_TICKET;

  return iosc.ImportSecretKey(*handle, IOSC::COMMON_KEY_HANDLES[index], iv.data(),
                              &ticket_bytes[offsetof(ES::Ticket, title_key)], PID_ES);
}

ReturnCode ESCore::ImportTitleInit(Context& context, const std::vector<u8>& tmd_bytes,
                                   const std::vector<u8>& cert_chain,
                                   VerifySignature verify_signature)
{
  INFO_LOG_FMT(IOS_ES, "ImportTitleInit");
  ResetTitleImportContext(&context, m_ios.GetIOSC());
  context.title_import_export.tmd.SetBytes(tmd_bytes);
  if (!context.title_import_export.tmd.IsValid())
  {
    ERROR_LOG_FMT(IOS_ES, "Invalid TMD while adding title (size = {})", tmd_bytes.size());
    return ES_EINVAL;
  }

  // Finish a previous import (if it exists).
  FinishStaleImport(context.title_import_export.tmd.GetTitleId());

  ReturnCode ret = IPC_SUCCESS;

  if (verify_signature != VerifySignature::No)
  {
    ret = VerifyContainer(VerifyContainerType::TMD, VerifyMode::UpdateCertStore,
                          context.title_import_export.tmd, cert_chain);
    if (ret != IPC_SUCCESS)
      return ret;
  }

  const auto ticket = FindSignedTicket(context.title_import_export.tmd.GetTitleId());
  if (!ticket.IsValid())
    return ES_NO_TICKET;

  if (verify_signature != VerifySignature::No)
  {
    std::vector<u8> cert_store;
    ret = ReadCertStore(&cert_store);
    if (ret != IPC_SUCCESS)
      return ret;

    ret = VerifyContainer(VerifyContainerType::Ticket, VerifyMode::DoNotUpdateCertStore, ticket,
                          cert_store);
    if (ret != IPC_SUCCESS)
      return ret;
  }

  ret = InitTitleImportKey(ticket.GetBytes(), m_ios.GetIOSC(),
                           &context.title_import_export.key_handle);
  if (ret != IPC_SUCCESS)
    return ret;

  if (!InitImport(context.title_import_export.tmd))
    return ES_EIO;

  context.title_import_export.valid = true;
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportTitleInit(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(4, 0))
    return IPCReply(ES_EINVAL);

  if (!ES::IsValidTMDSize(request.in_vectors[0].size))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  std::vector<u8> tmd(request.in_vectors[0].size);
  memory.CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  std::vector<u8> certs(request.in_vectors[1].size);
  memory.CopyFromEmu(certs.data(), request.in_vectors[1].address, request.in_vectors[1].size);
  return IPCReply(m_core.ImportTitleInit(context, tmd, certs));
}

ReturnCode ESCore::ImportContentBegin(Context& context, u64 title_id, u32 content_id)
{
  if (context.title_import_export.content.valid)
  {
    ERROR_LOG_FMT(IOS_ES, "Trying to add content when we haven't finished adding "
                          "another content. Unsupported.");
    return ES_EINVAL;
  }
  context.title_import_export.content = {};
  context.title_import_export.content.id = content_id;

  INFO_LOG_FMT(IOS_ES, "ImportContentBegin: title {:016x}, content ID {:08x}", title_id,
               context.title_import_export.content.id);

  if (!context.title_import_export.valid)
    return ES_EINVAL;

  if (title_id != context.title_import_export.tmd.GetTitleId())
  {
    ERROR_LOG_FMT(IOS_ES, "ImportContentBegin: title id {:016x} != TMD title id {:016x}, ignoring",
                  title_id, context.title_import_export.tmd.GetTitleId());
    return ES_EINVAL;
  }

  // The IV for title content decryption is the lower two bytes of the
  // content index, zero extended.
  ES::Content content_info;
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

IPCReply ESDevice::ImportContentBegin(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  u64 title_id = memory.Read_U64(request.in_vectors[0].address);
  u32 content_id = memory.Read_U32(request.in_vectors[1].address);
  return IPCReply(m_core.ImportContentBegin(context, title_id, content_id));
}

ReturnCode ESCore::ImportContentData(Context& context, u32 content_fd, const u8* data,
                                     u32 data_size)
{
  INFO_LOG_FMT(IOS_ES, "ImportContentData: content fd {:08x}, size {}", content_fd, data_size);
  context.title_import_export.content.buffer.insert(
      context.title_import_export.content.buffer.end(), data, data + data_size);
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportContentData(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  u32 content_fd = memory.Read_U32(request.in_vectors[0].address);
  u32 data_size = request.in_vectors[1].size;
  u8* data_start = memory.GetPointerForRange(request.in_vectors[1].address, data_size);
  return IPCReply(m_core.ImportContentData(context, content_fd, data_start, data_size));
}

static bool CheckIfContentHashMatches(const std::vector<u8>& content, const ES::Content& info)
{
  return Common::SHA1::CalculateDigest(content.data(), info.size) == info.sha1;
}

static std::string GetImportContentPath(u64 title_id, u32 content_id)
{
  return fmt::format("{}/content/{:08x}.app", Common::GetImportTitlePath(title_id), content_id);
}

ReturnCode ESCore::ImportContentEnd(Context& context, u32 content_fd)
{
  INFO_LOG_FMT(IOS_ES, "ImportContentEnd: content fd {:08x}", content_fd);

  if (!context.title_import_export.valid || !context.title_import_export.content.valid)
    return ES_EINVAL;

  std::vector<u8> decrypted_data(context.title_import_export.content.buffer.size());
  const ReturnCode decrypt_ret = m_ios.GetIOSC().Decrypt(
      context.title_import_export.key_handle, context.title_import_export.content.iv.data(),
      context.title_import_export.content.buffer.data(),
      context.title_import_export.content.buffer.size(), decrypted_data.data(), PID_ES);
  if (decrypt_ret != IPC_SUCCESS)
    return decrypt_ret;

  ES::Content content_info;
  context.title_import_export.tmd.FindContentById(context.title_import_export.content.id,
                                                  &content_info);
  if (!CheckIfContentHashMatches(decrypted_data, content_info))
  {
    ERROR_LOG_FMT(IOS_ES, "ImportContentEnd: Hash for content {:08x} doesn't match",
                  content_info.id);
    return ES_HASH_MISMATCH;
  }

  const auto fs = m_ios.GetFS();
  std::string content_path;
  if (content_info.IsShared())
  {
    ES::SharedContentMap shared_content{m_ios.GetFSCore()};
    content_path = shared_content.AddSharedContent(content_info.sha1);
  }
  else
  {
    content_path = GetImportContentPath(context.title_import_export.tmd.GetTitleId(),
                                        context.title_import_export.content.id);
  }

  const std::string temp_path =
      "/tmp/" + content_path.substr(content_path.find_last_of('/') + 1, std::string::npos);

  {
    constexpr FS::Modes content_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None};
    const auto file = fs->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, temp_path, content_modes);
    if (!file || !file->Write(decrypted_data.data(), content_info.size))
    {
      ERROR_LOG_FMT(IOS_ES, "ImportContentEnd: Failed to write to {}", temp_path);
      return ES_EIO;
    }
  }

  const FS::ResultCode rename_result = fs->Rename(PID_KERNEL, PID_KERNEL, temp_path, content_path);
  if (rename_result != FS::ResultCode::Success)
  {
    fs->Delete(PID_KERNEL, PID_KERNEL, temp_path);
    ERROR_LOG_FMT(IOS_ES, "ImportContentEnd: Failed to move content to {}", content_path);
    return FS::ConvertResult(rename_result);
  }

  context.title_import_export.content = {};
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportContentEnd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  u32 content_fd = memory.Read_U32(request.in_vectors[0].address);
  return IPCReply(m_core.ImportContentEnd(context, content_fd));
}

static bool HasAllRequiredContents(Kernel& ios, const ES::TMDReader& tmd)
{
  const u64 title_id = tmd.GetTitleId();
  const std::vector<ES::Content> contents = tmd.GetContents();
  const ES::SharedContentMap shared_content_map{ios.GetFSCore()};
  return std::all_of(contents.cbegin(), contents.cend(), [&](const ES::Content& content) {
    if (content.IsOptional())
      return true;

    if (content.IsShared())
      return shared_content_map.GetFilenameFromSHA1(content.sha1).has_value();

    // Note: the import hasn't been finalised yet, so the whole title directory
    // is still in /import, not /title.
    const std::string path = GetImportContentPath(title_id, content.id);
    return ios.GetFS()->GetMetadata(PID_KERNEL, PID_KERNEL, path).Succeeded();
  });
}

ReturnCode ESCore::ImportTitleDone(Context& context)
{
  if (!context.title_import_export.valid || context.title_import_export.content.valid)
  {
    ERROR_LOG_FMT(IOS_ES,
                  "ImportTitleDone: No title import, or a content import is still in progress");
    return ES_EINVAL;
  }

  // For system titles, make sure all listed, non-optional contents have been imported.
  const u64 title_id = context.title_import_export.tmd.GetTitleId();
  if (title_id - 0x100000001LL <= 0x100 &&
      !HasAllRequiredContents(m_ios, context.title_import_export.tmd))
  {
    ERROR_LOG_FMT(IOS_ES, "ImportTitleDone: Some required contents are missing");
    return ES_EINVAL;
  }

  if (!WriteImportTMD(context.title_import_export.tmd))
  {
    ERROR_LOG_FMT(IOS_ES, "ImportTitleDone: Failed to write import TMD");
    return ES_EIO;
  }

  if (!FinishImport(context.title_import_export.tmd))
  {
    ERROR_LOG_FMT(IOS_ES, "ImportTitleDone: Failed to finalise title import");
    return ES_EIO;
  }

  INFO_LOG_FMT(IOS_ES, "ImportTitleDone: title {:016x}", title_id);
  ResetTitleImportContext(&context, m_ios.GetIOSC());
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportTitleDone(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return IPCReply(ES_EINVAL);

  return IPCReply(m_core.ImportTitleDone(context));
}

ReturnCode ESCore::ImportTitleCancel(Context& context)
{
  // The TMD buffer can exist without a valid title import context.
  if (context.title_import_export.tmd.GetBytes().empty() ||
      context.title_import_export.content.valid)
    return ES_EINVAL;

  if (context.title_import_export.valid)
  {
    const u64 title_id = context.title_import_export.tmd.GetTitleId();
    FinishStaleImport(title_id);
    INFO_LOG_FMT(IOS_ES, "ImportTitleCancel: title {:016x}", title_id);
  }

  ResetTitleImportContext(&context, m_ios.GetIOSC());
  return IPC_SUCCESS;
}

IPCReply ESDevice::ImportTitleCancel(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return IPCReply(ES_EINVAL);

  return IPCReply(m_core.ImportTitleCancel(context));
}

static bool CanDeleteTitle(u64 title_id)
{
  // IOS only allows deleting non-system titles (or a system title higher than 00000001-00000101).
  return static_cast<u32>(title_id >> 32) != 0x00000001 || static_cast<u32>(title_id) > 0x101;
}

ReturnCode ESCore::DeleteTitle(u64 title_id)
{
  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  const std::string title_dir = Common::GetTitlePath(title_id);
  return FS::ConvertResult(m_ios.GetFS()->Delete(PID_KERNEL, PID_KERNEL, title_dir));
}

IPCReply ESDevice::DeleteTitle(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 8)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);
  return IPCReply(m_core.DeleteTitle(title_id));
}

ReturnCode ESCore::DeleteTicket(const u8* ticket_view)
{
  const auto fs = m_ios.GetFS();
  const u64 title_id = Common::swap64(ticket_view + offsetof(ES::TicketView, title_id));

  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  auto ticket = FindSignedTicket(title_id);
  if (!ticket.IsValid())
    return FS_ENOENT;

  const bool was_v1_ticket = ticket.IsV1Ticket();
  const std::string ticket_path =
      was_v1_ticket ? Common::GetV1TicketFileName(title_id) : Common::GetTicketFileName(title_id);

  const u64 ticket_id = Common::swap64(ticket_view + offsetof(ES::TicketView, ticket_id));
  ticket.DeleteTicket(ticket_id);

  const std::vector<u8>& new_ticket = ticket.GetBytes();

  if (!new_ticket.empty())
  {
    ASSERT(ticket.IsValid());
    ASSERT(ticket.IsV1Ticket() == was_v1_ticket);
    const auto file = fs->OpenFile(PID_KERNEL, PID_KERNEL, ticket_path, FS::Mode::ReadWrite);
    if (!file || !file->Write(new_ticket.data(), new_ticket.size()))
      return ES_EIO;
  }
  else
  {
    // Delete the ticket file if it is now empty.
    fs->Delete(PID_KERNEL, PID_KERNEL, ticket_path);
  }

  // Delete the ticket directory if it is now empty.
  const std::string ticket_parent_dir =
      fmt::format("/ticket/{:08x}", static_cast<u32>(title_id >> 32));
  const auto ticket_parent_dir_entries =
      fs->ReadDirectory(PID_KERNEL, PID_KERNEL, ticket_parent_dir);
  if (ticket_parent_dir_entries && ticket_parent_dir_entries->empty())
    fs->Delete(PID_KERNEL, PID_KERNEL, ticket_parent_dir);

  return IPC_SUCCESS;
}

IPCReply ESDevice::DeleteTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) ||
      request.in_vectors[0].size != sizeof(ES::TicketView))
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  return IPCReply(m_core.DeleteTicket(
      memory.GetPointerForRange(request.in_vectors[0].address, sizeof(ES::TicketView))));
}

ReturnCode ESCore::DeleteTitleContent(u64 title_id) const
{
  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  const std::string content_dir = Common::GetTitleContentPath(title_id);
  const auto files = m_ios.GetFS()->ReadDirectory(PID_KERNEL, PID_KERNEL, content_dir);
  if (!files)
    return FS::ConvertResult(files.Error());

  for (const std::string& file_name : *files)
  {
    if (file_name.size() == 12 && file_name.compare(8, 4, ".app") == 0)
      m_ios.GetFS()->Delete(PID_KERNEL, PID_KERNEL, content_dir + '/' + file_name);
  }

  return IPC_SUCCESS;
}

IPCReply ESDevice::DeleteTitleContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u64))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  return IPCReply(m_core.DeleteTitleContent(memory.Read_U64(request.in_vectors[0].address)));
}

ReturnCode ESCore::DeleteContent(u64 title_id, u32 content_id) const
{
  if (!CanDeleteTitle(title_id))
    return ES_EINVAL;

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return FS_ENOENT;

  ES::Content content;
  if (!tmd.FindContentById(content_id, &content))
    return ES_EINVAL;

  const std::string path =
      fmt::format("{}/{:08x}.app", Common::GetTitleContentPath(title_id), content_id);
  return FS::ConvertResult(m_ios.GetFS()->Delete(PID_KERNEL, PID_KERNEL, path));
}

IPCReply ESDevice::DeleteContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0) || request.in_vectors[0].size != sizeof(u64) ||
      request.in_vectors[1].size != sizeof(u32))
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  return IPCReply(m_core.DeleteContent(memory.Read_U64(request.in_vectors[0].address),
                                       memory.Read_U32(request.in_vectors[1].address)));
}

ReturnCode ESCore::ExportTitleInit(Context& context, u64 title_id, u8* tmd_bytes, u32 tmd_size,
                                   u64 caller_title_id, u32 caller_title_flags)
{
  // No concurrent title import/export is allowed.
  if (context.title_import_export.valid)
    return ES_EINVAL;

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return FS_ENOENT;

  ResetTitleImportContext(&context, m_ios.GetIOSC());
  context.title_import_export.tmd = tmd;

  const ReturnCode ret = InitBackupKey(caller_title_id, caller_title_flags, m_ios.GetIOSC(),
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

IPCReply ESDevice::ExportTitleInit(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 8)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);
  const u32 tmd_size = request.io_vectors[0].size;
  u8* tmd_bytes = memory.GetPointerForRange(request.io_vectors[0].address, tmd_size);

  return IPCReply(m_core.ExportTitleInit(context, title_id, tmd_bytes, tmd_size,
                                         m_core.m_title_context.tmd.GetTitleId(),
                                         m_core.m_title_context.tmd.GetTitleFlags()));
}

ReturnCode ESCore::ExportContentBegin(Context& context, u64 title_id, u32 content_id)
{
  context.title_import_export.content = {};
  if (!context.title_import_export.valid ||
      context.title_import_export.tmd.GetTitleId() != title_id)
  {
    ERROR_LOG_FMT(IOS_ES, "Tried to use ExportContentBegin with an invalid title export context.");
    return ES_EINVAL;
  }

  ES::Content content_info;
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

IPCReply ESDevice::ExportContentBegin(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0) || request.in_vectors[0].size != 8 ||
      request.in_vectors[1].size != 4)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);
  const u32 content_id = memory.Read_U32(request.in_vectors[1].address);

  return IPCReply(m_core.ExportContentBegin(context, title_id, content_id));
}

ReturnCode ESCore::ExportContentData(Context& context, u32 content_fd, u8* data, u32 data_size)
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
  const ReturnCode encrypt_ret = m_ios.GetIOSC().Encrypt(
      context.title_import_export.key_handle, context.title_import_export.content.iv.data(),
      buffer.data(), buffer.size(), output.data(), PID_ES);
  if (encrypt_ret != IPC_SUCCESS)
    return encrypt_ret;

  std::copy(output.cbegin(), output.cend(), data);
  return IPC_SUCCESS;
}

IPCReply ESDevice::ExportContentData(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 4 ||
      request.io_vectors[0].size == 0)
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u32 content_fd = memory.Read_U32(request.in_vectors[0].address);
  const u32 bytes_to_read = request.io_vectors[0].size;
  u8* data = memory.GetPointerForRange(request.io_vectors[0].address, bytes_to_read);

  return IPCReply(m_core.ExportContentData(context, content_fd, data, bytes_to_read));
}

ReturnCode ESCore::ExportContentEnd(Context& context, u32 content_fd)
{
  if (!context.title_import_export.valid || !context.title_import_export.content.valid)
    return ES_EINVAL;
  return static_cast<ReturnCode>(CloseContent(content_fd, 0));
}

IPCReply ESDevice::ExportContentEnd(Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 4)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u32 content_fd = memory.Read_U32(request.in_vectors[0].address);
  return IPCReply(m_core.ExportContentEnd(context, content_fd));
}

ReturnCode ESCore::ExportTitleDone(Context& context)
{
  ResetTitleImportContext(&context, m_ios.GetIOSC());
  return IPC_SUCCESS;
}

IPCReply ESDevice::ExportTitleDone(Context& context, const IOCtlVRequest& request)
{
  return IPCReply(m_core.ExportTitleDone(context));
}

ReturnCode ESCore::DeleteSharedContent(const std::array<u8, 20>& sha1) const
{
  ES::SharedContentMap map{m_ios.GetFSCore()};
  const auto content_path = map.GetFilenameFromSHA1(sha1);
  if (!content_path)
    return ES_EINVAL;

  // Check whether the shared content is used by a system title.
  const std::vector<u64> titles = GetInstalledTitles();
  const bool is_used_by_system_title = std::any_of(titles.begin(), titles.end(), [&](u64 id) {
    if (!ES::IsTitleType(id, ES::TitleType::System))
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
  const auto delete_result = m_ios.GetFS()->Delete(PID_KERNEL, PID_KERNEL, *content_path);
  if (delete_result != FS::ResultCode::Success)
    return FS::ConvertResult(delete_result);

  if (!map.DeleteSharedContent(sha1))
    return ES_EIO;

  return IPC_SUCCESS;
}

IPCReply ESDevice::DeleteSharedContent(const IOCtlVRequest& request)
{
  std::array<u8, 20> sha1;
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sha1.size())
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  memory.CopyFromEmu(sha1.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  return IPCReply(m_core.DeleteSharedContent(sha1));
}
}  // namespace IOS::HLE
