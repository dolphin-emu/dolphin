// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

namespace IOS::HLE
{
enum NET_IOCTL
{
  IOCTL_SO_ACCEPT = 1,
  IOCTL_SO_BIND,
  IOCTL_SO_CLOSE,
  IOCTL_SO_CONNECT,
  IOCTL_SO_FCNTL,
  IOCTL_SO_GETPEERNAME,
  IOCTL_SO_GETSOCKNAME,
  IOCTL_SO_GETSOCKOPT,
  IOCTL_SO_SETSOCKOPT,
  IOCTL_SO_LISTEN,
  IOCTL_SO_POLL,
  IOCTLV_SO_RECVFROM,
  IOCTLV_SO_SENDTO,
  IOCTL_SO_SHUTDOWN,
  IOCTL_SO_SOCKET,
  IOCTL_SO_GETHOSTID,
  IOCTL_SO_GETHOSTBYNAME,
  IOCTL_SO_GETHOSTBYADDR,
  IOCTLV_SO_GETNAMEINFO,
  IOCTL_SO_UNK14,
  IOCTL_SO_INETATON,
  IOCTL_SO_INETPTON,
  IOCTL_SO_INETNTOP,
  IOCTLV_SO_GETADDRINFO,
  IOCTL_SO_SOCKATMARK,
  IOCTLV_SO_UNK1A,
  IOCTLV_SO_UNK1B,
  IOCTLV_SO_GETINTERFACEOPT,
  IOCTLV_SO_SETINTERFACEOPT,
  IOCTL_SO_SETINTERFACE,
  IOCTL_SO_STARTUP,
  IOCTL_SO_ICMPSOCKET = 0x30,
  IOCTLV_SO_ICMPPING,
  IOCTL_SO_ICMPCANCEL,
  IOCTL_SO_ICMPCLOSE
};

namespace Device
{
class NetIPTop : public Device
{
public:
  NetIPTop(Kernel& ios, const std::string& device_name);
  virtual ~NetIPTop();

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void Update() override;

private:
  IPCCommandResult HandleStartUpRequest(const IOCtlRequest& request);
  IPCCommandResult HandleSocketRequest(const IOCtlRequest& request);
  IPCCommandResult HandleICMPSocketRequest(const IOCtlRequest& request);
  IPCCommandResult HandleCloseRequest(const IOCtlRequest& request);
  IPCCommandResult HandleDoSockRequest(const IOCtlRequest& request);
  IPCCommandResult HandleShutdownRequest(const IOCtlRequest& request);
  IPCCommandResult HandleListenRequest(const IOCtlRequest& request);
  IPCCommandResult HandleGetSockOptRequest(const IOCtlRequest& request);
  IPCCommandResult HandleSetSockOptRequest(const IOCtlRequest& request);
  IPCCommandResult HandleGetSockNameRequest(const IOCtlRequest& request);
  IPCCommandResult HandleGetPeerNameRequest(const IOCtlRequest& request);
  IPCCommandResult HandleGetHostIDRequest(const IOCtlRequest& request);
  IPCCommandResult HandleInetAToNRequest(const IOCtlRequest& request);
  IPCCommandResult HandleInetPToNRequest(const IOCtlRequest& request);
  IPCCommandResult HandleInetNToPRequest(const IOCtlRequest& request);
  IPCCommandResult HandlePollRequest(const IOCtlRequest& request);
  IPCCommandResult HandleGetHostByNameRequest(const IOCtlRequest& request);
  IPCCommandResult HandleICMPCancelRequest(const IOCtlRequest& request);

  IPCCommandResult HandleGetInterfaceOptRequest(const IOCtlVRequest& request);
  IPCCommandResult HandleSendToRequest(const IOCtlVRequest& request);
  IPCCommandResult HandleRecvFromRequest(const IOCtlVRequest& request);
  IPCCommandResult HandleGetAddressInfoRequest(const IOCtlVRequest& request);
  IPCCommandResult HandleICMPPingRequest(const IOCtlVRequest& request);

#ifdef _WIN32
  WSADATA InitData;
#endif
};
}  // namespace Device
}  // namespace IOS::HLE
