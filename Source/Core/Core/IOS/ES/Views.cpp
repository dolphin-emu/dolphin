// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cinttypes>
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
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
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
  const bool ios =
      IsTitleType(title_id, IOS::ES::TitleType::System) && title_id != Titles::SYSTEM_MENU;
  const bool disc_title = context.active && IOS::ES::IsDiscTitle(context.tmd.GetTitleId());
  return Core::WantsDeterminism() ||
         (ios && SConfig::GetInstance().m_disc_booted_from_game_list && disc_title);
}

IPCCommandResult ES::GetTicketViewCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(TitleID);
  u32 view_count = ticket.IsValid() ? static_cast<u32>(ticket.GetNumberOfTickets()) : 0;

  if (ShouldReturnFakeViewsForIOSes(TitleID, GetTitleContext()))
  {
    view_count = 1;
    WARN_LOG(IOS_ES, "GetViewCount: Faking IOS title %016" PRIx64 " being present", TitleID);
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWCNT for titleID: %016" PRIx64 " (View Count = %u)", TitleID,
           view_count);

  Memory::Write_U32(view_count, request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTicketViews(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 maxViews = Memory::Read_U32(request.in_vectors[1].address);

  const IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(TitleID);

  if (ticket.IsValid())
  {
    u32 number_of_views = std::min(maxViews, static_cast<u32>(ticket.GetNumberOfTickets()));
    for (u32 view = 0; view < number_of_views; ++view)
    {
      const std::vector<u8> ticket_view = ticket.GetRawTicketView(view);
      Memory::CopyToEmu(request.io_vectors[0].address + view * sizeof(IOS::ES::TicketView),
                        ticket_view.data(), ticket_view.size());
    }
  }
  else if (ShouldReturnFakeViewsForIOSes(TitleID, GetTitleContext()))
  {
    Memory::Memset(request.io_vectors[0].address, 0, sizeof(IOS::ES::TicketView));
    WARN_LOG(IOS_ES, "GetViews: Faking IOS title %016" PRIx64 " being present", TitleID);
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWS for titleID: %016" PRIx64 " (MaxViews = %i)", TitleID,
           maxViews);

  return GetDefaultReply(IPC_SUCCESS);
}

ReturnCode ES::GetV0TicketFromView(const u8* ticket_view, u8* ticket) const
{
  const u64 title_id = Common::swap64(&ticket_view[offsetof(IOS::ES::TicketView, title_id)]);
  const u64 ticket_id = Common::swap64(&ticket_view[offsetof(IOS::ES::TicketView, ticket_id)]);

  const auto installed_ticket = DiscIO::FindSignedTicket(title_id);
  // TODO: when we get std::optional, check for presence instead of validity.
  // This is close enough, though.
  if (!installed_ticket.IsValid())
    return ES_NO_TICKET;

  const std::vector<u8> ticket_bytes = installed_ticket.GetRawTicket(ticket_id);
  if (ticket_bytes.empty())
    return ES_NO_TICKET;

  if (!GetTitleContext().active)
    return ES_EINVAL;

  // Check for permission to export the ticket.
  const u32 title_identifier = static_cast<u32>(GetTitleContext().tmd.GetTitleId());
  const u32 permitted_title_mask =
      Common::swap32(ticket_bytes.data() + offsetof(IOS::ES::Ticket, permitted_title_mask));
  const u32 permitted_title_id =
      Common::swap32(ticket_bytes.data() + offsetof(IOS::ES::Ticket, permitted_title_id));
  const u8 title_export_allowed = ticket_bytes[offsetof(IOS::ES::Ticket, title_export_allowed)];

  // This is the check present in IOS. The 5 does not correspond to any known constant, sadly.
  if (!title_identifier || (title_identifier & ~permitted_title_mask) != permitted_title_id ||
      (title_export_allowed & 0xF) != 5)
  {
    return ES_EACCES;
  }

  std::copy(ticket_bytes.begin(), ticket_bytes.end(), ticket);
  return IPC_SUCCESS;
}

ReturnCode ES::GetTicketFromView(const u8* ticket_view, u8* ticket, u32* ticket_size) const
{
  const u8 version = ticket_view[offsetof(IOS::ES::TicketView, version)];
  if (version == 1)
  {
    // Currently, we have no support for v1 tickets at all (unlike IOS), so we fake it
    // and return that there is no ticket.
    // TODO: implement GetV1TicketFromView when we gain v1 ticket support.
    ERROR_LOG(IOS_ES, "GetV1TicketFromView: Unimplemented -- returning -1028");
    return ES_NO_TICKET;
  }
  if (ticket != nullptr)
  {
    if (*ticket_size >= sizeof(IOS::ES::Ticket))
      return GetV0TicketFromView(ticket_view, ticket);
    return ES_EINVAL;
  }
  *ticket_size = sizeof(IOS::ES::Ticket);
  return IPC_SUCCESS;
}

IPCCommandResult ES::GetV0TicketFromView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) ||
      request.in_vectors[0].size != sizeof(IOS::ES::TicketView) ||
      request.io_vectors[0].size != sizeof(IOS::ES::Ticket))
  {
    return GetDefaultReply(ES_EINVAL);
  }
  return GetDefaultReply(GetV0TicketFromView(Memory::GetPointer(request.in_vectors[0].address),
                                             Memory::GetPointer(request.io_vectors[0].address)));
}

