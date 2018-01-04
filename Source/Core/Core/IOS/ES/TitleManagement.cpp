// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cinttypes>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>
#include <mbedtls/sha1.h>

#include "Common/Align.h"
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
IPCCommandResult ES::AddTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> bytes(request.in_vectors[0].size);
  Memory::CopyFromEmu(bytes.data(), request.in_vectors[0].address, request.in_vectors[0].size);

  IOS::ES::TicketReader ticket{std::move(bytes)};
  if (!ticket.IsValid())
    return GetDefaultReply(ES_EINVAL);

  const u32 ticket_device_id = ticket.GetDeviceId();
  const u32 device_id = EcWii::GetInstance().GetNGID();
  if (ticket_device_id != 0)
  {
    if (device_id != ticket_device_id)
    {
      WARN_LOG(IOS_ES, "Device ID mismatch: ticket %08x, device %08x", ticket_device_id, device_id);
      return GetDefaultReply(ES_DEVICE_ID_MISMATCH);
    }
    const s32 ret = ticket.Unpersonalise();
    if (ret < 0)
    {
      ERROR_LOG(IOS_ES, "AddTicket: Failed to unpersonalise ticket for %016" PRIx64 " (ret = %d)",
                ticket.GetTitleId(), ret);
      return GetDefaultReply(ret);
    }
  }

  if (!DiscIO::AddTicket(ticket))
    return GetDefaultReply(ES_EIO);

  INFO_LOG(IOS_ES, "AddTicket: Imported ticket for title %016" PRIx64, ticket.GetTitleId());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTMD(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);

  // Ioctlv 0x2b writes the TMD to /tmp/title.tmd (for imports) and doesn't seem to write it
  // to either /import or /title. So here we simply have to set the import TMD.
  m_addtitle_tmd.SetBytes(std::move(tmd));
  // TODO: validate TMDs and return the proper error code (-1027) if the signature type is invalid.
  if (!m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  if (!IOS::ES::InitImport(m_addtitle_tmd.GetTitleId()))
    return GetDefaultReply(FS_EIO);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTitleStart(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(4, 0))
    return GetDefaultReply(ES_EINVAL);

  INFO_LOG(IOS_ES, "IOCTL_ES_ADDTITLESTART");
  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);

  m_addtitle_tmd.SetBytes(tmd);
  if (!m_addtitle_tmd.IsValid())
  {
    ERROR_LOG(IOS_ES, "Invalid TMD while adding title (size = %zd)", tmd.size());
    return GetDefaultReply(ES_EINVAL);
  }

  // Finish a previous import (if it exists).
  const IOS::ES::TMDReader previous_tmd = IOS::ES::FindImportTMD(m_addtitle_tmd.GetTitleId());
  if (previous_tmd.IsValid())
    FinishImport(previous_tmd);

  if (!IOS::ES::InitImport(m_addtitle_tmd.GetTitleId()))
    return GetDefaultReply(FS_EIO);

  // TODO: check and use the other vectors.

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddContentStart(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return GetDefaultReply(ES_EINVAL);

  u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  u32 content_id = Memory::Read_U32(request.in_vectors[1].address);

  if (m_addtitle_content_id != 0xFFFFFFFF)
  {
    ERROR_LOG(IOS_ES, "Trying to add content when we haven't finished adding "
                      "another content. Unsupported.");
    return GetDefaultReply(ES_EINVAL);
  }
  m_addtitle_content_id = content_id;

  m_addtitle_content_buffer.clear();

  INFO_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTSTART: title id %016" PRIx64 ", "
                   "content id %08x",
           title_id, m_addtitle_content_id);

  if (!m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  if (title_id != m_addtitle_tmd.GetTitleId())
  {
    ERROR_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTSTART: title id %016" PRIx64 " != "
                      "TMD title id %016" PRIx64 ", ignoring",
              title_id, m_addtitle_tmd.GetTitleId());
  }

  // We're supposed to return a "content file descriptor" here, which is
  // passed to further AddContentData / AddContentFinish. But so far there is
  // no known content installer which performs content addition concurrently.
  // Instead we just log an error (see above) if this condition is detected.
  s32 content_fd = 0;
  return GetDefaultReply(content_fd);
}

