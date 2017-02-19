// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// =======================================================
// File description
// -------------
/*  Here we handle /dev/es requests. We have cases for these functions, the exact
  DevKitPro/libogc name is in parenthesis:

  0x20 GetTitleID (ES_GetTitleID) (Input: none, Output: 8 bytes)
  0x1d GetDataDir (ES_GetDataDir) (Input: 8 bytes, Output: 30 bytes)

  0x1b DiGetTicketView (Input: none, Output: 216 bytes)
  0x16 GetConsumption (Input: 8 bytes, Output: 0 bytes, 4 bytes) // there are two output buffers

  0x12 GetNumTicketViews (ES_GetNumTicketViews) (Input: 8 bytes, Output: 4 bytes)
  0x14 GetTMDViewSize (ES_GetTMDViewSize) (Input: ?, Output: ?) // I don't get this anymore,
    it used to come after 0x12

  but only the first two are correctly supported. For the other four we ignore any potential
  input and only write zero to the out buffer. However, most games only use first two,
  but some Nintendo developed games use the other ones to:

  0x1b: Mario Galaxy, Mario Kart, SSBB
  0x16: Mario Galaxy, Mario Kart, SSBB
  0x12: Mario Kart
  0x14: Mario Kart: But only if we don't return a zeroed out buffer for the 0x12 question,
    and instead answer for example 1 will this question appear.

*/
// =============

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonFuncs.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Core/Boot/Boot_DOL.h"
#include "Core/ConfigManager.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/WiiRoot.h"
#include "Core/ec_wii.h"
#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
std::string ES::m_ContentFile;

constexpr u8 s_key_sd[0x10] = {0xab, 0x01, 0xb9, 0xd8, 0xe1, 0x62, 0x2b, 0x08,
                               0xaf, 0xba, 0xd8, 0x4d, 0xbf, 0xc2, 0xa5, 0x5d};
constexpr u8 s_key_ecc[0x1e] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
constexpr u8 s_key_empty[0x10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// default key table
constexpr const u8* s_key_table[11] = {
    s_key_ecc,    // ECC Private Key
    s_key_empty,  // Console ID
    s_key_empty,  // NAND AES Key
    s_key_empty,  // NAND HMAC
    s_key_empty,  // Common Key
    s_key_empty,  // PRNG seed
    s_key_sd,     // SD Key
    s_key_empty,  // Unknown
    s_key_empty,  // Unknown
    s_key_empty,  // Unknown
    s_key_empty,  // Unknown
};

ES::ES(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
}

void ES::LoadWAD(const std::string& _rContentFile)
{
  m_ContentFile = _rContentFile;
}

void ES::DecryptContent(u32 key_index, u8* iv, u8* input, u32 size, u8* new_iv, u8* output)
{
  mbedtls_aes_context AES_ctx;
  mbedtls_aes_setkey_dec(&AES_ctx, s_key_table[key_index], 128);
  memcpy(new_iv, iv, 16);
  mbedtls_aes_crypt_cbc(&AES_ctx, MBEDTLS_AES_DECRYPT, size, new_iv, input, output);
}

void ES::OpenInternal()
{
  auto& contentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(m_ContentFile);

  // check for cd ...
  if (contentLoader.IsValid())
  {
    m_TitleID = contentLoader.GetTitleID();

    m_TitleIDs.clear();
    DiscIO::cUIDsys uid_sys{Common::FromWhichRoot::FROM_SESSION_ROOT};
    uid_sys.GetTitleIDs(m_TitleIDs);
    // uncomment if  ES_GetOwnedTitlesCount / ES_GetOwnedTitles is implemented
    // m_TitleIDsOwned.clear();
    // DiscIO::cUIDsys::AccessInstance().GetTitleIDs(m_TitleIDsOwned, true);
  }
  else if (DVDInterface::VolumeIsValid())
  {
    // blindly grab the titleID from the disc - it's unencrypted at:
    // offset 0x0F8001DC and 0x0F80044C
    DVDInterface::GetVolume().GetTitleID(&m_TitleID);
  }
  else
  {
    m_TitleID = ((u64)0x00010000 << 32) | 0xF00DBEEF;
  }

  INFO_LOG(IOS_ES, "Set default title to %08x/%08x", (u32)(m_TitleID >> 32), (u32)m_TitleID);
}

void ES::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(m_ContentFile);
  OpenInternal();
  p.Do(m_AccessIdentID);
  p.Do(m_TitleIDs);

  m_addtitle_tmd.DoState(p);
  p.Do(m_addtitle_content_id);
  p.Do(m_addtitle_content_buffer);

  u32 Count = (u32)(m_ContentAccessMap.size());
  p.Do(Count);

  u32 CFD = 0;
  u32 Position = 0;
  u64 TitleID = 0;
  u16 Index = 0;
  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    for (u32 i = 0; i < Count; i++)
    {
      p.Do(CFD);
      p.Do(Position);
      p.Do(TitleID);
      p.Do(Index);
      CFD = OpenTitleContent(CFD, TitleID, Index);
      if (CFD != 0xffffffff)
      {
        m_ContentAccessMap[CFD].m_Position = Position;
      }
    }
  }
  else
  {
    for (auto& pair : m_ContentAccessMap)
    {
      CFD = pair.first;
      SContentAccess& Access = pair.second;
      Position = Access.m_Position;
      TitleID = Access.m_TitleID;
      Index = Access.m_Index;
      p.Do(CFD);
      p.Do(Position);
      p.Do(TitleID);
      p.Do(Index);
    }
  }
}

