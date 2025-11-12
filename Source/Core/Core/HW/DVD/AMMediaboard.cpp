// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/AMMediaboard.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include "Common/Buffer.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HLE/HLE.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_DeviceBaseboard.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/Movie.h"
#include "Core/System.h"

#include "DiscIO/DirectoryBlob.h"

#if defined(__linux__) or defined(__APPLE__) or defined(__FreeBSD__) or defined(__NetBSD__) or     \
    defined(__HAIKU__)

#include <unistd.h>

static constexpr auto* closesocket = close;
static auto ioctlsocket(auto... args)
{
  return ioctl(args...);
}

static constexpr int WSAEWOULDBLOCK = 10035;
static constexpr int SOCKET_ERROR = -1;

using SOCKET = int;

static constexpr SOCKET INVALID_SOCKET = SOCKET(~0);

static int WSAGetLastError()
{
  switch (errno)
  {
  case EINPROGRESS:
  case EWOULDBLOCK:
    return WSAEWOULDBLOCK;
  default:
    break;
  }

  return errno;
}

#endif

namespace AMMediaboard
{

static bool s_firmware_map = false;
static bool s_test_menu = false;
static SOCKET s_fd_namco_cam = 0;
static std::array<u32, 3> s_timeouts = {20000, 20000, 20000};
static u32 s_last_error = SSC_SUCCESS;

static u32 s_gcam_key_a = 0;
static u32 s_gcam_key_b = 0;
static u32 s_gcam_key_c = 0;

static File::IOFile s_netcfg;
static File::IOFile s_netctrl;
static File::IOFile s_extra;
static File::IOFile s_backup;
static File::IOFile s_dimm;

static Common::UniqueBuffer<u8> s_dimm_disc;

static u8 s_firmware[2 * 1024 * 1024];
static u32 s_media_buffer_32[192];
static u8* const s_media_buffer = reinterpret_cast<u8*>(s_media_buffer_32);
static u8 s_network_command_buffer[0x4FFE00];
static u8 s_network_buffer[512 * 1024];
static u8 s_allnet_buffer[4096];
static u8 s_allnet_settings[0x8500];

constexpr char s_allnet_reply[] = {
    "uri=http://"
    "sega.com&host=sega.com&nickname=sega&name=sega&year=2025&month=08&day=16&hour=21&minute=10&"
    "second=12&place_id=1234&setting=0x123&region0=jap&region_name0=japan&region_name1=usa&region_"
    "name2=asia&region_name3=export&end"};

static const MediaBoardRanges s_mediaboard_ranges[] = {
    {DIMMCommandVersion1, 0x1F900040, s_media_buffer, sizeof(s_media_buffer_32),
     DIMMCommandVersion1},
    {DIMMCommandVersion2, 0x84000060, s_media_buffer, sizeof(s_media_buffer_32),
     DIMMCommandVersion2},
    {DIMMCommandVersion2_2, 0x89000220, s_media_buffer, sizeof(s_media_buffer_32),
     DIMMCommandVersion2_2},
    {NetworkCommandAddress1, NetworkBufferAddress2, s_network_command_buffer,
     sizeof(s_network_command_buffer), NetworkCommandAddress1},
    {NetworkCommandAddress2, 0x89060200, s_network_command_buffer, sizeof(s_network_command_buffer),
     NetworkCommandAddress2},
    {NetworkBufferAddress1, 0x1FA10000, s_network_buffer, sizeof(s_network_buffer),
     NetworkBufferAddress1},
    {NetworkBufferAddress2, 0x1FD10000, s_network_buffer, sizeof(s_network_buffer),
     NetworkBufferAddress2},
    {NetworkBufferAddress3, 0x89110000, s_network_buffer, sizeof(s_network_buffer),
     NetworkBufferAddress3},
    {NetworkBufferAddress4, 0x89240000, s_network_buffer, sizeof(s_network_buffer),
     NetworkBufferAddress4},
    {NetworkBufferAddress5, 0x1FB10000, s_network_buffer, sizeof(s_network_buffer),
     NetworkBufferAddress5},
    {AllNetSettings, 0x1F000000, s_allnet_settings, sizeof(s_allnet_settings), AllNetSettings},
    {AllNetBuffer, 0x89011000, s_allnet_buffer, sizeof(s_allnet_buffer), AllNetBuffer},
};

static const std::unordered_map<u16, GameType> s_game_map = {
    {0x4747, FZeroAX},
    {0x4841, FZeroAXMonster},
    {0x4B50, MarioKartGP},
    {0x4B5A, MarioKartGP},
    {0x4E4A, MarioKartGP2},
    {0x4E4C, MarioKartGP2},
    {0x454A, VirtuaStriker3},
    {0x4559, VirtuaStriker3},
    {0x4C4A, VirtuaStriker4_2006},
    {0x4C4B, VirtuaStriker4_2006},
    {0x4C4C, VirtuaStriker4_2006},
    {0x484A, VirtuaStriker4},
    {0x484E, VirtuaStriker4},
    {0x485A, VirtuaStriker4},
    {0x4A41, VirtuaStriker4},
    {0x4A4A, VirtuaStriker4},
    {0x4658, KeyOfAvalon},
    {0x4A4E, KeyOfAvalon},
    {0x4758, GekitouProYakyuu},
    {0x5342, VirtuaStriker3},
    {0x3132, VirtuaStriker3},
    {0x454C, VirtuaStriker3},
    {0x3030, FirmwareUpdate},
};
// Sockets FDs are required to go from 0 to 63.
// Games use the FD as indexes so we have to workaround it.

static SOCKET s_sockets[64];

static u32 SocketCheck(u32 x)
{
  if (x < std::size(s_sockets))
    return x;

  WARN_LOG_FMT(AMMEDIABOARD, "GC-AM: Bad SOCKET value: {}", x);
  return 0;
}

static bool NetworkCMDBufferCheck(u32 offset, u32 length)
{
  if (offset <= std::size(s_network_command_buffer) &&
      length <= std::size(s_network_command_buffer) - offset)
  {
    return true;
  }

  ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid command buffer range: offset={}, length={}", offset,
                length);
  return false;
}

static bool NetworkBufferCheck(u32 offset, u32 length)
{
  if (offset <= std::size(s_network_buffer) && length <= std::size(s_network_buffer) - offset)
  {
    return true;
  }

  ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid network buffer range: offset={}, length={}", offset,
                length);
  return false;
}

