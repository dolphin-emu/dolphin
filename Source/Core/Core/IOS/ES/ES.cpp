// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <utility>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Core/CommonTitles.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"
#include "Core/IOS/VersionInfo.h"
#include "DiscIO/Enums.h"

namespace IOS::HLE::Device
{
// Title to launch after IOS has been reset and reloaded (similar to /sys/launch.sys).
static u64 s_title_to_launch;

struct DirectoryToCreate
{
  const char* path;
  FS::FileAttribute attribute;
  FS::Modes modes;
  FS::Uid uid = PID_KERNEL;
  FS::Gid gid = PID_KERNEL;
};

constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
constexpr std::array<DirectoryToCreate, 9> s_directories_to_create = {{
    {"/sys", 0, {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None}},
    {"/ticket", 0, {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None}},
    {"/title", 0, {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::Read}},
    {"/shared1", 0, {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None}},
    {"/shared2", 0, public_modes},
    {"/tmp", 0, public_modes},
    {"/import", 0, {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None}},
    {"/meta", 0, public_modes, SYSMENU_UID, SYSMENU_GID},
    {"/wfs", 0, {FS::Mode::ReadWrite, FS::Mode::None, FS::Mode::None}, PID_UNKNOWN, PID_UNKNOWN},
}};

ES::ES(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
  for (const auto& directory : s_directories_to_create)
  {
    // Note: ES sets its own UID and GID to 0/0 at boot, so all filesystem accesses in ES are done
    // as UID 0 even though its PID is 1.
    const auto result = m_ios.GetFS()->CreateDirectory(PID_KERNEL, PID_KERNEL, directory.path,
                                                       directory.attribute, directory.modes);
    if (result != FS::ResultCode::Success && result != FS::ResultCode::AlreadyExists)
      ERROR_LOG(IOS_ES, "Failed to create %s: error %d", directory.path, FS::ConvertResult(result));

    // Now update the UID/GID and other attributes.
    m_ios.GetFS()->SetMetadata(0, directory.path, directory.uid, directory.gid, directory.attribute,
                               directory.modes);
  }

  FinishAllStaleImports();

  if (s_title_to_launch != 0)
  {
    NOTICE_LOG(IOS, "Re-launching title after IOS reload.");
    LaunchTitle(s_title_to_launch, true);
    s_title_to_launch = 0;
  }
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

void TitleContext::Update(const IOS::ES::TMDReader& tmd_, const IOS::ES::TicketReader& ticket_,
                          DiscIO::Platform platform)
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
    SConfig::GetInstance().SetRunningGameMetadata(tmd, platform);
    first_change = false;
  }
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

ReturnCode ES::GetTitleId(u64* title_id) const
{
  if (!m_title_context.active)
    return ES_EINVAL;
  *title_id = m_title_context.tmd.GetTitleId();
  return IPC_SUCCESS;
}

IPCCommandResult ES::GetTitleId(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
    return GetDefaultReply(ES_EINVAL);

  u64 title_id;
  const ReturnCode ret = GetTitleId(&title_id);
  if (ret != IPC_SUCCESS)
    return GetDefaultReply(ret);

  Memory::Write_U64(title_id, request.io_vectors[0].address);
  INFO_LOG(IOS_ES, "IOCTL_ES_GETTITLEID: %08x/%08x", static_cast<u32>(title_id >> 32),
           static_cast<u32>(title_id));
  return GetDefaultReply(IPC_SUCCESS);
}

static bool UpdateUIDAndGID(Kernel& kernel, const IOS::ES::TMDReader& tmd)
{
  IOS::ES::UIDSys uid_sys{kernel.GetFS()};
  const u64 title_id = tmd.GetTitleId();
  const u32 uid = uid_sys.GetOrInsertUIDForTitle(title_id);
  if (!uid)
  {
    ERROR_LOG(IOS_ES, "Failed to get UID for title %016" PRIx64, title_id);
    return false;
  }
  kernel.SetUidForPPC(uid);
  kernel.SetGidForPPC(tmd.GetGroupId());
  return true;
}

static ReturnCode CheckIsAllowedToSetUID(Kernel& kernel, const u32 caller_uid)
{
  IOS::ES::UIDSys uid_map{kernel.GetFS()};
  const u32 system_menu_uid = uid_map.GetOrInsertUIDForTitle(Titles::SYSTEM_MENU);
  if (!system_menu_uid)
    return ES_SHORT_READ;
  return caller_uid == system_menu_uid ? IPC_SUCCESS : ES_EINVAL;
}

IPCCommandResult ES::SetUID(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 8)
    return GetDefaultReply(ES_EINVAL);