IPCCommandResult ES::GetTicketSizeFromView(const IOCtlVRequest& request)
{
  u32 ticket_size = 0;
  if (!request.HasNumberOfValidVectors(1, 1) ||
      request.in_vectors[0].size != sizeof(IOS::ES::TicketView) ||
      request.io_vectors[0].size != sizeof(ticket_size))
  {
    return GetDefaultReply(ES_EINVAL);
  }
  const ReturnCode ret =
      GetTicketFromView(Memory::GetPointer(request.in_vectors[0].address), nullptr, &ticket_size);
  Memory::Write_U32(ticket_size, request.io_vectors[0].address);
  return GetDefaultReply(ret);
}

IPCCommandResult ES::GetTicketFromView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) ||
      request.in_vectors[0].size != sizeof(IOS::ES::TicketView) ||
      request.in_vectors[1].size != sizeof(u32))
  {
    return GetDefaultReply(ES_EINVAL);
  }

  u32 ticket_size = Memory::Read_U32(request.in_vectors[1].address);
  if (ticket_size != request.io_vectors[0].size)
    return GetDefaultReply(ES_EINVAL);

  return GetDefaultReply(GetTicketFromView(Memory::GetPointer(request.in_vectors[0].address),
                                           Memory::GetPointer(request.io_vectors[0].address),
                                           &ticket_size));
}

