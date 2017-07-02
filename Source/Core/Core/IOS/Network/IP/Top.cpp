// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/IP/Top.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <netdb.h>
#include <poll.h>
#endif

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Network.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/Network/ICMP.h"
#include "Core/IOS/Network/MACUtils.h"
#include "Core/IOS/Network/Socket.h"

#ifdef _WIN32
#include <iphlpapi.h>
#include <ws2tcpip.h>

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#elif defined(__linux__) or defined(__APPLE__)
#include <netinet/in.h>
#include <sys/socket.h>

typedef struct pollfd pollfd_t;
#else
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

// WSAPoll doesn't support POLLPRI and POLLWRBAND flags
#ifdef _WIN32
#define UNSUPPORTED_WSAPOLL POLLPRI | POLLWRBAND
#else
#define UNSUPPORTED_WSAPOLL 0
#endif

namespace IOS
{
namespace HLE
{
namespace Device
{
NetIPTop::NetIPTop(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
#ifdef _WIN32
  int ret = WSAStartup(MAKEWORD(2, 2), &InitData);
  INFO_LOG(IOS_NET, "WSAStartup: %d", ret);
#endif
}

NetIPTop::~NetIPTop()
{
#ifdef _WIN32
  WSACleanup();
#endif
}

static constexpr u32 inet_addr(u8 a, u8 b, u8 c, u8 d)
{
  return (static_cast<u32>(a) << 24) | (static_cast<u32>(b) << 16) | (static_cast<u32>(c) << 8) | d;
}

static int inet_pton(const char* src, unsigned char* dst)
{
  int saw_digit, octets;
  char ch;
  unsigned char tmp[4], *tp;

  saw_digit = 0;
  octets = 0;
  *(tp = tmp) = 0;
  while ((ch = *src++) != '\0')
  {
    if (ch >= '0' && ch <= '9')
    {
      unsigned int newt = (*tp * 10) + (ch - '0');

      if (newt > 255)
        return 0;
      *tp = newt;
      if (!saw_digit)
      {
        if (++octets > 4)
          return 0;
        saw_digit = 1;
      }
    }
    else if (ch == '.' && saw_digit)
    {
      if (octets == 4)
        return 0;
      *++tp = 0;
      saw_digit = 0;
    }
    else
    {
      return 0;
    }
  }
  if (octets < 4)
    return 0;
  memcpy(dst, tmp, 4);
  return 1;
}

// Maps SOCKOPT level from Wii to native
static s32 MapWiiSockOptLevelToNative(u32 level)
{
  if (level == 0xFFFF)
    return SOL_SOCKET;

  INFO_LOG(IOS_NET, "SO_SETSOCKOPT: unknown level %u", level);
  return level;
}

// Maps SOCKOPT optname from native to Wii
static s32 MapWiiSockOptNameToNative(u32 optname)
{
  switch (optname)
  {
  case 0x4:
    return SO_REUSEADDR;
  case 0x1001:
    return SO_SNDBUF;
  case 0x1002:
    return SO_RCVBUF;
  case 0x1009:
    return SO_ERROR;
  }

  INFO_LOG(IOS_NET, "SO_SETSOCKOPT: unknown optname %u", optname);
  return optname;
}

IPCCommandResult NetIPTop::IOCtl(const IOCtlRequest& request)
{
  if (Core::WantsDeterminism())
  {
    return GetDefaultReply(IPC_EACCES);
  }

  switch (request.request)
  {
  case IOCTL_SO_STARTUP:
    return HandleStartUpRequest(request);
  case IOCTL_SO_SOCKET:
    return HandleSocketRequest(request);
  case IOCTL_SO_ICMPSOCKET:
    return HandleICMPSocketRequest(request);
  case IOCTL_SO_CLOSE:
  case IOCTL_SO_ICMPCLOSE:
    return HandleCloseRequest(request);
  case IOCTL_SO_ACCEPT:
  case IOCTL_SO_BIND:
  case IOCTL_SO_CONNECT:
  case IOCTL_SO_FCNTL:
    return HandleDoSockRequest(request);
  case IOCTL_SO_SHUTDOWN:
    return HandleShutdownRequest(request);
  case IOCTL_SO_LISTEN:
    return HandleListenRequest(request);
  case IOCTL_SO_GETSOCKOPT:
    return HandleGetSockOptRequest(request);
  case IOCTL_SO_SETSOCKOPT:
    return HandleSetSockOptRequest(request);
  case IOCTL_SO_GETSOCKNAME:
    return HandleGetSockNameRequest(request);
  case IOCTL_SO_GETPEERNAME:
    return HandleGetPeerNameRequest(request);
  case IOCTL_SO_GETHOSTID:
    return HandleGetHostIDRequest(request);
  case IOCTL_SO_INETATON:
    return HandleInetAToNRequest(request);
  case IOCTL_SO_INETPTON:
    return HandleInetPToNRequest(request);
  case IOCTL_SO_INETNTOP:
    return HandleInetNToPRequest(request);
  case IOCTL_SO_POLL:
    return HandlePollRequest(request);
  case IOCTL_SO_GETHOSTBYNAME:
    return HandleGetHostByNameRequest(request);
  case IOCTL_SO_ICMPCANCEL:
    return HandleICMPCancelRequest(request);
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_NET);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult NetIPTop::IOCtlV(const IOCtlVRequest& request)
{
  switch (request.request)
  {
  case IOCTLV_SO_GETINTERFACEOPT:
    return HandleGetInterfaceOptRequest(request);
  case IOCTLV_SO_SENDTO:
    return HandleSendToRequest(request);
  case IOCTLV_SO_RECVFROM:
    return HandleRecvFromRequest(request);
  case IOCTLV_SO_GETADDRINFO:
    return HandleGetAddressInfoRequest(request);
  case IOCTLV_SO_ICMPPING:
    return HandleICMPPingRequest(request);
  default:
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS_NET);
    break;
  }

  return GetDefaultReply(IPC_SUCCESS);
}

void NetIPTop::Update()
{
  WiiSockMan::GetInstance().Update();
}

IPCCommandResult NetIPTop::HandleStartUpRequest(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_WC24);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult NetIPTop::HandleSocketRequest(const IOCtlRequest& request)
{
  u32 af = Memory::Read_U32(request.buffer_in);
  u32 type = Memory::Read_U32(request.buffer_in + 4);
  u32 prot = Memory::Read_U32(request.buffer_in + 8);

  WiiSockMan& sm = WiiSockMan::GetInstance();
  const s32 return_value = sm.NewSocket(af, type, prot);
  INFO_LOG(IOS_NET, "IOCTL_SO_SOCKET "
                    "Socket: %08x (%d,%d,%d), BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
           return_value, af, type, prot, request.buffer_in, request.buffer_in_size,
           request.buffer_out, request.buffer_out_size);

  return GetDefaultReply(return_value);
}

IPCCommandResult NetIPTop::HandleICMPSocketRequest(const IOCtlRequest& request)
{
  u32 pf = Memory::Read_U32(request.buffer_in);

  WiiSockMan& sm = WiiSockMan::GetInstance();
  const s32 return_value = sm.NewSocket(pf, SOCK_RAW, IPPROTO_ICMP);
  INFO_LOG(IOS_NET, "IOCTL_SO_ICMPSOCKET(%x) %d", pf, return_value);
  return GetDefaultReply(return_value);
}

IPCCommandResult NetIPTop::HandleCloseRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_in);
  WiiSockMan& sm = WiiSockMan::GetInstance();
  const s32 return_value = sm.DeleteSocket(fd);
  INFO_LOG(IOS_NET, "%s(%x) %x",
           request.request == IOCTL_SO_ICMPCLOSE ? "IOCTL_SO_ICMPCLOSE" : "IOCTL_SO_CLOSE", fd,
           return_value);