  const u64 title_id = Memory::Read_U64(request.in_vectors[0].address);

  const s32 ret = CheckIsAllowedToSetUID(m_ios, uid);
  if (ret < 0)
  {
    ERROR_LOG(IOS_ES, "SetUID: Permission check failed with error %d", ret);
    return GetDefaultReply(ret);
  }

  const auto tmd = FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return GetDefaultReply(FS_ENOENT);

  if (!UpdateUIDAndGID(m_ios, tmd))
  {
    ERROR_LOG(IOS_ES, "SetUID: Failed to get UID for title %016" PRIx64, title_id);
    return GetDefaultReply(ES_SHORT_READ);
  }

  return GetDefaultReply(IPC_SUCCESS);
}

bool ES::LaunchTitle(u64 title_id, bool skip_reload)
{
  m_title_context.Clear();
  INFO_LOG(IOS_ES, "ES_Launch: Title context changed: (none)");

  NOTICE_LOG(IOS_ES, "Launching title %016" PRIx64 "...", title_id);

  if ((title_id == Titles::SHOP || title_id == Titles::KOREAN_SHOP) &&
      m_ios.GetIOSC().IsUsingDefaultId())
  {
    ERROR_LOG(IOS_ES, "Refusing to launch the shop channel with default device credentials");
    CriticalAlertT("You cannot use the Wii Shop Channel without using your own device credentials."
                   "\nPlease refer to the NAND usage guide for setup instructions: "
                   "https://dolphin-emu.org/docs/guides/nand-usage-guide/");

    // Send the user back to the system menu instead of returning an error, which would
    // likely make the system menu crash. Doing this is okay as anyone who has the shop
    // also has the system menu installed, and this behaviour is consistent with what
    // ES does when its DRM system refuses the use of a particular title.
    return LaunchTitle(Titles::SYSTEM_MENU);
  }

  if (IsTitleType(title_id, IOS::ES::TitleType::System) && title_id != Titles::SYSTEM_MENU)
    return LaunchIOS(title_id);
  return LaunchPPCTitle(title_id, skip_reload);
}

bool ES::LaunchIOS(u64 ios_title_id)
{
  // A real Wii goes through several steps before getting to MIOS.
  //
  // * The System Menu detects a GameCube disc and launches BC (1-100) instead of the game.
  // * BC (similar to boot1) lowers the clock speed to the Flipper's and then launches boot2.
  // * boot2 sees the lowered clock speed and launches MIOS (1-101) instead of the System Menu.
  //
  // Because we don't have boot1 and boot2, and BC is only ever used to launch MIOS
  // (indirectly via boot2), we can just launch MIOS when BC is launched.
  if (ios_title_id == Titles::BC)
  {
    NOTICE_LOG(IOS, "BC: Launching MIOS...");
    return LaunchIOS(Titles::MIOS);
  }

  // IOS checks whether the system title is installed and returns an error if it isn't.
  // Unfortunately, we can't rely on titles being installed as we don't require system titles,
  // so only have this check for MIOS (for which having the binary is *required*).
  if (ios_title_id == Titles::MIOS)
  {
    const IOS::ES::TMDReader tmd = FindInstalledTMD(ios_title_id);
    const IOS::ES::TicketReader ticket = FindSignedTicket(ios_title_id);
    IOS::ES::Content content;
    if (!tmd.IsValid() || !ticket.IsValid() || !tmd.GetContent(tmd.GetBootIndex(), &content) ||
        !m_ios.BootIOS(ios_title_id, GetContentPath(ios_title_id, content)))
    {
      PanicAlertT("Could not launch IOS %016" PRIx64 " because it is missing from the NAND.\n"
                  "The emulated software will likely hang now.",
                  ios_title_id);
      return false;
    }
    return true;
  }

  return m_ios.BootIOS(ios_title_id);
}

