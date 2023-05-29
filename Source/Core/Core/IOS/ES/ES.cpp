// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/ES/ES.h"

#include <algorithm>
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
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/FS/FileSystemProxy.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"
#include "Core/IOS/VersionInfo.h"
#include "Core/System.h"
#include "DiscIO/Enums.h"

namespace IOS::HLE
{
namespace
{
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

constexpr const char LAUNCH_FILE_PATH[] = "/sys/launch.sys";
constexpr const char SPACE_FILE_PATH[] = "/sys/space.sys";
constexpr size_t SPACE_FILE_SIZE = sizeof(u64) + sizeof(ES::TicketView) + ES::MAX_TMD_SIZE;

CoreTiming::EventType* s_finish_init_event;
CoreTiming::EventType* s_reload_ios_for_ppc_launch_event;
CoreTiming::EventType* s_bootstrap_ppc_for_launch_event;

constexpr SystemTimers::TimeBaseTick GetESBootTicks(u32 ios_version)
{
  if (ios_version < 28)
    return 22'000'000_tbticks;

  // Starting from IOS28, ES needs to load additional modules when it starts
  // since the main ELF only contains the kernel and core modules.
  if (ios_version < 57)
    return 33'000'000_tbticks;

  // These versions have extra modules that make them noticeably slower to load.
  if (ios_version == 57 || ios_version == 58 || ios_version == 59)
    return 39'000'000_tbticks;

  return 37'000'000_tbticks;
}
}  // namespace

ESCore::ESCore(Kernel& ios) : m_ios(ios)
{
  for (const auto& directory : s_directories_to_create)
  {
    // Note: ES sets its own UID and GID to 0/0 at boot, so all filesystem accesses in ES are done
    // as UID 0 even though its PID is 1.
    const auto result = m_ios.GetFS()->CreateDirectory(PID_KERNEL, PID_KERNEL, directory.path,
                                                       directory.attribute, directory.modes);
    if (result != FS::ResultCode::Success && result != FS::ResultCode::AlreadyExists)
    {
      ERROR_LOG_FMT(IOS_ES, "Failed to create {}: error {}", directory.path,
                    static_cast<s32>(FS::ConvertResult(result)));
    }

    // Now update the UID/GID and other attributes.
    m_ios.GetFS()->SetMetadata(0, directory.path, directory.uid, directory.gid, directory.attribute,
                               directory.modes);
  }

  FinishAllStaleImports();
}

ESCore::~ESCore() = default;

ESDevice::ESDevice(EmulationKernel& ios, ESCore& core, const std::string& device_name)
    : EmulationDevice(ios, device_name), m_core(core)
{
  if (Core::IsRunningAndStarted())
  {
    auto& system = ios.GetSystem();
    auto& core_timing = system.GetCoreTiming();
    core_timing.RemoveEvent(s_finish_init_event);
    core_timing.ScheduleEvent(GetESBootTicks(ios.GetVersion()), s_finish_init_event);
  }
  else
  {
    FinishInit();
  }
}

ESDevice::~ESDevice() = default;

void ESDevice::InitializeEmulationState()
{
  auto& system = Core::System::GetInstance();
  auto& core_timing = system.GetCoreTiming();
  s_finish_init_event =
      core_timing.RegisterEvent("IOS-ESFinishInit", [](Core::System& system_, u64, s64) {
        GetIOS()->GetESDevice()->FinishInit();
      });
  s_reload_ios_for_ppc_launch_event = core_timing.RegisterEvent(
      "IOS-ESReloadIOSForPPCLaunch", [](Core::System& system_, u64 ios_id, s64) {
        GetIOS()->GetESDevice()->LaunchTitle(ios_id, HangPPC::Yes);
      });
  s_bootstrap_ppc_for_launch_event =
      core_timing.RegisterEvent("IOS-ESBootstrapPPCForLaunch", [](Core::System& system_, u64, s64) {
        GetIOS()->GetESDevice()->BootstrapPPC();
      });
}

void ESDevice::FinalizeEmulationState()
{
  s_finish_init_event = nullptr;
  s_reload_ios_for_ppc_launch_event = nullptr;
  s_bootstrap_ppc_for_launch_event = nullptr;
}

void ESDevice::FinishInit()
{
  GetEmulationKernel().InitIPC();

  std::optional<u64> pending_launch_title_id;

  {
    const auto launch_file = GetEmulationKernel().GetFS()->OpenFile(
        PID_KERNEL, PID_KERNEL, LAUNCH_FILE_PATH, FS::Mode::Read);
    if (launch_file)
    {
      u64 id;
      if (launch_file->Read(&id, 1).Succeeded())
        pending_launch_title_id = id;
    }
  }

  if (pending_launch_title_id.has_value())
  {
    NOTICE_LOG_FMT(IOS, "Re-launching title {:016x} after IOS reload.", *pending_launch_title_id);
    LaunchTitle(*pending_launch_title_id, HangPPC::No);
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

void TitleContext::Update(const ES::TMDReader& tmd_, const ES::TicketReader& ticket_,
                          DiscIO::Platform platform)
{
  if (!tmd_.IsValid() || !ticket_.IsValid())
  {
    ERROR_LOG_FMT(IOS_ES, "TMD or ticket is not valid -- refusing to update title context");
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

IPCReply ESDevice::GetTitleDirectory(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 1))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);

  char* path = reinterpret_cast<char*>(memory.GetPointer(request.io_vectors[0].address));
  sprintf(path, "/title/%08x/%08x/data", static_cast<u32>(title_id >> 32),
          static_cast<u32>(title_id));

  INFO_LOG_FMT(IOS_ES, "IOCTL_ES_GETTITLEDIR: {}", path);
  return IPCReply(IPC_SUCCESS);
}

ReturnCode ESCore::GetTitleId(u64* title_id) const
{
  if (!m_title_context.active)
    return ES_EINVAL;
  *title_id = m_title_context.tmd.GetTitleId();
  return IPC_SUCCESS;
}

IPCReply ESDevice::GetTitleId(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
    return IPCReply(ES_EINVAL);

  u64 title_id;
  const ReturnCode ret = m_core.GetTitleId(&title_id);
  if (ret != IPC_SUCCESS)
    return IPCReply(ret);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  memory.Write_U64(title_id, request.io_vectors[0].address);
  INFO_LOG_FMT(IOS_ES, "IOCTL_ES_GETTITLEID: {:08x}/{:08x}", static_cast<u32>(title_id >> 32),
               static_cast<u32>(title_id));
  return IPCReply(IPC_SUCCESS);
}

static bool UpdateUIDAndGID(EmulationKernel& kernel, const ES::TMDReader& tmd)
{
  ES::UIDSys uid_sys{kernel.GetFSCore()};
  const u64 title_id = tmd.GetTitleId();
  const u32 uid = uid_sys.GetOrInsertUIDForTitle(title_id);
  if (uid == 0)
  {
    ERROR_LOG_FMT(IOS_ES, "Failed to get UID for title {:016x}", title_id);
    return false;
  }
  kernel.SetUidForPPC(uid);
  kernel.SetGidForPPC(tmd.GetGroupId());
  return true;
}

static ReturnCode CheckIsAllowedToSetUID(EmulationKernel& kernel, const u32 caller_uid,
                                         const ES::TMDReader& active_tmd)
{
  ES::UIDSys uid_map{kernel.GetFSCore()};
  const u32 system_menu_uid = uid_map.GetOrInsertUIDForTitle(Titles::SYSTEM_MENU);
  if (!system_menu_uid)
    return ES_SHORT_READ;

  if (caller_uid == system_menu_uid)
    return IPC_SUCCESS;

  if (kernel.GetVersion() == 62)
  {
    const bool is_wiiu_transfer_tool =
        active_tmd.IsValid() && (active_tmd.GetTitleId() | 0xFF) == 0x00010001'484353ff;
    if (is_wiiu_transfer_tool)
      return IPC_SUCCESS;
  }

  return ES_EINVAL;
}

IPCReply ESDevice::SetUID(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != 8)
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);

