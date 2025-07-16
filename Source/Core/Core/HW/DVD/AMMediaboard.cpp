// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/AMMediaboard.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigLoaders/BaseConfigLoader.h"
#include "Core/ConfigLoaders/NetPlayConfigLoader.h"
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
#include "Core/HW/Sram.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/Network/Socket.h"
#include "Core/Movie.h"
#include "Core/NetPlayProto.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "Core/WiiRoot.h"
#include "DiscIO/DirectoryBlob.h"
#include "DiscIO/Enums.h"
#include "DiscIO/VolumeDisc.h"

#if defined(__linux__) or defined(__APPLE__) or defined(__FreeBSD__) or defined(__NetBSD__) or     \
    defined(__HAIKU__)

#include <unistd.h>

#define closesocket close
#define ioctlsocket ioctl

#define WSAEWOULDBLOCK 10035L
#define SOCKET_ERROR (-1)

typedef int SOCKET;

#define INVALID_SOCKET  (SOCKET)(~0)

static int WSAGetLastError(void)
{
  switch (errno)
  {
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

static bool s_firmwaremap = false;
static bool s_segaboot = false;
static SOCKET s_namco_cam = 0;
static u32 s_timeouts[3] = {20000, 20000, 20000};
static u32 s_last_error = SSC_SUCCESS;

static u32 s_GCAM_key_a = 0;
static u32 s_GCAM_key_b = 0;
static u32 s_GCAM_key_c = 0;

static File::IOFile* s_netcfg = nullptr;
static File::IOFile* s_netctrl = nullptr;
static File::IOFile* s_extra = nullptr;
static File::IOFile* s_backup = nullptr;
static File::IOFile* s_dimm = nullptr;

static u8* s_dimm_disc = nullptr;

static u8 s_firmware[2 * 1024 * 1024];
static u8 s_media_buffer[0x300];
static u8 s_network_command_buffer[0x4FFE00];
static u8 s_network_buffer[256 * 1024];

/* Sockets FDs are required to go from 0 to 63.
   Games use the FD as indexes so we have to workaround it.
 */
static SOCKET s_sockets[64];

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

static SOCKET accept_(int fd, struct sockaddr* addr, int* len)
{
  for (u32 i = 1; i < 64; ++i)
  {
    if (s_sockets[i] == SOCKET_ERROR)
    {
      s_sockets[i] = accept(fd, addr, (socklen_t*)len);
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
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  for (u32 i = 0; i < length; i += 0x10)
  {
    INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: {:08x} {:08x} {:08x} {:08x}",
                 memory.Read_U32(address + i), memory.Read_U32(address + i + 4),
                 memory.Read_U32(address + i + 8), memory.Read_U32(address + i + 12));
  }
}

void FirmwareMap(bool on)
{
  s_firmwaremap = on;
}

void InitKeys(u32 key_a, u32 key_b, u32 key_c)
{
  s_GCAM_key_a = key_a;
  s_GCAM_key_b = key_b;
  s_GCAM_key_c = key_c;
}

static File::IOFile* OpenOrCreateFile(const std::string& filename)
{
  // Try opening for read/write first
  if (File::Exists(filename))
    return new File::IOFile(filename, "rb+");

  // Create new file
  return new File::IOFile(filename, "wb+");
}

void Init(void)
{
  memset(s_media_buffer, 0, sizeof(s_media_buffer));
  memset(s_network_buffer, 0, sizeof(s_network_buffer));
  memset(s_network_command_buffer, 0, sizeof(s_network_command_buffer));
  memset(s_firmware, -1, sizeof(s_firmware));
  memset(s_sockets, SOCKET_ERROR, sizeof(s_sockets));

  s_segaboot = false;
  s_firmwaremap = false;

  s_last_error = SSC_SUCCESS;

  s_GCAM_key_a = 0;
  s_GCAM_key_b = 0;
  s_GCAM_key_c = 0;

  std::string base_path = File::GetUserPath(D_TRIUSER_IDX);

  s_netcfg = OpenOrCreateFile(base_path + "trinetcfg.bin");
  s_netctrl = OpenOrCreateFile(base_path + "trinetctrl.bin");
  s_extra = OpenOrCreateFile(base_path + "triextra.bin");
  s_dimm = OpenOrCreateFile(base_path + "tridimm_" + SConfig::GetInstance().GetTriforceID() + ".bin");
  s_backup = OpenOrCreateFile(base_path + "backup_" + SConfig::GetInstance().GetTriforceID() + ".bin");

  if (!s_netcfg)
    PanicAlertFmt("Failed to open/create: {}", base_path + "s_netcfg.bin");
  if (!s_netctrl)
    PanicAlertFmt("Failed to open/create: {}", base_path + "s_netctrl.bin");
  if (!s_extra)
    PanicAlertFmt("Failed to open/create: {}", base_path + "s_extra.bin");
  if (!s_dimm)
    PanicAlertFmt("Failed to open/create: {}", base_path + "s_dimm.bin");
  if (!s_backup)
    PanicAlertFmt("Failed to open/create: {}", base_path + "s_backup.bin");

  // This is the firmware for the Triforce
  const std::string sega_boot_filename = base_path + "segaboot.gcm";

  if (!File::Exists(sega_boot_filename))
  {
    PanicAlertFmt("Failed to open segaboot.gcm({}), which is required for test menus.", sega_boot_filename.c_str());
    return;
  }

  File::IOFile sega_boot(sega_boot_filename, "rb+");
  if (!sega_boot.IsOpen())
  {
    PanicAlertFmt("Failed to read: {}", sega_boot_filename);
    return;
  }

  u64 length = std::min<u64>(sega_boot.GetSize(), sizeof(s_firmware));
  sega_boot.ReadBytes(s_firmware, length);
}

u8* InitDIMM(u32 size)
{
  if (size == 0)
    return nullptr;

  if (!s_dimm_disc)
  {
    s_dimm_disc = new (std::nothrow) u8[size];
    if (!s_dimm_disc)
    {
      PanicAlertFmt("Failed to allocate DIMM memory.");
      return nullptr;
    }
  }

  s_firmwaremap = 0;
  return s_dimm_disc;
}

static s32 NetDIMMAccept(int fd, struct sockaddr* addr, int* len)
{
  SOCKET clientSock = INVALID_SOCKET;
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 10000;  // 10 milliseconds

  int result = select(0, &readfds, NULL, NULL, &timeout);
  if (result > 0 && FD_ISSET(fd, &readfds))
  {
    clientSock = accept_(fd, addr, len);
    if (clientSock != INVALID_SOCKET)
    {
      s_last_error = SSC_SUCCESS;
      return clientSock;
    }
    else
    {
      s_last_error = SOCKET_ERROR;
      return SOCKET_ERROR;
    }
  }
  else if (result == 0)
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

static s32 NetDIMMConnect(int fd, struct sockaddr_in* addr, int len)
{
  // CyCraft Connect IP
  if (addr->sin_addr.s_addr == inet_addr("192.168.11.111"))
  {
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");
  }

  // NAMCO Camera ( IPs are: 192.168.29.104-108 )
  if ((addr->sin_addr.s_addr & 0xFFFFFF00) == 0xC0A81D00)
  {
    addr->sin_addr.s_addr = inet_addr("127.0.0.1");
    /*
      BUG: An invalid family value is used
    */
    addr->sin_family = htons(AF_INET);
    s_namco_cam = fd;
  }

  // Key of Avalon Client
  if (addr->sin_addr.s_addr == inet_addr("192.168.13.1"))
  {
    addr->sin_addr.s_addr = inet_addr("10.0.0.45");
  }

  addr->sin_family = Common::swap16(addr->sin_family);
  //*(u32*)(&addr.sin_addr) = Common::swap32(*(u32*)(&addr.sin_addr));

  int ret = 0;
  int err = 0;
  u_long val = 1;

  // Set socket to non-blocking
  ioctlsocket(fd, FIONBIO, &val);

  ret = connect(fd, (const sockaddr*)addr, len);
  err = WSAGetLastError();

  if (ret == SOCKET_ERROR && err == WSAEWOULDBLOCK)
  {
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = s_timeouts[0];

    ret = select(0, NULL, &writefds, NULL, &timeout);
    if (ret > 0 && FD_ISSET(fd, &writefds))
    {
      int so_error = 0;
      socklen_t optlen = sizeof(so_error);
      if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &optlen) == 0 && so_error == 0)
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
static void FileWriteData(File::IOFile* file, u32 seek_pos, const u8* data, size_t length)
{
  file->Seek(seek_pos, File::SeekOrigin::Begin);
  file->WriteBytes(data, length);
  file->Flush();
}
u32 ExecuteCommand(std::array<u32, 3>& DICMDBUF, u32 address, u32 length)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  auto& ppc_state = system.GetPPCState();
  auto& jit_interface = system.GetJitInterface();

  /*
    The triforce IPL sends these commands first
    01010000 00000101 00000000
    01010000 00000000 0000ffff
  */
  if (s_GCAM_key_a == 0)
  {
    /*
      Since it is currently unknown how the seed is created
      we have to patch out the crypto.
    */
    if (memory.Read_U32(0x8131ecf4))
    {
      memory.Write_U32(0, 0x8131ecf4);
      memory.Write_U32(0, 0x8131ecf8);
      memory.Write_U32(0, 0x8131ecfC);
      memory.Write_U32(0, 0x8131ebe0);
      memory.Write_U32(0, 0x8131ed6c);
      memory.Write_U32(0, 0x8131ed70);
      memory.Write_U32(0, 0x8131ed74);

      memory.Write_U32(0x4E800020, 0x813025C8);
      memory.Write_U32(0x4E800020, 0x81302674);

      ppc_state.iCache.Invalidate(memory, jit_interface, 0x813025C8);
      ppc_state.iCache.Invalidate(memory, jit_interface, 0x81302674);

      HLE::Patch(system, 0x813048B8, "OSReport");
      HLE::Patch(system, 0x8130095C, "OSReport");  // Apploader
    }
  }

  DICMDBUF[0] ^= s_GCAM_key_a;
  DICMDBUF[1] ^= s_GCAM_key_b;
  // length ^= s_GCAM_key_c; // DMA length is always plain

  u32 seed = DICMDBUF[0] >> 16;

  s_GCAM_key_a *= seed;
  s_GCAM_key_b *= seed;
  s_GCAM_key_c *= seed;

  DICMDBUF[0] <<= 24;
  DICMDBUF[1] <<= 2;

  // SegaBoot adds bits for some reason to offset/length
  // also adds 0x20 to offset
  if (DICMDBUF[1] == 0x00100440)
  {
    s_segaboot = true;
  }

  u32 command = DICMDBUF[0];
  u32 offset = DICMDBUF[1];

  INFO_LOG_FMT(DVDINTERFACE_AMMB,
               "GC-AM: {:08x} {:08x} DMA=addr:{:08x},len:{:08x} Keys: {:08x} {:08x} {:08x}",
               command, offset, address, length, s_GCAM_key_a, s_GCAM_key_b, s_GCAM_key_c);

  // Test mode
  if (offset == 0x0002440)
  {
    // Set by OSResetSystem
    if (memory.Read_U32(0x811FFF00) == 1)
    {
      // Don't map firmware while in SegaBoot
      if (memory.Read_U32(0x8006BF70) != 0x0A536567)
      {
        s_firmwaremap = 1;
      }
    }
  }

  switch (AMMBCommand(command >> 24))
  {
  case AMMBCommand::Inquiry:
    if (s_firmwaremap)
    {
      s_firmwaremap = false;
      s_segaboot = false;
    }

    // Returned value is used to set the protocol version.
    switch (GetGameType())
    { 
    default:
      return Version1;
    case KeyOfAvalon:
    case MarioKartGP:
    case MarioKartGP2:
    case FirmwareUpdate:
      return Version2;
    }
    break;
  case AMMBCommand::Read:
    if ((offset & 0x8FFF0000) == 0x80000000)
    {
      switch (offset)
      { 
      case MediaBoardStatus1:
        memory.Write_U16(Common::swap16(0x0100), address);
        break; 
      case MediaBoardStatus2:
        memset(memory.GetPointer(address), 0, length);
        break;
      case MediaBoardStatus3:
        memset(memory.GetPointer(address), 0xFF, length);
        // DIMM size (512MB)
        memory.Write_U32(Common::swap32(0x20000000), address);
        // GCAM signature
        memory.Write_U32(0x4743414D, address + 4);
        break;
      case 0x80000100:
        memory.Write_U32(Common::swap32((u32)0x001F1F1F), address);
        break;
      case FirmwareStatus1:
        memory.Write_U32(Common::swap32((u32)0x01FA), address);
        break;
      case FirmwareStatus2:
        memory.Write_U32(Common::swap32((u32)1), address);
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
      s_netcfg->Seek(0, File::SeekOrigin::Begin);
      s_netcfg->ReadBytes(memory.GetPointer(address), length);
      return 0;
    }

    // media crc check on/off
    if (offset == DIMMExtraSettings && length == 0x20)
    {
      s_extra->Seek(0, File::SeekOrigin::Begin);
      s_extra->ReadBytes(memory.GetPointer(address), length);
      return 0;
    }

    // DIMM memory (8MB)
    if (offset >= DIMMMemory && offset <= 0x1F800000)
    {
      u32 dimmoffset = offset - DIMMMemory;
      s_dimm->Seek(dimmoffset, File::SeekOrigin::Begin);
      s_dimm->ReadBytes(memory.GetPointer(address), length);
      return 0;
    }

    if (offset >= DIMMCommandVersion1 && offset < 0x1F900040)
    {
      u32 dimmoffset = offset - DIMMCommandVersion1;
      memcpy(memory.GetPointer(address), s_media_buffer + dimmoffset, length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read MEDIA BOARD COMM AREA (1) ({:08x},{})", offset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if (offset >= NetworkBufferAddress4 && offset < 0x891C0000)
    {
      u32 dimmoffset = offset - NetworkBufferAddress4;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK BUFFER (4) ({:08x},{})", offset, length);
      memcpy(memory.GetPointer(address), s_network_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= NetworkBufferAddress5 && offset < 0x1FB10000)
    {
      u32 dimmoffset = offset - NetworkBufferAddress5;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK BUFFER (5) ({:08x},{})", offset, length);
      memcpy(memory.GetPointer(address), s_network_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= NetworkCommandAddress && offset < 0x1FD00000)
    {
      u32 dimmoffset = offset - NetworkCommandAddress;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK COMMAND BUFFER ({:08x},{})", offset,
                   length);
      memcpy(memory.GetPointer(address), s_network_command_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= NetworkCommandAddress2 && offset < 0x89060200)
    {
      u32 dimmoffset = offset - NetworkCommandAddress2;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK COMMAND BUFFER (2) ({:08x},{})", offset,
                   length);
      memcpy(memory.GetPointer(address), s_network_command_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= NetworkBufferAddress1 && offset < 0x1FA10000)
    {
      u32 dimmoffset = offset - NetworkBufferAddress1;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK BUFFER (1) ({:08x},{})", offset, length);
      memcpy(memory.GetPointer(address), s_network_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= NetworkBufferAddress2 && offset < 0x1FD10000)
    {
      u32 dimmoffset = offset - NetworkBufferAddress2;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK BUFFER (2) ({:08x},{})", offset, length);
      memcpy(memory.GetPointer(address), s_network_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= NetworkBufferAddress3 && offset < 0x89110000)
    {
      u32 dimmoffset = offset - NetworkBufferAddress3;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read NETWORK BUFFER (3) ({:08x},{})", offset, length);
      memcpy(memory.GetPointer(address), s_network_buffer + dimmoffset, length);
      return 0;
    }

    if (offset >= DIMMCommandVersion2 && offset < 0x84000060)
    {
      u32 dimmoffset = offset - DIMMCommandVersion2;
      memcpy(memory.GetPointer(address), s_media_buffer + dimmoffset, length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read MEDIA BOARD COMM AREA (2) ({:08x},{})", offset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if (offset == DIMMCommandExecute2)
    {
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: EXECUTE MEDIA BOARD COMMAND");

      memcpy(s_media_buffer, s_media_buffer + 0x200, 0x20);
      memset(s_media_buffer + 0x200, 0, 0x20);
      s_media_buffer[0x204] = 1;

      // Recast for easier access
      u32* media_buffer_32 = (u32*)(s_media_buffer); 

      switch (AMMBCommand(*(u16*)(s_media_buffer + 2)))
      {
      case AMMBCommand::Unknown_001:
        media_buffer_32[1] = 1;
        break;
      case AMMBCommand::GetNetworkFirmVersion:
        media_buffer_32[1] = 0x1305;  // Version: 13.05
        s_media_buffer[6] = 1;        // Type: VxWorks
        break;
      case AMMBCommand::GetSystemFlags:
        s_media_buffer[4] = 1;
        s_media_buffer[6] = NANDMaskBoardNAND;
        s_media_buffer[7] = 1;
        break;
      // Empty reply
      case AMMBCommand::Unknown_103:
        break;
      // Network Commands
      case AMMBCommand::Accept:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        int ret = -1;

        // Handle optional paramters
        if (media_buffer_32[3] == 0 || media_buffer_32[4] == 0)
        {
          ret = NetDIMMAccept(fd, nullptr, nullptr);
        }
        else
        {
          u32 addr_off = media_buffer_32[3] - NetworkCommandAddress2;
          u32 len_off = media_buffer_32[4] - NetworkCommandAddress2;

          struct sockaddr* addr = (struct sockaddr*)(s_network_command_buffer + addr_off);
          int* len = (int*)(s_network_command_buffer + len_off);

          ret = NetDIMMAccept(fd, addr, len);
        }

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: accept( {}({}) ):{}\n", fd, media_buffer_32[2],
                       ret);
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Bind:
      {
        struct sockaddr_in addr;

        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        u32 off = media_buffer_32[3] - NetworkCommandAddress2;
        u32 len = media_buffer_32[4];

        memcpy((void*)&addr, s_network_command_buffer + off, sizeof(struct sockaddr_in));

        addr.sin_family = Common::swap16(addr.sin_family);
        *(u32*)(&addr.sin_addr) = Common::swap32(*(u32*)(&addr.sin_addr));

        /*
          Triforce games usually use hardcoded IPs
          This is replaced to listen to the ANY address instead
        */
        addr.sin_addr.s_addr = INADDR_ANY;

        int ret = bind(fd, (const sockaddr*)&addr, len);
        int err = WSAGetLastError();

        if (ret < 0)
          PanicAlertFmt("Socket Bind Failed with{0}", err);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: bind( {}, ({},{:08x}:{}), {} ):{} ({})\n", fd,
                       addr.sin_family, addr.sin_addr.s_addr, Common::swap16(addr.sin_port), len,
                       ret, err);

        media_buffer_32[1] = ret;
        s_last_error = SSC_SUCCESS;
        break;
      }
      case AMMBCommand::Closesocket:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];

        int ret = closesocket(fd);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: closesocket( {}({}) ):{}\n", fd,
                       media_buffer_32[2], ret);

        s_sockets[media_buffer_32[2]] = SOCKET_ERROR;

        media_buffer_32[1] = ret;
        s_last_error = SSC_SUCCESS;
        break;
      }
      case AMMBCommand::Connect:
      {
        struct sockaddr_in addr;

        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        u32 off = media_buffer_32[3] - NetworkCommandAddress2;
        u32 len = media_buffer_32[4];

        int ret = 0;
        int err = 0;

        memcpy((void*)&addr, s_network_command_buffer + off, sizeof(struct sockaddr_in));

        ret = NetDIMMConnect(fd, &addr, len);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: connect( {}({}), ({},{}:{}), {} ):{} ({})\n", fd,
                       media_buffer_32[2], addr.sin_family, inet_ntoa(addr.sin_addr),
                       Common::swap16(addr.sin_port), len, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::InetAddr:
      {
        u32 ip = inet_addr((char*)s_network_command_buffer);
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: InetAddr( {} )\n",
                       (char*)s_network_command_buffer);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = Common::swap32(ip);
        break;
      }
      case AMMBCommand::Listen:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        u32 backlog = media_buffer_32[3];

        int ret = listen(fd, backlog);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: listen( {}, {} ):{:d}\n", fd, backlog, ret);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Recv:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        u32 off = media_buffer_32[3];
        u32 len = media_buffer_32[4];

        if (len >= sizeof(s_network_buffer))
        {
          len = sizeof(s_network_buffer);
        }

        int ret = 0;
        char* buffer = (char*)(s_network_buffer + off);

        if (off >= NetworkBufferAddress4 && off < NetworkBufferAddress4 + sizeof(s_network_buffer))
        {
          buffer = (char*)(s_network_buffer + off - NetworkBufferAddress4);
        }
        else
        {
          PanicAlertFmt("RECV: Buffer overrun:{0} {1} ", off, len);
        }

        int err = 0;

        ret = recv(fd, buffer, len, 0);
        err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: recv( {}, 0x{:08x}, {} ):{} {}\n", fd, off, len,
                       ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Send:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        u32 off = media_buffer_32[3];
        u32 len = media_buffer_32[4];

        int ret = 0;

        if (off >= NetworkBufferAddress3 && off < NetworkBufferAddress3 + sizeof(s_network_buffer))
        {
          off -= NetworkBufferAddress3;
        }
        else
        {
          ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: send(error) unhandled destination:{:08x}\n",
                        off);
        }

        ret = send(fd, (char*)(s_network_buffer + off), len, 0);
        int err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: send( {}({}), 0x{:08x}, {} ): {} {}\n", fd,
                       media_buffer_32[2], off, len, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Socket:
      {
        // Protocol is not sent
        u32 af = media_buffer_32[2];
        u32 type = media_buffer_32[3];

        SOCKET fd = socket_(af, type, IPPROTO_TCP);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: socket( {}, {}, IPPROTO_TCP ):{}\n", af, type,
                       fd);

        s_media_buffer[1] = 0;
        media_buffer_32[1] = fd;
        break;
      }
      case AMMBCommand::Select:
      {
        u32 nfds = s_sockets[SocketCheck(media_buffer_32[2] - 1)];

        /*
          BUG: NAMCAM is hardcoded to call this with socket ID 0x100 which might be some magic
          thing? Winsocks expects a valid socket so we take the socket from the connect.
        */
        if (AMMediaboard::GetGameType() == MarioKartGP2)
        {
          if (media_buffer_32[2] == 256)
          {
            nfds = s_namco_cam;
          }
        }

        fd_set* readfds = nullptr;
        fd_set* writefds = nullptr;
        fd_set* exceptfds = nullptr;
        timeval* timeout = nullptr;

        // Only one of 3, 4, 5 is ever set alongside 6
        if (media_buffer_32[3] && media_buffer_32[6])
        {
          u32 ROffset = media_buffer_32[6] - NetworkCommandAddress2;
          readfds = (fd_set*)(s_network_command_buffer + ROffset);
          FD_ZERO(readfds);
          FD_SET(nfds, readfds);

          timeout =
              (timeval*)(s_network_command_buffer + media_buffer_32[3] - NetworkCommandAddress2);
        }
        else if (media_buffer_32[4] && media_buffer_32[6])
        {
          u32 WOffset = media_buffer_32[6] - NetworkCommandAddress2;
          writefds = (fd_set*)(s_network_command_buffer + WOffset);
          FD_ZERO(writefds);
          FD_SET(nfds, writefds);

          timeout =
              (timeval*)(s_network_command_buffer + media_buffer_32[4] - NetworkCommandAddress2);
        }
        else if (media_buffer_32[5] && media_buffer_32[6])
        {
          u32 EOffset = media_buffer_32[6] - NetworkCommandAddress2;
          exceptfds = (fd_set*)(s_network_command_buffer + EOffset);
          FD_ZERO(exceptfds);
          FD_SET(nfds, exceptfds);

          timeout =
              (timeval*)(s_network_command_buffer + media_buffer_32[5] - NetworkCommandAddress2);
        }

        if (AMMediaboard::GetGameType() == KeyOfAvalon)
        {
          timeout->tv_sec = 0;
          timeout->tv_usec = 1800;
        }

        int ret = select(nfds + 1, readfds, writefds, exceptfds, timeout);

        int err = WSAGetLastError();

        NOTICE_LOG_FMT(
            DVDINTERFACE_AMMB,
            "GC-AM: select( {}({}), 0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x} ):{} {} {}:{} \n", nfds,
            media_buffer_32[2], media_buffer_32[3], media_buffer_32[4], media_buffer_32[5],
            media_buffer_32[6], ret, err, timeout->tv_sec, timeout->tv_usec);
        // hexdump( s_network_command_buffer, 0x40 );

        s_media_buffer[1] = 0;
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::SetSockOpt:
      {
        SOCKET fd = (SOCKET)(s_sockets[SocketCheck(media_buffer_32[2])]);
        int level = (int)(media_buffer_32[3]);
        int optname = (int)(media_buffer_32[4]);
        const char* optval =
            (char*)(s_network_command_buffer + media_buffer_32[5] - NetworkCommandAddress2);
        int optlen = (int)(media_buffer_32[6]);

        int ret = setsockopt(fd, level, optname, optval, optlen);

        int err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB,
                       "GC-AM: setsockopt( {:d}, {:04x}, {}, {:p}, {} ):{:d} ({})\n", fd, level,
                       optname, optval, optlen, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::SetTimeOuts:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];
        u32 timeoutA = media_buffer_32[3];
        u32 timeoutB = media_buffer_32[4];
        u32 timeoutC = media_buffer_32[5];

        s_timeouts[0] = timeoutA;
        s_timeouts[1] = timeoutB;
        s_timeouts[2] = timeoutC;

        int ret = 0;

        if (fd != INVALID_SOCKET)
        {
          ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeoutB, sizeof(int));
          if (ret < 0)
          {
            ret = WSAGetLastError();
          }
          else
          {
            ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeoutC, sizeof(int));
            if (ret < 0)
              ret = WSAGetLastError();
          }
        }

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: SetTimeOuts( {}, {}, {}, {} ):{}\n", fd, timeoutA,
                       timeoutB, timeoutC, ret);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::GetParambyDHCPExec:
      {
        u32 value = media_buffer_32[2];

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: GetParambyDHCPExec({})\n", value);

        s_media_buffer[1] = 0;
        media_buffer_32[1] = 0;
        break;
      }
      case AMMBCommand::ModifyMyIPaddr:
      {
        u32 NetBufferOffset = *(u32*)(s_media_buffer + 8) - NetworkCommandAddress2;

        char* IP = (char*)(s_network_command_buffer + NetBufferOffset);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: modifyMyIPaddr({})\n", IP);
        break;
      }
      case AMMBCommand::GetLastError:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_32[2])];

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: GetLastError( {}({}) ):{}\n", fd,
                       media_buffer_32[2], s_last_error);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_32[1] = s_last_error;
      }
      break;
      case AMMBCommand::InitLink:
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: InitLink");
        break;
      default:
        ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Command:{:03X}", *(u16*)(s_media_buffer + 2));
        ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Command Unhandled!");
        break;
      }

      s_media_buffer[3] |= 0x80;  // Command complete flag

      memset(memory.GetPointer(address), 0, length);

      ExpansionInterface::GenerateInterrupt(0x10);
      return 0;
    }

    if (offset >= DIMMCommandVersion2_2 && offset <= 0x89000200)
    {
      u32 dimmoffset = offset - DIMMCommandVersion2_2;
      memcpy(memory.GetPointer(address), s_media_buffer + dimmoffset, length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Read MEDIA BOARD COMM AREA (3) ({:08x})", dimmoffset);
      PrintMBBuffer(address, length);
      return 0;
    }

    // DIMM memory (8MB)
    if (offset >= DIMMMemory2 && offset <= 0xFF800000)
    {
      u32 dimmoffset = offset - DIMMMemory2;
      s_dimm->Seek(dimmoffset, File::SeekOrigin::Begin);
      s_dimm->ReadBytes(memory.GetPointer(address), length);
      return 0;
    }

    if (offset == NetworkControl && length == 0x20)
    {
      s_netctrl->Seek(0, File::SeekOrigin::Begin);
      s_netctrl->ReadBytes(memory.GetPointer(address), length);
      return 0;
    }

    // Max GC disc offset
    if (offset >= 0x57058000)
    {
      PanicAlertFmtT("Unhandled Media Board Read:{0:08x}", offset);
      return 0;
    }

    if (s_firmwaremap)
    {
      if (s_segaboot)
      {
        DICMDBUF[1] &= ~0x00100000;
        DICMDBUF[1] -= 0x20;
      }
      memcpy(memory.GetPointer(address), s_firmware + offset, length);
      return 0;
    }

    if (s_dimm_disc)
    {
      memcpy(memory.GetPointer(address), s_dimm_disc + offset, length);
      return 0;
    }

    return 1;
    break;
  case AMMBCommand::Write:
    /*
      These two magic writes allow a new firmware to be programmed
    */
    if ((offset == FirmwareMagicWrite1) && (length == 0x20))
    {
      s_firmwaremap = true;
      return 0;
    }

    if ((offset == FirmwareMagicWrite2) && (length == 0x20))
    {
      s_firmwaremap = true;
      return 0;
    }

    if (s_firmwaremap)
    {
      // Firmware memory (2MB)
      if ((offset >= 0x00400000) && (offset <= 0x600000))
      {
        u32 fwoffset = offset - 0x00400000;
        memcpy(s_firmware + fwoffset, memory.GetPointer(address), length);
        return 0;
      }
    }

    // Network configuration
    if ((offset == 0x00000000) && (length == 0x80))
    {
      FileWriteData(s_netcfg, 0, memory.GetPointer(address), length);
      return 0;
    }

    // media crc check on/off
    if ((offset == DIMMExtraSettings) && (length == 0x20))
    {
      FileWriteData(s_extra, 0, memory.GetPointer(address), length);
      return 0;
    }

    // Backup memory (8MB)
    if ((offset >= BackupMemory) && (offset <= 0x00800000))
    {
      FileWriteData(s_backup, 0, memory.GetPointer(address), length);
      return 0;
    }

    // DIMM memory (8MB)
    if ((offset >= DIMMMemory) && (offset <= 0x1F800000))
    {
      u32 dimmoffset = offset - DIMMMemory;
      FileWriteData(s_dimm, dimmoffset, memory.GetPointer(address), length);
      return 0;
    }

    if ((offset >= NetworkCommandAddress) && (offset < 0x1F801240))
    {
      u32 dimmoffset = offset - NetworkCommandAddress;

      memcpy(s_network_command_buffer + dimmoffset, memory.GetPointer(address), length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write NETWORK COMMAND BUFFER ({:08x},{})", dimmoffset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if ((offset >= NetworkCommandAddress2) && (offset <= 0x890601FF))
    {
      u32 dimmoffset = offset - NetworkCommandAddress2;

      memcpy(s_network_command_buffer + dimmoffset, memory.GetPointer(address), length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write NETWORK COMMAND BUFFER (2) ({:08x},{})",
                   dimmoffset, length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if ((offset >= NetworkBufferAddress1) && (offset <= 0x1FA1FFFF))
    {
      u32 dimmoffset = offset - 0x1FA00000;

      memcpy(s_network_buffer + dimmoffset, memory.GetPointer(address), length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write NETWORK BUFFER (1) ({:08x},{})", dimmoffset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if ((offset >= NetworkBufferAddress2) && (offset <= 0x1FD0FFFF))
    {
      u32 dimmoffset = offset - 0x1FD00000;

      memcpy(s_network_buffer + dimmoffset, memory.GetPointer(address), length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write NETWORK BUFFER (2) ({:08x},{})", dimmoffset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if ((offset >= NetworkBufferAddress3) && (offset <= 0x8910FFFF))
    {
      u32 dimmoffset = offset - 0x89100000;

      memcpy(s_network_buffer + dimmoffset, memory.GetPointer(address), length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write NETWORK BUFFER (3) ({:08x},{})", dimmoffset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if ((offset >= DIMMCommandVersion1) && (offset <= 0x1F90003F))
    {
      u32 dimmoffset = offset - 0x1F900000;
      memcpy(s_media_buffer + dimmoffset, memory.GetPointer(address), length);

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write MEDIA BOARD COMM AREA (1) ({:08x},{})", offset,
                   length);
      PrintMBBuffer(address, length);
      return 0;
    }

    if ((offset >= DIMMCommandVersion2) && (offset <= 0x8400005F))
    {
      u32 dimmoffset = offset - 0x84000000;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write MEDIA BOARD COMM AREA (2) ({:08x},{})", offset,
                   length);
      PrintMBBuffer(address, length);

      u8 cmd_flag = memory.Read_U8(address);

      if (dimmoffset == 0x40 && cmd_flag == 1)
      {
        // Recast for easier access
        u32* media_buffer_in_32 = (u32*)(s_media_buffer + 0x20);
        u16* media_buffer_in_16 = (u16*)(s_media_buffer + 0x20);
        u32* media_buffer_out_32 = (u32*)(s_media_buffer);
        u16* media_buffer_out_16 = (u16*)(s_media_buffer);

        INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Execute command:{:03X}", media_buffer_in_16[1]);

        memset(s_media_buffer, 0, 0x20);

        media_buffer_out_32[0] = media_buffer_in_32[0] | 0x80000000;  // Set command okay flag

        memcpy(s_media_buffer + 0x40, s_media_buffer, 0x20);

        switch (static_cast<AMMBCommand>(media_buffer_in_16[1]))
        {
        case AMMBCommand::Unknown_000:
          media_buffer_out_32[1] = 1;
          break;
        case AMMBCommand::GetDIMMSize:
          media_buffer_out_32[1] = 0x1FFF8000;
          break;
        case AMMBCommand::GetMediaBoardStatus:
          // Status
          media_buffer_out_32[1] = LoadedGameProgram;
          // Progress in %
          media_buffer_out_32[2] = 100;
          break;
        // SegaBoot version: 3.09
        case AMMBCommand::GetSegaBootVersion:
          // Version
          media_buffer_out_16[2] = Common::swap16(0x0309);
          // Unknown
          media_buffer_out_16[3] = 2;
          media_buffer_out_32[2] = 0x4746;  // "GF"
          media_buffer_out_32[4] = 0xFF;
          break;
        case AMMBCommand::GetSystemFlags:
          s_media_buffer[4] = 0;
          s_media_buffer[5] = GDROM;

          // Enable development mode (Sega Boot)
          // This also allows region free booting
          s_media_buffer[6] = 1;
          media_buffer_out_16[4] = 0;  // Access Count
          s_media_buffer[7] = 1;
          break;
        case AMMBCommand::GetMediaBoardSerial:
          memcpy(s_media_buffer + 4, "A85E-01A62204904", 16);
          break;
        case AMMBCommand::Unknown_104:
          s_media_buffer[4] = 1;
          break;
        default:
          PanicAlertFmtT("Unhandled Media Board Command:{0:02x}", media_buffer_in_16[1]);
          break;
        }

        memcpy(memory.GetPointer(address), s_media_buffer, length);

        memset(s_media_buffer + 0x20, 0, 0x20);

        ExpansionInterface::GenerateInterrupt(0x04);
        return 0;
      }
      else
      {
        memcpy(s_media_buffer + dimmoffset, memory.GetPointer(address), length);
      }
      return 0;
    }

    if ((offset >= DIMMCommandVersion2_2) && (offset <= 0x89000200))
    {
      u32 dimmoffset = offset - 0x89000000;
      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write MEDIA BOARD COMM AREA (3) ({:08x})",
                   dimmoffset);
      PrintMBBuffer(address, length);

      memcpy(s_media_buffer + dimmoffset, memory.GetPointer(address), length);

      return 0;
    }

    // Firmware Write
    if ((offset >= FirmwareAddress) && (offset <= 0x84818000))
    {
      u32 dimmoffset = offset - 0x84800000;

      INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Write Firmware ({:08x})", dimmoffset);
      PrintMBBuffer(address, length);
      return 0;
    }

    // DIMM memory (8MB)
    if ((offset >= DIMMMemory2) && (offset <= 0xFF800000))
    {
      u32 dimmoffset = offset - 0xFF000000;
      FileWriteData(s_dimm, dimmoffset, memory.GetPointer(address), length);
      return 0;
    }

    if ((offset == NetworkControl) && (length == 0x20))
    {
      FileWriteData(s_netctrl, 0, memory.GetPointer(address), length);
      return 0;
    }

    // Max GC disc offset
    if (offset >= 0x57058000)
    {
      PrintMBBuffer(address, length);
      PanicAlertFmtT("Unhandled Media Board Write:{0:08x}", offset);
    }
    break;
  case AMMBCommand::Execute:
    if ((offset == 0) && (length == 0))
    {
      // Recast for easier access
      u32* media_buffer_in_32 = (u32*)(s_media_buffer + 0x20);
      u16* media_buffer_in_16 = (u16*)(s_media_buffer + 0x20);
      u32* media_buffer_out_32 = (u32*)(s_media_buffer);
      u16* media_buffer_out_16 = (u16*)(s_media_buffer);

      memset(s_media_buffer, 0, 0x20);

      media_buffer_out_16[0] = media_buffer_in_16[0];

      // Command
      media_buffer_out_16[1] = media_buffer_in_16[1] | 0x8000;  // Set command okay flag

      if (media_buffer_in_16[1])
        INFO_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: Execute command:{:03X}", media_buffer_in_16[1]);

      switch (static_cast<AMMBCommand>(media_buffer_in_16[1]))
      {
      case AMMBCommand::Unknown_000:
        media_buffer_out_32[1] = 1;
        break;
      case AMMBCommand::GetDIMMSize:
        media_buffer_out_32[1] = 0x20000000;
        break;
      case AMMBCommand::GetMediaBoardStatus:
      {
        // Fake loading the game to have a chance to enter test mode
        static u32 status = LoadingGameProgram;
        static u32 progress = 80;

        media_buffer_out_32[1] = status;
        media_buffer_out_32[2] = progress;
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
        media_buffer_out_16[2] = Common::swap16(0x1103);
        // Unknown
        media_buffer_out_16[3] = 1;
        media_buffer_out_32[2] = 1;
        media_buffer_out_32[4] = 0xFF;
        break;
      case AMMBCommand::GetSystemFlags:
        // 1: GD-ROM
        s_media_buffer[4] = 1;
        s_media_buffer[5] = 1;
        // Enable development mode (Sega Boot)
        // This also allows region free booting
        s_media_buffer[6] = 1;
        media_buffer_out_16[4] = 0;  // Access Count
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

        // ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x301: ({:08x})", *(u32*)(s_media_buffer+0x24)
        // );

        // Pointer to a memory address that is directly displayed on screen as a string
        // ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({:08x})", *(u32*)(s_media_buffer+0x28)
        // );

        // On real system it shows the status about the DIMM/GD-ROM here
        // We just show "TEST OK"
        memory.Write_U32(0x54534554, media_buffer_in_32[4]);
        memory.Write_U32(0x004B4F20, media_buffer_in_32[4] + 4);

        media_buffer_out_32[1] = media_buffer_in_32[1];
        break;
      case AMMBCommand::Closesocket:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_in_32[2])];

        int ret = closesocket(fd);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: closesocket( {}({}) ):{}\n", fd,
                       media_buffer_in_32[2], ret);

        s_sockets[media_buffer_in_32[2]] = SOCKET_ERROR;

        media_buffer_out_32[1] = ret;
        s_last_error = SSC_SUCCESS;
      }
      break;
      case AMMBCommand::Connect:
      {
        struct sockaddr_in addr;

        u32 fd = s_sockets[SocketCheck(media_buffer_in_32[2])];
        u32 off = media_buffer_in_32[3] - NetworkCommandAddress;
        u32 len = media_buffer_in_32[4];

        int ret = 0;
        int err = 0;

        memcpy((void*)&addr, s_network_command_buffer + off, sizeof(struct sockaddr_in));

        ret = NetDIMMConnect(fd, &addr, len);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: connect( {}({}), ({},{}:{}), {} ):{} ({})\n", fd,
                       media_buffer_in_32[2], addr.sin_family, inet_ntoa(addr.sin_addr),
                       Common::swap16(addr.sin_port), len, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_out_32[1] = ret;
      }
      break;
      case AMMBCommand::Recv:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_in_32[2])];
        u32 off = media_buffer_in_32[3];
        u32 len = media_buffer_in_32[4];

        if (len >= sizeof(s_network_buffer))
        {
          len = sizeof(s_network_buffer);
        }

        int ret = 0;
        char* buffer = (char*)(s_network_buffer + off);

        if (off >= NetworkBufferAddress5 && off < NetworkBufferAddress5 + sizeof(s_network_buffer))
        {
          buffer = (char*)(s_network_buffer + off - NetworkBufferAddress5);
        }
        else
        {
          PanicAlertFmt("RECV: Buffer overrun:{0} {1} ", off, len);
        }

        int err = 0;

        ret = recv(fd, buffer, len, 0);
        err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: recv( {}, 0x{:08x}, {} ):{} {}\n", fd, off, len,
                       ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_out_32[1] = ret;
      }
      break;
      case AMMBCommand::Send:
      {
        u32 fd = s_sockets[SocketCheck(media_buffer_in_32[2])];
        u32 off = media_buffer_in_32[3];
        u32 len = media_buffer_in_32[4];

        int ret = 0;

        if (off >= NetworkBufferAddress1 && off < NetworkBufferAddress1 + sizeof(s_network_buffer))
        {
          off -= NetworkBufferAddress1;
        }
        else
        {
          ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: send(error) unhandled destination:{:08x}\n",
                        off);
        }

        ret = send(fd, (char*)(s_network_buffer + off), len, 0);
        int err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: send( {}({}), 0x{:08x}, {} ): {} {}\n", fd,
                       media_buffer_in_32[2], off, len, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_out_32[1] = ret;
      }
      break;
      case AMMBCommand::Socket:
      {
        // Protocol is not sent
        u32 af = media_buffer_in_32[2];
        u32 type = media_buffer_in_32[3];

        SOCKET fd = socket_(af, type, IPPROTO_TCP);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: socket( {}, {}, 6 ):{}\n", af, type, fd);

        s_media_buffer[1] = 0;
        media_buffer_out_32[1] = fd;
      }
      break;
      case AMMBCommand::Select:
      {
        u32 nfds = s_sockets[SocketCheck(media_buffer_in_32[2] - 1)];

        fd_set* readfds = nullptr;
        fd_set* writefds = nullptr;
        fd_set* exceptfds = nullptr;
        timeval* timeout = nullptr;

        // Only one of 3, 4, 5 is ever set alongside 6
        if (media_buffer_in_32[3] && media_buffer_in_32[6])
        {
          u32 ROffset = media_buffer_in_32[6] - NetworkCommandAddress;

          readfds = (fd_set*)(s_network_command_buffer + ROffset);
          FD_ZERO(readfds);
          FD_SET(nfds, readfds);

          timeout =
              (timeval*)(s_network_command_buffer + media_buffer_in_32[3] - NetworkCommandAddress);
        }
        else if (media_buffer_in_32[4] && media_buffer_in_32[6])
        {
          u32 WOffset = media_buffer_in_32[6] - NetworkCommandAddress;
          writefds = (fd_set*)(s_network_command_buffer + WOffset);
          FD_ZERO(writefds);
          FD_SET(nfds, writefds);

          timeout =
              (timeval*)(s_network_command_buffer + media_buffer_in_32[4] - NetworkCommandAddress);
        }
        else if (media_buffer_in_32[5] && media_buffer_in_32[6])
        {
          u32 EOffset = media_buffer_in_32[6] - NetworkCommandAddress;
          exceptfds = (fd_set*)(s_network_command_buffer + EOffset);
          FD_ZERO(exceptfds);
          FD_SET(nfds, exceptfds);

          timeout =
              (timeval*)(s_network_command_buffer + media_buffer_in_32[5] - NetworkCommandAddress);
        }

        /*
          BUG?: F-Zero AX Monster calls select with a two second timeout for unknown reasons, which
          slows down the game a lot
        */
        if (AMMediaboard::GetGameType() == FZeroAXMonster)
        {
          timeout->tv_sec = 0;
          timeout->tv_usec = 1800;
        }

        int ret = select(nfds + 1, readfds, writefds, exceptfds, timeout);

        int err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB,
                       "GC-AM: select( {}({}), 0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x} ):{} {} \n",
                       nfds, media_buffer_in_32[2], media_buffer_in_32[3], media_buffer_in_32[4],
                       media_buffer_in_32[5], media_buffer_in_32[6], ret, err);
        // hexdump( NetworkCMDBuffer, 0x40 );

        s_media_buffer[1] = 0;
        media_buffer_out_32[1] = ret;
      }
      break;
      case AMMBCommand::SetSockOpt:
      {
        SOCKET fd = (SOCKET)(s_sockets[SocketCheck(media_buffer_in_32[2])]);
        int level = (int)(media_buffer_in_32[3]);
        int optname = (int)(media_buffer_in_32[4]);
        const char* optval =
            (char*)(s_network_command_buffer + media_buffer_in_32[5] - NetworkCommandAddress);
        int optlen = (int)(media_buffer_in_32[6]);

        int ret = setsockopt(fd, level, optname, optval, optlen);

        int err = WSAGetLastError();

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB,
                       "GC-AM: setsockopt( {:d}, {:04x}, {}, {:p}, {} ):{:d} ({})\n", fd, level,
                       optname, optval, optlen, ret, err);

        s_media_buffer[1] = s_media_buffer[8];
        media_buffer_out_32[1] = ret;
      }
      break;
      case AMMBCommand::ModifyMyIPaddr:
      {
        u32 NetBufferOffset = media_buffer_in_32[2] - NetworkCommandAddress;

        char* IP = (char*)(s_network_command_buffer + NetBufferOffset);

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: modifyMyIPaddr({})\n", IP);
      }
      break;
      // Empty reply
      case AMMBCommand::InitLink:
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x601");
        break;
      case AMMBCommand::Unknown_605:
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x605");
        break;
      case AMMBCommand::SetupLink:
      {
        struct sockaddr_in addra, addrb;
        addra.sin_addr.s_addr = media_buffer_in_32[4];
        addrb.sin_addr.s_addr = media_buffer_in_32[5];

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x606:");
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:  Size: ({}) ", media_buffer_in_16[2]);  // size
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:  Port: ({})",
                       Common::swap16(media_buffer_in_16[3]));  // port
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:LinkNum:({:02x})",
                       s_media_buffer[0x28]);  // linknum
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({:02x})", s_media_buffer[0x29]);
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({:04x})", media_buffer_in_16[5]);
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:   IP:  ({})", inet_ntoa(addra.sin_addr));  // IP
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:   IP:  ({})",
                       inet_ntoa(addrb.sin_addr));  // Target IP
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({:08x})",
                       Common::swap32(media_buffer_in_32[6]));  // some RAM address
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({:08x})",
                       Common::swap32(media_buffer_in_32[7]));  // some RAM address

        media_buffer_out_32[1] = 0;
      }
      break;
      // This sends a UDP packet to previously defined Target IP/Port
      case AMMBCommand::SearchDevices:
      {
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x607: ({})", media_buffer_in_16[2]);
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({})", media_buffer_in_16[3]);
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM:        ({:08x})", media_buffer_in_32[2]);

        u8* Data = (u8*)(s_network_buffer + media_buffer_in_32[2] - 0x1FD00000);

        for (u32 i = 0; i < 0x20; i += 0x10)
        {
          NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: {:08x} {:08x} {:08x} {:08x}", *(u32*)(Data + i),
                         *(u32*)(Data + i + 4), *(u32*)(Data + i + 8), *(u32*)(Data + i + 12));
        }

        media_buffer_out_32[1] = 0;
      }
      break;
      case AMMBCommand::Unknown_608:
      {
        u32 IP = media_buffer_in_32[2];
        u16 Port = media_buffer_in_16[4];
        u16 Flag = media_buffer_in_16[5];

        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x608( {} {} {} )", IP, Port, Flag);
      }
      break;
      case AMMBCommand::Unknown_614:
        NOTICE_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: 0x614");
        break;
      default:
        ERROR_LOG_FMT(DVDINTERFACE_AMMB, "GC-AM: execute buffer UNKNOWN:{:03x}",
                      *(u16*)(s_media_buffer + 0x22));
        break;
      }

      memset(s_media_buffer + 0x20, 0, 0x20);
      return 0;
    }

    PanicAlertFmtT("Unhandled Media Board Execute:{0:08x}", *(u16*)(s_media_buffer + 0x22));
    break;
    default:
    PanicAlertFmtT("Unhandled Media Board Command:{0:02x}", command );
    break;
  }

  return 0;
}