bool ES::LaunchPPCTitle(u64 title_id, bool skip_reload)
{
  const IOS::ES::TMDReader tmd = FindInstalledTMD(title_id);
  const IOS::ES::TicketReader ticket = FindSignedTicket(title_id);

  if (!tmd.IsValid() || !ticket.IsValid())
  {
    if (title_id == Titles::SYSTEM_MENU)
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

  // Before launching a title, IOS first reads the TMD and reloads into the specified IOS version,
  // even when that version is already running. After it has reloaded, ES_Launch will be called
  // again with the reload skipped, and the PPC will be bootstrapped then.
  if (!skip_reload)
  {
    s_title_to_launch = title_id;
    const u64 required_ios = tmd.GetIOSId();
    return LaunchTitle(required_ios);
  }

  m_title_context.Update(tmd, ticket, DiscIO::Platform::WiiWAD);
  INFO_LOG(IOS_ES, "LaunchPPCTitle: Title context changed: %016" PRIx64, tmd.GetTitleId());

  // Note: the UID/GID is also updated for IOS titles, but since we have no guarantee IOS titles
  // are installed, we can only do this for PPC titles.
  if (!UpdateUIDAndGID(m_ios, m_title_context.tmd))
  {
    m_title_context.Clear();
    INFO_LOG(IOS_ES, "LaunchPPCTitle: Title context changed: (none)");
    return false;
  }

  IOS::ES::Content content;
  if (!tmd.GetContent(tmd.GetBootIndex(), &content))
    return false;

  return m_ios.BootstrapPPC(GetContentPath(tmd.GetTitleId(), content));
}

void ES::Context::DoState(PointerWrap& p)
{
  p.Do(uid);
  p.Do(gid);
  title_import_export.DoState(p);

  p.Do(active);
  p.Do(ipc_fd);
}

void ES::DoState(PointerWrap& p)
{
  Device::DoState(p);

  for (auto& entry : m_content_table)
  {
    p.Do(entry.m_opened);
    p.Do(entry.m_title_id);
    p.Do(entry.m_content);
    p.Do(entry.m_fd);
    p.Do(entry.m_uid);
  }

  m_title_context.DoState(p);

  for (auto& context : m_contexts)
    context.DoState(p);
}

ES::ContextArray::iterator ES::FindActiveContext(s32 fd)
{
  return std::find_if(m_contexts.begin(), m_contexts.end(),
                      [fd](const auto& context) { return context.ipc_fd == fd && context.active; });
}

ES::ContextArray::iterator ES::FindInactiveContext()
{
  return std::find_if(m_contexts.begin(), m_contexts.end(),
                      [](const auto& context) { return !context.active; });
}

IPCCommandResult ES::Open(const OpenRequest& request)
{
  auto context = FindInactiveContext();
  if (context == m_contexts.end())
    return GetDefaultReply(ES_FD_EXHAUSTED);

  context->active = true;
  context->uid = request.uid;
  context->gid = request.gid;
  context->ipc_fd = request.fd;
  return Device::Open(request);
}

IPCCommandResult ES::Close(u32 fd)
{
  auto context = FindActiveContext(fd);
  if (context == m_contexts.end())
    return GetDefaultReply(ES_EINVAL);

  context->active = false;
  context->ipc_fd = -1;

  INFO_LOG(IOS_ES, "ES: Close");
  m_is_active = false;
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::IOCtlV(const IOCtlVRequest& request)
{
  DEBUG_LOG(IOS_ES, "%s (0x%x)", GetDeviceName().c_str(), request.request);
  auto context = FindActiveContext(request.fd);
  if (context == m_contexts.end())
    return GetDefaultReply(ES_EINVAL);

  switch (request.request)
  {
  case IOCTL_ES_ADDTICKET:
    return ImportTicket(request);
  case IOCTL_ES_ADDTMD:
    return ImportTmd(*context, request);
  case IOCTL_ES_ADDTITLESTART:
    return ImportTitleInit(*context, request);
  case IOCTL_ES_ADDCONTENTSTART:
    return ImportContentBegin(*context, request);
  case IOCTL_ES_ADDCONTENTDATA:
    return ImportContentData(*context, request);
  case IOCTL_ES_ADDCONTENTFINISH:
    return ImportContentEnd(*context, request);
  case IOCTL_ES_ADDTITLEFINISH:
    return ImportTitleDone(*context, request);
  case IOCTL_ES_ADDTITLECANCEL:
    return ImportTitleCancel(*context, request);
  case IOCTL_ES_GETDEVICEID:
    return GetDeviceId(request);

  case IOCTL_ES_OPENCONTENT:
    return OpenContent(context->uid, request);
  case IOCTL_ES_OPEN_ACTIVE_TITLE_CONTENT:
    return OpenActiveTitleContent(context->uid, request);
  case IOCTL_ES_READCONTENT:
    return ReadContent(context->uid, request);
  case IOCTL_ES_CLOSECONTENT:
    return CloseContent(context->uid, request);
  case IOCTL_ES_SEEKCONTENT:
    return SeekContent(context->uid, request);

  case IOCTL_ES_GETTITLEDIR:
    return GetTitleDirectory(request);
  case IOCTL_ES_GETTITLEID:
    return GetTitleId(request);
  case IOCTL_ES_SETUID:
    return SetUID(context->uid, request);
  case IOCTL_ES_DIVERIFY:
  case IOCTL_ES_DIVERIFY_WITH_VIEW:
    return DIVerify(request);

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

  case IOCTL_ES_GETSHAREDCONTENTCNT:
    return GetSharedContentsCount(request);
  case IOCTL_ES_GETSHAREDCONTENTS:
    return GetSharedContents(request);

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
  case IOCTL_ES_DIGETTMDSIZE:
    return DIGetTMDSize(request);
  case IOCTL_ES_DIGETTMD:
    return DIGetTMD(request);

  case IOCTL_ES_GETCONSUMPTION:
    return GetConsumption(request);
  case IOCTL_ES_DELETETITLE:
    return DeleteTitle(request);
  case IOCTL_ES_DELETETICKET:
    return DeleteTicket(request);
  case IOCTL_ES_DELETETITLECONTENT:
    return DeleteTitleContent(request);
  case IOCTL_ES_DELETESHAREDCONTENT:
    return DeleteSharedContent(request);
  case IOCTL_ES_DELETE_CONTENT:
    return DeleteContent(request);
  case IOCTL_ES_GETSTOREDTMDSIZE:
    return GetStoredTMDSize(request);
  case IOCTL_ES_GETSTOREDTMD:
    return GetStoredTMD(request);
  case IOCTL_ES_ENCRYPT:
    return Encrypt(context->uid, request);
  case IOCTL_ES_DECRYPT:
    return Decrypt(context->uid, request);
  case IOCTL_ES_LAUNCH:
    return Launch(request);
  case IOCTL_ES_LAUNCHBC:
    return LaunchBC(request);
  case IOCTL_ES_EXPORTTITLEINIT:
    return ExportTitleInit(*context, request);
  case IOCTL_ES_EXPORTCONTENTBEGIN:
    return ExportContentBegin(*context, request);
  case IOCTL_ES_EXPORTCONTENTDATA:
    return ExportContentData(*context, request);
  case IOCTL_ES_EXPORTCONTENTEND:
    return ExportContentEnd(*context, request);
  case IOCTL_ES_EXPORTTITLEDONE:
    return ExportTitleDone(*context, request);
  case IOCTL_ES_CHECKKOREAREGION:
    return CheckKoreaRegion(request);
  case IOCTL_ES_GETDEVICECERT:
    return GetDeviceCertificate(request);
  case IOCTL_ES_SIGN:
    return Sign(request);
  case IOCTL_ES_VERIFYSIGN:
    return VerifySign(request);
  case IOCTL_ES_GETBOOT2VERSION:
    return GetBoot2Version(request);

  case IOCTL_ES_GET_V0_TICKET_FROM_VIEW:
    return GetV0TicketFromView(request);
  case IOCTL_ES_GET_TICKET_SIZE_FROM_VIEW:
    return GetTicketSizeFromView(request);
  case IOCTL_ES_GET_TICKET_FROM_VIEW:
    return GetTicketFromView(request);

  case IOCTL_ES_SET_UP_STREAM_KEY:
    return SetUpStreamKey(*context, request);
  case IOCTL_ES_DELETE_STREAM_KEY:
    return DeleteStreamKey(request);

  case IOCTL_ES_UNKNOWN_41:
  case IOCTL_ES_UNKNOWN_42:
    PanicAlert("IOS-ES: Unimplemented ioctlv 0x%x (%zu in vectors, %zu io vectors)",
               request.request, request.in_vectors.size(), request.io_vectors.size());
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_ES, LogTypes::LERROR);
    return GetDefaultReply(IPC_EINVAL);

  case IOCTL_ES_INVALID_3F:
  default:
    return GetDefaultReply(IPC_EINVAL);
  }
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

  // Prevent loading installed IOSes that are not emulated.
  if (!IOS::HLE::IsEmulated(TitleID))
    return GetDefaultReply(FS_ENOENT);

  // IOS replies to the request through the mailbox on failure, and acks if the launch succeeds.
  // Note: Launch will potentially reset the whole IOS state -- including this ES instance.
  if (!LaunchTitle(TitleID))
    return GetDefaultReply(FS_ENOENT);

  // ES_LAUNCH involves restarting IOS, which results in two acknowledgements in a row
  // (one from the previous IOS for this IPC request, and one from the new one as it boots).
  // Nothing should be written to the command buffer if the launch succeeded for obvious reasons.
  return GetNoReply();
}

IPCCommandResult ES::LaunchBC(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return GetDefaultReply(ES_EINVAL);

  // Here, IOS checks the clock speed and prevents ioctlv 0x25 from being used in GC mode.
  // An alternative way to do this is to check whether the current active IOS is MIOS.
  if (m_ios.GetVersion() == 0x101)
    return GetDefaultReply(ES_EINVAL);

  if (!LaunchTitle(0x0000000100000100))
    return GetDefaultReply(FS_ENOENT);

  return GetNoReply();
}

// This is technically an ioctlv in IOS's ES, but it is an internal API which cannot be
// used from the PowerPC (for unpatched and up-to-date IOSes anyway).
// So we block access to it from the IPC interface.
IPCCommandResult ES::DIVerify(const IOCtlVRequest& request)
{
  return GetDefaultReply(ES_EINVAL);
}

static ReturnCode WriteTmdForDiVerify(FS::FileSystem* fs, const IOS::ES::TMDReader& tmd)
{
  const std::string temp_path = "/tmp/title.tmd";
  fs->Delete(PID_KERNEL, PID_KERNEL, temp_path);
  constexpr FS::Modes internal_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::None};
  {
    const auto file = fs->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, temp_path, internal_modes);
    if (!file)
      return FS::ConvertResult(file.Error());
    if (!file->Write(tmd.GetBytes().data(), tmd.GetBytes().size()))
      return ES_EIO;
  }

  const std::string tmd_dir = Common::GetTitleContentPath(tmd.GetTitleId());
  const std::string tmd_path = Common::GetTMDFileName(tmd.GetTitleId());
  constexpr FS::Modes parent_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::Read};
  const auto result = fs->CreateFullPath(PID_KERNEL, PID_KERNEL, tmd_path, 0, parent_modes);
  if (result != FS::ResultCode::Success)
    return FS::ConvertResult(result);

  fs->SetMetadata(PID_KERNEL, tmd_dir, PID_KERNEL, PID_KERNEL, 0, internal_modes);
  return FS::ConvertResult(fs->Rename(PID_KERNEL, PID_KERNEL, temp_path, tmd_path));
}

