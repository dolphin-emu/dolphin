// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/IOS.h"

#include <algorithm>
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/EnumUtils.h"
#include "Common/Logging/Log.h"
#include "Common/Timer.h"

#include "Core/Boot/AncastTypes.h"
#include "Core/Boot/DolReader.h"
#include "Core/Boot/ElfReader.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/WII_IPC.h"
#include "Core/IOS/Crypto/AesDevice.h"
#include "Core/IOS/Crypto/Sha.h"
#include "Core/IOS/DI/DI.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/DeviceStub.h"
#include "Core/IOS/DolphinDevice.h"
#include "Core/IOS/ES/ES.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/FS/FileSystemProxy.h"
#include "Core/IOS/MIOS.h"
#include "Core/IOS/Network/IP/Top.h"
#include "Core/IOS/Network/KD/NetKDRequest.h"
#include "Core/IOS/Network/KD/NetKDTime.h"
#include "Core/IOS/Network/NCD/Manage.h"
#include "Core/IOS/Network/SSL.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/IOS/Network/WD/Command.h"
#include "Core/IOS/SDIO/SDIOSlot0.h"
#include "Core/IOS/STM/STM.h"
#include "Core/IOS/USB/Bluetooth/BTEmu.h"
#include "Core/IOS/USB/Bluetooth/BTReal.h"
#include "Core/IOS/USB/OH0/OH0.h"
#include "Core/IOS/USB/OH0/OH0Device.h"
#include "Core/IOS/USB/USB_HID/HIDv4.h"
#include "Core/IOS/USB/USB_HID/HIDv5.h"
#include "Core/IOS/USB/USB_KBD.h"
#include "Core/IOS/USB/USB_VEN/VEN.h"
#include "Core/IOS/VersionInfo.h"
#include "Core/IOS/WFS/WFSI.h"
#include "Core/IOS/WFS/WFSSRV.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"