static bool SafeCopyToEmu(Memory::MemoryManager& memory, u32 address, const u8* source,
                          u64 source_size, u32 offset, u32 length)
{
  if (offset > source_size || length > source_size - offset)
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Read overflow: offset=0x{:08x}, length={}, source_size={}",
                  offset, length, source_size);
    return false;
  }

  auto span = memory.GetSpanForAddress(address);
  if (length > span.size())
  {
    ERROR_LOG_FMT(AMMEDIABOARD,
                  "GC-AM: Memory buffer too small: address=0x{:08x}, length={}, span={}", address,
                  length, span.size());
    return false;
  }

  memory.CopyToEmu(address, source + offset, length);
  return true;
}
static bool SafeCopyFromEmu(Memory::MemoryManager& memory, u8* destionation, u32 address,
                            u64 destionation_size, u32 offset, u32 length)
{
  if (offset > destionation_size || length > destionation_size - offset)
  {
    ERROR_LOG_FMT(AMMEDIABOARD,
                  "GC-AM: Write overflow: offset=0x{:08x}, length={}, destionation_size={}", offset,
                  length, destionation_size);
    return false;
  }

  auto span = memory.GetSpanForAddress(address);
  if (length > span.size())
  {
    ERROR_LOG_FMT(AMMEDIABOARD,
                  "GC-AM: Memory buffer too small: address=0x{:08x}, length={}, span={}", address,
                  length, span.size());
    return false;
  }

  memory.CopyFromEmu(destionation + offset, address, length);
  return true;
}

static SOCKET socket_(int af, int type, int protocol)
{
  for (u32 i = 1; i < 64; ++i)
  {
    if (s_sockets[i] == SOCKET_ERROR)
    {
      s_sockets[i] = socket(af, type, protocol);
      return i;
    }
  }

  // Out of sockets
  return SOCKET_ERROR;
}

static SOCKET accept_(int fd, sockaddr* addr, socklen_t* len)
{
  for (u32 i = 1; i < 64; ++i)
  {
    if (s_sockets[i] == SOCKET_ERROR)
    {
      s_sockets[i] = accept(fd, addr, len);
      if (s_sockets[i] == SOCKET_ERROR)
        return SOCKET_ERROR;
      return i;
    }
  }

  // Out of sockets
  return SOCKET_ERROR;
}

static inline void PrintMBBuffer(u32 address, u32 length)
{
  const auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  for (u32 i = 0; i < length; i += 0x10)
  {
    INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: {:08x} {:08x} {:08x} {:08x}", memory.Read_U32(address + i),
                 memory.Read_U32(address + i + 4), memory.Read_U32(address + i + 8),
                 memory.Read_U32(address + i + 12));
  }
}

void FirmwareMap(bool on)
{
  s_firmware_map = on;
}

void InitKeys(u32 key_a, u32 key_b, u32 key_c)
{
  s_gcam_key_a = key_a;
  s_gcam_key_b = key_b;
  s_gcam_key_c = key_c;
}

static File::IOFile OpenOrCreateFile(const std::string& filename)
{
  // Try opening for read/write first
  if (File::Exists(filename))
    return File::IOFile(filename, "rb+");

  // Create new file
  return File::IOFile(filename, "wb+");
}

void Init()
{
  std::ranges::fill(s_media_buffer_32, 0);
  std::ranges::fill(s_network_buffer, 0);
  std::ranges::fill(s_network_command_buffer, 0);
  std::ranges::fill(s_firmware, -1);
  std::ranges::fill(s_sockets, SOCKET_ERROR);
  std::ranges::fill(s_allnet_buffer, 0);
  std::ranges::fill(s_allnet_settings, 0);

  s_firmware_map = false;
  s_test_menu = false;

  s_last_error = SSC_SUCCESS;

  s_gcam_key_a = 0;
  s_gcam_key_b = 0;
  s_gcam_key_c = 0;

  const std::string base_path = File::GetUserPath(D_TRIUSER_IDX);

  s_netcfg = OpenOrCreateFile(base_path + "trinetcfg.bin");
  s_netctrl = OpenOrCreateFile(base_path + "trinetctrl.bin");
  s_extra = OpenOrCreateFile(base_path + "triextra.bin");
  s_dimm = OpenOrCreateFile(base_path + "tridimm_" + SConfig::GetInstance().GetGameID() + ".bin");
  s_backup = OpenOrCreateFile(base_path + "backup_" + SConfig::GetInstance().GetGameID() + ".bin");

  if (!s_netcfg.IsOpen())
    PanicAlertFmt("Failed to open/create: {}", base_path + "trinetcfg.bin");
  if (!s_netctrl.IsOpen())
    PanicAlertFmt("Failed to open/create: {}", base_path + "trinetctrl.bin");
  if (!s_extra.IsOpen())
    PanicAlertFmt("Failed to open/create: {}", base_path + "triextra.bin");
  if (!s_dimm.IsOpen())
    PanicAlertFmt("Failed to open/create: {}", base_path + "tridimm.bin");
  if (!s_backup.IsOpen())
    PanicAlertFmt("Failed to open/create: {}", base_path + "backup.bin");

  // This is the firmware for the Triforce
  const std::string sega_boot_filename = base_path + "segaboot.gcm";

  if (!File::Exists(sega_boot_filename))
  {
    PanicAlertFmt("Failed to open segaboot.gcm({}), which is required for test menus.",
                  sega_boot_filename.c_str());
    return;
  }

  File::IOFile sega_boot(sega_boot_filename, "rb+");
  if (!sega_boot.IsOpen())
  {
    PanicAlertFmt("Failed to read: {}", sega_boot_filename);
    return;
  }

  const u64 length = std::min<u64>(sega_boot.GetSize(), sizeof(s_firmware));
  sega_boot.ReadBytes(s_firmware, length);

  s_test_menu = true;
}

u8* InitDIMM(u32 size)
{
  if (size == 0)
    return nullptr;

  if (s_dimm_disc.empty())
  {
    s_dimm_disc.reset(size);
    if (s_dimm_disc.empty())
    {
      PanicAlertFmt("Failed to allocate DIMM memory.");
      return nullptr;
    }
  }

  s_firmware_map = false;
  return s_dimm_disc.data();
}

static s32 NetDIMMAccept(int fd, sockaddr* addr, socklen_t* len)
{
  SOCKET client_sock = INVALID_SOCKET;
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  timeval timeout{
      .tv_sec = 0,
      .tv_usec = 10000,  // 10 milliseconds
  };

  const int result = select(fd + 1, &readfds, nullptr, nullptr, &timeout);
  if (result > 0 && FD_ISSET(fd, &readfds))
  {
    client_sock = accept_(fd, addr, len);
    if (client_sock != INVALID_SOCKET)
    {
      s_last_error = SSC_SUCCESS;
      return client_sock;
    }

    s_last_error = SOCKET_ERROR;
    return SOCKET_ERROR;
  }

  if (result == 0)
  {
    // Timeout
    s_last_error = SSC_EWOULDBLOCK;
  }
  else
  {
    // select() failed
    s_last_error = SOCKET_ERROR;
  }

  return SOCKET_ERROR;
}