ReturnCode ES::DIVerify(const IOS::ES::TMDReader& tmd, const IOS::ES::TicketReader& ticket)
{
  m_title_context.Clear();
  INFO_LOG(IOS_ES, "ES_DIVerify: Title context changed: (none)");

  if (!tmd.IsValid() || !ticket.IsValid())
    return ES_EINVAL;

  if (tmd.GetTitleId() != ticket.GetTitleId())
    return ES_EINVAL;

  m_title_context.Update(tmd, ticket, DiscIO::Platform::WiiDisc);
  INFO_LOG(IOS_ES, "ES_DIVerify: Title context changed: %016" PRIx64, tmd.GetTitleId());

  // XXX: We are supposed to verify the TMD and ticket here, but cannot because
  // this may cause issues with custom/patched games.

  const auto fs = m_ios.GetFS();
  if (!FindInstalledTMD(tmd.GetTitleId()).IsValid())
  {
    if (const ReturnCode ret = WriteTmdForDiVerify(fs.get(), tmd))
    {
      ERROR_LOG(IOS_ES, "DiVerify failed to write disc TMD to NAND.");
      return ret;
    }
  }

  if (!UpdateUIDAndGID(*GetIOS(), m_title_context.tmd))
  {
    return ES_SHORT_READ;
  }

  const std::string data_dir = Common::GetTitleDataPath(tmd.GetTitleId());
  // Might already exist, so we only need to check whether the second operation succeeded.
  constexpr FS::Modes data_dir_modes{FS::Mode::ReadWrite, FS::Mode::None, FS::Mode::None};
  fs->CreateDirectory(PID_KERNEL, PID_KERNEL, data_dir, 0, data_dir_modes);
  return FS::ConvertResult(
      fs->SetMetadata(0, data_dir, m_ios.GetUidForPPC(), m_ios.GetGidForPPC(), 0, data_dir_modes));
}