namespace IOS::HLE
{
static std::unique_ptr<EmulationKernel> s_ios;

constexpr u64 ENQUEUE_REQUEST_FLAG = 0x100000000ULL;
static CoreTiming::EventType* s_event_enqueue;
static CoreTiming::EventType* s_event_finish_ppc_bootstrap;
static CoreTiming::EventType* s_event_finish_ios_boot;

constexpr u32 ADDR_LEGACY_MEM_SIZE = 0x28;
constexpr u32 ADDR_LEGACY_ARENA_LOW = 0x30;
constexpr u32 ADDR_LEGACY_ARENA_HIGH = 0x34;
constexpr u32 ADDR_LEGACY_MEM_SIM_SIZE = 0xf0;

constexpr u32 ADDR_MEM1_SIZE = 0x3100;
constexpr u32 ADDR_MEM1_SIM_SIZE = 0x3104;
constexpr u32 ADDR_MEM1_END = 0x3108;
constexpr u32 ADDR_MEM1_ARENA_BEGIN = 0x310c;
constexpr u32 ADDR_MEM1_ARENA_END = 0x3110;
constexpr u32 ADDR_PH1 = 0x3114;
constexpr u32 ADDR_MEM2_SIZE = 0x3118;
constexpr u32 ADDR_MEM2_SIM_SIZE = 0x311c;
constexpr u32 ADDR_MEM2_END = 0x3120;
constexpr u32 ADDR_MEM2_ARENA_BEGIN = 0x3124;
constexpr u32 ADDR_MEM2_ARENA_END = 0x3128;
constexpr u32 ADDR_PH2 = 0x312c;
constexpr u32 ADDR_IPC_BUFFER_BEGIN = 0x3130;
constexpr u32 ADDR_IPC_BUFFER_END = 0x3134;
constexpr u32 ADDR_HOLLYWOOD_REVISION = 0x3138;
constexpr u32 ADDR_PH3 = 0x313c;
constexpr u32 ADDR_IOS_VERSION = 0x3140;
constexpr u32 ADDR_IOS_DATE = 0x3144;
constexpr u32 ADDR_IOS_RESERVED_BEGIN = 0x3148;
constexpr u32 ADDR_IOS_RESERVED_END = 0x314c;
constexpr u32 ADDR_PH4 = 0x3150;
constexpr u32 ADDR_PH5 = 0x3154;
constexpr u32 ADDR_RAM_VENDOR = 0x3158;
constexpr u32 ADDR_BOOT_FLAG = 0x315c;
constexpr u32 ADDR_APPLOADER_FLAG = 0x315d;
constexpr u32 ADDR_DEVKIT_BOOT_PROGRAM_VERSION = 0x315e;
constexpr u32 ADDR_SYSMENU_SYNC = 0x3160;
constexpr u32 PLACEHOLDER = 0xDEADBEEF;

static bool SetupMemory(Memory::MemoryManager& memory, u64 ios_title_id, MemorySetupType setup_type)
{
  auto target_imv = std::find_if(
      GetMemoryValues().begin(), GetMemoryValues().end(),
      [&](const MemoryValues& imv) { return imv.ios_number == (ios_title_id & 0xffff); });

  if (target_imv == GetMemoryValues().end())
  {
    ERROR_LOG_FMT(IOS, "Unknown IOS version: {:016x}", ios_title_id);
    return false;
  }

  if (setup_type == MemorySetupType::IOSReload)
  {
    memory.Write_U32(target_imv->ios_version, ADDR_IOS_VERSION);

    // These values are written by the IOS kernel as part of its boot process (for IOS28 and newer).
    //
    // This works in a slightly different way on a real console: older IOS versions (< IOS28) all
    // have the same range (933E0000 - 93400000), thus they don't write it at boot and just inherit
    // all values. However, the range has changed since IOS28. To make things work properly
    // after a reload, newer IOSes always write the legacy range before loading an IOS kernel;
    // the new IOS either updates the range (>= IOS28) or inherits it (< IOS28).
    //
    // We can skip this convoluted process and just write the correct range directly.
    memory.Write_U32(target_imv->mem2_physical_size, ADDR_MEM2_SIZE);
    memory.Write_U32(target_imv->mem2_simulated_size, ADDR_MEM2_SIM_SIZE);
    memory.Write_U32(target_imv->mem2_end, ADDR_MEM2_END);
    memory.Write_U32(target_imv->mem2_arena_begin, ADDR_MEM2_ARENA_BEGIN);
    memory.Write_U32(target_imv->mem2_arena_end, ADDR_MEM2_ARENA_END);
    memory.Write_U32(target_imv->ipc_buffer_begin, ADDR_IPC_BUFFER_BEGIN);
    memory.Write_U32(target_imv->ipc_buffer_end, ADDR_IPC_BUFFER_END);
    memory.Write_U32(target_imv->ios_reserved_begin, ADDR_IOS_RESERVED_BEGIN);
    memory.Write_U32(target_imv->ios_reserved_end, ADDR_IOS_RESERVED_END);

    RAMOverrideForIOSMemoryValues(memory, setup_type);
    return true;
  }

  // This region is typically used to store constants (e.g. game ID, console type, ...)
  // and system information (see below).
  constexpr u32 LOW_MEM1_REGION_START = 0;
  constexpr u32 LOW_MEM1_REGION_SIZE = 0x3fff;
  memory.Memset(LOW_MEM1_REGION_START, 0, LOW_MEM1_REGION_SIZE);

  memory.Write_U32(target_imv->mem1_physical_size, ADDR_MEM1_SIZE);
  memory.Write_U32(target_imv->mem1_simulated_size, ADDR_MEM1_SIM_SIZE);
  memory.Write_U32(target_imv->mem1_end, ADDR_MEM1_END);
  memory.Write_U32(target_imv->mem1_arena_begin, ADDR_MEM1_ARENA_BEGIN);
  memory.Write_U32(target_imv->mem1_arena_end, ADDR_MEM1_ARENA_END);
  memory.Write_U32(PLACEHOLDER, ADDR_PH1);
  memory.Write_U32(target_imv->mem2_physical_size, ADDR_MEM2_SIZE);
  memory.Write_U32(target_imv->mem2_simulated_size, ADDR_MEM2_SIM_SIZE);
  memory.Write_U32(target_imv->mem2_end, ADDR_MEM2_END);
  memory.Write_U32(target_imv->mem2_arena_begin, ADDR_MEM2_ARENA_BEGIN);
  memory.Write_U32(target_imv->mem2_arena_end, ADDR_MEM2_ARENA_END);
  memory.Write_U32(PLACEHOLDER, ADDR_PH2);
  memory.Write_U32(target_imv->ipc_buffer_begin, ADDR_IPC_BUFFER_BEGIN);
  memory.Write_U32(target_imv->ipc_buffer_end, ADDR_IPC_BUFFER_END);
  memory.Write_U32(target_imv->hollywood_revision, ADDR_HOLLYWOOD_REVISION);
  memory.Write_U32(PLACEHOLDER, ADDR_PH3);
  memory.Write_U32(target_imv->ios_version, ADDR_IOS_VERSION);
  memory.Write_U32(target_imv->ios_date, ADDR_IOS_DATE);
  memory.Write_U32(target_imv->ios_reserved_begin, ADDR_IOS_RESERVED_BEGIN);
  memory.Write_U32(target_imv->ios_reserved_end, ADDR_IOS_RESERVED_END);
  memory.Write_U32(PLACEHOLDER, ADDR_PH4);
  memory.Write_U32(PLACEHOLDER, ADDR_PH5);
  memory.Write_U32(target_imv->ram_vendor, ADDR_RAM_VENDOR);
  memory.Write_U8(0xDE, ADDR_BOOT_FLAG);
  memory.Write_U8(0xAD, ADDR_APPLOADER_FLAG);
  memory.Write_U16(0xBEEF, ADDR_DEVKIT_BOOT_PROGRAM_VERSION);
  memory.Write_U32(target_imv->sysmenu_sync, ADDR_SYSMENU_SYNC);

  memory.Write_U32(target_imv->mem1_physical_size, ADDR_LEGACY_MEM_SIZE);
  memory.Write_U32(target_imv->mem1_arena_begin, ADDR_LEGACY_ARENA_LOW);
  memory.Write_U32(target_imv->mem1_arena_end, ADDR_LEGACY_ARENA_HIGH);
  memory.Write_U32(target_imv->mem1_simulated_size, ADDR_LEGACY_MEM_SIM_SIZE);

  RAMOverrideForIOSMemoryValues(memory, setup_type);
  return true;
}

// On a real console, the Starlet resets the PPC and holds it in reset limbo
// by asserting the PPC's HRESET signal (via HW_RESETS).
// We will simulate that by resetting MSR and putting the PPC into an infinite loop.
// The memory write will not be observable since the PPC is not running any code...
static void ResetAndPausePPC(Core::System& system)
{
  // This should be cleared when the PPC is released so that the write is not observable.
  auto& memory = system.GetMemory();
  auto& power_pc = system.GetPowerPC();

  memory.Write_U32(0x48000000, 0x00000000);  // b 0x0
  power_pc.Reset();
  power_pc.GetPPCState().pc = 0;
}

static void ReleasePPC(Core::System& system)
{
  system.GetMemory().Write_U32(0, 0);

  // HLE the bootstub that jumps to 0x3400.
  // NAND titles start with address translation off at 0x3400 (via the PPC bootstub)
  // The state of other CPU registers (like the BAT registers) doesn't matter much
  // because the realmode code at 0x3400 initializes everything itself anyway.
  system.GetPPCState().pc = 0x3400;
}

static void ReleasePPCAncast(Core::System& system)
{
  system.GetMemory().Write_U32(0, 0);

  // On a real console the Espresso verifies and decrypts the Ancast image,
  // then jumps to the decrypted ancast body.
  // The Ancast loader already did this, so just jump to the decrypted body.
  system.GetPPCState().pc = ESPRESSO_ANCAST_LOCATION_VIRT + sizeof(EspressoAncastHeader);
}

void RAMOverrideForIOSMemoryValues(Memory::MemoryManager& memory, MemorySetupType setup_type)
{
  // Don't touch anything if the feature isn't enabled.
  if (!Config::Get(Config::MAIN_RAM_OVERRIDE_ENABLE))
    return;

  // Some unstated constants that can be inferred.
  const u32 ipc_buffer_size =
      memory.Read_U32(ADDR_IPC_BUFFER_END) - memory.Read_U32(ADDR_IPC_BUFFER_BEGIN);
  const u32 ios_reserved_size =
      memory.Read_U32(ADDR_IOS_RESERVED_END) - memory.Read_U32(ADDR_IOS_RESERVED_BEGIN);

  const u32 mem1_physical_size = memory.GetRamSizeReal();
  const u32 mem1_simulated_size = memory.GetRamSizeReal();
  const u32 mem1_end = Memory::MEM1_BASE_ADDR + mem1_simulated_size;
  const u32 mem1_arena_begin = 0;
  const u32 mem1_arena_end = mem1_end;
  const u32 mem2_physical_size = memory.GetExRamSizeReal();
  const u32 mem2_simulated_size = memory.GetExRamSizeReal();
  const u32 mem2_end = Memory::MEM2_BASE_ADDR + mem2_simulated_size - ios_reserved_size;
  const u32 mem2_arena_begin = Memory::MEM2_BASE_ADDR + 0x800U;
  const u32 mem2_arena_end = mem2_end - ipc_buffer_size;
  const u32 ipc_buffer_begin = mem2_arena_end;
  const u32 ipc_buffer_end = mem2_end;
  const u32 ios_reserved_begin = mem2_end;
  const u32 ios_reserved_end = Memory::MEM2_BASE_ADDR + mem2_simulated_size;

  if (setup_type == MemorySetupType::Full)
  {
    // Overwriting these after the game's apploader sets them would be bad
    memory.Write_U32(mem1_physical_size, ADDR_MEM1_SIZE);
    memory.Write_U32(mem1_simulated_size, ADDR_MEM1_SIM_SIZE);
    memory.Write_U32(mem1_end, ADDR_MEM1_END);
    memory.Write_U32(mem1_arena_begin, ADDR_MEM1_ARENA_BEGIN);
    memory.Write_U32(mem1_arena_end, ADDR_MEM1_ARENA_END);

    memory.Write_U32(mem1_physical_size, ADDR_LEGACY_MEM_SIZE);
    memory.Write_U32(mem1_arena_begin, ADDR_LEGACY_ARENA_LOW);
    memory.Write_U32(mem1_arena_end, ADDR_LEGACY_ARENA_HIGH);
    memory.Write_U32(mem1_simulated_size, ADDR_LEGACY_MEM_SIM_SIZE);
  }
  memory.Write_U32(mem2_physical_size, ADDR_MEM2_SIZE);
  memory.Write_U32(mem2_simulated_size, ADDR_MEM2_SIM_SIZE);
  memory.Write_U32(mem2_end, ADDR_MEM2_END);
  memory.Write_U32(mem2_arena_begin, ADDR_MEM2_ARENA_BEGIN);
  memory.Write_U32(mem2_arena_end, ADDR_MEM2_ARENA_END);
  memory.Write_U32(ipc_buffer_begin, ADDR_IPC_BUFFER_BEGIN);
  memory.Write_U32(ipc_buffer_end, ADDR_IPC_BUFFER_END);
  memory.Write_U32(ios_reserved_begin, ADDR_IOS_RESERVED_BEGIN);
  memory.Write_U32(ios_reserved_end, ADDR_IOS_RESERVED_END);
}

void WriteReturnValue(Memory::MemoryManager& memory, s32 value, u32 address)
{
  memory.Write_U32(static_cast<u32>(value), address);
}

Kernel::Kernel(IOSC::ConsoleType console_type) : m_iosc(console_type)
{
  // Until the Wii root and NAND path stuff is entirely managed by IOS and made non-static,
  // using more than one IOS instance at a time is not supported.
  ASSERT(GetIOS() == nullptr);

  m_is_responsible_for_nand_root = !Core::WiiRootIsInitialized();
  if (m_is_responsible_for_nand_root)
    Core::InitializeWiiRoot(false);

  m_fs = FS::MakeFileSystem(IOS::HLE::FS::Location::Session, Core::GetActiveNandRedirects());
  ASSERT(m_fs);

  m_fs_core = std::make_unique<FSCore>(*this);
  m_es_core = std::make_unique<ESCore>(*this);
}

Kernel::~Kernel()
{
  if (m_is_responsible_for_nand_root)
    Core::ShutdownWiiRoot();
}

Kernel::Kernel(u64 title_id) : m_title_id(title_id)
{
}

EmulationKernel::EmulationKernel(Core::System& system, u64 title_id)
    : Kernel(title_id), m_system(system)
{
  INFO_LOG_FMT(IOS, "Starting IOS {:016x}", title_id);

  if (!SetupMemory(m_system.GetMemory(), title_id, MemorySetupType::IOSReload))
    WARN_LOG_FMT(IOS, "No information about this IOS -- cannot set up memory values");

  if (title_id == Titles::MIOS)
  {
    MIOS::Load(m_system);
    return;
  }

  m_fs = FS::MakeFileSystem(IOS::HLE::FS::Location::Session, Core::GetActiveNandRedirects());
  ASSERT(m_fs);

  AddDevice(std::make_unique<AesDevice>(*this, "/dev/aes"));
  AddDevice(std::make_unique<ShaDevice>(*this, "/dev/sha"));

  m_fs_core = std::make_unique<FSCore>(*this);
  AddDevice(std::make_unique<FSDevice>(*this, *m_fs_core, "/dev/fs"));
  m_es_core = std::make_unique<ESCore>(*this);
  AddDevice(std::make_unique<ESDevice>(*this, *m_es_core, "/dev/es"));

  AddStaticDevices();
}

EmulationKernel::~EmulationKernel()
{
  m_system.GetCoreTiming().RemoveAllEvents(s_event_enqueue);

  m_device_map.clear();
  m_socket_manager.reset();
}

// The title ID is a u64 where the first 32 bits are used for the title type.
// For IOS title IDs, the type will always be 00000001 (system), and the lower 32 bits
// are used for the IOS major version -- which is what we want here.
u32 Kernel::GetVersion() const
{
  return static_cast<u32>(m_title_id);
}

std::shared_ptr<FS::FileSystem> Kernel::GetFS()
{
  return m_fs;
}

FSCore& Kernel::GetFSCore()
{
  return *m_fs_core;
}

std::shared_ptr<FSDevice> EmulationKernel::GetFSDevice()
{
  return std::static_pointer_cast<FSDevice>(m_device_map.at("/dev/fs"));
}

ESCore& Kernel::GetESCore()
{
  return *m_es_core;
}

std::shared_ptr<ESDevice> EmulationKernel::GetESDevice()
{
  return std::static_pointer_cast<ESDevice>(m_device_map.at("/dev/es"));
}

std::shared_ptr<WiiSockMan> EmulationKernel::GetSocketManager()
{
  return m_socket_manager;
}

// Since we don't have actual processes, we keep track of only the PPC's UID/GID.
// These functions roughly correspond to syscalls 0x2b, 0x2c, 0x2d, 0x2e (though only for the PPC).
void EmulationKernel::SetUidForPPC(u32 uid)
{
  m_ppc_uid = uid;
}

u32 EmulationKernel::GetUidForPPC() const
{
  return m_ppc_uid;
}

void EmulationKernel::SetGidForPPC(u16 gid)
{
  m_ppc_gid = gid;
}

u16 EmulationKernel::GetGidForPPC() const
{
  return m_ppc_gid;
}

static std::vector<u8> ReadBootContent(FSCore& fs, const std::string& path, size_t max_size,
                                       Ticks ticks = {})
{
  const auto fd = fs.Open(0, 0, path, FS::Mode::Read, {}, ticks);
  if (fd.Get() < 0)
    return {};

  const size_t file_size = fs.GetFileStatus(fd.Get(), ticks)->size;
  if (max_size != 0 && file_size > max_size)
    return {};

  std::vector<u8> buffer(file_size);
  if (!fs.Read(fd.Get(), buffer.data(), buffer.size(), ticks))
    return {};
  return buffer;
}

// This corresponds to syscall 0x41, which loads a binary from the NAND and bootstraps the PPC.
// Unlike 0x42, IOS will set up some constants in memory before booting the PPC.
bool EmulationKernel::BootstrapPPC(const std::string& boot_content_path)
{
  // Seeking and processing overhead is ignored as most time is spent reading from the NAND.
  u64 ticks = 0;

  const DolReader dol{ReadBootContent(GetFSCore(), boot_content_path, 0, &ticks)};

  if (!dol.IsValid())
    return false;

  if (!SetupMemory(m_system.GetMemory(), m_title_id, MemorySetupType::Full))
    return false;

  // Reset the PPC and pause its execution until we're ready.
  ResetAndPausePPC(m_system);

  if (dol.IsAncast())
    INFO_LOG_FMT(IOS, "BootstrapPPC: Loading ancast image");

  if (!dol.LoadIntoMemory(m_system))
    return false;

  INFO_LOG_FMT(IOS, "BootstrapPPC: {}", boot_content_path);
  m_system.GetCoreTiming().ScheduleEvent(ticks, s_event_finish_ppc_bootstrap, dol.IsAncast());
  return true;
}

struct ARMBinary final
{
  explicit ARMBinary(std::vector<u8>&& bytes) : m_bytes(std::move(bytes)) {}
  bool IsValid() const
  {
    // The header is at least 0x10.
    if (m_bytes.size() < 0x10)
      return false;
    return m_bytes.size() >= (GetHeaderSize() + GetElfOffset() + GetElfSize());
  }