  return GetDefaultReply(return_value);
}

IPCCommandResult NetIPTop::HandleDoSockRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_in);
  WiiSockMan& sm = WiiSockMan::GetInstance();
  sm.DoSock(fd, request, static_cast<NET_IOCTL>(request.request));
  return GetNoReply();
}

IPCCommandResult NetIPTop::HandleShutdownRequest(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_WC24);

  u32 fd = Memory::Read_U32(request.buffer_in);
  u32 how = Memory::Read_U32(request.buffer_in + 4);
  int ret = shutdown(WiiSockMan::GetInstance().GetHostSocket(fd), how);

  return GetDefaultReply(WiiSockMan::GetNetErrorCode(ret, "SO_SHUTDOWN", false));
}

IPCCommandResult NetIPTop::HandleListenRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_in);
  u32 BACKLOG = Memory::Read_U32(request.buffer_in + 0x04);
  u32 ret = listen(WiiSockMan::GetInstance().GetHostSocket(fd), BACKLOG);

  request.Log(GetDeviceName(), LogTypes::IOS_WC24);
  return GetDefaultReply(WiiSockMan::GetNetErrorCode(ret, "SO_LISTEN", false));
}

IPCCommandResult NetIPTop::HandleGetSockOptRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_out);
  u32 level = Memory::Read_U32(request.buffer_out + 4);
  u32 optname = Memory::Read_U32(request.buffer_out + 8);

  request.Log(GetDeviceName(), LogTypes::IOS_WC24);

  // Do the level/optname translation
  int nat_level = MapWiiSockOptLevelToNative(level);
  int nat_optname = MapWiiSockOptNameToNative(optname);

  u8 optval[20];
  u32 optlen = 4;

  int ret = getsockopt(WiiSockMan::GetInstance().GetHostSocket(fd), nat_level, nat_optname,
                       (char*)&optval, (socklen_t*)&optlen);
  const s32 return_value = WiiSockMan::GetNetErrorCode(ret, "SO_GETSOCKOPT", false);

  Memory::Write_U32(optlen, request.buffer_out + 0xC);
  Memory::CopyToEmu(request.buffer_out + 0x10, optval, optlen);

  if (optname == SO_ERROR)
  {
    s32 last_error = WiiSockMan::GetInstance().GetLastNetError();

    Memory::Write_U32(sizeof(s32), request.buffer_out + 0xC);
    Memory::Write_U32(last_error, request.buffer_out + 0x10);
  }

  return GetDefaultReply(return_value);
}

