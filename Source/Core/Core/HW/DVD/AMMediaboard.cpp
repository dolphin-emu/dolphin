// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/DVD/AMMediaboard.h"

#include <algorithm>
#include <bit>
#include <random>
#include <ranges>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"

#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/MainSettings.h"
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

#include "DiscIO/CachedBlob.h"

#if defined(__linux__) or defined(__APPLE__) or defined(__FreeBSD__) or defined(__NetBSD__) or     \
    defined(__HAIKU__)

#include <unistd.h>

#include "Common/UnixUtil.h"

static constexpr auto* closesocket = close;
static auto ioctlsocket(auto... args)
{
  return ioctl(args...);
}

static constexpr int WSAEWOULDBLOCK = 10035;
static constexpr int SOCKET_ERROR = -1;

using SOCKET = int;
using WSAPOLLFD = pollfd;

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

MediaBoardRange::MediaBoardRange(u32 start_, u32 size_, std::span<u8> buffer_)
    : start{start_}, end{start_ + size_}, buffer{buffer_.data()}, buffer_size{buffer_.size()}
{
}

namespace AMMediaboard
{

using Common::SEND_FLAGS;

enum class GuestSocket : s32
{
};
static constexpr auto INVALID_GUEST_SOCKET = GuestSocket(-1);

struct TimeVal
{
  // TODO: Verify this.
  u64 seconds;
  u32 microseconds;
};

struct GuestFdSet
{
  static constexpr std::size_t BIT_COUNT = 256;

  std::array<u8, BIT_COUNT / CHAR_BIT> bits{};

  constexpr bool IsFdSet(GuestSocket s) const
  {
    const auto index = std::size_t(s);
    assert(index < BIT_COUNT);
    return Common::ExtractBit(bits[index / CHAR_BIT], index % CHAR_BIT) != 0;
  }

  constexpr void SetFd(GuestSocket s, bool value = true)
  {
    const auto index = std::size_t(s);
    assert(index < BIT_COUNT);
    Common::SetBit(bits[index / CHAR_BIT], index % CHAR_BIT, value);
  }
};
static_assert(sizeof(GuestFdSet) == 32);

// This seems to be based on VxWorks sockaddr_in.
struct GuestSocketAddress
{
  // Seemingly always zero or random values ? Game bug ?
  // This is the struct size in VxWorks.
  u8 unknown_value;

  u8 ip_family;
  u16 port;  // Network byte order.
  Common::IPAddress ip_address;
  std::array<u8, 8> padding;
};
static_assert(sizeof(GuestSocketAddress) == 16);

static bool s_firmware_map = false;
static bool s_test_menu = false;
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

static std::unique_ptr<DiscIO::BlobReader> s_dimm_disc;

static std::array<u8, 0x200000> s_firmware;
static std::array<u32, 0xc0> s_media_buffer_32;
static u8* const s_media_buffer = reinterpret_cast<u8*>(s_media_buffer_32.data());
static std::array<u8, 0x4ffe00> s_network_command_buffer;
static std::array<u8, 0x80000> s_network_buffer;
static std::array<u8, 0x1000> s_allnet_buffer;
static std::array<u8, 0x8500> s_allnet_settings;

static Common::IPAddress s_game_modified_ip_address;

// Fake loading the game to have a chance to enter test mode
static u32 s_board_status = LoadingGameProgram;
static u32 s_load_progress = 80;

static constexpr std::size_t MAX_IPV4_STRING_LENGTH = 15;

constexpr char s_allnet_reply[] = {
    "uri=http://"
    "sega.com&host=sega.com&nickname=sega&name=sega&year=2025&month=08&day=16&hour=21&minute=10&"
    "second=12&place_id=1234&setting=0x123&region0=jap&region_name0=japan&region_name1=usa&region_"
    "name2=asia&region_name3=export&end"};

static const MediaBoardRange s_mediaboard_ranges[] = {
    {DIMMCommandVersion1, 0x40, Common::AsWritableU8Span(s_media_buffer_32)},
    {DIMMCommandVersion2, 0x60, Common::AsWritableU8Span(s_media_buffer_32)},
    {DIMMCommandVersion2_2, 0x220, Common::AsWritableU8Span(s_media_buffer_32)},
    {NetworkCommandAddress1, 0x1040, s_network_command_buffer},
    {NetworkCommandAddress2, 0x20000, s_network_command_buffer},
    {NetworkBufferAddress1, 0x10000, s_network_buffer},
    {NetworkBufferAddress2, 0x10000, s_network_buffer},
    {NetworkBufferAddress3, 0x50000, s_network_buffer},
    {NetworkBufferAddress4, 0xc0000, s_network_buffer},
    {NetworkBufferAddress5, 0x10000, s_network_buffer},
    {AllNetSettings, 0x8000, s_allnet_settings},
    {AllNetBuffer, 0x1000, s_allnet_buffer},
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

static SOCKET s_sockets[SOCKET_FD_MAX];

// TODO: Verify this.
static constexpr u32 FIRST_VALID_FD = 1;

static GuestSocket GetAvailableGuestSocket()
{
  for (u32 i = FIRST_VALID_FD; i < std::size(s_sockets); ++i)
  {
    if (s_sockets[i] == SOCKET_ERROR)
      return GuestSocket(i);
  }

  // Out of sockets.
  return INVALID_GUEST_SOCKET;
}

static SOCKET GetHostSocket(GuestSocket x)
{
  const auto index = u32(x);

  if (index < std::size(s_sockets))
    return s_sockets[index];

  WARN_LOG_FMT(AMMEDIABOARD, "GC-AM: Bad GuestSocket value: {}", index);
  return INVALID_SOCKET;
}

static GuestSocket GetGuestSocket(SOCKET x)
{
  const auto it = std::find(std::begin(s_sockets) + FIRST_VALID_FD, std::end(s_sockets), x);

  if (it == std::end(s_sockets))
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "GuestSocket not found. This should not happen.");
    return INVALID_GUEST_SOCKET;
  }

