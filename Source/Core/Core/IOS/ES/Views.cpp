// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/VersionInfo.h"
#include "Core/System.h"

namespace IOS::HLE
{
// HACK: Since we do not want to require users to install disc updates when launching
//       Wii games from the game list (which is the inaccurate game boot path anyway),
//       or in TAS/netplay (because forcing every user to have a proper NAND setup in those cases
//       is unrealistic), IOSes have to be faked for games and homebrew that reload IOS
//       such as Gecko to work properly.
//
//       To minimize the effect of this hack, we should only do this for disc titles
//       booted from the game list, though.
static bool ShouldReturnFakeViewsForIOSes(u64 title_id, const TitleContext& context)
{
  const bool ios = IsTitleType(title_id, ES::TitleType::System) && title_id != Titles::SYSTEM_MENU;
  const bool disc_title = context.active && ES::IsDiscTitle(context.tmd.GetTitleId());
  return Core::WantsDeterminism() ||
         (ios && SConfig::GetInstance().m_disc_booted_from_game_list && disc_title);
}

IPCReply ESDevice::GetTicketViewCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 TitleID = memory.Read_U64(request.in_vectors[0].address);

  const ES::TicketReader ticket = m_core.FindSignedTicket(TitleID);
  u32 view_count = ticket.IsValid() ? static_cast<u32>(ticket.GetNumberOfTickets()) : 0;

  if (!IsEmulated(TitleID))
  {
    view_count = 0;
    ERROR_LOG_FMT(IOS_ES, "GetViewCount: Dolphin doesn't emulate IOS title {:016x}", TitleID);
  }
  else if (ShouldReturnFakeViewsForIOSes(TitleID, m_core.m_title_context))
  {
    view_count = 1;
    WARN_LOG_FMT(IOS_ES, "GetViewCount: Faking IOS title {:016x} being present", TitleID);
  }

  INFO_LOG_FMT(IOS_ES, "IOCTL_ES_GETVIEWCNT for titleID: {:016x} (View Count = {})", TitleID,
               view_count);

  memory.Write_U32(view_count, request.io_vectors[0].address);
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::GetTicketViews(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 TitleID = memory.Read_U64(request.in_vectors[0].address);
  const u32 maxViews = memory.Read_U32(request.in_vectors[1].address);

  const ES::TicketReader ticket = m_core.FindSignedTicket(TitleID);

  if (!IsEmulated(TitleID))
  {
    ERROR_LOG_FMT(IOS_ES, "GetViews: Dolphin doesn't emulate IOS title {:016x}", TitleID);
  }
  else if (ticket.IsValid())
  {
    u32 number_of_views = std::min(maxViews, static_cast<u32>(ticket.GetNumberOfTickets()));
    for (u32 view = 0; view < number_of_views; ++view)
    {
      const std::vector<u8> ticket_view = ticket.GetRawTicketView(view);
      memory.CopyToEmu(request.io_vectors[0].address + view * sizeof(ES::TicketView),
                       ticket_view.data(), ticket_view.size());
    }
  }
  else if (ShouldReturnFakeViewsForIOSes(TitleID, m_core.m_title_context))
  {
    memory.Memset(request.io_vectors[0].address, 0, sizeof(ES::TicketView));
    WARN_LOG_FMT(IOS_ES, "GetViews: Faking IOS title {:016x} being present", TitleID);
  }

  INFO_LOG_FMT(IOS_ES, "IOCTL_ES_GETVIEWS for titleID: {:016x} (MaxViews = {})", TitleID, maxViews);

  return IPCReply(IPC_SUCCESS);
}

ReturnCode ESCore::GetTicketFromView(const u8* ticket_view, u8* ticket, u32* ticket_size,
                                     std::optional<u8> desired_version) const
{
  const u64 title_id = Common::swap64(&ticket_view[offsetof(ES::TicketView, title_id)]);
  const u64 ticket_id = Common::swap64(&ticket_view[offsetof(ES::TicketView, ticket_id)]);
  const u8 view_version = ticket_view[offsetof(ES::TicketView, version)];
  const u8 version = desired_version.value_or(view_version);

  const auto installed_ticket = FindSignedTicket(title_id, version);
  if (!installed_ticket.IsValid())
    return ES_NO_TICKET;

  // Handle GetTicketSizeFromView
  if (ticket == nullptr)
  {
    *ticket_size = installed_ticket.GetTicketSize();
    return IPC_SUCCESS;
  }

  // Handle GetTicketFromView or GetV0TicketFromView
  const std::vector<u8> ticket_bytes = installed_ticket.GetRawTicket(ticket_id);
  if (ticket_bytes.empty())
    return ES_NO_TICKET;

  if (!m_title_context.active)
    return ES_EINVAL;

  // Check for permission to export the ticket.
  const u32 title_identifier = static_cast<u32>(m_title_context.tmd.GetTitleId());
  const u32 permitted_title_mask =
      Common::swap32(ticket_bytes.data() + offsetof(ES::Ticket, permitted_title_mask));
  const u32 permitted_title_id =
      Common::swap32(ticket_bytes.data() + offsetof(ES::Ticket, permitted_title_id));
  const u8 title_export_allowed = ticket_bytes[offsetof(ES::Ticket, title_export_allowed)];

  // This is the check present in IOS. The 5 does not correspond to any known constant, sadly.
  if (!title_identifier || (title_identifier & ~permitted_title_mask) != permitted_title_id ||
      (title_export_allowed & 0xF) != 5)
  {
    return ES_EACCES;
  }

  std::copy(ticket_bytes.begin(), ticket_bytes.end(), ticket);
  return IPC_SUCCESS;
}

IPCReply ESDevice::GetV0TicketFromView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) ||
      request.in_vectors[0].size != sizeof(ES::TicketView) ||
      request.io_vectors[0].size != sizeof(ES::Ticket))
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  return IPCReply(m_core.GetTicketFromView(
      memory.GetPointerForRange(request.in_vectors[0].address, sizeof(ES::TicketView)),
      memory.GetPointerForRange(request.io_vectors[0].address, sizeof(ES::Ticket)), nullptr, 0));
}