ReturnCode ES::Open(const OpenRequest& request)
{
  OpenInternal();

  if (m_is_active)
    INFO_LOG(IOS_ES, "Device was re-opened.");
  return Device::Open(request);
}

void ES::Close()
{
  m_ContentAccessMap.clear();
  m_TitleIDs.clear();
  m_TitleID = -1;
  m_AccessIdentID = 0;

  INFO_LOG(IOS_ES, "ES: Close");
  m_is_active = false;
  // clear the NAND content cache to make sure nothing remains open.
  DiscIO::CNANDContentManager::Access().ClearCache();
}

u32 ES::OpenTitleContent(u32 CFD, u64 TitleID, u16 Index)
{
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  if (!Loader.IsValid())
  {
    WARN_LOG(IOS_ES, "ES: loader not valid for %" PRIx64, TitleID);
    return 0xffffffff;
  }

  const DiscIO::SNANDContent* pContent = Loader.GetContentByIndex(Index);

  if (pContent == nullptr)
  {
    return 0xffffffff;  // TODO: what is the correct error value here?
  }

  SContentAccess Access;
  Access.m_Position = 0;
  Access.m_Index = pContent->m_Index;
  Access.m_Size = pContent->m_Size;
  Access.m_TitleID = TitleID;

  pContent->m_Data->Open();

  m_ContentAccessMap[CFD] = Access;
  return CFD;
}