  return GuestSocket(it - std::begin(s_sockets));
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

static constexpr u8* GetSafePtr(std::span<u8> buffer, u32 buffer_base, u32 offset, u32 length)
{
  if (offset >= buffer_base)
  {
    const std::size_t off = offset - buffer_base;
    if ((off + length) <= buffer.size())
      return std::data(buffer) + off;
  }

  if (offset != 0)
  {
    ERROR_LOG_FMT(AMMEDIABOARD,
                  "GetSafePtr: buffer[0x{:08x}] base=0x{:08x} offset=0x{:08x} length=0x{:08x}",
                  buffer.size(), buffer_base, offset, length);
  }

  return nullptr;
}

static constexpr std::string_view GetSafeString(std::span<u8> buffer, u32 buffer_base, u32 offset,
                                                u32 max_length)
{
  auto* const ptr = GetSafePtr(buffer, buffer_base, offset, 0);

  if (ptr == nullptr)
    return {};

  // Don't exceed max_length or end of buffer.
  const auto adjusted_length =
      std::min<std::size_t>(max_length, buffer.data() + buffer.size() - ptr);
  const auto length = strnlen(reinterpret_cast<char*>(ptr), adjusted_length);

  return {reinterpret_cast<char*>(ptr), length};
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

static bool SafeCopyFromEmu(Memory::MemoryManager& memory, u8* destination, u32 address,
                            u64 destination_size, u32 offset, u32 length)
{
  if (offset > destination_size || length > destination_size - offset)
  {
    ERROR_LOG_FMT(AMMEDIABOARD,
                  "GC-AM: Write overflow: offset=0x{:08x}, length={}, destination_size={}", offset,
                  length, destination_size);
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

  memory.CopyFromEmu(destination + offset, address, length);
  return true;
}

static GuestSocket socket_(int af, int type, int protocol)
{
  const auto guest_socket = GetAvailableGuestSocket();
  if (guest_socket == INVALID_GUEST_SOCKET)
    return INVALID_GUEST_SOCKET;

  const s32 host_fd = socket(af, type, protocol);
  if (host_fd < 0)
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: failed to create socket ({})", Common::StrNetworkError());
    return INVALID_GUEST_SOCKET;
  }

  Common::SetPlatformSocketOptions(host_fd);

  s_sockets[u32(guest_socket)] = host_fd;
  return guest_socket;
}

static GuestSocket accept_(int fd, sockaddr* addr, socklen_t* len)
{
  const auto guest_socket = GetAvailableGuestSocket();
  if (guest_socket == INVALID_GUEST_SOCKET)
    return INVALID_GUEST_SOCKET;

  const s32 host_fd = accept(fd, addr, len);
  if (host_fd < 0)
    return INVALID_GUEST_SOCKET;

  Common::SetPlatformSocketOptions(host_fd);

  s_sockets[u32(guest_socket)] = host_fd;
  return guest_socket;
}

static inline void PrintMBBuffer(u32 address, u32 length)
{
  const auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();

  for (u32 i = 0; i < length; i += 0x10)
  {
    DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM: {:08x} {:08x} {:08x} {:08x}", memory.Read_U32(address + i),
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

  s_game_modified_ip_address = {};

  s_board_status = LoadingGameProgram;
  s_load_progress = 80;

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
  sega_boot.ReadBytes(s_firmware.data(), length);

  s_test_menu = true;
}

void InitDIMM(const DiscIO::Volume& volume)
{
  // Load game into RAM, like on the actual Triforce.
  s_dimm_disc = DiscIO::CreateCachedBlobReader(volume.GetBlobReader().CopyReader());
}

static int PlatformPoll(std::span<WSAPOLLFD> pfds, std::chrono::milliseconds timeout)
{
#if defined(_WIN32)
  return WSAPoll(pfds.data(), ULONG(pfds.size()), INT(timeout.count()));
#else
  return UnixUtil::RetryOnEINTR(poll, pfds.data(), pfds.size(), timeout.count());
#endif
}

std::optional<ParsedIPOverride> ParseIPOverride(std::string_view str)
{
  // Everything after a space is the description.
  const auto ip_pair_str = std::string_view{str.begin(), std::ranges::find(str, ' ')};

  const auto parts = SplitStringIntoArray<2>(ip_pair_str, '=');
  if (!parts.has_value())
    return std::nullopt;

  const bool have_description = ip_pair_str.size() != str.size();

  return ParsedIPOverride{
      .original = (*parts)[0],
      .replacement = (*parts)[1],
      .description = have_description ? str.substr(ip_pair_str.size() + 1) : std::string_view{},
  };
}

// Caller should check if it matches first!
Common::IPv4Port IPAddressOverride::ApplyOverride(Common::IPv4Port subject) const
{
  // This logic could probably be better.
  // Ranges of different sizes will be weird in general.

  const auto replacement_first_ip_u32 = replacement.first.GetIPAddressValue();
  const auto ip_count = 1u + u64(replacement.last.GetIPAddressValue()) - replacement_first_ip_u32;
  const auto result_ip =
      u32(replacement_first_ip_u32 +
          ((subject.GetIPAddressValue() - original.first.GetIPAddressValue()) % ip_count));

  subject.ip_address = std::bit_cast<Common::IPAddress>(Common::BigEndianValue{result_ip});

  const auto replacement_first_port_u16 = replacement.first.GetPortValue();
  const auto port_count = 1u + u32(replacement.last.GetPortValue()) - replacement_first_port_u16;

  // If the replacement includes all ports then we don't alter the port.
  // This allows "10.0.0.1:80-88=10.0.0.2" to have the expected behavior.
  if (port_count != 65536u)
  {
    const auto result_port_u16 =
        u16(replacement_first_port_u16 +
            ((subject.GetPortValue() - original.first.GetPortValue()) % port_count));
    subject.port = std::bit_cast<u16>(Common::BigEndianValue{result_port_u16});
  }

  return subject;
}

Common::IPv4Port IPAddressOverride::ReverseOverride(Common::IPv4Port subject) const
{
  // Low effort implementation..
  return IPAddressOverride{.original = replacement, .replacement = original}.ApplyOverride(subject);
}

std::string IPAddressOverride::ToString() const
{
  return fmt::format("{}={}", original.ToString(), replacement.ToString());
}

IPOverrides GetIPOverrides()
{
  IPOverrides result;

  const auto ip_overrides_str = Config::Get(Config::MAIN_TRIFORCE_IP_OVERRIDES);
  for (auto&& ip_pair : ip_overrides_str | std::views::split(','))
  {
    const auto ip_pair_str = std::string_view{ip_pair};
    const auto parts = ParseIPOverride(ip_pair_str);
    if (parts.has_value())
    {
      const auto original = Common::StringToIPv4PortRange(parts->original);
      const auto replacement = Common::StringToIPv4PortRange(parts->replacement);

      if (original.has_value() && replacement.has_value())
      {
        result.emplace_back(*original, *replacement);
        continue;
      }

      ERROR_LOG_FMT(AMMEDIABOARD, "Bad IP pair string: {}", ip_pair_str);
    }
  }

  return result;
}

static std::optional<Common::IPv4Port> AdjustIPv4PortFromConfig(Common::IPv4Port subject)
{
  // TODO: We should parse this elsewhere to avoid repeated string manipulations.
  for (auto&& override : GetIPOverrides())
  {
    if (override.original.IsMatch(subject))
      return override.ApplyOverride(subject);
  }

  return std::nullopt;
}

static std::optional<Common::IPv4Port> ReverseAdjustIPv4PortFromConfig(Common::IPv4Port subject)
{
  // TODO: We should parse this elsewhere to avoid repeated string manipulations.
  for (auto&& override : GetIPOverrides())
  {
    if (override.replacement.IsMatch(subject))
      return override.ReverseOverride(subject);
  }

  return std::nullopt;
}

// Ports are in host byte order.
static bool BindEphemeralPort(SOCKET host_socket, Common::IPAddress ip_address,
                              u16 first_port_value, u16 last_port_value, u32 attempt_count)
{
  std::mt19937 rng(u32(Clock::now().time_since_epoch().count()));
  std::uniform_int_distribution<u16> port_distribution{first_port_value, last_port_value};

  sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_addr = std::bit_cast<in_addr>(ip_address),
  };

  while (attempt_count-- != 0)
  {
    const u16 port_value = port_distribution(rng);

    addr.sin_port = htons(port_value);

    const auto bind_result = bind(host_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    const int bind_err = WSAGetLastError();

    INFO_LOG_FMT(AMMEDIABOARD, "BindEphemeralPort: bind ({}:{}) = {} ({})",
                 Common::IPAddressToString(ip_address), port_value, bind_result,
                 Common::DecodeNetworkError(bind_err));

    if (bind_result == 0)
      return true;
  }

  return false;
}

static s32 NetDIMMConnect(GuestSocket guest_socket, const GuestSocketAddress& guest_addr)
{
  INFO_LOG_FMT(AMMEDIABOARD, "NetDIMMConnect: {}:{}",
               Common::IPAddressToString(guest_addr.ip_address), ntohs(guest_addr.port));

  sockaddr_in addr{
      .sin_family = guest_addr.ip_family,
  };

  // Adjust destination IP and port.
  const auto adjusted_ipv4port = AdjustIPv4PortFromConfig({guest_addr.ip_address, guest_addr.port});
  if (adjusted_ipv4port.has_value())
  {
    addr.sin_addr = std::bit_cast<in_addr>(adjusted_ipv4port->ip_address);
    addr.sin_port = adjusted_ipv4port->port;

    INFO_LOG_FMT(AMMEDIABOARD, "NetDIMMConnect: Overriding to: {}:{}",
                 Common::IPAddressToString(adjusted_ipv4port->ip_address),
                 ntohs(adjusted_ipv4port->port));
  }
  else
  {
    addr.sin_addr = std::bit_cast<in_addr>(guest_addr.ip_address);
    addr.sin_port = guest_addr.port;
  }

  const auto host_socket = GetHostSocket(guest_socket);

  // See if we have an override for the game modified IP.
  // If so, adjust the source IP by binding the socket.
  const auto adjusted_source_ipv4port = AdjustIPv4PortFromConfig({s_game_modified_ip_address, 0});
  if (adjusted_source_ipv4port.has_value())
  {
    // FYI: We don't handle the situation if games bind outgoing TCP themselves.
    // But I think that's unlikely.

    const u16 first_port_value = adjusted_source_ipv4port->GetPortValue();

    // If port zero is included then we don't care about the port number.
    const bool use_any_port = first_port_value == 0;
    const u32 attempt_count = use_any_port ? 1 : 10;

    // TODO: Handle the range properly. AdjustIPv4PortFromConfig should return a port range.
    // This magic 999 is here just to match our default config..
    const u16 last_port_value = use_any_port ? 0 : first_port_value + 999;

    const auto bind_result = BindEphemeralPort(host_socket, adjusted_source_ipv4port->ip_address,
                                               first_port_value, last_port_value, attempt_count);

    if (!bind_result)
    {
      s_last_error = SOCKET_ERROR;
      return SOCKET_ERROR;
    }
  }

  // Set socket to non-blocking
  {
    u_long val = 1;
    ioctlsocket(host_socket, FIONBIO, &val);
  }
  // Restore blocking mode
  Common::ScopeGuard guard{[&] {
    u_long val = 0;
    ioctlsocket(host_socket, FIONBIO, &val);
  }};

  const int connect_result =
      connect(host_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  const int err = WSAGetLastError();

  INFO_LOG_FMT(AMMEDIABOARD_NET, "NetDIMMConnect: connect( {}({}), ({},{}:{}) ):{} ({})",
               host_socket, u32(guest_socket), addr.sin_family, inet_ntoa(addr.sin_addr),
               Common::swap16(addr.sin_port), connect_result, err);

  if (connect_result == 0) [[unlikely]]
  {
    // Immediate success.
    s_last_error = SSC_SUCCESS;
    return 0;
  }

  if (err != WSAEWOULDBLOCK)
  {
    // Immediate failure (e.g. WSAECONNREFUSED)
    WARN_LOG_FMT(AMMEDIABOARD, "NetDIMMConnect: connect: {} ({})", err,
                 Common::DecodeNetworkError(err));

    s_last_error = SOCKET_ERROR;
    return SOCKET_ERROR;
  }

  WSAPOLLFD pfds[1]{{.fd = host_socket, .events = POLLOUT}};

  const auto timeout =
      duration_cast<std::chrono::milliseconds>(std::chrono::microseconds{s_timeouts[0]});

  const int poll_result = PlatformPoll(pfds, timeout);

  if (poll_result < 0) [[unlikely]]
  {
    // Poll failure.
    ERROR_LOG_FMT(AMMEDIABOARD, "NetDIMMConnect: PlatformPoll: {}", Common::StrNetworkError());

    s_last_error = SOCKET_ERROR;
    return SOCKET_ERROR;
  }

  if ((pfds[0].revents & POLLOUT) == 0)
  {
    // Timeout.
    s_last_error = SSC_EWOULDBLOCK;
    return SOCKET_ERROR;
  }

  int so_error = 0;
  socklen_t optlen = sizeof(so_error);
  const int getsockopt_result =
      getsockopt(host_socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &optlen);

  if (getsockopt_result != 0) [[unlikely]]
  {
    // getsockopt failure.
    ERROR_LOG_FMT(AMMEDIABOARD, "NetDIMMConnect: getsockopt: {}", Common::StrNetworkError());
  }
  else if (so_error == 0)
  {
    s_last_error = SSC_SUCCESS;
    return 0;
  }

  s_last_error = SOCKET_ERROR;
  return SOCKET_ERROR;
}

static GuestSocket NetDIMMAccept(GuestSocket guest_socket, u8* guest_addr_ptr,
                                 u8* guest_addrlen_ptr)
{
  // Either both parameters should be provided, or neither.
  if ((guest_addr_ptr != nullptr) != (guest_addrlen_ptr != nullptr))
  {
    ERROR_LOG_FMT(AMMEDIABOARD_NET, "NetDIMMAccept: bad parmeters");

    // TODO: Not hardware tested.
    s_last_error = SSC_EFAULT;
    return INVALID_GUEST_SOCKET;
  }

  const auto host_socket = GetHostSocket(guest_socket);
  WSAPOLLFD pfds[1]{{.fd = host_socket, .events = POLLIN}};

  // FYI: Currently using a 0ms timeout to make accept calls always non-blocking.
  constexpr auto timeout = std::chrono::milliseconds{0};

  DEBUG_LOG_FMT(AMMEDIABOARD, "NetDIMMAccept: {}({})", host_socket, int(guest_socket));

  const int poll_result = PlatformPoll(pfds, timeout);

  if (poll_result < 0) [[unlikely]]
  {
    // Poll failure.
    ERROR_LOG_FMT(AMMEDIABOARD, "NetDIMMAccept: PlatformPoll: {}", Common::StrNetworkError());

    s_last_error = SOCKET_ERROR;
    return INVALID_GUEST_SOCKET;
  }

  if ((pfds[0].revents & POLLIN) == 0)
  {
    // Timeout.
    DEBUG_LOG_FMT(AMMEDIABOARD, "NetDIMMAccept: Timeout.");

    s_last_error = SSC_EWOULDBLOCK;
    return INVALID_GUEST_SOCKET;
  }

  sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  const auto client_sock = accept_(host_socket, reinterpret_cast<sockaddr*>(&addr), &addrlen);

  if (client_sock == INVALID_GUEST_SOCKET)
  {
    ERROR_LOG_FMT(AMMEDIABOARD, "AMMBCommandAccept: accept: ({})", Common::StrNetworkError());
    s_last_error = SOCKET_ERROR;
    return INVALID_GUEST_SOCKET;
  }

  s_last_error = SSC_SUCCESS;

  NOTICE_LOG_FMT(AMMEDIABOARD, "AMMBCommandAccept: {}:{}",
                 Common::IPAddressToString(std::bit_cast<Common::IPAddress>(addr.sin_addr)),
                 ntohs(addr.sin_port));

  if (guest_addr_ptr == nullptr)
    return client_sock;

  GuestSocketAddress guest_addr{
      .ip_family = u8(addr.sin_family),
      .port = addr.sin_port,
      .ip_address = std::bit_cast<Common::IPAddress>(addr.sin_addr),
  };

  if (const auto adjusted_ipv4port =
          ReverseAdjustIPv4PortFromConfig({guest_addr.ip_address, guest_addr.port}))
  {
    guest_addr.ip_address = adjusted_ipv4port->ip_address;
    guest_addr.port = adjusted_ipv4port->port;

    NOTICE_LOG_FMT(AMMEDIABOARD, "AMMBCommandAccept: Translating result to: {}:{}",
                   Common::IPAddressToString(guest_addr.ip_address), ntohs(guest_addr.port));
  }

  const auto write_size =
      std::min<u32>(Common::BitCastPtr<u32>(guest_addrlen_ptr), sizeof(guest_addr));

  // Write out the addr.
  std::memcpy(guest_addr_ptr, &guest_addr, write_size);

  // Write out the addrlen.
  *guest_addrlen_ptr = sizeof(guest_addr);

  return client_sock;
}

static Common::IPv4Port GetAdjustedBindIPv4Port(Common::IPv4Port socket_addr)
{
  auto considered_ipv4 = socket_addr;

  if (std::bit_cast<u32>(considered_ipv4.ip_address) == INADDR_ANY)
  {
    // Because the game is binding to "0.0.0.0",
    //  use the "game modified" IP for override purposes.
    // If no override applies, then we still bind "0.0.0.0".
    considered_ipv4.ip_address = s_game_modified_ip_address;
    INFO_LOG_FMT(AMMEDIABOARD, "GetAdjustedBindIPv4Port: Considering game modified IP: {}",
                 Common::IPAddressToString(s_game_modified_ip_address));
  }

  if (const auto adjusted_ipv4 = AdjustIPv4PortFromConfig(considered_ipv4))
  {
    socket_addr = *adjusted_ipv4;
    INFO_LOG_FMT(AMMEDIABOARD, "GetAdjustedBindIPv4Port: Overriding to: {}:{}",
                 Common::IPAddressToString(socket_addr.ip_address), ntohs(socket_addr.port));
  }

  return socket_addr;
}

static u32 NetDIMMBind(GuestSocket guest_socket, const GuestSocketAddress& guest_addr)
{
  const auto host_socket = GetHostSocket(guest_socket);

  NOTICE_LOG_FMT(AMMEDIABOARD, "NetDIMMBind: {}({}) {}, {}, {}:{}", host_socket, int(guest_socket),
                 guest_addr.unknown_value, guest_addr.ip_family,
                 Common::IPAddressToString(guest_addr.ip_address), ntohs(guest_addr.port));

  const auto adjusted_ipv4port = GetAdjustedBindIPv4Port({guest_addr.ip_address, guest_addr.port});

  sockaddr_in addr{
      .sin_family = guest_addr.ip_family,
      .sin_port = adjusted_ipv4port.port,
      .sin_addr = std::bit_cast<in_addr>(adjusted_ipv4port.ip_address),
  };

  const int bind_result = bind(host_socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
  const int err = WSAGetLastError();

  INFO_LOG_FMT(AMMEDIABOARD_NET, "NetDIMMBind: bind( {}({}), ({},{}:{}) ):{}", host_socket,
               u32(guest_socket), addr.sin_family,
               Common::IPAddressToString(adjusted_ipv4port.ip_address),
               Common::swap16(adjusted_ipv4port.port), bind_result);

  if (bind_result < 0)
  {
    const auto* const err_msg = Common::DecodeNetworkError(err);
    ERROR_LOG_FMT(AMMEDIABOARD, "NetDIMMBind bind() = {} ({})", err, err_msg);

    PanicAlertFmt("Failed to bind socket {}:{}\nError: {} ({})",
                  Common::IPAddressToString(adjusted_ipv4port.ip_address),
                  ntohs(adjusted_ipv4port.port), err, err_msg);
  }

  return bind_result;
}

static void AMMBCommandRecv(u32 parameter_offset, u32 network_buffer_base)
{
  const auto fd = GetHostSocket(GuestSocket(s_media_buffer_32[parameter_offset]));
  u32 off = s_media_buffer_32[parameter_offset + 1];
  auto len = std::min<u32>(s_media_buffer_32[parameter_offset + 2], sizeof(s_network_buffer));
  const u64 off_len = u64(off) + len;

  if (off >= network_buffer_base && off_len <= network_buffer_base + sizeof(s_network_buffer))
  {
    off -= network_buffer_base;
  }
  else if (off_len > sizeof(s_network_buffer))
  {
    ERROR_LOG_FMT(AMMEDIABOARD_NET,
                  "GC-AM: recv(error) invalid destination or length: off={:08x}, len={}", off, len);
    off = 0;
    len = 0;
  }

  int ret = recv(fd, reinterpret_cast<char*>(s_network_buffer.data() + off), len, 0);
  const int err = WSAGetLastError();

  DEBUG_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: recv( {}, 0x{:08x}, {} ):{} {}", fd, off, len, ret, err);

  s_media_buffer[1] = s_media_buffer[8];
  s_media_buffer_32[1] = ret;
}

static void AMMBCommandSend(u32 parameter_offset, u32 network_buffer_base)
{
  const auto guest_socket = GuestSocket(s_media_buffer_32[parameter_offset]);
  const auto fd = GetHostSocket(guest_socket);
  u32 off = s_media_buffer_32[parameter_offset + 1];
  auto len = std::min<u32>(s_media_buffer_32[parameter_offset + 2], sizeof(s_network_buffer));
  const u64 off_len = u64(off) + len;

  if (off >= network_buffer_base && off_len <= network_buffer_base + sizeof(s_network_buffer))
  {
    off -= network_buffer_base;
  }
  else if (off_len > sizeof(s_network_buffer))
  {
    ERROR_LOG_FMT(AMMEDIABOARD_NET,
                  "GC-AM: send(error) unhandled destination or length: {:08x}, len={}", off, len);
    off = 0;
    len = 0;
  }

  const int ret = send(fd, reinterpret_cast<char*>(s_network_buffer.data() + off), len, SEND_FLAGS);
  const int err = WSAGetLastError();

  DEBUG_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: send( {}({}), 0x{:08x}, {} ): {} {}", fd,
                u32(guest_socket), off, len, ret, err);

  s_media_buffer[1] = s_media_buffer[8];
  s_media_buffer_32[1] = ret;
}

static void AMMBCommandSocket(u32 parameter_offset)
{
  // Protocol is not sent (determined automatically).
  const u32 domain = s_media_buffer_32[parameter_offset];
  const u32 type = s_media_buffer_32[parameter_offset + 1];

  const GuestSocket guest_socket = socket_(int(domain), int(type), 0);

  NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: socket( {}, {} ):{}", domain, type, u32(guest_socket));

  s_media_buffer[1] = 0;
  s_media_buffer_32[1] = u32(guest_socket);
}

static void AMMBCommandClosesocket(u32 parameter_offset)
{
  const auto guest_socket = GuestSocket(s_media_buffer_32[parameter_offset]);
  const auto fd = GetHostSocket(guest_socket);

  const int ret = closesocket(fd);

  NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: closesocket( {}({}) ):{}", fd, u32(guest_socket), ret);

  if (u32(guest_socket) < std::size(s_sockets))
    s_sockets[u32(guest_socket)] = SOCKET_ERROR;

  s_media_buffer_32[1] = ret;
  s_last_error = SSC_SUCCESS;
}

static void AMMBCommandConnect(u32 parameter_offset, u32 network_buffer_base)
{
  const auto guest_socket = GuestSocket(s_media_buffer_32[parameter_offset + 0]);
  const u32 addr_offset = s_media_buffer_32[parameter_offset + 1];
  const u32 len = s_media_buffer_32[parameter_offset + 2];

  GuestSocketAddress addr;

  if (len != sizeof(addr))
  {
    ERROR_LOG_FMT(AMMEDIABOARD_NET, "AMMBCommandConnect: Unexpected length: {}", len);
    return;
  }

  const auto* addr_ptr =
      GetSafePtr(s_network_command_buffer, network_buffer_base, addr_offset, sizeof(addr));
  if (addr_ptr == nullptr)
    return;

  memcpy(&addr, addr_ptr, sizeof(addr));

  const int ret = NetDIMMConnect(guest_socket, addr);

  s_media_buffer[1] = s_media_buffer[8];
  s_media_buffer_32[1] = ret;
}

static void AMMBCommandAccept(u32 parameter_offset, u32 network_buffer_base)
{
  const auto guest_socket = GuestSocket(s_media_buffer_32[parameter_offset]);
  const u32 addr_off = s_media_buffer_32[parameter_offset + 1];
  const u32 addrlen_off = s_media_buffer_32[parameter_offset + 2];

  auto* const addrlen_ptr =
      GetSafePtr(s_network_command_buffer, network_buffer_base, addrlen_off, sizeof(u32));

  auto* const addr_ptr = (addrlen_ptr == nullptr) ?
                             nullptr :
                             GetSafePtr(s_network_command_buffer, network_buffer_base, addr_off,
                                        Common::BitCastPtr<u32>(addrlen_ptr));

  const auto accept_result = NetDIMMAccept(guest_socket, addr_ptr, addrlen_ptr);

  s_media_buffer_32[1] = u32(accept_result);
}

static void AMMBCommandBind()
{
  const auto guest_socket = GuestSocket(s_media_buffer_32[2]);
  const u32 addr_offset = s_media_buffer_32[3];
  const u32 len = s_media_buffer_32[4];

  GuestSocketAddress guest_addr;

  if (len != sizeof(guest_addr))
  {
    ERROR_LOG_FMT(AMMEDIABOARD_NET, "AMMBCommandBind: Unexpected length: {}", len);
    return;
  }

  const auto* addr_ptr =
      GetSafePtr(s_network_command_buffer, NetworkCommandAddress2, addr_offset, sizeof(guest_addr));
  if (addr_ptr == nullptr)
    return;

  memcpy(&guest_addr, addr_ptr, sizeof(guest_addr));

  const auto bind_result = NetDIMMBind(guest_socket, guest_addr);

  s_media_buffer_32[1] = bind_result;
  s_last_error = SSC_SUCCESS;
}

// Expects a pointer to a GuestFdSet or nullptr.
static void FillPollFdsFromGuestFdSet(std::span<WSAPOLLFD> pfds, const void* guest_fds_ptr,
                                      short requested_events)
{
  if (guest_fds_ptr == nullptr)
    return;

  GuestFdSet guest_fds;
  std::memcpy(&guest_fds, guest_fds_ptr, sizeof(guest_fds));

  u32 index = 0;
  for (auto& pfd : pfds)
  {
    const auto guest_socket = GuestSocket(index);
    if (guest_fds.IsFdSet(guest_socket))
    {
      pfd.fd = GetHostSocket(guest_socket);
      pfd.events |= requested_events;
    }

    ++index;
  }
}

// Expects a pointer to a GuestFdSet or nullptr.
static void WriteGuestFdSetFromPollFds(void* guest_fds_ptr, std::span<const WSAPOLLFD> fds,
                                       short returned_events)
{
  if (guest_fds_ptr == nullptr)
    return;

  GuestFdSet guest_fds;

  for (const auto& fd : fds)
  {
    if ((fd.revents & returned_events) == 0)
      continue;

    const auto guest_socket = GetGuestSocket(fd.fd);
    if (guest_socket == INVALID_GUEST_SOCKET)
      continue;

    guest_fds.SetFd(guest_socket);
  }

  std::memcpy(guest_fds_ptr, &guest_fds, sizeof(guest_fds));
}

static void AMMBCommandSelect(u32 parameter_offset, u32 network_buffer_base)
{
  u32 nfds = int(s_media_buffer_32[parameter_offset]);

  const u32 readfds_offset = s_media_buffer_32[parameter_offset + 1];
  auto* const guest_readfds_ptr =
      GetSafePtr(s_network_command_buffer, network_buffer_base, readfds_offset, sizeof(GuestFdSet));

  const u32 writefds_offset = s_media_buffer_32[parameter_offset + 2];
  auto* const guest_writefds_ptr = GetSafePtr(s_network_command_buffer, network_buffer_base,
                                              writefds_offset, sizeof(GuestFdSet));

  const u32 exceptfds_offset = s_media_buffer_32[parameter_offset + 3];
  auto* const guest_exceptfds_ptr = GetSafePtr(s_network_command_buffer, network_buffer_base,
                                               exceptfds_offset, sizeof(GuestFdSet));

  const u32 timeout_offset = s_media_buffer_32[parameter_offset + 4];
  auto* const guest_timeout_ptr =
      GetSafePtr(s_network_command_buffer, network_buffer_base, timeout_offset, sizeof(TimeVal));

  // Games sometimes send 256 (the bit size of GuestFdSet).
  nfds = std::min<u32>(nfds, std::size(s_sockets));

  std::chrono::milliseconds timeout{-1};
  if (guest_timeout_ptr != nullptr)
  {
    TimeVal guest_timeout;
    std::memcpy(&guest_timeout, guest_timeout_ptr, sizeof(guest_timeout));

    timeout = duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds(guest_timeout.seconds) +
        std::chrono::microseconds(guest_timeout.microseconds));
  }

  if (timeout < std::chrono::milliseconds{})
  {
    // TODO: We should have a way to break out of any timeout on shutdown.
    // e.g. include a "wakeup" socket in each `poll` call.
    WARN_LOG_FMT(AMMEDIABOARD, "AMMBCommandSelect: Infinite timout!");
  }

  DEBUG_LOG_FMT(AMMEDIABOARD_NET,
                "GC-AM: select( {}, 0x{:08x} 0x{:08x} 0x{:08x} 0x{:08x} ) timeout={}", nfds,
                readfds_offset, writefds_offset, exceptfds_offset, timeout_offset, timeout.count());

  // Fill with the host sockets for each guest socket less-than `nfds` in each GuestFdSet.
  std::vector<WSAPOLLFD> pollfds(nfds, WSAPOLLFD{.fd = INVALID_SOCKET});

  FillPollFdsFromGuestFdSet(pollfds, guest_readfds_ptr, POLLIN);
  FillPollFdsFromGuestFdSet(pollfds, guest_writefds_ptr, POLLOUT);
  FillPollFdsFromGuestFdSet(pollfds, guest_exceptfds_ptr, POLLPRI);

  // Erase "INVALID" entries and also entries that weren't in any GuestFdSet.
  std::erase_if(pollfds, [](const WSAPOLLFD& fd) { return fd.fd == INVALID_SOCKET; });

  // TODO: There may be some edge cases where
  // poll's (POLLIN,POLLOUT,POLLPRI) don't map 1:1 with select's (readfds,writefds,exceptfds).

  DEBUG_LOG_FMT(AMMEDIABOARD, "AMMBCommandSelect: Polling with socket count: {}", pollfds.size());

  const int ret = PlatformPoll(pollfds, timeout);

  if (ret >= 0)
  {
    WriteGuestFdSetFromPollFds(guest_readfds_ptr, pollfds, POLLIN);
    WriteGuestFdSetFromPollFds(guest_writefds_ptr, pollfds, POLLOUT);
    WriteGuestFdSetFromPollFds(guest_exceptfds_ptr, pollfds, POLLPRI);
  }

  DEBUG_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: select result: {}", ret);

  s_media_buffer[1] = 0;
  s_media_buffer_32[1] = ret;
}

static void AMMBCommandSetSockOpt(u32 parameter_offset, u32 network_buffer_base)
{
  const auto fd = GetHostSocket(GuestSocket(s_media_buffer_32[parameter_offset]));
  const int level = static_cast<int>(s_media_buffer_32[parameter_offset + 1]);
  const int optname = static_cast<int>(s_media_buffer_32[parameter_offset + 2]);
  const u32 optval_offset = s_media_buffer_32[parameter_offset + 3] - network_buffer_base;
  const int optlen = static_cast<int>(s_media_buffer_32[parameter_offset + 4]);

  if (!NetworkCMDBufferCheck(optval_offset, optlen))
  {
    return;
  }

  const char* optval = reinterpret_cast<char*>(s_network_command_buffer.data() + optval_offset);

  // TODO: Ensure parameters are compatible with host's setsockopt
  const int ret = setsockopt(fd, level, optname, optval, optlen);
  const int err = WSAGetLastError();

  NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: setsockopt( {:d}, {:04x}, {}, {:p}, {} ):{:d} ({})", fd,
                 level, optname, optval, optlen, ret, err);

  s_media_buffer[1] = s_media_buffer[8];
  s_media_buffer_32[1] = ret;
}

static void AMMBCommandModifyMyIPaddr(u32 parameter_offset, u32 network_buffer_base)
{
  const u32 ip_address_offset = s_media_buffer_32[parameter_offset];
  const auto ip_address_str = GetSafeString(s_network_command_buffer, network_buffer_base,
                                            ip_address_offset, MAX_IPV4_STRING_LENGTH);

  NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: modifyMyIPaddr({})", ip_address_str);

  if (const auto parse_result = Common::StringToIPv4PortRange(ip_address_str))
    s_game_modified_ip_address = parse_result->first.ip_address;
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
        PanicAlertFmtT("Unhandled Media Board Read: offset={0:08x} length={0:08x}", offset, length);
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
      SafeCopyToEmu(memory, address, reinterpret_cast<const u8*>(s_allnet_reply),
                    sizeof(s_allnet_reply), offset - AllNetBuffer, sizeof(s_allnet_reply));
      return 0;
    }

    for (const auto& range : s_mediaboard_ranges)
    {
      if (offset >= range.start && offset < range.end)
      {
        DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM: Read MediaBoard ({:08x},{:08x},{:08x})", offset,
                      range.start, length);
        SafeCopyToEmu(memory, address, range.buffer, range.buffer_size, offset - range.start,
                      length);
        PrintMBBuffer(address, length);
        return 0;
      }
    }