  const s32 ret = CheckIsAllowedToSetUID(GetEmulationKernel(), uid, m_core.m_title_context.tmd);
  if (ret < 0)
  {
    ERROR_LOG_FMT(IOS_ES, "SetUID: Permission check failed with error {}", ret);
    return IPCReply(ret);
  }

  const auto tmd = m_core.FindInstalledTMD(title_id);
  if (!tmd.IsValid())
    return IPCReply(FS_ENOENT);

  if (!UpdateUIDAndGID(GetEmulationKernel(), tmd))
  {
    ERROR_LOG_FMT(IOS_ES, "SetUID: Failed to get UID for title {:016x}", title_id);
    return IPCReply(ES_SHORT_READ);
  }

  return IPCReply(IPC_SUCCESS);
}

bool ESDevice::LaunchTitle(u64 title_id, HangPPC hang_ppc)
{
  m_core.m_title_context.Clear();
  INFO_LOG_FMT(IOS_ES, "ES_Launch: Title context changed: (none)");

  NOTICE_LOG_FMT(IOS_ES, "Launching title {:016x}...", title_id);

  if ((title_id == Titles::SHOP || title_id == Titles::KOREAN_SHOP) &&
      GetEmulationKernel().GetIOSC().IsUsingDefaultId())
  {
    ERROR_LOG_FMT(IOS_ES, "Refusing to launch the shop channel with default device credentials");
    CriticalAlertFmtT(
        "You cannot use the Wii Shop Channel without using your own device credentials."
        "\nPlease refer to the NAND usage guide for setup instructions: "
        "https://dolphin-emu.org/docs/guides/nand-usage-guide/");

    // Send the user back to the system menu instead of returning an error, which would
    // likely make the system menu crash. Doing this is okay as anyone who has the shop
    // also has the system menu installed, and this behaviour is consistent with what
    // ES does when its DRM system refuses the use of a particular title.
    return LaunchTitle(Titles::SYSTEM_MENU, hang_ppc);
  }

  if (IsTitleType(title_id, ES::TitleType::System) && title_id != Titles::SYSTEM_MENU)
    return LaunchIOS(title_id, hang_ppc);
  return LaunchPPCTitle(title_id);
}