IPCCommandResult ES::IOCtlV(const IOCtlVRequest& request)
{
  DEBUG_LOG(IOS_ES, "%s (0x%x)", GetDeviceName().c_str(), request.request);

  // Clear the IO buffers. Note that this is unsafe for other ioctlvs.
  for (const auto& io_vector : request.io_vectors)
  {
    if (!request.HasInputVectorWithAddress(io_vector.address))
      Memory::Memset(io_vector.address, 0, io_vector.size);
  }

  switch (request.request)
  {
  case IOCTL_ES_ADDTICKET:
    return AddTicket(request);
  case IOCTL_ES_ADDTITLESTART:
    return AddTitleStart(request);
  case IOCTL_ES_ADDCONTENTSTART:
    return AddContentStart(request);
  case IOCTL_ES_ADDCONTENTDATA:
    return AddContentData(request);
  case IOCTL_ES_ADDCONTENTFINISH:
    return AddContentFinish(request);
  case IOCTL_ES_ADDTITLEFINISH:
    return AddTitleFinish(request);
  case IOCTL_ES_GETDEVICEID:
    return ESGetDeviceID(request);
  case IOCTL_ES_GETTITLECONTENTSCNT:
    return GetTitleContentsCount(request);
  case IOCTL_ES_GETTITLECONTENTS:
    return GetTitleContents(request);
  case IOCTL_ES_OPENTITLECONTENT:
    return OpenTitleContent(request);
  case IOCTL_ES_OPENCONTENT:
    return OpenContent(request);
  case IOCTL_ES_READCONTENT:
    return ReadContent(request);
  case IOCTL_ES_CLOSECONTENT:
    return CloseContent(request);
  case IOCTL_ES_SEEKCONTENT:
    return SeekContent(request);
  case IOCTL_ES_GETTITLEDIR:
    return GetTitleDirectory(request);
  case IOCTL_ES_GETTITLEID:
    return GetTitleID(request);
  case IOCTL_ES_SETUID:
    return SetUID(request);
  case IOCTL_ES_GETTITLECNT:
    return GetTitleCount(request);
  case IOCTL_ES_GETTITLES:
    return GetTitles(request);
  case IOCTL_ES_GETVIEWCNT:
    return GetViewCount(request);
  case IOCTL_ES_GETVIEWS:
    return GetViews(request);
  case IOCTL_ES_GETTMDVIEWCNT:
    return GetTMDViewCount(request);
  case IOCTL_ES_GETTMDVIEWS:
    return GetTMDViews(request);
  case IOCTL_ES_GETCONSUMPTION:
    return GetConsumption(request);
  case IOCTL_ES_DELETETICKET:
    return DeleteTicket(request);
  case IOCTL_ES_DELETETITLECONTENT:
    return DeleteTitleContent(request);
  case IOCTL_ES_GETSTOREDTMDSIZE:
    return GetStoredTMDSize(request);
  case IOCTL_ES_GETSTOREDTMD:
    return GetStoredTMD(request);
  case IOCTL_ES_ENCRYPT:
    return Encrypt(request);
  case IOCTL_ES_DECRYPT:
    return Decrypt(request);
  case IOCTL_ES_LAUNCH:
    return Launch(request);
  case IOCTL_ES_LAUNCHBC:
    return LaunchBC(request);
  case IOCTL_ES_CHECKKOREAREGION:
    return CheckKoreaRegion(request);
  case IOCTL_ES_GETDEVICECERT:
    return GetDeviceCertificate(request);
  case IOCTL_ES_SIGN:
    return Sign(request);
  case IOCTL_ES_GETBOOT2VERSION:
    return GetBoot2Version(request);

  // Unsupported functions
  case IOCTL_ES_DIGETTICKETVIEW:
    return DIGetTicketView(request);
  case IOCTL_ES_GETOWNEDTITLECNT:
    return GetOwnedTitleCount(request);
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTicket(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 3,
                   "IOCTL_ES_ADDTICKET wrong number of inputs");

  INFO_LOG(IOS_ES, "IOCTL_ES_ADDTICKET");
  std::vector<u8> ticket(request.in_vectors[0].size);
  Memory::CopyFromEmu(ticket.data(), request.in_vectors[0].address, request.in_vectors[0].size);
  DiscIO::AddTicket(ticket);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTitleStart(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 4,
                   "IOCTL_ES_ADDTITLESTART wrong number of inputs");

  INFO_LOG(IOS_ES, "IOCTL_ES_ADDTITLESTART");
  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);

  m_addtitle_tmd.SetBytes(tmd);
  if (!m_addtitle_tmd.IsValid())
  {
    ERROR_LOG(IOS_ES, "Invalid TMD while adding title (size = %zd)", tmd.size());
    return GetDefaultReply(ES_INVALID_TMD);
  }

  // Write the TMD to title storage.
  std::string tmd_path =
      Common::GetTMDFileName(m_addtitle_tmd.GetTitleId(), Common::FROM_SESSION_ROOT);
  File::CreateFullPath(tmd_path);

  File::IOFile fp(tmd_path, "wb");
  fp.WriteBytes(tmd.data(), tmd.size());

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddContentStart(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2,
                   "IOCTL_ES_ADDCONTENTSTART wrong number of inputs");

  u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  u32 content_id = Memory::Read_U32(request.in_vectors[1].address);

  if (m_addtitle_content_id != 0xFFFFFFFF)
  {
    ERROR_LOG(IOS_ES, "Trying to add content when we haven't finished adding "
                      "another content. Unsupported.");
    return GetDefaultReply(ES_WRITE_FAILURE);
  }
  m_addtitle_content_id = content_id;

  m_addtitle_content_buffer.clear();

  INFO_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTSTART: title id %016" PRIx64 ", "
                   "content id %08x",
           title_id, m_addtitle_content_id);

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
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2,
                   "IOCTL_ES_ADDCONTENTDATA wrong number of inputs");

  u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTDATA: content fd %08x, "
                   "size %d",
           content_fd, request.in_vectors[1].size);

  u8* data_start = Memory::GetPointer(request.in_vectors[1].address);
  u8* data_end = data_start + request.in_vectors[1].size;
  m_addtitle_content_buffer.insert(m_addtitle_content_buffer.end(), data_start, data_end);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddContentFinish(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1,
                   "IOCTL_ES_ADDCONTENTFINISH wrong number of inputs");

  u32 content_fd = Memory::Read_U32(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_ADDCONTENTFINISH: content fd %08x", content_fd);

  // Try to find the title key from a pre-installed ticket.
  std::vector<u8> ticket = DiscIO::FindSignedTicket(m_addtitle_tmd.GetTitleId());
  if (ticket.size() == 0)
  {
    return GetDefaultReply(ES_NO_TICKET_INSTALLED);
  }

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_setkey_dec(&aes_ctx, DiscIO::GetKeyFromTicket(ticket).data(), 128);

  // The IV for title content decryption is the lower two bytes of the
  // content index, zero extended.
  TMDReader::Content content_info;
  if (!m_addtitle_tmd.FindContentById(m_addtitle_content_id, &content_info))
  {
    return GetDefaultReply(ES_INVALID_TMD);
  }
  u8 iv[16] = {0};
  iv[0] = (content_info.index >> 8) & 0xFF;
  iv[1] = content_info.index & 0xFF;
  std::vector<u8> decrypted_data(m_addtitle_content_buffer.size());
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, m_addtitle_content_buffer.size(), iv,
                        m_addtitle_content_buffer.data(), decrypted_data.data());

  std::string path = StringFromFormat(
      "%s%08x.app",
      Common::GetTitleContentPath(m_addtitle_tmd.GetTitleId(), Common::FROM_SESSION_ROOT).c_str(),
      m_addtitle_content_id);

  File::IOFile fp(path, "wb");
  fp.WriteBytes(decrypted_data.data(), decrypted_data.size());

  m_addtitle_content_id = 0xFFFFFFFF;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::AddTitleFinish(const IOCtlVRequest& request)
{
  INFO_LOG(IOS_ES, "IOCTL_ES_ADDTITLEFINISH");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ESGetDeviceID(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETDEVICEID no io vectors");

  const EcWii& ec = EcWii::GetInstance();
  INFO_LOG(IOS_ES, "IOCTL_ES_GETDEVICEID %08X", ec.GetNGID());
  Memory::Write_U32(ec.GetNGID(), request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitleContentsCount(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 1);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 1);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const DiscIO::CNANDContentLoader& rNANDContent = AccessContentDevice(TitleID);
  u16 NumberOfPrivateContent = 0;
  s32 return_value = IPC_SUCCESS;
  if (rNANDContent.IsValid())  // Not sure if dolphin will ever fail this check
  {
    NumberOfPrivateContent = rNANDContent.GetNumEntries();

    if ((u32)(TitleID >> 32) == 0x00010000)
      Memory::Write_U32(0, request.io_vectors[0].address);
    else
      Memory::Write_U32(NumberOfPrivateContent, request.io_vectors[0].address);
  }
  else
  {
    return_value = static_cast<s32>(rNANDContent.GetContentSize());
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLECONTENTSCNT: TitleID: %08x/%08x  content count %i",
           (u32)(TitleID >> 32), (u32)TitleID,
           rNANDContent.IsValid() ? NumberOfPrivateContent : (u32)rNANDContent.GetContentSize());

  return GetDefaultReply(return_value);
}

IPCCommandResult ES::GetTitleContents(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2,
                   "IOCTL_ES_GETTITLECONTENTS bad in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1,
                   "IOCTL_ES_GETTITLECONTENTS bad out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const DiscIO::CNANDContentLoader& rNANDContent = AccessContentDevice(TitleID);
  s32 return_value = IPC_SUCCESS;
  if (rNANDContent.IsValid())  // Not sure if dolphin will ever fail this check
  {
    for (u16 i = 0; i < rNANDContent.GetNumEntries(); i++)
    {
      Memory::Write_U32(rNANDContent.GetContentByIndex(i)->m_ContentID,
                        request.io_vectors[0].address + i * 4);
      INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLECONTENTS: Index %d: %08x", i,
               rNANDContent.GetContentByIndex(i)->m_ContentID);
    }
  }
  else
  {
    return_value = static_cast<s32>(rNANDContent.GetContentSize());
    ERROR_LOG(IOS_ES, "IOCTL_ES_GETTITLECONTENTS: Unable to open content %zu",
              rNANDContent.GetContentSize());
  }

  return GetDefaultReply(return_value);
}

