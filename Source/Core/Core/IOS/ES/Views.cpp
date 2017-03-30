// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <vector>

#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/ES/NandUtils.h"
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
// HACK: Since we do not want to require users to install disc updates when launching
//       Wii games from the game list (which is the inaccurate game boot path anyway),
//       IOSes have to be faked for games which reload IOS to work properly.
//       To minimize the effect of this hack, we should only do this for disc titles
//       booted from the game list, though.
static bool ShouldReturnFakeViewsForIOSes(u64 title_id, const TitleContext& context)
{
  const bool ios = IsTitleType(title_id, IOS::ES::TitleType::System) && title_id != TITLEID_SYSMENU;
  const bool disc_title = context.active && IOS::ES::IsDiscTitle(context.tmd.GetTitleId());
  return ios && SConfig::GetInstance().m_BootType == SConfig::BOOT_ISO && disc_title;
}

IPCCommandResult ES::GetTicketViewCount(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(TitleID);
  u32 view_count = ticket.IsValid() ? ticket.GetNumberOfTickets() : 0;

  if (ShouldReturnFakeViewsForIOSes(TitleID, GetTitleContext()))
  {
    view_count = 1;
    WARN_LOG(IOS_ES, "GetViewCount: Faking IOS title %016" PRIx64 " being present", TitleID);
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWCNT for titleID: %08x/%08x (View Count = %u)",
           static_cast<u32>(TitleID >> 32), static_cast<u32>(TitleID), view_count);

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
    u32 number_of_views = std::min(maxViews, ticket.GetNumberOfTickets());
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

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWS for titleID: %08x/%08x (MaxViews = %i)", (u32)(TitleID >> 32),
           (u32)TitleID, maxViews);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTMDViewSize(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const IOS::ES::TMDReader tmd = IOS::ES::FindInstalledTMD(TitleID);

  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const u32 view_size = static_cast<u32>(tmd.GetRawView().size());
  Memory::Write_U32(view_size, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "GetTMDViewSize: %u bytes for title %016" PRIx64, view_size, TitleID);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTMDViews(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 MaxCount = Memory::Read_U32(request.in_vectors[1].address);

  const IOS::ES::TMDReader tmd = IOS::ES::FindInstalledTMD(TitleID);

  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const std::vector<u8> raw_view = tmd.GetRawView();
  if (raw_view.size() != request.io_vectors[0].size)
    return GetDefaultReply(ES_EINVAL);

  Memory::CopyToEmu(request.io_vectors[0].address, raw_view.data(), raw_view.size());

  INFO_LOG(IOS_ES, "GetTMDView: %u bytes for title %016" PRIx64, MaxCount, TitleID);
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

  if (tmd_view.size() != request.io_vectors[0].size)
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

  const bool has_ticket_vector = request.in_vectors[0].size == 0x2A4;

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

}  // namespace Device
}  // namespace HLE
}  // namespace IOS