bool ESDevice::LaunchIOS(u64 ios_title_id, HangPPC hang_ppc)
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
    NOTICE_LOG_FMT(IOS, "BC: Launching MIOS...");
    return LaunchIOS(Titles::MIOS, hang_ppc);
  }

  // IOS checks whether the system title is installed and returns an error if it isn't.
  // Unfortunately, we can't rely on titles being installed as we don't require system titles,
  // so only have this check for MIOS (for which having the binary is *required*).
  if (ios_title_id == Titles::MIOS)
  {
    const ES::TMDReader tmd = m_core.FindInstalledTMD(ios_title_id);
    const ES::TicketReader ticket = m_core.FindSignedTicket(ios_title_id);
    ES::Content content;
    if (!tmd.IsValid() || !ticket.IsValid() || !tmd.GetContent(tmd.GetBootIndex(), &content) ||
        !GetEmulationKernel().BootIOS(GetSystem(), ios_title_id, hang_ppc,
                                      m_core.GetContentPath(ios_title_id, content)))
    {
      PanicAlertFmtT("Could not launch IOS {0:016x} because it is missing from the NAND.\n"
                     "The emulated software will likely hang now.",
                     ios_title_id);
      return false;
    }
    return true;
  }

  return GetEmulationKernel().BootIOS(GetSystem(), ios_title_id, hang_ppc);
}

s32 ESDevice::WriteLaunchFile(const ES::TMDReader& tmd, Ticks ticks)
{
  GetEmulationKernel().GetFSCore().DeleteFile(PID_KERNEL, PID_KERNEL, SPACE_FILE_PATH, ticks);

  std::vector<u8> launch_data(sizeof(u64) + sizeof(ES::TicketView));
  const u64 title_id = tmd.GetTitleId();
  std::memcpy(launch_data.data(), &title_id, sizeof(title_id));
  // We're supposed to write a ticket view here, but we don't use it for anything (other than
  // to take up space in the NAND and slow down launches) so don't bother.
  launch_data.insert(launch_data.end(), tmd.GetBytes().begin(), tmd.GetBytes().end());
  return WriteSystemFile(LAUNCH_FILE_PATH, launch_data, ticks);
}