static s32 NetDIMMConnect(int fd, sockaddr_in* addr, int len)
{
  // All.Net Connect IP
  if (addr->sin_addr.s_addr == inet_addr("192.168.150.16"))
  {
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");
  }

  // CyCraft Connect IP
  if (addr->sin_addr.s_addr == inet_addr("192.168.11.111"))
  {
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");
  }

  // NAMCO Camera ( IPs are: 192.168.29.104-108 )
  if ((addr->sin_addr.s_addr & 0xFFFFFF00) == 0xC0A81D00)
  {
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");

    // BUG: An invalid family value is being used
    addr->sin_family = htons(AF_INET);
    s_fd_namco_cam = fd;
  }

  // Key of Avalon Client
  if (addr->sin_addr.s_addr == inet_addr("192.168.13.1"))
  {
    addr->sin_addr.s_addr = inet_addr("10.0.0.45");
  }

  addr->sin_family = Common::swap16(addr->sin_family);

  u_long val = 1;
  // Set socket to non-blocking
  ioctlsocket(fd, FIONBIO, &val);

  int ret = connect(fd, reinterpret_cast<const sockaddr*>(addr), len);
  const int err = WSAGetLastError();

  if (ret == SOCKET_ERROR && err == WSAEWOULDBLOCK)
  {
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);

    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = s_timeouts[0];

    ret = select(fd + 1, nullptr, &writefds, nullptr, &timeout);
    if (ret > 0 && FD_ISSET(fd, &writefds))
    {
      int so_error = 0;
      socklen_t optlen = sizeof(so_error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &optlen) == 0 &&
          so_error == 0)
      {
        s_last_error = SSC_SUCCESS;
        ret = 0;
      }
      else
      {
        s_last_error = SOCKET_ERROR;
        ret = SOCKET_ERROR;
      }
    }
    else if (ret == 0)
    {
      // Timeout
      s_last_error = SSC_EWOULDBLOCK;
      ret = SOCKET_ERROR;
    }
    else
    {
      // select() failed
      s_last_error = SOCKET_ERROR;
      ret = SOCKET_ERROR;
    }
  }
  else if (ret == SOCKET_ERROR)
  {
    // Immediate failure (e.g. WSAECONNREFUSED)
    s_last_error = ret;
  }
  else
  {
    s_last_error = SSC_SUCCESS;
  }

  // Restore blocking mode
  val = 0;
  ioctlsocket(fd, FIONBIO, &val);

  return ret;
}

static void FileWriteData(Memory::MemoryManager& memory, File::IOFile* file, u32 seek_pos,
                          u32 address, std::size_t length)
{
  auto span = memory.GetSpanForAddress(address);
  if (length <= span.size())
  {
    file->Seek(seek_pos, File::SeekOrigin::Begin);
    file->WriteBytes(span.data(), length);
    file->Flush();
  }
  else
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Write overflow: address=0x{:08x}, length={}, span={}",
                  address, length, span.size());
  }
}
static void FileReadData(Memory::MemoryManager& memory, File::IOFile* file, u32 seek_pos,
                         u32 address, std::size_t length)
{
  auto span = memory.GetSpanForAddress(address);
  if (length <= span.size())
  {
    file->Seek(seek_pos, File::SeekOrigin::Begin);
    file->ReadBytes(span.data(), length);
  }
  else
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Read overflow: address=0x{:08x}, length={}, span={}",
                  address, length, span.size());
  }
}

