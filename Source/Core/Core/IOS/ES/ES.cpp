// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <cinttypes>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
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
// Shared across all ES instances.
static std::string s_content_file;
static TitleContext s_title_context;

// Title to launch after IOS has been reset and reloaded (similar to /sys/launch.sys).
static u64 s_title_to_launch;

ES::ES(u32 device_id, const std::string& device_name) : Device(device_id, device_name)
{
}

static void FinishAllStaleImports()
{
  const std::vector<u64> titles = IOS::ES::GetTitleImports();
  for (const u64& title_id : titles)
  {
    const IOS::ES::TMDReader tmd = IOS::ES::FindImportTMD(title_id);
    if (!tmd.IsValid())
    {
      File::DeleteDirRecursively(Common::GetImportTitlePath(title_id) + "/content");
      continue;
    }

    FinishImport(tmd);
  }

  const std::string import_dir = Common::RootUserPath(Common::FROM_SESSION_ROOT) + "/import";
  File::DeleteDirRecursively(import_dir);
  File::CreateDir(import_dir);
}

void ES::Init()
{
  FinishAllStaleImports();

  s_content_file = "";
  s_title_context = TitleContext{};

  if (s_title_to_launch != 0)
  {
    NOTICE_LOG(IOS, "Re-launching title after IOS reload.");
    LaunchTitle(s_title_to_launch, true);
    s_title_to_launch = 0;
  }
}

TitleContext& ES::GetTitleContext()
{
  return s_title_context;
}

void TitleContext::Clear()
{
  ticket.SetBytes({});
  tmd.SetBytes({});
  active = false;
}

void TitleContext::DoState(PointerWrap& p)
{
  ticket.DoState(p);
  tmd.DoState(p);
  p.Do(active);
}

void TitleContext::Update(const DiscIO::CNANDContentLoader& content_loader)
{
  if (!content_loader.IsValid())
    return;
  Update(content_loader.GetTMD(), content_loader.GetTicket());
}

void TitleContext::Update(const IOS::ES::TMDReader& tmd_, const IOS::ES::TicketReader& ticket_)
{
  if (!tmd_.IsValid() || !ticket_.IsValid())
  {
    ERROR_LOG(IOS_ES, "TMD or ticket is not valid -- refusing to update title context");
    return;
  }

  ticket = ticket_;
  tmd = tmd_;
  active = true;

  // Interesting title changes (channel or disc game launch) always happen after an IOS reload.
  if (first_change)
  {
    SConfig::GetInstance().SetRunningGameMetadata(tmd);
    first_change = false;
  }
}

void ES::LoadWAD(const std::string& _rContentFile)
{
  s_content_file = _rContentFile;
  // XXX: Ideally, this should be done during a launch, but because we support launching WADs
  // without installing them (which is a bit of a hack), we have to do this manually here.
  const auto& content_loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(s_content_file);
  s_title_context.Update(content_loader);
  INFO_LOG(IOS_ES, "LoadWAD: Title context changed: %016" PRIx64, s_title_context.tmd.GetTitleId());
}

IPCCommandResult ES::GetTitleDirectory(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);

  char* Path = (char*)Memory::GetPointer(request.io_vectors[0].address);
  sprintf(Path, "/title/%08x/%08x/data", (u32)(TitleID >> 32), (u32)TitleID);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLEDIR: %s", Path);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetTitleID(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
    return GetDefaultReply(ES_EINVAL);

  if (!s_title_context.active)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = s_title_context.tmd.GetTitleId();
  Memory::Write_U64(title_id, request.io_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLEID: %08x/%08x", static_cast<u32>(title_id >> 32),
           static_cast<u32>(title_id));
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::SetUID(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0))
    return GetDefaultReply(ES_EINVAL);

  // TODO: fs permissions based on this
  u64 TitleID = Memory::Read_U64(request.in_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_SETUID titleID: %08x/%08x", (u32)(TitleID >> 32), (u32)TitleID);
  return GetDefaultReply(IPC_SUCCESS);
}