IPCCommandResult NetIPTop::HandleSetSockOptRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_in);
  u32 level = Memory::Read_U32(request.buffer_in + 4);
  u32 optname = Memory::Read_U32(request.buffer_in + 8);
  u32 optlen = Memory::Read_U32(request.buffer_in + 0xc);
  u8 optval[20];
  optlen = std::min(optlen, (u32)sizeof(optval));
  Memory::CopyFromEmu(optval, request.buffer_in + 0x10, optlen);

  INFO_LOG(IOS_NET, "IOCTL_SO_SETSOCKOPT(%08x, %08x, %08x, %08x) "
                    "BufferIn: (%08x, %i), BufferOut: (%08x, %i)"
                    "%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx "
                    "%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
           fd, level, optname, optlen, request.buffer_in, request.buffer_in_size,
           request.buffer_out, request.buffer_out_size, optval[0], optval[1], optval[2], optval[3],
           optval[4], optval[5], optval[6], optval[7], optval[8], optval[9], optval[10], optval[11],
           optval[12], optval[13], optval[14], optval[15], optval[16], optval[17], optval[18],
           optval[19]);

  // TODO: bug booto about this, 0x2005 most likely timeout related, default value on Wii is ,
  // 0x2001 is most likely tcpnodelay
  if (level == 6 && (optname == 0x2005 || optname == 0x2001))
    return GetDefaultReply(0);

  // Do the level/optname translation
  int nat_level = MapWiiSockOptLevelToNative(level);
  int nat_optname = MapWiiSockOptNameToNative(optname);

  int ret = setsockopt(WiiSockMan::GetInstance().GetHostSocket(fd), nat_level, nat_optname,
                       (char*)optval, optlen);
  return GetDefaultReply(WiiSockMan::GetNetErrorCode(ret, "SO_SETSOCKOPT", false));
}

IPCCommandResult NetIPTop::HandleGetSockNameRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_in);

  request.Log(GetDeviceName(), LogTypes::IOS_WC24);

  sockaddr sa;
  socklen_t sa_len = sizeof(sa);
  int ret = getsockname(WiiSockMan::GetInstance().GetHostSocket(fd), &sa, &sa_len);

  if (request.buffer_out_size < 2 + sizeof(sa.sa_data))
    WARN_LOG(IOS_NET, "IOCTL_SO_GETSOCKNAME output buffer is too small. Truncating");

  if (request.buffer_out_size > 0)
    Memory::Write_U8(request.buffer_out_size, request.buffer_out);
  if (request.buffer_out_size > 1)
    Memory::Write_U8(sa.sa_family & 0xFF, request.buffer_out + 1);
  if (request.buffer_out_size > 2)
  {
    Memory::CopyToEmu(request.buffer_out + 2, &sa.sa_data,
                      std::min<size_t>(sizeof(sa.sa_data), request.buffer_out_size - 2));
  }

  return GetDefaultReply(ret);
}

IPCCommandResult NetIPTop::HandleGetPeerNameRequest(const IOCtlRequest& request)
{
  u32 fd = Memory::Read_U32(request.buffer_in);

  sockaddr sa;
  socklen_t sa_len = sizeof(sa);
  int ret = getpeername(WiiSockMan::GetInstance().GetHostSocket(fd), &sa, &sa_len);

  if (request.buffer_out_size < 2 + sizeof(sa.sa_data))
    WARN_LOG(IOS_NET, "IOCTL_SO_GETPEERNAME output buffer is too small. Truncating");

  if (request.buffer_out_size > 0)
    Memory::Write_U8(request.buffer_out_size, request.buffer_out);
  if (request.buffer_out_size > 1)
    Memory::Write_U8(AF_INET, request.buffer_out + 1);
  if (request.buffer_out_size > 2)
  {
    Memory::CopyToEmu(request.buffer_out + 2, &sa.sa_data,
                      std::min<size_t>(sizeof(sa.sa_data), request.buffer_out_size - 2));
  }

  INFO_LOG(IOS_NET, "IOCTL_SO_GETPEERNAME(%x)", fd);
  return GetDefaultReply(ret);
}

