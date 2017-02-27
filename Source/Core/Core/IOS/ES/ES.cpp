// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/Align.h"
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

bool ES::LaunchTitle(u64 title_id, bool skip_reload) const
{
  NOTICE_LOG(IOS_ES, "Launching title %016" PRIx64 "...", title_id);

  // ES_Launch should probably reset the whole state, which at least means closing all open files.
  // leaving them open through ES_Launch may cause hangs and other funky behavior
  // (supposedly when trying to re-open those files).
  DiscIO::CNANDContentManager::Access().ClearCache();

  if (IsTitleType(title_id, IOS::ES::TitleType::System) && title_id != TITLEID_SYSMENU)
    return LaunchIOS(title_id);
  return LaunchPPCTitle(title_id, skip_reload);
}

bool ES::LaunchIOS(u64 ios_title_id) const
{
  return Reload(ios_title_id);
}

bool ES::LaunchPPCTitle(u64 title_id, bool skip_reload) const
{
  const DiscIO::CNANDContentLoader& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
  {
    PanicAlertT("Could not launch title %016" PRIx64 " because it is missing from the NAND.\n"
                "The emulated software will likely hang now.",
                title_id);
    return false;
  }

  // Before launching a title, IOS first reads the TMD and reloads into the specified IOS version,
  // even when that version is already running. After it has reloaded, ES_Launch will be called
  // again with the reload skipped, and the PPC will be bootstrapped then.
  if (!skip_reload)
  {
    SetTitleToLaunch(title_id);
    const u64 required_ios = content_loader.GetTMD().GetIOSId();
    return LaunchTitle(required_ios);
  }

  SetDefaultContentFile(Common::GetTitleContentPath(title_id, Common::FROM_SESSION_ROOT));
  return BootstrapPPC(content_loader);
}