bool ES::LaunchTitle(u64 title_id, bool skip_reload)
{
  s_title_context.Clear();
  INFO_LOG(IOS_ES, "ES_Launch: Title context changed: (none)");

  NOTICE_LOG(IOS_ES, "Launching title %016" PRIx64 "...", title_id);

  // ES_Launch should probably reset the whole state, which at least means closing all open files.
  // leaving them open through ES_Launch may cause hangs and other funky behavior
  // (supposedly when trying to re-open those files).
  DiscIO::CNANDContentManager::Access().ClearCache();

  if (IsTitleType(title_id, IOS::ES::TitleType::System) && title_id != TITLEID_SYSMENU)
    return LaunchIOS(title_id);
  return LaunchPPCTitle(title_id, skip_reload);
}

bool ES::LaunchIOS(u64 ios_title_id)
{
  return Reload(ios_title_id);
}

bool ES::LaunchPPCTitle(u64 title_id, bool skip_reload)
{
  const DiscIO::CNANDContentLoader& content_loader = AccessContentDevice(title_id);
  if (!content_loader.IsValid())
  {
    if (title_id == 0x0000000100000002)
    {
      PanicAlertT("Could not launch the Wii Menu because it is missing from the NAND.\n"
                  "The emulated software will likely hang now.");
    }
    else
    {
      PanicAlertT("Could not launch title %016" PRIx64 " because it is missing from the NAND.\n"
                  "The emulated software will likely hang now.",
                  title_id);
    }
    return false;
  }

  if (!content_loader.GetTMD().IsValid() || !content_loader.GetTicket().IsValid())
    return false;

  // Before launching a title, IOS first reads the TMD and reloads into the specified IOS version,
  // even when that version is already running. After it has reloaded, ES_Launch will be called
  // again with the reload skipped, and the PPC will be bootstrapped then.
  if (!skip_reload)
  {
    s_title_to_launch = title_id;
    const u64 required_ios = content_loader.GetTMD().GetIOSId();
    return LaunchTitle(required_ios);
  }

  s_title_context.Update(content_loader);
  INFO_LOG(IOS_ES, "LaunchPPCTitle: Title context changed: %016" PRIx64,
           s_title_context.tmd.GetTitleId());
  return BootstrapPPC(content_loader);
}

void ES::DoState(PointerWrap& p)
{
  Device::DoState(p);
  p.Do(s_content_file);
  p.Do(m_AccessIdentID);
  s_title_context.DoState(p);

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
  if (m_is_active)
    INFO_LOG(IOS_ES, "Device was re-opened.");
  return Device::Open(request);
}

void ES::Close()
{
  // XXX: does IOS really clear the content access map here?
  m_ContentAccessMap.clear();
  m_AccessIdentID = 0;

  INFO_LOG(IOS_ES, "ES: Close");
  m_is_active = false;
  // clear the NAND content cache to make sure nothing remains open.
  DiscIO::CNANDContentManager::Access().ClearCache();
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
  case IOCTL_ES_ADDTITLECANCEL:
    return AddTitleCancel(request);
  case IOCTL_ES_GETDEVICEID:
    return GetConsoleID(request);
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

  case IOCTL_ES_GETOWNEDTITLECNT:
    return GetOwnedTitleCount(request);
  case IOCTL_ES_GETOWNEDTITLES:
    return GetOwnedTitles(request);
  case IOCTL_ES_GETTITLECNT:
    return GetTitleCount(request);
  case IOCTL_ES_GETTITLES:
    return GetTitles(request);

  case IOCTL_ES_GETTITLECONTENTSCNT:
    return GetStoredContentsCount(request);
  case IOCTL_ES_GETTITLECONTENTS:
    return GetStoredContents(request);
  case IOCTL_ES_GETSTOREDCONTENTCNT:
    return GetTMDStoredContentsCount(request);
  case IOCTL_ES_GETSTOREDCONTENTS:
    return GetTMDStoredContents(request);

  case IOCTL_ES_GETVIEWCNT:
    return GetTicketViewCount(request);
  case IOCTL_ES_GETVIEWS:
    return GetTicketViews(request);
  case IOCTL_ES_DIGETTICKETVIEW:
    return DIGetTicketView(request);

  case IOCTL_ES_GETTMDVIEWCNT:
    return GetTMDViewSize(request);
  case IOCTL_ES_GETTMDVIEWS:
    return GetTMDViews(request);

  case IOCTL_ES_DIGETTMDVIEWSIZE:
    return DIGetTMDViewSize(request);
  case IOCTL_ES_DIGETTMDVIEW:
    return DIGetTMDView(request);

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
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_ES);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::GetConsumption(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 2))
    return GetDefaultReply(ES_EINVAL);

  // This is at least what crediar's ES module does
  Memory::Write_U32(0, request.io_vectors[1].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_GETCONSUMPTION");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Launch(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return GetDefaultReply(ES_EINVAL);

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
    return GetDefaultReply(FS_ENOENT);

  // Generate a "reply" to the IPC command.  ES_LAUNCH is unique because it
  // involves restarting IOS; IOS generates two acknowledgements in a row.
  // Note: If the launch succeeded, we should not write anything to the command buffer as
  // IOS does not even reply unless it failed.
  EnqueueCommandAcknowledgement(request.address, 0);
  return GetNoReply();
}