u32 ExecuteCommand(std::array<u32, 3>& dicmd_buf, u32* diimm_buf, u32 address, u32 length)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  dicmd_buf[0] ^= s_gcam_key_a;
  dicmd_buf[1] ^= s_gcam_key_b;
  dicmd_buf[2] ^= s_gcam_key_c;

  const u32 seed = dicmd_buf[0] >> 16;

  s_gcam_key_a *= seed;
  s_gcam_key_b *= seed;
  s_gcam_key_c *= seed;

  // Key setup for Triforce IPL:
  // These RAM offsets always hold the key for the next command. Since two dummy commands are sent
  // before any real ones, you can just use the key from RAM without missing a real command.

  if (s_gcam_key_a == 0)
  {
    if (memory.Read_U32(0))
    {
      HLE::Patch(system, 0x813048B8, "OSReport");
      HLE::Patch(system, 0x8130095C, "OSReport");  // Apploader

      InitKeys(memory.Read_U32(0), memory.Read_U32(4), memory.Read_U32(8));
    }
  }

  const u32 command = dicmd_buf[0] << 24;
  const u32 offset = dicmd_buf[1] << 2;

  DEBUG_LOG_FMT(AMMEDIABOARD,
                "GC-AM: {:08x} {:08x} DMA=addr:{:08x},len:{:08x} Keys: {:08x} {:08x} {:08x}",
                command, offset, address, length, s_gcam_key_a, s_gcam_key_b, s_gcam_key_c);

  // Test mode
  if (offset == 0x0002440)
  {
    // Set by OSResetSystem
    if (memory.Read_U32(0x811FFF00) == 1)
    {
      // Don't map firmware while in SegaBoot
      if (memory.Read_U32(0x8006BF70) != 0x0A536567)
      {
        s_firmware_map = true;
      }
    }
  }

  switch (AMMBDICommand(command >> 24))
  {
  case AMMBDICommand::Inquiry:
    if (s_firmware_map)
    {
      s_firmware_map = false;
    }

    // Returned value is used to set the protocol version.
    switch (GetGameType())
    {
    default:
      *diimm_buf = Version1;
      return 0;
    case VirtuaStriker4_2006:
    case KeyOfAvalon:
    case MarioKartGP:
    case MarioKartGP2:
    case FirmwareUpdate:
      *diimm_buf = Version2;
      return 0;
    }
    break;
  case AMMBDICommand::Read:
    if ((offset & 0x8FFF0000) == 0x80000000)
    {
      switch (offset)
      {
      case MediaBoardStatus1:
        memory.Write_U16(1, address);
        break;
      case MediaBoardStatus2:
        memory.Memset(address, 0, length);
        break;
      case MediaBoardStatus3:
        memory.Memset(address, 0xFF, length);
        // DIMM size (512MB)
        memory.Write_U32_Swap(0x20000000, address);
        // GCAM signature
        memory.Write_U32(0x4743414D, address + 4);
        break;
      case 0x80000100:
        memory.Write_U32_Swap(0x001F1F1F, address);
        break;
      case FirmwareStatus1:
        memory.Write_U32_Swap(0x01FA, address);
        break;
      case FirmwareStatus2:
        memory.Write_U32_Swap(1, address);
        break;
      case 0x80000160:
        memory.Write_U32(0x00001E00, address);
        break;
      case 0x80000180:
        memory.Write_U32(0, address);
        break;
      case 0x800001A0:
        memory.Write_U32(0xFFFFFFFF, address);
        break;
      default:
        PrintMBBuffer(address, length);
        PanicAlertFmtT("Unhandled Media Board Read:{0:08x}", offset);
        break;
      }
      return 0;
    }

    // Network configuration
    if (offset == 0x00000000 && length == 0x80)
    {
      FileReadData(memory, &s_netcfg, 0, address, length);
      return 0;
    }

    // Media crc check on/off
    if (offset == DIMMExtraSettings && length == 0x20)
    {
      FileReadData(memory, &s_extra, 0, address, length);
      return 0;
    }

    // DIMM memory (8MB)
    if (offset >= DIMMMemory && offset < 0x1F800000)
    {
      FileReadData(memory, &s_dimm, offset - DIMMMemory, address, length);
      return 0;
    }

    // DIMM memory (8MB)
    if (offset >= DIMMMemory2 && offset < 0xFF800000)
    {
      FileReadData(memory, &s_dimm, offset - DIMMMemory2, address, length);
      return 0;
    }

    if (offset == NetworkControl && length == 0x20)
    {
      FileReadData(memory, &s_netctrl, 0, address, length);
      return 0;
    }

    if (offset >= AllNetBuffer && offset < 0x89011000)
    {
      INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: Read All.Net Buffer ({:08x},{})", offset, length);
      // Fake reply
      SafeCopyToEmu(memory, address, (u8*)s_allnet_reply, sizeof(s_allnet_reply),
                    offset - AllNetBuffer, sizeof(s_allnet_reply));
      return 0;
    }

    for (const auto& range : s_mediaboard_ranges)
    {
      if (offset >= range.start && offset < range.end)
      {
        INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: Read MediaBoard ({:08x},{:08x},{:08x})", offset,
                     range.base_offset, length);
        SafeCopyToEmu(memory, address, range.buffer, range.buffer_size, offset - range.base_offset,
                      length);
        PrintMBBuffer(address, length);
        return 0;
      }
    }

    if (offset == DIMMCommandExecute2)
    {
      const AMMBCommand ammb_command = Common::BitCastPtr<AMMBCommand>(s_media_buffer + 0x202);

      INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: Execute command: (2){0:04X}",
                   static_cast<u16>(ammb_command));

      memcpy(s_media_buffer, s_media_buffer + 0x200, 0x20);
      memset(s_media_buffer + 0x200, 0, 0x20);
      s_media_buffer[0x204] = 1;

      switch (ammb_command)
      {
      case AMMBCommand::Unknown_001:
        s_media_buffer_32[1] = 1;
        break;
      case AMMBCommand::GetNetworkFirmVersion:
        s_media_buffer_32[1] = 0x1305;  // Version: 13.05
        s_media_buffer[6] = 1;          // Type: VxWorks
        break;
      case AMMBCommand::GetSystemFlags:
        s_media_buffer[4] = 1;
        s_media_buffer[6] = NANDMaskBoardNAND;
        s_media_buffer[7] = 1;
        break;
      // Empty reply
      case AMMBCommand::Unknown_103:
        break;
      case AMMBCommand::Unknown_104:
        s_media_buffer[4] = 1;
        break;
      case AMMBCommand::Accept:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        int ret = -1;
        sockaddr addr;
        socklen_t len = 0;

        // Handle optional parameters
        if (s_media_buffer_32[3] == 0 || s_media_buffer_32[4] == 0)
        {
          ret = NetDIMMAccept(fd, nullptr, nullptr);
        }
        else
        {
          const u32 addr_off = s_media_buffer_32[3] - NetworkCommandAddress2;
          const u32 len_off = s_media_buffer_32[4] - NetworkCommandAddress2;

          if (!NetworkCMDBufferCheck(addr_off, sizeof(sockaddr)) ||
              !NetworkCMDBufferCheck(len_off, sizeof(u32)))
          {
            break;
          }

          ret = NetDIMMAccept(fd, &addr, &len);
          if (len)
          {
            memcpy((s_network_command_buffer + addr_off), &addr, len);
            memcpy((s_network_command_buffer + len_off), &len, sizeof(int));
          }
        }

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: accept( {}({}) ):{}\n", fd, s_media_buffer_32[2],
                       ret);
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Bind:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        const u32 off = s_media_buffer_32[3] - NetworkCommandAddress2;
        const u32 len = s_media_buffer_32[4];

        if (!NetworkCMDBufferCheck(off, len))
        {
          break;
        }

        sockaddr_in addr;
        memcpy(&addr, s_network_command_buffer + off, sizeof(sockaddr_in));

        addr.sin_family = Common::swap16(addr.sin_family);

        // Triforce titles typically rely on hardcoded IP addresses.
        // This behavior has been modified to bind to the wildcard address instead.
        //
        // addr.sin_addr.s_addr = htonl(addr.sin_addr.s_addr);

        addr.sin_addr.s_addr = INADDR_ANY;

        const int ret = bind(fd, reinterpret_cast<const sockaddr*>(&addr), len);
        const int err = WSAGetLastError();

        if (ret < 0)
          PanicAlertFmt("Socket Bind Failed with{0}", err);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: bind( {}, ({},{:08x}:{}), {} ):{} ({})\n", fd,
                       addr.sin_family, addr.sin_addr.s_addr, Common::swap16(addr.sin_port), len,
                       ret, err);

        s_media_buffer_32[1] = ret;
        s_last_error = SSC_SUCCESS;
        break;
      }
      case AMMBCommand::Closesocket:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];

        const int ret = closesocket(fd);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: closesocket( {}({}) ):{}\n", fd,
                       s_media_buffer_32[2], ret);

        s_sockets[SocketCheck(s_media_buffer_32[2])] = SOCKET_ERROR;

        s_media_buffer_32[1] = ret;
        s_last_error = SSC_SUCCESS;
        break;
      }
      case AMMBCommand::Connect:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        const u32 off = s_media_buffer_32[3] - NetworkCommandAddress2;
        const u32 len = s_media_buffer_32[4];

        if (!NetworkCMDBufferCheck(off, len))
        {
          break;
        }

        sockaddr_in addr;
        memcpy(&addr, s_network_command_buffer + off, sizeof(sockaddr_in));

        const int ret = NetDIMMConnect(fd, &addr, len);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: connect( {}({}), ({},{}:{}), {} ):{}\n", fd,
                       s_media_buffer_32[2], addr.sin_family, inet_ntoa(addr.sin_addr),
                       Common::swap16(addr.sin_port), len, ret);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::InetAddr:
      {
        const char* ip_address = reinterpret_cast<char*>(s_network_command_buffer);

        // IP address shouldn't be longer than 15
        if (strnlen(ip_address, 15) > 15)
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: Invalid size for address: InetAddr():{}\n",
                        strlen(ip_address));
          break;
        }

        const u32 ip = inet_addr(ip_address);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: InetAddr( {} )\n", ip_address);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = Common::swap32(ip);
        break;
      }
      case AMMBCommand::Listen:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        const u32 backlog = s_media_buffer_32[3];

        const int ret = listen(fd, backlog);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: listen( {}, {} ):{:d}\n", fd, backlog, ret);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Recv:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        u32 off = s_media_buffer_32[3];
        auto len = std::min<u32>(s_media_buffer_32[4], sizeof(s_network_buffer));

        if (off >= NetworkBufferAddress4 &&
            off + len <= NetworkBufferAddress4 + sizeof(s_network_buffer))
        {
          off -= NetworkBufferAddress4;
        }
        else if (off + len > sizeof(s_network_buffer))
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET,
                        "GC-AM: recv(error) invalid destination or length: off={:08x}, len={}\n",
                        off, len);
          off = 0;
          len = 0;
        }

        int ret = recv(fd, reinterpret_cast<char*>(s_network_buffer + off), len, 0);
        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: recv( {}, 0x{:08x}, {} ):{} {}\n", fd, off, len,
                       ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Send:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        u32 off = s_media_buffer_32[3];
        auto len = std::min<u32>(s_media_buffer_32[4], sizeof(s_network_buffer));

        if (off >= NetworkBufferAddress3 &&
            off + len <= NetworkBufferAddress3 + sizeof(s_network_buffer))
        {
          off -= NetworkBufferAddress3;
        }
        else if (off + len > sizeof(s_network_buffer))
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET,
                        "GC-AM: send(error) unhandled destination or length: {:08x}, len={}", off,
                        len);
          off = 0;
          len = 0;
        }

        const int ret = send(fd, reinterpret_cast<char*>(s_network_buffer + off), len, 0);
        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: send( {}({}), 0x{:08x}, {} ): {} {}\n", fd,
                       s_media_buffer_32[2], off, len, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Socket:
      {
        // Protocol is not sent
        const u32 af = s_media_buffer_32[2];
        const u32 type = s_media_buffer_32[3];

        const SOCKET fd = socket_(af, type, IPPROTO_TCP);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: socket( {}, {}, IPPROTO_TCP ):{}\n", af, type, fd);

        s_media_buffer[1] = 0;
        s_media_buffer_32[1] = fd;
        break;
      }
      case AMMBCommand::Select:
      {
        SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2] - 1)];

        // BUG: NAMCAM is hardcoded to call this with socket ID 0x100 which might be some magic
        // thing? A valid value is needed so we use the socket from the connect.

        if (AMMediaboard::GetGameType() == MarioKartGP ||
            AMMediaboard::GetGameType() == MarioKartGP2)
        {
          if (s_media_buffer_32[2] == 256)
          {
            fd = s_fd_namco_cam;
          }
        }

        fd_set* readfds = nullptr;
        fd_set* writefds = nullptr;
        fd_set* exceptfds = nullptr;

        timeval timeout = {};
        u8* timeout_src = nullptr;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        // Only one of 3, 4, 5 is ever set alongside 6
        if (s_media_buffer_32[6] != 0)
        {
          const u32 fd_set_offset = s_media_buffer_32[6] - NetworkCommandAddress2;
          if (!NetworkCMDBufferCheck(fd_set_offset, sizeof(fd_set)))
          {
            ERROR_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: Select(error) unhandled destination:{:08x}\n",
                          s_media_buffer_32[6]);
            break;
          }

          Common::BitCastPtr<fd_set>(s_network_command_buffer + fd_set_offset) = fds;

          if (s_media_buffer_32[3] != 0)
          {
            readfds = &fds;
            timeout_src = s_network_command_buffer + s_media_buffer_32[3] - NetworkCommandAddress2;
          }
          else if (s_media_buffer_32[4] != 0)
          {
            writefds = &fds;
            timeout_src = s_network_command_buffer + s_media_buffer_32[4] - NetworkCommandAddress2;
          }
          else if (s_media_buffer_32[5] != 0)
          {
            exceptfds = &fds;
            timeout_src = s_network_command_buffer + s_media_buffer_32[5] - NetworkCommandAddress2;
          }
        }

        // Copy timeout if set
        if (timeout_src != nullptr)
        {
          std::memcpy(&timeout, timeout_src, sizeof(timeval));
        }

        // BUG: The game sets timeout to two seconds
        if (AMMediaboard::GetGameType() == KeyOfAvalon)
        {
          timeout.tv_sec = 0;
          timeout.tv_usec = 1800;
        }

        const int ret =
            select(fd + 1, readfds, writefds, exceptfds, timeout_src ? &timeout : nullptr);

        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(
            AMMEDIABOARD_NET,
            "GC-AM: select( {}({}), 0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x} ):{} {} {}:{} \n", fd,
            s_media_buffer_32[2], s_media_buffer_32[3], s_media_buffer_32[4], s_media_buffer_32[5],
            s_media_buffer_32[6], ret, err, timeout.tv_sec, timeout.tv_usec);
        // hexdump( s_network_command_buffer, 0x40 );

        s_media_buffer[1] = 0;
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::SetSockOpt:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        const int level = static_cast<int>(s_media_buffer_32[3]);
        const int optname = static_cast<int>(s_media_buffer_32[4]);
        const int optlen = static_cast<int>(s_media_buffer_32[6]);

        if (!NetworkCMDBufferCheck(s_media_buffer_32[5] - NetworkCommandAddress2, optlen))
        {
          break;
        }

        const char* optval = reinterpret_cast<char*>(s_network_command_buffer +
                                                     s_media_buffer_32[5] - NetworkCommandAddress2);

        const int ret = setsockopt(fd, level, optname, optval, optlen);

        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET,
                       "GC-AM: setsockopt( {:d}, {:04x}, {}, {:p}, {} ):{:d} ({})\n", fd, level,
                       optname, optval, optlen, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::SetTimeOuts:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];
        const u32 timeout_a = s_media_buffer_32[3];
        const u32 timeout_b = s_media_buffer_32[4];
        const u32 timeout_c = s_media_buffer_32[5];

        s_timeouts[0] = timeout_a;
        s_timeouts[1] = timeout_b;
        s_timeouts[2] = timeout_c;

        int ret = SOCKET_ERROR;

        if (fd != INVALID_SOCKET)
        {
          ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_b),
                           sizeof(int));
          if (ret < 0)
          {
            ret = WSAGetLastError();
          }
          else
          {
            ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_c),
                             sizeof(int));
            if (ret < 0)
              ret = WSAGetLastError();
          }

          NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: SetTimeOuts( {:d}, {}, {}, {} ):{}\n", fd,
                         timeout_a, timeout_b, timeout_c, ret);
        }
        else
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: Invalid Socket: SetTimeOuts( {}, {}, {} ):{}\n",
                        timeout_a, timeout_b, timeout_c, ret);
        }

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::GetParambyDHCPExec:
      {
        const u32 value = s_media_buffer_32[2];

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: GetParambyDHCPExec({})\n", value);

        s_media_buffer[1] = 0;
        s_media_buffer_32[1] = 0;
        break;
      }
      case AMMBCommand::ModifyMyIPaddr:
      {
        const u32 net_buffer_offset = s_media_buffer_32[2] - NetworkCommandAddress2;

        if (!NetworkCMDBufferCheck(net_buffer_offset, 15))
        {
          break;
        }

        const char* ip_address =
            reinterpret_cast<char*>(s_network_command_buffer + net_buffer_offset);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: modifyMyIPaddr({})\n",
                       fmt::string_view(ip_address, 15));
        break;
      }
      case AMMBCommand::GetLastError:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[2])];

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: GetLastError( {}({}) ):{}\n", fd,
                       s_media_buffer_32[2], s_last_error);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = s_last_error;
      }
      break;
      case AMMBCommand::InitLink:
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: InitLink");
        break;
      case AMMBCommand::AllNetInit:
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: AllNetInit");
        break;
      default:
        ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Command:{0:04x}", static_cast<u16>(ammb_command));
        ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Command Unhandled!");
        break;
      }

      s_media_buffer[3] |= 0x80;  // Command complete flag

      memory.Memset(address, 0, length);

      ExpansionInterface::GenerateInterrupt(0x10);
      return 0;
    }

    // Max GC disc offset
    if (offset >= 0x57058000)
    {
      PanicAlertFmtT("Unhandled Media Board Read:{0:08x}", offset);
      return 0;
    }

    if (s_firmware_map)
    {
      if (!SafeCopyToEmu(memory, address, s_firmware, sizeof(s_firmware), offset, length))
      {
        ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid firmware buffer range: offset={}, length={}",
                      offset, length);
      }
      return 0;
    }

    if (s_dimm_disc.size())
    {
      if (!SafeCopyToEmu(memory, address, s_dimm_disc.data(), s_dimm_disc.size(), offset, length))
      {
        ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid DIMM Disc read from: offset={}, length={}",
                      offset, length);
      }
      return 0;
    }

    return 1;
    break;
  case AMMBDICommand::Write:

    // These two magic writes allow a new firmware to be programmed
    if (offset == FirmwareMagicWrite1 && length == 0x20)
    {
      s_firmware_map = true;
      return 0;
    }

    if (offset == FirmwareMagicWrite2 && length == 0x20)
    {
      s_firmware_map = true;
      return 0;
    }

    if (s_firmware_map)
    {
      // Firmware memory (2MB)
      if ((offset >= 0x00400000) && (offset <= 0x600000))
      {
        const u32 fwoffset = offset - 0x00400000;
        memory.CopyFromEmu(s_firmware + fwoffset, address, length);
        return 0;
      }
    }

    // Network configuration
    if (offset == 0x00000000 && length == 0x80)
    {
      FileWriteData(memory, &s_netcfg, 0, address, length);
      return 0;
    }

    // media crc check on/off
    if (offset == DIMMExtraSettings && length == 0x20)
    {
      FileWriteData(memory, &s_extra, 0, address, length);
      return 0;
    }

    // Backup memory (8MB)
    if (offset >= BackupMemory && offset < 0x00800000)
    {
      FileWriteData(memory, &s_backup, 0, address, length);
      return 0;
    }

    // DIMM memory (8MB)
    if (offset >= DIMMMemory && offset < 0x1F800000)
    {
      FileWriteData(memory, &s_dimm, offset - DIMMMemory, address, length);
      return 0;
    }

    // Firmware Write
    if (offset >= FirmwareAddress && offset < 0x84818000)
    {
      INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: Write Firmware ({:08x})", offset);
      PrintMBBuffer(address, length);
      return 0;
    }

    // DIMM memory (8MB)
    if (offset >= DIMMMemory2 && offset < 0xFF800000)
    {
      FileWriteData(memory, &s_dimm, offset - DIMMMemory2, address, length);
      return 0;
    }

    if (offset == NetworkControl && length == 0x20)
    {
      FileWriteData(memory, &s_netctrl, 0, address, length);
      return 0;
    }

    if (offset == DIMMCommandExecute1 && length == 0x20)
    {
      if (memory.Read_U8(address) == 1)
      {
        const AMMBCommand ammb_command = Common::BitCastPtr<AMMBCommand>(s_media_buffer + 0x22);

        INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: Execute command: (1):{0:04X}",
                     static_cast<u16>(ammb_command));

        memset(s_media_buffer, 0, 0x20);

        // Counter/Command
        s_media_buffer_32[0] = s_media_buffer_32[8] | 0x80000000;  // Set command okay flag

        // Handle command
        switch (ammb_command)
        {
        case AMMBCommand::Unknown_000:
          s_media_buffer_32[1] = 1;
          break;
        case AMMBCommand::GetDIMMSize:
          s_media_buffer_32[1] = 0x20000000;
          break;
        case AMMBCommand::GetMediaBoardStatus:
          s_media_buffer_32[1] = LoadedGameProgram;
          s_media_buffer_32[2] = 100;
          break;
          // SegaBoot version: 3.09
        case AMMBCommand::GetSegaBootVersion:
          // Version
          s_media_buffer[4] = 0x03;
          s_media_buffer[5] = 0x09;
          // Unknown
          s_media_buffer[6] = 1;
          s_media_buffer_32[2] = 1;
          s_media_buffer_32[4] = 0xFF;
          break;
        case AMMBCommand::GetSystemFlags:
          s_media_buffer[4] = 1;
          s_media_buffer[5] = GDROM;
          // Enable development mode (Sega Boot)
          // This also allows region free booting
          s_media_buffer[6] = 1;
          s_media_buffer[8] = 0;  // Access Count
          break;
        case AMMBCommand::GetMediaBoardSerial:
          memcpy(s_media_buffer + 4, "A89E-27A50364511", 16);
          break;
        case AMMBCommand::Unknown_104:
          s_media_buffer[4] = 1;
          break;
        default:
          PanicAlertFmtT("Unhandled Media Board Command:{0:04x}", static_cast<u16>(ammb_command));
          break;
        }

        memset(s_media_buffer + 0x20, 0, 0x20);

        ExpansionInterface::GenerateInterrupt(0x04);
        return 0;
      }
    }

    for (const auto& range : s_mediaboard_ranges)
    {
      if (offset >= range.start && offset < range.end)
      {
        INFO_LOG_FMT(AMMEDIABOARD, "GC-AM: Write MediaBoard ({:08x},{:08x},{:08x})", offset,
                     range.base_offset, length);
        SafeCopyFromEmu(memory, range.buffer, address, range.buffer_size,
                        offset - range.base_offset, length);
        PrintMBBuffer(address, length);
        return 0;
      }
    }

    // Max GC disc offset
    if (offset >= 0x57058000)
    {
      PrintMBBuffer(address, length);
      PanicAlertFmtT("Unhandled Media Board Write:{0:08x}", offset);
    }
    break;
  case AMMBDICommand::Execute:
  {
    const auto ammb_command = static_cast<AMMBCommand>(s_media_buffer_32[8] >> 16);
    if (ammb_command == AMMBCommand::Unknown_000)
      break;

    if (offset == 0 && length == 0)
    {
      memset(s_media_buffer, 0, 0x20);

      // Counter/Command
      s_media_buffer_32[0] = s_media_buffer_32[8] | 0x80000000;  // Set command okay flag

      switch (ammb_command)
      {
      case AMMBCommand::Unknown_000:
        s_media_buffer_32[1] = 1;
        break;
      case AMMBCommand::GetDIMMSize:
        s_media_buffer_32[1] = 0x20000000;
        break;
      case AMMBCommand::GetMediaBoardStatus:
      {
        // Fake loading the game to have a chance to enter test mode
        static u32 status = LoadingGameProgram;
        static u32 progress = 80;

        s_media_buffer_32[1] = status;
        s_media_buffer_32[2] = progress;
        if (progress < 100)
        {
          progress++;
        }
        else
        {
          status = LoadedGameProgram;
        }
      }
      break;
      // SegaBoot version: 3.11
      case AMMBCommand::GetSegaBootVersion:
        // Version
        s_media_buffer[4] = 0x03;
        s_media_buffer[5] = 0x11;
        // Unknown
        s_media_buffer[6] = 1;
        s_media_buffer_32[2] = 1;
        s_media_buffer_32[4] = 0xFF;
        break;
      case AMMBCommand::GetSystemFlags:
        s_media_buffer[4] = 1;
        s_media_buffer[5] = GDROM;
        // Enable development mode (Sega Boot)
        // This also allows region free booting
        s_media_buffer[6] = 1;
        s_media_buffer[8] = 0;  // Access Count
        break;
      case AMMBCommand::GetMediaBoardSerial:
        memcpy(s_media_buffer + 4, "A89E-27A50364511", 16);
        break;
      case AMMBCommand::Unknown_104:
        s_media_buffer[4] = 1;
        break;
      case AMMBCommand::NetworkReInit:
        break;
      case AMMBCommand::TestHardware:
        // Test type

        // 0x01: Media board
        // 0x04: Network

        DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM: TestHardware: ({:08x})", s_media_buffer_32[11]);
        // Pointer to a memory address that is directly displayed on screen as a string
        DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM:               ({:08x})", s_media_buffer_32[12]);

        // On real systems it shows the status about the DIMM/GD-ROM here
        // We just show "TEST OK"
        memory.Write_U32(0x54534554, s_media_buffer_32[12]);
        memory.Write_U32(0x004B4F20, s_media_buffer_32[12] + 4);

        s_media_buffer_32[1] = s_media_buffer_32[9];
        break;
      case AMMBCommand::Closesocket:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[10])];

        const int ret = closesocket(fd);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: closesocket( {}({}) ):{}\n", fd,
                       s_media_buffer_32[10], ret);

        s_sockets[SocketCheck(s_media_buffer_32[10])] = SOCKET_ERROR;

        s_media_buffer_32[1] = ret;
        s_last_error = SSC_SUCCESS;
      }
      break;
      case AMMBCommand::Connect:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[10])];
        const u32 off = s_media_buffer_32[11] - NetworkCommandAddress1;
        const u32 len = s_media_buffer_32[12];

        if (NetworkCMDBufferCheck(off, sizeof(sockaddr_in)))
        {
          break;
        }

        sockaddr_in addr;
        memcpy(&addr, s_network_command_buffer + off, sizeof(sockaddr_in));

        const int ret = NetDIMMConnect(fd, &addr, len);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: connect( {}({}), ({},{}:{}), {} ):{}\n", fd,
                       s_media_buffer_32[10], addr.sin_family, inet_ntoa(addr.sin_addr),
                       Common::swap16(addr.sin_port), len, ret);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
      }
      break;
      case AMMBCommand::Recv:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[10])];
        u32 off = s_media_buffer_32[11];
        auto len = std::min<u32>(s_media_buffer_32[12], sizeof(s_network_buffer));

        if (off >= NetworkBufferAddress5 &&
            off + len <= NetworkBufferAddress5 + sizeof(s_network_buffer))
        {
          off -= NetworkBufferAddress5;
        }
        else if (off + len > sizeof(s_network_buffer))
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET,
                        "GC-AM: recv(error) invalid destination or length: off={:08x}, len={}\n",
                        off, len);
          off = 0;
          len = 0;
        }

        int ret = recv(fd, reinterpret_cast<char*>(s_network_buffer + off), len, 0);
        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: recv( {}, 0x{:08x}, {} ):{} {}\n", fd, off, len,
                       ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
      }
      break;
      case AMMBCommand::Send:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[10])];
        u32 off = s_media_buffer_32[11];
        auto len = std::min<u32>(s_media_buffer_32[12], sizeof(s_network_buffer));

        if (off >= NetworkBufferAddress1 &&
            off + len <= NetworkBufferAddress1 + sizeof(s_network_buffer))
        {
          off -= NetworkBufferAddress1;
        }
        else if (off + len > sizeof(s_network_buffer))
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET,
                        "GC-AM: send(error) unhandled destination or length: {:08x}, len={}", off,
                        len);
          off = 0;
          len = 0;
        }

        const int ret = send(fd, reinterpret_cast<char*>(s_network_buffer + off), len, 0);
        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: send( {}({}), 0x{:08x}, {} ): {} {}\n", fd,
                       s_media_buffer_32[10], off, len, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
      }
      break;
      case AMMBCommand::Socket:
      {
        // Protocol is not sent
        const u32 af = s_media_buffer_32[10];
        const u32 type = s_media_buffer_32[11];

        const SOCKET fd = socket_(af, type, IPPROTO_TCP);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: socket( {}, {}, IPPROTO_TCP ):{}\n", af, type, fd);

        s_media_buffer[1] = 0;
        s_media_buffer_32[1] = fd;
      }
      break;
      case AMMBCommand::Select:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[10] - 1)];

        fd_set* readfds = nullptr;
        fd_set* writefds = nullptr;
        fd_set* exceptfds = nullptr;

        timeval timeout = {};
        u8* timeout_src = nullptr;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        // Only one of 11, 12, 13 is ever set alongside 14
        if (s_media_buffer_32[14] != 0)
        {
          const u32 fd_set_offset = s_media_buffer_32[14] - NetworkCommandAddress1;
          if (!NetworkCMDBufferCheck(fd_set_offset, sizeof(fd_set)))
          {
            ERROR_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: Select(error) unhandled destination:{:08x}\n",
                          s_media_buffer_32[14]);
            break;
          }

          Common::BitCastPtr<fd_set>(s_network_command_buffer + fd_set_offset) = fds;

          if (s_media_buffer_32[11] != 0)
          {
            readfds = &fds;
            timeout_src = s_network_command_buffer + s_media_buffer_32[11] - NetworkCommandAddress1;
          }
          else if (s_media_buffer_32[12] != 0)
          {
            writefds = &fds;
            timeout_src = s_network_command_buffer + s_media_buffer_32[12] - NetworkCommandAddress1;
          }
          else if (s_media_buffer_32[13] != 0)
          {
            exceptfds = &fds;
            timeout_src = s_network_command_buffer + s_media_buffer_32[13] - NetworkCommandAddress1;
          }
        }

        // Copy timeout if set
        if (timeout_src != nullptr)
        {
          std::memcpy(&timeout, timeout_src, sizeof(timeval));
        }

        // BUG?: F-Zero AX Monster calls select with a two second timeout
        // for unknown reasons, which slows down the game a lot
        if (AMMediaboard::GetGameType() == FZeroAXMonster)
        {
          timeout.tv_sec = 0;
          timeout.tv_usec = 1800;
        }

        const int ret =
            select(fd + 1, readfds, writefds, exceptfds, timeout_src ? &timeout : nullptr);
        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET,
                       "GC-AM: select( {}({}), 0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x} ):{} {} \n", fd,
                       s_media_buffer_32[10], s_media_buffer_32[11], s_media_buffer_32[14],
                       s_media_buffer_32[15], s_media_buffer_32[16], ret, err);

        s_media_buffer[1] = 0;
        s_media_buffer_32[1] = ret;
      }
      break;
      case AMMBCommand::SetSockOpt:
      {
        const SOCKET fd = s_sockets[SocketCheck(s_media_buffer_32[10])];
        const int level = static_cast<int>(s_media_buffer_32[11]);
        const int optname = static_cast<int>(s_media_buffer_32[12]);
        const int optlen = static_cast<int>(s_media_buffer_32[14]);

        if (!NetworkCMDBufferCheck(s_media_buffer_32[13] - NetworkCommandAddress1, optlen))
        {
          break;
        }

        const char* optval = reinterpret_cast<char*>(
            s_network_command_buffer + s_media_buffer_32[13] - NetworkCommandAddress1);

        const int ret = setsockopt(fd, level, optname, optval, optlen);
        const int err = WSAGetLastError();

        NOTICE_LOG_FMT(AMMEDIABOARD_NET,
                       "GC-AM: setsockopt( {:d}, {:04x}, {}, {:p}, {} ):{:d} ({})\n", fd, level,
                       optname, optval, optlen, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
      }
      break;
      case AMMBCommand::ModifyMyIPaddr:
      {
        const u32 net_buffer_offset = s_media_buffer_32[10] - NetworkCommandAddress1;

        if (!NetworkCMDBufferCheck(net_buffer_offset, 15))
        {
          break;
        }

        const char* ip_address =
            reinterpret_cast<char*>(s_network_command_buffer + net_buffer_offset);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: modifyMyIPaddr({})\n",
                       fmt::string_view(ip_address, 15));
      }
      break;
      // Empty reply
      case AMMBCommand::InitLink:
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: InitLink");
        break;
      case AMMBCommand::Unknown_605:
        NOTICE_LOG_FMT(AMMEDIABOARD, "GC-AM: 0x605");
        break;
      case AMMBCommand::SetupLink:
      {
        sockaddr_in addra;
        sockaddr_in addrb;

        addra.sin_addr.s_addr = s_media_buffer_32[12];
        addrb.sin_addr.s_addr = s_media_buffer_32[13];

        const u16 size = s_media_buffer[0x24] | s_media_buffer[0x25] << 8;
        const u16 port = Common::swap16(s_media_buffer[0x27] | s_media_buffer[0x26] << 8);
        const u16 unknown = s_media_buffer[0x2D] | s_media_buffer[0x2C] << 8;

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: SetupLink:");
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:  Size: ({}) ", size);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:  Port: ({})", port);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:LinkNum:({:02x})", s_media_buffer[0x28]);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        ({:02x})", s_media_buffer[0x2A]);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        ({:04x})", unknown);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:   IP:  ({})", inet_ntoa(addra.sin_addr));  // IP ?
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:   IP:  ({})",
                       inet_ntoa(addrb.sin_addr));  // Target IP
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        ({:08x})",
                       Common::swap32(s_media_buffer_32[14]));  // some RAM address
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        ({:08x})",
                       Common::swap32(s_media_buffer_32[15]));  // some RAM address

        s_media_buffer_32[1] = 0;
      }
      break;
      // This sends a UDP packet to previously defined Target IP/Port
      case AMMBCommand::SearchDevices:
      {
        u16 unknown = s_media_buffer[0x25] | s_media_buffer[0x24] << 8;
        u16 off = s_media_buffer[0x26] | s_media_buffer[0x27] << 8;
        u32 addr = s_media_buffer_32[10];

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: SearchDevices: ({})", unknown);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        Offset: ({:04x})", off);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:                ({:08x})", addr);

        if (!NetworkBufferCheck(off + addr - NetworkBufferAddress2, 0x20))
        {
          break;
        }

        const u8* data = s_network_buffer + off + addr - NetworkBufferAddress2;

        for (u32 i = 0; i < 0x20; i += 0x10)
        {
          const std::array<u32, 4> data_u32 = Common::BitCastPtr<std::array<u32, 4>>(data + i);
          NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: {:08x} {:08x} {:08x} {:08x}", data_u32[0],
                         data_u32[1], data_u32[2], data_u32[3]);
        }

        s_media_buffer_32[1] = 0;
      }
      break;
      case AMMBCommand::Unknown_608:
      {
        const u32 ip = s_media_buffer_32[10];
        u16 port = Common::swap16(s_media_buffer[6] | s_media_buffer[7] << 8);
        u16 flag = s_media_buffer[10] | s_media_buffer[11] << 8;

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: 0x608( {} {} {} )", ip, port, flag);
      }
      break;
      case AMMBCommand::Unknown_614:
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: 0x614");
        break;
      default:
        ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Execute buffer UNKNOWN:{0:04x}",
                      static_cast<u16>(ammb_command));
        break;
      }

      memset(s_media_buffer + 0x20, 0, 0x20);
      return 0;
    }

    PanicAlertFmtT("Unhandled Media Board Execute:{0:04x}", static_cast<u16>(ammb_command));
    break;
  }
  default:
    PanicAlertFmtT("Unhandled Media Board Command:{0:02x}", command);
    break;
  }

  return 0;
}