bool ESDevice::LaunchPPCTitle(u64 title_id)
{
  u64 ticks = 0;

  const ES::TMDReader tmd = m_core.FindInstalledTMD(title_id, &ticks);
  const ES::TicketReader ticket = m_core.FindSignedTicket(title_id);

  if (!tmd.IsValid() || !ticket.IsValid())
  {
    if (title_id == Titles::SYSTEM_MENU)
    {
      PanicAlertFmtT("Could not launch the Wii Menu because it is missing from the NAND.\n"
                     "The emulated software will likely hang now.");
    }
    else
    {
      PanicAlertFmtT("Could not launch title {0:016x} because it is missing from the NAND.\n"
                     "The emulated software will likely hang now.",
                     title_id);
    }
    return false;
  }

  auto& system = GetSystem();
  auto& core_timing = system.GetCoreTiming();

  // Before launching a title, IOS first reads the TMD and reloads into the specified IOS version,
  // even when that version is already running. After it has reloaded, ES_Launch will be called
  // again and the PPC will be bootstrapped then.
  //
  // To keep track of the PPC title launch, a temporary launch file (LAUNCH_FILE_PATH) is used
  // to store the title ID of the title to launch and its TMD.
  // The launch file not existing means an IOS reload is required.
  if (const auto launch_file_fd = GetEmulationKernel().GetFSCore().Open(
          PID_KERNEL, PID_KERNEL, LAUNCH_FILE_PATH, FS::Mode::Read, {}, &ticks);
      launch_file_fd.Get() < 0)
  {
    if (WriteLaunchFile(tmd, &ticks) != IPC_SUCCESS)
    {
      PanicAlertFmt("LaunchPPCTitle: Failed to write launch file");
      return false;
    }

    const u64 required_ios = tmd.GetIOSId();
    if (!Core::IsRunningAndStarted())
      return LaunchTitle(required_ios, HangPPC::Yes);
    core_timing.RemoveEvent(s_reload_ios_for_ppc_launch_event);
    core_timing.ScheduleEvent(ticks, s_reload_ios_for_ppc_launch_event, required_ios);
    return true;
  }

  // Otherwise, assume that the PPC title can now be launched directly.
  // Unlike IOS, we won't bother checking the title ID in the launch file. (It's not useful.)
  GetEmulationKernel().GetFSCore().DeleteFile(PID_KERNEL, PID_KERNEL, LAUNCH_FILE_PATH, &ticks);
  WriteSystemFile(SPACE_FILE_PATH, std::vector<u8>(SPACE_FILE_SIZE), &ticks);

  m_core.m_title_context.Update(tmd, ticket, DiscIO::Platform::WiiWAD);
  INFO_LOG_FMT(IOS_ES, "LaunchPPCTitle: Title context changed: {:016x}", tmd.GetTitleId());

  // Note: the UID/GID is also updated for IOS titles, but since we have no guarantee IOS titles
  // are installed, we can only do this for PPC titles.
  if (!UpdateUIDAndGID(GetEmulationKernel(), m_core.m_title_context.tmd))
  {
    m_core.m_title_context.Clear();
    INFO_LOG_FMT(IOS_ES, "LaunchPPCTitle: Title context changed: (none)");
    return false;
  }

  ES::Content content;
  if (!tmd.GetContent(tmd.GetBootIndex(), &content))
    return false;

  m_pending_ppc_boot_content_path = m_core.GetContentPath(tmd.GetTitleId(), content);
  if (!Core::IsRunningAndStarted())
    return BootstrapPPC();

  core_timing.RemoveEvent(s_bootstrap_ppc_for_launch_event);
  core_timing.ScheduleEvent(ticks, s_bootstrap_ppc_for_launch_event);
  return true;
}

bool ESDevice::BootstrapPPC()
{
  const bool result =
      GetEmulationKernel().BootstrapPPC(GetSystem(), m_pending_ppc_boot_content_path);
  m_pending_ppc_boot_content_path = {};
  return result;
}

void ESCore::Context::DoState(PointerWrap& p)
{
  p.Do(uid);
  p.Do(gid);
  title_import_export.DoState(p);

  p.Do(active);
  p.Do(ipc_fd);
}

void ESDevice::DoState(PointerWrap& p)
{
  Device::DoState(p);

  for (auto& entry : m_core.m_content_table)
  {
    p.Do(entry.m_opened);
    p.Do(entry.m_title_id);
    p.Do(entry.m_content);
    p.Do(entry.m_fd);
    p.Do(entry.m_uid);
  }

  m_core.m_title_context.DoState(p);

  for (auto& context : m_contexts)
    context.DoState(p);

  p.Do(m_pending_ppc_boot_content_path);
}