IPCCommandResult ES::GetTMDViewSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const IOS::ES::TMDReader tmd = FindInstalledTMD(TitleID);

  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const u32 view_size = static_cast<u32>(tmd.GetRawView().size());
  Memory::Write_U32(view_size, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "GetTMDViewSize: %u bytes for title %016" PRIx64, view_size, TitleID);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTMDViews(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) ||
      request.in_vectors[0].size != sizeof(IOS::ES::TMDHeader::title_id) ||
      request.in_vectors[1].size != sizeof(u32) ||
      Memory::Read_U32(request.in_vectors[1].address) != request.io_vectors[0].size)
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const IOS::ES::TMDReader tmd = FindInstalledTMD(title_id);

  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const std::vector<u8> raw_view = tmd.GetRawView();
  if (request.io_vectors[0].size < raw_view.size())
    return GetDefaultReply(ES_EINVAL);

  Memory::CopyToEmu(request.io_vectors[0].address, raw_view.data(), raw_view.size());

  INFO_LOG(IOS_ES, "GetTMDView: %zu bytes for title %016" PRIx64, raw_view.size(), title_id);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DIGetTMDViewSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  // Sanity check the TMD size.
  if (request.in_vectors[0].size >= 4 * 1024 * 1024)
    return GetDefaultReply(ES_EINVAL);

  if (request.io_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  const bool has_tmd = request.in_vectors[0].size != 0;
  size_t tmd_view_size = 0;

  if (has_tmd)
  {
    std::vector<u8> tmd_bytes(request.in_vectors[0].size);
    Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
    const IOS::ES::TMDReader tmd{std::move(tmd_bytes)};

    // Yes, this returns -1017, not ES_INVALID_TMD.
    // IOS simply checks whether the TMD has all required content entries.
    if (!tmd.IsValid())
      return GetDefaultReply(ES_EINVAL);

    tmd_view_size = tmd.GetRawView().size();
  }
  else
  {
    // If no TMD was passed in and no title is active, IOS returns -1017.
    if (!GetTitleContext().active)
      return GetDefaultReply(ES_EINVAL);

    tmd_view_size = GetTitleContext().tmd.GetRawView().size();
  }

  Memory::Write_U32(static_cast<u32>(tmd_view_size), request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DIGetTMDView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_EINVAL);

  // Sanity check the TMD size.
  if (request.in_vectors[0].size >= 4 * 1024 * 1024)
    return GetDefaultReply(ES_EINVAL);

  // Check whether the TMD view size is consistent.
  if (request.in_vectors[1].size != sizeof(u32) ||
      Memory::Read_U32(request.in_vectors[1].address) != request.io_vectors[0].size)
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const bool has_tmd = request.in_vectors[0].size != 0;
  std::vector<u8> tmd_view;

  if (has_tmd)
  {
    std::vector<u8> tmd_bytes(request.in_vectors[0].size);
    Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[0].address, tmd_bytes.size());
    const IOS::ES::TMDReader tmd{std::move(tmd_bytes)};

    if (!tmd.IsValid())
      return GetDefaultReply(ES_EINVAL);

    tmd_view = tmd.GetRawView();
  }
  else
  {
    // If no TMD was passed in and no title is active, IOS returns -1017.
    if (!GetTitleContext().active)
      return GetDefaultReply(ES_EINVAL);

    tmd_view = GetTitleContext().tmd.GetRawView();
  }

  if (tmd_view.size() > request.io_vectors[0].size)
    return GetDefaultReply(ES_EINVAL);

  Memory::CopyToEmu(request.io_vectors[0].address, tmd_view.data(), tmd_view.size());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DIGetTicketView(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) ||
      request.io_vectors[0].size != sizeof(IOS::ES::TicketView))
  {
    return GetDefaultReply(ES_EINVAL);
  }

  const bool has_ticket_vector = request.in_vectors[0].size == sizeof(IOS::ES::Ticket);

  // This ioctlv takes either a signed ticket or no ticket, in which case the ticket size must be 0.
  if (!has_ticket_vector && request.in_vectors[0].size != 0)
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> view;

  // If no ticket was passed in, IOS returns the ticket view for the current title.
  // Of course, this returns -1017 if no title is active and no ticket is passed.
  if (!has_ticket_vector)
  {
    if (!GetTitleContext().active)
      return GetDefaultReply(ES_EINVAL);

    view = GetTitleContext().ticket.GetRawTicketView(0);
  }
  else
  {
    std::vector<u8> ticket_bytes(request.in_vectors[0].size);
    Memory::CopyFromEmu(ticket_bytes.data(), request.in_vectors[0].address, ticket_bytes.size());
    const IOS::ES::TicketReader ticket{std::move(ticket_bytes)};

    view = ticket.GetRawTicketView(0);
  }

  Memory::CopyToEmu(request.io_vectors[0].address, view.data(), view.size());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DIGetTMDSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  if (!GetTitleContext().active)
    return GetDefaultReply(ES_EINVAL);

  Memory::Write_U32(static_cast<u32>(GetTitleContext().tmd.GetBytes().size()),
                    request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DIGetTMD(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  const u32 tmd_size = Memory::Read_U32(request.in_vectors[0].address);
  if (tmd_size != request.io_vectors[0].size)
    return GetDefaultReply(ES_EINVAL);

  if (!GetTitleContext().active)
    return GetDefaultReply(ES_EINVAL);

  const std::vector<u8>& tmd_bytes = GetTitleContext().tmd.GetBytes();

  if (static_cast<u32>(tmd_bytes.size()) > tmd_size)
    return GetDefaultReply(ES_EINVAL);

  Memory::CopyToEmu(request.io_vectors[0].address, tmd_bytes.data(), tmd_bytes.size());
  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