u32 GetMediaType()
{
  switch (GetGameType())
  {
  default:
  case FZeroAX:
  case VirtuaStriker3:
  case VirtuaStriker4:
  case VirtuaStriker4_2006:
  case GekitouProYakyuu:
  case KeyOfAvalon:
    return GDROM;

  case MarioKartGP:
  case MarioKartGP2:
  case FZeroAXMonster:
    return NAND;
  }
}

// This is checking for the real game IDs (See boot.id within the game)
u32 GetGameType()
{
  u16 triforce_id = 0;
  const std::string& game_id = SConfig::GetInstance().GetGameID();

  if (game_id.length() == 6)
  {
    triforce_id = game_id[1] << 8 | game_id[2];
  }
  else
  {
    triforce_id = 0x454A;  // Fallback (VirtuaStriker3)
  }

  auto it = s_game_map.find(triforce_id);
  if (it != s_game_map.end())
  {
    return it->second;
  }

  PanicAlertFmtT("Unknown game ID:{0:08x}, using default controls.", triforce_id);
  return VirtuaStriker3;  // Fallback
}

bool GetTestMenu()
{
  return s_test_menu;
}

void Shutdown()
{
  s_netcfg.Close();
  s_netctrl.Close();
  s_extra.Close();
  s_backup.Close();
  s_dimm.Close();
  s_dimm_disc.clear();

  // Close all sockets
  for (u32 i = 1; i < 64; ++i)
  {
    if (s_sockets[i] != SOCKET_ERROR)
    {
      closesocket(s_sockets[i]);
    }
  }
}

}  // namespace AMMediaboard