ESDevice::ContextArray::iterator ESDevice::FindActiveContext(s32 fd)
{
  return std::find_if(m_contexts.begin(), m_contexts.end(),
                      [fd](const auto& context) { return context.ipc_fd == fd && context.active; });
}

ESDevice::ContextArray::iterator ESDevice::FindInactiveContext()
{
  return std::find_if(m_contexts.begin(), m_contexts.end(),
                      [](const auto& context) { return !context.active; });
}

std::optional<IPCReply> ESDevice::Open(const OpenRequest& request)
{
  auto context = FindInactiveContext();
  if (context == m_contexts.end())
    return IPCReply{ES_FD_EXHAUSTED};

  context->active = true;
  context->uid = request.uid;
  context->gid = request.gid;
  context->ipc_fd = request.fd;
  return Device::Open(request);
}

std::optional<IPCReply> ESDevice::Close(u32 fd)
{
  auto context = FindActiveContext(fd);
  if (context == m_contexts.end())
    return IPCReply(ES_EINVAL);

  context->active = false;
  context->ipc_fd = -1;

  INFO_LOG_FMT(IOS_ES, "ES: Close");
  m_is_active = false;
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> ESDevice::IOCtlV(const IOCtlVRequest& request)
{
  DEBUG_LOG_FMT(IOS_ES, "{} ({:#x})", GetDeviceName(), request.request);
  auto context = FindActiveContext(request.fd);
  if (context == m_contexts.end())
    return IPCReply(ES_EINVAL);

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
    PanicAlertFmt("IOS-ES: Unimplemented ioctlv {:#x} ({} in vectors, {} io vectors)",
                  request.request, request.in_vectors.size(), request.io_vectors.size());
    request.DumpUnknown(GetSystem(), GetDeviceName(), Common::Log::LogType::IOS_ES,
                        Common::Log::LogLevel::LERROR);
    return IPCReply(IPC_EINVAL);

  case IOCTL_ES_INVALID_3F:
  default:
    return IPCReply(IPC_EINVAL);
  }
}

IPCReply ESDevice::GetConsumption(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 2))
    return IPCReply(ES_EINVAL);

  // This is at least what crediar's ES module does
  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  memory.Write_U32(0, request.io_vectors[1].address);
  INFO_LOG_FMT(IOS_ES, "IOCTL_ES_GETCONSUMPTION");
  return IPCReply(IPC_SUCCESS);
}

std::optional<IPCReply> ESDevice::Launch(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 0))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  const u64 title_id = memory.Read_U64(request.in_vectors[0].address);
  const u32 view = memory.Read_U32(request.in_vectors[1].address);
  const u64 ticketid = memory.Read_U64(request.in_vectors[1].address + 4);
  const u32 devicetype = memory.Read_U32(request.in_vectors[1].address + 12);
  const u64 titleid = memory.Read_U64(request.in_vectors[1].address + 16);
  const u16 access = memory.Read_U16(request.in_vectors[1].address + 24);

  INFO_LOG_FMT(IOS_ES, "IOCTL_ES_LAUNCH {:016x} {:08x} {:016x} {:08x} {:016x} {:04x}", title_id,
               view, ticketid, devicetype, titleid, access);

  // Prevent loading installed IOSes that are not emulated.
  if (!IsEmulated(title_id))
    return IPCReply(FS_ENOENT);

  // IOS replies to the request through the mailbox on failure, and acks if the launch succeeds.
  // Note: Launch will potentially reset the whole IOS state -- including this ES instance.
  if (!LaunchTitle(title_id))
    return IPCReply(FS_ENOENT);

  // ES_LAUNCH involves restarting IOS, which results in two acknowledgements in a row
  // (one from the previous IOS for this IPC request, and one from the new one as it boots).
  // Nothing should be written to the command buffer if the launch succeeded for obvious reasons.
  return std::nullopt;
}

std::optional<IPCReply> ESDevice::LaunchBC(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return IPCReply(ES_EINVAL);

  // Here, IOS checks the clock speed and prevents ioctlv 0x25 from being used in GC mode.
  // An alternative way to do this is to check whether the current active IOS is MIOS.
  if (GetEmulationKernel().GetVersion() == 0x101)
    return IPCReply(ES_EINVAL);

  if (!LaunchTitle(0x0000000100000100))
    return IPCReply(FS_ENOENT);

  return std::nullopt;
}