ReturnCode ES::CheckStreamKeyPermissions(const u32 uid, const u8* ticket_view,
                                         const IOS::ES::TMDReader& tmd) const
{
  const u32 title_flags = tmd.GetTitleFlags();
  // Only allow using this function with some titles (WFS titles).
  // The following is the exact check from IOS. Unfortunately, other than knowing that the
  // title type is what IOS checks, we don't know much about the constants used here.
  constexpr u32 WFS_AND_0x4_FLAG = IOS::ES::TITLE_TYPE_0x4 | IOS::ES::TITLE_TYPE_WFS_MAYBE;
  if ((!(title_flags & IOS::ES::TITLE_TYPE_0x4) && ~(title_flags >> 5) & 1) ||
      (title_flags & WFS_AND_0x4_FLAG) == WFS_AND_0x4_FLAG)
  {
    return ES_EINVAL;
  }

  // This function can only be used by specific UIDs, depending on the title type.
  // It cannot be used at all internally, unless the request comes from the WFS process.
  // It can only be used from the PPC for some title types.
  // Note: PID 0x19 is used by the WFS modules.
  if (uid < FIRST_PPC_UID && uid != PID_UNKNOWN)
    return ES_EINVAL;

  // If the title type is of this specific type, then this function is limited to WFS.
  if (title_flags & IOS::ES::TITLE_TYPE_WFS_MAYBE && uid != PID_UNKNOWN)
    return ES_EINVAL;

  const u64 view_title_id = Common::swap64(ticket_view + offsetof(IOS::ES::TicketView, title_id));
  if (view_title_id != tmd.GetTitleId())
    return ES_EINVAL;

  // More permission checks.
  const u32 permitted_title_mask =
      Common::swap32(ticket_view + offsetof(IOS::ES::TicketView, permitted_title_mask));
  const u32 permitted_title_id =
      Common::swap32(ticket_view + offsetof(IOS::ES::TicketView, permitted_title_id));
  if ((uid == PID_UNKNOWN && (~permitted_title_mask & 0x13) != permitted_title_id) ||
      !IsActiveTitlePermittedByTicket(ticket_view))
  {
    return ES_EACCES;
  }

  return IPC_SUCCESS;
}