    if (offset == DIMMCommandExecute2)
    {
      const AMMBCommand ammb_command = Common::BitCastPtr<AMMBCommand>(s_media_buffer + 0x202);

      DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM: Execute command: (2){0:04X}",
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
        AMMBCommandAccept(2, NetworkCommandAddress2);
        break;
      case AMMBCommand::Bind:
        AMMBCommandBind();
        break;
      case AMMBCommand::Closesocket:
        AMMBCommandClosesocket(2);
        break;
      case AMMBCommand::Connect:
        AMMBCommandConnect(2, NetworkCommandAddress2);
        break;
      case AMMBCommand::InetAddr:
      {
        const char* ip_address = reinterpret_cast<char*>(s_network_command_buffer.data());

        // IP address shouldn't be longer than 15
        // TODO: Shouldn't this look at 16 characters for lack of null-termination?
        if (strnlen(ip_address, MAX_IPV4_STRING_LENGTH) > MAX_IPV4_STRING_LENGTH)
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: Invalid size for address: InetAddr():{}",
                        strlen(ip_address));
          break;
        }

        const u32 ip = inet_addr(ip_address);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: InetAddr( {} )", ip_address);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ip;
        break;
      }
      case AMMBCommand::Listen:
      {
        const auto fd = GetHostSocket(GuestSocket(s_media_buffer_32[2]));
        const u32 backlog = s_media_buffer_32[3];

        const int ret = listen(fd, backlog);

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: listen( {}, {} ):{:d}", fd, backlog, ret);

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::Recv:
        AMMBCommandRecv(2, NetworkBufferAddress4);
        break;
      case AMMBCommand::Send:
        AMMBCommandSend(2, NetworkBufferAddress3);
        break;
      case AMMBCommand::Socket:
        AMMBCommandSocket(2);
        break;
      case AMMBCommand::Select:
        AMMBCommandSelect(2, NetworkCommandAddress2);
        break;
      case AMMBCommand::SetSockOpt:
        AMMBCommandSetSockOpt(2, NetworkCommandAddress2);
        break;
      case AMMBCommand::SetTimeOuts:
      {
        const auto guest_socket = GuestSocket(s_media_buffer_32[2]);
        const auto host_socket = GetHostSocket(guest_socket);
        const u32 timeout_a = s_media_buffer_32[3];
        const u32 timeout_b = s_media_buffer_32[4];
        const u32 timeout_c = s_media_buffer_32[5];

        s_timeouts[0] = timeout_a;
        s_timeouts[1] = timeout_b;
        s_timeouts[2] = timeout_c;

        int ret = SOCKET_ERROR;

        if (host_socket != INVALID_SOCKET)
        {
          ret = setsockopt(host_socket, SOL_SOCKET, SO_SNDTIMEO,
                           reinterpret_cast<const char*>(&timeout_b), sizeof(int));
          if (ret < 0)
          {
            ret = WSAGetLastError();
          }
          else
          {
            ret = setsockopt(host_socket, SOL_SOCKET, SO_RCVTIMEO,
                             reinterpret_cast<const char*>(&timeout_c), sizeof(int));
            if (ret < 0)
              ret = WSAGetLastError();
          }

          INFO_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: SetTimeOuts( {}({}), {}, {}, {} ):{}", host_socket,
                       int(guest_socket), timeout_a, timeout_b, timeout_c, ret);
        }
        else
        {
          ERROR_LOG_FMT(AMMEDIABOARD_NET,
                        "GC-AM: Invalid Socket: SetTimeOuts( {}({}), {}, {}, {} ):{}", host_socket,
                        int(guest_socket), timeout_a, timeout_b, timeout_c, ret);
        }

        s_media_buffer[1] = s_media_buffer[8];
        s_media_buffer_32[1] = ret;
        break;
      }
      case AMMBCommand::GetParambyDHCPExec:
      {
        const u32 value = s_media_buffer_32[2];

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: GetParambyDHCPExec({})", value);

        s_media_buffer[1] = 0;
        s_media_buffer_32[1] = 0;
        break;
      }
      case AMMBCommand::ModifyMyIPaddr:
        AMMBCommandModifyMyIPaddr(2, NetworkCommandAddress2);
        break;
      case AMMBCommand::GetLastError:
      {
        const auto guest_socket = GuestSocket(s_media_buffer_32[2]);
        const auto host_socket = GetHostSocket(guest_socket);

        DEBUG_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: GetLastError( {}({}) ):{}", host_socket,
                      int(guest_socket), int(s_last_error));

        // Good enough, assuming it's called for the same socket right after an error.
        // TODO: Implement something similar per socket.
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
      PanicAlertFmtT("Unhandled Media Board Read: offset={0:08x} length={0:08x}", offset, length);
      return 0;
    }

    if (s_firmware_map)
    {
      if (!SafeCopyToEmu(memory, address, s_firmware.data(), s_firmware.size(), offset, length))
      {
        ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid firmware buffer range: offset={}, length={}",
                      offset, length);
      }
      return 0;
    }

    if (const auto span = memory.GetSpanForAddress(address); span.size() < length)
    {
      ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid DIMM Disc read from: offset={}, length={}",
                    offset, length);
    }
    else if (s_dimm_disc->Read(offset, length, span.data()))
    {
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
      if (offset >= 0x00400000 && offset <= 0x600000)
      {
        const u32 fw_offset = offset - 0x00400000;
        if (!SafeCopyFromEmu(memory, s_firmware.data(), address, s_firmware.size(), fw_offset,
                             length))
        {
          ERROR_LOG_FMT(AMMEDIABOARD, "GC-AM: Invalid firmware write: offset={}, length={}",
                        fw_offset, length);
        }
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

        DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM: Execute command: (1):{0:04X}",
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
        DEBUG_LOG_FMT(AMMEDIABOARD, "GC-AM: Write MediaBoard ({:08x},{:08x},{:08x})", offset,
                      range.start, length);
        SafeCopyFromEmu(memory, range.buffer, address, range.buffer_size, offset - range.start,
                        length);
        PrintMBBuffer(address, length);
        return 0;
      }
    }

    // Max GC disc offset
    if (offset >= 0x57058000)
    {
      PrintMBBuffer(address, length);
      PanicAlertFmtT("Unhandled Media Board Write: offset={0:08x} length={0:08x}", offset, length);
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
        s_media_buffer_32[1] = s_board_status;
        s_media_buffer_32[2] = s_load_progress;
        if (s_load_progress < 100)
        {
          s_load_progress++;
        }
        else
        {
          s_board_status = LoadedGameProgram;
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
        AMMBCommandClosesocket(10);
        break;
      case AMMBCommand::Connect:
        AMMBCommandConnect(10, NetworkCommandAddress1);
        break;
      case AMMBCommand::Recv:
        AMMBCommandRecv(10, NetworkBufferAddress5);
        break;
      case AMMBCommand::Send:
        AMMBCommandSend(10, NetworkBufferAddress1);
        break;
      case AMMBCommand::Socket:
        AMMBCommandSocket(10);
        break;
      case AMMBCommand::Select:
        AMMBCommandSelect(10, NetworkCommandAddress1);
        break;
      case AMMBCommand::SetSockOpt:
        AMMBCommandSetSockOpt(10, NetworkCommandAddress1);
        break;
      case AMMBCommand::ModifyMyIPaddr:
        AMMBCommandModifyMyIPaddr(10, NetworkCommandAddress1);
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
        const auto addra = std::bit_cast<Common::IPAddress>(s_media_buffer_32[12]);
        const auto addrb = std::bit_cast<Common::IPAddress>(s_media_buffer_32[13]);

        const u16 size = s_media_buffer[0x24] | s_media_buffer[0x25] << 8;
        const u16 port = Common::swap16(s_media_buffer[0x27] | s_media_buffer[0x26] << 8);
        const u16 unknown = s_media_buffer[0x2D] | s_media_buffer[0x2C] << 8;

        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM: SetupLink:");
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:  Size: ({}) ", size);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:  Port: ({})", port);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:LinkNum:({:02x})", s_media_buffer[0x28]);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        ({:02x})", s_media_buffer[0x2A]);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:        ({:04x})", unknown);
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:   IP:  ({})",
                       Common::IPAddressToString(addra));  // IP ?
        NOTICE_LOG_FMT(AMMEDIABOARD_NET, "GC-AM:   IP:  ({})",
                       Common::IPAddressToString(addrb));  // Target IP
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

        const u8* data = s_network_buffer.data() + (off + addr - NetworkBufferAddress2);

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