IPCReply ESDevice::GetTicketSizeFromView(const IOCtlVRequest& request)
{
  u32 ticket_size = 0;
  if (!request.HasNumberOfValidVectors(1, 1) ||
      request.in_vectors[0].size != sizeof(ES::TicketView) ||
      request.io_vectors[0].size != sizeof(ticket_size))
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  const ReturnCode ret = m_core.GetTicketFromView(
      memory.GetPointerForRange(request.in_vectors[0].address, sizeof(ES::TicketView)), nullptr,
      &ticket_size, std::nullopt);
  memory.Write_U32(ticket_size, request.io_vectors[0].address);
  return IPCReply(ret);
}

IPCReply ESDevice::GetTicketFromView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) ||
      request.in_vectors[0].size != sizeof(ES::TicketView) ||
      request.in_vectors[1].size != sizeof(u32))
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  u32 ticket_size = memory.Read_U32(request.in_vectors[1].address);
  if (ticket_size != request.io_vectors[0].size)
    return IPCReply(ES_EINVAL);

  return IPCReply(m_core.GetTicketFromView(
      memory.GetPointerForRange(request.in_vectors[0].address, sizeof(ES::TicketView)),
      memory.GetPointerForRange(request.io_vectors[0].address, ticket_size), &ticket_size,
      std::nullopt));
}