ReturnCode ES::SetUpStreamKey(const u32 uid, const u8* ticket_view, const IOS::ES::TMDReader& tmd,
                              u32* handle)
{
  ReturnCode ret = CheckStreamKeyPermissions(uid, ticket_view, tmd);
  if (ret != IPC_SUCCESS)
    return ret;

  // TODO (for the future): signature checks.

  // Find a signed ticket from the view.
  const u64 ticket_id = Common::swap64(&ticket_view[offsetof(IOS::ES::TicketView, ticket_id)]);
  const u64 title_id = Common::swap64(&ticket_view[offsetof(IOS::ES::TicketView, title_id)]);
  const IOS::ES::TicketReader installed_ticket = FindSignedTicket(title_id);
  // Unlike the other "get ticket from view" function, this returns a FS error, not ES_NO_TICKET.
  if (!installed_ticket.IsValid())
    return FS_ENOENT;
  const std::vector<u8> ticket_bytes = installed_ticket.GetRawTicket(ticket_id);
  if (ticket_bytes.empty())
    return ES_NO_TICKET;

  std::vector<u8> cert_store;
  ret = ReadCertStore(&cert_store);
  if (ret != IPC_SUCCESS)
    return ret;

  ret = VerifyContainer(VerifyContainerType::TMD, VerifyMode::UpdateCertStore, tmd, cert_store);
  if (ret != IPC_SUCCESS)
    return ret;
  ret = VerifyContainer(VerifyContainerType::Ticket, VerifyMode::UpdateCertStore, installed_ticket,
                        cert_store);
  if (ret != IPC_SUCCESS)
    return ret;

  // Create the handle and return it.
  std::array<u8, 16> iv{};
  std::memcpy(iv.data(), &title_id, sizeof(title_id));
  ret = m_ios.GetIOSC().CreateObject(handle, IOSC::ObjectType::TYPE_SECRET_KEY,
                                     IOSC::ObjectSubType::SUBTYPE_AES128, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  const u32 owner_uid = uid >= FIRST_PPC_UID ? PID_PPCBOOT : uid;
  ret = m_ios.GetIOSC().SetOwnership(*handle, 1 << owner_uid, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;

  const u8 index = ticket_bytes[offsetof(IOS::ES::Ticket, common_key_index)];
  if (index >= IOSC::COMMON_KEY_HANDLES.size())
    return ES_INVALID_TICKET;

  return m_ios.GetIOSC().ImportSecretKey(*handle, IOSC::COMMON_KEY_HANDLES[index], iv.data(),
                                         &ticket_bytes[offsetof(IOS::ES::Ticket, title_key)],
                                         PID_ES);
}

IPCCommandResult ES::SetUpStreamKey(const Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) ||
      request.in_vectors[0].size != sizeof(IOS::ES::TicketView) ||
      !IOS::ES::IsValidTMDSize(request.in_vectors[1].size) ||
      request.io_vectors[0].size != sizeof(u32))
  {
    return GetDefaultReply(ES_EINVAL);
  }

  std::vector<u8> tmd_bytes(request.in_vectors[1].size);
  Memory::CopyFromEmu(tmd_bytes.data(), request.in_vectors[1].address, tmd_bytes.size());
  const IOS::ES::TMDReader tmd{std::move(tmd_bytes)};

  if (!tmd.IsValid())
    return GetDefaultReply(ES_EINVAL);

  u32 handle;
  const ReturnCode ret =
      SetUpStreamKey(context.uid, Memory::GetPointer(request.in_vectors[0].address), tmd, &handle);
  Memory::Write_U32(handle, request.io_vectors[0].address);
  return GetDefaultReply(ret);
}