IPCCommandResult NetIPTop::HandleGetHostIDRequest(const IOCtlRequest& request)
{
  request.Log(GetDeviceName(), LogTypes::IOS_WC24);

  s32 return_value = 0;

#ifdef _WIN32
  DWORD forwardTableSize, ipTableSize, result;
  NET_IFINDEX ifIndex = NET_IFINDEX_UNSPECIFIED;
  std::unique_ptr<MIB_IPFORWARDTABLE> forwardTable;
  std::unique_ptr<MIB_IPADDRTABLE> ipTable;

  forwardTableSize = 0;
  if (GetIpForwardTable(nullptr, &forwardTableSize, FALSE) == ERROR_INSUFFICIENT_BUFFER)
  {
    forwardTable =
        std::unique_ptr<MIB_IPFORWARDTABLE>((PMIB_IPFORWARDTABLE) operator new(forwardTableSize));
  }

  ipTableSize = 0;
  if (GetIpAddrTable(nullptr, &ipTableSize, FALSE) == ERROR_INSUFFICIENT_BUFFER)
  {
    ipTable = std::unique_ptr<MIB_IPADDRTABLE>((PMIB_IPADDRTABLE) operator new(ipTableSize));
  }

  // find the interface IP used for the default route and use that
  result = GetIpForwardTable(forwardTable.get(), &forwardTableSize, FALSE);
  // can return ERROR_MORE_DATA on XP even after the first call
  while (result == NO_ERROR || result == ERROR_MORE_DATA)
  {
    for (DWORD i = 0; i < forwardTable->dwNumEntries; ++i)
    {
      if (forwardTable->table[i].dwForwardDest == 0)
      {
        ifIndex = forwardTable->table[i].dwForwardIfIndex;
        break;
      }
    }

    if (result == NO_ERROR || ifIndex != NET_IFINDEX_UNSPECIFIED)
      break;

    result = GetIpForwardTable(forwardTable.get(), &forwardTableSize, FALSE);
  }

  if (ifIndex != NET_IFINDEX_UNSPECIFIED &&
      GetIpAddrTable(ipTable.get(), &ipTableSize, FALSE) == NO_ERROR)
  {
    for (DWORD i = 0; i < ipTable->dwNumEntries; ++i)
    {
      if (ipTable->table[i].dwIndex == ifIndex)
      {
        return_value = Common::swap32(ipTable->table[i].dwAddr);
        break;
      }
    }
  }
#endif

  // default placeholder, in case of failure
  if (return_value == 0)
    return_value = 192 << 24 | 168 << 16 | 1 << 8 | 150;

  return GetDefaultReply(return_value);
}

IPCCommandResult NetIPTop::HandleInetAToNRequest(const IOCtlRequest& request)
{
  std::string hostname = Memory::GetString(request.buffer_in);
  struct hostent* remoteHost = gethostbyname(hostname.c_str());

  if (remoteHost == nullptr || remoteHost->h_addr_list == nullptr ||
      remoteHost->h_addr_list[0] == nullptr)
  {
    INFO_LOG(IOS_NET, "IOCTL_SO_INETATON = -1 "
                      "%s, BufferIn: (%08x, %i), BufferOut: (%08x, %i), IP Found: None",
             hostname.c_str(), request.buffer_in, request.buffer_in_size, request.buffer_out,
             request.buffer_out_size);
    return GetDefaultReply(0);
  }

  Memory::Write_U32(Common::swap32(*(u32*)remoteHost->h_addr_list[0]), request.buffer_out);
  INFO_LOG(IOS_NET, "IOCTL_SO_INETATON = 0 "
                    "%s, BufferIn: (%08x, %i), BufferOut: (%08x, %i), IP Found: %08X",
           hostname.c_str(), request.buffer_in, request.buffer_in_size, request.buffer_out,
           request.buffer_out_size, Common::swap32(*(u32*)remoteHost->h_addr_list[0]));
  return GetDefaultReply(1);
}

IPCCommandResult NetIPTop::HandleInetPToNRequest(const IOCtlRequest& request)
{
  std::string address = Memory::GetString(request.buffer_in);
  INFO_LOG(IOS_NET, "IOCTL_SO_INETPTON (Translating: %s)", address.c_str());
  return GetDefaultReply(inet_pton(address.c_str(), Memory::GetPointer(request.buffer_out + 4)));
}

IPCCommandResult NetIPTop::HandleInetNToPRequest(const IOCtlRequest& request)
{
  // u32 af = Memory::Read_U32(BufferIn);
  // u32 validAddress = Memory::Read_U32(request.buffer_in + 4);
  // u32 src = Memory::Read_U32(request.buffer_in + 8);

  char ip_s[16];
  sprintf(ip_s, "%i.%i.%i.%i", Memory::Read_U8(request.buffer_in + 8),
          Memory::Read_U8(request.buffer_in + 8 + 1), Memory::Read_U8(request.buffer_in + 8 + 2),
          Memory::Read_U8(request.buffer_in + 8 + 3));

  INFO_LOG(IOS_NET, "IOCTL_SO_INETNTOP %s", ip_s);
  Memory::CopyToEmu(request.buffer_out, (u8*)ip_s, strlen(ip_s));
  return GetDefaultReply(0);
}