IPCCommandResult ES::AddContentData(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return GetDefaultReply(ES_EINVAL);

  u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTDATA: content fd %08x, "
                   "size %d",
           content_fd, request.in_vectors[1].size);

  u8* data_start = Memory::GetPointer(request.in_vectors[1].address);
  u8* data_end = data_start + request.in_vectors[1].size;
  m_addtitle_content_buffer.insert(m_addtitle_content_buffer.end(), data_start, data_end);
  return GetDefaultReply(IPC_SUCCESS);
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

IPCCommandResult ES::AddContentFinish(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  if (m_addtitle_content_id == 0xFFFFFFFF)
    return GetDefaultReply(ES_EINVAL);

  u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTFINISH: content fd %08x", content_fd);

  if (!m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  // Try to find the title key from a pre-installed ticket.
  IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(m_addtitle_tmd.GetTitleId());
  if (!ticket.IsValid())
  {
    return GetDefaultReply(ES_NO_TICKET);
  }

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_setkey_dec(&aes_ctx, ticket.GetTitleKey().data(), 128);

  // The IV for title content decryption is the lower two bytes of the
  // content index, zero extended.
  IOS::ES::Content content_info;
  if (!m_addtitle_tmd.FindContentById(m_addtitle_content_id, &content_info))
  {
    return GetDefaultReply(ES_EINVAL);
  }
  u8 iv[16] = {0};
  iv[0] = (content_info.index >> 8) & 0xFF;
  iv[1] = content_info.index & 0xFF;
  std::vector<u8> decrypted_data(m_addtitle_content_buffer.size());
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, m_addtitle_content_buffer.size(), iv,
                        m_addtitle_content_buffer.data(), decrypted_data.data());
  if (!CheckIfContentHashMatches(decrypted_data, content_info))
  {
    ERROR_LOG(IOS_ES, "AddContentFinish: Hash for content %08x doesn't match", content_info.id);
    return GetDefaultReply(ES_HASH_MISMATCH);
  }

  std::string content_path;
  if (content_info.IsShared())
  {
    IOS::ES::SharedContentMap shared_content{Common::FROM_SESSION_ROOT};
    content_path = shared_content.AddSharedContent(content_info.sha1);
  }
  else
  {
    content_path = GetImportContentPath(m_addtitle_tmd.GetTitleId(), m_addtitle_content_id);
  }
  File::CreateFullPath(content_path);

  const std::string temp_path = Common::RootUserPath(Common::FROM_SESSION_ROOT) +
                                StringFromFormat("/tmp/%08x.app", m_addtitle_content_id);
  File::CreateFullPath(temp_path);

  {
    File::IOFile file(temp_path, "wb");
    if (!file.WriteBytes(decrypted_data.data(), content_info.size))
    {
      ERROR_LOG(IOS_ES, "AddContentFinish: Failed to write to %s", temp_path.c_str());
      return GetDefaultReply(ES_EIO);
    }
  }

  if (!File::Rename(temp_path, content_path))
  {
    ERROR_LOG(IOS_ES, "AddContentFinish: Failed to move content to %s", content_path.c_str());
    return GetDefaultReply(ES_EIO);
  }

  m_addtitle_content_id = 0xFFFFFFFF;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTitleFinish(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0) || !m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  if (!WriteImportTMD(m_addtitle_tmd))
    return GetDefaultReply(ES_EIO);

  if (!FinishImport(m_addtitle_tmd))
    return GetDefaultReply(FS_EIO);

  INFO_LOG(IOS_ES, "IOCTL_ES_ADDTITLEFINISH");
  m_addtitle_tmd.SetBytes({});
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTitleCancel(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0) || !m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  const IOS::ES::TMDReader original_tmd = IOS::ES::FindInstalledTMD(m_addtitle_tmd.GetTitleId());
  if (!original_tmd.IsValid())
  {
    // This should never happen unless someone messed with the installed TMD directly.
    // Still, let's check for this case and return an error instead of potentially crashing.
    return GetDefaultReply(FS_ENOENT);
  }

  if (!FinishImport(original_tmd))
    return GetDefaultReply(FS_EIO);

  m_addtitle_tmd.SetBytes({});
  return GetDefaultReply(IPC_SUCCESS);
}