IPCCommandResult ES::OpenTitleContent(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 3);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 0);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 Index = Memory::Read_U32(request.in_vectors[2].address);

  s32 CFD = OpenTitleContent(m_AccessIdentID++, TitleID, Index);

  INFO_LOG(IOS_ES, "IOCTL_ES_OPENTITLECONTENT: TitleID: %08x/%08x  Index %i -> got CFD %x",
           (u32)(TitleID >> 32), (u32)TitleID, Index, CFD);

  return GetDefaultReply(CFD);
}

IPCCommandResult ES::OpenContent(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 1);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 0);
  u32 Index = Memory::Read_U32(request.in_vectors[0].address);

  s32 CFD = OpenTitleContent(m_AccessIdentID++, m_TitleID, Index);
  INFO_LOG(IOS_ES, "IOCTL_ES_OPENCONTENT: Index %i -> got CFD %x", Index, CFD);

  return GetDefaultReply(CFD);
}

IPCCommandResult ES::ReadContent(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 1);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 1);

  u32 CFD = Memory::Read_U32(request.in_vectors[0].address);
  u32 Size = request.io_vectors[0].size;
  u32 Addr = request.io_vectors[0].address;

  auto itr = m_ContentAccessMap.find(CFD);
  if (itr == m_ContentAccessMap.end())
  {
    return GetDefaultReply(-1);
  }
  SContentAccess& rContent = itr->second;

  u8* pDest = Memory::GetPointer(Addr);

  if (rContent.m_Position + Size > rContent.m_Size)
  {
    Size = rContent.m_Size - rContent.m_Position;
  }

  if (Size > 0)
  {
    if (pDest)
    {
      const DiscIO::CNANDContentLoader& ContentLoader = AccessContentDevice(rContent.m_TitleID);
      // ContentLoader should never be invalid; rContent has been created by it.
      if (ContentLoader.IsValid())
      {
        const DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(rContent.m_Index);
        if (!pContent->m_Data->GetRange(rContent.m_Position, Size, pDest))
          ERROR_LOG(IOS_ES, "ES: failed to read %u bytes from %u!", Size, rContent.m_Position);
      }

      rContent.m_Position += Size;
    }
    else
    {
      PanicAlert("IOCTL_ES_READCONTENT - bad destination");
    }
  }

  DEBUG_LOG(IOS_ES,
            "IOCTL_ES_READCONTENT: CFD %x, Address 0x%x, Size %i -> stream pos %i (Index %i)", CFD,
            Addr, Size, rContent.m_Position, rContent.m_Index);

  return GetDefaultReply(Size);
}

IPCCommandResult ES::CloseContent(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 1);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 0);

  u32 CFD = Memory::Read_U32(request.in_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_CLOSECONTENT: CFD %x", CFD);

  auto itr = m_ContentAccessMap.find(CFD);
  if (itr == m_ContentAccessMap.end())
  {
    return GetDefaultReply(-1);
  }

  const DiscIO::CNANDContentLoader& ContentLoader = AccessContentDevice(itr->second.m_TitleID);
  // ContentLoader should never be invalid; we shouldn't be here if ES_OPENCONTENT failed before.
  if (ContentLoader.IsValid())
  {
    const DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(itr->second.m_Index);
    pContent->m_Data->Close();
  }

  m_ContentAccessMap.erase(itr);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::SeekContent(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 3);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 0);

  u32 CFD = Memory::Read_U32(request.in_vectors[0].address);
  u32 Addr = Memory::Read_U32(request.in_vectors[1].address);
  u32 Mode = Memory::Read_U32(request.in_vectors[2].address);

  auto itr = m_ContentAccessMap.find(CFD);
  if (itr == m_ContentAccessMap.end())
  {
    return GetDefaultReply(-1);
  }
  SContentAccess& rContent = itr->second;

  switch (Mode)
  {
  case 0:  // SET
    rContent.m_Position = Addr;
    break;

  case 1:  // CUR
    rContent.m_Position += Addr;
    break;

  case 2:  // END
    rContent.m_Position = rContent.m_Size + Addr;
    break;
  }

  DEBUG_LOG(IOS_ES, "IOCTL_ES_SEEKCONTENT: CFD %x, Address 0x%x, Mode %i -> Pos %i", CFD, Addr,
            Mode, rContent.m_Position);

  return GetDefaultReply(rContent.m_Position);
}