// This is technically an ioctlv in IOS's ES, but it is an internal API which cannot be
// used from the PowerPC (for unpatched and up-to-date IOSes anyway).
// So we block access to it from the IPC interface.
IPCReply ESDevice::DIVerify(const IOCtlVRequest& request)
{
  return IPCReply(ES_EINVAL);
}

static ReturnCode WriteTmdForDiVerify(FS::FileSystem* fs, const ES::TMDReader& tmd)
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

ReturnCode ESDevice::DIVerify(const ES::TMDReader& tmd, const ES::TicketReader& ticket)
{
  m_core.m_title_context.Clear();
  INFO_LOG_FMT(IOS_ES, "ES_DIVerify: Title context changed: (none)");

  if (!tmd.IsValid() || !ticket.IsValid())
    return ES_EINVAL;

  if (tmd.GetTitleId() != ticket.GetTitleId())
    return ES_EINVAL;

  m_core.m_title_context.Update(tmd, ticket, DiscIO::Platform::WiiDisc);
  INFO_LOG_FMT(IOS_ES, "ES_DIVerify: Title context changed: {:016x}", tmd.GetTitleId());

  // XXX: We are supposed to verify the TMD and ticket here, but cannot because
  // this may cause issues with custom/patched games.

  const auto fs = GetEmulationKernel().GetFS();
  if (!m_core.FindInstalledTMD(tmd.GetTitleId()).IsValid())
  {
    if (const ReturnCode ret = WriteTmdForDiVerify(fs.get(), tmd))
    {
      ERROR_LOG_FMT(IOS_ES, "DiVerify failed to write disc TMD to NAND.");
      return ret;
    }
  }

  if (!UpdateUIDAndGID(GetEmulationKernel(), m_core.m_title_context.tmd))
  {
    return ES_SHORT_READ;
  }

  const std::string data_dir = Common::GetTitleDataPath(tmd.GetTitleId());
  // Might already exist, so we only need to check whether the second operation succeeded.
  constexpr FS::Modes data_dir_modes{FS::Mode::ReadWrite, FS::Mode::None, FS::Mode::None};
  fs->CreateDirectory(PID_KERNEL, PID_KERNEL, data_dir, 0, data_dir_modes);
  return FS::ConvertResult(fs->SetMetadata(0, data_dir, GetEmulationKernel().GetUidForPPC(),
                                           GetEmulationKernel().GetGidForPPC(), 0, data_dir_modes));
}