static bool CanDeleteTitle(u64 title_id)
{
  // IOS only allows deleting non-system titles (or a system title higher than 00000001-00000101).
  return static_cast<u32>(title_id >> 32) != 0x00000001 || static_cast<u32>(title_id) > 0x101;
}

IPCCommandResult ES::DeleteTitle(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);

  if (!CanDeleteTitle(title_id))
    return GetDefaultReply(ES_EINVAL);

  const std::string title_dir =
      StringFromFormat("%s/title/%08x/%08x/", RootUserPath(Common::FROM_SESSION_ROOT).c_str(),
                       static_cast<u32>(title_id >> 32), static_cast<u32>(title_id));
  if (!File::IsDirectory(title_dir) ||
      !DiscIO::CNANDContentManager::Access().RemoveTitle(title_id, Common::FROM_SESSION_ROOT))
  {
    return GetDefaultReply(FS_ENOENT);
  }

  if (!File::DeleteDirRecursively(title_dir))
  {
    ERROR_LOG(IOS_ES, "DeleteTitle: Failed to delete title directory: %s", title_dir.c_str());
    return GetDefaultReply(FS_EACCESS);
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DeleteTicket(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_DELETETICKET: title: %08x/%08x", (u32)(TitleID >> 32), (u32)TitleID);

  // Presumably return -1017 when delete fails
  if (!File::Delete(Common::GetTicketFileName(TitleID, Common::FROM_SESSION_ROOT)))
    return GetDefaultReply(ES_EINVAL);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DeleteTitleContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_DELETETITLECONTENT: title: %08x/%08x", (u32)(TitleID >> 32),
           (u32)TitleID);

  // Presumably return -1017 when title not installed TODO verify
  if (!DiscIO::CNANDContentManager::Access().RemoveTitle(TitleID, Common::FROM_SESSION_ROOT))
    return GetDefaultReply(ES_EINVAL);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ExportTitleInit(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_EINVAL);

  // No concurrent title import/export is allowed.
  if (m_export_title_context.valid)
    return GetDefaultReply(ES_EINVAL);

  const auto tmd = IOS::ES::FindInstalledTMD(Memory::Read_U64(request.in_vectors[0].address));
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  m_export_title_context.tmd = tmd;

  const auto ticket = DiscIO::FindSignedTicket(m_export_title_context.tmd.GetTitleId());
  if (!ticket.IsValid())
    return GetDefaultReply(ES_NO_TICKET);
  if (ticket.GetTitleId() != m_export_title_context.tmd.GetTitleId())
    return GetDefaultReply(ES_EINVAL);

  m_export_title_context.title_key = ticket.GetTitleKey();

  const auto& raw_tmd = m_export_title_context.tmd.GetRawTMD();
  if (request.io_vectors[0].size != raw_tmd.size())
    return GetDefaultReply(ES_EINVAL);

  Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());

  m_export_title_context.valid = true;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ExportContentBegin(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0) || request.in_vectors[0].size != 8 ||
      request.in_vectors[1].size != 4)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const u32 content_id = Memory::Read_U32(request.in_vectors[1].address);

  if (!m_export_title_context.valid || m_export_title_context.tmd.GetTitleId() != title_id)
  {
    ERROR_LOG(IOS_ES, "Tried to use ExportContentBegin with an invalid title export context.");
    return GetDefaultReply(ES_EINVAL);
  }

  const auto& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const auto* content = content_loader.GetContentByID(content_id);
  if (!content)
    return GetDefaultReply(ES_EINVAL);

  OpenedContent entry;
  entry.m_position = 0;
  entry.m_content = content->m_metadata;
  entry.m_title_id = title_id;
  content->m_Data->Open();

  u32 cid = 0;
  while (m_export_title_context.contents.find(cid) != m_export_title_context.contents.end())
    cid++;

  TitleExportContext::ExportContent content_export;
  content_export.content = std::move(entry);
  content_export.iv[0] = (content->m_metadata.index >> 8) & 0xFF;
  content_export.iv[1] = content->m_metadata.index & 0xFF;

  m_export_title_context.contents.emplace(cid, content_export);
  // IOS returns a content ID which is passed to further content calls.
  return GetDefaultReply(cid);
}

