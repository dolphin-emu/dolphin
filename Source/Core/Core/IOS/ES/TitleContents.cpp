// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <cinttypes>
#include <utility>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
s32 ES::OpenContent(const IOS::ES::TMDReader& tmd, u16 content_index, u32 uid)
{
  const u64 title_id = tmd.GetTitleId();

  IOS::ES::Content content;
  if (!tmd.GetContent(content_index, &content))
    return ES_EINVAL;

  for (size_t i = 0; i < m_content_table.size(); ++i)
  {
    OpenedContent& entry = m_content_table[i];
    if (entry.m_opened)
      continue;

    if (!entry.m_file.Open(GetContentPath(title_id, content), "rb"))
      return FS_ENOENT;

    entry.m_opened = true;
    entry.m_position = 0;
    entry.m_content = content;
    entry.m_title_id = title_id;
    entry.m_uid = uid;
    INFO_LOG(IOS_ES, "OpenContent: title ID %016" PRIx64 ", UID 0x%x, CFD %zu", title_id, uid, i);
    return static_cast<s32>(i);
  }

  return FS_EFDEXHAUSTED;
}

IPCCommandResult ES::OpenContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0) || request.in_vectors[0].size != sizeof(u64) ||
      request.in_vectors[1].size != sizeof(IOS::ES::TicketView) ||
      request.in_vectors[2].size != sizeof(u32))
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const u32 content_index = Memory::Read_U32(request.in_vectors[2].address);
  // TODO: check the ticket view, check permissions.

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  return GetDefaultReply(OpenContent(tmd, content_index, uid));
}

IPCCommandResult ES::OpenActiveTitleContent(u32 caller_uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  const u32 content_index = Memory::Read_U32(request.in_vectors[0].address);

  if (!m_title_context.active)
    return GetDefaultReply(ES_EINVAL);

  IOS::ES::UIDSys uid_map{Common::FROM_SESSION_ROOT};
  const u32 uid = uid_map.GetOrInsertUIDForTitle(m_title_context.tmd.GetTitleId());
  if (caller_uid != 0 && caller_uid != uid)
    return GetDefaultReply(ES_EACCES);

  return GetDefaultReply(OpenContent(m_title_context.tmd, content_index, caller_uid));
}

s32 ES::ReadContent(u32 cfd, u8* buffer, u32 size, u32 uid)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;
  OpenedContent& entry = m_content_table[cfd];

  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  // XXX: make this reuse the FS code... ES just does a simple "IOS_Read" call here
  //      instead of all this duplicated filesystem logic.

  if (entry.m_position + size > entry.m_file.GetSize())
    size = static_cast<u32>(entry.m_file.GetSize()) - entry.m_position;

  entry.m_file.Seek(entry.m_position, SEEK_SET);
  if (!entry.m_file.ReadBytes(buffer, size))
  {
    ERROR_LOG(IOS_ES, "ES: failed to read %u bytes from %u!", size, entry.m_position);
    return ES_SHORT_READ;
  }

  entry.m_position += size;
  return size;
}

IPCCommandResult ES::ReadContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  const u32 cfd = Memory::Read_U32(request.in_vectors[0].address);
  const u32 size = request.io_vectors[0].size;
  const u32 addr = request.io_vectors[0].address;

  return GetDefaultReply(ReadContent(cfd, Memory::GetPointer(addr), size, uid));
}

ReturnCode ES::CloseContent(u32 cfd, u32 uid)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;

  OpenedContent& entry = m_content_table[cfd];
  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  entry = {};
  INFO_LOG(IOS_ES, "CloseContent: CFD %u", cfd);
  return IPC_SUCCESS;
}

IPCCommandResult ES::CloseContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  const u32 cfd = Memory::Read_U32(request.in_vectors[0].address);
  return GetDefaultReply(CloseContent(cfd, uid));
}

s32 ES::SeekContent(u32 cfd, u32 offset, SeekMode mode, u32 uid)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;

  OpenedContent& entry = m_content_table[cfd];
  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  // XXX: This should be a simple IOS_Seek.
  switch (mode)
  {
  case SeekMode::IOS_SEEK_SET:
    entry.m_position = offset;
    break;

  case SeekMode::IOS_SEEK_CUR:
    entry.m_position += offset;
    break;

  case SeekMode::IOS_SEEK_END:
    entry.m_position = static_cast<u32>(entry.m_content.size) + offset;
    break;

  default:
    return FS_EINVAL;
  }

  return entry.m_position;
}

IPCCommandResult ES::SeekContent(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);

  const u32 cfd = Memory::Read_U32(request.in_vectors[0].address);
  const u32 offset = Memory::Read_U32(request.in_vectors[1].address);
  const SeekMode mode = static_cast<SeekMode>(Memory::Read_U32(request.in_vectors[2].address));

  return GetDefaultReply(SeekContent(cfd, offset, mode, uid));
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