IPCCommandResult ES::DeleteStreamKey(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  const u32 handle = Memory::Read_U32(request.in_vectors[0].address);
  return GetDefaultReply(m_ios.GetIOSC().DeleteObject(handle, PID_ES));
}

bool ES::IsActiveTitlePermittedByTicket(const u8* ticket_view) const
{
  if (!m_title_context.active)
    return false;

  const u32 title_identifier = static_cast<u32>(m_title_context.tmd.GetTitleId());
  const u32 permitted_title_mask =
      Common::swap32(ticket_view + offsetof(IOS::ES::TicketView, permitted_title_mask));
  const u32 permitted_title_id =
      Common::swap32(ticket_view + offsetof(IOS::ES::TicketView, permitted_title_id));
  return title_identifier && (title_identifier & ~permitted_title_mask) == permitted_title_id;
}

bool ES::IsIssuerCorrect(VerifyContainerType type, const IOS::ES::CertReader& issuer_cert) const
{
  switch (type)
  {
  case VerifyContainerType::TMD:
    return issuer_cert.GetName().compare(0, 2, "CP") == 0;
  case VerifyContainerType::Ticket:
    return issuer_cert.GetName().compare(0, 2, "XS") == 0;
  case VerifyContainerType::Device:
    return issuer_cert.GetName().compare(0, 2, "MS") == 0;
  default:
    return false;
  }
}

static const std::string CERT_STORE_PATH = "/sys/cert.sys";

ReturnCode ES::ReadCertStore(std::vector<u8>* buffer) const
{
  if (!SConfig::GetInstance().m_enable_signature_checks)
    return IPC_SUCCESS;

  const auto store_file =
      m_ios.GetFS()->OpenFile(PID_KERNEL, PID_KERNEL, CERT_STORE_PATH, FS::Mode::Read);
  if (!store_file)
    return FS::ConvertResult(store_file.Error());

  buffer->resize(store_file->GetStatus()->size);
  if (!store_file->Read(buffer->data(), buffer->size()))
    return ES_SHORT_READ;
  return IPC_SUCCESS;
}

ReturnCode ES::WriteNewCertToStore(const IOS::ES::CertReader& cert)
{
  // Read the current store to determine if the new cert needs to be written.
  std::vector<u8> current_store;
  const ReturnCode ret = ReadCertStore(&current_store);
  if (ret == IPC_SUCCESS)
  {
    const std::map<std::string, IOS::ES::CertReader> certs = IOS::ES::ParseCertChain(current_store);
    // The cert is already present in the store. Nothing to do.
    if (certs.find(cert.GetName()) != certs.end())
      return IPC_SUCCESS;
  }

  // Otherwise, write the new cert at the end of the store.
  const auto store_file =
      m_ios.GetFS()->CreateAndOpenFile(PID_KERNEL, PID_KERNEL, CERT_STORE_PATH,
                                       {FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::Read});
  if (!store_file || !store_file->Seek(0, FS::SeekMode::End) ||
      !store_file->Write(cert.GetBytes().data(), cert.GetBytes().size()))
  {
    return ES_EIO;
  }
  return IPC_SUCCESS;
}