IPCCommandResult NetIPTop::HandlePollRequest(const IOCtlRequest& request)
{
  // Map Wii/native poll events types
  struct
  {
    int native;
    int wii;
  } mapping[] = {
      {POLLRDNORM, 0x0001}, {POLLRDBAND, 0x0002}, {POLLPRI, 0x0004}, {POLLWRNORM, 0x0008},
      {POLLWRBAND, 0x0010}, {POLLERR, 0x0020},    {POLLHUP, 0x0040}, {POLLNVAL, 0x0080},
  };

  u32 unknown = Memory::Read_U32(request.buffer_in);
  u32 timeout = Memory::Read_U32(request.buffer_in + 4);

  int nfds = request.buffer_out_size / 0xc;
  if (nfds == 0)
    ERROR_LOG(IOS_NET, "Hidden POLL");

  std::vector<pollfd_t> ufds(nfds);

  for (int i = 0; i < nfds; ++i)
  {
    s32 wii_fd = Memory::Read_U32(request.buffer_out + 0xc * i);
    ufds[i].fd = WiiSockMan::GetInstance().GetHostSocket(wii_fd);          // fd
    int events = Memory::Read_U32(request.buffer_out + 0xc * i + 4);       // events
    ufds[i].revents = Memory::Read_U32(request.buffer_out + 0xc * i + 8);  // revents

    // Translate Wii to native events
    int unhandled_events = events;
    ufds[i].events = 0;
    for (auto& map : mapping)
    {
      if (events & map.wii)
        ufds[i].events |= map.native;
      unhandled_events &= ~map.wii;
    }
    DEBUG_LOG(IOS_NET, "IOCTL_SO_POLL(%d) "
                       "Sock: %08x, Unknown: %08x, Events: %08x, "
                       "NativeEvents: %08x",
              i, wii_fd, unknown, events, ufds[i].events);

    // Do not pass return-only events to the native poll
    ufds[i].events &= ~(POLLERR | POLLHUP | POLLNVAL | UNSUPPORTED_WSAPOLL);

    if (unhandled_events)
      ERROR_LOG(IOS_NET, "SO_POLL: unhandled Wii event types: %04x", unhandled_events);
  }

  int ret = poll(ufds.data(), nfds, timeout);
  ret = WiiSockMan::GetNetErrorCode(ret, "SO_POLL", false);

  for (int i = 0; i < nfds; ++i)
  {
    // Translate native to Wii events
    int revents = 0;
    for (auto& map : mapping)
    {
      if (ufds[i].revents & map.native)
        revents |= map.wii;
    }

    // No need to change fd or events as they are input only.
    // Memory::Write_U32(ufds[i].fd, request.buffer_out + 0xc*i); //fd
    // Memory::Write_U32(events, request.buffer_out + 0xc*i + 4); //events
    Memory::Write_U32(revents, request.buffer_out + 0xc * i + 8);  // revents

    DEBUG_LOG(IOS_NET, "IOCTL_SO_POLL socket %d wevents %08X events %08X revents %08X", i, revents,
              ufds[i].events, ufds[i].revents);
  }

  return GetDefaultReply(ret);
}