IPCCommandResult ES::GetTitleDirectory(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 1);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 1);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  char* Path = (char*)Memory::GetPointer(request.io_vectors[0].address);
  sprintf(Path, "/title/%08x/%08x/data", (u32)(TitleID >> 32), (u32)TitleID);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLEDIR: %s", Path);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitleID(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 0);
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETTITLEID no out buffer");

  Memory::Write_U64(m_TitleID, request.io_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLEID: %08x/%08x", (u32)(m_TitleID >> 32), (u32)m_TitleID);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::SetUID(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1, "IOCTL_ES_SETUID no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 0,
                   "IOCTL_ES_SETUID has a payload, it shouldn't");

  // TODO: fs permissions based on this
  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_SETUID titleID: %08x/%08x", (u32)(TitleID >> 32), (u32)TitleID);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitleCount(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 0, "IOCTL_ES_GETTITLECNT has an in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1,
                   "IOCTL_ES_GETTITLECNT has no out buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors[0].size == 4,
                   "IOCTL_ES_GETTITLECNT payload[0].size != 4");

  Memory::Write_U32((u32)m_TitleIDs.size(), request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLECNT: Number of Titles %zu", m_TitleIDs.size());

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitles(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1, "IOCTL_ES_GETTITLES has an in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETTITLES has no out buffer");

  u32 MaxCount = Memory::Read_U32(request.in_vectors[0].address);
  u32 Count = 0;
  for (int i = 0; i < (int)m_TitleIDs.size(); i++)
  {
    Memory::Write_U64(m_TitleIDs[i], request.io_vectors[0].address + i * 8);
    INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLES: %08x/%08x", (u32)(m_TitleIDs[i] >> 32),
             (u32)m_TitleIDs[i]);
    Count++;
    if (Count >= MaxCount)
      break;
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLES: Number of titles returned %i", Count);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetViewCount(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1, "IOCTL_ES_GETVIEWCNT no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETVIEWCNT no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  u32 retVal = 0;
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);
  u32 ViewCount =
      static_cast<u32>(Loader.GetTicket().size()) / DiscIO::CNANDContentLoader::TICKET_SIZE;

  if (!ViewCount)
  {
    std::string TicketFilename = Common::GetTicketFileName(TitleID, Common::FROM_SESSION_ROOT);
    if (File::Exists(TicketFilename))
    {
      u32 FileSize = (u32)File::GetSize(TicketFilename);
      _dbg_assert_msg_(IOS_ES, (FileSize % DiscIO::CNANDContentLoader::TICKET_SIZE) == 0,
                       "IOCTL_ES_GETVIEWCNT ticket file size seems to be wrong");

      ViewCount = FileSize / DiscIO::CNANDContentLoader::TICKET_SIZE;
      _dbg_assert_msg_(IOS_ES, (ViewCount > 0) && (ViewCount <= 4),
                       "IOCTL_ES_GETVIEWCNT ticket count seems to be wrong");
    }
    else if (TitleID >> 32 == 0x00000001)
    {
      // Fake a ticket view to make IOS reload work.
      ViewCount = 1;
    }
    else
    {
      ViewCount = 0;
      if (TitleID == TITLEID_SYSMENU)
      {
        PanicAlertT("There must be a ticket for 00000001/00000002. Your NAND dump is probably "
                    "incomplete.");
      }
      // retVal = ES_NO_TICKET_INSTALLED;
    }
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWCNT for titleID: %08x/%08x (View Count = %i)",
           (u32)(TitleID >> 32), (u32)TitleID, ViewCount);

  Memory::Write_U32(ViewCount, request.io_vectors[0].address);
  return GetDefaultReply(retVal);
}

IPCCommandResult ES::GetViews(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2, "IOCTL_ES_GETVIEWS no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETVIEWS no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 maxViews = Memory::Read_U32(request.in_vectors[1].address);
  u32 retVal = 0;

  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  const std::vector<u8>& ticket = Loader.GetTicket();

  if (ticket.empty())
  {
    std::string TicketFilename = Common::GetTicketFileName(TitleID, Common::FROM_SESSION_ROOT);
    if (File::Exists(TicketFilename))
    {
      File::IOFile pFile(TicketFilename, "rb");
      if (pFile)
      {
        u8 FileTicket[DiscIO::CNANDContentLoader::TICKET_SIZE];
        for (unsigned int View = 0;
             View != maxViews &&
             pFile.ReadBytes(FileTicket, DiscIO::CNANDContentLoader::TICKET_SIZE);
             ++View)
        {
          Memory::Write_U32(View, request.io_vectors[0].address + View * 0xD8);
          Memory::CopyToEmu(request.io_vectors[0].address + 4 + View * 0xD8, FileTicket + 0x1D0,
                            212);
        }
      }
    }
    else if (TitleID >> 32 == 0x00000001)
    {
      // For IOS titles, the ticket view isn't normally parsed by either the
      // SDK or libogc, just passed to LaunchTitle, so this
      // shouldn't matter at all.  Just fill out some fields just
      // to be on the safe side.
      u32 Address = request.io_vectors[0].address;
      Memory::Memset(Address, 0, 0xD8);
      Memory::Write_U64(TitleID, Address + 4 + (0x1dc - 0x1d0));  // title ID
      Memory::Write_U16(0xffff, Address + 4 + (0x1e4 - 0x1d0));   // unnnown
      Memory::Write_U32(0xff00, Address + 4 + (0x1ec - 0x1d0));   // access mask
      Memory::Memset(Address + 4 + (0x222 - 0x1d0), 0xff, 0x20);  // content permissions
    }
    else
    {
      // retVal = ES_NO_TICKET_INSTALLED;
      PanicAlertT("IOCTL_ES_GETVIEWS: Tried to get data from an unknown ticket: %08x/%08x",
                  (u32)(TitleID >> 32), (u32)TitleID);
    }
  }
  else
  {
    u32 view_count =
        static_cast<u32>(Loader.GetTicket().size()) / DiscIO::CNANDContentLoader::TICKET_SIZE;
    for (unsigned int view = 0; view != maxViews && view < view_count; ++view)
    {
      Memory::Write_U32(view, request.io_vectors[0].address + view * 0xD8);
      Memory::CopyToEmu(request.io_vectors[0].address + 4 + view * 0xD8,
                        &ticket[0x1D0 + (view * DiscIO::CNANDContentLoader::TICKET_SIZE)], 212);
    }
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWS for titleID: %08x/%08x (MaxViews = %i)", (u32)(TitleID >> 32),
           (u32)TitleID, maxViews);

  return GetDefaultReply(retVal);
}