IPCReply ESDevice::GetTMDViewSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 TitleID = memory.Read_U64(request.in_vectors[0].address);
  const ES::TMDReader tmd = m_core.FindInstalledTMD(TitleID);

  if (!tmd.IsValid())
    return IPCReply(FS_ENOENT);

  const u32 view_size = static_cast<u32>(tmd.GetRawView().size());
  memory.Write_U32(view_size, request.io_vectors[0].address);

  INFO_LOG_FMT(IOS_ES, "GetTMDViewSize: {} bytes for title {:016x}", view_size, TitleID);
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::GetTMDViews(const IOCtlVRequest& request)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  if (!request.HasNumberOfValidVectors(2, 1) ||
      request.in_vectors[0].size != sizeof(ES::TMDHeader::title_id) ||
      request.in_vectors[1].size != sizeof(u32) ||
      memory.Read_U32(request.in_vectors[1].address) != request.io_vectors[0].size)
  {
    return IPCReply(ES_EINVAL);
  }

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);
  const ES::TMDReader tmd = m_core.FindInstalledTMD(title_id);

  if (!tmd.IsValid())
    return IPCReply(FS_ENOENT);

  const std::vector<u8> raw_view = tmd.GetRawView();
  if (request.io_vectors[0].size < raw_view.size())
    return IPCReply(ES_EINVAL);

  memory.CopyToEmu(request.io_vectors[0].address, raw_view.data(), raw_view.size());

  INFO_LOG_FMT(IOS_ES, "GetTMDView: {} bytes for title {:016x}", raw_view.size(), title_id);
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::DIGetTMDViewSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return IPCReply(ES_EINVAL);

  // Sanity check the TMD size.
  if (request.in_vectors[0].size >= 4 * 1024 * 1024)
    return IPCReply(ES_EINVAL);

  if (request.io_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const bool has_tmd = request.in_vectors[0].size != 0;
  size_t tmd_view_size = 0;

  if (has_tmd)
  {
    std::vector<u8> tmd_bytes(request.in_vectors[0].size);
    memory.CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
    const ES::TMDReader tmd{std::move(tmd_bytes)};

    // Yes, this returns -1017, not ES_INVALID_TMD.
    // IOS simply checks whether the TMD has all required content entries.
    if (!tmd.IsValid())
      return IPCReply(ES_EINVAL);

    tmd_view_size = tmd.GetRawView().size();
  }
  else
  {
    // If no TMD was passed in and no title is active, IOS returns -1017.
    if (!m_core.m_title_context.active)
      return IPCReply(ES_EINVAL);

    tmd_view_size = m_core.m_title_context.tmd.GetRawView().size();
  }

  memory.Write_U32(static_cast<u32>(tmd_view_size), request.io_vectors[0].address);
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::DIGetTMDView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return IPCReply(ES_EINVAL);

  // Sanity check the TMD size.
  if (request.in_vectors[0].size >= 4 * 1024 * 1024)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  // Check whether the TMD view size is consistent.
  if (request.in_vectors[1].size != sizeof(u32) ||
      memory.Read_U32(request.in_vectors[1].address) != request.io_vectors[0].size)
  {
    return IPCReply(ES_EINVAL);
  }

  const bool has_tmd = request.in_vectors[0].size != 0;
  std::vector<u8> tmd_view;

  if (has_tmd)
  {
    std::vector<u8> tmd_bytes(request.in_vectors[0].size);
    memory.CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
    const ES::TMDReader tmd{std::move(tmd_bytes)};

    if (!tmd.IsValid())
      return IPCReply(ES_EINVAL);

    tmd_view = tmd.GetRawView();
  }
  else
  {
    // If no TMD was passed in and no title is active, IOS returns -1017.
    if (!m_core.m_title_context.active)
      return IPCReply(ES_EINVAL);

    tmd_view = m_core.m_title_context.tmd.GetRawView();
  }

  if (tmd_view.size() > request.io_vectors[0].size)
    return IPCReply(ES_EINVAL);

  memory.CopyToEmu(request.io_vectors[0].address, tmd_view.data(), tmd_view.size());
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::DIGetTicketView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) ||
      request.io_vectors[0].size != sizeof(ES::TicketView))
  {
    return IPCReply(ES_EINVAL);
  }

  const bool has_ticket_vector = request.in_vectors[0].size == sizeof(ES::Ticket);

  // This ioctlv takes either a signed ticket or no ticket, in which case the ticket size must be 0.
  if (!has_ticket_vector && request.in_vectors[0].size != 0)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  std::vector<u8> view;

  // If no ticket was passed in, IOS returns the ticket view for the current title.
  // Of course, this returns -1017 if no title is active and no ticket is passed.
  if (!has_ticket_vector)
  {
    if (!m_core.m_title_context.active)
      return IPCReply(ES_EINVAL);

    view = m_core.m_title_context.ticket.GetRawTicketView(0);
  }
  else
  {
    std::vector<u8> ticket_bytes(request.in_vectors[0].size);
    memory.CopyFromEmu(ticket_bytes.data(), request.in_vectors[0].address, ticket_bytes.size());
    const ES::TicketReader ticket{std::move(ticket_bytes)};

    view = ticket.GetRawTicketView(0);
  }

  memory.CopyToEmu(request.io_vectors[0].address, view.data(), view.size());
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::DIGetTMDSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  if (!m_core.m_title_context.active)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  memory.Write_U32(static_cast<u32>(m_core.m_title_context.tmd.GetBytes().size()),
                   request.io_vectors[0].address);
  return IPCReply(IPC_SUCCESS);
}

IPCReply ESDevice::DIGetTMD(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u32 tmd_size = memory.Read_U32(request.in_vectors[0].address);
  if (tmd_size != request.io_vectors[0].size)
    return IPCReply(ES_EINVAL);

  if (!m_core.m_title_context.active)
    return IPCReply(ES_EINVAL);

  const std::vector<u8>& tmd_bytes = m_core.m_title_context.tmd.GetBytes();

  if (static_cast<u32>(tmd_bytes.size()) > tmd_size)
    return IPCReply(ES_EINVAL);

  memory.CopyToEmu(request.io_vectors[0].address, tmd_bytes.data(), tmd_bytes.size());
  return IPCReply(IPC_SUCCESS);
}
}  // namespace IOS::HLE