static void CloseAllSockets()
{
  for (u32 i = FIRST_VALID_FD; i < std::size(s_sockets); ++i)
  {
    if (s_sockets[i] != SOCKET_ERROR)
    {
      closesocket(s_sockets[i]);
      s_sockets[i] = SOCKET_ERROR;
    }
  }
}

void Shutdown()
{
  s_netcfg.Close();
  s_netctrl.Close();
  s_extra.Close();
  s_backup.Close();
  s_dimm.Close();

  s_dimm_disc.reset();

  CloseAllSockets();
}

void DoState(PointerWrap& p)
{
  p.Do(s_firmware_map);
  p.Do(s_test_menu);
  p.Do(s_timeouts);
  p.Do(s_last_error);
  p.Do(s_gcam_key_a);
  p.Do(s_gcam_key_b);
  p.Do(s_gcam_key_c);
  p.Do(s_firmware);
  p.Do(s_media_buffer_32);
  p.Do(s_network_command_buffer);
  p.Do(s_network_buffer);
  p.Do(s_allnet_buffer);
  p.Do(s_allnet_settings);

  p.Do(s_game_modified_ip_address);

  p.Do(s_board_status);
  p.Do(s_load_progress);

  // TODO: Handle the files better.
  // Data corruption is probably currently possible.

  // s_netcfg
  // s_netctrl
  // s_extra
  // s_backup
  // s_dimm

  // TODO: Handle sockets better.
  // For now, we just recreate a TCP socket for any socket that existed.
  // We should probably re-bind sockets and handle UDP sockets.

  GuestFdSet created_sockets{};
  if (p.IsWriteMode() || p.IsVerifyMode())
  {
    for (u32 i = FIRST_VALID_FD; i < std::size(s_sockets); ++i)
    {
      if (s_sockets[i] != SOCKET_ERROR)
        created_sockets.SetFd(GuestSocket(i));
    }
  }

  p.Do(created_sockets);

  if (p.IsReadMode())
  {
    CloseAllSockets();

    for (u32 i = FIRST_VALID_FD; i < std::size(s_sockets); ++i)
    {
      if (!created_sockets.IsFdSet(GuestSocket(i)))
        continue;

      s_sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
    }
  }
}

s32 DebuggerGetSocket(u32 triforce_fd)
{
  if (triforce_fd < std::size(s_sockets))
    return s32(s_sockets[triforce_fd]);

  WARN_LOG_FMT(AMMEDIABOARD, "GC-AM: Bad socket fd used by the debugger: {}", triforce_fd);
  return -1;
}
}  // namespace AMMediaboard
