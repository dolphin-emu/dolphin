// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/ES/ES.h"

#include <vector>

#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystemProxy.h"
#include "Core/IOS/Uids.h"
#include "Core/System.h"

namespace IOS::HLE
{
s32 ESCore::OpenContent(const ES::TMDReader& tmd, const u16 content_index, const u32 uid, const Ticks ticks)
{
  const u64 title_id = tmd.GetTitleId();

  ES::Content content;
  if (!tmd.GetContent(content_index, &content))
    return ES_EINVAL;

  for (size_t i = 0; i < m_content_table.size(); ++i)
  {
    auto& [m_opened, m_fd, entry_title_id, entry_content, entry_uid] = m_content_table[i];
    if (m_opened)
      continue;

    const std::string path = GetContentPath(title_id, content, ticks);
    auto fd = m_ios.GetFSCore().Open(PID_KERNEL, PID_KERNEL, path, FS::Mode::Read, {}, ticks);
    if (fd.Get() < 0)
      return fd.Get();

    m_opened = true;
    m_fd = fd.Release();
    entry_content = content;
    entry_title_id = title_id;
    entry_uid = uid;
    INFO_LOG_FMT(IOS_ES,
                 "OpenContent: title ID {:016x}, UID {:#x}, content {:08x} (index {}) -> CFD {}",
                 title_id, uid, content.id, content_index, i);
    return static_cast<s32>(i);
  }

  return FS_EFDEXHAUSTED;
}

IPCReply ESDevice::OpenContent(const u32 uid, const IOCtlVRequest& request) const
{
  return MakeIPCReply(IPC_OVERHEAD_TICKS, [&](const Ticks ticks) -> s32 {
    if (!request.HasNumberOfValidVectors(3, 0) || request.in_vectors[0].size != sizeof(u64) ||
        request.in_vectors[1].size != sizeof(ES::TicketView) ||
        request.in_vectors[2].size != sizeof(u32))
    {
      return ES_EINVAL;
    }

    const auto& system = GetSystem();
    const auto& memory = system.GetMemory();
    const u64 title_id = memory.Read_U64(request.in_vectors[0].address);
    const u32 content_index = memory.Read_U32(request.in_vectors[2].address);
    // TODO: check the ticket view, check permissions.

    const auto tmd = m_core.FindInstalledTMD(title_id, ticks);
    if (!tmd.IsValid())
      return FS_ENOENT;

    return m_core.OpenContent(tmd, content_index, uid, ticks);
  });
}

IPCReply ESDevice::OpenActiveTitleContent(const u32 caller_uid, const IOCtlVRequest& request) const
{
  return MakeIPCReply(IPC_OVERHEAD_TICKS, [&](const Ticks ticks) -> s32 {
    if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
      return ES_EINVAL;

    const auto& system = GetSystem();
    const auto& memory = system.GetMemory();
    const u32 content_index = memory.Read_U32(request.in_vectors[0].address);

    if (!m_core.m_title_context.active)
      return ES_EINVAL;

    ES::UIDSys uid_map{GetEmulationKernel().GetFSCore()};
    const u32 uid = uid_map.GetOrInsertUIDForTitle(m_core.m_title_context.tmd.GetTitleId());
    ticks.Add(uid_map.GetTicks());
    if (caller_uid != 0 && caller_uid != uid)
      return ES_EACCES;

    return m_core.OpenContent(m_core.m_title_context.tmd, content_index, caller_uid, ticks);
  });
}

s32 ESCore::ReadContent(const u32 cfd, u8* buffer, const u32 size, const u32 uid, const Ticks ticks) const
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;
  const auto& [m_opened, m_fd, _m_title_id, _m_content, entry_uid] = m_content_table[cfd];

  if (entry_uid != uid)
    return ES_EACCES;
  if (!m_opened)
    return IPC_EINVAL;

  return m_ios.GetFSCore().Read(m_fd, buffer, size, {}, ticks);
}

IPCReply ESDevice::ReadContent(const u32 uid, const IOCtlVRequest& request) const
{
  return MakeIPCReply(IPC_OVERHEAD_TICKS, [&](const Ticks ticks) -> s32 {
    if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u32))
      return ES_EINVAL;

    const auto& system = GetSystem();
    const auto& memory = system.GetMemory();
    const u32 cfd = memory.Read_U32(request.in_vectors[0].address);
    const u32 size = request.io_vectors[0].size;
    const u32 addr = request.io_vectors[0].address;

    INFO_LOG_FMT(IOS_ES, "ReadContent(uid={:#x}, cfd={}, size={}, addr={:08x})", uid, cfd, size,
                 addr);
    return m_core.ReadContent(cfd, memory.GetPointerForRange(addr, size), size, uid, ticks);
  });
}

s32 ESCore::CloseContent(const u32 cfd, const u32 uid, const Ticks ticks)
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;

  OpenedContent& entry = m_content_table[cfd];
  if (entry.m_uid != uid)
    return ES_EACCES;
  if (!entry.m_opened)
    return IPC_EINVAL;

  m_ios.GetFSCore().Close(entry.m_fd, ticks);
  entry = {};
  INFO_LOG_FMT(IOS_ES, "CloseContent: CFD {}", cfd);
  return IPC_SUCCESS;
}

IPCReply ESDevice::CloseContent(const u32 uid, const IOCtlVRequest& request) const
{
  return MakeIPCReply(IPC_OVERHEAD_TICKS, [&](const Ticks ticks) -> s32 {
    if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
      return ES_EINVAL;

    const auto& system = GetSystem();
    const auto& memory = system.GetMemory();
    const u32 cfd = memory.Read_U32(request.in_vectors[0].address);
    return m_core.CloseContent(cfd, uid, ticks);
  });
}

s32 ESCore::SeekContent(const u32 cfd, const u32 offset, SeekMode mode, const u32 uid, const Ticks ticks) const
{
  if (cfd >= m_content_table.size())
    return ES_EINVAL;

  const auto& [m_opened, m_fd, _m_title_id, _m_content, entry_uid] = m_content_table[cfd];
  if (entry_uid != uid)
    return ES_EACCES;
  if (!m_opened)
    return IPC_EINVAL;

  return m_ios.GetFSCore().Seek(m_fd, offset, static_cast<FS::SeekMode>(mode), ticks);
}

IPCReply ESDevice::SeekContent(const u32 uid, const IOCtlVRequest& request) const
{
  return MakeIPCReply(IPC_OVERHEAD_TICKS, [&](const Ticks ticks) -> s32 {
    if (!request.HasNumberOfValidVectors(3, 0))
      return ES_EINVAL;

    const auto& system = GetSystem();
    const auto& memory = system.GetMemory();
    const u32 cfd = memory.Read_U32(request.in_vectors[0].address);
    const u32 offset = memory.Read_U32(request.in_vectors[1].address);
    const auto mode = static_cast<SeekMode>(memory.Read_U32(request.in_vectors[2].address));

    return m_core.SeekContent(cfd, offset, mode, uid, ticks);
  });
}
}  // namespace IOS::HLE