IPCCommandResult NetIPTop::HandleGetHostByNameRequest(const IOCtlRequest& request)
{
  if (request.buffer_out_size != 0x460)
  {
    ERROR_LOG(IOS_NET, "Bad buffer size for IOCTL_SO_GETHOSTBYNAME");
    return GetDefaultReply(-1);
  }

  std::string hostname = Memory::GetString(request.buffer_in);
  hostent* remoteHost = gethostbyname(hostname.c_str());

  INFO_LOG(IOS_NET, "IOCTL_SO_GETHOSTBYNAME "
                    "Address: %s, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
           hostname.c_str(), request.buffer_in, request.buffer_in_size, request.buffer_out,
           request.buffer_out_size);

  if (remoteHost == nullptr)
    return GetDefaultReply(-1);

  for (int i = 0; remoteHost->h_aliases[i]; ++i)
  {
    DEBUG_LOG(IOS_NET, "alias%i:%s", i, remoteHost->h_aliases[i]);
  }

  for (int i = 0; remoteHost->h_addr_list[i]; ++i)
  {
    u32 ip = Common::swap32(*(u32*)(remoteHost->h_addr_list[i]));
    std::string ip_s =
        StringFromFormat("%i.%i.%i.%i", ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
    DEBUG_LOG(IOS_NET, "addr%i:%s", i, ip_s.c_str());
  }

  // Host name; located immediately after struct
  static const u32 GETHOSTBYNAME_STRUCT_SIZE = 0x10;
  static const u32 GETHOSTBYNAME_IP_LIST_OFFSET = 0x110;
  // Limit host name length to avoid buffer overflow.
  u32 name_length = (u32)strlen(remoteHost->h_name) + 1;
  if (name_length > (GETHOSTBYNAME_IP_LIST_OFFSET - GETHOSTBYNAME_STRUCT_SIZE))
  {
    ERROR_LOG(IOS_NET, "Hostname too long in IOCTL_SO_GETHOSTBYNAME");
    return GetDefaultReply(-1);
  }
  Memory::CopyToEmu(request.buffer_out + GETHOSTBYNAME_STRUCT_SIZE, remoteHost->h_name,
                    name_length);
  Memory::Write_U32(request.buffer_out + GETHOSTBYNAME_STRUCT_SIZE, request.buffer_out);

  // IP address list; located at offset 0x110.
  u32 num_ip_addr = 0;
  while (remoteHost->h_addr_list[num_ip_addr])
    num_ip_addr++;
  // Limit number of IP addresses to avoid buffer overflow.
  // (0x460 - 0x340) / sizeof(pointer) == 72
  static const u32 GETHOSTBYNAME_MAX_ADDRESSES = 71;
  num_ip_addr = std::min(num_ip_addr, GETHOSTBYNAME_MAX_ADDRESSES);
  for (u32 i = 0; i < num_ip_addr; ++i)
  {
    u32 addr = request.buffer_out + GETHOSTBYNAME_IP_LIST_OFFSET + i * 4;
    Memory::Write_U32_Swap(*(u32*)(remoteHost->h_addr_list[i]), addr);
  }

  // List of pointers to IP addresses; located at offset 0x340.
  // This must be exact: PPC code to convert the struct hardcodes
  // this offset.
  static const u32 GETHOSTBYNAME_IP_PTR_LIST_OFFSET = 0x340;
  Memory::Write_U32(request.buffer_out + GETHOSTBYNAME_IP_PTR_LIST_OFFSET, request.buffer_out + 12);
  for (u32 i = 0; i < num_ip_addr; ++i)
  {
    u32 addr = request.buffer_out + GETHOSTBYNAME_IP_PTR_LIST_OFFSET + i * 4;
    Memory::Write_U32(request.buffer_out + GETHOSTBYNAME_IP_LIST_OFFSET + i * 4, addr);
  }
  Memory::Write_U32(0, request.buffer_out + GETHOSTBYNAME_IP_PTR_LIST_OFFSET + num_ip_addr * 4);

  // Aliases - empty. (Hardware doesn't return anything.)
  Memory::Write_U32(request.buffer_out + GETHOSTBYNAME_IP_PTR_LIST_OFFSET + num_ip_addr * 4,
                    request.buffer_out + 4);

  // Returned struct must be ipv4.
  _assert_msg_(IOS_NET, remoteHost->h_addrtype == AF_INET && remoteHost->h_length == sizeof(u32),
               "returned host info is not IPv4");
  Memory::Write_U16(AF_INET, request.buffer_out + 8);
  Memory::Write_U16(sizeof(u32), request.buffer_out + 10);

  return GetDefaultReply(0);
}

IPCCommandResult NetIPTop::HandleICMPCancelRequest(const IOCtlRequest& request)
{
  ERROR_LOG(IOS_NET, "IOCTL_SO_ICMPCANCEL");
  return GetDefaultReply(0);
}

IPCCommandResult NetIPTop::HandleGetInterfaceOptRequest(const IOCtlVRequest& request)
{
  const u32 param = Memory::Read_U32(request.in_vectors[0].address);
  const u32 param2 = Memory::Read_U32(request.in_vectors[0].address + 4);
  const u32 param3 = Memory::Read_U32(request.io_vectors[0].address);
  const u32 param4 = Memory::Read_U32(request.io_vectors[1].address);
  u32 param5 = 0;

  if (request.io_vectors[0].size >= 8)
  {
    param5 = Memory::Read_U32(request.io_vectors[0].address + 4);
  }

  INFO_LOG(IOS_NET, "IOCTLV_SO_GETINTERFACEOPT(%08X, %08X, %X, %X, %X) "
                    "BufferIn: (%08x, %i), BufferIn2: (%08x, %i) ",
           param, param2, param3, param4, param5, request.in_vectors[0].address,
           request.in_vectors[0].size,
           request.in_vectors.size() > 1 ? request.in_vectors[1].address : 0,
           request.in_vectors.size() > 1 ? request.in_vectors[1].size : 0);

  switch (param2)
  {
  case 0xb003:  // dns server table
  {
    u32 address = 0;
#ifdef _WIN32
    if (!Core::WantsDeterminism())
    {
      PIP_ADAPTER_ADDRESSES AdapterAddresses = nullptr;
      ULONG OutBufferLength = 0;
      ULONG RetVal = 0, i;
      for (i = 0; i < 5; ++i)
      {
        RetVal = GetAdaptersAddresses(AF_INET, 0, nullptr, AdapterAddresses, &OutBufferLength);

        if (RetVal != ERROR_BUFFER_OVERFLOW)
        {
          break;
        }

        if (AdapterAddresses != nullptr)
        {
          FREE(AdapterAddresses);
        }

        AdapterAddresses = (PIP_ADAPTER_ADDRESSES)MALLOC(OutBufferLength);
        if (AdapterAddresses == nullptr)
        {
          RetVal = GetLastError();
          break;
        }
      }
      if (RetVal == NO_ERROR)
      {
        unsigned long dwBestIfIndex = 0;
        IPAddr dwDestAddr = (IPAddr)0x08080808;
        // If successful, output some information from the data we received
        PIP_ADAPTER_ADDRESSES AdapterList = AdapterAddresses;
        if (GetBestInterface(dwDestAddr, &dwBestIfIndex) == NO_ERROR)
        {
          while (AdapterList)
          {
            if (AdapterList->IfIndex == dwBestIfIndex && AdapterList->FirstDnsServerAddress &&
                AdapterList->OperStatus == IfOperStatusUp)
            {
              INFO_LOG(IOS_NET, "Name of valid interface: %S", AdapterList->FriendlyName);
              INFO_LOG(
                  IOS_NET, "DNS: %u.%u.%u.%u",
                  (unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[2],
                  (unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[3],
                  (unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[4],
                  (unsigned char)
                      AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[5]);
              address = Common::swap32(
                  *(u32*)(&AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[2]));
              break;
            }
            AdapterList = AdapterList->Next;
          }
        }
      }
      if (AdapterAddresses != nullptr)
      {
        FREE(AdapterAddresses);
      }
    }
#endif
    if (address == 0)
      address = 0x08080808;

    Memory::Write_U32(address, request.io_vectors[0].address);
    Memory::Write_U32(0x08080404, request.io_vectors[0].address + 4);
    break;
  }
  case 0x1003:  // error
    Memory::Write_U32(0, request.io_vectors[0].address);
    break;

  case 0x1004:  // mac address
    u8 address[Common::MAC_ADDRESS_SIZE];
    IOS::Net::GetMACAddress(address);
    Memory::CopyToEmu(request.io_vectors[0].address, address, sizeof(address));
    break;

  case 0x1005:  // link state
    Memory::Write_U32(1, request.io_vectors[0].address);
    break;

  case 0x3001:  // hardcoded value
    Memory::Write_U32(0x10, request.io_vectors[0].address);
    break;

  case 0x4002:  // ip addr numberHandle
    Memory::Write_U32(1, request.io_vectors[0].address);
    break;

  case 0x4003:  // ip addr table
    Memory::Write_U32(0xC, request.io_vectors[1].address);
    Memory::Write_U32(inet_addr(10, 0, 1, 30), request.io_vectors[0].address);
    Memory::Write_U32(inet_addr(255, 255, 255, 0), request.io_vectors[0].address + 4);
    Memory::Write_U32(inet_addr(10, 0, 255, 255), request.io_vectors[0].address + 8);
    break;

  case 0x4005:  // hardcoded value
    Memory::Write_U32(0x20, request.io_vectors[0].address);
    break;

  case 0x6003:  // hardcoded value
    Memory::Write_U32(0x80, request.io_vectors[0].address);
    break;

  case 0x600a:  // hardcoded value
    Memory::Write_U32(0x80, request.io_vectors[0].address);
    break;

  case 0x600c:  // hardcoded value
    Memory::Write_U32(0x80, request.io_vectors[0].address);
    break;

  case 0xb002:  // hardcoded value
    Memory::Write_U32(2, request.io_vectors[0].address);
    break;

  default:
    ERROR_LOG(IOS_NET, "Unknown param2: %08X", param2);
    break;
  }

  return GetDefaultReply(0);
}

IPCCommandResult NetIPTop::HandleSendToRequest(const IOCtlVRequest& request)
{
  u32 fd = Memory::Read_U32(request.in_vectors[1].address);
  WiiSockMan& sm = WiiSockMan::GetInstance();
  sm.DoSock(fd, request, IOCTLV_SO_SENDTO);
  return GetNoReply();
}

IPCCommandResult NetIPTop::HandleRecvFromRequest(const IOCtlVRequest& request)
{
  u32 fd = Memory::Read_U32(request.in_vectors[0].address);
  WiiSockMan& sm = WiiSockMan::GetInstance();
  sm.DoSock(fd, request, IOCTLV_SO_RECVFROM);
  return GetNoReply();
}

IPCCommandResult NetIPTop::HandleGetAddressInfoRequest(const IOCtlVRequest& request)
{
  addrinfo hints;

  if (request.in_vectors.size() > 2 && request.in_vectors[2].size)
  {
    hints.ai_flags = Memory::Read_U32(request.in_vectors[2].address);
    hints.ai_family = Memory::Read_U32(request.in_vectors[2].address + 0x4);
    hints.ai_socktype = Memory::Read_U32(request.in_vectors[2].address + 0x8);
    hints.ai_protocol = Memory::Read_U32(request.in_vectors[2].address + 0xC);
    hints.ai_addrlen = Memory::Read_U32(request.in_vectors[2].address + 0x10);
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_next = nullptr;
  }

  // getaddrinfo allows a null pointer for the nodeName or serviceName strings
  // So we have to do a bit of juggling here.
  std::string nodeNameStr;
  const char* pNodeName = nullptr;
  if (request.in_vectors.size() > 0 && request.in_vectors[0].size > 0)
  {
    nodeNameStr = Memory::GetString(request.in_vectors[0].address, request.in_vectors[0].size);
    pNodeName = nodeNameStr.c_str();
  }

  std::string serviceNameStr;
  const char* pServiceName = nullptr;
  if (request.in_vectors.size() > 1 && request.in_vectors[1].size > 0)
  {
    serviceNameStr = Memory::GetString(request.in_vectors[1].address, request.in_vectors[1].size);
    pServiceName = serviceNameStr.c_str();
  }

  addrinfo* result = nullptr;
  int ret = getaddrinfo(
      pNodeName, pServiceName,
      (request.in_vectors.size() > 2 && request.in_vectors[2].size) ? &hints : nullptr, &result);
  u32 addr = request.io_vectors[0].address;
  u32 sockoffset = addr + 0x460;
  if (ret == 0)
  {
    constexpr size_t WII_ADDR_INFO_SIZE = 0x20;
    for (addrinfo* result_iter = result; result_iter != nullptr; result_iter = result_iter->ai_next)
    {
      Memory::Write_U32(result_iter->ai_flags, addr);
      Memory::Write_U32(result_iter->ai_family, addr + 0x04);
      Memory::Write_U32(result_iter->ai_socktype, addr + 0x08);
      Memory::Write_U32(result_iter->ai_protocol, addr + 0x0C);
      Memory::Write_U32((u32)result_iter->ai_addrlen, addr + 0x10);
      // what to do? where to put? the buffer of 0x834 doesn't allow space for this
      Memory::Write_U32(/*result->ai_cannonname*/ 0, addr + 0x14);

      if (result_iter->ai_addr)
      {
        Memory::Write_U32(sockoffset, addr + 0x18);
        Memory::Write_U8(result_iter->ai_addrlen & 0xFF, sockoffset);
        Memory::Write_U8(result_iter->ai_addr->sa_family & 0xFF, sockoffset + 0x01);
        Memory::CopyToEmu(sockoffset + 0x2, result_iter->ai_addr->sa_data,
                          sizeof(result_iter->ai_addr->sa_data));
        sockoffset += 0x1C;
      }
      else
      {
        Memory::Write_U32(0, addr + 0x18);
      }

      if (result_iter->ai_next)
      {
        Memory::Write_U32(addr + WII_ADDR_INFO_SIZE, addr + 0x1C);
      }
      else
      {
        Memory::Write_U32(0, addr + 0x1C);
      }

      addr += WII_ADDR_INFO_SIZE;
    }

    freeaddrinfo(result);
  }
  else
  {
    // Host not found
    ret = -305;
  }

  request.Dump(GetDeviceName(), LogTypes::IOS_NET, LogTypes::LINFO);
  return GetDefaultReply(ret);
}

IPCCommandResult NetIPTop::HandleICMPPingRequest(const IOCtlVRequest& request)
{
  struct
  {
    u8 length;
    u8 addr_family;
    u16 icmp_id;
    u32 ip;
  } ip_info;

  u32 fd = Memory::Read_U32(request.in_vectors[0].address);
  u32 num_ip = Memory::Read_U32(request.in_vectors[0].address + 4);
  u64 timeout = Memory::Read_U64(request.in_vectors[0].address + 8);

  if (num_ip != 1)
  {
    INFO_LOG(IOS_NET, "IOCTLV_SO_ICMPPING %i IPs", num_ip);
  }

  ip_info.length = Memory::Read_U8(request.in_vectors[0].address + 16);
  ip_info.addr_family = Memory::Read_U8(request.in_vectors[0].address + 17);
  ip_info.icmp_id = Memory::Read_U16(request.in_vectors[0].address + 18);
  ip_info.ip = Memory::Read_U32(request.in_vectors[0].address + 20);

  if (ip_info.length != 8 || ip_info.addr_family != AF_INET)
  {
    INFO_LOG(IOS_NET, "IOCTLV_SO_ICMPPING strange IPInfo:\n"
                      "length %x addr_family %x",
             ip_info.length, ip_info.addr_family);
  }

  INFO_LOG(IOS_NET, "IOCTLV_SO_ICMPPING %x", ip_info.ip);

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = Common::swap32(ip_info.ip);
  memset(addr.sin_zero, 0, 8);

  u8 data[0x20];
  memset(data, 0, sizeof(data));
  s32 icmp_length = sizeof(data);

  if (request.in_vectors.size() > 1 && request.in_vectors[1].size == sizeof(data))
  {
    Memory::CopyFromEmu(data, request.in_vectors[1].address, request.in_vectors[1].size);
  }
  else
  {
    // TODO sequence number is incremented either statically, by
    // port, or by socket. Doesn't seem to matter, so we just leave
    // it 0
    ((u16*)data)[0] = Common::swap16(ip_info.icmp_id);
    icmp_length = 22;
  }

  int ret = icmp_echo_req(WiiSockMan::GetInstance().GetHostSocket(fd), &addr, data, icmp_length);
  if (ret == icmp_length)
  {
    ret = icmp_echo_rep(WiiSockMan::GetInstance().GetHostSocket(fd), &addr,
                        static_cast<u32>(timeout), icmp_length);
  }

  // TODO proper error codes
  return GetDefaultReply(0);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