u32 GetMediaType(void)
{
  switch (GetGameType())
  {
  default:
  case FZeroAX:
  case VirtuaStriker3:
  case VirtuaStriker4:
  case GekitouProYakyuu:
  case KeyOfAvalon:
    return GDROM;
    break;

  case MarioKartGP:
  case MarioKartGP2:
  case FZeroAXMonster:
    return NAND;
    break;
  }
  // Never reached
}

u32 GetGameType(void)
{
  u64 game_id = 0;

  // Convert game ID into hex
  if (strlen(SConfig::GetInstance().GetTriforceID().c_str()) > 4)
  {
    game_id = 0x30303030;
  }
  else
  {
    sscanf(SConfig::GetInstance().GetTriforceID().c_str(), "%s", (char*)&game_id);
  }

  // This is checking for the real game IDs (See boot.id within the game)
  switch (Common::swap32((u32)game_id))
  {
  // SBGG - F-ZERO AX
  case 0x53424747:
    return FZeroAX;
  // SBHA - F-ZERO AX (Monster)
  case 0x53424841:
    return FZeroAXMonster;
  // SBKJ/SBKP - MARIOKART ARCADE GP
  case 0x53424B50:
  case 0x53424B5A:
    return MarioKartGP;
  // SBNJ/SBNL - MARIOKART ARCADE GP2
  case 0x53424E4A:
  case 0x53424E4C:
    return MarioKartGP2;
  // SBEJ/SBEY - Virtua Striker 2002
  case 0x5342454A:
  case 0x53424559:
    return VirtuaStriker3;
  // SBLJ/SBLK/SBLL - VIRTUA STRIKER 4 Ver.2006
  case 0x53424C4A:
  case 0x53424C4B:
  case 0x53424C4C:
  // SBHJ/SBHN/SBHZ - VIRTUA STRIKER 4 VER.A
  case 0x5342484A:
  case 0x5342484E:
  case 0x5342485A:
  // SBJA/SBJJ  - VIRTUA STRIKER 4
  case 0x53424A41:
  case 0x53424A4A:
    return VirtuaStriker4;
  // SBFX/SBJN - Key of Avalon
  case 0x53424658:
  case 0x53424A4E:
    return KeyOfAvalon;
  // SBGX - Gekitou Pro Yakyuu (DIMM Upgrade 3.17)
  case 0x53424758:
    return GekitouProYakyuu;
  default:
    PanicAlertFmtT("Unknown game ID:{0:08x}, using default controls.", game_id);
  // GSBJ/G12U - VIRTUA STRIKER 3
  // RELS/RELJ - SegaBoot (does not have a boot.id)
  case 0x4753424A:
  case 0x47313255:
  case 0x52454C53:
  case 0x52454c4a:
    return VirtuaStriker3;
  // S000 - Firmware update
  case 0x53303030:
    return FirmwareUpdate;
  }
  // never reached
}

void Shutdown(void)
{
  if (s_netcfg)
    s_netcfg->Close();

  if (s_netctrl)
    s_netctrl->Close();

  if (s_extra)
    s_extra->Close();

  if (s_backup)
    s_backup->Close();

  if (s_dimm)
    s_dimm->Close();

  if (s_dimm_disc)
  {
    delete[] s_dimm_disc;
    s_dimm_disc = nullptr;
  }

  // close all sockets
  for (u32 i = 1; i < 64; ++i)
  {
    if (s_sockets[i] != SOCKET_ERROR)
    {
      closesocket(s_sockets[i]);
    }
  }
}

}  // namespace AMMediaboard