void ES::OpenInternal()
{
  auto& contentLoader = DiscIO::CNANDContentManager::Access().GetNANDLoader(m_ContentFile);

  // check for cd ...
  if (contentLoader.IsValid())
  {
    m_TitleID = contentLoader.GetTMD().GetTitleId();

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

  p.Do(m_export_title_context.valid);
  m_export_title_context.tmd.DoState(p);
  p.Do(m_export_title_context.title_key);
  p.Do(m_export_title_context.contents);

  u32 Count = (u32)(m_ContentAccessMap.size());
  p.Do(Count);

  if (p.GetMode() == PointerWrap::MODE_READ)
  {
    for (u32 i = 0; i < Count; i++)
    {
      u32 cfd = 0;
      OpenedContent content;
      p.Do(cfd);
      p.Do(content);
      cfd = OpenTitleContent(cfd, content.m_title_id, content.m_content.index);
    }
  }
  else
  {
    for (const auto& pair : m_ContentAccessMap)
    {
      p.Do(pair.first);
      p.Do(pair.second);
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

  if (!Loader.IsValid() || !Loader.GetTicket().IsValid())
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
  case IOCTL_ES_ADDTMD:
    return AddTMD(request);
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
  case IOCTL_ES_DELETETITLE:
    return DeleteTitle(request);
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
  case IOCTL_ES_EXPORTTITLEINIT:
    return ExportTitleInit(request);
  case IOCTL_ES_EXPORTCONTENTBEGIN:
    return ExportContentBegin(request);
  case IOCTL_ES_EXPORTCONTENTDATA:
    return ExportContentData(request);
  case IOCTL_ES_EXPORTCONTENTEND:
    return ExportContentEnd(request);
  case IOCTL_ES_EXPORTTITLEDONE:
    return ExportTitleDone(request);
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

  DiscIO::AddTicket(IOS::ES::TicketReader{std::move(ticket)});

  return GetDefaultReply(IPC_SUCCESS);
}

// TODO: write this to /tmp (or /import?) first, as title imports can be cancelled.
static bool WriteTMD(const IOS::ES::TMDReader& tmd)
{
  const std::string tmd_path = Common::GetTMDFileName(tmd.GetTitleId(), Common::FROM_SESSION_ROOT);
  File::CreateFullPath(tmd_path);

  File::IOFile fp(tmd_path, "wb");
  return fp.WriteBytes(tmd.GetRawTMD().data(), tmd.GetRawTMD().size());
}

IPCCommandResult ES::AddTMD(const IOCtlVRequest& request)
{
  // This may appear to be very similar to AddTitleStart, but AddTitleStart takes
  // three additional vectors and may do some additional processing -- so let's keep these separate.

  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  std::vector<u8> tmd(request.in_vectors[0].size);
  Memory::CopyFromEmu(tmd.data(), request.in_vectors[0].address, request.in_vectors[0].size);

  m_addtitle_tmd.SetBytes(tmd);
  if (!m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_INVALID_TMD);

  if (!WriteTMD(m_addtitle_tmd))
    return GetDefaultReply(ES_WRITE_FAILURE);

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

  if (!WriteTMD(m_addtitle_tmd))
    return GetDefaultReply(ES_WRITE_FAILURE);

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

  if (!m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

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

  if (!m_addtitle_tmd.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  // Try to find the title key from a pre-installed ticket.
  IOS::ES::TicketReader ticket = DiscIO::FindSignedTicket(m_addtitle_tmd.GetTitleId());
  if (!ticket.IsValid())
  {
    return GetDefaultReply(ES_NO_TICKET_INSTALLED);
  }

  mbedtls_aes_context aes_ctx;
  mbedtls_aes_setkey_dec(&aes_ctx, ticket.GetTitleKey().data(), 128);

  // The IV for title content decryption is the lower two bytes of the
  // content index, zero extended.
  IOS::ES::Content content_info;
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
  fp.WriteBytes(decrypted_data.data(), content_info.size);

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

  const DiscIO::CNANDContentLoader& nand_content = AccessContentDevice(TitleID);
  if (!nand_content.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u16 num_contents = nand_content.GetTMD().GetNumContents();

  if ((u32)(TitleID >> 32) == 0x00010000)
    Memory::Write_U32(0, request.io_vectors[0].address);
  else
    Memory::Write_U32(num_contents, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLECONTENTSCNT: TitleID: %08x/%08x  content count %i",
           (u32)(TitleID >> 32), (u32)TitleID, num_contents);

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitleContents(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2,
                   "IOCTL_ES_GETTITLECONTENTS bad in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1,
                   "IOCTL_ES_GETTITLECONTENTS bad out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const DiscIO::CNANDContentLoader& rNANDContent = AccessContentDevice(TitleID);
  if (!rNANDContent.IsValid())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  for (const auto& content : rNANDContent.GetTMD().GetContents())
  {
    const u16 index = content.index;
    Memory::Write_U32(content.id, request.io_vectors[0].address + index * 4);
    INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLECONTENTS: Index %d: %08x", index, content.id);
  }

  return GetDefaultReply(IPC_SUCCESS);
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
  _dbg_assert_(IOS_ES, request.in_vectors.size() == 1);
  _dbg_assert_(IOS_ES, request.io_vectors.size() == 0);

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

  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  size_t view_count = 0;
  if (TitleID >> 32 == 0x00000001 && TitleID != TITLEID_SYSMENU)
  {
    // Fake a ticket view to make IOS reload work.
    view_count = 1;
  }
  else if (Loader.IsValid() && Loader.GetTicket().IsValid())
  {
    view_count = Loader.GetTicket().GetNumberOfTickets();
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWCNT for titleID: %08x/%08x (View Count = %zu)",
           static_cast<u32>(TitleID >> 32), static_cast<u32>(TitleID), view_count);

  Memory::Write_U32(static_cast<u32>(view_count), request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetViews(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 2, "IOCTL_ES_GETVIEWS no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETVIEWS no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 maxViews = Memory::Read_U32(request.in_vectors[1].address);

  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  if (TitleID >> 32 == 0x00000001 && TitleID != TITLEID_SYSMENU)
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
  else if (Loader.IsValid() && Loader.GetTicket().IsValid())
  {
    u32 number_of_views = std::min(maxViews, Loader.GetTicket().GetNumberOfTickets());
    for (u32 view = 0; view < number_of_views; ++view)
    {
      const std::vector<u8> ticket_view = Loader.GetTicket().GetRawTicketView(view);
      Memory::CopyToEmu(request.io_vectors[0].address + view * sizeof(IOS::ES::TicketView),
                        ticket_view.data(), ticket_view.size());
    }
  }

  INFO_LOG(IOS_ES, "IOCTL_ES_GETVIEWS for titleID: %08x/%08x (MaxViews = %i)", (u32)(TitleID >> 32),
           (u32)TitleID, maxViews);

  return GetDefaultReply(IPC_SUCCESS);
}

// TODO: rename this to GetTMDViewSize. There is only one TMD, so the name doesn't make sense.
IPCCommandResult ES::GetTMDViewCount(const IOCtlVRequest& request)
{
  _dbg_assert_msg_(IOS_ES, request.in_vectors.size() == 1, "IOCTL_ES_GETTMDVIEWCNT no in buffer");
  _dbg_assert_msg_(IOS_ES, request.io_vectors.size() == 1, "IOCTL_ES_GETTMDVIEWCNT no out buffer");

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  const DiscIO::CNANDContentLoader& Loader = AccessContentDevice(TitleID);

  u32 view_size = 0;
  if (Loader.IsValid())
    view_size = static_cast<u32>(Loader.GetTMD().GetRawView().size());
  Memory::Write_U32(view_size, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTMDVIEWCNT: title: %08x/%08x (view size %i)", (u32)(TitleID >> 32),
           (u32)TitleID, view_size);
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
    const std::vector<u8> raw_view = Loader.GetTMD().GetRawView();
    if (raw_view.size() != request.io_vectors[0].size)
      return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

    Memory::CopyToEmu(request.io_vectors[0].address, raw_view.data(), raw_view.size());
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

static bool CanDeleteTitle(u64 title_id)
{
  // IOS only allows deleting non-system titles (or a system title higher than 00000001-00000101).
  return static_cast<u32>(title_id >> 32) != 0x00000001 || static_cast<u32>(title_id) > 0x101;
}

IPCCommandResult ES::DeleteTitle(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);

  if (!CanDeleteTitle(title_id))
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

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
  u32 tmd_size = 0;
  if (Loader.IsValid())
    tmd_size = static_cast<u32>(Loader.GetTMD().GetRawTMD().size());

  if (request.io_vectors.size())
    Memory::Write_U32(tmd_size, request.io_vectors[0].address);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETSTOREDTMDSIZE: title: %08x/%08x (view size %i)",
           (u32)(TitleID >> 32), (u32)TitleID, tmd_size);

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
    const std::vector<u8> raw_tmd = Loader.GetTMD().GetRawTMD();
    if (raw_tmd.size() != request.io_vectors[0].size)
      return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

    Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());
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

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  u32 view = Memory::Read_U32(request.in_vectors[1].address);
  u64 ticketid = Memory::Read_U64(request.in_vectors[1].address + 4);
  u32 devicetype = Memory::Read_U32(request.in_vectors[1].address + 12);
  u64 titleid = Memory::Read_U64(request.in_vectors[1].address + 16);
  u16 access = Memory::Read_U16(request.in_vectors[1].address + 24);

  INFO_LOG(IOS_ES, "IOCTL_ES_LAUNCH %016" PRIx64 " %08x %016" PRIx64 " %08x %016" PRIx64 " %04x",
           TitleID, view, ticketid, devicetype, titleid, access);

  // IOS replies to the request through the mailbox on failure, and acks if the launch succeeds.
  // Note: Launch will potentially reset the whole IOS state -- including this ES instance.
  if (!LaunchTitle(TitleID))
    return GetDefaultReply(ES_INVALID_TMD);

  // Generate a "reply" to the IPC command.  ES_LAUNCH is unique because it
  // involves restarting IOS; IOS generates two acknowledgements in a row.
  // Note: If the launch succeeded, we should not write anything to the command buffer as
  // IOS does not even reply unless it failed.
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

  if (!LaunchTitle(0x0000000100000100))
    return GetDefaultReply(ES_INVALID_TMD);

  EnqueueCommandAcknowledgement(request.address, 0);
  return GetNoReply();
}

IPCCommandResult ES::ExportTitleInit(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  // No concurrent title import/export is allowed.
  if (m_export_title_context.valid)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const auto& content_loader = AccessContentDevice(Memory::Read_U64(request.in_vectors[0].address));
  if (!content_loader.IsValid())
    return GetDefaultReply(FS_ENOENT);
  if (!content_loader.GetTMD().IsValid())
    return GetDefaultReply(ES_INVALID_TMD);

  m_export_title_context.tmd = content_loader.GetTMD();

  const auto ticket = DiscIO::FindSignedTicket(m_export_title_context.tmd.GetTitleId());
  if (!ticket.IsValid())
    return GetDefaultReply(ES_NO_TICKET_INSTALLED);
  if (ticket.GetTitleId() != m_export_title_context.tmd.GetTitleId())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  m_export_title_context.title_key = ticket.GetTitleKey();

  const auto& raw_tmd = m_export_title_context.tmd.GetRawTMD();
  if (request.io_vectors[0].size != raw_tmd.size())
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  Memory::CopyToEmu(request.io_vectors[0].address, raw_tmd.data(), raw_tmd.size());

  m_export_title_context.valid = true;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ExportContentBegin(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0) || request.in_vectors[0].size != 8 ||
      request.in_vectors[1].size != 4)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);
  const u32 content_id = Memory::Read_U32(request.in_vectors[1].address);

  if (!m_export_title_context.valid || m_export_title_context.tmd.GetTitleId() != title_id)
  {
    ERROR_LOG(IOS_ES, "Tried to use ExportContentBegin with an invalid title export context.");
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
  }

  const auto& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
    return GetDefaultReply(FS_ENOENT);

  const auto* content = content_loader.GetContentByID(content_id);
  if (!content)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

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
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
  }

  const u32 content_id = Memory::Read_U32(request.in_vectors[0].address);
  const u32 bytes_to_read = request.io_vectors[0].size;

  const auto iterator = m_export_title_context.contents.find(content_id);
  if (!m_export_title_context.valid || iterator == m_export_title_context.contents.end() ||
      iterator->second.content.m_position >= iterator->second.content.m_content.size)
  {
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
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
    ERROR_LOG(IOS_ES, "ExportContentData: ES_READ_LESS_DATA_THAN_EXPECTED");
    return GetDefaultReply(ES_READ_LESS_DATA_THAN_EXPECTED);
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
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
  }

  Memory::CopyToEmu(request.io_vectors[0].address, output.data(), output.size());
  metadata.m_position += length;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::ExportContentEnd(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 4)
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  const u32 content_id = Memory::Read_U32(request.in_vectors[0].address);

  const auto iterator = m_export_title_context.contents.find(content_id);
  if (!m_export_title_context.valid || iterator == m_export_title_context.contents.end() ||
      iterator->second.content.m_position != iterator->second.content.m_content.size)
  {
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);
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
    return GetDefaultReply(ES_PARAMETER_SIZE_OR_ALIGNMENT);

  m_export_title_context.valid = false;
  return GetDefaultReply(IPC_SUCCESS);
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

const DiscIO::CNANDContentLoader& ES::AccessContentDevice(u64 title_id) const
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

u32 ES::ES_DIVerify(const IOS::ES::TMDReader& tmd)
{
  if (!tmd.IsValid())
    return -1;

  u64 title_id = 0xDEADBEEFDEADBEEFull;
  u64 tmd_title_id = tmd.GetTitleId();

  DVDInterface::GetVolume().GetTitleID(&title_id);
  if (title_id != tmd_title_id)
    return -1;

  std::string tmd_path = Common::GetTMDFileName(tmd_title_id, Common::FROM_SESSION_ROOT);

  File::CreateFullPath(tmd_path);
  File::CreateFullPath(Common::GetTitleDataPath(tmd_title_id, Common::FROM_SESSION_ROOT));

  if (!File::Exists(tmd_path))
  {
    File::IOFile tmd_file(tmd_path, "wb");
    const std::vector<u8>& tmd_bytes = tmd.GetRawTMD();
    if (!tmd_file.WriteBytes(tmd_bytes.data(), tmd_bytes.size()))
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