IPCCommandResult ES::ExportContentData(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 4 ||
      request.io_vectors[0].size == 0)
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const u32 content_id = Memory::Read_U32(request.in_vectors[0].address);
  const u32 bytes_to_read = request.io_vectors[0].size;

  const auto iterator = m_export_title_context.contents.find(content_id);
  if (!m_export_title_context.valid || iterator == m_export_title_context.contents.end() ||
      iterator->second.content.m_position >= iterator->second.content.m_content.size)
  {
    return GetDefaultReply(ES_EINVAL);
  }

  auto& metadata = iterator->second.content;

  const auto& content_loader = AccessContentDevice(metadata.m_title_id);
  const auto* content = content_loader.GetContentByID(metadata.m_content.id);
  content->m_Data->Open();

  const u32 length =
      std::min(static_cast<u32>(metadata.m_content.size - metadata.m_position), bytes_to_read);
  std::vector<u8> buffer(length);

  if (!content->m_Data->GetRange(metadata.m_position, length, buffer.data()))
  {
    ERROR_LOG(IOS_ES, "ExportContentData: ES_SHORT_READ");
    return GetDefaultReply(ES_SHORT_READ);
  }

  // IOS aligns the buffer to 32 bytes. Since we also need to align it to 16 bytes,
  // let's just follow IOS here.
  buffer.resize(Common::AlignUp(buffer.size(), 32));
  std::vector<u8> output(buffer.size());

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_setkey_enc(&aes_ctx, m_export_title_context.title_key.data(), 128);
  const int ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, buffer.size(),
                                        iterator->second.iv.data(), buffer.data(), output.data());
  if (ret != 0)
  {
    // XXX: proper error code when IOSC_Encrypt fails.
    ERROR_LOG(IOS_ES, "ExportContentData: Failed to encrypt content.");
    return GetDefaultReply(ES_EINVAL);
  }

  Memory::CopyToEmu(request.io_vectors[0].address, output.data(), output.size());
  metadata.m_position += length;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ExportContentEnd(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 4)
    return GetDefaultReply(ES_EINVAL);

  const u32 content_id = Memory::Read_U32(request.in_vectors[0].address);

  const auto iterator = m_export_title_context.contents.find(content_id);
  if (!m_export_title_context.valid || iterator == m_export_title_context.contents.end() ||
      iterator->second.content.m_position != iterator->second.content.m_content.size)
  {
    return GetDefaultReply(ES_EINVAL);
  }

  // XXX: Check the content hash, as IOS does?

  const auto& content_loader = AccessContentDevice(iterator->second.content.m_title_id);
  content_loader.GetContentByID(iterator->second.content.m_content.id)->m_Data->Close();

  m_export_title_context.contents.erase(iterator);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ExportTitleDone(const IOCtlVRequest& request)
{
  if (!m_export_title_context.valid)
    return GetDefaultReply(ES_EINVAL);

  m_export_title_context.valid = false;
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