IPCCommandResult ES::GetTMDViewCount(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1, "IOCTL_ES_GETTMDVIEWCNT no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETTMDVIEWCNT no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  u32 TMDViewCnt = 0;
  if (Loader.IsValid())
  {
    TMDViewCnt += DiscIO::CNANDContentLoader::TMD_VIEW_SIZE;
    TMDViewCnt += 2;                                               // title version
    TMDViewCnt += 2;                                               // num entries
    TMDViewCnt += (u32)Loader.GetContentSize() * (4 + 2 + 2 + 8);  // content id, index, type, size
  }
  Memory::Write_U32(TMDViewCnt, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTMDVIEWCNT: title: %08x/%08x (view size %i)", (u32)(TitleID >> 32),
           (u32)TitleID, TMDViewCnt);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTMDViews(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2, "IOCTL_ES_GETTMDVIEWCNT no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETTMDVIEWCNT no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 MaxCount = Memory::Read_U32(request.in_vectors[1].address);

  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTMDVIEWCNT: title: %08x/%08x   buffer size: %i",
           (u32)(TitleID >> 32), (u32)TitleID, MaxCount);

  if (Loader.IsValid())
  {
    u32 Address = request.io_vectors[0].address;

    Memory::CopyToEmu(Address, Loader.GetTMDView(), DiscIO::CNANDContentLoader::TMD_VIEW_SIZE);
    Address += DiscIO::CNANDContentLoader::TMD_VIEW_SIZE;

    Memory::Write_U16(Loader.GetTitleVersion(), Address);
    Address += 2;
    Memory::Write_U16(Loader.GetNumEntries(), Address);
    Address += 2;

    const std::vector<DiscIO::SNANDContent>& rContent = Loader.GetContent();
    for (size_t i = 0; i < Loader.GetContentSize(); i++)
    {
      Memory::Write_U32(rContent[i].m_ContentID, Address);
      Address += 4;
      Memory::Write_U16(rContent[i].m_Index, Address);
      Address += 2;
      Memory::Write_U16(rContent[i].m_Type, Address);
      Address += 2;
      Memory::Write_U64(rContent[i].m_Size, Address);
      Address += 8;
    }

    _dbg_assert_(IOS_ES, (Address - request.io_vectors[0].address) == request.io_vectors[0].size);
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTMDVIEWS: title: %08x/%08x (buffer size: %i)", (u32)(TitleID >> 32),
           (u32)TitleID, MaxCount);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetConsumption(const IOCtlVRequest& request)
{
  // This is at least what crediar's ES module does
  Memory::Write_U32(0, request.io_vectors[1].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_GETCONSUMPTION");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DeleteTicket(const IOCtlVRequest& request)
{
  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_DELETETICKET: title: %08x/%08x", (u32)(TitleID >> 32), (u32)TitleID);

  // Presumably return -1017 when delete fails
  if (!File::Delete(Common::GetTicketFileName(TitleID, Common::FROM_SESSION_ROOT)))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DeleteTitleContent(const IOCtlVRequest& request)
{
  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_DELETETITLECONTENT: title: %08x/%08x", (u32)(TitleID >> 32),
           (u32)TitleID);

  // Presumably return -1017 when title not installed TODO verify
  if (!DiscIO::CNANDContentManager::Access().RemoveTitle(TitleID, Common::FROM_SESSION_ROOT))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetStoredTMDSize(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1,
                   "IOCTL_ES_GETSTOREDTMDSIZE no in buffer");
  // _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1,
  //                  "IOCTL_ES_ES_GETSTOREDTMDSIZE no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  _dbg_assert_(IOS_ES, Loader.IsValid());
  u32 TMDCnt = 0;
  if (Loader.IsValid())
  {
    TMDCnt += DiscIO::CNANDContentLoader::TMD_HEADER_SIZE;
    TMDCnt += (u32)Loader.GetContentSize() * DiscIO::CNANDContentLoader::CONTENT_HEADER_SIZE;
  }

  if (request.io_vectors.size())
    Memory::Write_U32(TMDCnt, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETSTOREDTMDSIZE: title: %08x/%08x (view size %i)",
           (u32)(TitleID >> 32), (u32)TitleID, TMDCnt);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetStoredTMD(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() > 0, "IOCTL_ES_GETSTOREDTMD no in buffer");
  // requires 1 inbuffer and no outbuffer, presumably outbuffer required when second inbuffer is
  // used for maxcount (allocated mem?)
  // called with 1 inbuffer after deleting a titleid
  //_dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1,
  //                 "IOCTL_ES_GETSTOREDTMD no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 MaxCount = 0;
  if (request.in_vectors.size() > 1)
  {
    // TODO: actually use this param in when writing to the outbuffer :/
    MaxCount = Memory::Read_U32(request.in_vectors[1].address);
  }
  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETSTOREDTMD: title: %08x/%08x   buffer size: %i",
           (u32)(TitleID >> 32), (u32)TitleID, MaxCount);

  if (Loader.IsValid() && request.io_vectors.size())
  {
    u32 Address = request.io_vectors[0].address;

    Memory::CopyToEmu(Address, Loader.GetTMDHeader(), DiscIO::CNANDContentLoader::TMD_HEADER_SIZE);
    Address += DiscIO::CNANDContentLoader::TMD_HEADER_SIZE;

    const std::vector<DiscIO::SNANDContent>& rContent = Loader.GetContent();
    for (size_t i = 0; i < Loader.GetContentSize(); i++)
    {
      Memory::CopyToEmu(Address, rContent[i].m_Header,
                        DiscIO::CNANDContentLoader::CONTENT_HEADER_SIZE);
      Address += DiscIO::CNANDContentLoader::CONTENT_HEADER_SIZE;
    }

    _dbg_assert_(IOS_ES, (Address - request.io_vectors[0].address) == request.io_vectors[0].size);
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETSTOREDTMD: title: %08x/%08x (buffer size: %i)",
           (u32)(TitleID >> 32), (u32)TitleID, MaxCount);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Encrypt(const IOCtlVRequest& request)
{
  u32 keyIndex = Memory::Read_U32(request.in_vectors[0].address);
  u8* IV = Memory::GetPointer(request.in_vectors[1].address);
  u8* source = Memory::GetPointer(request.in_vectors[2].address);
  u32 size = request.in_vectors[2].size;
  u8* newIV = Memory::GetPointer(request.io_vectors[0].address);
  u8* destination = Memory::GetPointer(request.io_vectors[1].address);

  mbedtls_aes_context AES_ctx;
  mbedtls_aes_setkey_enc(&AES_ctx, s_key_table[keyIndex], 128);
  memcpy(newIV, IV, 16);
  mbedtls_aes_crypt_cbc(&AES_ctx, MBEDTLS_AES_ENCRYPT, size, newIV, source, destination);

  _dbg_assert_msg_(IOS_ES, keyIndex == 6,
                   "IOCTL_ES_ENCRYPT: Key type is not SD, data will be crap");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Decrypt(const IOCtlVRequest& request)
{
  u32 keyIndex = Memory::Read_U32(request.in_vectors[0].address);
  u8* IV = Memory::GetPointer(request.in_vectors[1].address);
  u8* source = Memory::GetPointer(request.in_vectors[2].address);
  u32 size = request.in_vectors[2].size;
  u8* newIV = Memory::GetPointer(request.io_vectors[0].address);
  u8* destination = Memory::GetPointer(request.io_vectors[1].address);

  DecryptContent(keyIndex, IV, source, size, newIV, destination);

  _dbg_assert_msg_(IOS_ES, keyIndex == 6,
                   "IOCTL_ES_DECRYPT: Key type is not SD, data will be crap");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Launch(const IOCtlVRequest& request)
{
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 2);
  bool bSuccess = false;

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 view = Memory::Read_U32(request.in_vectors[1].address);
  u64 ticketid = Memory::Read_U64(request.in_vectors[1].address + 4);
  u32 devicetype = Memory::Read_U32(request.in_vectors[1].address + 12);
  u64 titleid = Memory::Read_U64(request.in_vectors[1].address + 16);
  u16 access = Memory::Read_U16(request.in_vectors[1].address + 24);

  NOTICE_LOG(IOS_ES, "IOCTL_ES_LAUNCH %016" PRIx64 " %08x %016" PRIx64 " %08x %016" PRIx64 " %04x",
             TitleID, view, ticketid, devicetype, titleid, access);

  // ES_LAUNCH should probably reset thw whole state, which at least means closing all open files.
  // leaving them open through ES_LAUNCH may cause hangs and other funky behavior
  // (supposedly when trying to re-open those files).
  DiscIO::CNANDContentManager::Access().ClearCache();

  u64 ios_to_load = 0;
  std::string tContentFile;
  if ((u32)(TitleID >> 32) == 0x00000001 && TitleID != TITLEID_SYSMENU)
  {
    ios_to_load = TitleID;
    bSuccess = true;
  }
  else
  {
    const DiscIO::CNANDContentLoader& ContentLoader = AccessContentDevice(TitleID);
    if (ContentLoader.IsValid())
    {
      ios_to_load = 0x0000000100000000ULL | ContentLoader.GetIosVersion();

      u32 bootInd = ContentLoader.GetBootIndex();
      const DiscIO::SNANDContent* pContent = ContentLoader.GetContentByIndex(bootInd);
      if (pContent)
      {
        tContentFile = Common::GetTitleContentPath(TitleID, Common::FROM_SESSION_ROOT);
        std::unique_ptr<CDolLoader> pDolLoader =
            std::make_unique<CDolLoader>(pContent->m_Data->Get());

        if (pDolLoader->IsValid())
        {
          pDolLoader->Load();
          // TODO: Check why sysmenu does not load the DOL correctly
          // NAND titles start with address translation off at 0x3400 (via the PPC bootstub)
          //
          // The state of other CPU registers (like the BAT registers) doesn't matter much
          // because the realmode code at 0x3400 initializes everything itself anyway.
          MSR = 0;
          PC = 0x3400;
          bSuccess = true;
        }
        else
        {
          PanicAlertT("IOCTL_ES_LAUNCH: The DOL file is invalid!");
        }
      }
    }
  }

  if (!bSuccess)
  {
    PanicAlertT(
        "IOCTL_ES_LAUNCH: Game tried to reload a title that is not available in your NAND dump\n"
        "TitleID %016" PRIx64 ".\n Dolphin will likely hang now.",
        TitleID);
  }
  else
  {
    ResetAfterLaunch(ios_to_load);
    SetDefaultContentFile(tContentFile);
  }

  // Generate a "reply" to the IPC command.  ES_LAUNCH is unique because it
  // involves restarting IOS; IOS generates two acknowledgements in a row.
  // Note: If we just reset the PPC, don't write anything to the command buffer. This
  // could clobber the DOL we just loaded.
  EnqueueCommandAcknowledgement(request.address, 0);
  return GetNoReply();
}

IPCCommandResult ES::LaunchBC(const IOCtlVRequest& request)
{
  if (request.in_vectors.size() != 0 || request.io_vectors.size() != 0)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  // Here, IOS checks the clock speed and prevents ioctlv 0x25 from being used in GC mode.
  // An alternative way to do this is to check whether the current active IOS is MIOS.
  if (GetVersion() == 0x101)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  ResetAfterLaunch(0x0000000100000100);
  EnqueueCommandAcknowledgement(request.address, 0);
  return GetNoReply();
}

void ES::ResetAfterLaunch(const u64 ios_to_load) const
{
  Reload(ios_to_load);
}

IPCCommandResult ES::CheckKoreaRegion(const IOCtlVRequest& request)
{
  // note by DacoTaco : name is unknown, I just tried to name it SOMETHING.
  // IOS70 has this to let system menu 4.2 check if the console is region changed. it returns
  // -1017
  // if the IOS didn't find the Korean keys and 0 if it does. 0 leads to a error 003
  INFO_LOG(IOS_ES, "IOCTL_ES_CHECKKOREAREGION: Title checked for Korean keys.");
  return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
}

IPCCommandResult ES::GetDeviceCertificate(const IOCtlVRequest& request)
{
  // (Input: none, Output: 384 bytes)
  INFO_LOG(IOS_ES, "IOCTL_ES_GETDEVICECERT");
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 1);
  u8* destination = Memory::GetPointer(request.io_vectors[0].address);

  const EcWii& ec = EcWii::GetInstance();
  MakeNGCert(destination, ec.GetNGID(), ec.GetNGKeyID(), ec.GetNGPriv(), ec.GetNGSig());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Sign(const IOCtlVRequest& request)
{
  INFO_LOG(IOS_ES, "IOCTL_ES_SIGN");
  u8* ap_cert_out = Memory::GetPointer(request.io_vectors[1].address);
  u8* data = Memory::GetPointer(request.in_vectors[0].address);
  u32 data_size = request.in_vectors[0].size;
  u8* sig_out = Memory::GetPointer(request.io_vectors[0].address);

  const EcWii& ec = EcWii::GetInstance();
  MakeAPSigAndCert(sig_out, ap_cert_out, m_TitleID, data, data_size, ec.GetNGPriv(), ec.GetNGID());

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetBoot2Version(const IOCtlVRequest& request)
{
  INFO_LOG(IOS_ES, "IOCTL_ES_GETBOOT2VERSION");

  // as of 26/02/2012, this was latest bootmii version
  Memory::Write_U32(4, request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::DIGetTicketView(const IOCtlVRequest& request)
{
  // (Input: none, Output: 216 bytes) bug crediar :D
  WARN_LOG(IOS_ES, "IOCTL_ES_DIGETTICKETVIEW: this looks really wrong...");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetOwnedTitleCount(const IOCtlVRequest& request)
{
  INFO_LOG(IOS_ES, "IOCTL_ES_GETOWNEDTITLECNT");
  Memory::Write_U32(0, request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

const DiscIO::CNANDContentLoader& ES::AccessContentDevice(u64 title_id)
{
  // for WADs, the passed title id and the stored title id match; along with m_ContentFile being set
  // to the
  // actual WAD file name. We cannot simply get a NAND Loader for the title id in those cases, since
  // the WAD
  // need not be installed in the NAND, but it could be opened directly from a WAD file anywhere on
  // disk.
  if (m_TitleID == title_id && !m_ContentFile.empty())
    return DiscIO::CNANDContentManager::Access().GetNANDLoader(m_ContentFile);

  return DiscIO::CNANDContentManager::Access().GetNANDLoader(title_id, Common::FROM_SESSION_ROOT);
}

u32 ES::ES_DIVerify(const std::vector<u8>& tmd)
{
  u64 title_id = 0xDEADBEEFDEADBEEFull;
  u64 tmd_title_id = Common::swap64(&tmd[0x18C]);

  DVDInterface::GetVolume().GetTitleID(&title_id);
  if (title_id != tmd_title_id)
    return -1;

  std::string tmd_path = Common::GetTMDFileName(tmd_title_id, Common::FROM_SESSION_ROOT);

  File::CreateFullPath(tmd_path);
  File::CreateFullPath(Common::GetTitleDataPath(tmd_title_id, Common::FROM_SESSION_ROOT));

  if (!File::Exists(tmd_path))
  {
    File::IOFile tmd_file(tmd_path, "wb");
    if (!tmd_file.WriteBytes(tmd.data(), tmd.size()))
      ERROR_LOG(IOS_ES, "DIVerify failed to write disc TMD to NAND.");
  }
  DiscIO::cUIDsys uid_sys{Common::FromWhichRoot::FROM_SESSION_ROOT};
  uid_sys.AddTitle(tmd_title_id);
  // DI_VERIFY writes to title.tmd, which is read and cached inside the NAND Content Manager.
  // clear the cache to avoid content access mismatches.
  DiscIO::CNANDContentManager::Access().ClearCache();
  return 0;
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