ReturnCode ESCore::CheckStreamKeyPermissions(const u32 uid, const u8* ticket_view,
                                             const ES::TMDReader& tmd) const
{
  const u32 title_flags = tmd.GetTitleFlags();
  // Only allow using this function with some titles (WFS titles).
  // The following is the exact check from IOS. Unfortunately, other than knowing that the
  // title type is what IOS checks, we don't know much about the constants used here.
  constexpr u32 WFS_AND_0x4_FLAG = ES::TITLE_TYPE_0x4 | ES::TITLE_TYPE_WFS_MAYBE;
  if ((!(title_flags & ES::TITLE_TYPE_0x4) && ~(title_flags >> 5) & 1) ||
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
  if (title_flags & ES::TITLE_TYPE_WFS_MAYBE && uid != PID_UNKNOWN)
    return ES_EINVAL;

  const u64 view_title_id = Common::swap64(ticket_view + offsetof(ES::TicketView, title_id));
  if (view_title_id != tmd.GetTitleId())
    return ES_EINVAL;

  // More permission checks.
  const u32 permitted_title_mask =
      Common::swap32(ticket_view + offsetof(ES::TicketView, permitted_title_mask));
  const u32 permitted_title_id =
      Common::swap32(ticket_view + offsetof(ES::TicketView, permitted_title_id));
  if ((uid == PID_UNKNOWN && (~permitted_title_mask & 0x13) != permitted_title_id) ||
      !IsActiveTitlePermittedByTicket(ticket_view))
  {
    return ES_EACCES;
  }

  return IPC_SUCCESS;
}

ReturnCode ESCore::SetUpStreamKey(const u32 uid, const u8* ticket_view, const ES::TMDReader& tmd,
                                  u32* handle)
{
  ReturnCode ret = CheckStreamKeyPermissions(uid, ticket_view, tmd);
  if (ret != IPC_SUCCESS)
    return ret;

  // TODO (for the future): signature checks.

  // Find a signed ticket from the view.
  const u64 ticket_id = Common::swap64(&ticket_view[offsetof(ES::TicketView, ticket_id)]);
  const u64 title_id = Common::swap64(&ticket_view[offsetof(ES::TicketView, title_id)]);
  const ES::TicketReader installed_ticket = FindSignedTicket(title_id);
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

  const u8 index = ticket_bytes[offsetof(ES::Ticket, common_key_index)];
  if (index >= IOSC::COMMON_KEY_HANDLES.size())
    return ES_INVALID_TICKET;

  return m_ios.GetIOSC().ImportSecretKey(*handle, IOSC::COMMON_KEY_HANDLES[index], iv.data(),
                                         &ticket_bytes[offsetof(ES::Ticket, title_key)], PID_ES);
}

IPCReply ESDevice::SetUpStreamKey(const Context& context, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(2, 1) ||
      request.in_vectors[0].size != sizeof(ES::TicketView) ||
      !ES::IsValidTMDSize(request.in_vectors[1].size) || request.io_vectors[0].size != sizeof(u32))
  {
    return IPCReply(ES_EINVAL);
  }

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  std::vector<u8> tmd_bytes(request.in_vectors[1].size);
  memory.CopyFromEmu(tmd_bytes.data(), request.in_vectors[1].address, tmd_bytes.size());
  const ES::TMDReader tmd{std::move(tmd_bytes)};

  if (!tmd.IsValid())
    return IPCReply(ES_EINVAL);

  u32 handle;
  const ReturnCode ret = m_core.SetUpStreamKey(
      context.uid, memory.GetPointer(request.in_vectors[0].address), tmd, &handle);
  memory.Write_U32(handle, request.io_vectors[0].address);
  return IPCReply(ret);
}

IPCReply ESDevice::DeleteStreamKey(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 0) || request.in_vectors[0].size != sizeof(u32))
    return IPCReply(ES_EINVAL);

  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  const u32 handle = memory.Read_U32(request.in_vectors[0].address);
  return IPCReply(GetEmulationKernel().GetIOSC().DeleteObject(handle, PID_ES));
}

bool ESCore::IsActiveTitlePermittedByTicket(const u8* ticket_view) const
{
  if (!m_title_context.active)
    return false;

  const u32 title_identifier = static_cast<u32>(m_title_context.tmd.GetTitleId());
  const u32 permitted_title_mask =
      Common::swap32(ticket_view + offsetof(ES::TicketView, permitted_title_mask));
  const u32 permitted_title_id =
      Common::swap32(ticket_view + offsetof(ES::TicketView, permitted_title_id));
  return title_identifier && (title_identifier & ~permitted_title_mask) == permitted_title_id;
}

bool ESCore::IsIssuerCorrect(VerifyContainerType type, const ES::CertReader& issuer_cert) const
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

ReturnCode ESCore::ReadCertStore(std::vector<u8>* buffer) const
{
  const auto store_file =
      m_ios.GetFS()->OpenFile(PID_KERNEL, PID_KERNEL, CERT_STORE_PATH, FS::Mode::Read);
  if (!store_file)
    return FS::ConvertResult(store_file.Error());

  buffer->resize(store_file->GetStatus()->size);
  if (!store_file->Read(buffer->data(), buffer->size()))
    return ES_SHORT_READ;
  return IPC_SUCCESS;
}