ReturnCode ES::VerifyContainer(VerifyContainerType type, VerifyMode mode,
                               const IOS::ES::SignedBlobReader& signed_blob,
                               const std::vector<u8>& cert_chain, u32* issuer_handle_out)
{
  if (!SConfig::GetInstance().m_enable_signature_checks)
    return IPC_SUCCESS;

  if (!signed_blob.IsSignatureValid())
    return ES_EINVAL;

  // A blob should have exactly 3 parent issuers.
  // Example for a ticket: "Root-CA00000001-XS00000003" => {"Root", "CA00000001", "XS00000003"}
  const std::string issuer = signed_blob.GetIssuer();
  const std::vector<std::string> parents = SplitString(issuer, '-');
  if (parents.size() != 3)
    return ES_EINVAL;

  // Find the direct issuer and the CA certificates for the blob.
  const std::map<std::string, IOS::ES::CertReader> certs = IOS::ES::ParseCertChain(cert_chain);
  const auto issuer_cert_iterator = certs.find(parents[2]);
  const auto ca_cert_iterator = certs.find(parents[1]);
  if (issuer_cert_iterator == certs.end() || ca_cert_iterator == certs.end())
    return ES_UNKNOWN_ISSUER;
  const IOS::ES::CertReader& issuer_cert = issuer_cert_iterator->second;
  const IOS::ES::CertReader& ca_cert = ca_cert_iterator->second;

  // Some blobs can only be signed by specific certificates.
  if (!IsIssuerCorrect(type, issuer_cert))
    return ES_EINVAL;

  // Verify the whole cert chain using IOSC.
  // IOS assumes that the CA cert will always be signed by the root certificate,
  // and that the issuer is signed by the CA.
  IOSC& iosc = m_ios.GetIOSC();
  IOSC::Handle handle;

  // Create and initialise a handle for the CA cert and the issuer cert.
  ReturnCode ret = iosc.CreateObject(&handle, IOSC::TYPE_PUBLIC_KEY, IOSC::SUBTYPE_RSA2048, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;
  Common::ScopeGuard ca_guard{[&] { iosc.DeleteObject(handle, PID_ES); }};
  ret = iosc.ImportCertificate(ca_cert, IOSC::HANDLE_ROOT_KEY, handle, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifyContainer: IOSC_ImportCertificate(ca) failed with error %d", ret);
    return ret;
  }

  IOSC::Handle issuer_handle;
  const IOSC::ObjectSubType subtype =
      type == VerifyContainerType::Device ? IOSC::SUBTYPE_ECC233 : IOSC::SUBTYPE_RSA2048;
  ret = iosc.CreateObject(&issuer_handle, IOSC::TYPE_PUBLIC_KEY, subtype, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;
  Common::ScopeGuard issuer_guard{[&] { iosc.DeleteObject(issuer_handle, PID_ES); }};
  ret = iosc.ImportCertificate(issuer_cert, handle, issuer_handle, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifyContainer: IOSC_ImportCertificate(issuer) failed with error %d", ret);
    return ret;
  }

  // Verify the signature.
  const std::vector<u8> signature = signed_blob.GetSignatureData();
  ret = iosc.VerifyPublicKeySign(signed_blob.GetSha1(), issuer_handle, signature, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifyContainer: IOSC_VerifyPublicKeySign failed with error %d", ret);
    return ret;
  }

  if (mode == VerifyMode::UpdateCertStore)
  {
    ret = WriteNewCertToStore(issuer_cert);
    if (ret != IPC_SUCCESS)
      ERROR_LOG(IOS_ES, "VerifyContainer: Writing the issuer cert failed with return code %d", ret);

    ret = WriteNewCertToStore(ca_cert);
    if (ret != IPC_SUCCESS)
      ERROR_LOG(IOS_ES, "VerifyContainer: Writing the CA cert failed with return code %d", ret);
  }

  if (ret == IPC_SUCCESS && issuer_handle_out)
  {
    *issuer_handle_out = issuer_handle;
    issuer_guard.Dismiss();
  }

  return ret;
}

ReturnCode ES::VerifyContainer(VerifyContainerType type, VerifyMode mode,
                               const IOS::ES::CertReader& cert, const std::vector<u8>& cert_chain,
                               u32 certificate_iosc_handle)
{
  IOSC::Handle issuer_handle;
  ReturnCode ret = VerifyContainer(type, mode, cert, cert_chain, &issuer_handle);
  // Import the signed blob.
  if (ret == IPC_SUCCESS)
  {
    ret = m_ios.GetIOSC().ImportCertificate(cert, issuer_handle, certificate_iosc_handle, PID_ES);
    m_ios.GetIOSC().DeleteObject(issuer_handle, PID_ES);
  }
  return ret;
}
}  // namespace IOS::HLE::Device