  std::vector<u8> GetElf() const
  {
    const auto iterator = m_bytes.cbegin() + GetHeaderSize() + GetElfOffset();
    return std::vector<u8>(iterator, iterator + GetElfSize());
  }

  u32 GetHeaderSize() const { return Common::swap32(m_bytes.data()); }
  u32 GetElfOffset() const { return Common::swap32(m_bytes.data() + 0x4); }
  u32 GetElfSize() const { return Common::swap32(m_bytes.data() + 0x8); }

private:
  std::vector<u8> m_bytes;
};

static void FinishIOSBoot(Core::System& system, u64 ios_title_id)
{
  // Shut down the active IOS first before switching to the new one.
  s_ios.reset();
  s_ios = std::make_unique<EmulationKernel>(system, ios_title_id);
}

static constexpr SystemTimers::TimeBaseTick GetIOSBootTicks(u32 version)
{
  // Older IOS versions are monolithic so the main ELF is much larger and takes longer to load.
  if (version < 28)
    return 16'000'000_tbticks;
  return 2'600'000_tbticks;
}

// Similar to syscall 0x42 (ios_boot); this is used to change the current active IOS.
// IOS writes the new version to 0x3140 before restarting, but it does *not* poke any
// of the other constants to the memory. Warning: this resets the kernel instance.
//
// Passing a boot content path is optional because we do not require IOSes
// to be installed at the moment. If one is passed, the boot binary must exist
// on the NAND, or the call will fail like on a Wii.
bool EmulationKernel::BootIOS(const u64 ios_title_id, HangPPC hang_ppc,
                              const std::string& boot_content_path)
{
  // IOS suspends regular PPC<->ARM IPC before loading a new IOS.
  // IPC is not resumed if the boot fails for any reason.
  m_ipc_paused = true;

  if (!boot_content_path.empty())
  {
    // Load the ARM binary to memory (if possible).
    // Because we do not actually emulate the Starlet, only load the sections that are in MEM1.

    ARMBinary binary{ReadBootContent(GetFSCore(), boot_content_path, 0xB00000)};
    if (!binary.IsValid())
      return false;

    ElfReader elf{binary.GetElf()};
    if (!elf.LoadIntoMemory(m_system, true))
      return false;
  }

  if (hang_ppc == HangPPC::Yes)
    ResetAndPausePPC(m_system);

  if (Core::IsRunningAndStarted())
  {
    m_system.GetCoreTiming().ScheduleEvent(GetIOSBootTicks(GetVersion()), s_event_finish_ios_boot,
                                           ios_title_id);
  }
  else
  {
    FinishIOSBoot(m_system, ios_title_id);
  }

  return true;
}

void EmulationKernel::InitIPC()
{
  if (!Core::IsRunning())
    return;

  INFO_LOG_FMT(IOS, "IPC initialised.");
  m_system.GetWiiIPC().GenerateAck(0);
}

void EmulationKernel::AddDevice(std::unique_ptr<Device> device)
{
  ASSERT(device->GetDeviceType() == Device::DeviceType::Static);
  m_device_map.insert_or_assign(device->GetDeviceName(), std::move(device));
}

void EmulationKernel::AddStaticDevices()
{
  const Feature features = GetFeatures(GetVersion());

  // Dolphin-specific device for letting homebrew access and alter emulator state.
  AddDevice(std::make_unique<DolphinDevice>(*this, "/dev/dolphin"));

  // OH1 (Bluetooth)
  AddDevice(std::make_unique<DeviceStub>(*this, "/dev/usb/oh1"));
  if (!Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
    AddDevice(std::make_unique<BluetoothEmuDevice>(*this, "/dev/usb/oh1/57e/305"));
  else
    AddDevice(std::make_unique<BluetoothRealDevice>(*this, "/dev/usb/oh1/57e/305"));

  // Other core modules
  AddDevice(std::make_unique<STMImmediateDevice>(*this, "/dev/stm/immediate"));
  AddDevice(std::make_unique<STMEventHookDevice>(*this, "/dev/stm/eventhook"));
  AddDevice(std::make_unique<DIDevice>(*this, "/dev/di"));
  AddDevice(std::make_unique<SDIOSlot0Device>(*this, "/dev/sdio/slot0"));
  AddDevice(std::make_unique<DeviceStub>(*this, "/dev/sdio/slot1"));

  // Network modules
  if (HasFeature(features, Feature::KD) || HasFeature(features, Feature::SO) ||
      HasFeature(features, Feature::SSL))
  {
    m_socket_manager = std::make_shared<IOS::HLE::WiiSockMan>(*this);
  }
  if (HasFeature(features, Feature::KD))
  {
    constexpr auto time_device_name = "/dev/net/kd/time";
    AddDevice(std::make_unique<NetKDTimeDevice>(*this, time_device_name));
    const auto time_device =
        std::static_pointer_cast<NetKDTimeDevice>(GetDeviceByName(time_device_name));
    AddDevice(std::make_unique<NetKDRequestDevice>(*this, "/dev/net/kd/request", time_device));
  }
  if (HasFeature(features, Feature::NCD))
  {
    AddDevice(std::make_unique<NetNCDManageDevice>(*this, "/dev/net/ncd/manage"));
  }
  if (HasFeature(features, Feature::WiFi))
  {
    AddDevice(std::make_unique<NetWDCommandDevice>(*this, "/dev/net/wd/command"));
  }
  if (HasFeature(features, Feature::SO))
  {
    AddDevice(std::make_unique<NetIPTopDevice>(*this, "/dev/net/ip/top"));
  }
  if (HasFeature(features, Feature::SSL))
  {
    AddDevice(std::make_unique<NetSSLDevice>(*this, "/dev/net/ssl"));
  }

  // USB modules
  // OH0 is unconditionally added because this device path is registered in all cases.
  AddDevice(std::make_unique<OH0>(*this, "/dev/usb/oh0"));
  if (HasFeature(features, Feature::NewUSB))
  {
    AddDevice(std::make_unique<USB_HIDv5>(*this, "/dev/usb/hid"));
    AddDevice(std::make_unique<USB_VEN>(*this, "/dev/usb/ven"));

    // TODO(IOS): register /dev/usb/usb, /dev/usb/msc, /dev/usb/hub and /dev/usb/ehc
    //            as stubs that return IPC_EACCES.
  }
  else
  {
    if (HasFeature(features, Feature::USB_HIDv4))
      AddDevice(std::make_unique<USB_HIDv4>(*this, "/dev/usb/hid"));
    if (HasFeature(features, Feature::USB_KBD))
      AddDevice(std::make_unique<USB_KBD>(*this, "/dev/usb/kbd"));
  }

  if (HasFeature(features, Feature::WFS))
  {
    AddDevice(std::make_unique<WFSSRVDevice>(*this, "/dev/usb/wfssrv"));
    AddDevice(std::make_unique<WFSIDevice>(*this, "/dev/wfsi"));
  }
}

s32 EmulationKernel::GetFreeDeviceID()
{
  for (u32 i = 0; i < IPC_MAX_FDS; i++)
  {
    if (m_fdmap[i] == nullptr)
    {
      return i;
    }
  }

  return -1;
}

std::shared_ptr<Device> EmulationKernel::GetDeviceByName(std::string_view device_name)
{
  const auto iterator = m_device_map.find(device_name);
  return iterator != m_device_map.end() ? iterator->second : nullptr;
}

std::shared_ptr<Device> EmulationKernel::GetDeviceByFileDescriptor(const u32 fd)
{
  if (fd < IPC_MAX_FDS)
    return m_fdmap[fd];

  switch (fd)
  {
  case 0x10000:
    return GetDeviceByName("/dev/aes");
  case 0x10001:
    return GetDeviceByName("/dev/sha");
  default:
    return nullptr;
  }
}

// Returns the FD for the newly opened device (on success) or an error code.
std::optional<IPCReply> EmulationKernel::OpenDevice(OpenRequest& request)
{
  const s32 new_fd = GetFreeDeviceID();
  INFO_LOG_FMT(IOS, "Opening {} (mode {}, fd {})", request.path,
               Common::ToUnderlying(request.flags), new_fd);
  if (new_fd < 0 || new_fd >= IPC_MAX_FDS)
  {
    ERROR_LOG_FMT(IOS, "Couldn't get a free fd, too many open files");
    return IPCReply{IPC_EMAX, 5000_tbticks};
  }
  request.fd = new_fd;

  std::shared_ptr<Device> device;
  if (request.path.find("/dev/usb/oh0/") == 0 && !GetDeviceByName(request.path) &&
      !HasFeature(GetVersion(), Feature::NewUSB))
  {
    device = std::make_shared<OH0Device>(*this, request.path);
  }
  else if (request.path.find("/dev/") == 0)
  {
    device = GetDeviceByName(request.path);
  }
  else if (request.path.find('/') == 0)
  {
    device = GetDeviceByName("/dev/fs");
  }

  if (!device)
  {
    ERROR_LOG_FMT(IOS, "Unknown device: {}", request.path);
    return IPCReply{IPC_ENOENT, 3700_tbticks};
  }

  std::optional<IPCReply> result = device->Open(request);
  if (result && result->return_value >= IPC_SUCCESS)
  {
    m_fdmap[new_fd] = device;
    result->return_value = new_fd;
  }
  return result;
}

std::optional<IPCReply> EmulationKernel::HandleIPCCommand(const Request& request)
{
  if (request.command < IPC_CMD_OPEN || request.command > IPC_CMD_IOCTLV)
    return IPCReply{IPC_EINVAL, 978_tbticks};

  if (request.command == IPC_CMD_OPEN)
  {
    OpenRequest open_request{GetSystem(), request.address};
    return OpenDevice(open_request);
  }

  const auto device = GetDeviceByFileDescriptor(request.fd);
  if (!device)
    return IPCReply{IPC_EINVAL, 550_tbticks};

  std::optional<IPCReply> ret;
  const u64 wall_time_before = Common::Timer::NowUs();

  switch (request.command)
  {
  case IPC_CMD_CLOSE:
    // if the fd is not a special IOS FD, we need to reset it too
    if (request.fd < IPC_MAX_FDS)
      m_fdmap[request.fd].reset();
    ret = device->Close(request.fd);
    break;
  case IPC_CMD_READ:
    ret = device->Read(ReadWriteRequest{GetSystem(), request.address});
    break;
  case IPC_CMD_WRITE:
    ret = device->Write(ReadWriteRequest{GetSystem(), request.address});
    break;
  case IPC_CMD_SEEK:
    ret = device->Seek(SeekRequest{GetSystem(), request.address});
    break;
  case IPC_CMD_IOCTL:
    ret = device->IOCtl(IOCtlRequest{GetSystem(), request.address});
    break;
  case IPC_CMD_IOCTLV:
    ret = device->IOCtlV(IOCtlVRequest{GetSystem(), request.address});
    break;
  default:
    ASSERT_MSG(IOS, false, "Unexpected command: {:#x}", Common::ToUnderlying(request.command));
    ret = IPCReply{IPC_EINVAL, 978_tbticks};
    break;
  }

  const u64 wall_time_after = Common::Timer::NowUs();
  constexpr u64 BLOCKING_IPC_COMMAND_THRESHOLD_US = 2000;
  if (wall_time_after - wall_time_before > BLOCKING_IPC_COMMAND_THRESHOLD_US)
  {
    WARN_LOG_FMT(IOS, "Previous request to device {} blocked emulation for {} microseconds.",
                 device->GetDeviceName(), wall_time_after - wall_time_before);
  }

  return ret;
}

void EmulationKernel::ExecuteIPCCommand(const u32 address)
{
  Request request{GetSystem(), address};
  std::optional<IPCReply> result = HandleIPCCommand(request);

  if (!result)
    return;

  // Ensure replies happen in order
  auto& core_timing = GetSystem().GetCoreTiming();
  const s64 ticks_until_last_reply = m_last_reply_time - core_timing.GetTicks();
  if (ticks_until_last_reply > 0)
    result->reply_delay_ticks += ticks_until_last_reply;
  m_last_reply_time = core_timing.GetTicks() + result->reply_delay_ticks;

  EnqueueIPCReply(request, result->return_value, result->reply_delay_ticks);
}

// Happens AS SOON AS IPC gets a new pointer!
void EmulationKernel::EnqueueIPCRequest(u32 address)
{
  // Based on hardware tests, IOS takes between 5µs and 10µs to acknowledge an IPC request.
  // Console 1: 456 TB ticks before ACK
  // Console 2: 658 TB ticks before ACK
  GetSystem().GetCoreTiming().ScheduleEvent(500_tbticks, s_event_enqueue,
                                            address | ENQUEUE_REQUEST_FLAG);
}

// Called to send a reply to an IOS syscall
void EmulationKernel::EnqueueIPCReply(const Request& request, const s32 return_value,
                                      s64 cycles_in_future, CoreTiming::FromThread from)
{
  auto& system = GetSystem();
  auto& memory = system.GetMemory();
  memory.Write_U32(static_cast<u32>(return_value), request.address + 4);
  // IOS writes back the command that was responded to in the FD field.
  memory.Write_U32(request.command, request.address + 8);
  // IOS also overwrites the command type with the reply type.
  memory.Write_U32(IPC_REPLY, request.address);
  system.GetCoreTiming().ScheduleEvent(cycles_in_future, s_event_enqueue, request.address, from);
}

void EmulationKernel::HandleIPCEvent(u64 userdata)
{
  if (userdata & ENQUEUE_REQUEST_FLAG)
    m_request_queue.push_back(static_cast<u32>(userdata));
  else
    m_reply_queue.push_back(static_cast<u32>(userdata));

  UpdateIPC();
}

void EmulationKernel::UpdateIPC()
{
  auto& wii_ipc = m_system.GetWiiIPC();
  if (m_ipc_paused || !wii_ipc.IsReady())
    return;

  if (!m_request_queue.empty())
  {
    wii_ipc.ClearX1();
    wii_ipc.GenerateAck(m_request_queue.front());
    u32 command = m_request_queue.front();
    m_request_queue.pop_front();
    ExecuteIPCCommand(command);
    return;
  }

  if (!m_reply_queue.empty())
  {
    wii_ipc.GenerateReply(m_reply_queue.front());
    DEBUG_LOG_FMT(IOS, "<<-- Reply to IPC Request @ {:#010x}", m_reply_queue.front());
    m_reply_queue.pop_front();
    return;
  }
}

void EmulationKernel::UpdateDevices()
{
  // Check if a hardware device must be updated
  for (const auto& entry : m_device_map)
  {
    if (entry.second->IsOpened())
    {
      entry.second->Update();
    }
  }
}

void EmulationKernel::UpdateWantDeterminism(const bool new_want_determinism)
{
  if (m_socket_manager)
    m_socket_manager->UpdateWantDeterminism(new_want_determinism);
  for (const auto& device : m_device_map)
    device.second->UpdateWantDeterminism(new_want_determinism);
}

void EmulationKernel::DoState(PointerWrap& p)
{
  p.Do(m_request_queue);
  p.Do(m_reply_queue);
  p.Do(m_last_reply_time);
  p.Do(m_ipc_paused);
  p.Do(m_title_id);
  p.Do(m_ppc_uid);
  p.Do(m_ppc_gid);

  m_iosc.DoState(p);
  m_fs->DoState(p);

  if (m_title_id == Titles::MIOS)
    return;

  if (m_socket_manager)
    m_socket_manager->DoState(p);

  for (const auto& entry : m_device_map)
    entry.second->DoState(p);

  if (p.IsReadMode())
  {
    for (u32 i = 0; i < IPC_MAX_FDS; i++)
    {
      u32 exists = 0;
      p.Do(exists);
      if (exists)
      {
        auto device_type = Device::DeviceType::Static;
        p.Do(device_type);
        switch (device_type)
        {
        case Device::DeviceType::Static:
        {
          std::string device_name;
          p.Do(device_name);
          m_fdmap[i] = GetDeviceByName(device_name);
          break;
        }
        case Device::DeviceType::OH0:
          m_fdmap[i] = std::make_shared<OH0Device>(*this, "");
          m_fdmap[i]->DoState(p);
          break;
        }
      }
    }
  }
  else
  {
    for (auto& descriptor : m_fdmap)
    {
      u32 exists = descriptor ? 1 : 0;
      p.Do(exists);
      if (exists)
      {
        auto device_type = descriptor->GetDeviceType();
        p.Do(device_type);
        if (device_type == Device::Device::DeviceType::Static)
        {
          std::string device_name = descriptor->GetDeviceName();
          p.Do(device_name);
        }
        else
        {
          descriptor->DoState(p);
        }
      }
    }
  }
}

IOSC& Kernel::GetIOSC()
{
  return m_iosc;
}

static void FinishPPCBootstrap(Core::System& system, u64 userdata, s64 cycles_late)
{
  // See Kernel::BootstrapPPC
  const bool is_ancast = userdata == 1;
  if (is_ancast)
    ReleasePPCAncast(system);
  else
    ReleasePPC(system);

  ASSERT(Core::IsCPUThread());
  Core::CPUThreadGuard guard(system);
  SConfig::OnNewTitleLoad(guard);

  INFO_LOG_FMT(IOS, "Bootstrapping done.");
}

void Init(Core::System& system)
{
  auto& core_timing = system.GetCoreTiming();

  s_event_enqueue =
      core_timing.RegisterEvent("IPCEvent", [](Core::System& system_, u64 userdata, s64) {
        if (s_ios)
          s_ios->HandleIPCEvent(userdata);
      });

  ESDevice::InitializeEmulationState(core_timing);

  s_event_finish_ppc_bootstrap =
      core_timing.RegisterEvent("IOSFinishPPCBootstrap", FinishPPCBootstrap);

  s_event_finish_ios_boot = core_timing.RegisterEvent(
      "IOSFinishIOSBoot",
      [](Core::System& system_, u64 ios_title_id, s64) { FinishIOSBoot(system_, ios_title_id); });

  DIDevice::s_finish_executing_di_command =
      core_timing.RegisterEvent("FinishDICommand", DIDevice::FinishDICommandCallback);

  // Start with IOS80 to simulate part of the Wii boot process.
  s_ios = std::make_unique<EmulationKernel>(system, Titles::SYSTEM_MENU_IOS);
  // On a Wii, boot2 launches the system menu IOS, which then launches the system menu
  // (which bootstraps the PPC). Bootstrapping the PPC results in memory values being set up.
  // This means that the constants in the 0x3100 region are always set up by the time
  // a game is launched. This is necessary because booting games from the game list skips
  // a significant part of a Wii's boot process.
  SetupMemory(system.GetMemory(), Titles::SYSTEM_MENU_IOS, MemorySetupType::Full);
}

void Shutdown()
{
  s_ios.reset();
  ESDevice::FinalizeEmulationState();
}

EmulationKernel* GetIOS()
{
  return s_ios.get();
}

// Based on a hardware test, a device takes at least ~2700 ticks to reply to an IPC request.
// Depending on how much work a command performs, this can take much longer (10000+)
// especially if the NAND filesystem is accessed.
//
// Because we currently don't emulate timing very accurately, we should not return
// the minimum possible reply time (~960 ticks from the kernel or ~2700 from devices)
// but an average value, otherwise we are going to be much too fast in most cases.
IPCReply::IPCReply(s32 return_value_) : IPCReply(return_value_, 4000_tbticks)
{
}

IPCReply::IPCReply(s32 return_value_, u64 reply_delay_ticks_)
    : return_value(return_value_), reply_delay_ticks(reply_delay_ticks_)
{
}
}  // namespace IOS::HLE