ReturnCode ESCore::WriteNewCertToStore(const ES::CertReader& cert)
{
  // Read the current store to determine if the new cert needs to be written.
  std::vector<u8> current_store;
  const ReturnCode ret = ReadCertStore(&current_store);
  if (ret == IPC_SUCCESS)
  {
    const std::map<std::string, ES::CertReader> certs = ES::ParseCertChain(current_store);
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

ReturnCode ESCore::VerifyContainer(VerifyContainerType type, VerifyMode mode,
                                   const ES::SignedBlobReader& signed_blob,
                                   const std::vector<u8>& cert_chain, u32* issuer_handle_out)
{
  if (!signed_blob.IsSignatureValid())
    return ES_EINVAL;

  // A blob should have exactly 3 parent issuers.
  // Example for a ticket: "Root-CA00000001-XS00000003" => {"Root", "CA00000001", "XS00000003"}
  const std::string issuer = signed_blob.GetIssuer();
  const std::vector<std::string> parents = SplitString(issuer, '-');
  if (parents.size() != 3)
    return ES_EINVAL;

  // Find the direct issuer and the CA certificates for the blob.
  const std::map<std::string, ES::CertReader> certs = ES::ParseCertChain(cert_chain);
  const auto issuer_cert_iterator = certs.find(parents[2]);
  const auto ca_cert_iterator = certs.find(parents[1]);
  if (issuer_cert_iterator == certs.end() || ca_cert_iterator == certs.end())
    return ES_UNKNOWN_ISSUER;
  const ES::CertReader& issuer_cert = issuer_cert_iterator->second;
  const ES::CertReader& ca_cert = ca_cert_iterator->second;

  // Some blobs can only be signed by specific certificates.
  if (!IsIssuerCorrect(type, issuer_cert))
    return ES_EINVAL;

  // Verify the whole cert chain using IOSC.
  // IOS assumes that the CA cert will always be signed by the root certificate,
  // and that the issuer is signed by the CA.
  IOSC& iosc = m_ios.GetIOSC();
  IOSC::Handle ca_handle;

  // Create and initialise a handle for the CA cert and the issuer cert.
  ReturnCode ret =
      iosc.CreateObject(&ca_handle, IOSC::TYPE_PUBLIC_KEY, IOSC::SUBTYPE_RSA2048, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;
  Common::ScopeGuard ca_guard{[&] { iosc.DeleteObject(ca_handle, PID_ES); }};
  ret = iosc.ImportCertificate(ca_cert, IOSC::HANDLE_ROOT_KEY, ca_handle, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG_FMT(IOS_ES, "VerifyContainer: IOSC_ImportCertificate(ca) failed with error {}",
                  static_cast<s32>(ret));
    return ret;
  }

  IOSC::Handle issuer_handle;
  const IOSC::ObjectSubType subtype =
      type == VerifyContainerType::Device ? IOSC::SUBTYPE_ECC233 : IOSC::SUBTYPE_RSA2048;
  ret = iosc.CreateObject(&issuer_handle, IOSC::TYPE_PUBLIC_KEY, subtype, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;
  Common::ScopeGuard issuer_guard{[&] { iosc.DeleteObject(issuer_handle, PID_ES); }};
  ret = iosc.ImportCertificate(issuer_cert, ca_handle, issuer_handle, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG_FMT(IOS_ES, "VerifyContainer: IOSC_ImportCertificate(issuer) failed with error {}",
                  static_cast<s32>(ret));
    return ret;
  }

  // Verify the signature.
  const std::vector<u8> signature = signed_blob.GetSignatureData();
  ret = iosc.VerifyPublicKeySign(signed_blob.GetSha1(), issuer_handle, signature, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG_FMT(IOS_ES, "VerifyContainer: IOSC_VerifyPublicKeySign failed with error {}",
                  static_cast<s32>(ret));
    return ret;
  }

  if (mode == VerifyMode::UpdateCertStore)
  {
    ret = WriteNewCertToStore(issuer_cert);
    if (ret != IPC_SUCCESS)
    {
      ERROR_LOG_FMT(IOS_ES, "VerifyContainer: Writing the issuer cert failed with return code {}",
                    static_cast<s32>(ret));
    }

    ret = WriteNewCertToStore(ca_cert);
    if (ret != IPC_SUCCESS)
      ERROR_LOG_FMT(IOS_ES, "VerifyContainer: Writing the CA cert failed with return code {}",
                    static_cast<s32>(ret));
  }

  if (ret == IPC_SUCCESS && issuer_handle_out)
  {
    *issuer_handle_out = issuer_handle;
    issuer_guard.Dismiss();
  }

  return ret;
}

ReturnCode ESCore::VerifyContainer(VerifyContainerType type, VerifyMode mode,
                                   const ES::CertReader& cert, const std::vector<u8>& cert_chain,
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
}  // namespace IOS::HLE
