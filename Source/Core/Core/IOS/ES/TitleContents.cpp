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
#include "DiscIO/NANDContentLoader.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
u32 ES::OpenTitleContent(u32 CFD, u64 TitleID, u16 Index)
{
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  if (!Loader.IsValid() || !Loader.GetTMD().IsValid() || !Loader.GetTicket().IsValid())
  {
    WARN_LOG(IOS_ES, "ES: loader not valid for %" PRIx64, TitleID);
    return 0xffffffff;
  }

  const DiscIO::SNANDContent* pContent = Loader.GetContentByIndex(Index);

  if (pContent == nullptr)
  {
    return 0xffffffff;  // TODO: what is the correct error value here?
  }

  OpenedContent content;
  content.m_position = 0;
  content.m_content = pContent->m_metadata;
  content.m_title_id = TitleID;

  pContent->m_Data->Open();

  m_ContentAccessMap[CFD] = content;
  return CFD;
}

IPCCommandResult ES::OpenTitleContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 Index = Memory::Read_U32(request.in_vectors[2].address);

  s32 CFD = OpenTitleContent(m_AccessIdentID++, TitleID, Index);

  INFO_LOG(IOS_ES, "IOCTL_ES_OPENTITLECONTENT: TitleID: %08x/%08x  Index %i -> got CFD %x",
           (u32)(TitleID >> 32), (u32)TitleID, Index, CFD);

  return GetDefaultReply(CFD);
}

IPCCommandResult ES::OpenContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);
  u32 Index = Memory::Read_U32(request.in_vectors[0].address);

  if (!GetTitleContext().active)
    return GetDefaultReply(ES_EINVAL);

  s32 CFD = OpenTitleContent(m_AccessIdentID++, GetTitleContext().tmd.GetTitleId(), Index);
  INFO_LOG(IOS_ES, "IOCTL_ES_OPENCONTENT: Index %i -> got CFD %x", Index, CFD);

  return GetDefaultReply(CFD);
}

IPCCommandResult ES::ReadContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  u32 CFD = Memory::Read_U32(request.in_vectors[0].address);
  u32 Size = request.io_vectors[0].size;
  u32 Addr = request.io_vectors[0].address;

  auto itr = m_ContentAccessMap.find(CFD);
  if (itr == m_ContentAccessMap.end())
  {
    return GetDefaultReply(-1);
  }
  OpenedContent& rContent = itr->second;

  u8* pDest = Memory::GetPointer(Addr);

  if (rContent.m_position + Size > rContent.m_content.size)
  {
    Size = static_cast<u32>(rContent.m_content.size) - rContent.m_position;
  }

  if (Size > 0)
  {
    if (pDest)
    {
      const DiscIO::CNANDContentLoader& ContentLoader = AccessContentDevice(rContent.m_title_id);
      // ContentLoader should never be invalid; rContent has been created by it.
      if (ContentLoader.IsValid() && ContentLoader.GetTicket().IsValid())
      {
        const DiscIO::SNANDContent* pContent =
            ContentLoader.GetContentByIndex(rContent.m_content.index);
        if (!pContent->m_Data->GetRange(rContent.m_position, Size, pDest))
          ERROR_LOG(IOS_ES, "ES: failed to read %u bytes from %u!", Size, rContent.m_position);
      }

      rContent.m_position += Size;
    }
    else
    {
      PanicAlert("IOCTL_ES_READCONTENT - bad destination");
    }
  }

  DEBUG_LOG(IOS_ES,
            "IOCTL_ES_READCONTENT: CFD %x, Address 0x%x, Size %i -> stream pos %i (Index %i)", CFD,
            Addr, Size, rContent.m_position, rContent.m_content.index);

  return GetDefaultReply(Size);
}

IPCCommandResult ES::CloseContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  u32 CFD = Memory::Read_U32(request.in_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_CLOSECONTENT: CFD %x", CFD);

  auto itr = m_ContentAccessMap.find(CFD);
  if (itr == m_ContentAccessMap.end())
  {
    return GetDefaultReply(-1);
  }

  const DiscIO::CNANDContentLoader& ContentLoader = AccessContentDevice(itr->second.m_title_id);
  // ContentLoader should never be invalid; we shouldn't be here if ES_OPENCONTENT failed before.
  if (ContentLoader.IsValid())
  {
    const DiscIO::SNANDContent* pContent =
        ContentLoader.GetContentByIndex(itr->second.m_content.index);
    pContent->m_Data->Close();
  }

  m_ContentAccessMap.erase(itr);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::SeekContent(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);

  u32 CFD = Memory::Read_U32(request.in_vectors[0].address);
  u32 Addr = Memory::Read_U32(request.in_vectors[1].address);
  u32 Mode = Memory::Read_U32(request.in_vectors[2].address);

  auto itr = m_ContentAccessMap.find(CFD);
  if (itr == m_ContentAccessMap.end())
  {
    return GetDefaultReply(-1);
  }
  OpenedContent& rContent = itr->second;

  switch (Mode)
  {
  case 0:  // SET
    rContent.m_position = Addr;
    break;

  case 1:  // CUR
    rContent.m_position += Addr;
    break;

  case 2:  // END
    rContent.m_position = static_cast<u32>(rContent.m_content.size) + Addr;
    break;
  }

  DEBUG_LOG(IOS_ES, "IOCTL_ES_SEEKCONTENT: CFD %x, Address 0x%x, Mode %i -> Pos %i", CFD, Addr,
            Mode, rContent.m_position);

  return GetDefaultReply(rContent.m_position);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