IPCCommandResult ES::LaunchBC(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return GetDefaultReply(ES_EINVAL);

  // Here, IOS checks the clock speed and prevents ioctlv 0x25 from being used in GC mode.
  // An alternative way to do this is to check whether the current active IOS is MIOS.
  if (GetVersion() == 0x101)
    return GetDefaultReply(ES_EINVAL);

  if (!LaunchTitle(0x0000000100000100))
    return GetDefaultReply(FS_ENOENT);

  EnqueueCommandAcknowledgement(request.address, 0);
  return GetNoReply();
}

const DiscIO::CNANDContentLoader& ES::AccessContentDevice(u64 title_id)
{
  // for WADs, the passed title id and the stored title id match; along with s_content_file
  // being set to the actual WAD file name. We cannot simply get a NAND Loader for the title id
  // in those cases, since the WAD need not be installed in the NAND, but it could be opened
  // directly from a WAD file anywhere on disk.
  if (s_title_context.active && s_title_context.tmd.GetTitleId() == title_id &&
      !s_content_file.empty())
  {
    return DiscIO::CNANDContentManager::Access().GetNANDLoader(s_content_file);
  }

  return DiscIO::CNANDContentManager::Access().GetNANDLoader(title_id, Common::FROM_SESSION_ROOT);
}

// This is technically an ioctlv in IOS's ES, but it is an internal API which cannot be
// used from the PowerPC (for unpatched IOSes anyway).
s32 ES::DIVerify(const IOS::ES::TMDReader& tmd, const IOS::ES::TicketReader& ticket)
{
  s_title_context.Clear();
  INFO_LOG(IOS_ES, "ES_DIVerify: Title context changed: (none)");

  if (!tmd.IsValid() || !ticket.IsValid())
    return ES_EINVAL;

  if (tmd.GetTitleId() != ticket.GetTitleId())
    return ES_EINVAL;

  std::string tmd_path = Common::GetTMDFileName(tmd.GetTitleId(), Common::FROM_SESSION_ROOT);

  File::CreateFullPath(tmd_path);
  File::CreateFullPath(Common::GetTitleDataPath(tmd.GetTitleId(), Common::FROM_SESSION_ROOT));

  if (!File::Exists(tmd_path))
  {
    File::IOFile tmd_file(tmd_path, "wb");
    const std::vector<u8>& tmd_bytes = tmd.GetRawTMD();
    if (!tmd_file.WriteBytes(tmd_bytes.data(), tmd_bytes.size()))
      ERROR_LOG(IOS_ES, "DIVerify failed to write disc TMD to NAND.");
  }
  IOS::ES::UIDSys uid_sys{Common::FromWhichRoot::FROM_SESSION_ROOT};
  uid_sys.AddTitle(tmd.GetTitleId());
  // DI_VERIFY writes to title.tmd, which is read and cached inside the NAND Content Manager.
  // clear the cache to avoid content access mismatches.
  DiscIO::CNANDContentManager::Access().ClearCache();

  s_title_context.Update(tmd, ticket);
  INFO_LOG(IOS_ES, "ES_DIVerify: Title context changed: %016" PRIx64, tmd.GetTitleId());
  return IPC_SUCCESS;
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
